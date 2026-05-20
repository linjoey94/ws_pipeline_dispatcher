# ws_pipeline_dispatcher Internal Spec

本文件只描述 `ws_pipeline_dispatcher` repo 內部如何工作。跨 repo contract 請看 Linear：

- [Simplified Integration Spec](https://linear.app/grason/document/simplified-integration-spec-bcfbf6ee3e4c)

若本文件與 Linear 衝突，以 Linear 為準。

## Repo Role

`ws_pipeline_dispatcher` 是 C / UNIX pipeline。它不接 WebSocket，也不負責 ESP32 packet parsing。

它負責：

- 接收 `edge-ws-host` 透過 `spawn()` 傳入的 CLI 參數。
- 讀取 `/tmp/stream/{session_id}/{session_id}.bin` session-level binary buffer。
- 監聽 `.pipeline_end` 作為 session 結束訊號。
- 產生 clip JSON Lines。
- 過濾 `type=clip`。
- 寫入 `/tmp/clips.db` 純文字 index。

## Non-Goals

以下不屬於這個 repo 的責任：

- 建立 WebSocket server 或管理 client connection。
- 解析 ESP32 原始封包格式。
- 定義跨 repo runtime sequence。
- 追蹤所有 integration mismatch；這類專案層級資訊以 Linear 為主。
- 在 v2.0 提供 benchmark numbers、Toybox/GNU compatibility matrix 或完整 man pages。

## Internal Pipeline

```text
pipeline_dispatcher
  ├─ stream_merge  stdout ── pipe_1 ──> log_parse
  ├─ log_parse     stdout ── pipe_2 ──> clip_store
  └─ clip_store    writes /tmp/clips.db
```

每個 process 的角色：

- `pipeline_dispatcher`：建立 pipe、spawn child、回收 exit status。
- `stream_merge`：正確設計下應從 growing binary stream 搭配 metadata sidecar 產生 clip metadata JSON Lines；目前 baseline implementation 仍有 fixture-driven mismatch，詳見 `stream_merge-v1.0` 與 `v2-gap-list`。
- `log_parse`：對上游 structured records 做 parse / filter / reformat。
- `clip_store`：把 clip records 寫入 file-backed index，並支援 TTL / GC。

## Runtime Data Flow

正常資料流如下：

1. 上層先建立 `/tmp/stream/{session_id}/` 與對應資料檔。
2. `pipeline_dispatcher` 以 CLI 參數啟動三個 applet。
3. `stream_merge` 讀 `{session_id}.bin` 與 `{session_id}.meta.jsonl`，用 sidecar metadata 判斷每個 `DATA` payload 在 session buffer 中的 byte range、資料是否有接上、duration 與 events。
4. `stream_merge` 從 session `.bin` buffer 抽出 5s 等時間窗對應的 byte range，輸出 clip metadata records 到 stdout。
5. `log_parse` 從 stdin 讀取 records，保留 `type=clip` 或做格式轉換。
6. `clip_store` 從 stdin 讀取 clip JSON Lines，寫入 `db_path`。
7. `.pipeline_end` 出現後，`stream_merge` drain 剩餘 bytes 並結束；下游跟著 EOF 收束。

這個 repo 的重點不是 packet ingress，而是把已存在的 session data 透過 UNIX pipeline 收斂成可查詢的 clip index。

## Runtime Inputs

目前最小 cross-repo 版本中，`pipeline_dispatcher` 可由 `edge-ws-host` 在 `END_` 後啟動；只要 session artifact contract 固定為 `.bin + .meta.jsonl + .pipeline_end`，後續若要回到更即時的 growing-file tailing，不需要重做下層資料格式。

```text
pipeline_dispatcher <session_id> <src_dir> <db_path> <ttl_seconds>
```

目前約定：

- `session_id` 等同 `eventId`。
- `src_dir` 是 `/tmp/stream/{session_id}/`。
- `db_path` 預設是 `/tmp/clips.db`。
- `ttl_seconds` 由上層決定。

若上層想調整啟動時機、CLI 來源或 session layout，這屬於 integration contract，必須先改 Linear 文件。

## Cross-Repo Assumptions

目前先以 `ESP32 -> edge-ws-host` 的 WebSocket/TCP ingress 為主，cross-repo contract 先收斂到以下幾點：

- `.pipeline_end` 代表上層這個 session 的 WS packet 傳遞已結束，而且上層已完成本 session artifact 的寫入與 close。
- 一個 session 的生命週期是 `STRT -> many DATA / JSON messages -> END_`；`onmessage` 只代表收到一個完整 WS message，不代表整段影片或整個 session。
- 上層仍需要先讀完整 WS message，才能知道 packet opcode 是 `STRT`、`DATA`、`JSON` 還是 `END_`。
- WebSocket/TCP 層負責把 message 邊界交給上層 handler；上層不需要自己從 TCP byte stream 重新切 packet。
- 上層在知道 packet 類型後，需把每個 `DATA` message 的 payload append 到同一個 `{session_id}.bin`，讓它成為整個 session 的巨大 binary video buffer，並把最小 metadata 追加到 `{session_id}.meta.jsonl`。
- 上層不需要把整個 `DATA` payload 重新包成 JSONL 再丟給下層；binary data 與 sidecar metadata 應分開保存。
- `JSON` packet 若屬於事件資料，應以 sidecar record 形式寫進 `{session_id}.meta.jsonl`，而不是要求下層直接理解上層 WS packet。

## Filesystem Assumptions

本 repo 假設上層已建立以下 layout：

```text
/tmp/stream/{session_id}/
    {session_id}.bin
    {session_id}.meta.jsonl
    .pipeline_end
```

`{session_id}.bin` 是 append-only session-level binary buffer，內容應是所有 `DATA` message payload 串接後的 video bytes，不是 JSON payload。上層目前以 WebSocket/TCP 接 ESP32，因此 v2.1 不先假設 UDP 式亂序或晚到封包處理。

`{session_id}.meta.jsonl` 應提供最小 sidecar metadata，例如 `kind`、`sequence`、`offset`、`length`、`ts_ms`，必要時再加 events。沒有這類 sidecar metadata，`stream_merge` 無法可靠從 session buffer 抽出 5s 等時間窗對應的 byte range，也無法完成 continuity 檢查、partial clip 與 event merge。

`.pipeline_end` 表示上層已收到 `END_`，整個 session 的 WS packet 傳遞已結束，而且上層已完成 `.bin` / `.meta.jsonl` 寫入。它不是單一 `DATA` message 的結束標記。

`clip_store` 目前把 clips 寫到單一 file-backed index，例如 `/tmp/clips.db`。這是 repo-local storage artifact，不等於跨 repo database contract。

## Applet Responsibility Split

- `pipeline_dispatcher`
  - 建立 `pipe()`。
  - `fork()` / `execv()` child processes。
  - 關閉不需要的 fd。
  - `waitpid()` 收斂 child exit status。
- `stream_merge`
  - 監聽 growing file 追加內容。
  - 依 metadata sidecar 解讀 chunk 邊界與 clip 切割條件。
  - 在目前 TCP/WebSocket 假設下，只需先防守 sequence/offset continuity，不先做 UDP 亂序重排。
  - 感知 sentinel，決定何時 drain 並正常退出。
  - 保持 stdout 為 structured records，不輸出診斷文字。
- `log_parse`
  - 支援 regex parse mode 與 integration filter mode。
  - 支援 JSON / CSV output 與欄位選取。
  - 負責資料過濾與轉換，不負責持久化。
- `clip_store`
  - 支援 append / get / gc 相關 CLI 路徑。
  - 管理 TTL 與 file-backed index 格式。
  - 對文件要誠實，不應描述尚未完成的完整 CRUD 或 crash-safety。

## Shared Library Responsibility Split

- `libpipeline`
  - 放 repo 內 applet 共用的 POSIX / inotify / sentinel helper。
  - 不解析 JSON，不持有業務資料格式知識。
- `stream_logger`
  - 提供 stderr-only diagnostic logging。
  - 保護 stdout 不被 log 汙染。

這兩個 library 的定位是支援 applet，不是獨立對外 API 產品。

## Stdout / Stderr Rule

stdout 是資料流，只能放下一層要讀的 structured output。

stderr 是診斷流，所有 log、warning、error 都必須走 stderr。

這條規則很重要，否則 UNIX pipe 會被 log 污染。

對這個 repo 而言，這是一條 architecture rule，不只是 coding style。任何新功能若需要額外輸出資訊，都應先判斷那是資料還是診斷訊息。

## Process Topology And Failure Model

- pipeline 是固定三段：`stream_merge | log_parse | clip_store`
- parent process 只負責 orchestration，不處理 clip payload
- 任一 child 提前失敗時，pipe 會自然斷開，其他 child 應能觀察到 EOF 或 write failure 並收束
- `pipeline_dispatcher` 最終用 exit code 對上層回報 pipeline 是否成功

## Error Propagation

任一 child process 異常時，pipe 會自然 EOF，其他 child 會收束。`pipeline_dispatcher` 用 `waitpid()` 收集狀態，並用 exit code 回報給上層。

repo docs 應只描述目前實作有的 error model：

- 參數錯誤
- `pipe()` / `fork()` / `execv()` 失敗
- child process 非 0 exit
- child process 被 signal 終止

更細的 retry policy、上層補償機制或 session restart 策略，不在這個 repo 內定義。

## Build, Test, And Demo Relationship

- `Makefile` 提供 repo-local build / test / smoke 入口。
- `make test` 是目前最主要的 correctness evidence。
- README 的 quick demo 用來說明最小可執行路徑，不等於 final demo evidence。
- benchmark、compatibility matrix、report-ready demo artifacts 屬於後續 v2.2 收斂。

因此 `full_spec` 應說明 repo 如何工作，但不應把尚未補齊的 benchmark/man/help 說成已完成交付。

## What Belongs Here

- applet CLI 與 exit code。
- repo-local build/test/run 注意事項。
- C 實作細節與內部資料結構。
- debug、logging、failure handling。
- repo 內部 architecture、責任切分與資料流。

## What Belongs In Linear

- `edge-ws-host` 與本 repo 的 contract。
- packet format。
- filesystem layout 的跨 repo 決策。
- runtime sequence。
- current gaps / project tracking。

## Reading Path

如果是第一次進 repo，建議順序：

1. `README.md`：先看課程主軸、build/test/demo。
2. `.docs/compliance-matrix.md`：確認哪些要求已完成、哪些是 follow-up。
3. `.docs/full_spec.md`：理解 repo role、pipeline 結構與內部邊界。
4. applet / library spec：進入單一模組細節。

# ws_pipeline_dispatcher

`ws_pipeline_dispatcher` 是一組以 C 實作的 UNIX pipeline applets，對應 UNIX 系統程式設計期末專題選項 B「BusyBox 工具擴充」中的方向三「Embedded Data Pipeline」。

本 repo 的主軸不是 WebSocket server，而是把上層落地的 append-only stream 轉成可被 UNIX pipe 組合、過濾、儲存的資料處理管線。

## 專案主軸

系統分成兩層：

- `edge-ws-host`：接收 ESP32 的持久 WebSocket/TCP 連線，在 `STRT ... many DATA/JSON ... END_` 的 session 期間持續收 packet，將每個 `DATA` payload append 到 session-level `{session_id}.bin` buffer，將 offset metadata 落到 sidecar，並在 session 結束後啟動 C pipeline。
- `ws_pipeline_dispatcher`：讀取上層落地的 session artifact，切出 structured clip metadata，過濾 clip event，寫入 file-backed index。

核心設計是把複雜串流處理拆成三個小工具：

```text
stream_merge | log_parse --filter type=clip | clip_store
```

每個 applet 只做一件事，stdout 只傳資料，stderr 只寫診斷訊息。這讓整體行為符合 UNIX pipeline 的組合方式，也能在資源受限環境中以小型 C binary 運作。

## 作業方向對應

| 作業 B + 方向三要求 | 本 repo 對應 |
| --- | --- |
| 使用 C 語言實作 3 個新的 applet | `stream_merge`、`log_parse`、`clip_store` |
| 結構化日誌解析器 | `log_parse --regex ... --fields ... --format json\|csv` |
| 串流資料過濾與轉換工具 | `stream_merge` 讀取 growing file，`log_parse --filter key=value` 過濾 records |
| 輕量級資料儲存引擎 | `clip_store` 寫入 file-backed key-value index，支援 TTL、查詢與 GC |
| 三個工具可透過 UNIX pipe 組成完整管線 | `pipeline_dispatcher` 建立 `stream_merge -> log_parse -> clip_store` |
| 提取共用邏輯為內部函式庫 | `libpipeline`、`stream_logger` |
| 遵循 stdout/stderr 與 CLI 慣例 | applet stdout 保持資料流，diagnostic logs 走 stderr |

完整對照表見 [`.docs/core/compliance.md`](.docs/core/compliance.md)。

## Pipeline 架構

```text
ESP32 Video Data
  -> edge-ws-host
  -> /tmp/stream/{session_id}/{session_id}.bin
  -> /tmp/stream/{session_id}/{session_id}.meta.jsonl
  -> pipeline_dispatcher
       stream_merge stdout -> log_parse stdout -> clip_store
  -> /tmp/clips.db
```

`pipeline_dispatcher` 是 C entry point，負責建立 process pipeline：

- 用 `pipe()` 建立 applet 間的資料通道。
- 用 `fork()` 建立 child processes。
- 用 `execv()` 執行三個 applet。
- 用 `waitpid()` 回收 child 狀態並回報整體 exit code。

跨 repo 的啟動時機、檔案 layout 與 packet contract 以 Linear integration docs 為準；repo 內部實作細節則記錄在 `.docs/`。

## Applets

### `pipeline_dispatcher`

建立三段 UNIX pipeline，不直接處理 clip JSON：

```text
pipeline_dispatcher <session_id> <src_dir> <db_path> <ttl_seconds>
```

### `stream_merge`

正確 contract 下會讀取 `{src_dir}/{session_id}.bin` 與 `{src_dir}/{session_id}.meta.jsonl`。上層目前以持久 WebSocket/TCP 連線接收 ESP32 資料；一個 session 會在 `STRT` 與 `END_` 之間收到很多個 `DATA` messages。每個 `DATA` 只是一小段影片資料，會被 append 到同一個 `.bin`，因此 `.bin` 是供下層操作的 session-level binary buffer。`stream_merge` 依 sidecar 從這個 buffer 抽出 5s 等時間窗對應的 byte range，並輸出 clip metadata。

### `log_parse`

stdin -> stdout 的 structured record processor，支援兩種用途：

- 基本需求：regex-based parsing、輸出 JSON 或 CSV。
- Filter 功能：讀取 JSON Lines，使用 `--filter key=value` 保留指定 records。

### `clip_store`

pipeline 終端的 file-backed index。從 stdin 讀取 clip JSON Lines，用 `session_id:ts` 作為 key，clip path 作為 value，寫入純文字 DB，並提供查詢、TTL 與 GC 行為。

## 系統程式設計重點

本 repo 的技術重點集中在 UNIX 系統程式設計，而不是 Web framework：

- Process management：`fork()`、`execv()`、`waitpid()`。
- IPC：`pipe()`、stdin/stdout chaining。
- Filesystem streaming：append-only `.bin`、`.meta.jsonl` sidecar、sentinel file、tail-read offset。
- Event notification：`inotify`、`poll()`。
- File-backed storage：`open()`、`flock()`、append-only index、GC rewrite 方向。
- Error handling：exit code propagation、child process failure handling。
- Stream discipline：stdout 只放 structured data，stderr 只放 diagnostic logs。

## 編譯與測試

```bash
make              # 編譯所有 applet 到 build/
make test         # 執行 C unit tests 與 applet shell tests
make smoke        # 執行 end-to-end smoke test
make install-man  # 安裝 man pages 到 /usr/local/share/man/man1（可用 MANDIR= 覆蓋）
make clean        # 移除 build artifacts
```

安裝後可透過 `man stream_merge`、`man log_parse`、`man clip_store` 查閱文件；
未安裝時可直接用 `man ./man/stream_merge.1` 預覽。

目前測試涵蓋：

- `libpipeline` inotify、buffer、sentinel helpers。
- `stream_logger` stderr-only logging。
- `log_parse` regex parsing、JSON/CSV output、filter 行為。
- `stream_merge` `.meta.jsonl` sidecar drain、5s window、continuity 與 sentinel 行為。
- `clip_store` append/get/TTL/GC/concurrent writes。
- `pipeline_dispatcher` process orchestration 與 end-to-end DB 寫入。

## 快速 Demo

最小 end-to-end 使用方式：

```bash
make
rm -rf /tmp/stream/demo /tmp/clips.db
mkdir -p /tmp/stream/demo
: > /tmp/stream/demo/demo.bin
: > /tmp/stream/demo/demo.meta.jsonl
./build/pipeline_dispatcher demo /tmp/stream/demo /tmp/clips.db 300 &
pid=$!
printf '\x00\x01\x02\x03' >> /tmp/stream/demo/demo.bin
printf '%s\n' '{"kind":"data","seq":1,"offset":0,"length":4,"ts_ms":1000}' >> /tmp/stream/demo/demo.meta.jsonl
touch /tmp/stream/demo/.pipeline_end
wait "$pid"
cat /tmp/clips.db
```

完整 demo 會在 v2.2 的 benchmark/demo evidence 中補成可重跑腳本，涵蓋多筆 stream、malformed input、TTL/GC 與 failure behavior。

## 目錄結構

```text
.
|-- applets/
|   |-- pipeline_dispatcher.c   # fork + pipe + exec orchestration
|   |-- stream_merge.c          # sidecar-driven clip metadata emitter
|   |-- log_parse.c             # regex parser, JSON/CSV formatter, record filter
|   `-- clip_store.c            # file-backed clip index
|-- lib/
|   |-- libpipeline.{h,c}       # inotify, monotonic time, buffer, sentinel helpers
|   `-- stream_logger.{h,c}     # stderr-only diagnostic logger
|-- man/
|   |-- stream_merge.1          # man page: stream_merge
|   |-- log_parse.1             # man page: log_parse
|   `-- clip_store.1            # man page: clip_store
|-- tests/                      # C unit tests and shell integration tests
|-- .docs/                      # repo-local implementation and design docs
|   |-- core/                   # project overview and compliance summary
|   |-- applets/                # per-applet behavior docs
`-- Makefile
```

## 目前狀態

目前 repo 已完成可測試的 pipeline baseline：

- `pipeline_dispatcher` 可建立 `stream_merge -> log_parse -> clip_store` process pipeline。
- `stream_merge` 可讀取 `.meta.jsonl` sidecar、驗證 session `.bin` 存在、做時間窗與 continuity 檢查，並輸出 clip byte-range metadata JSON Lines；CRC、events merge 與實體 mp4 clip extraction 留作 future work。
- `log_parse` 可做 regex parsing、JSON/CSV output 與 JSONL filter。
- `clip_store` 可寫入 file-backed index，並支援查詢、TTL、GC。
- `libpipeline` 與 `stream_logger` 提供 applet 共用低階 helper。

v2 會優先補齊作業交付需要的文件、收斂與證據：

- v2.0：文件主軸對齊，補 README、compliance matrix、repo docs 與開源文件骨架。
- v2.1：Pipeline 核心收斂，修正 contract mismatch 並抽出必要共用邏輯。
- v2.2：Benchmark 與 Demo 證據，補可重跑 benchmark、compatibility matrix 與 final demo script。

## 文件

- Repo-local docs index：[`./.docs/Home.md`](.docs/Home.md)
- Core docs：[`./.docs/core/`](.docs/core/)
- Applet docs：[`./.docs/applets/`](.docs/applets/)
- Assignment compliance summary：[`./.docs/core/compliance.md`](.docs/core/compliance.md)
- Internal overview：[`./.docs/core/overview.md`](.docs/core/overview.md)
- Man pages：[`man/stream_merge.1`](man/stream_merge.1)、[`man/log_parse.1`](man/log_parse.1)、[`man/clip_store.1`](man/clip_store.1)
- Cross-repo integration contract：Linear integration docs

若 Linear integration docs 與 repo-local docs 衝突，以 Linear 作為跨 repo contract 的 source of truth；repo `.docs/` 則描述本 repo 的實作細節、測試與設計限制。

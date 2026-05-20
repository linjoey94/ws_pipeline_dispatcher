# v2 Gap List

本文件收斂 v2.0 之後剩餘的 code、docs、assignment evidence 缺口，避免後續工作散落在 README、compliance matrix、applet specs 與 Linear issue 中。

## Routing Rules

- v2.1：pipeline code、CLI contract、storage semantics、failure behavior、shared helper 收斂。
- v2.2：benchmark、demo script、compatibility matrix、man/help、final report evidence。
- GRA-19：暫緩到 v2.1/v2.2 接近穩定後再回來補開源交付文件骨架，避免先寫出很快失準的 docs。

## v2.1 Scope

v2.1 的目標是讓目前可跑的 C pipeline 更穩、更一致，並把已知 contract mismatch 修掉。

| Gap | Why It Matters | Planned Issue | Required For Assignment |
| --- | --- | --- | --- |
| `clip_store --ttl 0` 目前代表立即過期，但 milestone 期望可表達不過期 | TTL 語意會影響 demo、測試與使用者理解 | GRA-21 | Yes |
| `pipeline_dispatcher` child exec path / failure propagation 仍可更明確 | final demo 需要能說明 fork/exec/waitpid、exit code 與 failure behavior | GRA-22 | Yes |
| JSON/key parsing helper 分散在 `log_parse` 與 `clip_store` | 重複 ad-hoc parsing 會讓後續修 edge case 變難 | GRA-23 | Optional but useful |
| `stream_merge` 尚未達到提案書中的核心切割能力：以 `.meta.jsonl` 驅動時間窗切割、continuity 檢查、partial clip、idle timeout、event merge | 提案書把 `stream-merge` 定義為核心 applet；但正確 contract 下 `.bin` 應是整個 session 的 binary video buffer，current baseline 卻把 growing file 當 JSON object blob 處理，這不只是功能不足，也是 input model mismatch | GRA-24 | Yes |
| `edge-ws-host` 目前 file-mode artifact contract 仍是 `chunk_NNNN.bin/json`，且預設 path 與下層期望的 `.bin + .meta.jsonl + .pipeline_end` 不一致 | 若上層不先把 session artifact contract 收斂，下層 `stream_merge` 很難在 v2.1 做出可防守版本 | GRA-30 | Yes |
| `clip_store --gc` 目前是 in-place rewrite + `fsync()`，不是 tmp file + rename | docs 已誠實標示，但 storage engine 若要更像可防守的作業成果，需要 crash-safety story | GRA-25 | Strongly recommended |

## v2.2 Scope

v2.2 的目標是把 repo 現有能力轉成可評分、可 demo、可放進 final report 的證據。

| Gap | Why It Matters | Planned Issue | Required For Assignment |
| --- | --- | --- | --- |
| 沒有 report-ready evidence package | 期末報告需要可引用的命令、表格、系統呼叫、結果解讀 | GRA-26 | Yes |
| 目前只有 README quick demo，缺可重跑 final demo script | 老師或組員需要一鍵重跑完整 pipeline 證據，且需要涵蓋 stream-merge gap/idle/partial 場景 | GRA-27 | Yes |
| 沒有 benchmark harness 與 sample dataset | 作業要求需要效能/資源使用證據，不能只用口頭描述；提案書也承諾 synthetic stream、file replay、perf/time 對照 | GRA-28 | Yes |
| 沒有 Toybox/GNU/shell pipeline compatibility matrix | BusyBox-style applet 題目需要說明 UNIX CLI 慣例與比較邊界 | GRA-29 | Yes |

## Linear Issue Plan

v2.1 planned issues:

- GRA-21：修正 clip_store TTL 與 expiry 語意。
- GRA-22：收斂 pipeline_dispatcher process failure 行為。
- GRA-23：整理最小 record/path 共用 helper。
- GRA-24：收斂 stream_merge 提案功能落差與核心切割行為。
- GRA-25：強化 clip_store GC 與 persistence 行為。
- GRA-30：修正 edge-ws-host 的 session artifact contract。

v2.2 planned issues:

- GRA-26：整理 final report evidence package。
- GRA-27：新增可重跑 final demo script。
- GRA-28：新增 benchmark harness 與 sample datasets。
- GRA-29：新增 compatibility matrix 與 CLI help/man 文件。

## Assignment Classification

Baseline required before final submission:

- Three C applets remain buildable and pipe-friendly.
- Pipeline behavior has tests for success, malformed input, sentinel drain, TTL/GC, and failure behavior.
- README, compliance matrix, specs, demo, benchmark, compatibility and report evidence agree with each other.

Nice-to-have if time permits:

- Broader JSON parser support.
- More storage commands such as `--list` and `--delete`.
- More benchmark dimensions beyond basic throughput, latency and memory observations.

Out of scope unless integration requires it:

- WebSocket server behavior.
- ESP32 packet parsing.
- Cross-repo filesystem layout changes.
- Large framework-style rewrite.

## Proposal-Specific stream_merge Gap

原提案書把 `stream-merge` 定義為核心 applet，目標不只是把 JSON object 從 growing file 裡切出來，而是要處理即時影音 chunk pipeline 的控制語意。

目前先以 ESP32 -> `edge-ws-host` 的持久 WebSocket/TCP ingress 為主。這表示 v2.1 最小版不需要先做 UDP 式 drop/reorder/late-packet handling，但仍要防守 session artifact 自己是否有接好。

定案模型是：一個 session 是 `STRT -> many DATA / JSON messages -> END_`。每個 `DATA` message 只是一小段影片資料，會被 append 到同一個 `{session_id}.bin`；因此 `.bin` 是整個 session 的巨大 binary buffer，`stream_merge` 再依 `.meta.jsonl` 從中抽出 5s 等時間窗對應的 byte range。

正確 contract 應是：

- `{session_id}.bin` 保存整個 session 的 binary video buffer。
- `{session_id}.meta.jsonl` 保存最小 sidecar metadata，例如 `kind`、`sequence`、`offset`、`length`、`ts_ms`，必要時再加 events。
- `stream_merge` 依 metadata sidecar 從 `.bin` 決定 clip byte range，再輸出 clip metadata JSONL。

因此目前 baseline 的主要問題不只是少做功能，而是把 `.bin` 誤當成 JSON payload source。

目前已完成：

- 使用 inotify/poll 監聽 growing file 與 `.pipeline_end`。
- 從 append-only source drain 新 bytes。
- 以 brace-balanced framing 切出完整 JSON object。
- 只輸出 `type=clip` records，讓下游 `log_parse --filter type=clip` 與 `clip_store` 可串接。
- sentinel 出現後 drain final bytes 並退出。

這些「已完成」只描述 current baseline，不代表 target architecture 正確。

目前未完成，應納入 GRA-24 或後續 v2.1 拆分：

- 以 `.meta.jsonl` 驅動的時間窗切割 complete clip。
- 以 metadata sidecar 驅動 binary `.bin` 切割，而不是從 `.bin` 解析 JSON。
- `sequence` / `offset` continuity 檢查。
- 最小 `Collecting -> EmitComplete -> EmitPartial -> Reset` 類 FSM。
- gap 發生時輸出 partial clip 並重設 buffer。
- idle timeout。
- CRC32 校驗與重複 chunk 去重。
- 從 meta chunks 解析 events 並和 clip metadata 合併輸出。

不需要在 v2.1 最小版先做：

- UDP 式封包亂序重排。
- late packet acceptance/rejection 複雜狀態。
- 把 `.bin` 真的切出 playable mp4 小檔。

v2.2 驗證應納入：

- synthetic stream 注入 drop/reorder/delay。
- file replay 模式，必要時用 ffmpeg 產生可重跑片段。
- benchmark 中明確說明是否仍有 polling fallback、inotify 事件合併，以及與 GNU/coreutils 類似 pipeline 的比較邊界。

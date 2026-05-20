# stream_merge

`stream_merge` 是 pipeline 的唯一 stream reader。正確 contract 下，它應該讀取上層落下來的 `{session_id}.bin` session-level binary video buffer，搭配 `{session_id}.meta.jsonl` sidecar metadata 判斷 clip 邊界、資料是否有接上與 events，最後輸出 clip metadata JSON Lines 到 stdout。

目前 repo 內的 baseline implementation 仍有明顯 mismatch：它尚未真的把 binary `.bin` 做 clip 切割，而是把 growing file 當成可切出 JSON object 的測試輸入來源。這個 mismatch 已轉進 `GRA-24`，不應再被當成最終設計。

## Responsibilities

- 正確設計下：開啟 `{src_dir}/{session_id}.bin` session-level binary buffer。
- 監聽檔案 `IN_MODIFY` / writer close，並監聽目錄中的 `.pipeline_end`。
- 搭配 sidecar metadata 判斷 chunk 邊界、timestamp、duration、資料 continuity、events。
- 依規則輸出 complete / partial clip metadata JSON Lines。
- 偵測 `.pipeline_end` 後 drain 剩餘 bytes，flush final clip，exit 0。

目前 baseline implementation 實際在做：

- 從 growing file 讀新增 bytes。
- 在內部 buffer 累積 bytes。
- 以 brace-balanced JSON object framing 從測試用 blob 切出完整 object。
- 只輸出看起來是 `"type":"clip"` 的 object；其他 object 直接忽略。
- 每個通過條件的 object 原樣輸出到 stdout，並補一個 `\n`。

## Inputs

```text
stream_merge --src <src_dir> --session <session_id>
```

正確 contract 下的預期檔案：

```text
{src_dir}/{session_id}.bin
{src_dir}/{session_id}.meta.jsonl
{src_dir}/.pipeline_end
```

- `{session_id}.bin`：整個 session 的 binary video buffer，由 `edge-ws-host` 把多個 `DATA` message payload 依序 append 而成。
- `{session_id}.meta.jsonl`：最小 sidecar metadata，至少應有 `kind`、`sequence`、`offset`、`length`、`ts_ms`；JSON events 也可落在這裡。
- `.pipeline_end`：上層收到 `END_` 後建立，表示整個 session 的 WS packet 傳遞已結束。

## Output Contract

stdout 每行是一個完整 JSON object，並以 `\n` 結尾。

正確設計下，stdout 應該是 clip metadata，不是 raw mp4 bytes。下游 `log_parse --filter type=clip` / `clip_store` 只需要讀 metadata record。

在目前 v2.1 收斂方向中，clip metadata 可以先表達 session `.bin` buffer 裡的 byte range 與時間窗，例如 5s clip 對應的 `byte_start` / `byte_end`，不必先要求 `stream_merge` 真的切出實體 mp4 小檔。

目前 baseline implementation 不重建 schema，而是直接轉送從測試 blob 中切出的完整 JSON object。這只是為了讓 v1 pipeline 能跑通，不代表 `.bin` 真正應被當成 JSON payload。

常見輸出 shape：

```json
{"type":"clip","session_id":"sess_001","ts":1747065600,"path":"/tmp/clips/sess_001/1747065600.mp4"}
```

## Sentinel Behavior

`.pipeline_end` 表示上層已收到 `END_`，整個 session 的 WS packet 傳遞已結束，而且 `.bin` / `.meta.jsonl` 已完成寫入。

收到 sentinel 後不應立刻退出；必須先繼續 read 到 EOF，再 flush final clip。

若 process 啟動時 sentinel 已存在，`stream_merge` 仍會先把目前檔案內容 drain 完再退出。

## Stdout / Stderr Rule

- stdout：只放 JSON Lines。
- stderr：log、warning、debug、error。

## Implementation Notes

- `IN_MODIFY` 事件可能被 kernel 合併，不可假設事件數等於 write 次數。
- 每次被喚醒後都應讀到 `read()` 暫時沒有更多資料為止。
- EOF 在看到 sentinel 前不代表 session 結束，只代表目前還沒有新資料。
- 正確設計下，`stream_merge` 不應從 `.bin` 解析 JSON，而應根據 metadata sidecar 從 session binary buffer 抽取 clip byte range。
- 目前假設上層是 WebSocket/TCP，因此不先處理 UDP 式封包亂序；最小版本只需檢查 `sequence` 是否連續，以及 `offset + length` 是否有接上。
- object framing 目前只靠大括號深度與字串跳脫處理，不做完整 JSON schema 驗證；這是 current mismatch，不是目標架構。
- 目前沒有 state file / crash recovery cursor；重啟後會從檔案開頭重新掃描。

## Current Mismatch

目前 code 與目標 architecture 的落差：

- `.bin` 在正確 contract 中是 session-level binary video buffer，不是 JSON payload。
- 目前 code 沒有以 `.meta.jsonl` 驅動的時間窗切割、continuity 檢查、partial clip、idle timeout、CRC32、去重、events extraction。
- 目前輸出的 `clip` object 是 fixture-style metadata pass-through，不是由 `stream_merge` 實際切出 clip 後產生的 metadata。

這些缺口已整理到 `.docs/v2-gap-list.md` 與 Linear `GRA-24`。

## Local Test Focus

- append bytes 後會從正確 offset 繼續讀。
- 多次 write 被合併成一次 event 時不漏資料。
- sentinel 後會 drain final bytes。
- stdout 不混入 log。

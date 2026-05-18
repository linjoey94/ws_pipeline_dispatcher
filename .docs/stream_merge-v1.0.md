# stream_merge

`stream_merge` 是 pipeline 的唯一 stream reader。它讀取上層持續 append 的 `{session_id}.bin`，輸出 clip JSON Lines 到 stdout。

## Responsibilities

- 開啟 `{src_dir}/{session_id}.bin`。
- 監聽檔案 `IN_MODIFY`。
- 從上次 offset 繼續 tail-read。
- 在內部 buffer 累積 bytes。
- 依目前實作策略切出 clip。
- 每個 clip 以一行 JSON 輸出到 stdout。
- 偵測 `.pipeline_end` 後 drain 剩餘 bytes，flush final clip，exit 0。

## Inputs

```text
stream_merge --src <src_dir> --session <session_id>
```

預期檔案：

```text
{src_dir}/{session_id}.bin
{src_dir}/.pipeline_end
```

## Output Contract

stdout 每行是一個完整 JSON object，並以 `\n` 結尾。

至少需要支援後續 `log_parse --filter type=clip` 與 `clip_store` 使用的欄位：

```json
{"type":"clip","session_id":"sess_001","ts":1747065600,"path":"/tmp/clips/sess_001/1747065600.mp4"}
```

## Sentinel Behavior

`.pipeline_end` 表示上層已 close write stream。

收到 sentinel 後不應立刻退出；必須先繼續 read 到 EOF，再 flush final clip。

## Stdout / Stderr Rule

- stdout：只放 JSON Lines。
- stderr：log、warning、debug、error。

## Implementation Notes

- `IN_MODIFY` 事件可能被 kernel 合併，不可假設事件數等於 write 次數。
- 每次被喚醒後都應讀到 `read()` 暫時沒有更多資料為止。
- EOF 在看到 sentinel 前不代表 session 結束，只代表目前還沒有新資料。
- 若需要 crash recovery，使用 state file 記錄 cursor；這是 repo-local 實作細節，不寫入 Linear contract。

## Local Test Focus

- append bytes 後會從正確 offset 繼續讀。
- 多次 write 被合併成一次 event 時不漏資料。
- sentinel 後會 drain final bytes。
- stdout 不混入 log。

# pipeline_dispatcher

`pipeline_dispatcher` 是本 repo 的 C entry point。它只負責建立 pipeline，不處理業務資料。

跨 repo 啟動時機與參數來源以 Linear 為準。

## Responsibilities

- 建立兩條 UNIX pipe。
- `fork()` / `exec()` 三個 applet：`stream_merge`、`log_parse`、`clip_store`。
- 正確設定 stdin / stdout fd。
- 關閉 parent process 不需要的 pipe fd。
- `waitpid()` 等待 child processes。
- 將 child failure 轉成 dispatcher exit code。

## CLI

```text
pipeline_dispatcher <session_id> <src_dir> <db_path> <ttl_seconds>
```

參數：

- `session_id`：session/event id。
- `src_dir`：stream session directory。
- `db_path`：clip index path。
- `ttl_seconds`：clip index TTL。

## Pipe Topology

```text
stream_merge --src <src_dir> --session <session_id>
  | log_parse --filter type=clip
  | clip_store --db <db_path> --ttl <ttl_seconds>
```

## Exit Codes

- `0`：所有 child processes 正常退出。
- `1`：參數錯誤。
- `2`：pipe / fork / exec setup 失敗。
- `3`：任一 child process 非 0 exit 或被 signal 結束。

## Implementation Notes

- `pipeline_dispatcher` 不應讀寫 clip JSON。
- stdout 預設不輸出診斷訊息。
- 所有診斷訊息走 stderr。
- child exec path 要避免依賴 caller working directory，若目前尚未處理，需在實作或 wrapper 中固定搜尋路徑。

## Local Test Focus

- 參數不足時回傳 `1`。
- pipe / fork failure path 有清理已開 fd。
- 任一 child exit non-zero 時 dispatcher 回傳 `3`。
- 正常 EOF cascade 不留下 zombie process。

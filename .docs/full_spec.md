# ws_pipeline_dispatcher Internal Spec

本文件只描述 `ws_pipeline_dispatcher` repo 內部如何工作。跨 repo contract 請看 Linear：

- [Simplified Integration Spec](https://linear.app/grason/document/simplified-integration-spec-bcfbf6ee3e4c)

若本文件與 Linear 衝突，以 Linear 為準。

## Repo Role

`ws_pipeline_dispatcher` 是 C / UNIX pipeline。它不接 WebSocket，也不負責 ESP32 packet parsing。

它負責：

- 接收 `edge-ws-host` 透過 `spawn()` 傳入的 CLI 參數。
- 讀取 `/tmp/stream/{session_id}/{session_id}.bin`。
- 監聽 `.pipeline_end` 作為 session 結束訊號。
- 產生 clip JSON Lines。
- 過濾 `type=clip`。
- 寫入 `/tmp/clips.db` 純文字 index。

## Internal Pipeline

```text
pipeline_dispatcher
  ├─ stream_merge  stdout ── pipe_1 ──> log_parse
  ├─ log_parse     stdout ── pipe_2 ──> clip_store
  └─ clip_store    writes /tmp/clips.db
```

## Runtime Inputs

`pipeline_dispatcher` 由 `edge-ws-host` 在 `STRT` 後啟動。

```text
pipeline_dispatcher <session_id> <src_dir> <db_path> <ttl_seconds>
```

目前約定：

- `session_id` 等同 `eventId`。
- `src_dir` 是 `/tmp/stream/{session_id}/`。
- `db_path` 預設是 `/tmp/clips.db`。
- `ttl_seconds` 由上層決定。

## Filesystem Assumptions

本 repo 假設上層已建立以下 layout：

```text
/tmp/stream/{session_id}/
    {session_id}.bin
    {session_id}.meta.jsonl
    .pipeline_end
```

`{session_id}.bin` 是 append-only growing blob。`stream_merge` 以 tail-read 方式讀取新增 bytes。

`.pipeline_end` 表示上層已完成寫入。它不是啟動 pipeline 的 trigger。

## Stdout / Stderr Rule

stdout 是資料流，只能放下一層要讀的 structured output。

stderr 是診斷流，所有 log、warning、error 都必須走 stderr。

這條規則很重要，否則 UNIX pipe 會被 log 污染。

## Error Propagation

任一 child process 異常時，pipe 會自然 EOF，其他 child 會收束。`pipeline_dispatcher` 用 `waitpid()` 收集狀態，並用 exit code 回報給上層。

## What Belongs Here

- applet CLI 與 exit code。
- repo-local build/test/run 注意事項。
- C 實作細節與內部資料結構。
- debug、logging、failure handling。

## What Belongs In Linear

- `edge-ws-host` 與本 repo 的 contract。
- packet format。
- filesystem layout 的跨 repo 決策。
- runtime sequence。
- current gaps / project tracking。

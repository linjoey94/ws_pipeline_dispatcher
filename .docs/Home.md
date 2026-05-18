# ws_pipeline_dispatcher Docs

這裡只記錄 `ws_pipeline_dispatcher` repo 內部工作需要知道的內容。

跨 repo 的 integration contract 以 Linear 為準：

- [Simplified Integration Spec](https://linear.app/grason/document/simplified-integration-spec-bcfbf6ee3e4c)
- [Overview](https://linear.app/grason/document/integration-spec-overview-d9089c6021ea)
- [Packet Contract](https://linear.app/grason/document/integration-spec-packet-contract-e6e2904eec34)
- [edge-ws-host](https://linear.app/grason/document/integration-spec-edge-ws-host-b6f1f719f434)
- [ws_pipeline_dispatcher](https://linear.app/grason/document/integration-spec-ws-pipeline-dispatcher-54a13943053e)
- [Runtime Sequence](https://linear.app/grason/document/integration-spec-runtime-sequence-6aeaad3ad100)
- [Current Gaps](https://linear.app/grason/document/integration-spec-current-gaps-1f49ef512314)

若本 repo docs 與 Linear 衝突，以 Linear 為準。

## How To Read

1. 先看 `full_spec`，了解本 repo 的內部邊界。
2. 修改某個 applet 前，看對應頁面。
3. 跨 repo 行為不要在這裡新增完整規格，請更新 Linear。

## Pages

- `full_spec`：本 repo 的內部工作總覽。
- `pipeline_dispatcher-v1.0`：entry point、fork/exec、exit code。
- `stream_merge-v1.0`：讀取 growing blob、sentinel、clip JSON Lines。
- `log_parse-v1.0`：regex-based 結構化日誌解析、JSON/CSV 輸出、integration filter。
- `clip_store-v1.0`：純文字 index 與查詢行為。
- `libpipeline-v1.0`：共用低階 helper。
- `stream_logger-v1.0`：stderr 診斷輸出。

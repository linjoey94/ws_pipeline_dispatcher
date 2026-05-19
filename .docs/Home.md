# ws_pipeline_dispatcher Docs

這裡只記錄 `ws_pipeline_dispatcher` repo 內部實作、設計與維護需要知道的內容。

跨 repo 的 integration contract 以 Linear 為準：

- [Simplified Integration Spec](https://linear.app/grason/document/simplified-integration-spec-bcfbf6ee3e4c)
- [Overview](https://linear.app/grason/document/integration-spec-overview-d9089c6021ea)
- [Packet Contract](https://linear.app/grason/document/integration-spec-packet-contract-e6e2904eec34)
- [edge-ws-host](https://linear.app/grason/document/integration-spec-edge-ws-host-b6f1f719f434)
- [ws_pipeline_dispatcher](https://linear.app/grason/document/integration-spec-ws-pipeline-dispatcher-54a13943053e)
- [Runtime Sequence](https://linear.app/grason/document/integration-spec-runtime-sequence-6aeaad3ad100)
- [Current Gaps](https://linear.app/grason/document/integration-spec-current-gaps-1f49ef512314)

若本 repo docs 與 Linear 衝突，以 Linear 為準。

## Doc Ownership

- repo docs：描述這個 repo 內部如何 build、run、test、組 pipeline，與每個 applet/lib 的責任邊界。
- Linear docs：描述跨 repo contract，例如 `edge-ws-host` 如何啟動本 repo、packet format、runtime sequence 與 integration gap tracking。

如果某個問題同時牽涉本 repo 與其他 repo，先在 Linear 更新 contract，再回來修 repo-local 文件。

## How To Read

1. 先看 `README.md`，了解作業主軸與快速執行方式。
2. 再看 `compliance-matrix`，確認作業要求、目前狀態與後續 milestone。
3. 接著看 `full_spec`，了解本 repo 的內部邊界、資料流、process topology 與 stdout/stderr 規則。
4. 要排 v2.1/v2.2 工作時，看 `v2-gap-list`。
5. 要改 orchestration 或 pipeline 行為時，先讀 `pipeline_dispatcher-v1.0`。
6. 要改某個 applet 時，讀對應 applet spec，再看 `libpipeline-v1.0` / `stream_logger-v1.0` 是否已定義共用行為。
7. 跨 repo 行為不要在這裡新增完整規格，請更新 Linear。

## Suggested Reading By Task

- 想快速跑起來：`README.md` -> `full_spec` -> `pipeline_dispatcher-v1.0`
- 想確認課程要求是否對齊：`README.md` -> `compliance-matrix` -> `full_spec`
- 想規劃 v2.1/v2.2：`compliance-matrix` -> `v2-gap-list` -> Linear milestone issues
- 想修改 `stream_merge` / `log_parse` / `clip_store`：`full_spec` -> applet spec -> `libpipeline-v1.0` / `stream_logger-v1.0`
- 想追 integration contract：直接看 Linear，不要只依賴 repo docs

## Pages

- `full_spec`：本 repo 的內部總覽，先定義 role、boundary、data flow 與 topology。
- `compliance-matrix`：作業 B + 方向三要求對照表，含完成狀態與 follow-up routing。
- `v2-gap-list`：v2.1/v2.2 缺口列表與 Linear issue planning。
- `pipeline_dispatcher-v1.0`：entry point、fork/exec、pipe topology、exit code。
- `stream_merge-v1.0`：讀取 growing blob、sentinel、clip JSON Lines。
- `log_parse-v1.0`：regex-based 結構化日誌解析、JSON/CSV 輸出、integration filter。
- `clip_store-v1.0`：純文字 index、TTL、GC 與查詢行為。
- `libpipeline-v1.0`：共用低階 helper，例如 inotify watch 與 sentinel helper。
- `stream_logger-v1.0`：stderr-only 診斷輸出與 log API。

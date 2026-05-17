# ws_pipeline_dispatcher

C 子系統，負責把 Fastify 上層（`edge-ws-host`）落地的 chunk 檔案
經由 `stream_merge → log_parse → clip_store` 三段 UNIX pipeline 處理，
最終寫入 `clips.db`。

詳細規格見 [`.docs/full_spec.md`](.docs/full_spec.md) 及各模組文件。

## 編譯

```bash
make           # 編譯所有 binary 到 build/
make test      # 執行 libpipeline 單元測試
make smoke     # end-to-end 骨架煙霧測試
make clean
```

## 目錄結構

```
.
├── applets/
│   ├── pipeline_dispatcher.c   ← C entry point (fork + pipe + exec)
│   ├── stream_merge.c          ← v1 skeleton stub
│   ├── log_parse.c             ← v1 skeleton stub
│   └── clip_store.c            ← v1 skeleton stub
├── lib/
│   ├── libpipeline.{h,c}       ← 共用基礎函式（inotify、time、JSON 壓縮、sentinel）
│   └── stream_logger.{h,c}     ← stderr-only LOG_* 巨集
├── tests/
│   └── test_libpipeline.c
├── .docs/                      ← 規格與設計文件
└── Makefile
```

## v1 範圍

本 PR 提供**最小可編譯骨架**：

- `pipeline_dispatcher` 真的會 `pipe()` + `fork()` + `exec()` 把三個 applet 串成 pipeline。
- 三個 applet 為 stub：`stream_merge` 送一條 heartbeat、`log_parse` pass-through、
  `clip_store` append-only 寫檔。
- `libpipeline` 與 `stream_logger` 為 v1 完整實作（小但可用）。

真實的 ingest／filter／store 邏輯將在後續 PR 取代各 applet 的 `main()`。

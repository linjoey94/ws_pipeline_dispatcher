# Compliance Matrix

本文件對照 UNIX 系統程式設計期末專題選項 B「BusyBox 工具擴充」與方向三「Embedded Data Pipeline」的要求，標示 `ws_pipeline_dispatcher` 目前的完成狀態、repo artifact 與後續 milestone。

狀態定義：

- `Done`：目前 repo 已有可編譯、可測試的實作或文件。
- `Partial`：核心方向已存在，但仍需在 v2.0/v2.1/v2.2 收斂文件、CLI 或證據。
- `Planned in v2.1`：屬於 pipeline code/contract 收斂。
- `Planned in v2.2`：屬於 benchmark、demo、compatibility 或 final evidence。

## Scope Boundary

`ws_pipeline_dispatcher` 不實作 WebSocket server，也不負責 ESP32 packet parsing。跨 repo 啟動時機、packet contract 與 filesystem layout 以 Linear integration docs 為 source of truth；本 repo 的交付範圍是 C applets、UNIX pipeline orchestration、structured parsing/filtering、file-backed storage 與 repo-local 文件/測試。

## Requirement Matrix

| Requirement | Status | Repo Artifact | Evidence | Gap / Follow-up |
| --- | --- | --- | --- | --- |
| 使用 C 語言實作新的 BusyBox-style applets | Done | `applets/stream_merge.c`, `applets/log_parse.c`, `applets/clip_store.c` | `Makefile` builds all applets into `build/`; `make test` covers applet behavior | BusyBox upstream integration packaging is not in scope for v2.0; document CLI/man shape in GRA-19 |
| 至少 3 個新的 applet | Done | `stream_merge`, `log_parse`, `clip_store` | README applet section; applet tests under `tests/` | None for course baseline |
| applets 可用 UNIX pipe 組成完整資料管線 | Done | `applets/pipeline_dispatcher.c` | `pipeline_dispatcher` builds `stream_merge -> log_parse -> clip_store`; `tests/test_pipeline_dispatcher.sh` | v2.1 may tighten contract mismatch and child exec path details |
| 使用 process management 建立 pipeline | Done | `pipeline_dispatcher` | Uses `pipe()`, `fork()`, `execv()`, `waitpid()` as documented in README and `.docs/pipeline_dispatcher-v1.0.md` | v2.1 can refine failure propagation docs/tests if needed |
| stdout/stderr 遵守 UNIX stream discipline | Done | all applets, `lib/stream_logger.*` | README system programming section; `tests/test_stream_logger.c`; applet docs | Continue enforcing in future applet changes |
| 方向三：結構化日誌解析器 | Done | `applets/log_parse.c` | `log_parse --regex ... --fields ... --format json`; `tests/test_log_parse.sh` | v2.0 GRA-16 should align docs with exact implemented edge cases |
| 支援 JSON/CSV structured output | Done | `log_parse` | README and `.docs/log_parse-v1.0.md`; shell tests cover JSON and CSV output | None for course baseline |
| 支援欄位選取與 regex capture mapping | Done | `log_parse --fields` | `.docs/log_parse-v1.0.md`; `tests/test_log_parse.sh` | Document unsupported regex features if discovered during GRA-16 |
| 支援串流資料過濾 | Done | `log_parse --filter key=value` | Integration mode keeps `type=clip` JSON Lines; tests cover filter behavior | v2.1 may expand filter contract only if required by demo |
| 支援串流資料轉換 | Partial | `stream_merge`, `log_parse` | `stream_merge` frames growing input into JSON Lines; `log_parse` converts regex logs to JSON/CSV | v2.1 should clarify binary/video payload boundary and final record framing contract |
| 支援 growing file / append-only stream | Done | `applets/stream_merge.c`, `lib/libpipeline.*` | inotify/poll behavior documented in `.docs/stream_merge-v1.0.md`; `tests/test_stream_merge.sh` | v2.1 can add more edge-case tests for long sessions if needed |
| 使用 sentinel 表示 stream 結束並 drain final bytes | Done | `stream_merge`, `libpipeline` | `.pipeline_end` behavior documented; stream_merge tests cover drain behavior | None for current baseline |
| 方向三：輕量級資料儲存引擎 | Done | `applets/clip_store.c` | file-backed index at `--db`; `tests/test_clip_store.sh` | v2.1 should keep docs honest about supported CRUD surface |
| File-backed key-value index | Done | `clip_store` | DB format documented as `key<TAB>value<TAB>expire_at`; tests cover append/get | None for course baseline |
| TTL and GC behavior | Done | `clip_store --ttl`, `clip_store --gc` | README testing section; `tests/test_clip_store.sh` | GRA-16 should avoid overstating crash-safety if not fully implemented |
| Concurrent write safety | Partial | `clip_store`, file locking behavior | tests mention concurrent writes; README lists coverage | v2.1 should verify/extend lock semantics if final demo stresses concurrency |
| 提取共用邏輯為內部函式庫 | Done | `lib/libpipeline.*`, `lib/stream_logger.*` | `tests/test_libpipeline.c`, `tests/test_stream_logger.c`; `.docs/libpipeline-v1.0.md` | None for current baseline |
| applet CLI 有清楚 usage/help 方向 | Partial | applet docs, README commands | README documents primary commands; applet docs document expected CLI shape | GRA-19 should add man/help document skeleton and final help contract |
| Open-source delivery docs | Partial | `README.md`, `.docs/` | README now explains project, build/test/demo and status | GRA-19 should add `CONTRIBUTING.md` and open-source delivery document entries |
| Build instructions | Done | `Makefile`, README | `make`, `make test`, `make smoke`, `make clean` documented | Makefile comments still contain older skeleton wording; can be cleaned in a separate docs pass if desired |
| Test coverage for applets and libraries | Done | `tests/`, `Makefile` | `make test` runs C unit tests and shell applet tests | v2.2 may add benchmark/demo evidence tests, not required for baseline correctness |
| Runnable end-to-end demo | Partial | README quick demo, `pipeline_dispatcher` | README has a minimal demo that writes `/tmp/clips.db` | v2.2 should add replayable final demo script with malformed input, TTL/GC and failure behavior |
| Benchmark evidence | Planned in v2.2 | none yet | README states benchmark/demo evidence is follow-up | GRA-19 defines benchmark document shape; v2.2 fills numbers and comparison data |
| Toybox/GNU compatibility matrix | Planned in v2.2 | none yet | Identified as assignment delivery item | GRA-19 defines compatibility matrix skeleton; v2.2 validates behavior |
| Final report / demo evidence can reference repo artifacts | Partial | README, this matrix, `.docs/` | README provides project axis; this matrix provides requirement mapping | GRA-20 should collect remaining mismatches into one gap list |

## Milestone Routing

Remaining work is intentionally split by milestone:

- v2.0：文件主軸對齊，包含 README、compliance matrix、repo docs、applet docs、open-source delivery skeleton 與 gap list。
- v2.1：pipeline code/contract 收斂，包含 stream framing、child process behavior、storage semantics 與 integration mismatch。
- v2.2：benchmark/demo evidence，包含 final demo script、compatibility matrix、benchmark numbers 與 report-ready artifacts。

v2.1/v2.2 的單一追蹤入口是 `.docs/v2-gap-list.md`，Linear milestone issues 依該文件拆分。

## Current High-Risk Gaps

- `Makefile` comments still mention earlier skeleton wording even though behavior and README have moved past that framing.
- man/help pages and compatibility matrix are not complete yet; they should be scoped to GRA-19 and v2.2 rather than silently implied as done.
- benchmark evidence is not present yet; README correctly routes it to v2.2.
- applet docs need a dedicated pass in GRA-16 to ensure they describe implemented behavior without overstating future crash-safety or CRUD features.

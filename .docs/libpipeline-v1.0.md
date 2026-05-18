# libpipeline

`libpipeline` 放 repo 內 applet 共用的低階 helper。它不包含業務邏輯。

## Responsibilities

- inotify watch helper。
- monotonic time helper。
- sentinel filename check。
- 其他 applet 共用、但不適合放在單一 applet 的 POSIX utility。

`libpipeline` 不提供 JSON parser、JSON filter 或 JSON 壓縮 helper。JSON schema 與資料驗證屬於上層 applet 的業務邏輯；若需要解析 JSON，應在呼叫端使用完整 JSON library（例如 cJSON）處理，避免在底層 helper 實作不完整、容易產生漏洞或資料破壞風險的 ad-hoc parser。

## Public Helpers

目前最小介面：

```c
int pipeline_open_dir_watch(const char *dir_path, int *watch_descriptor);
int pipeline_open_file_watch(const char *file_path, int *watch_descriptor);
int64_t pipeline_get_monotonic_time_ms(void);
int pipeline_path_is_sentinel(const char *filename);
```

## Sentinel

sentinel 檔名固定：

```text
.pipeline_end
```

`pipeline_path_is_sentinel()` 只做 basename 比對，不讀取檔案內容。

## Inotify Semantics

`pipeline_open_dir_watch()` 用於目錄層級的 completed-file 通知，至少監聽 `IN_CLOSE_WRITE | IN_MOVED_TO`。這讓呼叫端能處理檔案被寫完後 close，或先寫到暫存檔再 rename/move 進目錄的情境。

`pipeline_open_file_watch()` 用於單一檔案的 append/modify 通知，至少監聽 `IN_MODIFY`。`IN_CLOSE_WRITE` 只會在 writer 關閉可寫 fd 時觸發；如果 writer 長時間開著 fd 並持續 append，單靠 `IN_CLOSE_WRITE` 會讓呼叫端在 close 前收不到任何內容變更通知。因此 file watch 同時監聽 `IN_MODIFY` 與 `IN_CLOSE_WRITE`，分別支援 growing file 的即時更新與 writer close 的完成訊號。

## Design Rules

- 不解析 clip JSON schema。
- 不提供 JSON parser、JSON filter 或 JSON 壓縮 helper。
- 不讀寫 `/tmp/clips.db`。
- 不持有 hidden global business state。
- 回傳錯誤時使用 return value + errno，呼叫端負責 log。

## Local Test Focus

- inotify fd 建立失敗時正確回傳錯誤。
- `pipeline_open_file_watch()` 至少監聽 `IN_MODIFY`。
- `pipeline_open_file_watch()` 可觀察到 append/modify 事件，不只依賴 `IN_CLOSE_WRITE`。
- `pipeline_path_is_sentinel(".pipeline_end") == 1`。
- `pipeline_path_is_sentinel("/path/to/.pipeline_end") == 1`。
- `pipeline_path_is_sentinel()` 對其他檔名回傳 0。
- `pipeline_get_monotonic_time_ms()` 使用 monotonic clock，不受 system time 調整影響。

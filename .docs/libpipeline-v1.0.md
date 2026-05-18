# libpipeline

`libpipeline` 放 repo 內 applet 共用的低階 helper。它不包含業務邏輯。

## Responsibilities

- inotify watch helper。
- monotonic time helper。
- sentinel filename check。
- 小型 JSON line helper。
- 其他 applet 共用、但不適合放在單一 applet 的 POSIX utility。

## Public Helpers

目前最小介面：

```c
int pipeline_watch_dir(const char *dir_path, int *watch_descriptor);
int pipeline_watch_file(const char *file_path, int *watch_descriptor);
int64_t pipeline_now_ms(void);
int pipeline_compress_json(const char *src, char *dst, size_t dst_size);
int pipeline_is_sentinel(const char *filename);
```

## Sentinel

sentinel 檔名固定：

```text
.pipeline_end
```

`pipeline_is_sentinel()` 只做 basename 比對，不讀取檔案內容。

## Design Rules

- 不解析 clip JSON schema。
- 不讀寫 `/tmp/clips.db`。
- 不持有 hidden global business state。
- 回傳錯誤時使用 return value + errno，呼叫端負責 log。

## Local Test Focus

- inotify fd 建立失敗時正確回傳錯誤。
- `pipeline_is_sentinel(".pipeline_end") == 1`。
- `pipeline_is_sentinel()` 對其他檔名回傳 0。
- `pipeline_now_ms()` 使用 monotonic clock，不受 system time 調整影響。

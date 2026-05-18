# stream_logger

`stream_logger` 是 repo-local 診斷工具。它不參與業務資料流。

## Responsibilities

- 提供 `LOG_DEBUG`、`LOG_INFO`、`LOG_WARN`、`LOG_ERROR`、`LOG_FATAL`。
- 支援輸出到 stderr 或 log file。
- 保持 stdout 乾淨，避免污染 pipeline data stream。

## Public Shape

```c
typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO  = 1,
    LOG_WARN  = 2,
    LOG_ERROR = 3,
    LOG_FATAL = 4
} LogLevel;

int  logger_init(const LoggerConfig *cfg);
void logger_close(void);
void logger_write(LogLevel level, const char *file, int line, const char *fmt, ...);
```

## Integration Rule

在 pipeline applet 中，diagnostic log 必須走 stderr 或指定 log file，不得寫到 stdout。

stdout 是下一層 applet 的 input。

## Repo-Local Features

以下功能可以保留為內部實作，不需要寫進 Linear：

- log rotation。
- JSON log format。
- per-session log field。
- level filter。

## Local Test Focus

- level filter 生效。
- stderr output 不寫入 stdout。
- JSON log mode 產生合法 JSON line。
- log rotation 不遺失目前 log file。

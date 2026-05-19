# stream_logger

`stream_logger` 是 repo-local 診斷工具。它只負責 applet 的 diagnostic output，不參與業務資料流。

在本 repo 中，stdout 是 UNIX pipeline 的資料通道；所有 log、warning、error 都必須走 stderr，避免污染下一層 applet 的 input。

## Responsibilities

- 提供 applet 共用的 stderr-only logging helper。
- 支援基本 log levels：`DEBUG`、`INFO`、`WARN`、`ERROR`。
- 支援每個 applet 設定 module tag，例如 `stream_merge`、`log_parse`、`clip_store`、`dispatcher`。
- 每筆 log 輸出 timestamp、level、tag 與訊息。
- 保持 stdout 完全乾淨，不得輸出任何 diagnostic log。

## Public API

v1 public shape：

```c
typedef enum {
    LOG_LVL_DEBUG = 0,
    LOG_LVL_INFO  = 1,
    LOG_LVL_WARN  = 2,
    LOG_LVL_ERROR = 3
} log_level_t;

void stream_logger_set_tag(const char *tag);
void stream_logger_log(log_level_t lvl, const char *fmt, ...);

#define LOG_DEBUG(...) stream_logger_log(LOG_LVL_DEBUG, __VA_ARGS__)
#define LOG_INFO(...)  stream_logger_log(LOG_LVL_INFO,  __VA_ARGS__)
#define LOG_WARN(...)  stream_logger_log(LOG_LVL_WARN,  __VA_ARGS__)
#define LOG_ERROR(...) stream_logger_log(LOG_LVL_ERROR, __VA_ARGS__)
```

## Output Format

stderr 每行一筆 log。建議格式：

```text
2026-05-18T13:47:47.481Z [INFO] stream_merge: message
```

欄位：

- UTC timestamp。
- log level。
- module tag。
- formatted message。

## Integration Rule

在 pipeline applet 中：

- stdout：只允許 structured data，例如 JSON Lines、CSV 或查詢結果。
- stderr：所有 diagnostic log、warning、error。

這條規則是硬性限制，否則 UNIX pipe 會被 log 污染。

## Out Of Scope For v1

以下功能暫不列入 v1 必要範圍，可作為後續 extension：

- log file output。
- log rotation。
- JSON log format。
- per-session log field。
- runtime level filter。
- `FATAL` level。

## Local Test Focus

- `LOG_DEBUG`、`LOG_INFO`、`LOG_WARN`、`LOG_ERROR` 會輸出到 stderr。
- logger 不會寫入 stdout。
- `stream_logger_set_tag()` 後，log line 會包含該 tag。
- `stream_logger_set_tag(NULL)` 不應 crash。
- log line 至少包含 timestamp、level、tag 與 message。

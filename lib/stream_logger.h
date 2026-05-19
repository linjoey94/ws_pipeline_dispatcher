#ifndef STREAM_LOGGER_H
#define STREAM_LOGGER_H

#include <stdio.h>
#include <stdarg.h>

typedef enum {
    LOG_LVL_DEBUG = 0,
    LOG_LVL_INFO  = 1,
    LOG_LVL_WARN  = 2,
    LOG_LVL_ERROR = 3
} log_level_t;

/**
 * @brief Set the process-level logger tag.
 *
 * Each applet should call this once near the start of main(). Passing NULL is
 * ignored and leaves the existing tag unchanged.
 *
 * @param tag Module tag to include in subsequent log lines.
 */
void stream_logger_set_tag(const char *tag);

/**
 * @brief Write a formatted diagnostic message to stderr.
 *
 * The format string and variadic arguments follow printf-style rules.
 *
 * @param lvl Log level for this message.
 * @param fmt printf-style format string.
 * @param ... Additional arguments for fmt.
 */
void stream_logger_log(log_level_t lvl, const char *fmt, ...);

#define LOG_DEBUG(...) stream_logger_log(LOG_LVL_DEBUG, __VA_ARGS__)
#define LOG_INFO(...)  stream_logger_log(LOG_LVL_INFO,  __VA_ARGS__)
#define LOG_WARN(...)  stream_logger_log(LOG_LVL_WARN,  __VA_ARGS__)
#define LOG_ERROR(...) stream_logger_log(LOG_LVL_ERROR, __VA_ARGS__)

#endif /* STREAM_LOGGER_H */

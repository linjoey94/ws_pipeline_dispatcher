/*
 * stream_logger.c — minimal v1 implementation.
 */

#include "stream_logger.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static char g_tag[32] = "applet";

void stream_logger_set_tag(const char *tag)
{
    if (tag == NULL) {
        return;
    }
    strncpy(g_tag, tag, sizeof(g_tag) - 1);
    g_tag[sizeof(g_tag) - 1] = '\0';
}

static const char *lvl_name(log_level_t lvl)
{
    switch (lvl) {
        case LOG_LVL_DEBUG: return "DEBUG";
        case LOG_LVL_INFO:  return "INFO";
        case LOG_LVL_WARN:  return "WARN";
        case LOG_LVL_ERROR: return "ERROR";
        default:            return "?";
    }
}

void stream_logger_log(log_level_t lvl, const char *fmt, ...)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    gmtime_r(&ts.tv_sec, &tm);
    char tbuf[32];
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%dT%H:%M:%S", &tm);

    fprintf(stderr, "%s.%03ldZ [%s] %s: ",
            tbuf, ts.tv_nsec / 1000000, lvl_name(lvl), g_tag);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fputc('\n', stderr);
}

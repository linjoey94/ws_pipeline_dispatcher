/*
 * stream_merge.c — v1 SKELETON STUB
 *
 * Real implementation will:
 *   - parse --src / --session CLI args
 *   - call pipeline_watch_dir() on --src
 *   - read chunk_NNNN.bin files (seq from filename, no binary header)
 *   - emit one compressed JSON line per 5s window to stdout
 *   - exit 0 when .pipeline_end sentinel observed
 *
 * v1 skeleton: parse args, log lifecycle, exit 0 immediately. This lets
 * pipeline_dispatcher be tested end-to-end without the real ingest logic.
 */

#include "libpipeline.h"
#include "stream_logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    stream_logger_set_tag("stream_merge");

    const char *src = NULL;
    const char *session = NULL;
    for (int i = 1; i + 1 < argc; ++i) {
        if (strcmp(argv[i], "--src") == 0) {
            src = argv[++i];
        } else if (strcmp(argv[i], "--session") == 0) {
            session = argv[++i];
        }
    }

    LOG_INFO("skeleton start session=%s src=%s",
             session ? session : "(null)", src ? src : "(null)");

    /* v1 skeleton: emit one heartbeat JSON line so the pipe is exercised. */
    char buf[256];
    int64_t now = pipeline_now_ms();
    snprintf(buf, sizeof(buf),
             "{\"type\":\"heartbeat\",\"session_id\":\"%s\",\"ts\":%lld}\n",
             session ? session : "unknown", (long long)now);

    char compressed[256];
    int n = pipeline_compress_json(buf, compressed, sizeof(compressed));
    if (n > 0) {
        fwrite(compressed, 1, (size_t)n, stdout);
        fputc('\n', stdout);
        fflush(stdout);
    }

    LOG_INFO("skeleton exit 0");
    return 0;
}

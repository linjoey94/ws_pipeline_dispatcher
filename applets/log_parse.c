/*
 * log_parse.c — v1 SKELETON STUB
 *
 * Real implementation will:
 *   - parse --filter key=value CLI arg
 *   - read JSON Lines from stdin
 *   - pass through only those matching the filter
 *
 * v1 skeleton: do a literal pass-through (cat-like). Sufficient to verify
 * pipe wiring inside pipeline_dispatcher.
 */

#include "stream_logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    stream_logger_set_tag("log_parse");

    const char *filter = NULL;
    for (int i = 1; i + 1 < argc; ++i) {
        if (strcmp(argv[i], "--filter") == 0) {
            filter = argv[++i];
        }
    }
    LOG_INFO("skeleton start filter=%s", filter ? filter : "(none)");

    char *line = NULL;
    size_t cap = 0;
    ssize_t n;
    while ((n = getline(&line, &cap, stdin)) > 0) {
        fwrite(line, 1, (size_t)n, stdout);
        fflush(stdout);
    }
    free(line);

    LOG_INFO("skeleton exit 0");
    return 0;
}

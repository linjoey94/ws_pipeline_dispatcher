/*
 * clip_store.c — v1 SKELETON STUB
 *
 * Real implementation will:
 *   - parse --db and --ttl CLI args
 *   - read JSON Lines from stdin
 *   - append "{key}\t{path}\t{expires_at}\n" rows to --db
 *
 * v1 skeleton: append each incoming line verbatim to --db (when given),
 * otherwise just count and log. Exit 0 on EOF.
 */

#include "stream_logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    stream_logger_set_tag("clip_store");

    const char *db = NULL;
    const char *ttl = NULL;
    for (int i = 1; i + 1 < argc; ++i) {
        if (strcmp(argv[i], "--db") == 0)  db  = argv[++i];
        if (strcmp(argv[i], "--ttl") == 0) ttl = argv[++i];
    }
    LOG_INFO("skeleton start db=%s ttl=%s",
             db ? db : "(null)", ttl ? ttl : "(null)");

    FILE *out = NULL;
    if (db != NULL) {
        out = fopen(db, "a");
        if (out == NULL) {
            LOG_WARN("cannot open db=%s, falling back to stdout only", db);
        }
    }

    char *line = NULL;
    size_t cap = 0;
    ssize_t n;
    long count = 0;
    while ((n = getline(&line, &cap, stdin)) > 0) {
        if (out != NULL) {
            fwrite(line, 1, (size_t)n, out);
            fflush(out);
        }
        ++count;
    }
    free(line);
    if (out != NULL) fclose(out);

    LOG_INFO("skeleton exit 0 ingested=%ld", count);
    return 0;
}

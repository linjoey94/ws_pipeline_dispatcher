/*
 * clip_store.c -- plain-text clip index persistence applet.
 */

#include "libpipeline.h"
#include "stream_logger.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    char *key;
    char *value;
    long expire_at;
} row_t;

static void usage(const char *prog)
{
    fprintf(stderr, "usage: %s --db <db_path> [--ttl <seconds>] [--get <key>|--gc]\n", prog);
}

static char *json_find_string_value(const char *line, const char *key)
{
    char needle[128];
    int n = snprintf(needle, sizeof(needle), "\"%s\"", key);
    if (n < 0 || (size_t)n >= sizeof(needle)) {
        return NULL;
    }

    const char *p = strstr(line, needle);
    if (p == NULL) {
        return NULL;
    }
    p += (size_t)n;
    while (*p == ' ' || *p == '\t') {
        ++p;
    }
    if (*p++ != ':') {
        return NULL;
    }
    while (*p == ' ' || *p == '\t') {
        ++p;
    }
    if (*p++ != '"') {
        return NULL;
    }

    pipeline_buffer_t out = {0};
    int escaped = 0;
    for (; *p != '\0'; ++p) {
        if (escaped) {
            if (pipeline_buffer_append_char(&out, *p) != 0) {
                pipeline_buffer_free(&out);
                return NULL;
            }
            escaped = 0;
        } else if (*p == '\\') {
            escaped = 1;
        } else if (*p == '"') {
            return out.data == NULL ? strdup("") : out.data;
        } else if (pipeline_buffer_append_char(&out, *p) != 0) {
            pipeline_buffer_free(&out);
            return NULL;
        }
    }

    pipeline_buffer_free(&out);
    return NULL;
}

static char *json_find_scalar_value(const char *line, const char *key)
{
    char needle[128];
    int n = snprintf(needle, sizeof(needle), "\"%s\"", key);
    if (n < 0 || (size_t)n >= sizeof(needle)) {
        return NULL;
    }

    const char *p = strstr(line, needle);
    if (p == NULL) {
        return NULL;
    }
    p += (size_t)n;
    while (*p == ' ' || *p == '\t') {
        ++p;
    }
    if (*p++ != ':') {
        return NULL;
    }
    while (*p == ' ' || *p == '\t') {
        ++p;
    }
    if (*p == '"') {
        return json_find_string_value(line, key);
    }

    const char *start = p;
    while (*p != '\0' && *p != ',' && *p != '}' && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
        ++p;
    }
    if (p == start) {
        return NULL;
    }
    size_t len = (size_t)(p - start);
    char *out = malloc(len + 1);
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

static int parse_row(char *line, row_t *row)
{
    char *a = strchr(line, '\t');
    if (a == NULL) {
        return -1;
    }
    char *b = strchr(a + 1, '\t');
    if (b == NULL) {
        return -1;
    }
    *a = '\0';
    *b = '\0';
    char *end = NULL;
    long expire_at = strtol(b + 1, &end, 10);
    if (end == b + 1) {
        return -1;
    }
    row->key = line;
    row->value = a + 1;
    row->expire_at = expire_at;
    return 0;
}

static int load_latest(FILE *fp, const char *key, row_t *out, long now)
{
    char *line = NULL;
    size_t cap = 0;
    int found = 0;

    rewind(fp);
    while (getline(&line, &cap, fp) > 0) {
        line[strcspn(line, "\r\n")] = '\0';
        row_t row = {0};
        if (parse_row(line, &row) != 0) {
            continue;
        }
        if (strcmp(row.key, key) == 0) {
            free(out->key);
            free(out->value);
            out->key = strdup(row.key);
            out->value = strdup(row.value);
            out->expire_at = row.expire_at;
            found = out->key != NULL && out->value != NULL;
        }
    }
    free(line);

    if (!found || out->expire_at <= now) {
        return 0;
    }
    return 1;
}

static int append_row(FILE *fp, const char *key, const char *value, long expire_at)
{
    if (fseek(fp, 0, SEEK_END) != 0) {
        return -1;
    }
    if (fprintf(fp, "%s\t%s\t%ld\n", key, value, expire_at) < 0) {
        return -1;
    }
    return fflush(fp);
}

static int rewrite_gc(FILE *fp, long now)
{
    row_t *rows = NULL;
    size_t len = 0;
    size_t cap = 0;
    char *line = NULL;
    size_t line_cap = 0;

    rewind(fp);
    while (getline(&line, &line_cap, fp) > 0) {
        line[strcspn(line, "\r\n")] = '\0';
        row_t parsed = {0};
        if (parse_row(line, &parsed) != 0) {
            continue;
        }

        size_t idx = len;
        for (size_t i = 0; i < len; ++i) {
            if (strcmp(rows[i].key, parsed.key) == 0) {
                idx = i;
                break;
            }
        }
        if (idx == len) {
            if (len == cap) {
                size_t next = cap == 0 ? 16 : cap * 2;
                row_t *grown = realloc(rows, next * sizeof(rows[0]));
                if (grown == NULL) {
                    free(line);
                    return -1;
                }
                rows = grown;
                cap = next;
            }
            rows[len++] = (row_t){0};
        } else {
            free(rows[idx].key);
            free(rows[idx].value);
        }
        rows[idx].key = strdup(parsed.key);
        rows[idx].value = strdup(parsed.value);
        rows[idx].expire_at = parsed.expire_at;
        if (rows[idx].key == NULL || rows[idx].value == NULL) {
            free(line);
            return -1;
        }
    }
    free(line);

    if (ftruncate(fileno(fp), 0) != 0) {
        return -1;
    }
    if (rewind(fp), fseek(fp, 0, SEEK_SET) != 0) {
        return -1;
    }
    for (size_t i = 0; i < len; ++i) {
        if (rows[i].expire_at > now && fprintf(fp, "%s\t%s\t%ld\n", rows[i].key, rows[i].value, rows[i].expire_at) < 0) {
            return -1;
        }
    }
    if (fflush(fp) != 0 || fsync(fileno(fp)) != 0) {
        return -1;
    }
    for (size_t i = 0; i < len; ++i) {
        free(rows[i].key);
        free(rows[i].value);
    }
    free(rows);
    return 0;
}

int main(int argc, char *argv[])
{
    stream_logger_set_tag("clip_store");

    const char *db = NULL;
    const char *ttl_arg = "3600";
    const char *get_key = NULL;
    int do_gc = 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--db") == 0 && i + 1 < argc) {
            db = argv[++i];
        } else if (strcmp(argv[i], "--ttl") == 0 && i + 1 < argc) {
            ttl_arg = argv[++i];
        } else if (strcmp(argv[i], "--get") == 0 && i + 1 < argc) {
            get_key = argv[++i];
        } else if (strcmp(argv[i], "--gc") == 0) {
            do_gc = 1;
        } else {
            usage(argv[0]);
            return 1;
        }
    }
    if (db == NULL) {
        usage(argv[0]);
        return 1;
    }

    char *end = NULL;
    long ttl = strtol(ttl_arg, &end, 10);
    if (end == ttl_arg || *end != '\0' || ttl < 0) {
        LOG_ERROR("invalid ttl=%s", ttl_arg);
        return 1;
    }

    int fd = open(db, O_RDWR | O_CREAT, 0600);
    if (fd < 0) {
        LOG_ERROR("open db failed: %s", strerror(errno));
        return 1;
    }
    if (flock(fd, LOCK_EX) != 0) {
        LOG_ERROR("lock db failed: %s", strerror(errno));
        close(fd);
        return 1;
    }
    FILE *fp = fdopen(fd, "r+");
    if (fp == NULL) {
        LOG_ERROR("fdopen db failed: %s", strerror(errno));
        close(fd);
        return 1;
    }

    long now = (long)time(NULL);
    int rc = 0;
    if (get_key != NULL) {
        row_t row = {0};
        if (load_latest(fp, get_key, &row, now)) {
            printf("%s\n", row.value);
        }
        free(row.key);
        free(row.value);
    } else if (do_gc) {
        rc = rewrite_gc(fp, now) == 0 ? 0 : 1;
    } else {
        char *line = NULL;
        size_t cap = 0;
        while (getline(&line, &cap, stdin) > 0) {
            char *session = json_find_string_value(line, "session_id");
            char *ts = json_find_scalar_value(line, "ts");
            char *path = json_find_string_value(line, "path");
            if (session == NULL || ts == NULL || path == NULL) {
                LOG_WARN("malformed clip JSON; skipping");
                free(session);
                free(ts);
                free(path);
                continue;
            }
            pipeline_buffer_t key = {0};
            long row_now = (long)time(NULL);
            if (pipeline_buffer_append_str(&key, session) != 0 ||
                pipeline_buffer_append_char(&key, ':') != 0 ||
                pipeline_buffer_append_str(&key, ts) != 0 ||
                append_row(fp, key.data, path, row_now + ttl) != 0) {
                LOG_ERROR("write db failed: %s", strerror(errno));
                rc = 1;
                pipeline_buffer_free(&key);
                free(session);
                free(ts);
                free(path);
                break;
            }
            pipeline_buffer_free(&key);
            free(session);
            free(ts);
            free(path);
        }
        free(line);
    }

    fclose(fp);
    return rc;
}

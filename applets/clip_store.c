

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
#include <getopt.h>
#include <stdbool.h>

typedef struct {
    char *key;
    char *value;
    long expire_at;
} row_t;

static void print_usage(FILE *stream, const char *prog_name) {
    fprintf(stream, "Usage: %s [OPTIONS]\n\n", prog_name);
    fprintf(stream, "Description:\n");
    fprintf(stream, "  Plain-text clip index persistence applet for UNIX pipelines.\n");
    fprintf(stream, "  Reads JSONL from stdin and appends to the specified database file.\n\n");
    fprintf(stream, "Options:\n");
    fprintf(stream, "  -d, --db <path>        Path to the database file (Required)\n");
    fprintf(stream, "  -t, --ttl <seconds>    Time-To-Live for stored clips (default: 3600)\n");
    fprintf(stream, "      --get <key>        Retrieve the latest value for a specific key\n");
    fprintf(stream, "      --gc               Run standalone Garbage Collection and exit\n");
    fprintf(stream, "  -h, --help             Show this help message and exit\n");
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

    if (!found || (out->expire_at != 0 && out->expire_at <= now)) {
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

static int rewrite_gc(FILE *fp, const char *db_path, long now)
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

    char tmp_path[PATH_MAX];
    if (snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", db_path) >= (int)sizeof(tmp_path)) {
        for (size_t i = 0; i < len; ++i) { free(rows[i].key); free(rows[i].value); }
        free(rows);
        return -1;
    }
    int tmp_fd = open(tmp_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (tmp_fd < 0) {
        for (size_t i = 0; i < len; ++i) { free(rows[i].key); free(rows[i].value); }
        free(rows);
        return -1;
    }
    FILE *tmp_fp = fdopen(tmp_fd, "w");
    if (tmp_fp == NULL) {
        close(tmp_fd);
        unlink(tmp_path);
        for (size_t i = 0; i < len; ++i) { free(rows[i].key); free(rows[i].value); }
        free(rows);
        return -1;
    }
    for (size_t i = 0; i < len; ++i) {
        if ((rows[i].expire_at == 0 || rows[i].expire_at > now) &&
            fprintf(tmp_fp, "%s\t%s\t%ld\n", rows[i].key, rows[i].value, rows[i].expire_at) < 0) {
            fclose(tmp_fp);
            unlink(tmp_path);
            for (size_t j = 0; j < len; ++j) { free(rows[j].key); free(rows[j].value); }
            free(rows);
            return -1;
        }
    }
    if (fflush(tmp_fp) != 0 || fsync(fileno(tmp_fp)) != 0) {
        fclose(tmp_fp);
        unlink(tmp_path);
        for (size_t i = 0; i < len; ++i) { free(rows[i].key); free(rows[i].value); }
        free(rows);
        return -1;
    }
    fclose(tmp_fp);
    for (size_t i = 0; i < len; ++i) { free(rows[i].key); free(rows[i].value); }
    free(rows);
    if (rename(tmp_path, db_path) != 0) {
        unlink(tmp_path);
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    stream_logger_set_tag("clip_store");

    const char *db = NULL;
    const char *ttl_arg = "3600";
    const char *get_key = NULL;
    int do_gc = 0;

    int opt;
    static struct option long_options[] = {
        {"db",   required_argument, 0, 'd'},
        {"ttl",  required_argument, 0, 't'},
        {"get",  required_argument, 0, 1000},
        {"gc",   no_argument,       0, 1001},
        {"help", no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "d:t:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'd':
                db = optarg;
                break;
            case 't':
                ttl_arg = optarg;
                break;
            case 1000: // --get
                get_key = optarg;
                break;
            case 1001: // --gc
                do_gc = 1;
                break;
            case 'h':
                print_usage(stdout, argv[0]);
                exit(EXIT_SUCCESS);
            case '?':
                print_usage(stderr, argv[0]);
                exit(EXIT_FAILURE);
            default:
                exit(EXIT_FAILURE);
        }
    }

    if (db == NULL && optind < argc) {
        db = argv[optind];
    }

    if (db == NULL) {
        fprintf(stderr, "Error: --db <path> is required.\n\n");
        print_usage(stderr, argv[0]);
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
        rc = rewrite_gc(fp, db, now) == 0 ? 0 : 1;
    } else {
        char *line = NULL;
        size_t cap = 0;
        while (getline(&line, &cap, stdin) > 0) {
            char *session = pipeline_json_find_string(line, "session_id");
            char *ts = pipeline_json_find_scalar(line, "ts");
            char *path = pipeline_json_find_string(line, "path");
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
                append_row(fp, key.data, path, ttl == 0 ? 0 : row_now + ttl) != 0) {
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
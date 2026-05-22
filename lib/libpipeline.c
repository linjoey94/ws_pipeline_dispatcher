/*
 * libpipeline.c
 * version: v1.1
 */

#include "libpipeline.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <time.h>
#include <unistd.h>

int pipeline_open_dir_watch(const char *dir_path, int *watch_descriptor) {
    if (dir_path == NULL || watch_descriptor == NULL) {
        errno = EINVAL;
        return -1;
    }

    /* Use a non-blocking inotify fd so the caller can integrate it into polling loops. */
    int fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (fd < 0) {
        return -1;
    }

    /* Report files that become ready in the directory, either after close or rename into place. */
    int wd = inotify_add_watch(fd, dir_path, IN_CLOSE_WRITE | IN_MOVED_TO);
    if (wd < 0) {
        /* Preserve the original watch failure even if close() touches errno. */
        int saved = errno;
        close(fd);
        errno = saved;
        return -1;
    }

    *watch_descriptor = wd;
    return fd;
}

int pipeline_open_file_watch(const char *file_path, int *watch_descriptor) {
    if (file_path == NULL || watch_descriptor == NULL) {
        errno = EINVAL;
        return -1;
    }

    /* The file watch uses the same non-blocking fd contract as directory watches. */
    int fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (fd < 0) {
        return -1;
    }

    /*
     * IN_MODIFY reports growing-file updates while the writer still holds the
     * fd open; IN_CLOSE_WRITE reports the eventual writer-close completion.
     */
    int wd = inotify_add_watch(fd, file_path, IN_MODIFY | IN_CLOSE_WRITE | IN_MOVE_SELF | IN_DELETE_SELF);
    if (wd < 0) {
        /* Preserve the original watch failure even if close() touches errno. */
        int saved = errno;
        close(fd);
        errno = saved;
        return -1;
    }

    *watch_descriptor = wd;
    return fd;
}

int64_t pipeline_get_monotonic_time_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    /* Convert seconds plus nanoseconds to a monotonic millisecond counter. */
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void pipeline_buffer_free(pipeline_buffer_t *buf) {
    if (buf == NULL) {
        return;
    }
    free(buf->data);
    buf->data = NULL;
    buf->len = 0;
    buf->cap = 0;
}

int pipeline_buffer_reserve(pipeline_buffer_t *buf, size_t extra) {
    if (buf == NULL || extra > SIZE_MAX - buf->len ||
        buf->len + extra > SIZE_MAX - 1) {
        return -1;
    }

    size_t need = buf->len + extra + 1;
    if (need <= buf->cap) {
        return 0;
    }

    size_t next = buf->cap == 0 ? 128 : buf->cap;
    while (next < need) {
        if (next > SIZE_MAX / 2) {
            next = need;
            break;
        }
        next *= 2;
    }

    char *data = realloc(buf->data, next);
    if (data == NULL) {
        return -1;
    }

    buf->data = data;
    buf->cap = next;
    return 0;
}

int pipeline_buffer_append_char(pipeline_buffer_t *buf, char c) {
    if (pipeline_buffer_reserve(buf, 1) != 0) {
        return -1;
    }
    buf->data[buf->len++] = c;
    buf->data[buf->len] = '\0';
    return 0;
}

int pipeline_buffer_append_mem(pipeline_buffer_t *buf, const void *src, size_t len) {
    if (buf == NULL || (src == NULL && len > 0)) {
        return -1;
    }
    if (pipeline_buffer_reserve(buf, len) != 0) {
        return -1;
    }
    if (len > 0) {
        memcpy(buf->data + buf->len, src, len);
        buf->len += len;
    }
    buf->data[buf->len] = '\0';
    return 0;
}

int pipeline_buffer_append_str(pipeline_buffer_t *buf, const char *s) {
    if (s == NULL) {
        return -1;
    }
    return pipeline_buffer_append_mem(buf, s, strlen(s));
}

int pipeline_path_is_sentinel(const char *filename) {
    if (filename == NULL) {
        return 0;
    }

    const char *base = strrchr(filename, '/');
    if (base != NULL) {
        filename = base + 1;
    }

    return strcmp(filename, PIPELINE_SENTINEL_NAME) == 0 ? 1 : 0;
}

int pipeline_path_join(char *out, size_t sz, const char *dir, const char *name) {
    if (out == NULL || dir == NULL || name == NULL) {
        return -1;
    }
    int n = snprintf(out, sz, "%s/%s", dir, name);
    return n >= 0 && (size_t)n < sz ? 0 : -1;
}

char *pipeline_strndup(const char *src, size_t len) {
    if (src == NULL || len > SIZE_MAX - 1) {
        return NULL;
    }
    char *out = malloc(len + 1);
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, src, len);
    out[len] = '\0';
    return out;
}

/* Locate the value token after "key": in a JSON line.
 * Returns pointer to first non-whitespace char after the colon, or NULL. */
static const char *json_key_to_value(const char *line, const char *key) {
    if (line == NULL || key == NULL) return NULL;
    char needle[128];
    int n = snprintf(needle, sizeof(needle), "\"%s\"", key);
    if (n < 0 || (size_t)n >= sizeof(needle)) return NULL;
    const char *p = strstr(line, needle);
    if (p == NULL) return NULL;
    p += (size_t)n;
    while (*p == ' ' || *p == '\t') ++p;
    if (*p++ != ':') return NULL;
    while (*p == ' ' || *p == '\t') ++p;
    return p;
}

char *pipeline_json_find_string(const char *line, const char *key) {
    const char *p = json_key_to_value(line, key);
    if (p == NULL || *p != '"') return NULL;
    ++p;

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

char *pipeline_json_find_scalar(const char *line, const char *key) {
    const char *p = json_key_to_value(line, key);
    if (p == NULL) return NULL;
    if (*p == '"') return pipeline_json_find_string(line, key);

    const char *start = p;
    while (*p != '\0' && *p != ',' && *p != '}' &&
           *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
        ++p;
    }
    if (p == start) return NULL;
    return pipeline_strndup(start, (size_t)(p - start));
}

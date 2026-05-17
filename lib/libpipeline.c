/*
 * libpipeline.c — v1 minimal implementation
 * See lib/libpipeline.h and .docs/libpipeline-v1.0.md.
 */

#include "libpipeline.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/inotify.h>
#include <time.h>
#include <unistd.h>

int pipeline_watch_dir(const char *dir_path, int *watch_descriptor)
{
    if (dir_path == NULL || watch_descriptor == NULL) {
        errno = EINVAL;
        return -1;
    }

    int fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (fd < 0) {
        return -1;
    }

    int wd = inotify_add_watch(fd, dir_path,
                               IN_CLOSE_WRITE | IN_MOVED_TO);
    if (wd < 0) {
        int saved = errno;
        close(fd);
        errno = saved;
        return -1;
    }

    *watch_descriptor = wd;
    return fd;
}

int64_t pipeline_now_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int pipeline_compress_json(const char *src, char *dst, size_t dst_size)
{
    if (src == NULL || dst == NULL || dst_size == 0) {
        return -1;
    }

    size_t out = 0;
    for (const char *p = src; *p != '\0'; ++p) {
        unsigned char c = (unsigned char)*p;
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            continue;
        }
        if (out + 1 >= dst_size) {
            return -1; /* leave space for NUL */
        }
        dst[out++] = (char)c;
    }
    dst[out] = '\0';
    return (int)out;
}

int pipeline_is_sentinel(const char *filename)
{
    if (filename == NULL) {
        return 0;
    }
    return strcmp(filename, PIPELINE_SENTINEL_NAME) == 0 ? 1 : 0;
}

/*
 * libpipeline.c
 * version: v1.1
 */

#include "libpipeline.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
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
    int wd = inotify_add_watch(fd, file_path,
                               IN_MODIFY | IN_CLOSE_WRITE | IN_MOVE_SELF | IN_DELETE_SELF);
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

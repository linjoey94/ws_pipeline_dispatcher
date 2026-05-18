/*
 * libpipeline.h
 * version: v1.1
 *
 * Shared low-level helpers used by all applets:
 *   - inotify directory/file watching
 *   - monotonic time
 *   - sentinel filename detection
 *
 * See .docs/libpipeline-v1.0.md for the full specification.
 */

#ifndef LIBPIPELINE_H
#define LIBPIPELINE_H

#include <stddef.h>
#include <stdint.h>

// Sentinel filename created by the upstream writer when a session is complete.
#define PIPELINE_SENTINEL_NAME ".pipeline_end"

/**
 * @brief Open an inotify fd that watches a directory for completed writes.
 *
 * Watches at least IN_CLOSE_WRITE | IN_MOVED_TO. Callers own the returned fd
 * and must close it when the watch is no longer needed.
 *
 * @param dir_path absolute or relative directory path; must already exist
 * @param watch_descriptor out: inotify_add_watch() watch descriptor
 *
 * @return inotify fd (>= 0), or -1 on failure with errno set.
 */
int pipeline_open_dir_watch(const char *dir_path, int *watch_descriptor);

/**
 * @brief Open an inotify fd that watches a file for append/modify events.
 *
 * Watches at least IN_MODIFY. IN_MODIFY lets callers observe append/growing-file
 * updates before the writer closes the fd; IN_CLOSE_WRITE reports writer-close
 * completion. Callers own the returned fd and must close it when the watch is
 * no longer needed.
 *
 * @param file_path absolute or relative file path; must already exist
 * @param watch_descriptor out: inotify_add_watch() watch descriptor
 *
 * @return inotify fd (>= 0), or -1 on failure with errno set.
 */
int pipeline_open_file_watch(const char *file_path, int *watch_descriptor);

/**
 * @brief Read the current monotonic time in milliseconds.
 *
 * Uses CLOCK_MONOTONIC so it is not affected by wall-clock changes.
 *
 * @return monotonic timestamp in milliseconds, or 0 if unavailable.
 */
int64_t pipeline_get_monotonic_time_ms(void);

/**
 * @brief Check whether a filename/path refers to the pipeline sentinel.
 *
 * Compares the basename against PIPELINE_SENTINEL_NAME and does not read file contents.
 * 
 * @param filename filename or path to check.
 * @return 1 for sentinel, 0 otherwise.
 */
int pipeline_path_is_sentinel(const char *filename);

#endif

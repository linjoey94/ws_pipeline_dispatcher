/*
 * libpipeline.h — v1 minimal interface
 *
 * Shared low-level helpers used by all applets:
 *   - inotify directory watching
 *   - monotonic time
 *   - JSON whitespace compression
 *   - sentinel filename detection
 *
 * See .docs/libpipeline-v1.0.md for the full specification.
 */

#ifndef LIBPIPELINE_H
#define LIBPIPELINE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Sentinel filename — created by Fastify on END_ opcode. */
#define PIPELINE_SENTINEL_NAME ".pipeline_end"

/**
 * Create an inotify fd watching dir_path for IN_CLOSE_WRITE | IN_MOVED_TO.
 *
 * @param dir_path           absolute directory path (must already exist)
 * @param watch_descriptor   out: inotify_add_watch() wd
 *
 * @return inotify fd (>= 0), or -1 on failure (errno set).
 */
int pipeline_watch_dir(const char *dir_path, int *watch_descriptor);

/**
 * Current monotonic time in milliseconds.
 */
int64_t pipeline_now_ms(void);

/**
 * Compress JSON by stripping ASCII whitespace (space/tab/CR/LF).
 *
 * @return compressed length (excluding NUL), or -1 if dst_size too small.
 */
int pipeline_compress_json(const char *src, char *dst, size_t dst_size);

/**
 * Return 1 if filename equals PIPELINE_SENTINEL_NAME, else 0.
 */
int pipeline_is_sentinel(const char *filename);

#ifdef __cplusplus
}
#endif

#endif /* LIBPIPELINE_H */

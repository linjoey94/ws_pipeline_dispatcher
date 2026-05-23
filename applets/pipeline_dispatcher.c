/*
 * pipeline_dispatcher.c — v1 skeleton (C entry point)
 *
 * Builds the pipe1/pipe2 topology and fork+exec's the three applets:
 *
 *     stream_merge  | log_parse  | clip_store
 *           stdout──pipe1─▶stdin           │
 *                              stdout──pipe2─▶stdin
 *
 * See .docs/applets/pipeline-dispatcher.md.
 *
 * CLI:
 *   pipeline_dispatcher <session_id> <src_dir> <db_path> <ttl_seconds>
 *
 * Exit codes:
 *    0  success (all three children exited 0)
 *   -1  pipe/fork setup failure
 *   -2  one or more children exited non-zero or were killed
 */

#include "libpipeline.h"
#include "stream_logger.h"

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define READ_END  0
#define WRITE_END 1

/* Resolve sibling binaries living next to argv[0]. v1 simplification: assume
 * cwd contains the build outputs. A future pipeline_safe_spawn() helper in
 * libpipeline can take over locating these. */
static int sibling_path(char *out, size_t out_size, const char *argv0, const char *name)
{
    const char *slash = strrchr(argv0, '/');
    if (slash == NULL) {
        int n = snprintf(out, out_size, "./%s", name);
        return n >= 0 && (size_t)n < out_size ? 0 : -1;
    }

    size_t dir_len = (size_t)(slash - argv0);
    if (dir_len + 1 + strlen(name) + 1 > out_size) {
        return -1;
    }
    memcpy(out, argv0, dir_len);
    out[dir_len] = '/';
    strcpy(out + dir_len + 1, name);
    return 0;
}

static pid_t spawn_child(const char *bin, char *const argv[],
                         int stdin_fd, int stdout_fd,
                         int *close_in_child, size_t close_n)
{
    pid_t pid = fork();
    if (pid < 0) {
        LOG_ERROR("fork(%s) failed: %s", bin, strerror(errno));
        return -1;
    }
    if (pid == 0) {
        /* child */
        if (stdin_fd != STDIN_FILENO) {
            if (dup2(stdin_fd, STDIN_FILENO) < 0) _exit(126);
        }
        if (stdout_fd != STDOUT_FILENO) {
            if (dup2(stdout_fd, STDOUT_FILENO) < 0) _exit(126);
        }
        for (size_t i = 0; i < close_n; ++i) {
            if (close_in_child[i] >= 0) close(close_in_child[i]);
        }
        execv(bin, argv);
        /* execv only returns on failure */
        fprintf(stderr, "exec %s failed: %s\n", bin, strerror(errno));
        _exit(127);
    }
    return pid;
}

static int wait_all(pid_t pids[3])
{
    int worst = 0;
    for (int i = 0; i < 3; ++i) {
        if (pids[i] < 0) continue;
        int status = 0;
        if (waitpid(pids[i], &status, 0) < 0) {
            LOG_ERROR("waitpid(%d) failed: %s", (int)pids[i], strerror(errno));
            worst = -2;
            continue;
        }
        if (WIFEXITED(status)) {
            int code = WEXITSTATUS(status);
            LOG_INFO("child %d exited with %d", (int)pids[i], code);
            if (code != 0) worst = -2;
        } else if (WIFSIGNALED(status)) {
            LOG_WARN("child %d killed by signal %d",
                     (int)pids[i], WTERMSIG(status));
            worst = -2;
        }
    }
    return worst;
}

static void kill_and_reap(pid_t pids[3])
{
    for (int i = 0; i < 3; ++i) {
        if (pids[i] > 0) kill(pids[i], SIGTERM);
    }
    for (int i = 0; i < 3; ++i) {
        if (pids[i] > 0) {
            int status;
            waitpid(pids[i], &status, 0);
        }
    }
}

int main(int argc, char *argv[])
{
    stream_logger_set_tag("dispatcher");

    if (argc != 5) {
        fprintf(stderr,
                "usage: %s <session_id> <src_dir> <db_path> <ttl_seconds>\n",
                argv[0]);
        return 2;
    }
    const char *session_id = argv[1];
    const char *src_dir    = argv[2];
    const char *db_path    = argv[3];
    const char *ttl_str    = argv[4];

    LOG_INFO("starting pipeline session=%s src=%s db=%s ttl=%s",
             session_id, src_dir, db_path, ttl_str);

    char bin_stream_merge[PATH_MAX];
    char bin_log_parse[PATH_MAX];
    char bin_clip_store[PATH_MAX];
    if (sibling_path(bin_stream_merge, sizeof(bin_stream_merge), argv[0], "stream_merge") != 0 ||
        sibling_path(bin_log_parse, sizeof(bin_log_parse), argv[0], "log_parse") != 0 ||
        sibling_path(bin_clip_store, sizeof(bin_clip_store), argv[0], "clip_store") != 0) {
        LOG_ERROR("failed to resolve applet paths");
        return -1;
    }

    int pipe1[2] = {-1, -1};
    int pipe2[2] = {-1, -1};
    if (pipe(pipe1) < 0) {
        LOG_ERROR("pipe1 failed: %s", strerror(errno));
        return -1;
    }
    if (pipe(pipe2) < 0) {
        LOG_ERROR("pipe2 failed: %s", strerror(errno));
        close(pipe1[0]); close(pipe1[1]);
        return -1;
    }

    /* Build argv for each applet. */
    char *merge_argv[] = {
        bin_stream_merge,
        (char *)session_id,
        (char *)src_dir,
        NULL
    };
    char *parse_argv[] = {
        bin_log_parse,
        "--filter", "type=clip",
        NULL
    };
    char *store_argv[] = {
        bin_clip_store,
        "--db",  (char *)db_path,
        "--ttl", (char *)ttl_str,
        NULL
    };

    int all_pipes[] = {pipe1[0], pipe1[1], pipe2[0], pipe2[1]};
    pid_t pids[3] = {-1, -1, -1};

    /* stream_merge: stdout → pipe1[WRITE], stdin = /dev/null (inotify driven). */
    pids[0] = spawn_child(
        bin_stream_merge, merge_argv,
        STDIN_FILENO, pipe1[WRITE_END],
        all_pipes, 4);
    if (pids[0] < 0) goto fail;

    /* log_parse: stdin ← pipe1[READ], stdout → pipe2[WRITE]. */
    pids[1] = spawn_child(
        bin_log_parse, parse_argv,
        pipe1[READ_END], pipe2[WRITE_END],
        all_pipes, 4);
    if (pids[1] < 0) goto fail;

    /* clip_store: stdin ← pipe2[READ], stdout = inherited (debug). */
    pids[2] = spawn_child(
        bin_clip_store, store_argv,
        pipe2[READ_END], STDOUT_FILENO,
        all_pipes, 4);
    if (pids[2] < 0) goto fail;

    /* Parent closes all pipe fds — only the children should hold them. */
    close(pipe1[0]); close(pipe1[1]);
    close(pipe2[0]); close(pipe2[1]);

    int rc = wait_all(pids);

    if (rc == 0) {
        LOG_INFO("pipeline complete session=%s", session_id);
    } else {
        LOG_ERROR("pipeline failed session=%s rc=%d", session_id, rc);
    }
    return rc;

fail:
    close(pipe1[0]); close(pipe1[1]);
    close(pipe2[0]); close(pipe2[1]);
    kill_and_reap(pids);
    return -1;
}

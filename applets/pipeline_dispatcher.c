/*
 * pipeline_dispatcher.c — v1 skeleton (C entry point)
 *
 * Builds the pipe1/pipe2 topology and fork+exec's the three applets:
 *
 *     stream_merge  | log_parse  | clip_store
 *           stdout──pipe1─▶stdin           │
 *                              stdout──pipe2─▶stdin
 *
 * See .docs/pipeline_dispatcher-v1.0.md.
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
static const char *BIN_STREAM_MERGE = "./stream_merge";
static const char *BIN_LOG_PARSE    = "./log_parse";
static const char *BIN_CLIP_STORE   = "./clip_store";

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
        (char *)BIN_STREAM_MERGE,
        "--src",     (char *)src_dir,
        "--session", (char *)session_id,
        NULL
    };
    char *parse_argv[] = {
        (char *)BIN_LOG_PARSE,
        "--filter", "type=clip",
        NULL
    };
    char *store_argv[] = {
        (char *)BIN_CLIP_STORE,
        "--db",  (char *)db_path,
        "--ttl", (char *)ttl_str,
        NULL
    };

    int all_pipes[] = {pipe1[0], pipe1[1], pipe2[0], pipe2[1]};

    /* stream_merge: stdout → pipe1[WRITE], stdin = /dev/null (inotify driven). */
    pid_t merge_pid = spawn_child(
        BIN_STREAM_MERGE, merge_argv,
        STDIN_FILENO, pipe1[WRITE_END],
        all_pipes, 4);
    if (merge_pid < 0) goto fail;

    /* log_parse: stdin ← pipe1[READ], stdout → pipe2[WRITE]. */
    pid_t parse_pid = spawn_child(
        BIN_LOG_PARSE, parse_argv,
        pipe1[READ_END], pipe2[WRITE_END],
        all_pipes, 4);
    if (parse_pid < 0) goto fail;

    /* clip_store: stdin ← pipe2[READ], stdout = inherited (debug). */
    pid_t store_pid = spawn_child(
        BIN_CLIP_STORE, store_argv,
        pipe2[READ_END], STDOUT_FILENO,
        all_pipes, 4);
    if (store_pid < 0) goto fail;

    /* Parent closes all pipe fds — only the children should hold them. */
    close(pipe1[0]); close(pipe1[1]);
    close(pipe2[0]); close(pipe2[1]);

    pid_t pids[3] = { merge_pid, parse_pid, store_pid };
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
    return -1;
}

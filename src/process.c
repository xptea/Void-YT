#include "subprocess.h"

#include <stdio.h>
#include <string.h>

#define VOIDYT_PROCESS_LINE_CAP 8192

typedef struct voidyt_line_reader {
    char line[VOIDYT_PROCESS_LINE_CAP];
    size_t length;
    voidyt_process_line_callback callback;
    void *context;
} voidyt_line_reader;

static void dispatch_line(voidyt_line_reader *reader) {
    if (reader->length > 0 && reader->line[reader->length - 1] == '\r') {
        --reader->length;
    }
    reader->line[reader->length] = '\0';
    if (reader->callback != NULL) {
        reader->callback(reader->line, reader->context);
    }
    reader->length = 0;
}

static void consume_output(voidyt_line_reader *reader, const char *data, size_t length) {
    size_t i;
    for (i = 0; i < length; ++i) {
        if (data[i] == '\n') {
            dispatch_line(reader);
        } else if (reader->length + 1 < sizeof(reader->line)) {
            reader->line[reader->length++] = data[i];
        }
    }
}

static void finish_output(voidyt_line_reader *reader) {
    if (reader->length > 0) {
        dispatch_line(reader);
    }
}

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <process.h>

int voidyt_run_process(const char *program, const char *const arguments[]) {
    intptr_t result = _spawnv(_P_WAIT, program, arguments);
    if (result == -1) {
        perror("void-yt: could not start dependency");
        return 127;
    }
    return (int)result;
}

int voidyt_run_process_capture(const char *program,
                               const char *const arguments[],
                               voidyt_process_line_callback callback,
                               void *context) {
    int pipe_fds[2];
    int saved_stdout;
    int saved_stderr;
    intptr_t child;
    int status = 127;
    char buffer[4096];
    int count;
    voidyt_line_reader reader = {{0}, 0, callback, context};

    if (_pipe(pipe_fds, 4096, _O_BINARY) != 0) {
        perror("void-yt: could not create output pipe");
        return 127;
    }
    saved_stdout = _dup(_fileno(stdout));
    saved_stderr = _dup(_fileno(stderr));
    if (saved_stdout < 0 || saved_stderr < 0) {
        if (saved_stdout >= 0) _close(saved_stdout);
        if (saved_stderr >= 0) _close(saved_stderr);
        _close(pipe_fds[0]);
        _close(pipe_fds[1]);
        return 127;
    }

    fflush(NULL);
    if (_dup2(pipe_fds[1], _fileno(stdout)) != 0 ||
        _dup2(pipe_fds[1], _fileno(stderr)) != 0) {
        _dup2(saved_stdout, _fileno(stdout));
        _dup2(saved_stderr, _fileno(stderr));
        _close(saved_stdout);
        _close(saved_stderr);
        _close(pipe_fds[0]);
        _close(pipe_fds[1]);
        return 127;
    }

    child = _spawnv(_P_NOWAIT, program, arguments);
    _dup2(saved_stdout, _fileno(stdout));
    _dup2(saved_stderr, _fileno(stderr));
    _close(saved_stdout);
    _close(saved_stderr);
    _close(pipe_fds[1]);
    if (child == -1) {
        _close(pipe_fds[0]);
        perror("void-yt: could not start dependency");
        return 127;
    }

    while ((count = _read(pipe_fds[0], buffer, (unsigned int)sizeof(buffer))) > 0) {
        consume_output(&reader, buffer, (size_t)count);
    }
    _close(pipe_fds[0]);
    finish_output(&reader);
    if (_cwait(&status, child, _WAIT_CHILD) == -1) {
        perror("void-yt: could not wait for dependency");
        return 127;
    }
    return status;
}

#else
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int voidyt_run_process(const char *program, const char *const arguments[]) {
    pid_t child = fork();
    int status;
    if (child < 0) {
        perror("void-yt: fork failed");
        return 127;
    }
    if (child == 0) {
        execv(program, (char *const *)arguments);
        perror("void-yt: could not start dependency");
        _exit(errno == ENOENT ? 127 : 126);
    }
    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) {
            perror("void-yt: wait failed");
            return 127;
        }
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return 1;
}

int voidyt_run_process_capture(const char *program,
                               const char *const arguments[],
                               voidyt_process_line_callback callback,
                               void *context) {
    int pipe_fds[2];
    pid_t child;
    int status;
    ssize_t count;
    char buffer[4096];
    voidyt_line_reader reader = {{0}, 0, callback, context};

    if (pipe(pipe_fds) != 0) {
        perror("void-yt: pipe failed");
        return 127;
    }
    child = fork();
    if (child < 0) {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        perror("void-yt: fork failed");
        return 127;
    }
    if (child == 0) {
        close(pipe_fds[0]);
        if (dup2(pipe_fds[1], STDOUT_FILENO) < 0 ||
            dup2(pipe_fds[1], STDERR_FILENO) < 0) {
            _exit(126);
        }
        close(pipe_fds[1]);
        execv(program, (char *const *)arguments);
        _exit(errno == ENOENT ? 127 : 126);
    }

    close(pipe_fds[1]);
    while ((count = read(pipe_fds[0], buffer, sizeof(buffer))) != 0) {
        if (count > 0) {
            consume_output(&reader, buffer, (size_t)count);
        } else if (errno != EINTR) {
            break;
        }
    }
    close(pipe_fds[0]);
    finish_output(&reader);

    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) {
            perror("void-yt: wait failed");
            return 127;
        }
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return 1;
}
#endif

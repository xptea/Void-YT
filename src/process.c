#include "subprocess.h"

#include <stdio.h>

#ifdef _WIN32
#include <process.h>

int voidyt_run_process(const char *program, const char *const arguments[]) {
    intptr_t result = _spawnv(_P_WAIT, program, arguments);
    if (result == -1) {
        perror("void-yt: could not start dependency");
        return 127;
    }
    return (int)result;
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
#endif

#define _POSIX_C_SOURCE 200809L

#include "yaza_runtime_v1.h"

#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
    const pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        struct rlimit no_core = {0, 0};
        (void)setrlimit(RLIMIT_CORE, &no_core);
        const int sink = open("/dev/null", O_WRONLY);
        if (sink >= 0) {
            (void)dup2(sink, STDERR_FILENO);
            (void)close(sink);
        }
        assert(setenv("YAZA_RT_V1_PROCESS_LIMIT_BYTES", "not-a-number", 1) == 0);
        yaza_rt_v1_init();
        _exit(0);
    }

    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status));
    assert(WTERMSIG(status) == SIGABRT);
    puts("runtime-v1: malformed deployment memory profile fails closed before application startup");
    return 0;
}

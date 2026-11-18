#include "yaza_runtime_v1.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int text_equals(const YazaTextV1 *text, const char *expected) {
    const size_t expected_length = strlen(expected);
    return text != NULL && text->length == expected_length &&
        (expected_length == 0 || memcmp(text->data, expected, expected_length) == 0);
}

int main(void) {
    char *arguments[] = {"args_v1", "alpha", "", "omega", NULL};
    yaza_rt_v1_set_args(4, arguments);

    assert(yaza_rt_v1_arg_count() == 3);
    assert(text_equals(yaza_rt_v1_arg(0), "args_v1"));
    assert(text_equals(yaza_rt_v1_arg(1), "alpha"));
    assert(text_equals(yaza_rt_v1_arg(2), ""));
    assert(text_equals(yaza_rt_v1_arg(3), "omega"));

    int diagnostic_pipe[2];
    assert(pipe(diagnostic_pipe) == 0);
    const pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        close(diagnostic_pipe[0]);
        assert(dup2(diagnostic_pipe[1], STDERR_FILENO) == STDERR_FILENO);
        close(diagnostic_pipe[1]);
        (void)yaza_rt_v1_arg(4);
        _exit(99);
    }

    close(diagnostic_pipe[1]);
    char diagnostic[128];
    const ssize_t diagnostic_length = read(diagnostic_pipe[0], diagnostic, sizeof(diagnostic) - 1);
    assert(diagnostic_length >= 0);
    close(diagnostic_pipe[0]);
    diagnostic[diagnostic_length] = '\0';

    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 2);
    assert(strcmp(diagnostic, "yaza: argument 4 was not provided\n") == 0);

    puts("runtime-v1: process arguments and bounds diagnostics passed");
    return 0;
}

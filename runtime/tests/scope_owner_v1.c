#define _POSIX_C_SOURCE 200809L

#include "yaza_runtime_v1.h"

#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct ListReadProbe {
    void *list;
    uint64_t count;
} ListReadProbe;

static void *read_request_list(void *opaque) {
    ListReadProbe *probe = (ListReadProbe *)opaque;
    probe->count = yaza_list_v1_count_typed(probe->list, "List[Number]", sizeof(uint64_t));
    return NULL;
}

int main(void) {
    yaza_rt_v1_init();
    YazaArenaV1 *request = yaza_arena_v1_create(128ULL * 1024ULL);
    assert(request != NULL);
    YazaArenaContextV1 context = {0};
    assert(yaza_arena_v1_scope_begin(request, &context) == 1);
    void *list = yaza_list_v1_create_typed("List[Number]", sizeof(uint64_t));
    uint64_t value = 7;
    assert(yaza_list_v1_push_typed(list, &value, "List[Number]", sizeof(value)) == 0);

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
        ListReadProbe probe = {list, 0};
        pthread_t thread;
        assert(pthread_create(&thread, NULL, read_request_list, &probe) == 0);
        assert(pthread_join(thread, NULL) == 0);
        _exit(0);
    }

    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status));
    assert(WTERMSIG(status) == SIGABRT);
    assert(yaza_list_v1_count_typed(list, "List[Number]", sizeof(value)) == 1);
    assert(yaza_arena_v1_scope_end(&context) == 1);
    assert(yaza_arena_v1_destroy(request) == 1);
    puts("arena-v1: request-owned typed Lists reject access from another thread");
    return 0;
}

#include "yaza_runtime_v1.h"

#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define WORKER_COUNT 8
#define ALLOCATIONS_PER_WORKER 200

typedef struct TaskWorkerContext {
    uint32_t thread_id;
    YazaArenaV1 *observed_task_arena;
    int success;
} TaskWorkerContext;

static void task_worker(void *opaque) {
    TaskWorkerContext *ctx = (TaskWorkerContext *)opaque;
    YazaArenaV1 *task_arena = yaza_rt_v1_task_arena_handle();
    YazaArenaV1 *current_arena = yaza_arena_v1_current();
    YazaArenaV1 *program_arena = yaza_rt_v1_program_arena_handle();

    // Invariant 1: Task arena must be active and isolated from program arena
    assert(task_arena != NULL);
    assert(task_arena == current_arena);
    assert(task_arena != program_arena);

    ctx->observed_task_arena = task_arena;

    // Invariant 2: Perform independent allocations and verify data isolation
    uint8_t *buffers[ALLOCATIONS_PER_WORKER];
    uint64_t sizes[ALLOCATIONS_PER_WORKER];

    for (uint32_t i = 0; i < ALLOCATIONS_PER_WORKER; ++i) {
        uint64_t sz = 16 + (i * 7) % 512;
        sizes[i] = sz;
        buffers[i] = (uint8_t *)yaza_rt_v1_alloc(sz);
        assert(buffers[i] != NULL);
        uint8_t pattern = (uint8_t)((ctx->thread_id + 1) * 31 + i);
        memset(buffers[i], pattern, sz);
    }

    // Text allocations in task arena
    char name_buf[64];
    snprintf(name_buf, sizeof(name_buf), "task-worker-%u", ctx->thread_id);
    YazaTextV1 *text = yaza_rt_v1_text_from_cstr(name_buf);
    assert(text != NULL && text->length == strlen(name_buf));
    assert(memcmp(text->data, name_buf, text->length) == 0);

    // Yield/sleep briefly to let all workers interleave allocations
    usleep(2000);

    // Invariant 3: Verify all buffers retain their exact per-thread values (no cross-thread overwriting)
    for (uint32_t i = 0; i < ALLOCATIONS_PER_WORKER; ++i) {
        uint8_t expected_pattern = (uint8_t)((ctx->thread_id + 1) * 31 + i);
        for (uint64_t b = 0; b < sizes[i]; ++b) {
            assert(buffers[i][b] == expected_pattern);
        }
    }

    assert(memcmp(text->data, name_buf, text->length) == 0);

    // Arena metrics must report active usage
    assert(yaza_arena_v1_used(task_arena) > 0);

    ctx->success = 1;
}

int main(void) {
    yaza_rt_v1_init();

    // Main thread is on the program arena
    assert(yaza_rt_v1_task_arena_handle() == NULL);
    assert(yaza_arena_v1_current() == yaza_rt_v1_program_arena_handle());

    uint64_t baseline_scope_count = yaza_rt_v1_arena_scope_count();
    uint64_t program_used_before = yaza_rt_v1_arena_used();

    TaskWorkerContext contexts[WORKER_COUNT];
    YazaThreadV1 *threads[WORKER_COUNT];

    // Spawn workers concurrently
    for (uint32_t i = 0; i < WORKER_COUNT; ++i) {
        contexts[i].thread_id = i;
        contexts[i].observed_task_arena = NULL;
        contexts[i].success = 0;
        int status = yaza_rt_v1_thread_spawn(task_worker, &contexts[i], &threads[i]);
        assert(status == 0 && threads[i] != NULL);
    }

    // Join all workers
    for (uint32_t i = 0; i < WORKER_COUNT; ++i) {
        int status = yaza_rt_v1_thread_join(threads[i]);
        assert(status == 0);
        assert(contexts[i].success == 1);
    }

    // Invariant 4: Every worker had a unique, distinct task arena
    for (uint32_t i = 0; i < WORKER_COUNT; ++i) {
        assert(contexts[i].observed_task_arena != NULL);
        for (uint32_t j = i + 1; j < WORKER_COUNT; ++j) {
            assert(contexts[i].observed_task_arena != contexts[j].observed_task_arena);
        }
    }

    // Invariant 5: All task arenas have been cleanly destroyed and reclaimed
    uint64_t final_scope_count = yaza_rt_v1_arena_scope_count();
    assert(final_scope_count == baseline_scope_count);

    // Program arena used bytes should not have been polluted by workers' local allocations
    uint64_t program_used_after = yaza_rt_v1_arena_used();
    assert(program_used_after == program_used_before);

    puts("task-arena-isolation-ok");
    return 0;
}

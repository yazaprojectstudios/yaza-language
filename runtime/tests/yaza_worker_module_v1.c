#include "yaza_runtime_v1.h"

#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define WORKER_COUNT 8
#ifndef REQUESTS_PER_WORKER
#define REQUESTS_PER_WORKER 10000ULL
#endif

extern int64_t yaza_export_request_length(int64_t request_id, int64_t suffix);

typedef struct Worker {
    YazaArenaV1 *parent;
    YazaArenaCycleV1 *cycle;
    uint64_t completed;
} Worker;

static void *run_worker(void *opaque) {
    Worker *worker = (Worker *)opaque;
    for (uint64_t request = 0; request < REQUESTS_PER_WORKER; ++request) {
        const int64_t request_id = (int64_t)(request + 1000000ULL);
        assert(yaza_arena_v1_cycle_begin(worker->cycle) == 1);
        assert(yaza_arena_v1_current() != worker->parent);
        const int64_t actual = yaza_export_request_length(request_id, 7);
        char expected_text[64];
        const int expected = snprintf(expected_text, sizeof(expected_text), "request-%lld", (long long)(request_id + 7));
        assert(expected > 0 && actual == expected);

        YazaArenaCycleMetricsV1 metrics = {.struct_size = sizeof(metrics)};
        assert(yaza_arena_v1_cycle_end(worker->cycle, &metrics) == 1);
        assert(metrics.generation == 2ULL * (request + 1));
        assert(metrics.successful_allocations >= 1);
        assert(metrics.allocation_failures == 0);
        assert(metrics.peak_used_bytes > 0);
        assert(metrics.peak_storage_used_bytes >= metrics.peak_used_bytes);
        assert(metrics.retained_reserved_bytes <= 64ULL * 1024ULL);
        assert(yaza_rt_v1_current_arena_used() == 0);
        worker->completed += 1;
    }
    return NULL;
}

int main(void) {
    yaza_rt_v1_init();
    Worker workers[WORKER_COUNT] = {0};
    pthread_t threads[WORKER_COUNT];
    for (int index = 0; index < WORKER_COUNT; ++index) {
        workers[index].parent = yaza_arena_v1_create(256ULL * 1024ULL);
        assert(workers[index].parent != NULL);
        workers[index].cycle = yaza_arena_v1_cycle_create(workers[index].parent, 128ULL * 1024ULL);
        assert(workers[index].cycle != NULL);
        assert(pthread_create(&threads[index], NULL, run_worker, &workers[index]) == 0);
    }

    uint64_t completed = 0;
    for (int index = 0; index < WORKER_COUNT; ++index) {
        assert(pthread_join(threads[index], NULL) == 0);
        assert(workers[index].completed == REQUESTS_PER_WORKER);
        YazaArenaCycleMetricsV1 metrics = {.struct_size = sizeof(metrics)};
        assert(yaza_arena_v1_cycle_last_metrics(workers[index].cycle, &metrics) == 1);
        assert(metrics.generation == 2ULL * REQUESTS_PER_WORKER);
        completed += workers[index].completed;
        assert(yaza_arena_v1_cycle_destroy(workers[index].cycle) == 1);
        assert(yaza_arena_v1_destroy(workers[index].parent) == 1);
    }
    assert(yaza_rt_v1_arena_used() == 0);
    assert(yaza_rt_v1_arena_storage_used() == 0);
    assert(yaza_rt_v1_arena_reserved() == 0);
    assert(yaza_rt_v1_arena_scope_count() == 0);
    printf("arena-v1: %llu YAZA module calls completed inside bounded request cycles across %d workers\n",
        (unsigned long long)completed, WORKER_COUNT);
    return 0;
}

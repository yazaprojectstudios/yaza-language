#include "yaza_runtime_v1.h"

#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define WORKER_COUNT 8
#ifndef REQUESTS_PER_WORKER
#define REQUESTS_PER_WORKER 25000ULL
#endif

typedef struct RequestWorker {
    YazaArenaV1 *arena;
    YazaArenaCycleV1 *cycle;
    uint64_t requests;
} RequestWorker;

typedef struct RequestMetricObserver {
    RequestWorker *workers;
    _Atomic int done;
    _Atomic uint64_t reads;
} RequestMetricObserver;

static void *run_requests(void *opaque) {
    RequestWorker *worker = (RequestWorker *)opaque;
    for (uint64_t request = 0; request < REQUESTS_PER_WORKER; ++request) {
        assert(yaza_arena_v1_cycle_begin(worker->cycle) == 1);
        YazaArenaV1 *request_arena = yaza_arena_v1_current();
        assert(request_arena != worker->arena);
        void *numbers = yaza_list_v1_create_typed("List[Number]", sizeof(uint64_t));
        uint64_t request_number = request;
        assert(yaza_list_v1_push_typed(numbers, &request_number, "List[Number]", sizeof(request_number)) == 0);
        uint64_t copied_number = UINT64_MAX;
        assert(yaza_list_v1_get_typed(numbers, 0, "List[Number]", sizeof(copied_number), &copied_number) == 0);
        assert(copied_number == request);

        YazaTextV1 worker_text = {(const unsigned char *)"worker", 6};
        YazaTextV1 request_suffix = {(const unsigned char *)"-request", 8};
        YazaTextV1 *joined = yaza_rt_v1_text_concat(&worker_text, &request_suffix);
        const YazaTextV1 *joined_text = joined;
        void *texts = yaza_list_v1_create_typed("TextList", sizeof(joined_text));
        assert(yaza_list_v1_push_typed(texts, &joined_text, "TextList", sizeof(joined_text)) == 0);
        const YazaTextV1 *stored = NULL;
        assert(yaza_list_v1_get_typed(texts, 0, "TextList", sizeof(stored), &stored) == 0);
        assert(stored->length == 14 && memcmp(stored->data, "worker-request", 14) == 0);
        assert(yaza_arena_v1_storage_used(request_arena) >= yaza_arena_v1_used(request_arena));
        assert(yaza_arena_v1_peak_storage_used(request_arena) >= yaza_arena_v1_storage_used(request_arena));
        YazaArenaCycleMetricsV1 metrics = {.struct_size = sizeof(metrics)};
        assert(yaza_arena_v1_cycle_end(worker->cycle, &metrics) == 1);
        assert(metrics.generation == 2ULL * (request + 1));
        assert(metrics.successful_allocations >= 3);
        assert(metrics.allocation_failures == 0);
        assert(metrics.peak_storage_used_bytes >= metrics.peak_used_bytes);
        assert(metrics.retained_reserved_bytes == 64ULL * 1024ULL);
        assert(yaza_arena_v1_current() == yaza_rt_v1_program_arena_handle());
        worker->requests += 1;
    }
    return NULL;
}

static void *observe_request_metrics(void *opaque) {
    RequestMetricObserver *observer = (RequestMetricObserver *)opaque;
    while (!atomic_load_explicit(&observer->done, memory_order_relaxed)) {
        for (int index = 0; index < WORKER_COUNT; ++index) {
            YazaArenaCycleMetricsV1 metrics = {.struct_size = sizeof(metrics)};
            assert(yaza_arena_v1_cycle_last_metrics(observer->workers[index].cycle, &metrics) == 1);
            assert(metrics.abi_version == YAZA_ARENA_CYCLE_METRICS_V1_ABI_VERSION);
            if (metrics.generation != 0) {
                assert(metrics.allocation_failures == 0);
                assert(metrics.successful_allocations > 0);
                assert(metrics.peak_storage_used_bytes >= metrics.peak_used_bytes);
                assert(metrics.peak_reserved_bytes >= metrics.retained_reserved_bytes);
            }
        }
        atomic_fetch_add_explicit(&observer->reads, 1, memory_order_relaxed);
    }
    return NULL;
}

int main(void) {
    yaza_rt_v1_init();
    pthread_t threads[WORKER_COUNT];
    RequestWorker workers[WORKER_COUNT];
    for (int index = 0; index < WORKER_COUNT; ++index) {
        workers[index].arena = yaza_arena_v1_create(256ULL * 1024ULL);
        workers[index].cycle = yaza_arena_v1_cycle_create(workers[index].arena, 128ULL * 1024ULL);
        workers[index].requests = 0;
        assert(workers[index].arena != NULL && workers[index].cycle != NULL);
        assert(pthread_create(&threads[index], NULL, run_requests, &workers[index]) == 0);
    }
    RequestMetricObserver observer = {.workers = workers, .done = 0, .reads = 0};
    pthread_t observer_thread;
    assert(pthread_create(&observer_thread, NULL, observe_request_metrics, &observer) == 0);
    uint64_t completed = 0;
    for (int index = 0; index < WORKER_COUNT; ++index) {
        assert(pthread_join(threads[index], NULL) == 0);
        assert(workers[index].requests == REQUESTS_PER_WORKER);
        YazaArenaCycleMetricsV1 metrics = {.struct_size = sizeof(metrics)};
        assert(yaza_arena_v1_cycle_last_metrics(workers[index].cycle, &metrics) == 1);
        assert(metrics.generation == 2ULL * REQUESTS_PER_WORKER);
        assert(metrics.successful_allocations >= 3);
        assert(metrics.allocation_failures == 0);
        assert(metrics.peak_used_bytes > 0);
        assert(metrics.peak_storage_used_bytes >= metrics.peak_used_bytes);
        assert(metrics.peak_reserved_bytes >= 64ULL * 1024ULL);
        assert(metrics.retained_reserved_bytes == 64ULL * 1024ULL);
        completed += workers[index].requests;
    }
    atomic_store_explicit(&observer.done, 1, memory_order_relaxed);
    assert(pthread_join(observer_thread, NULL) == 0);
    assert(atomic_load_explicit(&observer.reads, memory_order_relaxed) > 0);
    assert(yaza_rt_v1_arena_used() == 0);
    assert(yaza_rt_v1_arena_storage_used() == 0);
    assert(yaza_rt_v1_arena_reserved() == (uint64_t)WORKER_COUNT * 64ULL * 1024ULL);
    assert(yaza_rt_v1_arena_peak_storage_used() > 0);
    assert(yaza_rt_v1_arena_peak_reserved() >= (uint64_t)WORKER_COUNT * 64ULL * 1024ULL);
    for (int index = 0; index < WORKER_COUNT; ++index) {
        assert(yaza_arena_v1_cycle_destroy(workers[index].cycle) == 1);
        assert(yaza_arena_v1_destroy(workers[index].arena) == 1);
    }
    assert(completed == WORKER_COUNT * REQUESTS_PER_WORKER);
    assert(yaza_rt_v1_arena_used() == 0);
    assert(yaza_rt_v1_arena_storage_used() == 0);
    assert(yaza_rt_v1_arena_reserved() == 0);
    assert(yaza_rt_v1_arena_scope_count() == 0);
    printf("arena-v1: %llu concurrent reusable request scopes across %d workers reclaimed all managed bytes\n", (unsigned long long)completed, WORKER_COUNT);
    return 0;
}

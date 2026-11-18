#include "yaza_runtime_v1.h"

#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define REQUEST_COUNT 1000000ULL

typedef struct ScopeAllocationProbe {
    YazaArenaV1 *arena;
    void *allocation;
} ScopeAllocationProbe;

static void *try_cross_thread_scope_allocation(void *opaque) {
    ScopeAllocationProbe *probe = (ScopeAllocationProbe *)opaque;
    probe->allocation = yaza_arena_v1_alloc(probe->arena, 16);
    return NULL;
}

int main(void) {
    yaza_rt_v1_init();
    YazaArenaV1 *worker_arena = yaza_arena_v1_create(256ULL * 1024ULL);
    assert(worker_arena != NULL);
    YazaArenaCycleMetricsV1 initial_metrics = {.struct_size = sizeof(initial_metrics)};
    assert(yaza_arena_v1_last_cycle_metrics(worker_arena, &initial_metrics) == 1);
    assert(initial_metrics.generation == 0);
    YazaArenaCycleMetricsV1 undersized_metrics = {.struct_size = sizeof(undersized_metrics) - 1};
    assert(yaza_arena_v1_last_cycle_metrics(worker_arena, &undersized_metrics) == 0);
    YazaArenaContextV1 context = {0};

    for (uint64_t request = 0; request < REQUEST_COUNT; ++request) {
        assert(yaza_arena_v1_scope_begin(worker_arena, &context) == 1);
        assert(yaza_arena_v1_current() == worker_arena);
        if (request == 0) {
            YazaArenaContextV1 conflicting_context = {0};
            assert(yaza_arena_v1_enter(worker_arena, &conflicting_context) == 0);
            assert(yaza_arena_v1_release(worker_arena) == 0);
            assert(yaza_arena_v1_retain(worker_arena) == 1);
            assert(yaza_arena_v1_scope_end(&context) == 0);
            assert(context.active == 1);
            assert(yaza_arena_v1_release(worker_arena) == 1);
            assert(yaza_arena_v1_leave(&context) == 0);
            assert(context.active == 1);
            ScopeAllocationProbe probe = {worker_arena, NULL};
            pthread_t thread;
            assert(pthread_create(&thread, NULL, try_cross_thread_scope_allocation, &probe) == 0);
            assert(pthread_join(thread, NULL) == 0);
            assert(probe.allocation == NULL);

            YazaArenaV1 *bounded_child = yaza_arena_v1_create_child(worker_arena, 64);
            assert(bounded_child != NULL);
            assert(yaza_arena_v1_alloc(bounded_child, 65) == NULL);
            assert(yaza_arena_v1_destroy(bounded_child) == 1);
        }
        void *allocation = yaza_rt_v1_alloc(128);
        assert(allocation != NULL);
        assert((uintptr_t)allocation % _Alignof(max_align_t) == 0);
        void *texts = yaza_list_v1_create(6);
        assert(texts != NULL);
        YazaTextV1 per = {(const unsigned char *)"per", 3};
        YazaTextV1 request_text = {(const unsigned char *)"request", 7};
        YazaTextV1 *joined = yaza_rt_v1_text_concat(&per, &request_text);
        assert(yaza_list_v1_push(texts, (uint64_t)(uintptr_t)joined, 6) == 0);
        assert(yaza_list_v1_count(texts, 6) == 1);
        const YazaTextV1 *retained_text = (const YazaTextV1 *)(uintptr_t)yaza_list_v1_get(texts, 0, 6);
        assert(retained_text->length == 10 && memcmp(retained_text->data, "perrequest", 10) == 0);
        assert(yaza_arena_v1_used(worker_arena) > 128);
        assert(yaza_arena_v1_storage_used(worker_arena) >= yaza_arena_v1_used(worker_arena));
        assert(yaza_arena_v1_peak_storage_used(worker_arena) >= yaza_arena_v1_storage_used(worker_arena));
        assert(yaza_arena_v1_peak_reserved(worker_arena) >= yaza_arena_v1_reserved(worker_arena));
        assert(yaza_arena_v1_scope_begin(worker_arena, &context) == 0);
        assert(yaza_arena_v1_scope_end(&context) == 1);
        assert(yaza_arena_v1_current() == yaza_rt_v1_program_arena_handle());
        assert(yaza_arena_v1_used(worker_arena) == 0);
        assert(yaza_arena_v1_storage_used(worker_arena) == 0);
        assert(yaza_arena_v1_reserved(worker_arena) == 64ULL * 1024ULL);
        if (request == 0) {
            YazaArenaCycleMetricsV1 metrics = {.struct_size = sizeof(metrics)};
            assert(yaza_arena_v1_last_cycle_metrics(worker_arena, &metrics) == 1);
            assert(metrics.abi_version == YAZA_ARENA_CYCLE_METRICS_V1_ABI_VERSION);
            assert(metrics.generation == 2);
            assert(metrics.successful_allocations >= 3);
            assert(metrics.allocation_failures == 2);
            assert(metrics.peak_used_bytes > 128);
            assert(metrics.peak_storage_used_bytes >= metrics.peak_used_bytes);
            assert(metrics.peak_reserved_bytes >= 64ULL * 1024ULL);
            assert(metrics.retained_reserved_bytes == 64ULL * 1024ULL);
        }
    }

    assert(yaza_arena_v1_generation(worker_arena) == (2ULL * REQUEST_COUNT) + 1ULL);
    assert(yaza_arena_v1_last_cycle_generation(worker_arena) == 2ULL * REQUEST_COUNT);
    assert(yaza_arena_v1_last_cycle_peak_used(worker_arena) > 128);
    assert(yaza_arena_v1_last_cycle_peak_storage_used(worker_arena) >= yaza_arena_v1_last_cycle_peak_used(worker_arena));
    assert(yaza_arena_v1_last_cycle_peak_reserved(worker_arena) >= 64ULL * 1024ULL);
    YazaArenaCycleMetricsV1 last_metrics = {.struct_size = sizeof(last_metrics)};
    assert(yaza_arena_v1_last_cycle_metrics(worker_arena, &last_metrics) == 1);
    assert(last_metrics.generation == 2ULL * REQUEST_COUNT);
    assert(last_metrics.successful_allocations >= 3);
    assert(last_metrics.allocation_failures == 0);
    assert(last_metrics.retained_reserved_bytes == 64ULL * 1024ULL);
    assert(yaza_arena_v1_allocation_count(worker_arena) == 0);
    assert(yaza_rt_v1_arena_used() == 0);
    assert(yaza_rt_v1_arena_storage_used() == 0);
    assert(yaza_rt_v1_arena_reserved() == 64ULL * 1024ULL);
    assert(yaza_rt_v1_arena_peak_storage_used() > 0);
    assert(yaza_rt_v1_arena_peak_reserved() >= 64ULL * 1024ULL);
    assert(yaza_rt_v1_arena_allocation_count() >= 5ULL * REQUEST_COUNT);
    assert(yaza_arena_v1_destroy(worker_arena) == 1);
    assert(yaza_rt_v1_arena_used() == 0);
    assert(yaza_rt_v1_arena_storage_used() == 0);
    assert(yaza_rt_v1_arena_reserved() == 0);

    YazaArenaV1 *parent_arena = yaza_arena_v1_create(256ULL * 1024ULL);
    assert(parent_arena != NULL);
    YazaArenaContextV1 parent_context = {0};
    assert(yaza_arena_v1_scope_begin(parent_arena, &parent_context) == 1);
    assert(yaza_rt_v1_alloc(32) != NULL);
    YazaArenaV1 *nested_arena = yaza_arena_v1_create_child(parent_arena, 64ULL * 1024ULL);
    assert(nested_arena != NULL);
    YazaArenaContextV1 nested_context = {0};
    assert(yaza_arena_v1_scope_begin(nested_arena, &nested_context) == 1);
    assert(yaza_arena_v1_current() == nested_arena);
    assert(yaza_rt_v1_alloc(16) != NULL);
    assert(yaza_arena_v1_scope_end(&nested_context) == 1);
    assert(yaza_arena_v1_current() == parent_arena);
    assert(yaza_arena_v1_destroy(nested_arena) == 1);
    assert(yaza_arena_v1_scope_end(&parent_context) == 1);
    assert(yaza_arena_v1_destroy(parent_arena) == 1);

    YazaArenaV1 *request_arena = yaza_arena_v1_create(256ULL * 1024ULL);
    assert(request_arena != NULL);
    YazaArenaContextV1 request_context = {0};
    assert(yaza_arena_v1_scope_begin(request_arena, &request_context) == 1);
    assert(yaza_rt_v1_alloc(64) != NULL);
    YazaArenaV1 *task_arena = yaza_arena_v1_create_child(request_arena, 64ULL * 1024ULL);
    assert(task_arena != NULL);
    assert(yaza_arena_v1_scope_end(&request_context) == 0);
    assert(request_context.active == 1);
    assert(yaza_arena_v1_current() == request_arena);
    assert(yaza_arena_v1_leave(&request_context) == 0);
    assert(yaza_arena_v1_used(request_arena) == 64);
    assert(yaza_arena_v1_storage_used(request_arena) >= 64);
    assert(yaza_arena_v1_destroy(task_arena) == 1);
    assert(yaza_arena_v1_scope_end(&request_context) == 1);
    assert(request_context.active == 0);
    assert(yaza_arena_v1_current() == yaza_rt_v1_program_arena_handle());
    assert(yaza_arena_v1_used(request_arena) == 0);
    assert(yaza_arena_v1_storage_used(request_arena) == 0);
    assert(yaza_arena_v1_last_cycle_peak_used(request_arena) == 64);
    assert(yaza_arena_v1_destroy(request_arena) == 1);

    puts("arena-v1: 1,000,000 request scopes reclaimed runtime allocations, Text concatenations, and typed lists; child scopes blocked premature cleanup");
    return 0;
}

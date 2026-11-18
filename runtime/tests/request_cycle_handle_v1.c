#include "yaza_runtime_v1.h"

#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

typedef struct CycleEndProbe {
    YazaArenaCycleV1 *cycle;
    int32_t result;
} CycleEndProbe;

typedef struct TaskCycleProbe {
    YazaArenaCycleV1 *cycle;
    uint64_t allocations;
    pthread_mutex_t lock;
    pthread_cond_t condition;
    int started;
    int finish;
} TaskCycleProbe;

static void *try_end_cycle_on_another_thread(void *opaque) {
    CycleEndProbe *probe = (CycleEndProbe *)opaque;
    YazaArenaCycleMetricsV1 metrics = {.struct_size = sizeof(metrics)};
    probe->result = yaza_arena_v1_cycle_end(probe->cycle, &metrics);
    return NULL;
}

static void *run_child_cycle(void *opaque) {
    TaskCycleProbe *probe = (TaskCycleProbe *)opaque;
    assert(yaza_arena_v1_cycle_begin(probe->cycle) == 1);
    assert(yaza_rt_v1_alloc(16) != NULL);
    assert(pthread_mutex_lock(&probe->lock) == 0);
    probe->started = 1;
    assert(pthread_cond_broadcast(&probe->condition) == 0);
    while (!probe->finish) assert(pthread_cond_wait(&probe->condition, &probe->lock) == 0);
    assert(pthread_mutex_unlock(&probe->lock) == 0);
    YazaArenaCycleMetricsV1 metrics = {.struct_size = sizeof(metrics)};
    assert(yaza_arena_v1_cycle_end(probe->cycle, &metrics) == 1);
    probe->allocations = metrics.successful_allocations;
    assert(metrics.allocation_failures == 0);
    assert(yaza_arena_v1_cycle_destroy(probe->cycle) == 1);
    return NULL;
}

int main(void) {
    yaza_rt_v1_init();
    YazaArenaV1 *parent = yaza_arena_v1_create(1024ULL * 1024ULL);
    assert(parent != NULL);
    YazaArenaCycleV1 *cycle = yaza_arena_v1_cycle_create(parent, 256ULL * 1024ULL);
    assert(cycle != NULL);

    assert(yaza_arena_v1_cycle_begin(cycle) == 1);
    assert(yaza_arena_v1_current() != parent);
    assert(yaza_arena_v1_cycle_begin(cycle) == 0);
    assert(yaza_arena_v1_cycle_destroy(cycle) == 0);
    assert(yaza_rt_v1_alloc(32) != NULL);

    YazaArenaCycleMetricsV1 small_metrics = {.struct_size = sizeof(small_metrics) - 1};
    assert(yaza_arena_v1_cycle_end(cycle, &small_metrics) == 0);
    assert(yaza_arena_v1_current() != parent);

    CycleEndProbe probe = {.cycle = cycle, .result = -1};
    pthread_t wrong_thread;
    assert(pthread_create(&wrong_thread, NULL, try_end_cycle_on_another_thread, &probe) == 0);
    assert(pthread_join(wrong_thread, NULL) == 0);
    assert(probe.result == 0);
    assert(yaza_arena_v1_current() != parent);

    YazaArenaV1 *task = yaza_arena_v1_create_child(yaza_arena_v1_current(), 128ULL * 1024ULL);
    assert(task != NULL);
    assert(yaza_arena_v1_alloc(task, 16) != NULL);
    YazaArenaCycleMetricsV1 metrics = {.struct_size = sizeof(metrics)};
    assert(yaza_arena_v1_cycle_end(cycle, &metrics) == 0);
    assert(yaza_arena_v1_current() != parent);
    assert(yaza_arena_v1_destroy(task) == 1);

    YazaArenaCycleV1 *child_cycle = yaza_arena_v1_cycle_create(yaza_arena_v1_current(), 128ULL * 1024ULL);
    assert(child_cycle != NULL);
    TaskCycleProbe task_probe = {.cycle = child_cycle, .allocations = 0, .started = 0, .finish = 0};
    assert(pthread_mutex_init(&task_probe.lock, NULL) == 0);
    assert(pthread_cond_init(&task_probe.condition, NULL) == 0);
    pthread_t task_thread;
    assert(pthread_create(&task_thread, NULL, run_child_cycle, &task_probe) == 0);
    assert(pthread_mutex_lock(&task_probe.lock) == 0);
    while (!task_probe.started) assert(pthread_cond_wait(&task_probe.condition, &task_probe.lock) == 0);
    assert(pthread_mutex_unlock(&task_probe.lock) == 0);
    assert(yaza_arena_v1_cycle_end(cycle, &metrics) == 0);
    assert(yaza_arena_v1_current() != parent);
    assert(pthread_mutex_lock(&task_probe.lock) == 0);
    task_probe.finish = 1;
    assert(pthread_cond_broadcast(&task_probe.condition) == 0);
    assert(pthread_mutex_unlock(&task_probe.lock) == 0);
    assert(pthread_join(task_thread, NULL) == 0);
    assert(pthread_cond_destroy(&task_probe.condition) == 0);
    assert(pthread_mutex_destroy(&task_probe.lock) == 0);
    assert(task_probe.allocations == 1);

    assert(yaza_arena_v1_cycle_end(cycle, &metrics) == 1);
    assert(metrics.abi_version == YAZA_ARENA_CYCLE_METRICS_V1_ABI_VERSION);
    assert(metrics.generation == 2);
    assert(metrics.successful_allocations == 3);
    assert(metrics.allocation_failures == 0);
    assert(metrics.peak_used_bytes >= 48);
    assert(metrics.peak_storage_used_bytes >= metrics.peak_used_bytes);
    assert(metrics.peak_reserved_bytes >= 128ULL * 1024ULL);
    assert(metrics.retained_reserved_bytes == 64ULL * 1024ULL);
    assert(yaza_arena_v1_current() == yaza_rt_v1_program_arena_handle());
    YazaArenaCycleMetricsV1 last_metrics = {.struct_size = sizeof(last_metrics)};
    assert(yaza_arena_v1_cycle_last_metrics(cycle, &last_metrics) == 1);
    assert(last_metrics.generation == metrics.generation);
    assert(last_metrics.successful_allocations == metrics.successful_allocations);
    assert(last_metrics.peak_used_bytes == metrics.peak_used_bytes);
    YazaArenaCycleMetricsV1 undersized_last_metrics = {.struct_size = sizeof(undersized_last_metrics) - 1};
    assert(yaza_arena_v1_cycle_last_metrics(cycle, &undersized_last_metrics) == 0);

    assert(yaza_arena_v1_cycle_begin(cycle) == 1);
    assert(yaza_rt_v1_alloc(24) != NULL);
    YazaArenaV1 *active_arena = yaza_arena_v1_current();
    assert(yaza_arena_v1_retain(active_arena) == 1);
    assert(yaza_arena_v1_cycle_end(cycle, &metrics) == 0);
    assert(yaza_arena_v1_current() == active_arena);
    assert(yaza_arena_v1_release(active_arena) == 1);
    assert(yaza_arena_v1_cycle_end(cycle, &metrics) == 1);
    assert(metrics.generation == 4);
    assert(metrics.successful_allocations == 1);
    assert(metrics.allocation_failures == 0);
    assert(metrics.peak_used_bytes == 24);
    assert(metrics.retained_reserved_bytes == 64ULL * 1024ULL);

    assert(yaza_arena_v1_cycle_destroy(cycle) == 1);
    assert(yaza_arena_v1_used(parent) == 0);
    assert(yaza_arena_v1_reserved(parent) == 0);
    assert(yaza_arena_v1_destroy(parent) == 1);
    assert(yaza_rt_v1_live_scope_count() == 0);
    puts("arena-v1: opaque reusable cycle handle preserves retry, thread affinity, metrics, and cleanup");
    return 0;
}

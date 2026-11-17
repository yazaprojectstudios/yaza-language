#define _POSIX_C_SOURCE 200809L

#include "yaza_runtime_v1.h"

#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_WORKERS 8
#define ITERATIONS_PER_WORKER 500000ULL
#define REPETITIONS 5
#define ALLOCATION_BYTES 24ULL

#if defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_METRICS)
#define METRIC_MODE "process_live_metrics_skipped"
#elif defined(YAZA_RT_V1_BENCH_GLOBAL_PROCESS_LIVE_METRICS)
#define METRIC_MODE "global_process_live_metrics"
#elif defined(YAZA_RT_V1_BENCH_GLOBAL_PROCESS_ALLOCATION_COUNT)
#define METRIC_MODE "global_process_allocation_count_reference"
#elif defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_PEAK_METRICS)
#define METRIC_MODE "process_peaks_skipped"
#elif defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_STORAGE) && defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_ALLOCATION_COUNT)
#define METRIC_MODE "process_used_only"
#elif defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_USED)
#define METRIC_MODE "process_used_skipped"
#elif defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_STORAGE)
#define METRIC_MODE "process_storage_skipped"
#elif defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_ALLOCATION_COUNT)
#define METRIC_MODE "process_alloc_count_skipped"
#else
#define METRIC_MODE "sharded_process_live_metrics"
#endif

typedef struct Worker {
    YazaArenaV1 *arena;
    uint64_t iterations;
    _Atomic int *start;
    int failed;
} Worker;

static uint64_t monotonic_nanoseconds(void) {
    struct timespec value;
    assert(clock_gettime(CLOCK_MONOTONIC, &value) == 0);
    return (uint64_t)value.tv_sec * 1000000000ULL + (uint64_t)value.tv_nsec;
}

static void *allocate_worker(void *opaque) {
    Worker *worker = (Worker *)opaque;
    while (!atomic_load_explicit(worker->start, memory_order_acquire)) {
    }
    for (uint64_t index = 0; index < worker->iterations; ++index) {
        unsigned char *allocation = (unsigned char *)yaza_arena_v1_alloc(worker->arena, ALLOCATION_BYTES);
        if (allocation == NULL || (uintptr_t)allocation % _Alignof(max_align_t) != 0) {
            worker->failed = 1;
            return NULL;
        }
        allocation[0] = (unsigned char)index;
    }
    return NULL;
}

static void sort_three(uint64_t values[REPETITIONS]) {
    for (size_t left = 0; left < REPETITIONS; ++left) {
        for (size_t right = left + 1; right < REPETITIONS; ++right) {
            if (values[right] < values[left]) {
                const uint64_t temporary = values[left];
                values[left] = values[right];
                values[right] = temporary;
            }
        }
    }
}

static uint64_t run_one_case(int shared_tree, int worker_count) {
    YazaArenaV1 *arenas[MAX_WORKERS] = {0};
    Worker workers[MAX_WORKERS] = {0};
    pthread_t threads[MAX_WORKERS];
    _Atomic int start = 0;
    const uint64_t bytes_per_worker = ITERATIONS_PER_WORKER * ALLOCATION_BYTES;
    const uint64_t per_arena_limit = bytes_per_worker + 1024ULL * 1024ULL;

    if (shared_tree) {
        arenas[0] = yaza_arena_v1_create(bytes_per_worker * (uint64_t)worker_count + 1024ULL * 1024ULL);
        assert(arenas[0] != NULL);
    } else {
        for (int index = 0; index < worker_count; ++index) {
            arenas[index] = yaza_arena_v1_create(per_arena_limit);
            assert(arenas[index] != NULL);
        }
    }

    for (int index = 0; index < worker_count; ++index) {
        workers[index].arena = shared_tree ? arenas[0] : arenas[index];
        workers[index].iterations = ITERATIONS_PER_WORKER;
        workers[index].start = &start;
        assert(pthread_create(&threads[index], NULL, allocate_worker, &workers[index]) == 0);
    }

    const uint64_t started = monotonic_nanoseconds();
    atomic_store_explicit(&start, 1, memory_order_release);
    for (int index = 0; index < worker_count; ++index) {
        assert(pthread_join(threads[index], NULL) == 0);
        assert(workers[index].failed == 0);
    }
    const uint64_t elapsed = monotonic_nanoseconds() - started;
    const uint64_t expected_bytes = bytes_per_worker * (uint64_t)worker_count;
    uint64_t expected_process_storage = 0;
    if (shared_tree) {
        assert(yaza_arena_v1_used(arenas[0]) == expected_bytes);
        assert(yaza_arena_v1_allocation_count(arenas[0]) == ITERATIONS_PER_WORKER * (uint64_t)worker_count);
        expected_process_storage = yaza_arena_v1_storage_used(arenas[0]);
#if !defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_METRICS) && !defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_USED)
        assert(yaza_rt_v1_arena_used() == expected_bytes);
#endif
#if !defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_METRICS) && !defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_STORAGE)
        assert(yaza_rt_v1_arena_storage_used() == expected_process_storage);
#endif
        assert(yaza_arena_v1_destroy(arenas[0]) == 1);
    } else {
        uint64_t total_bytes = 0;
        uint64_t total_allocations = 0;
        for (int index = 0; index < worker_count; ++index) {
            total_bytes += yaza_arena_v1_used(arenas[index]);
            expected_process_storage += yaza_arena_v1_storage_used(arenas[index]);
            total_allocations += yaza_arena_v1_allocation_count(arenas[index]);
        }
        assert(total_bytes == expected_bytes);
        assert(total_allocations == ITERATIONS_PER_WORKER * (uint64_t)worker_count);
#if !defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_METRICS) && !defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_USED)
        assert(yaza_rt_v1_arena_used() == expected_bytes);
#endif
#if !defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_METRICS) && !defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_STORAGE)
        assert(yaza_rt_v1_arena_storage_used() == expected_process_storage);
#endif
        for (int index = 0; index < worker_count; ++index) {
            assert(yaza_arena_v1_destroy(arenas[index]) == 1);
        }
    }
#if !defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_METRICS) && !defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_USED)
    assert(yaza_rt_v1_arena_used() == 0);
#endif
#if !defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_METRICS) && !defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_STORAGE)
    assert(yaza_rt_v1_arena_storage_used() == 0);
#endif
    (void)expected_process_storage;
    return elapsed;
}

static void run_case(const char *name, int shared_tree, int worker_count) {
    uint64_t samples[REPETITIONS];
    for (int repetition = 0; repetition < REPETITIONS; ++repetition) {
        samples[repetition] = run_one_case(shared_tree, worker_count);
    }
    sort_three(samples);
    const uint64_t total_allocations = ITERATIONS_PER_WORKER * (uint64_t)worker_count;
    const double seconds = (double)samples[1] / 1000000000.0;
    const double allocations_per_second = (double)total_allocations / seconds;
    printf("%s,%s,%d,%llu,%llu,%.3f,%.0f\n", METRIC_MODE, name, worker_count,
        (unsigned long long)ITERATIONS_PER_WORKER,
        (unsigned long long)samples[1], seconds * 1000.0, allocations_per_second);
}

int main(void) {
    yaza_rt_v1_init();
    puts("metrics,arena_mode,workers,allocations_per_worker,median_ns,median_ms,allocations_per_second");
    const int worker_counts[] = {1, 2, 4, 8};
    for (size_t index = 0; index < sizeof(worker_counts) / sizeof(worker_counts[0]); ++index) {
        run_case("shared_tree", 1, worker_counts[index]);
        run_case("independent_roots", 0, worker_counts[index]);
    }
    return 0;
}

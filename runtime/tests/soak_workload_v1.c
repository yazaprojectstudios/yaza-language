#include "yaza_runtime_v1.h"

#include <assert.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>

#define DEFAULT_WORKER_COUNT 8
#ifndef TOTAL_REQUESTS
#define TOTAL_REQUESTS 100000ULL
#endif

typedef struct SoakWorker {
    uint32_t id;
    uint64_t request_count;
    uint64_t completed;
    uint64_t failure_count;
    YazaArenaV1 *parent;
    YazaArenaCycleV1 *cycle;
    uint64_t *latencies_ns;
} SoakWorker;

static uint64_t get_rss_bytes(void) {
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) != 0) return 0;
#if defined(__APPLE__)
    return (uint64_t)usage.ru_maxrss;
#else
    return (uint64_t)usage.ru_maxrss * 1024ULL;
#endif
}

static int compare_u64(const void *a, const void *b) {
    const uint64_t va = *(const uint64_t *)a;
    const uint64_t vb = *(const uint64_t *)b;
    if (va < vb) return -1;
    if (va > vb) return 1;
    return 0;
}

static void *run_soak_worker(void *opaque) {
    SoakWorker *w = (SoakWorker *)opaque;
    for (uint64_t i = 0; i < w->request_count; ++i) {
        const int64_t start_ns = yaza_rt_v1_monotonic_ns();
        if (yaza_arena_v1_cycle_begin(w->cycle) != 1) {
            w->failure_count += 1;
            continue;
        }

        // Simulate typical microservice request workload:
        // 1. Allocate typed list
        void *list = yaza_list_v1_try_create_typed("NumberList", sizeof(int64_t));
        assert(list != NULL);
        for (int64_t item = 0; item < 16; ++item) {
            int64_t val = (int64_t)(i + (uint64_t)item);
            assert(yaza_list_v1_try_push_typed(list, &val, "NumberList", sizeof(int64_t)) == 1);
        }

        // 2. Allocate and format text
        YazaTextV1 *greeting = yaza_rt_v1_text_from_cstr("status=ok;worker=");
        assert(greeting != NULL);
        YazaTextV1 id_text = {(const unsigned char *)"1", 1};
        YazaTextV1 *msg = yaza_rt_v1_try_text_concat(greeting, &id_text);
        assert(msg != NULL && msg->length > 0);

        // 3. Complete cycle and capture metrics
        YazaArenaCycleMetricsV1 metrics = {.struct_size = sizeof(metrics)};
        if (yaza_arena_v1_cycle_end(w->cycle, &metrics) != 1) {
            w->failure_count += 1;
            continue;
        }

        const int64_t end_ns = yaza_rt_v1_monotonic_ns();
        if (w->latencies_ns != NULL) {
            w->latencies_ns[i] = (uint64_t)(end_ns - start_ns);
        }
        w->completed += 1;
    }
    return NULL;
}

int main(int argc, char **argv) {
    yaza_rt_v1_init();

    uint64_t total_requests = TOTAL_REQUESTS;
    if (argc > 1) {
        long long val = atoll(argv[1]);
        if (val > 0) total_requests = (uint64_t)val;
    }
    const uint32_t worker_count = DEFAULT_WORKER_COUNT;
    const uint64_t per_worker = total_requests / worker_count;

    printf("=================================================================\n");
    printf(" YAZA Microservice Soak / Load Test Profile\n");
    printf(" Workers: %u | Target Requests: %" PRIu64 " (%" PRIu64 "/worker)\n",
           worker_count, total_requests, per_worker);
    printf("=================================================================\n");

    const uint64_t initial_rss = get_rss_bytes();
    const int64_t soak_start_ns = yaza_rt_v1_monotonic_ns();

    SoakWorker workers[DEFAULT_WORKER_COUNT] = {0};
    pthread_t threads[DEFAULT_WORKER_COUNT];

    for (uint32_t i = 0; i < worker_count; ++i) {
        workers[i].id = i;
        workers[i].request_count = per_worker;
        workers[i].parent = yaza_arena_v1_create(256ULL * 1024ULL);
        assert(workers[i].parent != NULL);
        workers[i].cycle = yaza_arena_v1_cycle_create(workers[i].parent, 128ULL * 1024ULL);
        assert(workers[i].cycle != NULL);
        workers[i].latencies_ns = (uint64_t *)calloc(per_worker, sizeof(uint64_t));
        assert(pthread_create(&threads[i], NULL, run_soak_worker, &workers[i]) == 0);
    }

    uint64_t total_completed = 0;
    uint64_t total_failures = 0;
    for (uint32_t i = 0; i < worker_count; ++i) {
        assert(pthread_join(threads[i], NULL) == 0);
        total_completed += workers[i].completed;
        total_failures += workers[i].failure_count;
    }

    const int64_t soak_end_ns = yaza_rt_v1_monotonic_ns();
    const double elapsed_sec = (double)(soak_end_ns - soak_start_ns) / 1000000000.0;
    const double rps = (double)total_completed / (elapsed_sec > 0.0001 ? elapsed_sec : 1.0);

    // Merge and compute latency percentiles
    uint64_t *all_latencies = (uint64_t *)malloc(total_completed * sizeof(uint64_t));
    if (all_latencies != NULL) {
        uint64_t offset = 0;
        for (uint32_t i = 0; i < worker_count; ++i) {
            if (workers[i].latencies_ns != NULL) {
                memcpy(all_latencies + offset, workers[i].latencies_ns, workers[i].completed * sizeof(uint64_t));
                offset += workers[i].completed;
                free(workers[i].latencies_ns);
            }
        }
        qsort(all_latencies, total_completed, sizeof(uint64_t), compare_u64);
        const double p50_us = (double)all_latencies[total_completed * 50 / 100] / 1000.0;
        const double p95_us = (double)all_latencies[total_completed * 95 / 100] / 1000.0;
        const double p99_us = (double)all_latencies[total_completed * 99 / 100] / 1000.0;
        printf(" Throughput: %.0f req/sec | Elapsed: %.3f sec\n", rps, elapsed_sec);
        printf(" Latency p50: %.2f µs | p95: %.2f µs | p99: %.2f µs\n", p50_us, p95_us, p99_us);
        free(all_latencies);
    }

    // Measure retained memory before worker destruction
    const uint64_t active_reserved = yaza_rt_v1_arena_reserved();
    const uint64_t active_used = yaza_rt_v1_arena_used();
    const uint64_t peak_reserved = yaza_rt_v1_arena_peak_reserved();
    const uint64_t post_soak_rss = get_rss_bytes();

    printf(" Retained Managed Reserved: %" PRIu64 " bytes (%.1f KiB/worker)\n",
           active_reserved, (double)active_reserved / (double)worker_count / 1024.0);
    printf(" Retained Managed Used:     %" PRIu64 " bytes\n", active_used);
    printf(" Process Peak Reserved:     %" PRIu64 " bytes (%.1f KiB)\n",
           peak_reserved, (double)peak_reserved / 1024.0);
    printf(" Process RSS: Start = %.1f MiB | End = %.1f MiB\n",
           (double)initial_rss / (1024.0 * 1024.0), (double)post_soak_rss / (1024.0 * 1024.0));
    printf(" Total Failures: %" PRIu64 "\n", total_failures);

    // Destroy all worker cycles and parent arenas
    for (uint32_t i = 0; i < worker_count; ++i) {
        assert(yaza_arena_v1_cycle_destroy(workers[i].cycle) == 1);
        assert(yaza_arena_v1_destroy(workers[i].parent) == 1);
    }

    // Verify all memory is 100% reclaimed
    assert(yaza_rt_v1_arena_used() == 0);
    assert(yaza_rt_v1_arena_storage_used() == 0);
    assert(yaza_rt_v1_arena_reserved() == 0);
    assert(yaza_rt_v1_arena_scope_count() == 0);
    assert(total_failures == 0);

    printf(" Memory Bound Invariant: PASS (Zero Retained Managed Bytes on Worker Destroy)\n");
    printf("=================================================================\n");
    return 0;
}

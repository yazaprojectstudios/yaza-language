#include "yaza_runtime_v1.h"

#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define WORKER_COUNT 8
#ifndef REQUESTS_PER_WORKER
#define REQUESTS_PER_WORKER 2000ULL
#endif

extern int32_t yaza_export_transform_request_v1(const YazaModuleInputV1 *inputs, uint64_t input_count, YazaModuleResultV1 *out_result);

typedef struct Worker {
    YazaArenaV1 *parent;
    YazaArenaCycleV1 *cycle;
    uint64_t completed;
} Worker;

static void initialize_result(YazaModuleResultV1 *result) {
    memset(result, 0, sizeof(*result));
    result->abi_version = YAZA_MODULE_RESULT_V1_ABI_VERSION;
    result->struct_size = sizeof(*result);
}

static YazaModuleInputV1 text_input(const YazaTextV1 *value) {
    YazaModuleInputV1 input = {
        YAZA_MODULE_INPUT_V1_ABI_VERSION,
        sizeof(YazaModuleInputV1),
        YAZA_MODULE_INPUT_V1_TEXT,
        0,
        value->data,
        value->length
    };
    return input;
}

static YazaModuleInputV1 bytes_input(const YazaBytesV1 *value) {
    YazaModuleInputV1 input = {
        YAZA_MODULE_INPUT_V1_ABI_VERSION,
        sizeof(YazaModuleInputV1),
        YAZA_MODULE_INPUT_V1_BYTES,
        0,
        value->data,
        value->length
    };
    return input;
}

static int32_t invoke_request(const YazaTextV1 *route, const YazaBytesV1 *payload, YazaModuleResultV1 *result) {
    const YazaModuleInputV1 inputs[] = {text_input(route), bytes_input(payload)};
    return yaza_export_transform_request_v1(inputs, 2, result);
}

static void check_success_result(const YazaModuleResultV1 *result, const unsigned char *payload, uint64_t payload_length) {
    static const unsigned char route[] = "route";
    assert(result->status == YAZA_MODULE_RESULT_V1_OK);
    assert(result->data != NULL);
    assert(result->length == sizeof(route) - 1 + 1 + payload_length);
    assert(memcmp(result->data, route, sizeof(route) - 1) == 0);
    assert(result->data[sizeof(route) - 1] == ':');
    assert(memcmp(result->data + sizeof(route), payload, (size_t)payload_length) == 0);
}

static void *run_worker(void *opaque) {
    Worker *worker = (Worker *)opaque;
    static const unsigned char route_data[] = "route";
    for (uint64_t request = 0; request < REQUESTS_PER_WORKER; ++request) {
        unsigned char payload_data[] = {0, 0xff, (unsigned char)request, 0x7f};
        const YazaTextV1 route = {route_data, sizeof(route_data) - 1};
        const YazaBytesV1 payload = {payload_data, sizeof(payload_data)};
        YazaModuleResultV1 result;
        initialize_result(&result);

        assert(yaza_arena_v1_cycle_begin(worker->cycle) == 1);
        assert(invoke_request(&route, &payload, &result) == 0);
        check_success_result(&result, payload_data, sizeof(payload_data));
        memset(payload_data, 0xa5, sizeof(payload_data));
        assert(result.data[sizeof(route_data) - 1] == ':');
        assert(memcmp(result.data + sizeof(route_data), "\0\xff", 2) == 0);

        if (request % 101ULL == 0) {
            const YazaTextV1 empty_route = {NULL, 0};
            initialize_result(&result);
            assert(invoke_request(&empty_route, &payload, &result) == 0);
            assert(result.status == YAZA_MODULE_RESULT_V1_APPLICATION_ERROR);
            assert(result.length == strlen("route is required"));
            assert(memcmp(result.data, "route is required", result.length) == 0);
        }

        YazaArenaCycleMetricsV1 metrics = {.struct_size = sizeof(metrics)};
        assert(yaza_arena_v1_cycle_end(worker->cycle, &metrics) == 1);
        assert(metrics.generation == 2ULL * (request + 1));
        assert(metrics.successful_allocations >= 4);
        assert(metrics.allocation_failures == 0);
        assert(metrics.peak_used_bytes > 0);
        assert(metrics.peak_storage_used_bytes >= metrics.peak_used_bytes);
        assert(metrics.retained_reserved_bytes <= 64ULL * 1024ULL);
        assert(yaza_rt_v1_current_arena_used() == 0);
        worker->completed += 1;
    }
    return NULL;
}

static void check_input_rejections(void) {
    static const unsigned char route_data[] = "route";
    static const unsigned char invalid_utf8[] = {0xc0};
    static unsigned char byte = 0;
    const YazaBytesV1 payload = {&byte, 1};
    YazaArenaV1 *parent = yaza_arena_v1_create(4096);
    assert(parent != NULL);
    YazaArenaCycleV1 *cycle = yaza_arena_v1_cycle_create(parent, 2048);
    assert(cycle != NULL);

    const YazaTextV1 invalid = {invalid_utf8, sizeof(invalid_utf8)};
    YazaModuleResultV1 result;
    initialize_result(&result);
    assert(yaza_arena_v1_cycle_begin(cycle) == 1);
    assert(invoke_request(&invalid, &payload, &result) == YAZA_MODULE_RESULT_V1_INVALID_INPUT);
    assert(result.status == YAZA_MODULE_RESULT_V1_INVALID_INPUT);
    assert(result.length > 0 && result.data != NULL);
    YazaArenaCycleMetricsV1 metrics = {.struct_size = sizeof(metrics)};
    assert(yaza_arena_v1_cycle_end(cycle, &metrics) == 1);
    assert(metrics.successful_allocations == 0);
    assert(metrics.allocation_failures == 0);

    const YazaTextV1 valid_route = {route_data, sizeof(route_data) - 1};
    const YazaBytesV1 oversized = {&byte, 64ULL * 1024ULL * 1024ULL + 1};
    initialize_result(&result);
    assert(yaza_arena_v1_cycle_begin(cycle) == 1);
    assert(invoke_request(&valid_route, &oversized, &result) == YAZA_MODULE_RESULT_V1_INVALID_INPUT);
    assert(result.status == YAZA_MODULE_RESULT_V1_INVALID_INPUT);
    metrics = (YazaArenaCycleMetricsV1){.struct_size = sizeof(metrics)};
    assert(yaza_arena_v1_cycle_end(cycle, &metrics) == 1);
    assert(metrics.successful_allocations == 1);
    assert(metrics.allocation_failures == 0);

    assert(yaza_arena_v1_cycle_destroy(cycle) == 1);
    assert(yaza_arena_v1_destroy(parent) == 1);
}

static void check_no_scope_and_quota_errors(void) {
    static const unsigned char route_data[] = "route";
    static unsigned char payload_data[32] = {0};
    const YazaTextV1 route = {route_data, sizeof(route_data) - 1};
    const YazaBytesV1 payload = {payload_data, sizeof(payload_data)};
    YazaModuleResultV1 result;
    initialize_result(&result);
    assert(invoke_request(&route, &payload, &result) == YAZA_MODULE_RESULT_V1_NO_REQUEST_SCOPE);
    assert(result.status == YAZA_MODULE_RESULT_V1_NO_REQUEST_SCOPE);

    YazaArenaV1 *parent = yaza_arena_v1_create(4096);
    assert(parent != NULL);
    YazaArenaCycleV1 *cycle = yaza_arena_v1_cycle_create(parent, 40);
    assert(cycle != NULL);
    initialize_result(&result);
    assert(yaza_arena_v1_cycle_begin(cycle) == 1);
    assert(invoke_request(&route, &payload, &result) == YAZA_MODULE_RESULT_V1_RESOURCE_LIMIT);
    assert(result.status == YAZA_MODULE_RESULT_V1_RESOURCE_LIMIT);
    assert(result.length > 0 && result.data != NULL);
    YazaArenaCycleMetricsV1 metrics = {.struct_size = sizeof(metrics)};
    assert(yaza_arena_v1_cycle_end(cycle, &metrics) == 1);
    assert(metrics.successful_allocations == 1);
    assert(metrics.allocation_failures == 1);
    assert(metrics.peak_used_bytes == sizeof(YazaTextV1) + sizeof(route_data));
    assert(metrics.retained_reserved_bytes <= 40);
    assert(yaza_arena_v1_cycle_destroy(cycle) == 1);
    assert(yaza_arena_v1_destroy(parent) == 1);
}

int main(void) {
    yaza_rt_v1_init();
    check_no_scope_and_quota_errors();
    check_input_rejections();

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
    printf("arena-v1: %llu Text/Bytes YAZA request calls completed across %d worker cycles\n",
        (unsigned long long)completed, WORKER_COUNT);
    return 0;
}

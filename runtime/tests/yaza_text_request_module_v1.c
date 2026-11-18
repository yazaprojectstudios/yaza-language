#include "yaza_runtime_v1.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int32_t yaza_export_transform_text_request_v1(
    const YazaModuleInputV1 *inputs,
    uint64_t input_count,
    YazaModuleResultV1 *out_result);

static void initialize_result(YazaModuleResultV1 *result) {
    memset(result, 0, sizeof(*result));
    result->abi_version = YAZA_MODULE_RESULT_V1_ABI_VERSION;
    result->struct_size = sizeof(*result);
}

static YazaModuleInputV1 text_input(const YazaTextV1 *value) {
    YazaModuleInputV1 input = {
        YAZA_MODULE_INPUT_V1_ABI_VERSION,
        (uint32_t)sizeof(YazaModuleInputV1),
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
        (uint32_t)sizeof(YazaModuleInputV1),
        YAZA_MODULE_INPUT_V1_BYTES,
        0,
        value->data,
        value->length
    };
    return input;
}

static int32_t invoke_request(const YazaTextV1 *route, const YazaBytesV1 *payload, YazaModuleResultV1 *result) {
    static const unsigned char separator_data[] = ":";
    const YazaTextV1 separator = {separator_data, sizeof(separator_data) - 1};
    const YazaModuleInputV1 inputs[] = {text_input(route), bytes_input(payload), text_input(&separator)};
    return yaza_export_transform_text_request_v1(inputs, 3, result);
}

int main(void) {
    static const unsigned char route_data[] = "route";
    static const unsigned char payload_data[] = {0, 0xff, 0x7f, 0x41};
    const YazaTextV1 route = {route_data, sizeof(route_data) - 1};
    const YazaBytesV1 payload = {(unsigned char *)payload_data, sizeof(payload_data)};
    YazaModuleResultV1 result;

    yaza_rt_v1_init();
    initialize_result(&result);
    assert(invoke_request(&route, &payload, &result) == YAZA_MODULE_RESULT_V1_NO_REQUEST_SCOPE);
    assert(result.status == YAZA_MODULE_RESULT_V1_NO_REQUEST_SCOPE);

    YazaArenaV1 *parent = yaza_arena_v1_create(128ULL * 1024ULL);
    assert(parent != NULL);
    YazaArenaCycleV1 *cycle = yaza_arena_v1_cycle_create(parent, 64ULL * 1024ULL);
    assert(cycle != NULL);
    assert(yaza_arena_v1_cycle_begin(cycle) == 1);

    initialize_result(&result);
    assert(invoke_request(&route, &payload, &result) == YAZA_MODULE_RESULT_V1_OK);
    assert(result.status == YAZA_MODULE_RESULT_V1_OK);
    assert(result.length == strlen("route:4"));
    assert(memcmp(result.data, "route:4", result.length) == 0);
    YazaArenaCycleMetricsV1 metrics = {.struct_size = sizeof(metrics)};
    assert(yaza_arena_v1_cycle_end(cycle, &metrics) == 1);
    assert(metrics.successful_allocations >= 5);
    assert(metrics.allocation_failures == 0);
    assert(metrics.retained_reserved_bytes <= 64ULL * 1024ULL);

    assert(yaza_arena_v1_cycle_begin(cycle) == 1);
    const YazaModuleInputV1 valid_inputs[] = {text_input(&route), bytes_input(&payload), text_input(&route)};
    initialize_result(&result);
    assert(yaza_export_transform_text_request_v1(valid_inputs, 2, &result) == YAZA_MODULE_RESULT_V1_INVALID_INPUT);
    assert(result.status == YAZA_MODULE_RESULT_V1_INVALID_INPUT);
    metrics.struct_size = sizeof(metrics);
    assert(yaza_arena_v1_cycle_end(cycle, &metrics) == 1);
    assert(metrics.successful_allocations == 0);
    assert(metrics.allocation_failures == 0);

    assert(yaza_arena_v1_cycle_begin(cycle) == 1);
    YazaModuleInputV1 wrong_version_inputs[] = {text_input(&route), bytes_input(&payload), text_input(&route)};
    wrong_version_inputs[2].abi_version += 1;
    initialize_result(&result);
    assert(yaza_export_transform_text_request_v1(wrong_version_inputs, 3, &result) == YAZA_MODULE_RESULT_V1_INVALID_INPUT);
    assert(result.status == YAZA_MODULE_RESULT_V1_INVALID_INPUT);
    metrics.struct_size = sizeof(metrics);
    assert(yaza_arena_v1_cycle_end(cycle, &metrics) == 1);
    assert(metrics.successful_allocations == 2);
    assert(metrics.allocation_failures == 0);

    assert(yaza_arena_v1_cycle_begin(cycle) == 1);
    YazaModuleInputV1 wrong_kind_inputs[] = {text_input(&route), bytes_input(&payload), text_input(&route)};
    wrong_kind_inputs[1].kind = YAZA_MODULE_INPUT_V1_TEXT;
    initialize_result(&result);
    assert(yaza_export_transform_text_request_v1(wrong_kind_inputs, 3, &result) == YAZA_MODULE_RESULT_V1_INVALID_INPUT);
    assert(result.status == YAZA_MODULE_RESULT_V1_INVALID_INPUT);
    metrics.struct_size = sizeof(metrics);
    assert(yaza_arena_v1_cycle_end(cycle, &metrics) == 1);
    assert(metrics.successful_allocations == 1);
    assert(metrics.allocation_failures == 0);

    assert(yaza_arena_v1_cycle_begin(cycle) == 1);
    const YazaTextV1 empty_route = {NULL, 0};
    initialize_result(&result);
    assert(invoke_request(&empty_route, &payload, &result) == YAZA_MODULE_RESULT_V1_OK);
    assert(result.status == YAZA_MODULE_RESULT_V1_APPLICATION_ERROR);
    assert(result.length == strlen("route is required"));
    assert(memcmp(result.data, "route is required", result.length) == 0);
    metrics.struct_size = sizeof(metrics);
    assert(yaza_arena_v1_cycle_end(cycle, &metrics) == 1);
    assert(metrics.allocation_failures == 0);

    assert(yaza_arena_v1_cycle_begin(cycle) == 1);
    const uint64_t large_route_length = 50000;
    unsigned char *large_route_data = (unsigned char *)malloc((size_t)large_route_length);
    assert(large_route_data != NULL);
    memset(large_route_data, 'r', (size_t)large_route_length);
    const YazaTextV1 large_route = {large_route_data, large_route_length};
    initialize_result(&result);
    assert(invoke_request(&large_route, &payload, &result) == YAZA_MODULE_RESULT_V1_OK);
    assert(result.status == YAZA_MODULE_RESULT_V1_APPLICATION_ERROR);
    assert(result.length == strlen("Could not allocate Text in the active arena"));
    assert(memcmp(result.data, "Could not allocate Text in the active arena", result.length) == 0);
    free(large_route_data);
    metrics.struct_size = sizeof(metrics);
    assert(yaza_arena_v1_cycle_end(cycle, &metrics) == 1);
    assert(metrics.successful_allocations >= 3);
    assert(metrics.allocation_failures == 1);
    assert(metrics.retained_reserved_bytes <= 64ULL * 1024ULL);

    assert(yaza_arena_v1_cycle_begin(cycle) == 1);
    const uint64_t formatter_route_length = 49900;
    unsigned char *formatter_route_data = (unsigned char *)malloc((size_t)formatter_route_length);
    assert(formatter_route_data != NULL);
    memset(formatter_route_data, 'f', (size_t)formatter_route_length);
    const YazaTextV1 formatter_route = {formatter_route_data, formatter_route_length};
    const uint64_t formatter_payload_length = 15570;
    unsigned char *formatter_payload_data = (unsigned char *)malloc((size_t)formatter_payload_length);
    assert(formatter_payload_data != NULL);
    memset(formatter_payload_data, 0xa5, (size_t)formatter_payload_length);
    const YazaBytesV1 formatter_payload = {formatter_payload_data, formatter_payload_length};
    initialize_result(&result);
    assert(invoke_request(&formatter_route, &formatter_payload, &result) == YAZA_MODULE_RESULT_V1_OK);
    assert(result.status == YAZA_MODULE_RESULT_V1_APPLICATION_ERROR);
    assert(result.length == strlen("Could not allocate Text in the active arena"));
    assert(memcmp(result.data, "Could not allocate Text in the active arena", result.length) == 0);
    free(formatter_route_data);
    free(formatter_payload_data);
    metrics.struct_size = sizeof(metrics);
    assert(yaza_arena_v1_cycle_end(cycle, &metrics) == 1);
    assert(metrics.successful_allocations == 3);
    assert(metrics.allocation_failures == 1);
    assert(metrics.retained_reserved_bytes <= 64ULL * 1024ULL);
    assert(yaza_arena_v1_cycle_destroy(cycle) == 1);
    assert(yaza_arena_v1_destroy(parent) == 1);
    puts("arena-v1: Text-returning YAZA request handler reports success, application errors, concat/formatter quota failures, and cycle cleanup");
    return 0;
}

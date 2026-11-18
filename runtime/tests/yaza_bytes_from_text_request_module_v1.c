#include "yaza_runtime_v1.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int32_t yaza_export_try_bytes_from_text_request_v1(const YazaModuleInputV1 *inputs, uint64_t input_count, YazaModuleResultV1 *out_result);

static void initialize_result(YazaModuleResultV1 *result) {
    memset(result, 0, sizeof(*result));
    result->abi_version = YAZA_MODULE_RESULT_V1_ABI_VERSION;
    result->struct_size = sizeof(*result);
}

static YazaModuleInputV1 make_text_input(const unsigned char *data, uint64_t length) {
    return (YazaModuleInputV1){
        YAZA_MODULE_INPUT_V1_ABI_VERSION,
        sizeof(YazaModuleInputV1),
        YAZA_MODULE_INPUT_V1_TEXT,
        0,
        data,
        length
    };
}

int main(void) {
    yaza_rt_v1_init();
    static const unsigned char route[] = "copy";
    static const unsigned char payload[] = "hello";
    static const unsigned char empty_padding[] = "";
    const YazaModuleInputV1 success_inputs[] = {
        make_text_input(route, sizeof(route) - 1),
        make_text_input(payload, sizeof(payload) - 1),
        make_text_input(empty_padding, 0)
    };

    YazaArenaV1 *parent = yaza_arena_v1_create(128ULL * 1024ULL);
    assert(parent != NULL);
    YazaArenaCycleV1 *cycle = yaza_arena_v1_cycle_create(parent, 64ULL * 1024ULL);
    assert(cycle != NULL);

    YazaModuleResultV1 result;
    initialize_result(&result);
    assert(yaza_arena_v1_cycle_begin(cycle) == 1);
    assert(yaza_export_try_bytes_from_text_request_v1(success_inputs, 3, &result) == YAZA_MODULE_RESULT_V1_OK);
    assert(result.status == YAZA_MODULE_RESULT_V1_OK);
    assert(result.length == sizeof(payload) - 1 && memcmp(result.data, payload, sizeof(payload) - 1) == 0);
    YazaArenaCycleMetricsV1 metrics = {.struct_size = sizeof(metrics)};
    assert(yaza_arena_v1_cycle_end(cycle, &metrics) == 1);
    assert(metrics.allocation_failures == 0);

    const uint64_t padding_length = 42000;
    const uint64_t payload_length = 20000;
    unsigned char *large_padding = (unsigned char *)malloc((size_t)padding_length);
    unsigned char *large_payload = (unsigned char *)malloc((size_t)payload_length);
    assert(large_padding != NULL && large_payload != NULL);
    memset(large_padding, 'p', (size_t)padding_length);
    memset(large_payload, 'p', (size_t)payload_length);
    const YazaModuleInputV1 failure_inputs[] = {
        make_text_input(route, sizeof(route) - 1),
        make_text_input(large_payload, payload_length),
        make_text_input(large_padding, padding_length)
    };
    initialize_result(&result);
    assert(yaza_arena_v1_cycle_begin(cycle) == 1);
    assert(yaza_export_try_bytes_from_text_request_v1(failure_inputs, 3, &result) == YAZA_MODULE_RESULT_V1_OK);
    static const char expected_error[] = "Could not allocate Bytes in the active arena";
    assert(result.status == YAZA_MODULE_RESULT_V1_APPLICATION_ERROR);
    assert(result.data != NULL && result.length == sizeof(expected_error) - 1);
    assert(memcmp(result.data, expected_error, sizeof(expected_error) - 1) == 0);
    metrics = (YazaArenaCycleMetricsV1){.struct_size = sizeof(metrics)};
    assert(yaza_arena_v1_cycle_end(cycle, &metrics) == 1);
    assert(metrics.allocation_failures == 1);
    assert(metrics.peak_used_bytes < 64ULL * 1024ULL);
    assert(metrics.retained_reserved_bytes <= 64ULL * 1024ULL);

    free(large_padding);
    free(large_payload);
    assert(yaza_arena_v1_cycle_destroy(cycle) == 1);
    assert(yaza_arena_v1_destroy(parent) == 1);
    assert(yaza_rt_v1_arena_used() == 0);
    assert(yaza_rt_v1_arena_scope_count() == 0);
    puts("arena-v1: request handler recovers Text-to-Bytes quota failure and reclaims the cycle");
    return 0;
}

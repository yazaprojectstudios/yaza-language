#include "yaza_runtime_v1.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int32_t yaza_export_try_bytes_concat_request_v1(const YazaModuleInputV1 *inputs, uint64_t input_count, YazaModuleResultV1 *out_result);

static void initialize_result(YazaModuleResultV1 *result) {
    memset(result, 0, sizeof(*result));
    result->abi_version = YAZA_MODULE_RESULT_V1_ABI_VERSION;
    result->struct_size = sizeof(*result);
}

static YazaModuleInputV1 make_input(uint32_t kind, const unsigned char *data, uint64_t length) {
    return (YazaModuleInputV1){
        YAZA_MODULE_INPUT_V1_ABI_VERSION,
        sizeof(YazaModuleInputV1),
        kind,
        0,
        data,
        length
    };
}

int main(void) {
    yaza_rt_v1_init();
    static const unsigned char route[] = "concat";
    const unsigned char left[] = {0x00, 0x80, 0xff};
    const unsigned char right[] = {0x7f, 0x00};
    const YazaModuleInputV1 success_inputs[] = {
        make_input(YAZA_MODULE_INPUT_V1_TEXT, route, sizeof(route) - 1),
        make_input(YAZA_MODULE_INPUT_V1_BYTES, left, sizeof(left)),
        make_input(YAZA_MODULE_INPUT_V1_BYTES, right, sizeof(right))
    };

    YazaArenaV1 *parent = yaza_arena_v1_create(128ULL * 1024ULL);
    assert(parent != NULL);
    YazaArenaCycleV1 *cycle = yaza_arena_v1_cycle_create(parent, 64ULL * 1024ULL);
    assert(cycle != NULL);

    YazaModuleResultV1 result;
    initialize_result(&result);
    assert(yaza_arena_v1_cycle_begin(cycle) == 1);
    assert(yaza_export_try_bytes_concat_request_v1(success_inputs, 3, &result) == YAZA_MODULE_RESULT_V1_OK);
    assert(result.status == YAZA_MODULE_RESULT_V1_OK);
    assert(result.length == 5 && result.data != NULL);
    assert(memcmp(result.data, "\0\x80\xff\x7f\0", 5) == 0);
    YazaArenaCycleMetricsV1 metrics = {.struct_size = sizeof(metrics)};
    assert(yaza_arena_v1_cycle_end(cycle, &metrics) == 1);
    assert(metrics.allocation_failures == 0);
    assert(metrics.peak_used_bytes >= sizeof(YazaTextV1) + sizeof(route) - 1 + sizeof(left) + sizeof(right));

    const uint64_t large_length = 20000;
    unsigned char *large_left = (unsigned char *)malloc((size_t)large_length);
    unsigned char *large_right = (unsigned char *)malloc((size_t)large_length);
    assert(large_left != NULL && large_right != NULL);
    memset(large_left, 0x31, (size_t)large_length);
    memset(large_right, 0x32, (size_t)large_length);
    const YazaModuleInputV1 failure_inputs[] = {
        make_input(YAZA_MODULE_INPUT_V1_TEXT, route, sizeof(route) - 1),
        make_input(YAZA_MODULE_INPUT_V1_BYTES, large_left, large_length),
        make_input(YAZA_MODULE_INPUT_V1_BYTES, large_right, large_length)
    };
    initialize_result(&result);
    assert(yaza_arena_v1_cycle_begin(cycle) == 1);
    assert(yaza_export_try_bytes_concat_request_v1(failure_inputs, 3, &result) == YAZA_MODULE_RESULT_V1_OK);
    static const char expected_error[] = "Could not allocate concatenated Bytes in the active arena";
    assert(result.status == YAZA_MODULE_RESULT_V1_APPLICATION_ERROR);
    assert(result.data != NULL && result.length == sizeof(expected_error) - 1);
    assert(memcmp(result.data, expected_error, sizeof(expected_error) - 1) == 0);
    metrics = (YazaArenaCycleMetricsV1){.struct_size = sizeof(metrics)};
    assert(yaza_arena_v1_cycle_end(cycle, &metrics) == 1);
    assert(metrics.allocation_failures == 1);
    assert(metrics.peak_used_bytes < 64ULL * 1024ULL);
    assert(metrics.retained_reserved_bytes <= 64ULL * 1024ULL);

    free(large_left);
    free(large_right);
    assert(yaza_arena_v1_cycle_destroy(cycle) == 1);
    assert(yaza_arena_v1_destroy(parent) == 1);
    assert(yaza_rt_v1_arena_used() == 0);
    assert(yaza_rt_v1_arena_scope_count() == 0);
    puts("arena-v1: request handler recovers Bytes concatenation quota failure and reclaims the cycle");
    return 0;
}

#include "yaza_runtime_v1.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern int32_t yaza_export_try_list_get_result_request_v1(
    const YazaModuleInputV1 *inputs,
    uint64_t input_count,
    YazaModuleResultV1 *out_result);

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
    static const unsigned char route[] = "list-get";
    static const unsigned char empty_payload[] = "";
    static const char expected_error[] = "Could not allocate List in the active arena";
    const YazaModuleInputV1 inputs[] = {
        make_input(YAZA_MODULE_INPUT_V1_TEXT, route, sizeof(route) - 1),
        make_input(YAZA_MODULE_INPUT_V1_BYTES, empty_payload, 0)
    };

    YazaArenaV1 *parent = yaza_arena_v1_create(128ULL * 1024ULL);
    assert(parent != NULL);
    YazaArenaCycleV1 *cycle = yaza_arena_v1_cycle_create(parent, 64ULL * 1024ULL);
    assert(cycle != NULL);

    YazaModuleResultV1 result = {
        .abi_version = YAZA_MODULE_RESULT_V1_ABI_VERSION,
        .struct_size = sizeof(YazaModuleResultV1)
    };
    assert(yaza_arena_v1_cycle_begin(cycle) == 1);
    assert(yaza_export_try_list_get_result_request_v1(inputs, 2, &result) == YAZA_MODULE_RESULT_V1_OK);
    assert(result.status == YAZA_MODULE_RESULT_V1_APPLICATION_ERROR);
    assert(result.data != NULL);
    assert(result.length == sizeof(expected_error) - 1);
    assert(memcmp(result.data, expected_error, sizeof(expected_error) - 1) == 0);

    YazaArenaCycleMetricsV1 metrics = {.struct_size = sizeof(metrics)};
    assert(yaza_arena_v1_cycle_end(cycle, &metrics) == 1);
    assert(metrics.allocation_failures == 1);
    assert(metrics.peak_used_bytes > 0);
    assert(yaza_arena_v1_cycle_destroy(cycle) == 1);
    assert(yaza_arena_v1_destroy(parent) == 1);
    assert(yaza_rt_v1_arena_used() == 0);
    assert(yaza_rt_v1_arena_scope_count() == 0);
    puts("arena-v1: request handler recovers aggregate List read allocation failure and closes child scope");
    return 0;
}

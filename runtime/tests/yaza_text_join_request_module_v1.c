#include "yaza_runtime_v1.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern int32_t yaza_export_try_text_join_request_v1(
    const YazaModuleInputV1 *inputs,
    uint64_t input_count,
    YazaModuleResultV1 *out_result
);

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

static void initialize_result(YazaModuleResultV1 *result) {
    memset(result, 0, sizeof(*result));
    result->abi_version = YAZA_MODULE_RESULT_V1_ABI_VERSION;
    result->struct_size = sizeof(*result);
}

static void run_request(
    YazaArenaCycleV1 *cycle,
    const YazaModuleInputV1 *inputs,
    int32_t expected_status,
    const char *expected_data,
    uint64_t minimum_failures
) {
    YazaModuleResultV1 result;
    initialize_result(&result);
    assert(yaza_arena_v1_cycle_begin(cycle) == 1);
    assert(yaza_export_try_text_join_request_v1(inputs, 2, &result) == YAZA_MODULE_RESULT_V1_OK);
    assert(result.status == expected_status);
    assert(result.data != NULL);
    assert(result.length == strlen(expected_data));
    assert(memcmp(result.data, expected_data, (size_t)result.length) == 0);
    YazaArenaCycleMetricsV1 metrics = {.struct_size = sizeof(metrics)};
    assert(yaza_arena_v1_cycle_end(cycle, &metrics) == 1);
    assert(metrics.allocation_failures >= minimum_failures);
    assert(metrics.peak_used_bytes < 64ULL * 1024ULL);
    assert(metrics.retained_reserved_bytes <= 64ULL * 1024ULL);
}

int main(void) {
    yaza_rt_v1_init();
    static const unsigned char ok_route[] = "ok";
    static const unsigned char oom_route[] = "oom";
    static const unsigned char empty_payload[] = "";
    const YazaModuleInputV1 ok_inputs[] = {
        make_input(YAZA_MODULE_INPUT_V1_TEXT, ok_route, sizeof(ok_route) - 1),
        make_input(YAZA_MODULE_INPUT_V1_BYTES, empty_payload, 0)
    };
    const YazaModuleInputV1 oom_inputs[] = {
        make_input(YAZA_MODULE_INPUT_V1_TEXT, oom_route, sizeof(oom_route) - 1),
        make_input(YAZA_MODULE_INPUT_V1_BYTES, empty_payload, 0)
    };

    YazaArenaV1 *parent = yaza_arena_v1_create(128ULL * 1024ULL);
    assert(parent != NULL);
    YazaArenaCycleV1 *cycle = yaza_arena_v1_cycle_create(parent, 64ULL * 1024ULL);
    assert(cycle != NULL);

    run_request(cycle, ok_inputs, YAZA_MODULE_RESULT_V1_OK, "left/right", 0);
    run_request(
        cycle,
        oom_inputs,
        YAZA_MODULE_RESULT_V1_APPLICATION_ERROR,
        "Could not allocate Text in the active arena",
        1
    );

    assert(yaza_arena_v1_cycle_destroy(cycle) == 1);
    assert(yaza_arena_v1_destroy(parent) == 1);
    assert(yaza_rt_v1_arena_used() == 0);
    assert(yaza_rt_v1_arena_scope_count() == 0);
    puts("arena-v1: request handler recovers Text join quota failure and reclaims the cycle");
    return 0;
}

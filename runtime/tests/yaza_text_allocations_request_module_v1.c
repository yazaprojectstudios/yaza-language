#include "yaza_runtime_v1.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern int32_t yaza_export_try_text_allocations_request_v1(
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
    assert(yaza_export_try_text_allocations_request_v1(inputs, 2, &result) == YAZA_MODULE_RESULT_V1_OK);
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
    static const unsigned char route_data[] = " users/ignored ";
    static const unsigned char body_data[] = " ready \n now ";
    static const unsigned char bad_split_route[] = "bad-split";
    static const unsigned char oom_route[] = "oom";
    static const unsigned char oom_list_route[] = "oom-list";
    static const unsigned char oom_slice_route[] = "oom-slice";
    static const unsigned char oom_interpolate_route[] = "oom-interpolate";
    static const unsigned char empty_data[] = "";

    const YazaModuleInputV1 success_inputs[] = {
        make_input(YAZA_MODULE_INPUT_V1_TEXT, route_data, sizeof(route_data) - 1),
        make_input(YAZA_MODULE_INPUT_V1_BYTES, body_data, sizeof(body_data) - 1)
    };
    const YazaModuleInputV1 bad_split_inputs[] = {
        make_input(YAZA_MODULE_INPUT_V1_TEXT, bad_split_route, sizeof(bad_split_route) - 1),
        make_input(YAZA_MODULE_INPUT_V1_BYTES, empty_data, 0)
    };
    const YazaModuleInputV1 oom_inputs[] = {
        make_input(YAZA_MODULE_INPUT_V1_TEXT, oom_route, sizeof(oom_route) - 1),
        make_input(YAZA_MODULE_INPUT_V1_BYTES, empty_data, 0)
    };
    const YazaModuleInputV1 oom_list_inputs[] = {
        make_input(YAZA_MODULE_INPUT_V1_TEXT, oom_list_route, sizeof(oom_list_route) - 1),
        make_input(YAZA_MODULE_INPUT_V1_BYTES, empty_data, 0)
    };
    const YazaModuleInputV1 oom_slice_inputs[] = {
        make_input(YAZA_MODULE_INPUT_V1_TEXT, oom_slice_route, sizeof(oom_slice_route) - 1),
        make_input(YAZA_MODULE_INPUT_V1_BYTES, empty_data, 0)
    };
    const YazaModuleInputV1 oom_interpolate_inputs[] = {
        make_input(YAZA_MODULE_INPUT_V1_TEXT, oom_interpolate_route, sizeof(oom_interpolate_route) - 1),
        make_input(YAZA_MODULE_INPUT_V1_BYTES, empty_data, 0)
    };

    YazaModuleResultV1 result;
    initialize_result(&result);
    assert(yaza_export_try_text_allocations_request_v1(success_inputs, 2, &result) == YAZA_MODULE_RESULT_V1_NO_REQUEST_SCOPE);
    assert(result.status == YAZA_MODULE_RESULT_V1_NO_REQUEST_SCOPE);

    YazaArenaV1 *parent = yaza_arena_v1_create(128ULL * 1024ULL);
    assert(parent != NULL);
    YazaArenaCycleV1 *cycle = yaza_arena_v1_cycle_create(parent, 64ULL * 1024ULL);
    assert(cycle != NULL);

    run_request(cycle, success_inputs, YAZA_MODULE_RESULT_V1_OK, "users:ready : now", 0);
    run_request(
        cycle,
        bad_split_inputs,
        YAZA_MODULE_RESULT_V1_APPLICATION_ERROR,
        "text_split delimiter cannot be empty",
        0
    );
    run_request(
        cycle,
        oom_inputs,
        YAZA_MODULE_RESULT_V1_APPLICATION_ERROR,
        "Could not allocate List in the active arena",
        1
    );
    run_request(
        cycle,
        oom_list_inputs,
        YAZA_MODULE_RESULT_V1_APPLICATION_ERROR,
        "Could not allocate List in the active arena",
        1
    );
    run_request(
        cycle,
        oom_slice_inputs,
        YAZA_MODULE_RESULT_V1_APPLICATION_ERROR,
        "Could not allocate Text in the active arena",
        1
    );
    run_request(
        cycle,
        oom_interpolate_inputs,
        YAZA_MODULE_RESULT_V1_APPLICATION_ERROR,
        "Could not allocate Text in the active arena",
        1
    );

    assert(yaza_arena_v1_cycle_destroy(cycle) == 1);
    assert(yaza_arena_v1_destroy(parent) == 1);
    assert(yaza_rt_v1_arena_used() == 0);
    assert(yaza_rt_v1_arena_scope_count() == 0);
    puts("arena-v1: request handler recovers Text trim/split/lines/slice/interpolation quota failures and reclaims each cycle");
    return 0;
}

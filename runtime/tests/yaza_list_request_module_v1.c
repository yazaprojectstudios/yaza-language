#include "yaza_runtime_v1.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int32_t yaza_export_try_list_request_v1(const YazaModuleInputV1 *inputs, uint64_t input_count, YazaModuleResultV1 *out_result);

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

static void expect_application_error(
    YazaArenaCycleV1 *cycle,
    const YazaModuleInputV1 *inputs,
    uint64_t input_count,
    const char *message,
    uint64_t minimum_allocation_failures
) {
    YazaModuleResultV1 result;
    initialize_result(&result);
    assert(yaza_arena_v1_cycle_begin(cycle) == 1);
    assert(yaza_export_try_list_request_v1(inputs, input_count, &result) == YAZA_MODULE_RESULT_V1_OK);
    assert(result.status == YAZA_MODULE_RESULT_V1_APPLICATION_ERROR);
    assert(result.data != NULL);
    if (result.length != strlen(message) || memcmp(result.data, message, (size_t)result.length) != 0) {
        fprintf(stderr, "expected request error '%s', got '%.*s' (%llu bytes)\n",
            message, (int)result.length, (const char *)result.data,
            (unsigned long long)result.length);
        abort();
    }

    YazaArenaCycleMetricsV1 metrics = {.struct_size = sizeof(metrics)};
    assert(yaza_arena_v1_cycle_end(cycle, &metrics) == 1);
    assert(metrics.allocation_failures >= minimum_allocation_failures);
    assert(metrics.peak_used_bytes < 64ULL * 1024ULL);
    assert(metrics.retained_reserved_bytes <= 64ULL * 1024ULL);
}

int main(void) {
    yaza_rt_v1_init();
    static const unsigned char route[] = "list";
    static const unsigned char empty_payload[] = "";
    const YazaModuleInputV1 success_inputs[] = {
        make_input(YAZA_MODULE_INPUT_V1_TEXT, route, sizeof(route) - 1),
        make_input(YAZA_MODULE_INPUT_V1_BYTES, empty_payload, 0)
    };

    YazaArenaV1 *parent = yaza_arena_v1_create(128ULL * 1024ULL);
    assert(parent != NULL);
    YazaArenaCycleV1 *cycle = yaza_arena_v1_cycle_create(parent, 64ULL * 1024ULL);
    assert(cycle != NULL);

    YazaModuleResultV1 result;
    initialize_result(&result);
    assert(yaza_arena_v1_cycle_begin(cycle) == 1);
    assert(yaza_export_try_list_request_v1(success_inputs, 2, &result) == YAZA_MODULE_RESULT_V1_OK);
    assert(result.status == YAZA_MODULE_RESULT_V1_OK);
    assert(result.length == 1 && result.data != NULL && result.data[0] == '1');
    YazaArenaCycleMetricsV1 metrics = {.struct_size = sizeof(metrics)};
    assert(yaza_arena_v1_cycle_end(cycle, &metrics) == 1);
    assert(metrics.allocation_failures == 0);

    const uint64_t push_failure_length = 65380;
    unsigned char *large_payload = (unsigned char *)malloc((size_t)push_failure_length);
    assert(large_payload != NULL);
    memset(large_payload, 'x', (size_t)push_failure_length);
    static const unsigned char push_failure_route[] = "list";
    const YazaModuleInputV1 push_failure_inputs[] = {
        make_input(YAZA_MODULE_INPUT_V1_TEXT, push_failure_route, sizeof(push_failure_route) - 1),
        make_input(YAZA_MODULE_INPUT_V1_BYTES, large_payload, push_failure_length)
    };
    expect_application_error(cycle, push_failure_inputs, 2, "Could not append Number to request List", 1);

    const uint64_t create_failure_length = 65416;
    unsigned char *create_failure_payload = (unsigned char *)malloc((size_t)create_failure_length);
    assert(create_failure_payload != NULL);
    memset(create_failure_payload, 'y', (size_t)create_failure_length);
    static const unsigned char create_failure_route[] = "list";
    const YazaModuleInputV1 create_failure_inputs[] = {
        make_input(YAZA_MODULE_INPUT_V1_TEXT, create_failure_route, sizeof(create_failure_route) - 1),
        make_input(YAZA_MODULE_INPUT_V1_BYTES, create_failure_payload, create_failure_length)
    };
    expect_application_error(cycle, create_failure_inputs, 2, "Could not allocate List in the active arena", 1);

    static const unsigned char list_literal_route[] = "list-from";
    const YazaModuleInputV1 list_literal_failure_inputs[] = {
        make_input(YAZA_MODULE_INPUT_V1_TEXT, list_literal_route, sizeof(list_literal_route) - 1),
        make_input(YAZA_MODULE_INPUT_V1_BYTES, empty_payload, 0)
    };
    expect_application_error(cycle, list_literal_failure_inputs, 2, "Could not allocate List in the active arena", 1);

    static const unsigned char set_failure_route[] = "set-oom";
    const YazaModuleInputV1 set_failure_inputs[] = {
        make_input(YAZA_MODULE_INPUT_V1_TEXT, set_failure_route, sizeof(set_failure_route) - 1),
        make_input(YAZA_MODULE_INPUT_V1_BYTES, empty_payload, 0)
    };
    expect_application_error(cycle, set_failure_inputs, 2, "Could not update Text item in request List", 2);

    free(create_failure_payload);
    free(large_payload);
    assert(yaza_arena_v1_cycle_destroy(cycle) == 1);
    assert(yaza_arena_v1_destroy(parent) == 1);
    assert(yaza_rt_v1_arena_used() == 0);
    assert(yaza_rt_v1_arena_scope_count() == 0);
    puts("arena-v1: request handler recovers typed List construction/growth/update quota failures and reclaims the cycle");
    return 0;
}

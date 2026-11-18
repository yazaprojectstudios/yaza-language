#include "yaza_runtime_v1.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern int32_t yaza_export_try_secure_random_request_v1(
    const YazaModuleInputV1 *inputs,
    uint64_t input_count,
    YazaModuleResultV1 *out_result
);

static YazaModuleInputV1 text_input(const unsigned char *data, uint64_t length) {
    return (YazaModuleInputV1){
        YAZA_MODULE_INPUT_V1_ABI_VERSION,
        sizeof(YazaModuleInputV1),
        YAZA_MODULE_INPUT_V1_TEXT,
        0,
        data,
        length
    };
}

static YazaModuleInputV1 bytes_input(const unsigned char *data, uint64_t length) {
    return (YazaModuleInputV1){
        YAZA_MODULE_INPUT_V1_ABI_VERSION,
        sizeof(YazaModuleInputV1),
        YAZA_MODULE_INPUT_V1_BYTES,
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

static int32_t invoke(const unsigned char *route, uint64_t route_length,
                      const unsigned char *payload, uint64_t payload_length,
                      YazaModuleResultV1 *result) {
    const YazaModuleInputV1 inputs[] = {
        text_input(route, route_length),
        bytes_input(payload, payload_length)
    };
    return yaza_export_try_secure_random_request_v1(inputs, 2, result);
}

static void run_one_cycle(YazaArenaCycleV1 *cycle,
                          const unsigned char *route, uint64_t route_length,
                          const unsigned char *payload, uint64_t payload_length,
                          int32_t expected_status, const char *expected_message) {
    YazaModuleResultV1 result;
    initialize_result(&result);
    assert(yaza_arena_v1_cycle_begin(cycle) == 1);
    assert(invoke(route, route_length, payload, payload_length, &result) == YAZA_MODULE_RESULT_V1_OK);
    assert(result.status == expected_status);
    if (expected_status == YAZA_MODULE_RESULT_V1_OK) {
        assert(result.data != NULL);
        assert(result.length == payload_length);
        assert(result.length > 0);
    } else {
        const size_t expected_length = strlen(expected_message);
        assert(result.data != NULL);
        if (result.length != expected_length) {
            fprintf(stderr, "secure random request error length mismatch: got %llu, expected %zu, message: %.*s\n",
                    (unsigned long long)result.length, expected_length,
                    (int)result.length, result.data);
        }
        assert(result.length == expected_length);
        assert(memcmp(result.data, expected_message, expected_length) == 0);
    }
    YazaArenaCycleMetricsV1 metrics = {.struct_size = sizeof(metrics)};
    assert(yaza_arena_v1_cycle_end(cycle, &metrics) == 1);
    assert(metrics.generation > 0);
    assert(yaza_rt_v1_current_arena_used() == 0);
}

int main(void) {
    static const unsigned char invalid_route[] = "invalid";
    static const unsigned char random_route[] = "random";
    static const unsigned char input[50000] = {0};
    static const unsigned char small_input[16] = {0};

    yaza_rt_v1_init();
    YazaArenaV1 *parent = yaza_arena_v1_create(256ULL * 1024ULL);
    assert(parent != NULL);
    YazaArenaCycleV1 *cycle = yaza_arena_v1_cycle_create(parent, 64ULL * 1024ULL);
    assert(cycle != NULL);

    run_one_cycle(cycle, invalid_route, sizeof(invalid_route) - 1,
                  small_input, sizeof(small_input),
                  YAZA_MODULE_RESULT_V1_APPLICATION_ERROR,
                  "secure random length must be between 0 and 64 MiB");
    run_one_cycle(cycle, random_route, sizeof(random_route) - 1,
                  small_input, sizeof(small_input),
                  YAZA_MODULE_RESULT_V1_OK, NULL);
    run_one_cycle(cycle, random_route, sizeof(random_route) - 1,
                  input, sizeof(input),
                  YAZA_MODULE_RESULT_V1_APPLICATION_ERROR,
                  "could not allocate secure random Bytes in the active arena");

    assert(yaza_arena_v1_cycle_destroy(cycle) == 1);
    assert(yaza_arena_v1_destroy(parent) == 1);
    puts("arena-v1: secure random request handler returns success and recovers invalid-length and quota failures");
    return 0;
}

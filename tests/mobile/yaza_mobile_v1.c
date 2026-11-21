#define _POSIX_C_SOURCE 200809L
#include "yaza_mobile_v1.h"

#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CALLS_PER_WORKER 100ULL
#define WORKER_COUNT 4
#define SOAK_ENGINE_CYCLES 150U
#define SOAK_DISPATCH_ITERATIONS 2000U

typedef struct WorkerContext {
    YazaMobileEngineHandleV1 engine;
    uint64_t calls;
} WorkerContext;

static YazaMobileView view_of(const char *value) {
    return (YazaMobileView){(const uint8_t *)value, (uint64_t)strlen(value)};
}

static void expect_state(YazaMobileBufferV1 *buffer, const char *expected) {
    assert(buffer->data != NULL);
    assert(buffer->length == strlen(expected));
    assert(memcmp(buffer->data, expected, (size_t)buffer->length) == 0);
    assert(buffer->data[buffer->length] == 0);
    assert(yaza_mobile_v1_buffer_release(buffer) == YAZA_MOBILE_V1_OK);
    assert(buffer->data == NULL && buffer->lease_id == 0 && buffer->length == 0);
}

static int contains_bytes(const uint8_t *data, size_t data_length,
                          const char *needle, size_t needle_length) {
    if (data == NULL || needle == NULL || needle_length > data_length) return 0;
    for (size_t offset = 0; offset <= data_length - needle_length; ++offset) {
        if (memcmp(data + offset, needle, needle_length) == 0) return 1;
    }
    return 0;
}

static void *increment_worker(void *opaque) {
    WorkerContext *context = (WorkerContext *)opaque;
    for (uint64_t i = 0; i < context->calls; ++i) {
        YazaMobileBufferV1 state = {0};
        assert(yaza_mobile_v1_dispatch(context->engine, YAZA_MOBILE_EVENT_V1_INCREMENT, view_of("1"), &state) == YAZA_MOBILE_V1_OK);
        assert(state.length > 0);
        assert(yaza_mobile_v1_buffer_release(&state) == YAZA_MOBILE_V1_OK);
    }
    return NULL;
}

static void test_lifecycle_and_arena_soak(const YazaMobileHostV1 *host) {
    YazaMobileEngineHandleV1 previous_handle = 0;
    for (unsigned i = 0; i < SOAK_ENGINE_CYCLES; ++i) {
        YazaMobileEngineHandleV1 handle = 0;
        assert(yaza_mobile_v1_create(host, &handle) == YAZA_MOBILE_V1_OK);
        assert(handle > previous_handle);
        previous_handle = handle;

        YazaMobileBufferV1 out = {0};
        assert(yaza_mobile_v1_dispatch(handle, YAZA_MOBILE_EVENT_V1_INCREMENT, view_of("5"), &out) == YAZA_MOBILE_V1_OK);
        expect_state(&out, "5");
        assert(yaza_mobile_v1_dispatch(handle, YAZA_MOBILE_EVENT_V1_INCREMENT, view_of("bad"), &out) == YAZA_MOBILE_V1_APPLICATION_ERROR);
        assert(yaza_mobile_v1_buffer_release(&out) == YAZA_MOBILE_V1_OK);
        assert(yaza_mobile_v1_dispatch(handle, YAZA_MOBILE_EVENT_V1_SNAPSHOT, view_of(""), &out) == YAZA_MOBILE_V1_OK);
        expect_state(&out, "5");

        /* Buffer must outlive engine destruction cleanly without UAF or leak. */
        assert(yaza_mobile_v1_dispatch(handle, YAZA_MOBILE_EVENT_V1_RESET, view_of(""), &out) == YAZA_MOBILE_V1_OK);
        assert(yaza_mobile_v1_destroy(handle) == YAZA_MOBILE_V1_OK);
        expect_state(&out, "0");
    }

    YazaMobileEngineHandleV1 soak_engine = 0;
    assert(yaza_mobile_v1_create(host, &soak_engine) == YAZA_MOBILE_V1_OK);

    static uint8_t large_payload[32768];
    memset(large_payload, '9', sizeof(large_payload));
    const YazaMobileView large_view = {large_payload, (uint64_t)sizeof(large_payload)};

    for (unsigned i = 0; i < SOAK_DISPATCH_ITERATIONS; ++i) {
        YazaMobileBufferV1 out = {0};
        const unsigned mod = i % 4U;
        if (mod == 0U) {
            assert(yaza_mobile_v1_dispatch(soak_engine, YAZA_MOBILE_EVENT_V1_INCREMENT, view_of("1"), &out) == YAZA_MOBILE_V1_OK);
            assert(out.length > 0 && out.data != NULL);
            assert(yaza_mobile_v1_buffer_release(&out) == YAZA_MOBILE_V1_OK);
        } else if (mod == 1U) {
            assert(yaza_mobile_v1_dispatch(soak_engine, YAZA_MOBILE_EVENT_V1_INCREMENT, large_view, &out) == YAZA_MOBILE_V1_APPLICATION_ERROR);
            assert(yaza_mobile_v1_buffer_release(&out) == YAZA_MOBILE_V1_OK);
        } else if (mod == 2U) {
            assert(yaza_mobile_v1_dispatch(soak_engine, YAZA_MOBILE_EVENT_V1_SNAPSHOT, view_of(""), &out) == YAZA_MOBILE_V1_OK);
            assert(yaza_mobile_v1_buffer_release(&out) == YAZA_MOBILE_V1_OK);
        } else {
            assert(yaza_mobile_v1_dispatch(soak_engine, YAZA_MOBILE_EVENT_V1_RESET, view_of(""), &out) == YAZA_MOBILE_V1_OK);
            expect_state(&out, "0");
        }
    }
    assert(yaza_mobile_v1_destroy(soak_engine) == YAZA_MOBILE_V1_OK);
}

int main(void) {
    const YazaMobileHostV1 host = {sizeof(YazaMobileHostV1), YAZA_MOBILE_V1_ABI_VERSION, NULL, NULL};
    YazaMobileEngineHandleV1 engine = 0;
    assert(yaza_mobile_v1_create(&host, &engine) == YAZA_MOBILE_V1_OK);
    assert(engine > 0);

    YazaMobileEngineHandleV1 extra_engines[YAZA_MOBILE_V1_MAX_ENGINES - 1U] = {0};
    for (size_t i = 0; i < YAZA_MOBILE_V1_MAX_ENGINES - 1U; ++i) {
        assert(yaza_mobile_v1_create(&host, &extra_engines[i]) == YAZA_MOBILE_V1_OK);
        assert(extra_engines[i] > 0);
    }
    YazaMobileEngineHandleV1 over_limit_engine = 1;
    assert(yaza_mobile_v1_create(&host, &over_limit_engine) == YAZA_MOBILE_V1_RESOURCE_LIMIT);
    assert(over_limit_engine == 0);
    const YazaMobileEngineHandleV1 stale_engine = extra_engines[0];
    for (size_t i = 0; i < YAZA_MOBILE_V1_MAX_ENGINES - 1U; ++i) {
        assert(yaza_mobile_v1_destroy(extra_engines[i]) == YAZA_MOBILE_V1_OK);
    }
    YazaMobileEngineHandleV1 reused_slot_engine = 0;
    assert(yaza_mobile_v1_create(&host, &reused_slot_engine) == YAZA_MOBILE_V1_OK);
    assert(reused_slot_engine != stale_engine);
    YazaMobileBufferV1 stale_engine_result = {0};
    assert(yaza_mobile_v1_dispatch(stale_engine, YAZA_MOBILE_EVENT_V1_INCREMENT, view_of("1"), &stale_engine_result) == YAZA_MOBILE_V1_INVALID_ARGUMENT);
    assert(yaza_mobile_v1_destroy(stale_engine) == YAZA_MOBILE_V1_INVALID_ARGUMENT);
    assert(yaza_mobile_v1_destroy(reused_slot_engine) == YAZA_MOBILE_V1_OK);

    YazaMobileBufferV1 result = {0};
    assert(yaza_mobile_v1_dispatch(engine, YAZA_MOBILE_EVENT_V1_INCREMENT, view_of("2"), &result) == YAZA_MOBILE_V1_OK);
    expect_state(&result, "2");
    assert(yaza_mobile_v1_dispatch(engine, YAZA_MOBILE_EVENT_V1_INCREMENT, view_of("3"), &result) == YAZA_MOBILE_V1_OK);
    expect_state(&result, "5");
    assert(yaza_mobile_v1_dispatch(engine, YAZA_MOBILE_EVENT_V1_SNAPSHOT, view_of(""), &result) == YAZA_MOBILE_V1_OK);
    expect_state(&result, "5");

    assert(yaza_mobile_v1_dispatch(engine, YAZA_MOBILE_EVENT_V1_INCREMENT, view_of("1001"), &result) == YAZA_MOBILE_V1_APPLICATION_ERROR);
    assert(result.data != NULL && result.length > 0);
    assert(contains_bytes(result.data, (size_t)result.length, "between 1 and 1000", 18));
    assert(yaza_mobile_v1_buffer_release(&result) == YAZA_MOBILE_V1_OK);

    assert(yaza_mobile_v1_dispatch(engine, YAZA_MOBILE_EVENT_V1_INCREMENT, view_of("1"), &result) == YAZA_MOBILE_V1_OK);
    expect_state(&result, "6");
    assert(yaza_mobile_v1_dispatch(engine, YAZA_MOBILE_EVENT_V1_RESET, view_of(""), &result) == YAZA_MOBILE_V1_OK);
    expect_state(&result, "0");
    assert(yaza_mobile_v1_dispatch(engine, YAZA_MOBILE_EVENT_V1_SNAPSHOT, view_of(""), &result) == YAZA_MOBILE_V1_OK);
    expect_state(&result, "0");

    assert(yaza_mobile_v1_dispatch(engine, YAZA_MOBILE_EVENT_V1_INCREMENT, view_of("1"), &result) == YAZA_MOBILE_V1_OK);
    YazaMobileBufferV1 stale_buffer = result;
    assert(yaza_mobile_v1_buffer_release(&result) == YAZA_MOBILE_V1_OK);
    assert(yaza_mobile_v1_dispatch(engine, YAZA_MOBILE_EVENT_V1_INCREMENT, view_of("1"), &result) == YAZA_MOBILE_V1_OK);
    assert(stale_buffer.lease_id != result.lease_id);
    assert(yaza_mobile_v1_buffer_release(&stale_buffer) == YAZA_MOBILE_V1_INVALID_ARGUMENT);
    assert(result.length == 1 && result.data[0] == '2');
    expect_state(&result, "2");
    assert(yaza_mobile_v1_dispatch(engine, 99, view_of(""), &result) == YAZA_MOBILE_V1_INVALID_ARGUMENT);
    assert(yaza_mobile_v1_dispatch(engine, YAZA_MOBILE_EVENT_V1_INCREMENT, (YazaMobileView){NULL, YAZA_MOBILE_V1_MAX_EVENT_BYTES + 1}, &result) == YAZA_MOBILE_V1_INVALID_ARGUMENT);
    const uint8_t oversized_payload = 0;
    assert(yaza_mobile_v1_dispatch(engine, YAZA_MOBILE_EVENT_V1_INCREMENT, (YazaMobileView){&oversized_payload, YAZA_MOBILE_V1_MAX_EVENT_BYTES + 1}, &result) == YAZA_MOBILE_V1_RESOURCE_LIMIT);
    const uint8_t malformed_utf8[] = {0xc0U, 0x80U};
    assert(yaza_mobile_v1_dispatch(engine, YAZA_MOBILE_EVENT_V1_INCREMENT, (YazaMobileView){malformed_utf8, sizeof(malformed_utf8)}, &result) == YAZA_MOBILE_V1_INVALID_ARGUMENT);
    const uint8_t surrogate_utf8[] = {0xedU, 0xa0U, 0x80U};
    assert(yaza_mobile_v1_dispatch(engine, YAZA_MOBILE_EVENT_V1_INCREMENT, (YazaMobileView){surrogate_utf8, sizeof(surrogate_utf8)}, &result) == YAZA_MOBILE_V1_INVALID_ARGUMENT);
    assert(yaza_mobile_v1_dispatch(-1, YAZA_MOBILE_EVENT_V1_INCREMENT, view_of("1"), &result) == YAZA_MOBILE_V1_INVALID_ARGUMENT);

    YazaMobileBufferV1 forged = {(const uint8_t *)(uintptr_t)2, 1, 1};
    assert(yaza_mobile_v1_buffer_release(&forged) == YAZA_MOBILE_V1_INVALID_ARGUMENT);
    YazaMobileBufferV1 empty_buf = {0};
    assert(yaza_mobile_v1_buffer_release(&empty_buf) == YAZA_MOBILE_V1_OK);
    assert(yaza_mobile_v1_buffer_release(&empty_buf) == YAZA_MOBILE_V1_OK);

    pthread_t threads[WORKER_COUNT];
    WorkerContext worker = {engine, CALLS_PER_WORKER};
    for (size_t i = 0; i < WORKER_COUNT; ++i) assert(pthread_create(&threads[i], NULL, increment_worker, &worker) == 0);
    for (size_t i = 0; i < WORKER_COUNT; ++i) assert(pthread_join(threads[i], NULL) == 0);
    assert(yaza_mobile_v1_dispatch(engine, YAZA_MOBILE_EVENT_V1_INCREMENT, view_of("1"), &result) == YAZA_MOBILE_V1_OK);
    expect_state(&result, "403");

    assert(yaza_mobile_v1_destroy(engine) == YAZA_MOBILE_V1_OK);
    test_lifecycle_and_arena_soak(&host);

    {
        char persist_dir[256];
        snprintf(persist_dir, sizeof(persist_dir), "/tmp/yaza-mob-persist-%lu", (unsigned long)getpid());
        (void)mkdir(persist_dir, 0700);
        assert(setenv("YAZA_MOBILE_STORAGE_DIR", persist_dir, 1) == 0);

        YazaMobileEngineHandleV1 e1 = 0;
        YazaMobileBufferV1 pbuf = {0};
        assert(yaza_mobile_v1_create(&host, &e1) == YAZA_MOBILE_V1_OK);
        assert(yaza_mobile_v1_dispatch(e1, YAZA_MOBILE_EVENT_V1_INCREMENT, view_of("15"), &pbuf) == YAZA_MOBILE_V1_OK);
        expect_state(&pbuf, "15");
        assert(yaza_mobile_v1_destroy(e1) == YAZA_MOBILE_V1_OK);

        YazaMobileEngineHandleV1 e2 = 0;
        assert(yaza_mobile_v1_create(&host, &e2) == YAZA_MOBILE_V1_OK);
        assert(yaza_mobile_v1_dispatch(e2, YAZA_MOBILE_EVENT_V1_SNAPSHOT, view_of(""), &pbuf) == YAZA_MOBILE_V1_OK);
        expect_state(&pbuf, "15");
        assert(yaza_mobile_v1_dispatch(e2, YAZA_MOBILE_EVENT_V1_RESET, view_of(""), &pbuf) == YAZA_MOBILE_V1_OK);
        expect_state(&pbuf, "0");
        assert(yaza_mobile_v1_destroy(e2) == YAZA_MOBILE_V1_OK);

        assert(unsetenv("YAZA_MOBILE_STORAGE_DIR") == 0);
        char f1[300], f2[300];
        snprintf(f1, sizeof(f1), "%s/mobile_state.ydb", persist_dir);
        snprintf(f2, sizeof(f2), "%s/mobile_state.ydb.lock", persist_dir);
        (void)remove(f1);
        (void)remove(f2);
        (void)rmdir(persist_dir);
    }

    puts("mobile bridge v1: event/state round-trip, bounds, non-reused handle/lease IDs, engine cap, error rollback, buffer ownership, serialized concurrency, embedded persistence, and lifecycle/arena soak passed");
    return 0;
}

#include "yaza_runtime_v1.h"

#include <assert.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#define NUMERIC_TEXT_WORKERS 8
#define NUMERIC_TEXT_ITERATIONS 5000

typedef struct NumericTextWorker {
    int64_t worker_id;
} NumericTextWorker;

static int text_equals(const YazaTextV1 *text, const char *expected) {
    const size_t expected_length = strlen(expected);
    return text != NULL && text->length == expected_length &&
        (expected_length == 0 || memcmp(text->data, expected, expected_length) == 0);
}

static void *numeric_text_worker(void *argument) {
    const NumericTextWorker *worker = (const NumericTextWorker *)argument;
    for (int64_t index = 0; index < NUMERIC_TEXT_ITERATIONS; index += 1) {
        const int64_t number = worker->worker_id * INT64_C(1000000) + index;
        char expected_number[32];
        assert(snprintf(expected_number, sizeof(expected_number), "%" PRId64, number) > 0);
        assert(text_equals(yaza_rt_v1_number_text(number), expected_number));

        const int32_t int32_value = (int32_t)(index - NUMERIC_TEXT_ITERATIONS / 2);
        char expected_int32[16];
        assert(snprintf(expected_int32, sizeof(expected_int32), "%" PRId32, int32_value) > 0);
        assert(text_equals(yaza_rt_v1_int32_text(int32_value), expected_int32));
    }
    return NULL;
}

int main(void) {
    yaza_rt_v1_init();
    const uint64_t used_before = yaza_rt_v1_arena_used();

    assert(text_equals(yaza_rt_v1_number_text(INT64_MIN), "-9223372036854775808"));
    assert(text_equals(yaza_rt_v1_number_text(INT64_MAX), "9223372036854775807"));
    assert(text_equals(yaza_rt_v1_number_text(0), "0"));
    assert(text_equals(yaza_rt_v1_int32_text(INT32_MIN), "-2147483648"));
    assert(text_equals(yaza_rt_v1_int32_text(INT32_MAX), "2147483647"));
    assert(text_equals(yaza_rt_v1_try_number_text(INT64_MIN), "-9223372036854775808"));
    assert(text_equals(yaza_rt_v1_try_int32_text(INT32_MAX), "2147483647"));
    assert(text_equals(yaza_rt_v1_try_float64_text(1.25), "1.25"));
    assert(yaza_rt_v1_arena_used() > used_before);

    pthread_t threads[NUMERIC_TEXT_WORKERS];
    NumericTextWorker workers[NUMERIC_TEXT_WORKERS];
    for (int64_t index = 0; index < NUMERIC_TEXT_WORKERS; index += 1) {
        workers[index].worker_id = index;
        assert(pthread_create(&threads[index], NULL, numeric_text_worker, &workers[index]) == 0);
    }
    for (size_t index = 0; index < NUMERIC_TEXT_WORKERS; index += 1) {
        assert(pthread_join(threads[index], NULL) == 0);
    }

    puts("runtime-v1: portable fallible Number/Int32/Float64 formatting, arena ownership, and concurrent use passed");
    return 0;
}

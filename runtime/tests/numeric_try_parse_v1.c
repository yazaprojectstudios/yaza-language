#include "yaza_runtime_v1.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEXT_VALUE(name, literal) \
    static const uint8_t name##_bytes[] = literal; \
    const YazaTextV1 name = {name##_bytes, sizeof(name##_bytes) - 1ULL}

int main(void) {
    TEXT_VALUE(number_min, "-9223372036854775808");
    TEXT_VALUE(number_max, "  +9223372036854775807");
    TEXT_VALUE(number_overflow, "9223372036854775808");
    TEXT_VALUE(number_underflow, "-9223372036854775809");
    TEXT_VALUE(number_trailing, "42 ");
    TEXT_VALUE(number_empty, "");
    TEXT_VALUE(int32_min, "-2147483648");
    TEXT_VALUE(int32_overflow, "2147483648");
    TEXT_VALUE(float_ok, "+1.25e2");
    TEXT_VALUE(float_nan, "nan");
    TEXT_VALUE(float_overflow, "1e9999");
    TEXT_VALUE(float_bad_grammar, "1.");

    int64_t number = 7;
    assert(yaza_rt_v1_try_number_parse(&number_min, &number) == 1 && number == INT64_MIN);
    assert(yaza_rt_v1_try_number_parse(&number_max, &number) == 1 && number == INT64_MAX);
    number = 99;
    assert(yaza_rt_v1_try_number_parse(&number_overflow, &number) == 0 && number == 99);
    assert(yaza_rt_v1_try_number_parse(&number_underflow, &number) == 0 && number == 99);
    assert(yaza_rt_v1_try_number_parse(&number_trailing, &number) == 0 && number == 99);
    assert(yaza_rt_v1_try_number_parse(&number_empty, &number) == 0 && number == 99);

    int32_t small = 3;
    assert(yaza_rt_v1_try_int32_parse(&int32_min, &small) == 1 && small == INT32_MIN);
    small = 99;
    assert(yaza_rt_v1_try_int32_parse(&int32_overflow, &small) == 0 && small == 99);

    double floating = 3.0;
    assert(yaza_rt_v1_try_float64_parse(&float_ok, &floating) == 1 && floating == 125.0);
    floating = 3.0;
    assert(yaza_rt_v1_try_float64_parse(&float_nan, &floating) == 0 && floating == 3.0);
    assert(yaza_rt_v1_try_float64_parse(&float_overflow, &floating) == 0 && floating == 3.0);
    assert(yaza_rt_v1_try_float64_parse(&float_bad_grammar, &floating) == 0 && floating == 3.0);

    puts("numeric-try-parse-v1-ok");
    return 0;
}

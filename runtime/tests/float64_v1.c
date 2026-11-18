#include "yaza_runtime_v1.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static double parse_float64(const char *value) {
    YazaTextV1 text = {(const unsigned char *)value, (uint64_t)strlen(value)};
    return yaza_rt_v1_float64_parse(&text);
}

int main(void) {
    yaza_rt_v1_init();
    assert(parse_float64("42") == 42.0);
    assert(parse_float64("+1.5") == 1.5);
    assert(parse_float64("1.25e2") == 125.0);
    assert(parse_float64("5E-3") == 0.005);
    assert(parse_float64("2.5e+1") == 25.0);
    assert(parse_float64("-0.0") == 0.0);
    assert(signbit(parse_float64("-0.0")) != 0);
    assert(isfinite(parse_float64("2.2250738585072014e-308")));
    puts("runtime-v1: Float64 grammar, decimal/scientific parsing, and finite range passed");
    return 0;
}

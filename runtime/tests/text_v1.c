#include "yaza_runtime_v1.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static YazaTextV1 view(const unsigned char *data, uint64_t length) {
    return (YazaTextV1){data, length};
}

static int equals(const YazaTextV1 *text, const unsigned char *bytes, uint64_t length) {
    return text != NULL && text->length == length &&
        (length == 0 || memcmp(text->data, bytes, (size_t)length) == 0) && text->data[length] == '\0';
}

int main(void) {
    yaza_rt_v1_init();
    YazaArenaV1 *arena = yaza_arena_v1_create(1024ULL * 1024ULL);
    assert(arena != NULL);
    YazaArenaContextV1 context = {0};
    assert(yaza_arena_v1_scope_begin(arena, &context) == 1);

    const unsigned char embedded_nul[] = {'a', 0, 'b', 0};
    YazaTextV1 binary_text = view(embedded_nul, 3);
    assert(yaza_rt_v1_text_length(&binary_text) == 3);
    assert(yaza_rt_v1_text_byte(&binary_text, 0) == 'a');
    assert(yaza_rt_v1_text_byte(&binary_text, 1) == 0);
    assert(yaza_rt_v1_text_byte(&binary_text, 2) == 'b');

    const unsigned char nul_b[] = {0, 'b', 0};
    YazaTextV1 *slice = yaza_rt_v1_text_slice(&binary_text, 1, 2);
    assert(equals(slice, nul_b, 2));
    YazaTextV1 *try_slice = NULL;
    assert(yaza_rt_v1_try_text_slice(&binary_text, 1, 2, &try_slice) == 1);
    assert(equals(try_slice, nul_b, 2));
    try_slice = (YazaTextV1 *)1;
    assert(yaza_rt_v1_try_text_slice(&binary_text, 2, 2, &try_slice) == -1);
    assert(try_slice == NULL);
    YazaTextV1 needle_b = view((const unsigned char *)"b", 1);
    assert(yaza_rt_v1_text_find(&binary_text, &needle_b) == 2);
    YazaTextV1 prefix_a = view((const unsigned char *)"a", 1);
    assert(yaza_rt_v1_text_starts_with(&binary_text, &prefix_a));
    assert(yaza_rt_v1_text_ends_with(&binary_text, &needle_b));

    YazaTextV1 equal_copy = view(embedded_nul, 3);
    assert(yaza_rt_v1_text_compare(&binary_text, &equal_copy) == 0);
    assert(yaza_rt_v1_text_compare(&binary_text, &prefix_a) > 0);
    YazaTextV1 suffix_z = view((const unsigned char *)"z", 1);
    YazaTextV1 *joined = yaza_rt_v1_text_concat(&binary_text, &suffix_z);
    const unsigned char expected_joined[] = {'a', 0, 'b', 'z', 0};
    assert(equals(joined, expected_joined, 4));

    YazaTextV1 repeat_source = view((const unsigned char *)"ab", 2);
    YazaTextV1 *repeated = NULL;
    assert(yaza_rt_v1_try_text_repeat(&repeat_source, 3, &repeated) == 1);
    assert(equals(repeated, (const unsigned char *)"ababab", 6));
    assert(yaza_rt_v1_try_text_repeat(&repeat_source, 0, &repeated) == 1);
    assert(equals(repeated, (const unsigned char *)"", 0));
    YazaTextV1 empty_text = view((const unsigned char *)"", 0);
    assert(yaza_rt_v1_try_text_repeat(&empty_text, INT64_MAX, &repeated) == 1);
    assert(equals(repeated, (const unsigned char *)"", 0));
    repeated = (YazaTextV1 *)1;
    assert(yaza_rt_v1_try_text_repeat(&repeat_source, -1, &repeated) == -1);
    assert(repeated == NULL);
    const YazaTextV1 *repeat_count_error = yaza_rt_v1_text_repeat_count_error();
    assert(equals(repeat_count_error, (const unsigned char *)"text_try_repeat count must be non-negative", 42));
    assert(yaza_rt_v1_try_text_repeat(&repeat_source, INT64_MAX, &repeated) == -2);
    assert(repeated == NULL);
    const YazaTextV1 *repeat_size_error = yaza_rt_v1_text_repeat_size_error();
    assert(equals(repeat_size_error, (const unsigned char *)"text_try_repeat output size is too large", 40));

    YazaTextV1 trim_source = view((const unsigned char *)" \tYAZA \n", 8);
    YazaTextV1 *try_trimmed = yaza_rt_v1_try_text_trim(&trim_source);
    assert(equals(try_trimmed, (const unsigned char *)"YAZA", 4));

    YazaTextV1 raw_source = view((const unsigned char *)"a\0b", 3);
    void *parts = yaza_text_v1_split(&raw_source, &needle_b);
    const YazaTextV1 *part = NULL;
    assert(yaza_list_v1_count_typed(parts, "TextList", sizeof(part)) == 2);
    assert(yaza_list_v1_get_typed(parts, 0, "TextList", sizeof(part), &part) == 0);
    assert(equals(part, embedded_nul, 2));
    assert(yaza_list_v1_get_typed(parts, 1, "TextList", sizeof(part), &part) == 0);
    assert(equals(part, (const unsigned char *)"", 0));
    void *try_parts = (void *)1;
    assert(yaza_rt_v1_try_text_split(&raw_source, &needle_b, &try_parts) == 1);
    assert(try_parts != NULL && yaza_list_v1_count_typed(try_parts, "TextList", sizeof(part)) == 2);
    YazaTextV1 empty_delimiter = view((const unsigned char *)"", 0);
    try_parts = (void *)1;
    assert(yaza_rt_v1_try_text_split(&raw_source, &empty_delimiter, &try_parts) == -1);
    assert(try_parts == NULL);
    const YazaTextV1 *split_error = yaza_rt_v1_text_split_delimiter_error();
    assert(equals(split_error, (const unsigned char *)"text_split delimiter cannot be empty", 36));
    YazaTextV1 separator = view((const unsigned char *)":", 1);
    YazaTextV1 *rejoined = yaza_rt_v1_text_join(parts, &separator);
    const unsigned char expected_rejoined[] = {'a', 0, ':', 0};
    assert(equals(rejoined, expected_rejoined, 3));
    const uint64_t allocations_before_try_join = yaza_rt_v1_arena_allocation_count();
    YazaTextV1 *try_rejoined = yaza_rt_v1_try_text_join(parts, &separator);
    assert(equals(try_rejoined, expected_rejoined, 3));
    assert(yaza_rt_v1_arena_allocation_count() == allocations_before_try_join + 1);

    YazaTextV1 lines_source = view((const unsigned char *)"one\ntwo\n", 8);
    void *lines = yaza_rt_v1_text_lines(&lines_source);
    assert(yaza_list_v1_count_typed(lines, "TextList", sizeof(part)) == 2);
    assert(yaza_list_v1_get_typed(lines, 1, "TextList", sizeof(part), &part) == 0);
    assert(equals(part, (const unsigned char *)"two", 3));
    void *try_lines = (void *)1;
    assert(yaza_rt_v1_try_text_lines(&lines_source, &try_lines) == 1);
    assert(try_lines != NULL && yaza_list_v1_count_typed(try_lines, "TextList", sizeof(part)) == 2);

    YazaBytesV1 bytes = {NULL, 0};
    yaza_bytes_v1_from_text(&binary_text, &bytes);
    assert(bytes.length == 3 && memcmp(bytes.data, embedded_nul, 3) == 0);
    YazaTextV1 *checked = (YazaTextV1 *)1;
    assert(yaza_bytes_v1_to_text_checked(bytes.data, bytes.length, &checked) == 0);
    assert(checked == NULL);

    YazaTextV1 number = view((const unsigned char *)"-9223372036854775808", 20);
    assert(yaza_rt_v1_number_parse(&number) == INT64_MIN);
    YazaTextV1 float_number = view((const unsigned char *)"1.25e2", 6);
    assert(yaza_rt_v1_float64_parse(&float_number) == 125.0);
    assert(equals(yaza_rt_v1_number_text(INT64_MAX), (const unsigned char *)"9223372036854775807", 19));

    assert(yaza_arena_v1_scope_end(&context) == 1);
    assert(yaza_arena_v1_used(arena) == 0);
    assert(yaza_arena_v1_destroy(arena) == 1);
    puts("runtime-v1: explicit Text lengths, embedded bytes, fallible/fail-fast slicing, search, ordering, lists, joins, repetition, and numeric conversion passed");
    return 0;
}

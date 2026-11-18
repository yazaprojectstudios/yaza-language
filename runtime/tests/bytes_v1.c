#include "yaza_runtime_v1.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static YazaTextV1 text_view(const char *value) {
    return (YazaTextV1){(const unsigned char *)value, (uint64_t)strlen(value)};
}

static uint32_t reference_nibble_checksum(const unsigned char *data, uint64_t length,
                                         uint64_t zero_offset, uint64_t zero_length,
                                         const uint32_t nibble_table[16]) {
    uint32_t crc = UINT32_MAX;
    for (uint64_t index = 0; index < length; ++index) {
        unsigned char byte = data[index];
        if (index >= zero_offset && index - zero_offset < zero_length) byte = 0;
        crc ^= byte;
        crc = (crc >> 4) ^ nibble_table[crc & 0x0fU];
        crc = (crc >> 4) ^ nibble_table[crc & 0x0fU];
    }
    return crc ^ UINT32_MAX;
}

static void check_utf8_case(const unsigned char *bytes, uint64_t length, int should_be_text) {
    YazaTextV1 *text = (YazaTextV1 *)1;
    const int32_t accepted = yaza_bytes_v1_to_text_checked(bytes, length, &text);
    assert(accepted == should_be_text);
    if (should_be_text) {
        assert(text != NULL);
        assert(text->length == length);
        if (length > 0) {
            assert(memcmp(text->data, bytes, (size_t)length) == 0);
        }
    } else {
        assert(text == NULL);
    }
}

int main(void) {
    yaza_rt_v1_init();
    YazaArenaV1 *arena = yaza_arena_v1_create(256ULL * 1024ULL);
    assert(arena != NULL);
    YazaArenaContextV1 context = {0};
    assert(yaza_arena_v1_scope_begin(arena, &context) == 1);

    YazaBytesV1 word = {NULL, 0};
    YazaTextV1 hello = text_view("hello");
    yaza_bytes_v1_from_text(&hello, &word);
    assert(word.length == 5 && memcmp(word.data, "hello", 5) == 0);
    YazaBytesV1 try_text = {NULL, 0};
    char *try_text_error = NULL;
    assert(yaza_bytes_v1_try_from_text(&hello, &try_text, &try_text_error) == 1);
    assert(try_text_error == NULL && try_text.length == 5 && memcmp(try_text.data, "hello", 5) == 0);
    const unsigned char embedded_nul_text_data[] = {'A', 0, 'B'};
    const YazaTextV1 embedded_nul_text = {embedded_nul_text_data, sizeof(embedded_nul_text_data)};
    assert(yaza_bytes_v1_try_from_text(&embedded_nul_text, &try_text, &try_text_error) == 1);
    assert(try_text_error == NULL && try_text.length == 3 && memcmp(try_text.data, embedded_nul_text_data, 3) == 0);
    const YazaTextV1 empty_text = {NULL, 0};
    assert(yaza_bytes_v1_try_from_text(&empty_text, &try_text, &try_text_error) == 1);
    assert(try_text_error == NULL && try_text.data == NULL && try_text.length == 0);
    const YazaTextV1 invalid_text = {NULL, 1};
    assert(yaza_bytes_v1_try_from_text(&invalid_text, &try_text, &try_text_error) == 0);
    assert(try_text.data == NULL && try_text.length == 0);
    assert(try_text_error != NULL && strcmp(try_text_error, "Text descriptor is invalid") == 0);
    const YazaTextV1 oversized_text = {(const unsigned char *)"x", 64ULL * 1024ULL * 1024ULL + 1};
    assert(yaza_bytes_v1_try_from_text(&oversized_text, &try_text, &try_text_error) == 0);
    assert(try_text.data == NULL && try_text.length == 0);
    assert(try_text_error != NULL && strcmp(try_text_error, "Text exceeds the 64 MiB Bytes limit") == 0);
    const unsigned char crc_input[] = {'a', 'b', 'c', 'd', 'e', 'f'};
    const unsigned char crc_zeroed[] = {'a', 0, 0, 'd', 'e', 'f'};
    const unsigned char standard_crc_vector[] = "123456789";
    assert(yaza_bytes_v1_crc32c(standard_crc_vector, 9) == 0xe3069283U);
    assert(yaza_bytes_v1_ydbv1_checksum(standard_crc_vector, 9) == 0x8c327e89U);
    assert(yaza_bytes_v1_crc32c_zeroed_range(crc_input, sizeof(crc_input), 1, 2) ==
           yaza_bytes_v1_crc32c(crc_zeroed, sizeof(crc_zeroed)));
    assert(yaza_bytes_v1_crc32c_zeroed_range(crc_input, sizeof(crc_input), 2, 0) ==
           yaza_bytes_v1_crc32c(crc_input, sizeof(crc_input)));
    assert(yaza_bytes_v1_ydbv1_checksum_zeroed_range(crc_input, sizeof(crc_input), 1, 2) ==
           yaza_bytes_v1_ydbv1_checksum(crc_zeroed, sizeof(crc_zeroed)));
    static const uint32_t legacy_ydbv1_nibble_table[16] = {
        0x00000000U, 0x105ec76fU, 0x20bd8edeU, 0x30e34991U,
        0x417b1dbcU, 0x5125dad3U, 0x61c69302U, 0x7198546dU,
        0x82f63b78U, 0x92a8fc17U, 0xa24bb596U, 0xb21572f9U,
        0xc38d26c4U, 0xd3d3e1abU, 0xe330a81aU, 0xf36e6f75U
    };
    static const uint32_t crc32c_nibble_table[16] = {
        0x00000000U, 0x105ec76fU, 0x20bd8edeU, 0x30e349b1U,
        0x417b1dbcU, 0x5125dad3U, 0x61c69362U, 0x7198540dU,
        0x82f63b78U, 0x92a8fc17U, 0xa24bb5a6U, 0xb21572c9U,
        0xc38d26c4U, 0xd3d3e1abU, 0xe330a81aU, 0xf36e6f75U
    };
    unsigned char checksum_page[16384];
    for (uint64_t index = 0; index < sizeof(checksum_page); ++index) {
        checksum_page[index] = (unsigned char)(index * 17U + index / 13U + 29U);
    }
    const uint64_t checksum_lengths[] = {0, 1, 7, 255, sizeof(checksum_page)};
    for (size_t index = 0; index < sizeof(checksum_lengths) / sizeof(checksum_lengths[0]); ++index) {
        const uint64_t length = checksum_lengths[index];
        const uint64_t zero_offset = length / 3U;
        const uint64_t zero_length = length == 0 ? 0 : length / 5U;
        assert(yaza_bytes_v1_ydbv1_checksum(checksum_page, length) ==
               reference_nibble_checksum(checksum_page, length, 0, 0, legacy_ydbv1_nibble_table));
        assert(yaza_bytes_v1_ydbv1_checksum_zeroed_range(checksum_page, length, (int64_t)zero_offset, (int64_t)zero_length) ==
               reference_nibble_checksum(checksum_page, length, zero_offset, zero_length, legacy_ydbv1_nibble_table));
        assert(yaza_bytes_v1_crc32c(checksum_page, length) ==
               reference_nibble_checksum(checksum_page, length, 0, 0, crc32c_nibble_table));
        assert(yaza_bytes_v1_crc32c_zeroed_range(checksum_page, length, (int64_t)zero_offset, (int64_t)zero_length) ==
               reference_nibble_checksum(checksum_page, length, zero_offset, zero_length, crc32c_nibble_table));
    }
    YazaBytesV1 zeroed = {NULL, 0};
    char *zeroed_error = NULL;
    const uint64_t used_before_zeroed = yaza_arena_v1_used(arena);
    assert(yaza_bytes_v1_try_new_zeroed(8, &zeroed, &zeroed_error) == 1);
    assert(zeroed_error == NULL && zeroed.length == 8 && zeroed.data != NULL);
    for (uint64_t index = 0; index < zeroed.length; ++index) assert(zeroed.data[index] == 0);
    assert(yaza_arena_v1_used(arena) == used_before_zeroed + 8);
    assert(yaza_bytes_v1_try_new_zeroed(0, &zeroed, &zeroed_error) == 1);
    assert(zeroed_error == NULL && zeroed.data == NULL && zeroed.length == 0);
    assert(yaza_bytes_v1_try_new_zeroed(-1, &zeroed, &zeroed_error) == 0);
    assert(zeroed.data == NULL && zeroed.length == 0);
    assert(zeroed_error != NULL && strcmp(zeroed_error, "Bytes length must be between 0 and 64 MiB") == 0);
    assert(yaza_bytes_v1_try_new_zeroed(64LL * 1024LL * 1024LL + 1, &zeroed, &zeroed_error) == 0);
    assert(zeroed.data == NULL && zeroed.length == 0);
    assert(zeroed_error != NULL && strcmp(zeroed_error, "Bytes length must be between 0 and 64 MiB") == 0);
    YazaBytesV1 zero = {NULL, 0};
    yaza_bytes_v1_from_number(0, &zero);
    YazaBytesV1 try_number = {NULL, 0};
    char *try_number_error = NULL;
    assert(yaza_bytes_v1_try_from_number(255, &try_number, &try_number_error) == 1);
    assert(try_number_error == NULL && try_number.length == 1 && try_number.data[0] == 0xff);
    assert(yaza_bytes_v1_try_from_number(256, &try_number, &try_number_error) == 0);
    assert(try_number.data == NULL && try_number.length == 0);
    assert(try_number_error != NULL && strcmp(try_number_error, "Bytes value must be between 0 and 255") == 0);
    YazaBytesV1 random_empty = {NULL, 0};
    const uint64_t used_before_empty_random = yaza_arena_v1_used(arena);
    yaza_bytes_v1_secure_random(0, &random_empty);
    assert(random_empty.data == NULL && random_empty.length == 0);
    assert(yaza_arena_v1_used(arena) == used_before_empty_random);
    YazaBytesV1 random_bytes = {NULL, 0};
    const uint64_t used_before_random = yaza_arena_v1_used(arena);
    yaza_bytes_v1_secure_random(32, &random_bytes);
    assert(random_bytes.data != NULL && random_bytes.length == 32);
    assert(yaza_arena_v1_used(arena) == used_before_random + 32);
    YazaBytesV1 framed = {NULL, 0};
    yaza_bytes_v1_concat(word.data, word.length, zero.data, zero.length, &framed);
    assert(framed.length == 6 && framed.data[5] == 0);
    assert(yaza_bytes_v1_equal(word.data, word.length, framed.data, framed.length) == 0);
    assert(yaza_bytes_v1_equal_constant_time(word.data, word.length, framed.data, framed.length) == 0);

    YazaBytesV1 sliced = {NULL, 0};
    yaza_bytes_v1_slice(framed.data, framed.length, 0, 5, &sliced);
    assert(yaza_bytes_v1_equal(word.data, word.length, sliced.data, sliced.length) == 1);
    YazaBytesV1 try_sliced = {NULL, 0};
    char *try_slice_error = NULL;
    assert(yaza_bytes_v1_try_slice(framed.data, framed.length, 1, 3, &try_sliced, &try_slice_error) == 1);
    assert(try_slice_error == NULL && try_sliced.length == 3 && memcmp(try_sliced.data, "ell", 3) == 0);
    assert(yaza_bytes_v1_try_slice(framed.data, framed.length, 4, 4, &try_sliced, &try_slice_error) == 0);
    assert(try_sliced.data == NULL && try_sliced.length == 0);
    assert(try_slice_error != NULL && strcmp(try_slice_error, "Bytes slice is out of range") == 0);
    YazaTextV1 *decoded = NULL;
    assert(yaza_bytes_v1_to_text_checked(sliced.data, sliced.length, &decoded) == 1);
    assert(decoded != NULL && decoded->length == 5 && memcmp(decoded->data, "hello", 5) == 0);
    assert(yaza_bytes_v1_to_text_checked(framed.data, framed.length, &decoded) == 0);
    assert(decoded == NULL);
    char *try_text_conversion_error = NULL;
    assert(yaza_bytes_v1_try_to_text(sliced.data, sliced.length, &decoded, &try_text_conversion_error) == 1);
    assert(try_text_conversion_error == NULL && decoded != NULL && decoded->length == 5);
    assert(memcmp(decoded->data, "hello", 5) == 0 && decoded->data[5] == '\0');
    assert(yaza_bytes_v1_try_to_text(framed.data, framed.length, &decoded, &try_text_conversion_error) == 0);
    assert(decoded == NULL && try_text_conversion_error != NULL);
    assert(strcmp(try_text_conversion_error, "Bytes value is not valid non-NUL UTF-8 Text") == 0);
    assert(yaza_bytes_v1_try_to_text(NULL, 0, &decoded, &try_text_conversion_error) == 1);
    assert(try_text_conversion_error == NULL && decoded != NULL && decoded->length == 0 && decoded->data[0] == '\0');

    const unsigned char overlong[] = {0xc0, 0xaf};
    assert(yaza_bytes_v1_to_text_checked(overlong, sizeof(overlong), &decoded) == 0);
    assert(decoded == NULL);
    assert(yaza_bytes_v1_try_to_text(overlong, sizeof(overlong), &decoded, &try_text_conversion_error) == 0);
    assert(decoded == NULL && try_text_conversion_error != NULL);
    const unsigned char surrogate[] = {0xed, 0xa0, 0x80};
    assert(yaza_bytes_v1_to_text_checked(surrogate, sizeof(surrogate), &decoded) == 0);
    assert(decoded == NULL);

    const unsigned char utf8_valid_cases[][4] = {
        {0x7f, 0, 0, 0},
        {0xc2, 0x80, 0, 0},
        {0xdf, 0xbf, 0, 0},
        {0xe0, 0xa0, 0x80, 0},
        {0xed, 0x9f, 0xbf, 0},
        {0xef, 0xbf, 0xbf, 0},
        {0xf0, 0x90, 0x80, 0x80},
        {0xf4, 0x8f, 0xbf, 0xbf},
    };
    const uint64_t utf8_valid_lengths[] = {1, 2, 2, 3, 3, 3, 4, 4};
    for (size_t index = 0; index < sizeof(utf8_valid_lengths) / sizeof(utf8_valid_lengths[0]); ++index) {
        check_utf8_case(utf8_valid_cases[index], utf8_valid_lengths[index], 1);
    }

    const unsigned char utf8_invalid_cases[][4] = {
        {0x80, 0, 0, 0},
        {0xc0, 0x80, 0, 0},
        {0xe0, 0x80, 0x80, 0},
        {0xf0, 0x80, 0x80, 0x80},
        {0xf4, 0x90, 0x80, 0x80},
        {0xf5, 0x80, 0x80, 0x80},
        {0xe2, 0x82, 0, 0},
        {0xf0, 0x90, 0x80, 0},
        {0x61, 0, 0, 0},
    };
    const uint64_t utf8_invalid_lengths[] = {1, 2, 3, 4, 4, 4, 2, 3, 2};
    for (size_t index = 0; index < sizeof(utf8_invalid_lengths) / sizeof(utf8_invalid_lengths[0]); ++index) {
        check_utf8_case(utf8_invalid_cases[index], utf8_invalid_lengths[index], 0);
    }
    check_utf8_case(NULL, 0, 1);

    YazaBytesV1 bounded = {NULL, 0};
    char *error = NULL;
    YazaTextV1 abc = text_view("abc");
    assert(yaza_bytes_v1_from_text_bounded(&abc, 3, &bounded, &error) == 1);
    assert(error == NULL && bounded.length == 3 && memcmp(bounded.data, "abc", 3) == 0);
    assert(yaza_bytes_v1_from_text_bounded(&abc, 2, &bounded, &error) == 0);
    assert(error != NULL && strcmp(error, "Text exceeds the configured byte limit") == 0);
    assert(bounded.length == 0 && bounded.data == NULL);

    YazaTextV1 source = text_view("a,,b,");
    YazaTextV1 delimiter = text_view(",");
    void *parts = yaza_text_v1_split(&source, &delimiter);
    assert(yaza_list_v1_count_typed(parts, "TextList", sizeof(char *)) == 4);
    const YazaTextV1 *part = NULL;
    assert(yaza_list_v1_get_typed(parts, 0, "TextList", sizeof(char *), &part) == 0 && part->length == 1 && memcmp(part->data, "a", 1) == 0);
    assert(yaza_list_v1_get_typed(parts, 1, "TextList", sizeof(char *), &part) == 0 && part->length == 0);
    assert(yaza_list_v1_get_typed(parts, 2, "TextList", sizeof(char *), &part) == 0 && part->length == 1 && memcmp(part->data, "b", 1) == 0);
    assert(yaza_list_v1_get_typed(parts, 3, "TextList", sizeof(char *), &part) == 0 && part->length == 0);

    void *chunks = yaza_list_v1_create_typed("List[Bytes]", sizeof(YazaBytesV1));
    assert(yaza_list_v1_push_typed(chunks, &word, "List[Bytes]", sizeof(word)) == 0);
    assert(yaza_list_v1_push_typed(chunks, &sliced, "List[Bytes]", sizeof(sliced)) == 0);
    YazaBytesV1 joined = {NULL, 0};
    assert(yaza_bytes_v1_join(chunks, &joined, &error) == 1);
    assert(error == NULL && joined.length == 10 && memcmp(joined.data, "hellohello", 10) == 0);
    void *empty_chunks = yaza_list_v1_create_typed("List[Bytes]", sizeof(YazaBytesV1));
    assert(yaza_bytes_v1_join(empty_chunks, &joined, &error) == 1);
    assert(error == NULL && joined.length == 0 && joined.data == NULL);

    assert(yaza_arena_v1_scope_end(&context) == 1);
    assert(yaza_arena_v1_used(arena) == 0);
    assert(yaza_arena_v1_destroy(arena) == 1);

    YazaArenaV1 *small_arena = yaza_arena_v1_create(64ULL * 1024ULL);
    assert(small_arena != NULL);
    YazaArenaContextV1 small_context = {0};
    assert(yaza_arena_v1_scope_begin(small_arena, &small_context) == 1);
    assert(yaza_arena_v1_alloc(small_arena, 64ULL * 1024ULL) != NULL);
    YazaTextV1 x = text_view("x");
    assert(yaza_bytes_v1_from_text_bounded(&x, 1, &bounded, &error) == 0);
    assert(error != NULL && strstr(error, "active arena") != NULL);
    assert(yaza_bytes_v1_try_new_zeroed(1, &zeroed, &zeroed_error) == 0);
    assert(zeroed.data == NULL && zeroed.length == 0);
    assert(zeroed_error != NULL && strstr(zeroed_error, "active arena") != NULL);
    assert(yaza_arena_v1_scope_end(&small_context) == 1);
    assert(yaza_arena_v1_destroy(small_arena) == 1);
    assert(yaza_rt_v1_arena_used() == 0);
    assert(yaza_rt_v1_arena_reserved() == 0);

    puts("runtime-v1: arena-owned Bytes, zeroed fallible Bytes, secure random, checked UTF-8, bounded errors, text_split, and Bytes joins passed");
    return 0;
}

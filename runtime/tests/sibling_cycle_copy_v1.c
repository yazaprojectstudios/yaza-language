#include "yaza_runtime_v1.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void verify_independent_root_copies(void) {
    static const unsigned char expected_bytes[] = {0x00, 0x80, 0xff, 0x41};
    static const unsigned char source_text[] = "independent-root";
    unsigned char source_bytes[sizeof(expected_bytes)];
    unsigned char host_bytes[sizeof(expected_bytes)];
    unsigned char host_text[sizeof(source_text) - 1];
    uint64_t host_text_length = 0;

    YazaArenaV1 *first_parent = yaza_arena_v1_create(4096);
    YazaArenaV1 *second_parent = yaza_arena_v1_create(4096);
    assert(first_parent != NULL && second_parent != NULL);
    YazaArenaCycleV1 *first = yaza_arena_v1_cycle_create(first_parent, 2048);
    YazaArenaCycleV1 *second = yaza_arena_v1_cycle_create(second_parent, 2048);
    assert(first != NULL && second != NULL);
    memcpy(source_bytes, expected_bytes, sizeof(source_bytes));

    assert(yaza_arena_v1_cycle_begin(first) == 1);
    YazaTextV1 *text_in_first = yaza_rt_v1_text_from_cstr("independent-root");
    assert(text_in_first != NULL);
    host_text_length = text_in_first->length;
    assert(host_text_length == sizeof(host_text));
    memcpy(host_text, text_in_first->data, sizeof(host_text));

    const YazaBytesV1 bytes_source = {source_bytes, sizeof(source_bytes)};
    YazaBytesV1 bytes_in_first = {0};
    assert(yaza_rt_v1_module_bytes_copy(&bytes_source, &bytes_in_first) == YAZA_MODULE_RESULT_V1_OK);
    memcpy(host_bytes, bytes_in_first.data, sizeof(host_bytes));
    memset(source_bytes, 0xa5, sizeof(source_bytes));
    YazaArenaCycleMetricsV1 first_metrics = {.struct_size = sizeof(first_metrics)};
    assert(yaza_arena_v1_cycle_end(first, &first_metrics) == 1);
    assert(yaza_arena_v1_cycle_destroy(first) == 1);
    assert(yaza_arena_v1_destroy(first_parent) == 1);

    assert(yaza_arena_v1_cycle_begin(second) == 1);
    const YazaTextV1 text_source = {host_text, host_text_length};
    YazaTextV1 *text_in_second = NULL;
    assert(yaza_rt_v1_module_text_copy(&text_source, &text_in_second) == YAZA_MODULE_RESULT_V1_OK);
    memset(host_text, 0x5a, sizeof(host_text));
    assert(text_in_second->length == sizeof(source_text) - 1);
    assert(memcmp(text_in_second->data, source_text, sizeof(source_text) - 1) == 0);

    const YazaBytesV1 bytes_transfer = {host_bytes, sizeof(host_bytes)};
    YazaBytesV1 bytes_in_second = {0};
    assert(yaza_rt_v1_module_bytes_copy(&bytes_transfer, &bytes_in_second) == YAZA_MODULE_RESULT_V1_OK);
    memset(host_bytes, 0x5a, sizeof(host_bytes));
    assert(bytes_in_second.length == sizeof(expected_bytes));
    assert(memcmp(bytes_in_second.data, expected_bytes, sizeof(expected_bytes)) == 0);

    YazaArenaCycleMetricsV1 second_metrics = {.struct_size = sizeof(second_metrics)};
    assert(yaza_arena_v1_cycle_end(second, &second_metrics) == 1);
    assert(yaza_arena_v1_cycle_destroy(second) == 1);
    assert(yaza_arena_v1_destroy(second_parent) == 1);
}

int main(void) {
    static const unsigned char expected_bytes[] = {0x00, 0xff, 0x41, 0x00, 0x7f};
    unsigned char source_bytes[sizeof(expected_bytes)];
    unsigned char sibling_transfer[sizeof(expected_bytes)] = {0};
    memcpy(source_bytes, expected_bytes, sizeof(source_bytes));
    yaza_rt_v1_init();
    YazaArenaV1 *parent = yaza_arena_v1_create(512ULL * 1024ULL);
    assert(parent != NULL);
    YazaArenaContextV1 parent_context = {0};
    assert(yaza_arena_v1_scope_begin(parent, &parent_context) == 1);

    void *texts = yaza_list_v1_create_typed("TextList", sizeof(const YazaTextV1 *));
    YazaArenaCycleV1 *first = yaza_arena_v1_cycle_create(parent, 128ULL * 1024ULL);
    YazaArenaCycleV1 *second = yaza_arena_v1_cycle_create(parent, 128ULL * 1024ULL);
    assert(texts != NULL && first != NULL && second != NULL);

    assert(yaza_arena_v1_cycle_begin(first) == 1);
    YazaTextV1 *first_text = yaza_rt_v1_text_from_cstr("copied from sibling A");
    const YazaTextV1 *first_text_pointer = first_text;
    assert(yaza_list_v1_push_typed(texts, &first_text_pointer, "TextList", sizeof(first_text_pointer)) == 0);
    const YazaBytesV1 source = {source_bytes, sizeof(source_bytes)};
    YazaBytesV1 first_bytes = {0};
    assert(yaza_rt_v1_module_bytes_copy(&source, &first_bytes) == YAZA_MODULE_RESULT_V1_OK);
    assert(first_bytes.length == sizeof(source_bytes));
    memcpy(sibling_transfer, first_bytes.data, sizeof(sibling_transfer));
    memset((void *)source_bytes, 0xa5, sizeof(source_bytes));
    YazaArenaCycleMetricsV1 first_metrics = {.struct_size = sizeof(first_metrics)};
    assert(yaza_arena_v1_cycle_end(first, &first_metrics) == 1);
    assert(first_metrics.successful_allocations > 0);
    assert(yaza_arena_v1_current() == parent);

    assert(yaza_arena_v1_cycle_begin(second) == 1);
    const YazaTextV1 *copied_text = NULL;
    assert(yaza_list_v1_get_typed(texts, 0, "TextList", sizeof(copied_text), &copied_text) == 0);
    assert(copied_text != NULL);
    assert(copied_text->length == strlen("copied from sibling A"));
    assert(memcmp(copied_text->data, "copied from sibling A", copied_text->length) == 0);
    const YazaBytesV1 sibling_source = {sibling_transfer, sizeof(sibling_transfer)};
    YazaBytesV1 copied_bytes = {0};
    assert(yaza_rt_v1_module_bytes_copy(&sibling_source, &copied_bytes) == YAZA_MODULE_RESULT_V1_OK);
    memset(sibling_transfer, 0x5a, sizeof(sibling_transfer));
    assert(copied_bytes.length == sizeof(source_bytes));
    assert(memcmp(copied_bytes.data, expected_bytes, sizeof(expected_bytes)) == 0);
    YazaArenaCycleMetricsV1 second_metrics = {.struct_size = sizeof(second_metrics)};
    assert(yaza_arena_v1_cycle_end(second, &second_metrics) == 1);

    assert(yaza_arena_v1_cycle_destroy(first) == 1);
    assert(yaza_arena_v1_cycle_destroy(second) == 1);
    assert(yaza_arena_v1_scope_end(&parent_context) == 1);
    assert(yaza_arena_v1_used(parent) == 0);
    assert(yaza_arena_v1_destroy(parent) == 1);
    verify_independent_root_copies();
    assert(yaza_rt_v1_live_scope_count() == 0);

    puts("arena-v1: sibling and independent request cycles transfer Text/Bytes only through explicit copies");
    return 0;
}

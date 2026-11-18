#include "yaza_runtime_v1.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    YazaRuntimeConfigV1 config = {
        .abi_version = YAZA_RUNTIME_CONFIG_V1_ABI_VERSION,
        .struct_size = sizeof(YazaRuntimeConfigV1),
        .process_limit_bytes = 128ULL * 1024ULL,
        .preferred_chunk_bytes = 16ULL * 1024ULL,
        .max_live_scopes = 2,
        .max_list_items = 16,
        .max_list_vector_bytes = 64,
    };

    YazaRuntimeConfigV1 invalid = config;
    invalid.preferred_chunk_bytes = 3ULL * 1024ULL;
    assert(yaza_rt_v1_configure(&invalid) == 0);
    invalid = config;
    invalid.max_list_vector_bytes = config.process_limit_bytes + 1;
    assert(yaza_rt_v1_configure(&invalid) == 0);
    invalid = config;
    invalid.max_list_vector_bytes = sizeof(uint64_t) - 1;
    assert(yaza_rt_v1_configure(&invalid) == 0);
    invalid = config;
    invalid.abi_version += 1;
    assert(yaza_rt_v1_configure(&invalid) == 0);
    invalid = config;
    invalid.struct_size -= 1;
    assert(yaza_rt_v1_configure(&invalid) == 0);
    assert(yaza_rt_v1_configure(&config) == 1);

    YazaRuntimeConfigV1 observed = {0};
    assert(yaza_rt_v1_get_config(&observed) == 1);
    assert(observed.abi_version == config.abi_version);
    assert(observed.struct_size == sizeof(YazaRuntimeConfigV1));
    assert(observed.process_limit_bytes == config.process_limit_bytes);
    assert(observed.preferred_chunk_bytes == config.preferred_chunk_bytes);
    assert(observed.max_live_scopes == config.max_live_scopes);
    assert(observed.max_list_items == config.max_list_items);
    assert(observed.max_list_vector_bytes == config.max_list_vector_bytes);
    assert(yaza_rt_v1_configure(&config) == 0);

    YazaArenaV1 *first = yaza_arena_v1_create(config.process_limit_bytes);
    YazaArenaV1 *second = yaza_arena_v1_create(config.process_limit_bytes);
    assert(first != NULL && second != NULL);
    assert(yaza_arena_v1_create(config.process_limit_bytes) == NULL);
    _Alignas(YAZA_RT_V1_SCOPE_INLINE_STORAGE_ALIGN)
        unsigned char inline_scope_storage[YAZA_RT_V1_SCOPE_INLINE_STORAGE_BYTES +
            YAZA_RT_V1_SCOPE_INLINE_STORAGE_ALIGN] = {0};
    const uint64_t failures_before_invalid_storage = yaza_rt_v1_arena_allocation_failures();
    assert(yaza_rt_v1_scope_enter_inplace(0, inline_scope_storage,
        sizeof(inline_scope_storage)) == NULL);
    assert(yaza_rt_v1_arena_allocation_failures() == failures_before_invalid_storage);
    assert(yaza_rt_v1_scope_enter_inplace(32ULL * 1024ULL, inline_scope_storage + 1,
        sizeof(inline_scope_storage) - 1) == NULL);
    assert(yaza_rt_v1_arena_allocation_failures() == failures_before_invalid_storage);
    assert(yaza_rt_v1_arena_scope_count() == config.max_live_scopes);
    const uint64_t failures_before_scope_cap = yaza_rt_v1_arena_allocation_failures();
    assert(yaza_rt_v1_scope_enter_inplace(32ULL * 1024ULL, inline_scope_storage,
        sizeof(inline_scope_storage)) == NULL);
    assert(yaza_rt_v1_arena_allocation_failures() == failures_before_scope_cap + 1);
    assert(yaza_rt_v1_arena_scope_count() == config.max_live_scopes);
    assert(yaza_arena_v1_alloc(first, 64ULL * 1024ULL) != NULL);
    assert(yaza_arena_v1_alloc(second, 64ULL * 1024ULL) != NULL);
    assert(yaza_rt_v1_arena_reserved() == config.process_limit_bytes);
    assert(yaza_arena_v1_alloc(second, 65ULL * 1024ULL) == NULL);
    assert(yaza_arena_v1_destroy(first) == 1);
    YazaArenaScopeV1 *inline_scope = yaza_rt_v1_scope_enter_inplace(32ULL * 1024ULL,
        inline_scope_storage, sizeof(inline_scope_storage));
    assert(inline_scope != NULL);
    assert(yaza_rt_v1_scope_leave(inline_scope) == 1);
    assert(yaza_rt_v1_arena_scope_count() == 1);
    assert(yaza_arena_v1_alloc(second, 65ULL * 1024ULL) == NULL);
    assert(yaza_arena_v1_destroy(second) == 1);
    assert(yaza_rt_v1_arena_used() == 0);
    assert(yaza_rt_v1_arena_reserved() == 0);

    YazaArenaV1 *list_arena = yaza_arena_v1_create(config.process_limit_bytes);
    assert(list_arena != NULL);
    YazaArenaContextV1 context = {0};
    assert(yaza_arena_v1_scope_begin(list_arena, &context) == 1);
    void *numbers = yaza_list_v1_create_typed("List[Number]", sizeof(double));
    for (uint64_t index = 0; index < config.max_list_vector_bytes / sizeof(double); ++index) {
        double value = (double)index;
        assert(yaza_list_v1_try_push_typed(numbers, &value, "List[Number]", sizeof(value)) == 1);
    }
    double extra = 8.0;
    assert(yaza_list_v1_try_push_typed(numbers, &extra, "List[Number]", sizeof(extra)) == 0);
    assert(yaza_list_v1_count_typed(numbers, "List[Number]", sizeof(double)) == config.max_list_vector_bytes / sizeof(double));

    void *booleans = yaza_list_v1_create_typed("List[Boolean]", sizeof(uint8_t));
    uint8_t flag = 1;
    for (uint64_t index = 0; index < config.max_list_items; ++index) {
        assert(yaza_list_v1_try_push_typed(booleans, &flag, "List[Boolean]", sizeof(flag)) == 1);
    }
    assert(yaza_list_v1_try_push_typed(booleans, &flag, "List[Boolean]", sizeof(flag)) == 0);
    assert(yaza_list_v1_count_typed(booleans, "List[Boolean]", sizeof(flag)) == config.max_list_items);
    assert(yaza_arena_v1_scope_end(&context) == 1);
    assert(yaza_arena_v1_used(list_arena) == 0);
    assert(yaza_arena_v1_destroy(list_arena) == 1);
    assert(yaza_rt_v1_arena_scope_count() == 0);
    assert(yaza_rt_v1_arena_reserved() == 0);

    puts("runtime-v1: pre-init memory profile, process quota, scope cap, list cap, and immutable configuration passed");
    return 0;
}

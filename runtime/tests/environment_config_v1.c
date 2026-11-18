#include "yaza_runtime_v1.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    YazaRuntimeConfigV1 config = {0};
    assert(yaza_rt_v1_get_config(&config) == 1);
    assert(config.abi_version == YAZA_RUNTIME_CONFIG_V1_ABI_VERSION);
    assert(config.process_limit_bytes == 128ULL * 1024ULL);
    assert(config.preferred_chunk_bytes == 16ULL * 1024ULL);
    assert(config.max_live_scopes == 2);
    assert(config.max_list_items == 16);
    assert(config.max_list_vector_bytes == 64);
    YazaArenaV1 *arena = yaza_arena_v1_create(config.process_limit_bytes);
    assert(arena != NULL);
    assert(yaza_arena_v1_destroy(arena) == 1);
    puts("runtime-v1: deployment environment memory profile loaded before runtime initialization");
    return 0;
}

#include "yaza_runtime_v1.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    yaza_rt_v1_init();
    YazaRuntimeConfigV1 config = {0};
    assert(yaza_rt_v1_get_config(&config) == 1);
    assert(config.process_limit_bytes == 131072);
    assert(config.max_list_vector_bytes == 131072);
    YazaArenaV1 *first = yaza_arena_v1_create(131072);
    YazaArenaV1 *second = yaza_arena_v1_create(131072);
    YazaArenaV1 *third = yaza_arena_v1_create(131072);
    assert(first != NULL && second != NULL && third != NULL);
    assert(yaza_rt_v1_arena_scope_count() == 3);

    assert(yaza_arena_v1_alloc(first, 100000) != NULL);
    assert(yaza_arena_v1_alloc(second, 30000) != NULL);
    assert(yaza_rt_v1_arena_reserved() == 131072);
    assert(yaza_arena_v1_alloc(second, 1000) != NULL);
    assert(yaza_rt_v1_arena_used() == 131000);
    assert(yaza_arena_v1_alloc(second, 2000) == NULL);
    assert(yaza_arena_v1_alloc(third, 1) == NULL);

    assert(yaza_arena_v1_destroy(first) == 1);
    assert(yaza_rt_v1_arena_scope_count() == 2);
    assert(yaza_arena_v1_alloc(third, 65536) != NULL);
    assert(yaza_arena_v1_destroy(second) == 1);
    assert(yaza_arena_v1_used(third) == 65536);
    assert(yaza_rt_v1_arena_reserved() == 65536);
    assert(yaza_arena_v1_destroy(third) == 1);
    assert(yaza_rt_v1_arena_scope_count() == 0);
    assert(yaza_rt_v1_arena_used() == 0);
    assert(yaza_rt_v1_arena_reserved() == 0);
    const uint64_t process_peak_used = yaza_rt_v1_arena_peak_used();
    assert(process_peak_used >= 131000);
    assert(process_peak_used <= config.process_limit_bytes * 3);
    assert(yaza_rt_v1_arena_allocation_count() == 4);
    assert(yaza_rt_v1_arena_allocation_failures() == 2);
    puts("arena-v1: independent scope trees obey one process-wide byte quota and reclaim reservations");
    return 0;
}

#include "yaza_runtime_v1.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    yaza_rt_v1_init();
    YazaArenaV1 *root_a = yaza_arena_v1_create(1024);
    YazaArenaV1 *root_b = yaza_arena_v1_create(1024);
    assert(root_a != NULL && root_b != NULL);
    YazaArenaV1 *child = yaza_arena_v1_create_child(root_a, 512);
    assert(child != NULL);
    assert(yaza_rt_v1_arena_scope_count() == 3);
    assert(yaza_arena_v1_create(1) == NULL);

    assert(yaza_arena_v1_destroy(root_b) == 1);
    YazaArenaV1 *root_c = yaza_arena_v1_create(1024);
    assert(root_c != NULL);
    assert(yaza_arena_v1_destroy(root_a) == 0);
    assert(yaza_arena_v1_destroy(child) == 1);
    assert(yaza_arena_v1_destroy(root_a) == 1);
    assert(yaza_arena_v1_destroy(root_c) == 1);
    assert(yaza_rt_v1_arena_scope_count() == 0);
    puts("arena-v1: process-wide scope metadata cap is enforced and reclaimed");
    return 0;
}

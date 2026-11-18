#include "yaza_runtime_v1.h"

#include <stdio.h>
#include <string.h>

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define YAZA_TEST_WITH_ADDRESS_SANITIZER 1
#endif
#endif
#if defined(__SANITIZE_ADDRESS__)
#define YAZA_TEST_WITH_ADDRESS_SANITIZER 1
#endif
#ifndef YAZA_TEST_WITH_ADDRESS_SANITIZER
#define YAZA_TEST_WITH_ADDRESS_SANITIZER 0
#endif

#if YAZA_TEST_WITH_ADDRESS_SANITIZER
#include <sanitizer/asan_interface.h>
#endif

int main(int argc, char **argv) {
#if YAZA_TEST_WITH_ADDRESS_SANITIZER
    yaza_rt_v1_init();
    if (argc == 2 && strcmp(argv[1], "--read-after-reset") == 0) {
        YazaArenaV1 *stale_arena = yaza_arena_v1_create(1024ULL * 1024ULL);
        if (stale_arena == NULL) return 10;
        volatile unsigned char *stale = (volatile unsigned char *)yaza_arena_v1_alloc(stale_arena, 32);
        if (stale == NULL || !yaza_arena_v1_reset(stale_arena)) return 11;
        volatile unsigned char value = stale[0];
        return (int)value;
    }
    if (argc != 1) {
        fputs("arena ASan poisoning: unknown test mode\n", stderr);
        return 2;
    }

    YazaArenaV1 *arena = yaza_arena_v1_create(1024ULL * 1024ULL);
    if (arena == NULL) {
        fputs("arena ASan poisoning: could not create arena\n", stderr);
        return 1;
    }

    unsigned char *first = (unsigned char *)yaza_arena_v1_alloc(arena, 32);
    if (first == NULL || __asan_address_is_poisoned(first)) {
        fputs("arena ASan poisoning: live allocation is poisoned\n", stderr);
        return 1;
    }
    first[0] = 0x5a;
    if (!yaza_arena_v1_reset(arena)) {
        fputs("arena ASan poisoning: reset failed\n", stderr);
        return 1;
    }
    if (!__asan_address_is_poisoned(first)) {
        fputs("arena ASan poisoning: reset did not poison stale storage\n", stderr);
        return 1;
    }

    unsigned char *second = (unsigned char *)yaza_arena_v1_alloc(arena, 32);
    if (second == NULL || second != first || __asan_address_is_poisoned(second)) {
        fputs("arena ASan poisoning: reused allocation was not unpoisoned\n", stderr);
        return 1;
    }
    second[0] = 0xa5;
    if (!yaza_arena_v1_reset(arena) || !__asan_address_is_poisoned(second)) {
        fputs("arena ASan poisoning: second reset did not poison reused storage\n", stderr);
        return 1;
    }
    if (!yaza_arena_v1_destroy(arena)) {
        fputs("arena ASan poisoning: destroy failed\n", stderr);
        return 1;
    }
    puts("arena ASan poisoning: reset poisons reclaimed storage and allocation unpoisons reused bytes");
    return 0;
#else
    (void)argc;
    (void)argv;
    puts("arena ASan poisoning: skipped without AddressSanitizer");
    return 0;
#endif
}

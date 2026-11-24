#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE
#endif

#include "yaza_runtime_v1.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(YAZA_RT_V1_WITH_CURL)
#include <curl/curl.h>
#endif
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#include <io.h>
#include <direct.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#ifndef SSIZE_MAX
#define SSIZE_MAX INTPTR_MAX
#endif
#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif
typedef SOCKET yaza_net_sock_t;
#define YAZA_NET_INVALID_SOCKET INVALID_SOCKET
typedef struct _stat64 yaza_stat_t;
static int yaza_fstat64(int fd, yaza_stat_t *st) { return _fstat64(fd, st); }
static int yaza_stat64(const char *path, yaza_stat_t *st) { return _stat64(path, st); }
#else
#include <fcntl.h>
#include <unistd.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <dirent.h>
#include <time.h>
#include <limits.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <poll.h>
#include <signal.h>
#include <sched.h>
#include <termios.h>
#include <dlfcn.h>
typedef int yaza_net_sock_t;
#define YAZA_NET_INVALID_SOCKET (-1)
typedef struct stat yaza_stat_t;
static int yaza_fstat64(int fd, yaza_stat_t *st) { return fstat(fd, st); }
static int yaza_stat64(const char *path, yaza_stat_t *st) { return stat(path, st); }
#endif

#if defined(YAZA_RT_V1_WITH_ARGON2)
#include <argon2.h>
#endif

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#include <sys/sysctl.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonHMAC.h>
#include <CommonCrypto/CommonKeyDerivation.h>
#endif

#if !defined(_WIN32)
extern char **environ;
#endif

struct YazaThreadV1 {
    pthread_t native_thread;
    YazaThreadEntryV1 entry;
    void *context;
};

struct YazaMutexV1 {
    pthread_mutex_t native_mutex;
};

struct YazaConditionV1 {
    pthread_cond_t native_condition;
};

static void *yaza_rt_v1_thread_entry(void *opaque);

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define YAZA_RT_V1_WITH_ADDRESS_SANITIZER 1
#endif
#endif
#if defined(__SANITIZE_ADDRESS__)
#define YAZA_RT_V1_WITH_ADDRESS_SANITIZER 1
#endif
#ifndef YAZA_RT_V1_WITH_ADDRESS_SANITIZER
#define YAZA_RT_V1_WITH_ADDRESS_SANITIZER 0
#endif
#if YAZA_RT_V1_WITH_ADDRESS_SANITIZER
#include <sanitizer/asan_interface.h>
#endif

#ifndef YAZA_RT_V1_PROCESS_LIMIT
#define YAZA_RT_V1_PROCESS_LIMIT (4096ULL * 1024ULL * 1024ULL)
#endif
#ifndef YAZA_RT_V1_CHUNK_SIZE
#define YAZA_RT_V1_CHUNK_SIZE (64ULL * 1024ULL)
#endif
#ifndef YAZA_RT_V1_MAX_SCOPED_ARENAS
#define YAZA_RT_V1_MAX_SCOPED_ARENAS 65536ULL
#endif
#ifndef YAZA_LIST_V1_MAX_ITEMS
#define YAZA_LIST_V1_MAX_ITEMS 1000000ULL
#endif
#ifndef YAZA_LIST_V1_MAX_BYTES
#define YAZA_LIST_V1_MAX_BYTES (64ULL * 1024ULL * 1024ULL)
#endif
#if YAZA_RT_V1_PROCESS_LIMIT < YAZA_LIST_V1_MAX_BYTES
#define YAZA_LIST_V1_DEFAULT_MAX_BYTES YAZA_RT_V1_PROCESS_LIMIT
#else
#define YAZA_LIST_V1_DEFAULT_MAX_BYTES YAZA_LIST_V1_MAX_BYTES
#endif
#define YAZA_LIST_V1_TEXT_LIST_TYPE 6ULL
#define YAZA_RT_V1_MIN_PROCESS_LIMIT (64ULL * 1024ULL)
#define YAZA_RT_V1_MIN_CHUNK_SIZE 4096ULL
#define YAZA_RT_V1_MAX_CHUNK_SIZE (4ULL * 1024ULL * 1024ULL)
#define YAZA_RT_V1_MAX_LIVE_SCOPES 1000000ULL
#define YAZA_RT_V1_PROCESS_ALLOCATION_SHARDS 256ULL
#define YAZA_RT_V1_PROCESS_METRIC_SHARDS 256ULL

typedef struct YazaRtV1Chunk {
    struct YazaRtV1Chunk *next;
    uint64_t capacity;
    uint64_t used;
    _Alignas(max_align_t) unsigned char data[];
} YazaRtV1Chunk;

_Static_assert(offsetof(YazaRtV1Chunk, data) % _Alignof(max_align_t) == 0, "arena payload must be max_align_t-aligned");

typedef struct YazaListV1 {
    _Atomic bool lock;
    _Atomic uint64_t mutation_generation;
    YazaArenaV1 *owner;
    uint64_t owner_generation;
    uint64_t element_type_id;
    const char *collection_type;
    uint64_t element_size;
    uint64_t length;
    uint64_t capacity;
    uint64_t storage_generation;
    uint64_t current_storage_borrows;
    unsigned char *items;
} YazaListV1;

struct YazaArenaV1 {
    /* One tree lock protects every arena and budget below a root. */
    _Atomic bool lock;
    struct YazaArenaV1 *root;
    struct YazaArenaV1 *parent;
    uint32_t process_metric_shard;
    YazaRtV1Chunk *head;
    YazaRtV1Chunk *tail;
    uint64_t limit;
    uint64_t local_reserved;
    uint64_t local_used;
    uint64_t local_storage_used;
    uint64_t subtree_reserved;
    uint64_t subtree_used;
    uint64_t subtree_storage_used;
    uint64_t peak_subtree_used;
    uint64_t peak_subtree_storage_used;
    uint64_t peak_subtree_reserved;
    uint64_t last_cycle_generation;
    uint64_t cycle_successful_allocations;
    uint64_t cycle_allocation_failures;
    uint64_t last_cycle_successful_allocations;
    uint64_t last_cycle_allocation_failures;
    uint64_t last_cycle_peak_used;
    uint64_t last_cycle_peak_storage_used;
    uint64_t last_cycle_peak_reserved;
    uint64_t last_cycle_retained_reserved;
    uint64_t allocations;
    uint64_t generation;
    uint64_t child_count;
    uint64_t descendant_count;
    uint64_t active_leases;
    uint64_t active_contexts;
    uint64_t explicit_leases;
    int scope_active;
    YazaArenaContextV1 *scope_context;
    int destroyed;
    int heap_owned;
};

struct YazaArenaScopeV1 {
    YazaArenaV1 *arena;
    YazaArenaContextV1 context;
    int context_active;
    int heap_owned;
};

struct YazaArenaCycleV1 {
    YazaArenaV1 *arena;
    YazaArenaContextV1 context;
    int active;
};

typedef struct YazaRtV1InlineScopeStorage {
    YazaArenaScopeV1 scope;
    YazaArenaV1 arena;
} YazaRtV1InlineScopeStorage;

_Static_assert(sizeof(YazaRtV1InlineScopeStorage) <= YAZA_RT_V1_SCOPE_INLINE_STORAGE_BYTES,
    "compiler-provided lexical-scope storage is too small for runtime metadata");
_Static_assert(_Alignof(YazaRtV1InlineScopeStorage) <= YAZA_RT_V1_SCOPE_INLINE_STORAGE_ALIGN,
    "compiler-provided lexical-scope storage alignment is insufficient");

static YazaArenaV1 yaza_rt_v1_program_arena = {
    .lock = false,
    .root = NULL,
    .parent = NULL,
    .process_metric_shard = 0,
    .head = NULL,
    .tail = NULL,
    .limit = YAZA_RT_V1_PROCESS_LIMIT,
    .local_reserved = 0,
    .local_used = 0,
    .local_storage_used = 0,
    .subtree_reserved = 0,
    .subtree_used = 0,
    .subtree_storage_used = 0,
    .peak_subtree_used = 0,
    .peak_subtree_storage_used = 0,
    .peak_subtree_reserved = 0,
    .last_cycle_generation = 0,
    .last_cycle_peak_used = 0,
    .last_cycle_peak_storage_used = 0,
    .last_cycle_peak_reserved = 0,
    .allocations = 0,
    .generation = 1,
    .child_count = 0,
    .descendant_count = 0,
    .active_leases = 0,
    .active_contexts = 0,
    .explicit_leases = 0,
    .scope_active = 0,
    .scope_context = NULL,
    .destroyed = 0,
    .heap_owned = 0,
};
static YazaRuntimeConfigV1 yaza_rt_v1_config = {
    .abi_version = YAZA_RUNTIME_CONFIG_V1_ABI_VERSION,
    .struct_size = sizeof(YazaRuntimeConfigV1),
    .process_limit_bytes = YAZA_RT_V1_PROCESS_LIMIT,
    .preferred_chunk_bytes = YAZA_RT_V1_CHUNK_SIZE,
    .max_live_scopes = YAZA_RT_V1_MAX_SCOPED_ARENAS,
    .max_list_items = YAZA_LIST_V1_MAX_ITEMS,
    .max_list_vector_bytes = YAZA_LIST_V1_DEFAULT_MAX_BYTES,
};
static atomic_flag yaza_rt_v1_exit_registration_lock = ATOMIC_FLAG_INIT;
static atomic_flag yaza_rt_v1_init_lock = ATOMIC_FLAG_INIT;
static atomic_flag yaza_rt_v1_args_lock = ATOMIC_FLAG_INIT;
static _Atomic int yaza_rt_v1_initialized = 0;
static _Atomic int yaza_rt_v1_args_initialized = 0;
static int yaza_rt_v1_config_set_explicitly = 0;
static int64_t yaza_rt_v1_user_arg_count = 0;
static YazaTextV1 *yaza_rt_v1_arguments = NULL;
static _Atomic int yaza_rt_v1_exit_registered = 0;
static _Atomic uint64_t yaza_rt_v1_process_reserved = 0;
static _Atomic uint64_t yaza_rt_v1_process_peak_reserved = 0;
static _Atomic uint64_t yaza_rt_v1_process_scope_count = 0;
#if defined(YAZA_RT_V1_BENCH_GLOBAL_PROCESS_LIVE_METRICS)
static _Atomic uint64_t yaza_rt_v1_process_used = 0;
static _Atomic uint64_t yaza_rt_v1_process_storage_used = 0;
static _Atomic uint64_t yaza_rt_v1_process_peak_used = 0;
static _Atomic uint64_t yaza_rt_v1_process_peak_storage_used = 0;
#elif !defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_METRICS)
typedef struct YazaRtV1LiveMetricShard {
    _Atomic uint64_t used;
    _Atomic uint64_t storage_used;
    _Atomic uint64_t peak_used;
    _Atomic uint64_t peak_storage_used;
    unsigned char padding[64 - 4 * sizeof(_Atomic uint64_t)];
} YazaRtV1LiveMetricShard;

_Static_assert(sizeof(_Atomic uint64_t) < 64, "atomic counter must fit in a cache-line shard");
_Static_assert(sizeof(YazaRtV1LiveMetricShard) == 64, "live metric shard must occupy one 64-byte cache line");
_Alignas(64) static YazaRtV1LiveMetricShard yaza_rt_v1_process_live_metric_shards[YAZA_RT_V1_PROCESS_METRIC_SHARDS];
static _Atomic uint64_t yaza_rt_v1_next_process_metric_shard = 1;
#endif
#if defined(YAZA_RT_V1_BENCH_GLOBAL_PROCESS_ALLOCATION_COUNT)
static _Atomic uint64_t yaza_rt_v1_process_allocation_count = 0;
#elif !defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_METRICS) && !defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_ALLOCATION_COUNT)
/* The hot cumulative counter is sharded by thread; the read API sums these gauges. */
typedef struct YazaRtV1MetricShard {
    _Atomic uint64_t allocation_count;
    unsigned char padding[64 - sizeof(_Atomic uint64_t)];
} YazaRtV1MetricShard;

_Static_assert(sizeof(_Atomic uint64_t) < 64, "atomic counter must fit in a cache-line shard");
_Static_assert(sizeof(YazaRtV1MetricShard) == 64, "process metric shard must occupy one 64-byte cache line");
#endif
_Static_assert(offsetof(YazaModuleResultV1, status) == 8, "module result ABI status offset changed");
_Static_assert(offsetof(YazaModuleResultV1, data) == 16, "module result ABI data offset changed");
_Static_assert(offsetof(YazaModuleResultV1, length) == 24, "module result ABI length offset changed");
_Static_assert(sizeof(YazaModuleResultV1) == 32, "module result ABI size changed");
_Static_assert(offsetof(YazaModuleInputV1, kind) == 8, "module input ABI kind offset changed");
_Static_assert(offsetof(YazaModuleInputV1, data) == 16, "module input ABI data offset changed");
_Static_assert(offsetof(YazaModuleInputV1, length) == 24, "module input ABI length offset changed");
_Static_assert(sizeof(YazaModuleInputV1) == 32, "module input ABI size changed");
#if !defined(YAZA_RT_V1_BENCH_GLOBAL_PROCESS_ALLOCATION_COUNT) && !defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_METRICS) && !defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_ALLOCATION_COUNT)
_Alignas(64) static YazaRtV1MetricShard yaza_rt_v1_process_allocation_shards[YAZA_RT_V1_PROCESS_ALLOCATION_SHARDS];
static _Atomic uint64_t yaza_rt_v1_next_allocation_shard = 0;
static _Thread_local uint32_t yaza_rt_v1_allocation_shard_index = UINT32_MAX;
#endif
static _Atomic uint64_t yaza_rt_v1_process_allocation_failures = 0;
static _Thread_local YazaArenaV1 *yaza_rt_v1_current_arena = NULL;
static _Thread_local YazaArenaContextV1 *yaza_rt_v1_current_context = NULL;

typedef struct YazaVfsV1Slot {
    int descriptor;
    uint64_t generation;
    uint32_t active_operations;
    uint32_t child_handles;
    uint32_t kind;
    uint32_t access;
    uint32_t parent_slot;
    uint64_t parent_generation;
} YazaVfsV1Slot;

enum {
    YAZA_VFS_V1_KIND_FREE = 0,
    YAZA_VFS_V1_KIND_DIRECTORY = 1,
    YAZA_VFS_V1_KIND_FILE = 2,
    YAZA_VFS_V1_ACCESS_READ = 1,
    YAZA_VFS_V1_ACCESS_WRITE = 2,
};

static atomic_flag yaza_vfs_v1_registry_lock = ATOMIC_FLAG_INIT;
static YazaVfsV1Slot yaza_vfs_v1_slots[YAZA_VFS_V1_MAX_HANDLES];
static uint64_t yaza_vfs_v1_next_generation = 1;
static _Atomic uint64_t yaza_vfs_v1_temporary_bytes = 0;
#if defined(YAZA_VFS_V1_TESTING) && defined(YAZA_VFS_V1_TEST_MAX_HANDLES)
#define YAZA_VFS_V1_ACTIVE_HANDLE_LIMIT YAZA_VFS_V1_TEST_MAX_HANDLES
_Static_assert(YAZA_VFS_V1_TEST_MAX_HANDLES > 0 && YAZA_VFS_V1_TEST_MAX_HANDLES <= YAZA_VFS_V1_MAX_HANDLES,
    "test VFS handle limit must fit the production handle table");
#else
#define YAZA_VFS_V1_ACTIVE_HANDLE_LIMIT YAZA_VFS_V1_MAX_HANDLES
#endif
#define YAZA_VFS_V1_MAX_DIRECTORY_LIST_BYTES YAZA_VFS_V1_MAX_TRANSFER_BYTES
#if defined(YAZA_VFS_V1_TESTING)
static _Atomic uint32_t yaza_vfs_v1_test_pread_eintr_count = 0;
static _Atomic uint32_t yaza_vfs_v1_test_pwrite_eintr_count = 0;
static _Atomic uint64_t yaza_vfs_v1_test_max_io_chunk_bytes = 0;
static _Atomic int64_t yaza_vfs_v1_test_pwrite_fail_after_bytes = -1;
static _Atomic uint32_t yaza_vfs_v1_test_failure_mask = 0;
static _Atomic uint64_t yaza_vfs_v1_test_temporary_limit = YAZA_VFS_V1_MAX_TEMPORARY_BYTES;
static _Atomic uint64_t yaza_vfs_v1_test_fsync_counter = 0;
static _Atomic uint64_t yaza_vfs_v1_test_fsync_crash_target = 0;
#endif

static void yaza_rt_v1_asan_poison(const void *address, size_t size) {
#if YAZA_RT_V1_WITH_ADDRESS_SANITIZER
    if (address != NULL && size != 0) {
        __asan_poison_memory_region(address, size);
    }
#else
    (void)address;
    (void)size;
#endif
}


static void yaza_rt_v1_asan_unpoison(const void *address, size_t size) {
#if YAZA_RT_V1_WITH_ADDRESS_SANITIZER
    if (address != NULL && size != 0) {
        __asan_unpoison_memory_region(address, size);
    }
#else
    (void)address;
    (void)size;
#endif
}

/* An active reusable scope is exclusive to the thread that entered it, but
   same-thread nested scopes retain the parent's context in this stack. */
static int yaza_rt_v1_context_contains(const YazaArenaContextV1 *context) {
    for (YazaArenaContextV1 *active = yaza_rt_v1_current_context; active != NULL; active = active->previous_context) {
        if (active == context) return 1;
    }
    return 0;
}

#if defined(YAZA_RT_V1_TESTING)
static _Atomic uint64_t yaza_rt_v1_test_alloc_countdown = UINT64_MAX;

void yaza_rt_v1_test_fail_alloc_after(uint64_t successful_allocations) {
    atomic_store_explicit(&yaza_rt_v1_test_alloc_countdown, successful_allocations, memory_order_relaxed);
}

void yaza_rt_v1_test_disable_alloc_failures(void) {
    atomic_store_explicit(&yaza_rt_v1_test_alloc_countdown, UINT64_MAX, memory_order_relaxed);
}

static int yaza_rt_v1_test_should_fail_alloc(void) {
    uint64_t remaining = atomic_load_explicit(&yaza_rt_v1_test_alloc_countdown, memory_order_relaxed);
    while (remaining != UINT64_MAX) {
        const uint64_t next = remaining == 0 ? UINT64_MAX : remaining - 1;
        if (atomic_compare_exchange_weak_explicit(&yaza_rt_v1_test_alloc_countdown, &remaining, next,
                memory_order_relaxed, memory_order_relaxed)) {
            return remaining == 0;
        }
    }
    return 0;
}
#else
static int yaza_rt_v1_test_should_fail_alloc(void) {
    return 0;
}
#endif

static void *yaza_rt_v1_system_malloc(size_t size) {
    if (yaza_rt_v1_test_should_fail_alloc()) return NULL;
    return malloc(size);
}

static void *yaza_rt_v1_system_calloc(size_t count, size_t size) {
    if (yaza_rt_v1_test_should_fail_alloc()) return NULL;
    return calloc(count, size);
}

static void yaza_rt_v1_record_process_reservation(uint64_t reserved);

static void yaza_rt_v1_record_process_allocation(void) {
#if defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_METRICS) || defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_ALLOCATION_COUNT)
    return;
#elif defined(YAZA_RT_V1_BENCH_GLOBAL_PROCESS_ALLOCATION_COUNT)
    atomic_fetch_add_explicit(&yaza_rt_v1_process_allocation_count, 1, memory_order_relaxed);
#else
    if (yaza_rt_v1_allocation_shard_index == UINT32_MAX) {
        const uint64_t next = atomic_fetch_add_explicit(&yaza_rt_v1_next_allocation_shard, 1, memory_order_relaxed);
        yaza_rt_v1_allocation_shard_index = (uint32_t)(next % YAZA_RT_V1_PROCESS_ALLOCATION_SHARDS);
    }
    atomic_fetch_add_explicit(
        &yaza_rt_v1_process_allocation_shards[yaza_rt_v1_allocation_shard_index].allocation_count,
        1, memory_order_relaxed);
#endif
}

static int yaza_rt_v1_reserve_process_bytes(uint64_t desired, uint64_t minimum, uint64_t *reserved) {
    uint64_t current = atomic_load_explicit(&yaza_rt_v1_process_reserved, memory_order_relaxed);
    for (;;) {
        if (current > yaza_rt_v1_config.process_limit_bytes) {
            return 0;
        }
        const uint64_t available = yaza_rt_v1_config.process_limit_bytes - current;
        const uint64_t capacity = desired < available ? desired : available;
        if (capacity < minimum) {
            return 0;
        }
        uint64_t expected = current;
        if (atomic_compare_exchange_weak_explicit(&yaza_rt_v1_process_reserved, &expected, current + capacity, memory_order_acq_rel, memory_order_relaxed)) {
            *reserved = capacity;
            yaza_rt_v1_record_process_reservation(current + capacity);
            return 1;
        }
        current = expected;
    }
}

static void yaza_rt_v1_update_peak(_Atomic uint64_t *peak_counter, uint64_t value) {
    uint64_t peak = atomic_load_explicit(peak_counter, memory_order_relaxed);
    while (value > peak && !atomic_compare_exchange_weak_explicit(peak_counter, &peak, value, memory_order_relaxed, memory_order_relaxed)) {
    }
}

static void yaza_rt_v1_record_process_use(YazaArenaV1 *arena, uint64_t size) {
#if defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_METRICS) || defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_USED)
    (void)arena;
    (void)size;
#elif defined(YAZA_RT_V1_BENCH_GLOBAL_PROCESS_LIVE_METRICS)
    (void)arena;
    const uint64_t used = atomic_fetch_add_explicit(&yaza_rt_v1_process_used, size, memory_order_relaxed) + size;
#if !defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_PEAK_METRICS)
    yaza_rt_v1_update_peak(&yaza_rt_v1_process_peak_used, used);
#else
    (void)used;
#endif
#else
    YazaRtV1LiveMetricShard *shard = &yaza_rt_v1_process_live_metric_shards[arena->root->process_metric_shard];
    const uint64_t used = atomic_fetch_add_explicit(&shard->used, size, memory_order_relaxed) + size;
#if !defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_PEAK_METRICS)
    yaza_rt_v1_update_peak(&shard->peak_used, used);
#else
    (void)used;
#endif
#endif
}

static void yaza_rt_v1_record_process_storage_use(YazaArenaV1 *arena, uint64_t size) {
#if defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_METRICS) || defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_STORAGE)
    (void)arena;
    (void)size;
#elif defined(YAZA_RT_V1_BENCH_GLOBAL_PROCESS_LIVE_METRICS)
    (void)arena;
    const uint64_t used = atomic_fetch_add_explicit(&yaza_rt_v1_process_storage_used, size, memory_order_relaxed) + size;
#if !defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_PEAK_METRICS)
    yaza_rt_v1_update_peak(&yaza_rt_v1_process_peak_storage_used, used);
#else
    (void)used;
#endif
#else
    YazaRtV1LiveMetricShard *shard = &yaza_rt_v1_process_live_metric_shards[arena->root->process_metric_shard];
    const uint64_t used = atomic_fetch_add_explicit(&shard->storage_used, size, memory_order_relaxed) + size;
#if !defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_PEAK_METRICS)
    yaza_rt_v1_update_peak(&shard->peak_storage_used, used);
#else
    (void)used;
#endif
#endif
}

static void yaza_rt_v1_reclaim_process_use(YazaArenaV1 *arena, uint64_t size) {
#if defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_METRICS) || defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_USED)
    (void)arena;
    (void)size;
#elif defined(YAZA_RT_V1_BENCH_GLOBAL_PROCESS_LIVE_METRICS)
    (void)arena;
    atomic_fetch_sub_explicit(&yaza_rt_v1_process_used, size, memory_order_relaxed);
#else
    YazaRtV1LiveMetricShard *shard = &yaza_rt_v1_process_live_metric_shards[arena->root->process_metric_shard];
    atomic_fetch_sub_explicit(&shard->used, size, memory_order_relaxed);
#endif
}

static void yaza_rt_v1_reclaim_process_storage(YazaArenaV1 *arena, uint64_t size) {
#if defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_METRICS) || defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_STORAGE)
    (void)arena;
    (void)size;
#elif defined(YAZA_RT_V1_BENCH_GLOBAL_PROCESS_LIVE_METRICS)
    (void)arena;
    atomic_fetch_sub_explicit(&yaza_rt_v1_process_storage_used, size, memory_order_relaxed);
#else
    YazaRtV1LiveMetricShard *shard = &yaza_rt_v1_process_live_metric_shards[arena->root->process_metric_shard];
    atomic_fetch_sub_explicit(&shard->storage_used, size, memory_order_relaxed);
#endif
}

#if !defined(YAZA_RT_V1_BENCH_GLOBAL_PROCESS_LIVE_METRICS) && !defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_METRICS)
enum {
    YAZA_RT_V1_METRIC_USED,
    YAZA_RT_V1_METRIC_STORAGE_USED,
    YAZA_RT_V1_METRIC_PEAK_USED,
    YAZA_RT_V1_METRIC_PEAK_STORAGE_USED,
};

static uint64_t yaza_rt_v1_sum_process_metric(unsigned int metric) {
    uint64_t total = 0;
    for (uint64_t index = 0; index < YAZA_RT_V1_PROCESS_METRIC_SHARDS; ++index) {
        const YazaRtV1LiveMetricShard *shard = &yaza_rt_v1_process_live_metric_shards[index];
        uint64_t value = 0;
        switch (metric) {
            case YAZA_RT_V1_METRIC_USED:
                value = atomic_load_explicit(&shard->used, memory_order_relaxed);
                break;
            case YAZA_RT_V1_METRIC_STORAGE_USED:
                value = atomic_load_explicit(&shard->storage_used, memory_order_relaxed);
                break;
            case YAZA_RT_V1_METRIC_PEAK_USED:
                value = atomic_load_explicit(&shard->peak_used, memory_order_relaxed);
                break;
            case YAZA_RT_V1_METRIC_PEAK_STORAGE_USED:
                value = atomic_load_explicit(&shard->peak_storage_used, memory_order_relaxed);
                break;
            default:
                return 0;
        }
        if (value > UINT64_MAX - total) {
            return UINT64_MAX;
        }
        total += value;
    }
    return total;
}
#endif

static void yaza_rt_v1_record_process_reservation(uint64_t reserved) {
    yaza_rt_v1_update_peak(&yaza_rt_v1_process_peak_reserved, reserved);
}

static int yaza_rt_v1_acquire_scope_slot(void) {
    uint64_t current = atomic_load_explicit(&yaza_rt_v1_process_scope_count, memory_order_relaxed);
    for (;;) {
        if (current >= yaza_rt_v1_config.max_live_scopes) {
            return 0;
        }
        if (atomic_compare_exchange_weak_explicit(&yaza_rt_v1_process_scope_count, &current, current + 1, memory_order_acq_rel, memory_order_relaxed)) {
            return 1;
        }
    }
}

static void yaza_rt_v1_lock(_Atomic bool *lock) {
    for (;;) {
        bool unlocked = false;
        if (atomic_compare_exchange_weak_explicit(lock, &unlocked, true, memory_order_acquire, memory_order_relaxed)) {
            return;
        }
        while (atomic_load_explicit(lock, memory_order_relaxed)) {
        }
    }
}

static void yaza_rt_v1_unlock(_Atomic bool *lock) {
    atomic_store_explicit(lock, false, memory_order_release);
}

static void yaza_rt_v1_tree_lock(YazaArenaV1 *root) {
    yaza_rt_v1_lock(&root->lock);
}

static void yaza_rt_v1_tree_unlock(YazaArenaV1 *root) {
    yaza_rt_v1_unlock(&root->lock);
}

static void yaza_rt_v1_saturating_increment(uint64_t *value) {
    if (*value != UINT64_MAX) {
        *value += 1;
    }
}

/* Caller holds the root tree lock. Attribute failed allocations to every
   active ancestor cycle so a request snapshot includes its child scopes. */
static void yaza_rt_v1_record_allocation_failure_locked(YazaArenaV1 *arena) {
    atomic_fetch_add_explicit(&yaza_rt_v1_process_allocation_failures, 1, memory_order_relaxed);
    for (YazaArenaV1 *scope = arena; scope != NULL; scope = scope->parent) {
        if (scope->scope_active) {
            yaza_rt_v1_saturating_increment(&scope->cycle_allocation_failures);
        }
    }
}

static void yaza_rt_v1_record_allocation_failure(YazaArenaV1 *arena) {
    if (arena == NULL) {
        atomic_fetch_add_explicit(&yaza_rt_v1_process_allocation_failures, 1, memory_order_relaxed);
        return;
    }
    YazaArenaV1 *root = arena->root;
    yaza_rt_v1_tree_lock(root);
    yaza_rt_v1_record_allocation_failure_locked(arena);
    yaza_rt_v1_tree_unlock(root);
}

static void yaza_rt_v1_free_chunks(YazaRtV1Chunk *chunk) {
    while (chunk != NULL) {
        YazaRtV1Chunk *next = chunk->next;
        free(chunk);
        chunk = next;
    }
}

static void yaza_rt_v1_cleanup_async_resources(void);

static void yaza_rt_v1_destroy_program_arena(void) {
    yaza_rt_v1_cleanup_async_resources();
    const uint64_t reclaimed_reserved = yaza_rt_v1_program_arena.local_reserved;
    const uint64_t reclaimed_used = yaza_rt_v1_program_arena.local_used;
    const uint64_t reclaimed_storage_used = yaza_rt_v1_program_arena.local_storage_used;
    yaza_rt_v1_free_chunks(yaza_rt_v1_program_arena.head);
    yaza_rt_v1_program_arena.head = NULL;
    yaza_rt_v1_program_arena.tail = NULL;
    yaza_rt_v1_program_arena.local_reserved = 0;
    yaza_rt_v1_program_arena.local_used = 0;
    yaza_rt_v1_program_arena.local_storage_used = 0;
    yaza_rt_v1_program_arena.subtree_reserved = 0;
    yaza_rt_v1_program_arena.subtree_used = 0;
    yaza_rt_v1_program_arena.subtree_storage_used = 0;
    atomic_fetch_sub_explicit(&yaza_rt_v1_process_reserved, reclaimed_reserved, memory_order_relaxed);
    yaza_rt_v1_reclaim_process_use(&yaza_rt_v1_program_arena, reclaimed_used);
    yaza_rt_v1_reclaim_process_storage(&yaza_rt_v1_program_arena, reclaimed_storage_used);
}

static int yaza_rt_v1_config_is_valid(const YazaRuntimeConfigV1 *config);

static int yaza_rt_v1_parse_unsigned_env(const char *name, uint64_t *out_value) {
    const char *text = getenv(name);
    if (text == NULL) {
        return 1;
    }
    if (text[0] == '\0') {
        return 0;
    }
    uint64_t value = 0;
    for (const unsigned char *cursor = (const unsigned char *)text; *cursor != '\0'; ++cursor) {
        if (*cursor < '0' || *cursor > '9') {
            return 0;
        }
        const uint64_t digit = (uint64_t)(*cursor - '0');
        if (value > (UINT64_MAX - digit) / 10) {
            return 0;
        }
        value = value * 10 + digit;
    }
    *out_value = value;
    return 1;
}

static int yaza_rt_v1_load_environment_config(void) {
    YazaRuntimeConfigV1 config = yaza_rt_v1_config;
    config.struct_size = sizeof(YazaRuntimeConfigV1);
    const int process_limit_overridden = getenv("YAZA_RT_V1_PROCESS_LIMIT_BYTES") != NULL;
    const int list_vector_limit_overridden = getenv("YAZA_RT_V1_MAX_LIST_VECTOR_BYTES") != NULL;
    if (!yaza_rt_v1_parse_unsigned_env("YAZA_RT_V1_PROCESS_LIMIT_BYTES", &config.process_limit_bytes) ||
        !yaza_rt_v1_parse_unsigned_env("YAZA_RT_V1_PREFERRED_CHUNK_BYTES", &config.preferred_chunk_bytes) ||
        !yaza_rt_v1_parse_unsigned_env("YAZA_RT_V1_MAX_LIVE_SCOPES", &config.max_live_scopes) ||
        !yaza_rt_v1_parse_unsigned_env("YAZA_RT_V1_MAX_LIST_ITEMS", &config.max_list_items) ||
        !yaza_rt_v1_parse_unsigned_env("YAZA_RT_V1_MAX_LIST_VECTOR_BYTES", &config.max_list_vector_bytes)) {
        fputs("YAZA runtime v1: invalid startup memory profile in YAZA_RT_V1_* environment variables\n", stderr);
        return 0;
    }
    if (process_limit_overridden && !list_vector_limit_overridden && config.max_list_vector_bytes > config.process_limit_bytes) {
        config.max_list_vector_bytes = config.process_limit_bytes;
    }
    if (!yaza_rt_v1_config_is_valid(&config)) {
        fputs("YAZA runtime v1: invalid startup memory profile in YAZA_RT_V1_* environment variables\n", stderr);
        return 0;
    }
    yaza_rt_v1_config = config;
    yaza_rt_v1_program_arena.limit = config.process_limit_bytes;
    return 1;
}

void yaza_rt_v1_init(void) {
    if (!atomic_load_explicit(&yaza_rt_v1_initialized, memory_order_acquire)) {
        while (atomic_flag_test_and_set_explicit(&yaza_rt_v1_init_lock, memory_order_acquire)) {
        }
        if (!atomic_load_explicit(&yaza_rt_v1_initialized, memory_order_relaxed)) {
            if (!yaza_rt_v1_config_set_explicitly && !yaza_rt_v1_load_environment_config()) {
                atomic_flag_clear_explicit(&yaza_rt_v1_init_lock, memory_order_release);
                abort();
            }
            yaza_rt_v1_program_arena.root = &yaza_rt_v1_program_arena;
            atomic_store_explicit(&yaza_rt_v1_initialized, 1, memory_order_release);
#if defined(SIGPIPE)
            (void)signal(SIGPIPE, SIG_IGN);
#endif
        }
        atomic_flag_clear_explicit(&yaza_rt_v1_init_lock, memory_order_release);
    }
    if (atomic_load_explicit(&yaza_rt_v1_exit_registered, memory_order_acquire)) {
        return;
    }
    while (atomic_flag_test_and_set_explicit(&yaza_rt_v1_exit_registration_lock, memory_order_acquire)) {
    }
    if (!atomic_load_explicit(&yaza_rt_v1_exit_registered, memory_order_relaxed)) {
        if (atexit(yaza_rt_v1_destroy_program_arena) != 0) {
            fputs("YAZA runtime v1: cannot register program arena cleanup\n", stderr);
            abort();
        }
        atomic_store_explicit(&yaza_rt_v1_exit_registered, 1, memory_order_release);
    }
    atomic_flag_clear_explicit(&yaza_rt_v1_exit_registration_lock, memory_order_release);
}

static int yaza_rt_v1_config_is_valid(const YazaRuntimeConfigV1 *config) {
    if (config == NULL || config->abi_version != YAZA_RUNTIME_CONFIG_V1_ABI_VERSION || config->struct_size < sizeof(YazaRuntimeConfigV1)) {
        return 0;
    }
    if (config->process_limit_bytes < YAZA_RT_V1_MIN_PROCESS_LIMIT || config->process_limit_bytes > (uint64_t)SIZE_MAX - sizeof(YazaRtV1Chunk)) {
        return 0;
    }
    if (config->preferred_chunk_bytes < YAZA_RT_V1_MIN_CHUNK_SIZE || config->preferred_chunk_bytes > YAZA_RT_V1_MAX_CHUNK_SIZE || config->preferred_chunk_bytes > config->process_limit_bytes || config->preferred_chunk_bytes % _Alignof(max_align_t) != 0) {
        return 0;
    }
    if (config->max_live_scopes == 0 || config->max_live_scopes > YAZA_RT_V1_MAX_LIVE_SCOPES) {
        return 0;
    }
    if (config->max_list_items == 0 || config->max_list_items > INT64_MAX) {
        return 0;
    }
    if (config->max_list_vector_bytes < sizeof(uint64_t) || config->max_list_vector_bytes > SIZE_MAX || config->max_list_vector_bytes > config->process_limit_bytes) {
        return 0;
    }
    return 1;
}

int yaza_rt_v1_configure(const YazaRuntimeConfigV1 *config) {
    if (!yaza_rt_v1_config_is_valid(config)) {
        return 0;
    }
    while (atomic_flag_test_and_set_explicit(&yaza_rt_v1_init_lock, memory_order_acquire)) {
    }
    if (atomic_load_explicit(&yaza_rt_v1_initialized, memory_order_relaxed) ||
        atomic_load_explicit(&yaza_rt_v1_process_reserved, memory_order_relaxed) != 0 ||
        atomic_load_explicit(&yaza_rt_v1_process_scope_count, memory_order_relaxed) != 0) {
        atomic_flag_clear_explicit(&yaza_rt_v1_init_lock, memory_order_release);
        return 0;
    }
    yaza_rt_v1_config = *config;
    yaza_rt_v1_config.struct_size = sizeof(YazaRuntimeConfigV1);
    yaza_rt_v1_program_arena.limit = config->process_limit_bytes;
    yaza_rt_v1_config_set_explicitly = 1;
    atomic_flag_clear_explicit(&yaza_rt_v1_init_lock, memory_order_release);
    return 1;
}

int yaza_rt_v1_get_config(YazaRuntimeConfigV1 *out_config) {
    if (out_config == NULL) {
        return 0;
    }
    yaza_rt_v1_init();
    *out_config = yaza_rt_v1_config;
    return 1;
}

YazaArenaV1 *yaza_arena_v1_create(uint64_t limit_bytes) {
    yaza_rt_v1_init();
    if (limit_bytes == 0 || limit_bytes > yaza_rt_v1_config.process_limit_bytes) {
        return NULL;
    }
    if (!yaza_rt_v1_acquire_scope_slot()) {
        yaza_rt_v1_record_allocation_failure(yaza_rt_v1_current_arena);
        return NULL;
    }
    YazaArenaV1 *arena = (YazaArenaV1 *)yaza_rt_v1_system_calloc(1, sizeof(YazaArenaV1));
    if (arena == NULL) {
        atomic_fetch_sub_explicit(&yaza_rt_v1_process_scope_count, 1, memory_order_relaxed);
        yaza_rt_v1_record_allocation_failure(yaza_rt_v1_current_arena);
        return NULL;
    }
    atomic_init(&arena->lock, false);
    arena->root = arena;
#if !defined(YAZA_RT_V1_BENCH_GLOBAL_PROCESS_LIVE_METRICS) && !defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_METRICS)
    const uint64_t next_metric_shard = atomic_fetch_add_explicit(&yaza_rt_v1_next_process_metric_shard, 1, memory_order_relaxed);
    arena->process_metric_shard = (uint32_t)(next_metric_shard % YAZA_RT_V1_PROCESS_METRIC_SHARDS);
#else
    arena->process_metric_shard = 0;
#endif
    arena->limit = limit_bytes;
    arena->generation = 1;
    arena->heap_owned = 1;
    return arena;
}

YazaArenaV1 *yaza_arena_v1_create_child(YazaArenaV1 *parent, uint64_t limit_bytes) {
    if (parent == NULL || limit_bytes == 0) {
        return NULL;
    }
    if (!yaza_rt_v1_acquire_scope_slot()) {
        yaza_rt_v1_record_allocation_failure(parent);
        return NULL;
    }
    if (!yaza_arena_v1_retain(parent)) {
        atomic_fetch_sub_explicit(&yaza_rt_v1_process_scope_count, 1, memory_order_relaxed);
        return NULL;
    }
    YazaArenaV1 *arena = (YazaArenaV1 *)yaza_rt_v1_system_calloc(1, sizeof(YazaArenaV1));
    if (arena == NULL) {
        yaza_arena_v1_release(parent);
        atomic_fetch_sub_explicit(&yaza_rt_v1_process_scope_count, 1, memory_order_relaxed);
        yaza_rt_v1_record_allocation_failure(parent);
        return NULL;
    }
    YazaArenaV1 *root = parent->root;
    yaza_rt_v1_tree_lock(root);
    if (parent->destroyed || limit_bytes > parent->limit || root->descendant_count >= yaza_rt_v1_config.max_live_scopes) {
        yaza_rt_v1_record_allocation_failure_locked(parent);
        yaza_rt_v1_tree_unlock(root);
        free(arena);
        yaza_arena_v1_release(parent);
        atomic_fetch_sub_explicit(&yaza_rt_v1_process_scope_count, 1, memory_order_relaxed);
        return NULL;
    }
    arena->root = root;
    arena->parent = parent;
    arena->limit = limit_bytes;
    arena->generation = 1;
    arena->heap_owned = 1;
    parent->child_count += 1;
    root->descendant_count += 1;
    yaza_rt_v1_tree_unlock(root);
    yaza_arena_v1_release(parent);
    return arena;
}

static YazaArenaV1 *yaza_arena_v1_create_child_inplace(YazaArenaV1 *parent, uint64_t limit_bytes, YazaArenaV1 *arena) {
    if (parent == NULL || arena == NULL || limit_bytes == 0) {
        return NULL;
    }
    if (!yaza_rt_v1_acquire_scope_slot()) {
        yaza_rt_v1_record_allocation_failure(parent);
        return NULL;
    }
    YazaArenaV1 *root = parent->root;
    yaza_rt_v1_tree_lock(root);
    if (parent->destroyed || limit_bytes > parent->limit || root->descendant_count >= yaza_rt_v1_config.max_live_scopes) {
        yaza_rt_v1_record_allocation_failure_locked(parent);
        yaza_rt_v1_tree_unlock(root);
        atomic_fetch_sub_explicit(&yaza_rt_v1_process_scope_count, 1, memory_order_relaxed);
        return NULL;
    }
    memset(arena, 0, sizeof(*arena));
    atomic_init(&arena->lock, false);
    arena->root = root;
    arena->parent = parent;
    arena->limit = limit_bytes;
    arena->generation = 1;
    arena->heap_owned = 0;
    parent->child_count += 1;
    root->descendant_count += 1;
    yaza_rt_v1_tree_unlock(root);
    return arena;
}

void *yaza_arena_v1_alloc(YazaArenaV1 *arena, uint64_t requested_size) {
    if (arena == NULL) {
        yaza_rt_v1_record_allocation_failure(NULL);
        return NULL;
    }
    const uint64_t size = requested_size == 0 ? 1 : requested_size;
    const uint64_t alignment = (uint64_t)_Alignof(max_align_t);
    YazaArenaV1 *root = arena->root;
    yaza_rt_v1_tree_lock(root);
    if (arena->destroyed || size > arena->limit ||
        (arena->scope_active && !yaza_rt_v1_context_contains(arena->scope_context))) {
        yaza_rt_v1_record_allocation_failure_locked(arena);
        yaza_rt_v1_tree_unlock(root);
        return NULL;
    }

    YazaRtV1Chunk *chunk = arena->tail;
    uint64_t offset = 0;
    if (chunk != NULL) {
        offset = chunk->used;
        const uint64_t remainder = offset % alignment;
        if (remainder != 0) {
            const uint64_t padding = alignment - remainder;
            if (offset > UINT64_MAX - padding) {
                offset = UINT64_MAX;
            } else {
                offset += padding;
            }
        }
    }
    if (chunk == NULL || offset > chunk->capacity || size > chunk->capacity - offset) {
        uint64_t available = arena->limit - arena->local_reserved;
        for (YazaArenaV1 *scope = arena; scope != NULL; scope = scope->parent) {
            const uint64_t scope_available = scope->limit - scope->subtree_reserved;
            if (scope_available < available) {
                available = scope_available;
            }
        }
        uint64_t desired = size > yaza_rt_v1_config.preferred_chunk_bytes ? size : yaza_rt_v1_config.preferred_chunk_bytes;
        if (desired > available) {
            desired = available;
        }
        if (desired < size || desired > SIZE_MAX - sizeof(YazaRtV1Chunk)) {
            yaza_rt_v1_record_allocation_failure_locked(arena);
            yaza_rt_v1_tree_unlock(root);
            return NULL;
        }
        uint64_t capacity = 0;
        if (!yaza_rt_v1_reserve_process_bytes(desired, size, &capacity) || capacity > SIZE_MAX - sizeof(YazaRtV1Chunk)) {
            yaza_rt_v1_record_allocation_failure_locked(arena);
            yaza_rt_v1_tree_unlock(root);
            return NULL;
        }
        chunk = (YazaRtV1Chunk *)yaza_rt_v1_system_malloc(sizeof(YazaRtV1Chunk) + (size_t)capacity);
        if (chunk == NULL) {
            atomic_fetch_sub_explicit(&yaza_rt_v1_process_reserved, capacity, memory_order_relaxed);
            yaza_rt_v1_record_allocation_failure_locked(arena);
            yaza_rt_v1_tree_unlock(root);
            return NULL;
        }
        chunk->next = NULL;
        chunk->capacity = capacity;
        chunk->used = 0;
        if (arena->tail == NULL) {
            arena->head = chunk;
        } else {
            arena->tail->next = chunk;
        }
        arena->tail = chunk;
        arena->local_reserved += capacity;
        for (YazaArenaV1 *scope = arena; scope != NULL; scope = scope->parent) {
            scope->subtree_reserved += capacity;
            if (scope->subtree_reserved > scope->peak_subtree_reserved) {
                scope->peak_subtree_reserved = scope->subtree_reserved;
            }
        }
        offset = 0;
    }

    const uint64_t previous_chunk_used = chunk->used;
    const uint64_t new_chunk_used = offset + size;
    const uint64_t storage_increment = new_chunk_used - previous_chunk_used;
    void *result = chunk->data + offset;
    yaza_rt_v1_asan_unpoison(result, (size_t)size);
    chunk->used = new_chunk_used;
    arena->local_used += size;
    arena->local_storage_used += storage_increment;
    arena->allocations += 1;
    yaza_rt_v1_record_process_allocation();
    yaza_rt_v1_record_process_use(arena, size);
    yaza_rt_v1_record_process_storage_use(arena, storage_increment);
    for (YazaArenaV1 *scope = arena; scope != NULL; scope = scope->parent) {
        if (scope->scope_active) {
            yaza_rt_v1_saturating_increment(&scope->cycle_successful_allocations);
        }
        scope->subtree_used += size;
        if (scope->subtree_used > scope->peak_subtree_used) {
            scope->peak_subtree_used = scope->subtree_used;
        }
        scope->subtree_storage_used += storage_increment;
        if (scope->subtree_storage_used > scope->peak_subtree_storage_used) {
            scope->peak_subtree_storage_used = scope->subtree_storage_used;
        }
    }
    yaza_rt_v1_tree_unlock(root);
    return result;
}

int yaza_arena_v1_retain(YazaArenaV1 *arena) {
    if (arena == NULL) {
        return 0;
    }
    YazaArenaV1 *root = arena->root;
    yaza_rt_v1_tree_lock(root);
    int retained = !arena->destroyed && arena->active_leases != UINT64_MAX && arena->explicit_leases != UINT64_MAX;
    if (retained) {
        arena->active_leases += 1;
        arena->explicit_leases += 1;
    }
    yaza_rt_v1_tree_unlock(root);
    return retained;
}

static int yaza_rt_v1_retain_for_current_access(YazaArenaV1 *arena) {
    if (arena == NULL) return 0;
    /* Program-arena storage lives until process exit, so list reads need no lease. */
    if (arena == &yaza_rt_v1_program_arena) {
        return 1;
    }
    YazaArenaV1 *root = arena->root;
    yaza_rt_v1_tree_lock(root);
    const int retained = !arena->destroyed && arena->active_leases != UINT64_MAX &&
        (!arena->scope_active || yaza_rt_v1_context_contains(arena->scope_context));
    if (retained) arena->active_leases += 1;
    yaza_rt_v1_tree_unlock(root);
    return retained;
}

static int yaza_rt_v1_release_current_access(YazaArenaV1 *arena) {
    if (arena == NULL) return 0;
    /* Matches retain: the process-lifetime arena has no reset/destroy transition. */
    if (arena == &yaza_rt_v1_program_arena) {
        return 1;
    }
    YazaArenaV1 *root = arena->root;
    yaza_rt_v1_tree_lock(root);
    const uint64_t non_access_leases = arena->active_contexts + arena->explicit_leases;
    const int released = !arena->destroyed && arena->active_leases > non_access_leases;
    if (released) arena->active_leases -= 1;
    yaza_rt_v1_tree_unlock(root);
    return released;
}

int yaza_arena_v1_release(YazaArenaV1 *arena) {
    if (arena == NULL) {
        return 0;
    }
    YazaArenaV1 *root = arena->root;
    yaza_rt_v1_tree_lock(root);
    /* Only a matching retain() may be released; context and in-flight access leases are private. */
    int released = !arena->destroyed && arena->explicit_leases > 0;
    if (released) {
        arena->active_leases -= 1;
        arena->explicit_leases -= 1;
    }
    yaza_rt_v1_tree_unlock(root);
    return released;
}

int yaza_arena_v1_enter(YazaArenaV1 *arena, YazaArenaContextV1 *context) {
    if (arena == NULL || context == NULL) {
        return 0;
    }
    for (YazaArenaContextV1 *active = yaza_rt_v1_current_context; active != NULL; active = active->previous_context) {
        if (active == context) {
            return 0;
        }
    }
    YazaArenaV1 *root = arena->root;
    yaza_rt_v1_tree_lock(root);
    if (arena->destroyed || arena->scope_active || arena->active_leases == UINT64_MAX || arena->active_contexts == UINT64_MAX) {
        yaza_rt_v1_tree_unlock(root);
        return 0;
    }
    arena->active_leases += 1;
    arena->active_contexts += 1;
    const uint64_t generation = arena->generation;
    yaza_rt_v1_tree_unlock(root);
    context->previous = yaza_rt_v1_current_arena;
    context->previous_context = yaza_rt_v1_current_context;
    context->entered = arena;
    context->generation = generation;
    context->active = 1;
    yaza_rt_v1_current_arena = arena;
    yaza_rt_v1_current_context = context;
    return 1;
}

int yaza_arena_v1_leave(YazaArenaContextV1 *context) {
    if (context == NULL || !context->active || yaza_rt_v1_current_context != context || yaza_rt_v1_current_arena != context->entered) {
        return 0;
    }
    YazaArenaV1 *arena = context->entered;
    YazaArenaV1 *root = arena->root;
    yaza_rt_v1_tree_lock(root);
    const int released = !arena->destroyed && !(arena->scope_active && arena->scope_context == context) &&
        arena->generation == context->generation && arena->active_leases > 0 && arena->active_contexts > 0;
    if (released) {
        arena->active_leases -= 1;
        arena->active_contexts -= 1;
    }
    yaza_rt_v1_tree_unlock(root);
    if (!released) {
        return 0;
    }
    yaza_rt_v1_current_arena = context->previous;
    yaza_rt_v1_current_context = context->previous_context;
    context->active = 0;
    context->previous = NULL;
    context->previous_context = NULL;
    context->entered = NULL;
    context->generation = 0;
    return 1;
}

static int yaza_rt_v1_reset_locked(YazaArenaV1 *arena, uint64_t expected_leases, YazaRtV1Chunk **discard_out, uint64_t *reclaimed_reserved_out, uint64_t *reclaimed_used_out, uint64_t *reclaimed_storage_used_out) {
    if (arena->destroyed || arena->child_count != 0 || arena->active_leases != expected_leases || arena->generation == UINT64_MAX) {
        return 0;
    }
    YazaRtV1Chunk *keep = arena->head;
    if (keep != NULL && keep->capacity > yaza_rt_v1_config.preferred_chunk_bytes) {
        keep = NULL;
    }
    YazaRtV1Chunk *discard = arena->head;
    uint64_t kept_capacity = 0;
    if (keep != NULL) {
        discard = keep->next;
        keep->next = NULL;
        yaza_rt_v1_asan_poison(keep->data, (size_t)keep->used);
        keep->used = 0;
        kept_capacity = keep->capacity;
        arena->head = keep;
        arena->tail = keep;
    } else {
        arena->head = NULL;
        arena->tail = NULL;
    }
    const uint64_t reclaimed = arena->local_reserved - kept_capacity;
    const uint64_t reclaimed_used = arena->local_used;
    const uint64_t reclaimed_storage_used = arena->local_storage_used;
    arena->local_reserved = kept_capacity;
    arena->local_used = 0;
    arena->local_storage_used = 0;
    arena->allocations = 0;
    arena->cycle_successful_allocations = 0;
    arena->cycle_allocation_failures = 0;
    arena->peak_subtree_used = 0;
    arena->peak_subtree_storage_used = 0;
    arena->generation += 1;
    for (YazaArenaV1 *scope = arena; scope != NULL; scope = scope->parent) {
        scope->subtree_reserved -= reclaimed;
        scope->subtree_used -= reclaimed_used;
        scope->subtree_storage_used -= reclaimed_storage_used;
    }
    arena->peak_subtree_reserved = arena->subtree_reserved;
    *discard_out = discard;
    *reclaimed_reserved_out = reclaimed;
    *reclaimed_used_out = reclaimed_used;
    *reclaimed_storage_used_out = reclaimed_storage_used;
    return 1;
}

static void yaza_rt_v1_finish_reset(YazaArenaV1 *arena, YazaRtV1Chunk *discard, uint64_t reclaimed_reserved, uint64_t reclaimed_used, uint64_t reclaimed_storage_used) {
    yaza_rt_v1_free_chunks(discard);
    atomic_fetch_sub_explicit(&yaza_rt_v1_process_reserved, reclaimed_reserved, memory_order_relaxed);
    yaza_rt_v1_reclaim_process_use(arena, reclaimed_used);
    yaza_rt_v1_reclaim_process_storage(arena, reclaimed_storage_used);
}

int yaza_arena_v1_scope_begin(YazaArenaV1 *arena, YazaArenaContextV1 *context) {
    if (arena == NULL || arena == &yaza_rt_v1_program_arena || context == NULL) {
        return 0;
    }
    for (YazaArenaContextV1 *active = yaza_rt_v1_current_context; active != NULL; active = active->previous_context) {
        if (active == context) {
            return 0;
        }
    }
    yaza_rt_v1_init();
    YazaArenaV1 *root = arena->root;
    YazaRtV1Chunk *discard = NULL;
    uint64_t reclaimed_reserved = 0;
    uint64_t reclaimed_used = 0;
    uint64_t reclaimed_storage_used = 0;
    yaza_rt_v1_tree_lock(root);
    if (arena->scope_active || arena->active_leases == UINT64_MAX || arena->active_contexts == UINT64_MAX ||
        !yaza_rt_v1_reset_locked(arena, 0, &discard, &reclaimed_reserved, &reclaimed_used, &reclaimed_storage_used)) {
        yaza_rt_v1_tree_unlock(root);
        return 0;
    }
    arena->active_leases += 1;
    arena->active_contexts += 1;
    arena->scope_active = 1;
    arena->scope_context = context;
    context->previous = yaza_rt_v1_current_arena;
    context->previous_context = yaza_rt_v1_current_context;
    context->entered = arena;
    context->generation = arena->generation;
    context->active = 1;
    yaza_rt_v1_current_arena = arena;
    yaza_rt_v1_current_context = context;
    yaza_rt_v1_tree_unlock(root);
    yaza_rt_v1_finish_reset(arena, discard, reclaimed_reserved, reclaimed_used, reclaimed_storage_used);
    return 1;
}

int yaza_arena_v1_scope_end(YazaArenaContextV1 *context) {
    if (context == NULL || !context->active || yaza_rt_v1_current_context != context || yaza_rt_v1_current_arena != context->entered) {
        return 0;
    }
    YazaArenaV1 *arena = context->entered;
    YazaArenaV1 *root = arena->root;
    YazaRtV1Chunk *discard = NULL;
    uint64_t reclaimed_reserved = 0;
    uint64_t reclaimed_used = 0;
    uint64_t reclaimed_storage_used = 0;
    yaza_rt_v1_tree_lock(root);
    if (!arena->scope_active || arena->scope_context != context || context->generation != arena->generation || arena->active_contexts != 1) {
        yaza_rt_v1_tree_unlock(root);
        return 0;
    }
    const uint64_t completed_generation = arena->generation;
    const uint64_t completed_allocations = arena->cycle_successful_allocations;
    const uint64_t completed_failures = arena->cycle_allocation_failures;
    const uint64_t completed_peak_used = arena->peak_subtree_used;
    const uint64_t completed_peak_storage_used = arena->peak_subtree_storage_used;
    const uint64_t completed_peak_reserved = arena->peak_subtree_reserved;
    if (!yaza_rt_v1_reset_locked(arena, 1, &discard, &reclaimed_reserved, &reclaimed_used, &reclaimed_storage_used)) {
        yaza_rt_v1_tree_unlock(root);
        return 0;
    }
    arena->last_cycle_generation = completed_generation;
    arena->last_cycle_successful_allocations = completed_allocations;
    arena->last_cycle_allocation_failures = completed_failures;
    arena->last_cycle_peak_used = completed_peak_used;
    arena->last_cycle_peak_storage_used = completed_peak_storage_used;
    arena->last_cycle_peak_reserved = completed_peak_reserved;
    arena->last_cycle_retained_reserved = arena->subtree_reserved;
    arena->active_leases = 0;
    arena->active_contexts = 0;
    arena->scope_active = 0;
    arena->scope_context = NULL;
    yaza_rt_v1_current_arena = context->previous;
    yaza_rt_v1_current_context = context->previous_context;
    context->active = 0;
    context->previous = NULL;
    context->previous_context = NULL;
    context->entered = NULL;
    context->generation = 0;
    yaza_rt_v1_tree_unlock(root);
    yaza_rt_v1_finish_reset(arena, discard, reclaimed_reserved, reclaimed_used, reclaimed_storage_used);
    return 1;
}

YazaArenaV1 *yaza_arena_v1_current(void) {
    yaza_rt_v1_init();
    if (yaza_rt_v1_current_arena != NULL) {
        return yaza_rt_v1_current_arena;
    }
    return &yaza_rt_v1_program_arena;
}

YazaArenaScopeV1 *yaza_rt_v1_scope_enter(uint64_t limit_bytes) {
    yaza_rt_v1_init();
    if (limit_bytes == 0) {
        return NULL;
    }
    YazaArenaV1 *parent = yaza_arena_v1_current();
    YazaArenaScopeV1 *scope = (YazaArenaScopeV1 *)yaza_rt_v1_system_calloc(1, sizeof(*scope));
    if (scope == NULL) {
        yaza_rt_v1_record_allocation_failure(parent);
        return NULL;
    }
    scope->arena = yaza_arena_v1_create_child(parent, limit_bytes);
    if (scope->arena == NULL) {
        free(scope);
        return NULL;
    }
    if (!yaza_arena_v1_scope_begin(scope->arena, &scope->context)) {
        if (!yaza_arena_v1_destroy(scope->arena)) {
            fprintf(stderr, "YAZA runtime v1: failed to clean up an unopened arena scope\n");
            abort();
        }
        free(scope);
        return NULL;
    }
    scope->context_active = 1;
    scope->heap_owned = 1;
    return scope;
}

YazaArenaScopeV1 *yaza_rt_v1_scope_enter_inplace(uint64_t limit_bytes, void *storage, uint64_t storage_bytes) {
    yaza_rt_v1_init();
    if (storage == NULL || storage_bytes < sizeof(YazaRtV1InlineScopeStorage) ||
        ((uintptr_t)storage % _Alignof(YazaRtV1InlineScopeStorage)) != 0 || limit_bytes == 0) {
        return NULL;
    }
    YazaRtV1InlineScopeStorage *inline_scope = (YazaRtV1InlineScopeStorage *)storage;
    memset(inline_scope, 0, sizeof(*inline_scope));
    YazaArenaScopeV1 *scope = &inline_scope->scope;
    YazaArenaV1 *parent = yaza_arena_v1_current();
    scope->arena = yaza_arena_v1_create_child_inplace(parent, limit_bytes, &inline_scope->arena);
    if (scope->arena == NULL) {
        return NULL;
    }
    if (!yaza_arena_v1_scope_begin(scope->arena, &scope->context)) {
        if (!yaza_arena_v1_destroy(scope->arena)) {
            fprintf(stderr, "YAZA runtime v1: failed to clean up an unopened inline arena scope\n");
            abort();
        }
        scope->arena = NULL;
        return NULL;
    }
    scope->context_active = 1;
    scope->heap_owned = 0;
    return scope;
}

int32_t yaza_rt_v1_scope_leave(YazaArenaScopeV1 *scope) {
    if (scope == NULL || scope->arena == NULL) return 0;
    if (scope->context_active) {
        if (!yaza_arena_v1_scope_end(&scope->context)) return 0;
        scope->context_active = 0;
    }
    if (!yaza_arena_v1_destroy(scope->arena)) return 0;
    scope->arena = NULL;
    if (scope->heap_owned) free(scope);
    return 1;
}

YazaArenaCycleV1 *yaza_arena_v1_cycle_create(YazaArenaV1 *parent, uint64_t limit_bytes) {
    if (parent == NULL || limit_bytes == 0) return NULL;
    YazaArenaCycleV1 *cycle = (YazaArenaCycleV1 *)yaza_rt_v1_system_calloc(1, sizeof(*cycle));
    if (cycle == NULL) {
        yaza_rt_v1_record_allocation_failure(parent);
        return NULL;
    }
    cycle->arena = yaza_arena_v1_create_child(parent, limit_bytes);
    if (cycle->arena == NULL) {
        free(cycle);
        return NULL;
    }
    return cycle;
}

int32_t yaza_arena_v1_cycle_begin(YazaArenaCycleV1 *cycle) {
    if (cycle == NULL || cycle->arena == NULL || cycle->active) return 0;
    if (!yaza_arena_v1_scope_begin(cycle->arena, &cycle->context)) return 0;
    cycle->active = 1;
    return 1;
}

int32_t yaza_arena_v1_cycle_end(YazaArenaCycleV1 *cycle, YazaArenaCycleMetricsV1 *out_metrics) {
    if (cycle == NULL || cycle->arena == NULL || !cycle->active || out_metrics == NULL || out_metrics->struct_size < sizeof(*out_metrics)) return 0;
    if (!yaza_arena_v1_scope_end(&cycle->context)) return 0;
    cycle->active = 0;
    return yaza_arena_v1_last_cycle_metrics(cycle->arena, out_metrics);
}

int32_t yaza_arena_v1_cycle_last_metrics(YazaArenaCycleV1 *cycle, YazaArenaCycleMetricsV1 *out_metrics) {
    if (cycle == NULL || cycle->arena == NULL) return 0;
    return yaza_arena_v1_last_cycle_metrics(cycle->arena, out_metrics);
}

int32_t yaza_arena_v1_cycle_destroy(YazaArenaCycleV1 *cycle) {
    if (cycle == NULL || cycle->arena == NULL || cycle->active) return 0;
    if (!yaza_arena_v1_destroy(cycle->arena)) return 0;
    cycle->arena = NULL;
    free(cycle);
    return 1;
}

static int yaza_rt_v1_module_request_scope_active(void) {
    YazaArenaV1 *arena = yaza_rt_v1_current_arena;
    YazaArenaContextV1 *context = yaza_rt_v1_current_context;
    if (arena == NULL || context == NULL) return 0;
    YazaArenaV1 *root = arena->root == NULL ? arena : arena->root;
    yaza_rt_v1_lock(&root->lock);
    const int active = !arena->destroyed && arena->scope_active && arena->scope_context == context &&
        context->active && context->entered == arena;
    yaza_rt_v1_unlock(&root->lock);
    return active;
}

uint64_t yaza_rt_v1_current_arena_used(void) {
    return yaza_arena_v1_used(yaza_arena_v1_current());
}

uint64_t yaza_rt_v1_current_arena_storage_used(void) {
    return yaza_arena_v1_storage_used(yaza_arena_v1_current());
}

uint64_t yaza_rt_v1_current_arena_reserved(void) {
    return yaza_arena_v1_reserved(yaza_arena_v1_current());
}

uint64_t yaza_rt_v1_current_arena_peak_used(void) {
    return yaza_arena_v1_peak_used(yaza_arena_v1_current());
}

uint64_t yaza_rt_v1_current_arena_peak_storage_used(void) {
    return yaza_arena_v1_peak_storage_used(yaza_arena_v1_current());
}

uint64_t yaza_rt_v1_current_arena_peak_reserved(void) {
    return yaza_arena_v1_peak_reserved(yaza_arena_v1_current());
}

uint64_t yaza_rt_v1_current_arena_allocation_count(void) {
    return yaza_arena_v1_allocation_count(yaza_arena_v1_current());
}

uint64_t yaza_rt_v1_current_arena_generation(void) {
    return yaza_arena_v1_generation(yaza_arena_v1_current());
}

uint64_t yaza_rt_v1_live_scope_count(void) {
    return yaza_rt_v1_arena_scope_count();
}

int yaza_arena_v1_reset(YazaArenaV1 *arena) {
    if (arena == NULL || arena == &yaza_rt_v1_program_arena) {
        return 0;
    }
    YazaArenaV1 *root = arena->root;
    YazaRtV1Chunk *discard = NULL;
    uint64_t reclaimed_reserved = 0;
    uint64_t reclaimed_used = 0;
    uint64_t reclaimed_storage_used = 0;
    yaza_rt_v1_tree_lock(root);
    if (arena->scope_active || !yaza_rt_v1_reset_locked(arena, 0, &discard, &reclaimed_reserved, &reclaimed_used, &reclaimed_storage_used)) {
        yaza_rt_v1_tree_unlock(root);
        return 0;
    }
    yaza_rt_v1_tree_unlock(root);
    yaza_rt_v1_finish_reset(arena, discard, reclaimed_reserved, reclaimed_used, reclaimed_storage_used);
    return 1;
}

int yaza_arena_v1_destroy(YazaArenaV1 *arena) {
    if (arena == NULL || arena == &yaza_rt_v1_program_arena) {
        return 0;
    }
    YazaArenaV1 *root = arena->root;
    yaza_rt_v1_tree_lock(root);
    if (arena->destroyed || arena->scope_active || arena->child_count != 0 || arena->active_leases != 0) {
        yaza_rt_v1_tree_unlock(root);
        return 0;
    }
    arena->destroyed = 1;
    const int heap_owned = arena->heap_owned;
    YazaRtV1Chunk *chunks = arena->head;
    const uint64_t reclaimed = arena->local_reserved;
    const uint64_t reclaimed_used = arena->local_used;
    const uint64_t reclaimed_storage_used = arena->local_storage_used;
    YazaArenaV1 *parent = arena->parent;
    for (YazaArenaV1 *scope = parent; scope != NULL; scope = scope->parent) {
        scope->subtree_reserved -= reclaimed;
        scope->subtree_used -= reclaimed_used;
        scope->subtree_storage_used -= reclaimed_storage_used;
    }
    if (parent != NULL) {
        parent->child_count -= 1;
        root->descendant_count -= 1;
    }
    yaza_rt_v1_tree_unlock(root);
    yaza_rt_v1_free_chunks(chunks);
    atomic_fetch_sub_explicit(&yaza_rt_v1_process_reserved, reclaimed, memory_order_relaxed);
    yaza_rt_v1_reclaim_process_use(arena, reclaimed_used);
    yaza_rt_v1_reclaim_process_storage(arena, reclaimed_storage_used);
    atomic_fetch_sub_explicit(&yaza_rt_v1_process_scope_count, 1, memory_order_relaxed);
    if (heap_owned) free(arena);
    return 1;
}

uint64_t yaza_arena_v1_used(YazaArenaV1 *arena) {
    if (arena == NULL) {
        return 0;
    }
    YazaArenaV1 *root = arena->root;
    yaza_rt_v1_tree_lock(root);
    const uint64_t result = arena->destroyed ? 0 : arena->subtree_used;
    yaza_rt_v1_tree_unlock(root);
    return result;
}

uint64_t yaza_arena_v1_storage_used(YazaArenaV1 *arena) {
    if (arena == NULL) {
        return 0;
    }
    YazaArenaV1 *root = arena->root;
    yaza_rt_v1_tree_lock(root);
    const uint64_t result = arena->destroyed ? 0 : arena->subtree_storage_used;
    yaza_rt_v1_tree_unlock(root);
    return result;
}

uint64_t yaza_arena_v1_reserved(YazaArenaV1 *arena) {
    if (arena == NULL) {
        return 0;
    }
    YazaArenaV1 *root = arena->root;
    yaza_rt_v1_tree_lock(root);
    const uint64_t result = arena->destroyed ? 0 : arena->subtree_reserved;
    yaza_rt_v1_tree_unlock(root);
    return result;
}

uint64_t yaza_arena_v1_peak_used(YazaArenaV1 *arena) {
    if (arena == NULL) {
        return 0;
    }
    YazaArenaV1 *root = arena->root;
    yaza_rt_v1_tree_lock(root);
    const uint64_t result = arena->destroyed ? 0 : arena->peak_subtree_used;
    yaza_rt_v1_tree_unlock(root);
    return result;
}

uint64_t yaza_arena_v1_peak_storage_used(YazaArenaV1 *arena) {
    if (arena == NULL) {
        return 0;
    }
    YazaArenaV1 *root = arena->root;
    yaza_rt_v1_tree_lock(root);
    const uint64_t result = arena->destroyed ? 0 : arena->peak_subtree_storage_used;
    yaza_rt_v1_tree_unlock(root);
    return result;
}

uint64_t yaza_arena_v1_peak_reserved(YazaArenaV1 *arena) {
    if (arena == NULL) {
        return 0;
    }
    YazaArenaV1 *root = arena->root;
    yaza_rt_v1_tree_lock(root);
    const uint64_t result = arena->destroyed ? 0 : arena->peak_subtree_reserved;
    yaza_rt_v1_tree_unlock(root);
    return result;
}

uint64_t yaza_arena_v1_last_cycle_generation(YazaArenaV1 *arena) {
    if (arena == NULL) {
        return 0;
    }
    YazaArenaV1 *root = arena->root;
    yaza_rt_v1_tree_lock(root);
    const uint64_t result = arena->destroyed ? 0 : arena->last_cycle_generation;
    yaza_rt_v1_tree_unlock(root);
    return result;
}

uint64_t yaza_arena_v1_last_cycle_peak_used(YazaArenaV1 *arena) {
    if (arena == NULL) {
        return 0;
    }
    YazaArenaV1 *root = arena->root;
    yaza_rt_v1_tree_lock(root);
    const uint64_t result = arena->destroyed ? 0 : arena->last_cycle_peak_used;
    yaza_rt_v1_tree_unlock(root);
    return result;
}

uint64_t yaza_arena_v1_last_cycle_peak_storage_used(YazaArenaV1 *arena) {
    if (arena == NULL) {
        return 0;
    }
    YazaArenaV1 *root = arena->root;
    yaza_rt_v1_tree_lock(root);
    const uint64_t result = arena->destroyed ? 0 : arena->last_cycle_peak_storage_used;
    yaza_rt_v1_tree_unlock(root);
    return result;
}

uint64_t yaza_arena_v1_last_cycle_peak_reserved(YazaArenaV1 *arena) {
    if (arena == NULL) {
        return 0;
    }
    YazaArenaV1 *root = arena->root;
    yaza_rt_v1_tree_lock(root);
    const uint64_t result = arena->destroyed ? 0 : arena->last_cycle_peak_reserved;
    yaza_rt_v1_tree_unlock(root);
    return result;
}

int yaza_arena_v1_last_cycle_metrics(YazaArenaV1 *arena, YazaArenaCycleMetricsV1 *out_metrics) {
    if (arena == NULL || out_metrics == NULL || out_metrics->struct_size < sizeof(*out_metrics)) {
        return 0;
    }
    YazaArenaV1 *root = arena->root;
    yaza_rt_v1_tree_lock(root);
    if (arena->destroyed) {
        yaza_rt_v1_tree_unlock(root);
        return 0;
    }
    const uint32_t caller_struct_size = out_metrics->struct_size;
    *out_metrics = (YazaArenaCycleMetricsV1){
        .abi_version = YAZA_ARENA_CYCLE_METRICS_V1_ABI_VERSION,
        .struct_size = caller_struct_size,
        .generation = arena->last_cycle_generation,
        .successful_allocations = arena->last_cycle_successful_allocations,
        .allocation_failures = arena->last_cycle_allocation_failures,
        .peak_used_bytes = arena->last_cycle_peak_used,
        .peak_storage_used_bytes = arena->last_cycle_peak_storage_used,
        .peak_reserved_bytes = arena->last_cycle_peak_reserved,
        .retained_reserved_bytes = arena->last_cycle_retained_reserved,
    };
    yaza_rt_v1_tree_unlock(root);
    return 1;
}

uint64_t yaza_arena_v1_generation(YazaArenaV1 *arena) {
    if (arena == NULL) {
        return 0;
    }
    YazaArenaV1 *root = arena->root;
    yaza_rt_v1_tree_lock(root);
    const uint64_t result = arena->destroyed ? 0 : arena->generation;
    yaza_rt_v1_tree_unlock(root);
    return result;
}

uint64_t yaza_arena_v1_allocation_count(YazaArenaV1 *arena) {
    if (arena == NULL) {
        return 0;
    }
    YazaArenaV1 *root = arena->root;
    yaza_rt_v1_tree_lock(root);
    const uint64_t result = arena->destroyed ? 0 : arena->allocations;
    yaza_rt_v1_tree_unlock(root);
    return result;
}

uint64_t yaza_rt_v1_arena_used(void) {
    yaza_rt_v1_init();
#if defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_METRICS) || defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_USED)
    return 0;
#elif defined(YAZA_RT_V1_BENCH_GLOBAL_PROCESS_LIVE_METRICS)
    return atomic_load_explicit(&yaza_rt_v1_process_used, memory_order_relaxed);
#else
    return yaza_rt_v1_sum_process_metric(YAZA_RT_V1_METRIC_USED);
#endif
}

uint64_t yaza_rt_v1_arena_storage_used(void) {
    yaza_rt_v1_init();
#if defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_METRICS) || defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_STORAGE)
    return 0;
#elif defined(YAZA_RT_V1_BENCH_GLOBAL_PROCESS_LIVE_METRICS)
    return atomic_load_explicit(&yaza_rt_v1_process_storage_used, memory_order_relaxed);
#else
    return yaza_rt_v1_sum_process_metric(YAZA_RT_V1_METRIC_STORAGE_USED);
#endif
}

uint64_t yaza_rt_v1_arena_reserved(void) {
    yaza_rt_v1_init();
    return atomic_load_explicit(&yaza_rt_v1_process_reserved, memory_order_relaxed);
}

uint64_t yaza_rt_v1_arena_peak_used(void) {
    yaza_rt_v1_init();
#if defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_METRICS) || defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_USED) || defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_PEAK_METRICS)
    return 0;
#elif defined(YAZA_RT_V1_BENCH_GLOBAL_PROCESS_LIVE_METRICS)
    return atomic_load_explicit(&yaza_rt_v1_process_peak_used, memory_order_relaxed);
#else
    return yaza_rt_v1_sum_process_metric(YAZA_RT_V1_METRIC_PEAK_USED);
#endif
}

uint64_t yaza_rt_v1_arena_peak_storage_used(void) {
    yaza_rt_v1_init();
#if defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_METRICS) || defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_STORAGE) || defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_PEAK_METRICS)
    return 0;
#elif defined(YAZA_RT_V1_BENCH_GLOBAL_PROCESS_LIVE_METRICS)
    return atomic_load_explicit(&yaza_rt_v1_process_peak_storage_used, memory_order_relaxed);
#else
    return yaza_rt_v1_sum_process_metric(YAZA_RT_V1_METRIC_PEAK_STORAGE_USED);
#endif
}

uint64_t yaza_rt_v1_arena_peak_reserved(void) {
    yaza_rt_v1_init();
    return atomic_load_explicit(&yaza_rt_v1_process_peak_reserved, memory_order_relaxed);
}

uint64_t yaza_rt_v1_arena_scope_count(void) {
    yaza_rt_v1_init();
    return atomic_load_explicit(&yaza_rt_v1_process_scope_count, memory_order_relaxed);
}

uint64_t yaza_rt_v1_arena_allocation_count(void) {
    yaza_rt_v1_init();
#if defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_METRICS) || defined(YAZA_RT_V1_BENCH_SKIP_PROCESS_ALLOCATION_COUNT)
    return 0;
#elif defined(YAZA_RT_V1_BENCH_GLOBAL_PROCESS_ALLOCATION_COUNT)
    return atomic_load_explicit(&yaza_rt_v1_process_allocation_count, memory_order_relaxed);
#else
    uint64_t total = 0;
    for (uint64_t index = 0; index < YAZA_RT_V1_PROCESS_ALLOCATION_SHARDS; ++index) {
        total += atomic_load_explicit(&yaza_rt_v1_process_allocation_shards[index].allocation_count, memory_order_relaxed);
    }
    return total;
#endif
}

uint64_t yaza_rt_v1_arena_allocation_failures(void) {
    yaza_rt_v1_init();
    return atomic_load_explicit(&yaza_rt_v1_process_allocation_failures, memory_order_relaxed);
}

YazaArenaV1 *yaza_rt_v1_program_arena_handle(void) {
    yaza_rt_v1_init();
    return &yaza_rt_v1_program_arena;
}

void *yaza_rt_v1_alloc(uint64_t size) {
    yaza_rt_v1_init();
    void *memory = yaza_arena_v1_alloc(yaza_arena_v1_current(), size);
    if (memory == NULL) {
        fputs("YAZA runtime v1: program arena limit or allocation failure\n", stderr);
        abort();
    }
    return memory;
}

void *yaza_rt_v1_try_alloc(uint64_t size) {
    yaza_rt_v1_init();
    return yaza_arena_v1_alloc(yaza_arena_v1_current(), size);
}

static void yaza_rt_v1_secure_zero(void *pointer, size_t length) {
    if (pointer == NULL || length == 0) return;
    volatile unsigned char *cursor = (volatile unsigned char *)pointer;
    for (size_t index = 0; index < length; ++index) {
        cursor[index] = 0;
    }
    atomic_signal_fence(memory_order_seq_cst);
}

static void yaza_text_v1_require(const YazaTextV1 *text, const char *operation) {
    if (text == NULL || (text->length > 0 && text->data == NULL) || text->length > SIZE_MAX - 1) {
        fprintf(stderr, "YAZA runtime v1: invalid Text descriptor in %s\n", operation);
        abort();
    }
}

static YazaTextV1 *yaza_text_v1_copy_to_arena(YazaArenaV1 *arena, const unsigned char *data, uint64_t length) {
    if (arena == NULL || (length > 0 && data == NULL) ||
        length > (uint64_t)SIZE_MAX - (uint64_t)sizeof(YazaTextV1) - 1) return NULL;
    const uint64_t allocation_size = (uint64_t)sizeof(YazaTextV1) + length + 1;
    YazaTextV1 *text = (YazaTextV1 *)yaza_arena_v1_alloc(arena, allocation_size);
    if (text == NULL) return NULL;
    unsigned char *copy = (unsigned char *)(text + 1);
    if (length > 0) memcpy(copy, data, (size_t)length);
    copy[length] = '\0';
    text->data = copy;
    text->length = length;
    return text;
}

YazaTextV1 *yaza_rt_v1_text_from_cstr(const char *value) {
    if (value == NULL) {
        fputs("YAZA runtime v1: null C string passed to Text adapter\n", stderr);
        abort();
    }
    const size_t length = strlen(value);
    YazaTextV1 *text = yaza_text_v1_copy_to_arena(yaza_arena_v1_current(), (const unsigned char *)value, (uint64_t)length);
    if (text == NULL) {
        fputs("YAZA runtime v1: arena limit or allocation failure creating Text\n", stderr);
        abort();
    }
    return text;
}

YazaTextV1 *yaza_rt_v1_try_text_from_cstr(const char *value) {
    if (value == NULL) return NULL;
    const size_t length = strlen(value);
    return yaza_text_v1_copy_to_arena(yaza_arena_v1_current(), (const unsigned char *)value, (uint64_t)length);
}

void yaza_rt_v1_set_args(int32_t argc, char **argv) {
    if (argc < 0 || (argc > 0 && argv == NULL)) {
        fputs("YAZA runtime v1: invalid process argument vector\n", stderr);
        abort();
    }
    yaza_rt_v1_init();
    while (atomic_flag_test_and_set_explicit(&yaza_rt_v1_args_lock, memory_order_acquire)) {
    }
    if (atomic_load_explicit(&yaza_rt_v1_args_initialized, memory_order_relaxed)) {
        atomic_flag_clear_explicit(&yaza_rt_v1_args_lock, memory_order_release);
        fputs("YAZA runtime v1: process arguments were initialized more than once\n", stderr);
        abort();
    }
    if (argc > 0) {
        const uint64_t argument_count = (uint64_t)argc;
        if (argument_count > UINT64_MAX / sizeof(*yaza_rt_v1_arguments)) {
            atomic_flag_clear_explicit(&yaza_rt_v1_args_lock, memory_order_release);
            fputs("YAZA runtime v1: process argument vector is too large\n", stderr);
            abort();
        }
        yaza_rt_v1_arguments = (YazaTextV1 *)yaza_rt_v1_alloc(argument_count * sizeof(*yaza_rt_v1_arguments));
        for (int32_t index = 0; index < argc; index += 1) {
            if (argv[index] == NULL) {
                atomic_flag_clear_explicit(&yaza_rt_v1_args_lock, memory_order_release);
                fputs("YAZA runtime v1: process argument is null\n", stderr);
                abort();
            }
            yaza_rt_v1_arguments[index].data = (const unsigned char *)argv[index];
            yaza_rt_v1_arguments[index].length = (uint64_t)strlen(argv[index]);
        }
        yaza_rt_v1_user_arg_count = (int64_t)argc - 1;
    }
    atomic_store_explicit(&yaza_rt_v1_args_initialized, 1, memory_order_release);
    atomic_flag_clear_explicit(&yaza_rt_v1_args_lock, memory_order_release);
}

int64_t yaza_rt_v1_arg_count(void) {
    if (!atomic_load_explicit(&yaza_rt_v1_args_initialized, memory_order_acquire)) {
        fputs("YAZA runtime v1: process arguments accessed before initialization\n", stderr);
        abort();
    }
    return yaza_rt_v1_user_arg_count;
}

YazaTextV1 *yaza_rt_v1_arg(int64_t index) {
    if (!atomic_load_explicit(&yaza_rt_v1_args_initialized, memory_order_acquire)) {
        fputs("YAZA runtime v1: process arguments accessed before initialization\n", stderr);
        abort();
    }
    if (index < 0 || index > yaza_rt_v1_user_arg_count) {
        fprintf(stderr, "yaza: argument %" PRId64 " was not provided\n", index);
        exit(2);
    }
    static const unsigned char empty_data[] = "";
    static YazaTextV1 empty_argument = {empty_data, 0};
    if (yaza_rt_v1_arguments == NULL) return &empty_argument;
    return &yaza_rt_v1_arguments[index];
}

const YazaTextV1 *yaza_rt_v1_text_allocation_error(void) {
    static const unsigned char message[] = "Could not allocate Text in the active arena";
    static const YazaTextV1 error = {message, sizeof(message) - 1};
    return &error;
}

const YazaTextV1 *yaza_rt_v1_list_allocation_error(void) {
    static const unsigned char message[] = "Could not allocate List in the active arena";
    static const YazaTextV1 error = {message, sizeof(message) - 1};
    return &error;
}

YazaTextV1 *yaza_rt_v1_try_text_concat(const YazaTextV1 *left, const YazaTextV1 *right) {
    yaza_text_v1_require(left, "text_concat");
    yaza_text_v1_require(right, "text_concat");
    if (right->length > UINT64_MAX - left->length) return NULL;
    const uint64_t length = left->length + right->length;
    if (length > (uint64_t)SIZE_MAX - (uint64_t)sizeof(YazaTextV1) - 1) return NULL;
    const uint64_t allocation_size = (uint64_t)sizeof(YazaTextV1) + length + 1;
    YazaTextV1 *result = (YazaTextV1 *)yaza_rt_v1_try_alloc(allocation_size);
    if (result == NULL) return NULL;
    unsigned char *data = (unsigned char *)(result + 1);
    if (left->length > 0) memcpy(data, left->data, (size_t)left->length);
    if (right->length > 0) memcpy(data + left->length, right->data, (size_t)right->length);
    data[length] = '\0';
    result->data = data;
    result->length = length;
    return result;
}

const YazaTextV1 *yaza_rt_v1_text_repeat_count_error(void) {
    static const unsigned char message[] = "text_try_repeat count must be non-negative";
    static const YazaTextV1 error = {message, sizeof(message) - 1};
    return &error;
}

const YazaTextV1 *yaza_rt_v1_text_repeat_size_error(void) {
    static const unsigned char message[] = "text_try_repeat output size is too large";
    static const YazaTextV1 error = {message, sizeof(message) - 1};
    return &error;
}

int32_t yaza_rt_v1_try_text_repeat(const YazaTextV1 *text, int64_t count, YazaTextV1 **out_text) {
    if (out_text == NULL) {
        fputs("YAZA runtime v1: null output passed to text_try_repeat\n", stderr);
        abort();
    }
    *out_text = NULL;
    yaza_text_v1_require(text, "text_try_repeat");
    if (count < 0) return -1;

    const uint64_t repetitions = (uint64_t)count;
    if (text->length != 0 && repetitions > UINT64_MAX / text->length) return -2;
    const uint64_t length = text->length * repetitions;
    if (length > (uint64_t)SIZE_MAX - (uint64_t)sizeof(YazaTextV1) - 1) return -2;

    const uint64_t allocation_size = (uint64_t)sizeof(YazaTextV1) + length + 1;
    YazaTextV1 *result = (YazaTextV1 *)yaza_rt_v1_try_alloc(allocation_size);
    if (result == NULL) return 0;
    unsigned char *data = (unsigned char *)(result + 1);
    if (text->length != 0 && repetitions != 0) {
        memcpy(data, text->data, (size_t)text->length);
        uint64_t written = text->length;
        while (written < length) {
            uint64_t chunk = written;
            const uint64_t remaining = length - written;
            if (chunk > remaining) chunk = remaining;
            memcpy(data + written, data, (size_t)chunk);
            written += chunk;
        }
    }
    data[length] = '\0';
    result->data = data;
    result->length = length;
    *out_text = result;
    return 1;
}

YazaTextV1 *yaza_rt_v1_text_concat(const YazaTextV1 *left, const YazaTextV1 *right) {
    yaza_text_v1_require(left, "text_concat");
    yaza_text_v1_require(right, "text_concat");
    if (right->length > UINT64_MAX - left->length || left->length + right->length > SIZE_MAX - sizeof(YazaTextV1) - 1) {
        fputs("YAZA runtime v1: Text concatenation size overflow\n", stderr);
        abort();
    }
    YazaTextV1 *result = yaza_rt_v1_try_text_concat(left, right);
    if (result == NULL) {
        fputs("YAZA runtime v1: arena limit or allocation failure concatenating Text\n", stderr);
        abort();
    }
    return result;
}

int64_t yaza_rt_v1_text_length(const YazaTextV1 *text) {
    yaza_text_v1_require(text, "text_length");
    if (text->length > INT64_MAX) {
        fputs("YAZA runtime v1: Text length exceeds Number range\n", stderr);
        exit(1);
    }
    return (int64_t)text->length;
}

int64_t yaza_rt_v1_text_byte(const YazaTextV1 *text, int64_t index) {
    yaza_text_v1_require(text, "text_byte");
    if (index < 0 || (uint64_t)index >= text->length) {
        fputs("yaza: Text byte index is out of range\n", stderr);
        exit(1);
    }
    return (int64_t)text->data[index];
}

YazaTextV1 *yaza_rt_v1_text_slice(const YazaTextV1 *text, int64_t start, int64_t length) {
    yaza_text_v1_require(text, "text_slice");
    if (start < 0 || length < 0 || (uint64_t)start > text->length ||
        (uint64_t)length > text->length - (uint64_t)start) {
        fputs("yaza: Text slice is out of range\n", stderr);
        exit(1);
    }
    const unsigned char *data = text->data == NULL ? (const unsigned char *)"" : text->data + (uint64_t)start;
    YazaTextV1 *slice = yaza_text_v1_copy_to_arena(yaza_arena_v1_current(), data, (uint64_t)length);
    if (slice == NULL) {
        fputs("YAZA runtime v1: arena limit or allocation failure slicing Text\n", stderr);
        abort();
    }
    return slice;
}

int32_t yaza_rt_v1_try_text_slice(const YazaTextV1 *text, int64_t start, int64_t length, YazaTextV1 **out_slice) {
    if (out_slice == NULL) {
        fputs("YAZA runtime v1: null output passed to text_try_slice\n", stderr);
        abort();
    }
    *out_slice = NULL;
    yaza_text_v1_require(text, "text_try_slice");
    if (start < 0 || length < 0 || (uint64_t)start > text->length ||
        (uint64_t)length > text->length - (uint64_t)start) {
        return -1;
    }
    const unsigned char *data = text->data == NULL ? (const unsigned char *)"" : text->data + (uint64_t)start;
    YazaTextV1 *slice = yaza_text_v1_copy_to_arena(yaza_arena_v1_current(), data, (uint64_t)length);
    if (slice == NULL) return 0;
    *out_slice = slice;
    return 1;
}

const YazaTextV1 *yaza_rt_v1_text_slice_range_error(void) {
    static const unsigned char message[] = "Text slice is out of range";
    static const YazaTextV1 error = {message, sizeof(message) - 1};
    return &error;
}

int64_t yaza_rt_v1_text_find(const YazaTextV1 *text, const YazaTextV1 *needle) {
    yaza_text_v1_require(text, "text_find");
    yaza_text_v1_require(needle, "text_find");
    if (needle->length == 0) return 0;
    if (needle->length > text->length) return -1;
    const uint64_t last = text->length - needle->length;
    for (uint64_t index = 0; index <= last; ++index) {
        if (memcmp(text->data + index, needle->data, (size_t)needle->length) == 0) return (int64_t)index;
    }
    return -1;
}

int32_t yaza_rt_v1_text_starts_with(const YazaTextV1 *text, const YazaTextV1 *prefix) {
    yaza_text_v1_require(text, "text_starts_with");
    yaza_text_v1_require(prefix, "text_starts_with");
    return prefix->length <= text->length &&
        (prefix->length == 0 || memcmp(text->data, prefix->data, (size_t)prefix->length) == 0) ? 1 : 0;
}

int32_t yaza_rt_v1_text_ends_with(const YazaTextV1 *text, const YazaTextV1 *suffix) {
    yaza_text_v1_require(text, "text_ends_with");
    yaza_text_v1_require(suffix, "text_ends_with");
    return suffix->length <= text->length &&
        (suffix->length == 0 || memcmp(text->data + text->length - suffix->length,
                                      suffix->data, (size_t)suffix->length) == 0) ? 1 : 0;
}

int64_t yaza_rt_v1_text_compare(const YazaTextV1 *left, const YazaTextV1 *right) {
    yaza_text_v1_require(left, "text_compare");
    yaza_text_v1_require(right, "text_compare");
    const uint64_t common = left->length < right->length ? left->length : right->length;
    for (uint64_t index = 0; index < common; ++index) {
        if (left->data[index] != right->data[index]) {
            return (int64_t)left->data[index] - (int64_t)right->data[index];
        }
    }
    if (left->length < right->length) {
        const uint8_t next = right->data[common];
        return next == 0 ? -1 : -(int64_t)next;
    }
    if (left->length > right->length) {
        const uint8_t next = left->data[common];
        return next == 0 ? 1 : (int64_t)next;
    }
    return 0;
}

int32_t yaza_rt_v1_say_number(int64_t value) {
    return (int32_t)printf("%" PRId64 "\n", value);
}

int32_t yaza_rt_v1_say_int32(int32_t value) {
    return (int32_t)printf("%" PRId32 "\n", value);
}

YazaTextV1 *yaza_rt_v1_try_number_text(int64_t value) {
    unsigned char buffer[21];
    unsigned char *cursor = buffer + sizeof(buffer);
    const int negative = value < 0;
    uint64_t magnitude = negative ? (uint64_t)(-(value + 1)) + 1 : (uint64_t)value;
    do {
        *--cursor = (unsigned char)('0' + magnitude % 10);
        magnitude /= 10;
    } while (magnitude > 0);
    if (negative) {
        *--cursor = '-';
    }
    return yaza_text_v1_copy_to_arena(yaza_arena_v1_current(), cursor, (uint64_t)((buffer + sizeof(buffer)) - cursor));
}

YazaTextV1 *yaza_rt_v1_number_text(int64_t value) {
    unsigned char buffer[21];
    unsigned char *cursor = buffer + sizeof(buffer);
    const int negative = value < 0;
    uint64_t magnitude = negative ? (uint64_t)(-(value + 1)) + 1 : (uint64_t)value;
    do {
        *--cursor = (unsigned char)('0' + magnitude % 10);
        magnitude /= 10;
    } while (magnitude > 0);
    if (negative) *--cursor = '-';
    YazaTextV1 *text = yaza_text_v1_copy_to_arena(yaza_arena_v1_current(), cursor,
        (uint64_t)((buffer + sizeof(buffer)) - cursor));
    if (text == NULL) {
        fputs("YAZA runtime v1: arena limit or allocation failure creating Number Text\n", stderr);
        abort();
    }
    return text;
}

YazaTextV1 *yaza_rt_v1_try_int32_text(int32_t value) {
    char buffer[16];
    const int written = snprintf(buffer, sizeof(buffer), "%" PRId32, value);
    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return NULL;
    }
    return yaza_rt_v1_try_text_from_cstr(buffer);
}

YazaTextV1 *yaza_rt_v1_int32_text(int32_t value) {
    char buffer[16];
    const int written = snprintf(buffer, sizeof(buffer), "%" PRId32, value);
    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        fputs("yaza: Int32 formatting failed\n", stderr);
        exit(1);
    }
    return yaza_rt_v1_text_from_cstr(buffer);
}

YazaTextV1 *yaza_rt_v1_try_text_trim(const YazaTextV1 *text) {
    yaza_text_v1_require(text, "text_try_trim");
    uint64_t start = 0;
    uint64_t end = text->length;
    while (start < end && isspace((unsigned char)text->data[start])) start += 1;
    while (end > start && isspace((unsigned char)text->data[end - 1])) end -= 1;
    const unsigned char *data = text->data == NULL ? (const unsigned char *)"" : text->data + start;
    return yaza_text_v1_copy_to_arena(yaza_arena_v1_current(), data, end - start);
}

YazaTextV1 *yaza_rt_v1_text_trim(const YazaTextV1 *text) {
    yaza_text_v1_require(text, "text_trim");
    YazaTextV1 *trimmed = yaza_rt_v1_try_text_trim(text);
    if (trimmed == NULL) {
        fputs("YAZA runtime v1: arena limit or allocation failure trimming Text\n", stderr);
        abort();
    }
    return trimmed;
}

int32_t yaza_rt_v1_say_text(const YazaTextV1 *text) {
    yaza_text_v1_require(text, "say");
    const size_t length = (size_t)text->length;
    const size_t written = length == 0 ? 0 : fwrite(text->data, 1, length, stdout);
    if (written != length || fputc('\n', stdout) == EOF) return -1;
    return text->length >= INT32_MAX ? INT32_MAX : (int32_t)text->length + 1;
}

static int64_t yaza_text_v1_parse_error(const char *text) {
    fputs("yaza: invalid Number '", stderr);
    if (text != NULL) fputs(text, stderr);
    fputs("'\n", stderr);
    exit(1);
    return 0;
}

int64_t yaza_rt_v1_number_parse(const YazaTextV1 *text) {
    yaza_text_v1_require(text, "number_parse");
    if (text->length > INT64_MAX || (text->length > 0 && memchr(text->data, '\0', (size_t)text->length) != NULL)) {
        return yaza_text_v1_parse_error("<invalid Text>");
    }
    errno = 0;
    char *end = NULL;
    const char *bytes = text->data == NULL ? "" : (const char *)text->data;
    const long long value = strtoll(bytes, &end, 10);
    if (errno != 0 || end == bytes || (uint64_t)(end - bytes) != text->length || *end != '\0') return yaza_text_v1_parse_error(bytes);
    return (int64_t)value;
}

static int yaza_rt_v1_try_number_value(const YazaTextV1 *text, int64_t *out_value) {
    if (text == NULL || out_value == NULL || text->length > INT64_MAX ||
        (text->length > 0 && (text->data == NULL || memchr(text->data, '\0', (size_t)text->length) != NULL))) {
        return 0;
    }

    uint64_t index = 0;
    while (index < text->length && (text->data[index] == ' ' || text->data[index] == '\t' ||
           text->data[index] == '\n' || text->data[index] == '\r' || text->data[index] == '\v' ||
           text->data[index] == '\f')) {
        index += 1;
    }

    int negative = 0;
    if (index < text->length && (text->data[index] == '+' || text->data[index] == '-')) {
        negative = text->data[index] == '-';
        index += 1;
    }
    const uint64_t digit_start = index;
    const uint64_t limit = negative ? (uint64_t)INT64_MAX + 1ULL : (uint64_t)INT64_MAX;
    uint64_t magnitude = 0;
    while (index < text->length && text->data[index] >= '0' && text->data[index] <= '9') {
        const uint64_t digit = (uint64_t)(text->data[index] - '0');
        if (magnitude > (limit - digit) / 10ULL) return 0;
        magnitude = magnitude * 10ULL + digit;
        index += 1;
    }
    if (index == digit_start || index != text->length) return 0;

    if (negative) {
        if (magnitude == (uint64_t)INT64_MAX + 1ULL) *out_value = INT64_MIN;
        else *out_value = -(int64_t)magnitude;
    } else {
        *out_value = (int64_t)magnitude;
    }
    return 1;
}

int32_t yaza_rt_v1_try_number_parse(const YazaTextV1 *text, int64_t *out_value) {
    return yaza_rt_v1_try_number_value(text, out_value);
}

int32_t yaza_rt_v1_try_int32_parse(const YazaTextV1 *text, int32_t *out_value) {
    if (out_value == NULL) return 0;
    int64_t value = 0;
    if (!yaza_rt_v1_try_number_value(text, &value) || value < INT32_MIN || value > INT32_MAX) return 0;
    *out_value = (int32_t)value;
    return 1;
}

int64_t yaza_rt_v1_fail(const char *message) {
    fprintf(stderr, "yaza: %s\n", message == NULL ? "compiler failure" : message);
    exit(1);
    return 1;
}

int64_t yaza_rt_v1_assert_fail(const char *message) {
    fprintf(stderr, "%s\n", message == NULL ? "Assertion failed" : message);
    exit(1);
    return 1;
}

int64_t yaza_rt_v1_fail_text(const YazaTextV1 *message) {
    fputs("yaza: ", stderr);
    if (message == NULL) fputs("compiler failure", stderr);
    else {
        yaza_text_v1_require(message, "fail");
        if (message->length > 0) (void)fwrite(message->data, 1, (size_t)message->length, stderr);
    }
    fputc('\n', stderr);
    exit(1);
    return 1;
}

int64_t yaza_rt_v1_assert_fail_text(const YazaTextV1 *message) {
    if (message == NULL) fputs("Assertion failed", stderr);
    else {
        yaza_text_v1_require(message, "assert_fail");
        if (message->length > 0) (void)fwrite(message->data, 1, (size_t)message->length, stderr);
    }
    fputc('\n', stderr);
    exit(1);
    return 1;
}

int64_t yaza_rt_v1_exit(int64_t status) {
    exit((int)status);
    return status;
}

static int yaza_rt_v1_float64_text_valid(const YazaTextV1 *text) {
    if (text == NULL || text->length == 0 || (text->length > 0 && text->data == NULL)) return 0;
    uint64_t index = 0;
    if (text->data[index] == '+' || text->data[index] == '-') index += 1;
    int integer_digits = 0;
    while (index < text->length && text->data[index] >= '0' && text->data[index] <= '9') {
        integer_digits = 1;
        index += 1;
    }
    if (index < text->length && text->data[index] == '.') {
        index += 1;
        int fraction_digits = 0;
        while (index < text->length && text->data[index] >= '0' && text->data[index] <= '9') {
            fraction_digits = 1;
            index += 1;
        }
        if (!fraction_digits) return 0;
    }
    if (!integer_digits) return 0;
    if (index < text->length && (text->data[index] == 'e' || text->data[index] == 'E')) {
        index += 1;
        if (index < text->length && (text->data[index] == '+' || text->data[index] == '-')) index += 1;
        int exponent_digits = 0;
        while (index < text->length && text->data[index] >= '0' && text->data[index] <= '9') {
            exponent_digits = 1;
            index += 1;
        }
        if (!exponent_digits) return 0;
    }
    return index == text->length && memchr(text->data, '\0', (size_t)text->length) == NULL;
}

double yaza_rt_v1_float64_parse(const YazaTextV1 *text) {
    if (!yaza_rt_v1_float64_text_valid(text)) {
        yaza_rt_v1_fail("invalid Float64 value or arithmetic error in parsing");
        return 0.0;
    }
    errno = 0;
    char *end = NULL;
    const char *bytes = (const char *)text->data;
    const double value = strtod(bytes, &end);
    if (errno == ERANGE || end == bytes || (uint64_t)(end - bytes) != text->length || *end != '\0' || !isfinite(value)) {
        yaza_rt_v1_fail("invalid Float64 value or arithmetic error in parsing");
        return 0.0;
    }
    return value;
}

int32_t yaza_rt_v1_try_float64_parse(const YazaTextV1 *text, double *out_value) {
    if (out_value == NULL || !yaza_rt_v1_float64_text_valid(text)) return 0;
    errno = 0;
    char *end = NULL;
    const char *bytes = (const char *)text->data;
    const double value = strtod(bytes, &end);
    if (errno == ERANGE || end == bytes || (uint64_t)(end - bytes) != text->length || *end != '\0' || !isfinite(value)) return 0;
    *out_value = value;
    return 1;
}

YazaTextV1 *yaza_rt_v1_try_float64_text(double value) {
    if (!isfinite(value)) {
        return NULL;
    }
    char buffer[64];
    const int written = snprintf(buffer, sizeof(buffer), "%.17g", value);
    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return NULL;
    }
    return yaza_rt_v1_try_text_from_cstr(buffer);
}

YazaTextV1 *yaza_rt_v1_float64_text(double value) {
    if (!isfinite(value)) {
        yaza_rt_v1_fail("invalid Float64 value or arithmetic error in formatting");
        return NULL;
    }
    char buffer[64];
    const int written = snprintf(buffer, sizeof(buffer), "%.17g", value);
    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        yaza_rt_v1_fail("invalid Float64 value or arithmetic error in formatting");
        return NULL;
    }
    return yaza_rt_v1_text_from_cstr(buffer);
}

#define YAZA_RT_V1_BYTES_MAX_BYTES (64ULL * 1024ULL * 1024ULL)

static void yaza_bytes_v1_fail(const char *message) {
    fprintf(stderr, "YAZA runtime v1: %s\n", message);
    abort();
}

static YazaBytesV1 yaza_bytes_v1_copy(const unsigned char *data, uint64_t length) {
    if (length > YAZA_RT_V1_BYTES_MAX_BYTES || (length > 0 && data == NULL)) {
        yaza_bytes_v1_fail("Bytes length or source pointer is invalid");
    }
    YazaBytesV1 result = {NULL, length};
    if (length > 0) {
        result.data = (unsigned char *)yaza_rt_v1_alloc(length);
        memcpy(result.data, data, (size_t)length);
    }
    return result;
}

void yaza_bytes_v1_from_text(const YazaTextV1 *text, YazaBytesV1 *out_bytes) {
    if (text == NULL || out_bytes == NULL) {
        yaza_bytes_v1_fail("bytes_from_text received a null pointer");
    }
    yaza_text_v1_require(text, "bytes_from_text");
    const YazaBytesV1 result = yaza_bytes_v1_copy(text->data, text->length);
    *out_bytes = result;
}

int32_t yaza_bytes_v1_try_from_text(const YazaTextV1 *text, YazaBytesV1 *out_bytes, char **out_error) {
    if (out_bytes == NULL || out_error == NULL) {
        yaza_bytes_v1_fail("bytes_try_from_text received a null output pointer");
    }
    *out_bytes = (YazaBytesV1){NULL, 0};
    *out_error = NULL;
    if (text == NULL || text->length > SIZE_MAX - 1 || (text->length > 0 && text->data == NULL)) {
        *out_error = (char *)"Text descriptor is invalid";
        return 0;
    }
    if (text->length > YAZA_RT_V1_BYTES_MAX_BYTES) {
        *out_error = (char *)"Text exceeds the 64 MiB Bytes limit";
        return 0;
    }
    YazaBytesV1 result = {NULL, text->length};
    if (text->length > 0) {
        result.data = (unsigned char *)yaza_rt_v1_try_alloc(text->length);
        if (result.data == NULL) {
            result.length = 0;
            *out_error = (char *)"Could not allocate Bytes in the active arena";
            return 0;
        }
        memcpy(result.data, text->data, (size_t)text->length);
    }
    *out_bytes = result;
    return 1;
}

int32_t yaza_bytes_v1_try_new_zeroed(int64_t requested_length, YazaBytesV1 *out_bytes, char **out_error) {
    if (out_bytes == NULL || out_error == NULL) {
        yaza_bytes_v1_fail("bytes_try_new_zeroed received a null output pointer");
    }
    *out_bytes = (YazaBytesV1){NULL, 0};
    *out_error = NULL;
    if (requested_length < 0 || (uint64_t)requested_length > YAZA_RT_V1_BYTES_MAX_BYTES) {
        *out_error = (char *)"Bytes length must be between 0 and 64 MiB";
        return 0;
    }
    const uint64_t length = (uint64_t)requested_length;
    YazaBytesV1 result = {NULL, length};
    if (length > 0) {
        result.data = (unsigned char *)yaza_rt_v1_try_alloc(length);
        if (result.data == NULL) {
            result.length = 0;
            *out_error = (char *)"Could not allocate zeroed Bytes in the active arena";
            return 0;
        }
        memset(result.data, 0, (size_t)length);
    }
    *out_bytes = result;
    return 1;
}

void yaza_bytes_v1_from_number(int64_t value, YazaBytesV1 *out_bytes) {
    if (out_bytes == NULL || value < 0 || value > UINT8_MAX) {
        yaza_bytes_v1_fail("bytes_from_number expects a value from 0 through 255");
    }
    const unsigned char byte = (unsigned char)value;
    const YazaBytesV1 result = yaza_bytes_v1_copy(&byte, 1);
    *out_bytes = result;
}

int32_t yaza_bytes_v1_try_from_number(int64_t value, YazaBytesV1 *out_bytes, char **out_error) {
    if (out_bytes == NULL || out_error == NULL) {
        yaza_bytes_v1_fail("bytes_try_from_number received a null output pointer");
    }
    *out_bytes = (YazaBytesV1){NULL, 0};
    *out_error = NULL;
    if (value < 0 || value > UINT8_MAX) {
        *out_error = (char *)"Bytes value must be between 0 and 255";
        return 0;
    }
    unsigned char *byte = (unsigned char *)yaza_rt_v1_try_alloc(1);
    if (byte == NULL) {
        *out_error = (char *)"Could not allocate Bytes in the active arena";
        return 0;
    }
    byte[0] = (unsigned char)value;
    *out_bytes = (YazaBytesV1){byte, 1};
    return 1;
}

void yaza_bytes_v1_secure_random(int64_t requested_length, YazaBytesV1 *out_bytes) {
    if (out_bytes == NULL) {
        yaza_bytes_v1_fail("secure_random_bytes received a null output pointer");
    }
    if (requested_length < 0 || (uint64_t)requested_length > YAZA_RT_V1_BYTES_MAX_BYTES) {
        yaza_rt_v1_fail("secure_random_bytes length must be between 0 and 64 MiB");
        return;
    }

    const uint64_t length = (uint64_t)requested_length;
    YazaBytesV1 result = {NULL, length};
    if (length == 0) {
        *out_bytes = result;
        return;
    }

    result.data = (unsigned char *)yaza_rt_v1_alloc(length);
#if defined(_WIN32)
    uint64_t offset = 0;
    while (offset < length) {
        ULONG chunk = (length - offset) > 0x7fffffffULL ? 0x7fffffffUL : (ULONG)(length - offset);
        if (BCryptGenRandom(NULL, result.data + (size_t)offset, chunk, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
            volatile unsigned char *wipe = result.data;
            for (uint64_t index = 0; index < length; ++index) wipe[index] = 0;
            fprintf(stderr, "yaza: operating-system random source failed\n");
            exit(1);
        }
        offset += (uint64_t)chunk;
    }
    *out_bytes = result;
#else
    const int random_fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (random_fd < 0) {
        fprintf(stderr, "yaza: operating-system random source is unavailable: %s\n", strerror(errno));
        exit(1);
    }

    uint64_t offset = 0;
    while (offset < length) {
        const ssize_t received = read(random_fd, result.data + (size_t)offset, (size_t)(length - offset));
        if (received > 0) {
            offset += (uint64_t)received;
        } else if (received < 0 && errno == EINTR) {
            continue;
        } else {
            const int read_error = received == 0 ? EIO : errno;
            volatile unsigned char *wipe = result.data;
            for (uint64_t index = 0; index < length; ++index) wipe[index] = 0;
            (void)close(random_fd);
            fprintf(stderr, "yaza: operating-system random source failed: %s\n", strerror(read_error));
            exit(1);
        }
    }

    if (close(random_fd) != 0) {
        const int close_error = errno;
        volatile unsigned char *wipe = result.data;
        for (uint64_t index = 0; index < length; ++index) wipe[index] = 0;
        fprintf(stderr, "yaza: could not close operating-system random source: %s\n", strerror(close_error));
        exit(1);
    }
    *out_bytes = result;
#endif
}

int32_t yaza_bytes_v1_try_secure_random(int64_t requested_length, YazaBytesV1 *out_bytes, char **out_error) {
    if (out_bytes == NULL || out_error == NULL) {
        yaza_bytes_v1_fail("bytes_try_secure_random received a null output pointer");
    }
    *out_bytes = (YazaBytesV1){NULL, 0};
    *out_error = NULL;
    if (requested_length < 0 || (uint64_t)requested_length > YAZA_RT_V1_BYTES_MAX_BYTES) {
        *out_error = (char *)"secure random length must be between 0 and 64 MiB";
        return 0;
    }

    const uint64_t length = (uint64_t)requested_length;
    if (length == 0) {
        *out_bytes = (YazaBytesV1){NULL, 0};
        return 1;
    }

#if defined(_WIN32)
    unsigned char *data = (unsigned char *)yaza_rt_v1_try_alloc(length);
    if (data == NULL) {
        *out_error = (char *)"could not allocate secure random Bytes in the active arena";
        return 0;
    }
    uint64_t offset = 0;
    while (offset < length) {
        ULONG chunk = (length - offset) > 0x7fffffffULL ? 0x7fffffffUL : (ULONG)(length - offset);
        if (BCryptGenRandom(NULL, data + (size_t)offset, chunk, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
            volatile unsigned char *wipe = data;
            for (uint64_t index = 0; index < length; ++index) wipe[index] = 0;
            *out_error = (char *)"operating-system random source failed";
            return 0;
        }
        offset += (uint64_t)chunk;
    }
    *out_bytes = (YazaBytesV1){data, length};
    return 1;
#else
    const int random_fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (random_fd < 0) {
        *out_error = (char *)"operating-system random source is unavailable";
        return 0;
    }

    unsigned char *data = (unsigned char *)yaza_rt_v1_try_alloc(length);
    if (data == NULL) {
        (void)close(random_fd);
        *out_error = (char *)"could not allocate secure random Bytes in the active arena";
        return 0;
    }

    uint64_t offset = 0;
    while (offset < length) {
        const ssize_t received = read(random_fd, data + (size_t)offset, (size_t)(length - offset));
        if (received > 0) {
            offset += (uint64_t)received;
        } else if (received < 0 && errno == EINTR) {
            continue;
        } else {
            volatile unsigned char *wipe = data;
            for (uint64_t index = 0; index < length; ++index) wipe[index] = 0;
            (void)close(random_fd);
            *out_error = (char *)"operating-system random source failed";
            return 0;
        }
    }

    if (close(random_fd) != 0) {
        volatile unsigned char *wipe = data;
        for (uint64_t index = 0; index < length; ++index) wipe[index] = 0;
        *out_error = (char *)"could not close operating-system random source";
        return 0;
    }

    *out_bytes = (YazaBytesV1){data, length};
    return 1;
#endif
}

#if !defined(__APPLE__) || defined(YAZA_TEST_SOFTWARE_SHA256)
static inline uint32_t yaza_rotr32_v1(uint32_t value, uint32_t bits) {
    return (value >> bits) | (value << (32U - bits));
}

typedef struct {
    uint32_t state[8];
    uint64_t bitlen;
    unsigned char buffer[64];
    size_t buffer_len;
} YazaSwSha256CtxV1;

static void yaza_sw_sha256_transform_v1(uint32_t state[8], const unsigned char block[64]) {
    static const uint32_t k[64] = {
        0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
        0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
        0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
        0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
        0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
        0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
        0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
        0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
    };
    uint32_t w[64];
    for (size_t i = 0; i < 16; ++i) {
        size_t j = i * 4;
        w[i] = ((uint32_t)block[j] << 24) | ((uint32_t)block[j + 1] << 16) |
               ((uint32_t)block[j + 2] << 8) | (uint32_t)block[j + 3];
    }
    for (size_t i = 16; i < 64; ++i) {
        uint32_t s0 = yaza_rotr32_v1(w[i - 15], 7) ^ yaza_rotr32_v1(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = yaza_rotr32_v1(w[i - 2], 17) ^ yaza_rotr32_v1(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
    for (size_t i = 0; i < 64; ++i) {
        uint32_t S1 = yaza_rotr32_v1(e, 6) ^ yaza_rotr32_v1(e, 11) ^ yaza_rotr32_v1(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + S1 + ch + k[i] + w[i];
        uint32_t S0 = yaza_rotr32_v1(a, 2) ^ yaza_rotr32_v1(a, 13) ^ yaza_rotr32_v1(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;
        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

static void yaza_sw_sha256_init_v1(YazaSwSha256CtxV1 *ctx) {
    ctx->state[0] = 0x6a09e667U; ctx->state[1] = 0xbb67ae85U;
    ctx->state[2] = 0x3c6ef372U; ctx->state[3] = 0xa54ff53aU;
    ctx->state[4] = 0x510e527fU; ctx->state[5] = 0x9b05688cU;
    ctx->state[6] = 0x1f83d9abU; ctx->state[7] = 0x5be0cd19U;
    ctx->bitlen = 0;
    ctx->buffer_len = 0;
}

static void yaza_sw_sha256_update_v1(YazaSwSha256CtxV1 *ctx, const unsigned char *data, uint64_t len) {
    if (len == 0 || data == NULL) return;
    ctx->bitlen += len * 8ULL;
    size_t offset = 0;
    if (ctx->buffer_len > 0) {
        size_t need = 64 - ctx->buffer_len;
        size_t take = (len < (uint64_t)need) ? (size_t)len : need;
        memcpy(ctx->buffer + ctx->buffer_len, data, take);
        ctx->buffer_len += take;
        offset += take;
        if (ctx->buffer_len == 64) {
            yaza_sw_sha256_transform_v1(ctx->state, ctx->buffer);
            ctx->buffer_len = 0;
        }
    }
    while ((uint64_t)offset + 64ULL <= len) {
        yaza_sw_sha256_transform_v1(ctx->state, data + offset);
        offset += 64;
    }
    if ((uint64_t)offset < len) {
        ctx->buffer_len = (size_t)(len - (uint64_t)offset);
        memcpy(ctx->buffer, data + offset, ctx->buffer_len);
    }
}

static void yaza_sw_sha256_final_v1(YazaSwSha256CtxV1 *ctx, unsigned char out_digest[32]) {
    ctx->buffer[ctx->buffer_len++] = 0x80U;
    if (ctx->buffer_len > 56) {
        while (ctx->buffer_len < 64) ctx->buffer[ctx->buffer_len++] = 0x00U;
        yaza_sw_sha256_transform_v1(ctx->state, ctx->buffer);
        ctx->buffer_len = 0;
    }
    while (ctx->buffer_len < 56) ctx->buffer[ctx->buffer_len++] = 0x00U;
    for (int shift = 56, idx = 56; shift >= 0; shift -= 8, ++idx) {
        ctx->buffer[idx] = (unsigned char)((ctx->bitlen >> shift) & 0xffULL);
    }
    yaza_sw_sha256_transform_v1(ctx->state, ctx->buffer);
    for (size_t i = 0; i < 8; ++i) {
        out_digest[i * 4] = (unsigned char)((ctx->state[i] >> 24) & 0xffU);
        out_digest[i * 4 + 1] = (unsigned char)((ctx->state[i] >> 16) & 0xffU);
        out_digest[i * 4 + 2] = (unsigned char)((ctx->state[i] >> 8) & 0xffU);
        out_digest[i * 4 + 3] = (unsigned char)(ctx->state[i] & 0xffU);
    }
}

static void yaza_sw_sha256_raw_v1(const unsigned char *data, uint64_t len, unsigned char out_digest[32]) {
    YazaSwSha256CtxV1 ctx;
    yaza_sw_sha256_init_v1(&ctx);
    yaza_sw_sha256_update_v1(&ctx, data, len);
    yaza_sw_sha256_final_v1(&ctx, out_digest);
}

static void yaza_sw_hmac_sha256_parts_v1(const unsigned char *key, uint64_t key_len,
                                         const unsigned char *part1, uint64_t len1,
                                         const unsigned char *part2, uint64_t len2,
                                         unsigned char out_digest[32]) {
    unsigned char key_block[64];
    memset(key_block, 0, sizeof(key_block));
    if (key_len > 64ULL) {
        yaza_sw_sha256_raw_v1(key, key_len, key_block);
    } else if (key_len > 0ULL && key != NULL) {
        memcpy(key_block, key, (size_t)key_len);
    }
    unsigned char ipad[64];
    unsigned char opad[64];
    for (size_t i = 0; i < 64; ++i) {
        ipad[i] = (unsigned char)(key_block[i] ^ 0x36U);
        opad[i] = (unsigned char)(key_block[i] ^ 0x5cU);
    }
    unsigned char inner_digest[32];
    YazaSwSha256CtxV1 ctx;
    yaza_sw_sha256_init_v1(&ctx);
    yaza_sw_sha256_update_v1(&ctx, ipad, 64);
    yaza_sw_sha256_update_v1(&ctx, part1, len1);
    yaza_sw_sha256_update_v1(&ctx, part2, len2);
    yaza_sw_sha256_final_v1(&ctx, inner_digest);

    yaza_sw_sha256_init_v1(&ctx);
    yaza_sw_sha256_update_v1(&ctx, opad, 64);
    yaza_sw_sha256_update_v1(&ctx, inner_digest, 32);
    yaza_sw_sha256_final_v1(&ctx, out_digest);

    yaza_rt_v1_secure_zero(key_block, sizeof(key_block));
    yaza_rt_v1_secure_zero(ipad, sizeof(ipad));
    yaza_rt_v1_secure_zero(opad, sizeof(opad));
    yaza_rt_v1_secure_zero(inner_digest, sizeof(inner_digest));
    yaza_rt_v1_secure_zero(&ctx, sizeof(ctx));
}

static void yaza_sw_pbkdf2_sha256_raw_v1(const unsigned char *password, uint64_t password_len,
                                          const unsigned char *salt, uint64_t salt_len,
                                          int64_t iterations, int64_t key_length,
                                          unsigned char *out_derived) {
    size_t remaining = (size_t)key_length;
    size_t offset = 0;
    uint32_t block_index = 1;
    while (remaining > 0) {
        unsigned char be_index[4];
        be_index[0] = (unsigned char)((block_index >> 24) & 0xffU);
        be_index[1] = (unsigned char)((block_index >> 16) & 0xffU);
        be_index[2] = (unsigned char)((block_index >> 8) & 0xffU);
        be_index[3] = (unsigned char)(block_index & 0xffU);

        unsigned char u[32];
        unsigned char t[32];
        yaza_sw_hmac_sha256_parts_v1(password, password_len, salt, salt_len, be_index, 4, u);
        memcpy(t, u, 32);
        for (int64_t iter = 2; iter <= iterations; ++iter) {
            yaza_sw_hmac_sha256_parts_v1(password, password_len, u, 32, NULL, 0, u);
            for (size_t j = 0; j < 32; ++j) t[j] ^= u[j];
        }
        size_t take = (remaining < 32) ? remaining : 32;
        memcpy(out_derived + offset, t, take);
        offset += take;
        remaining -= take;
        block_index += 1;
        yaza_rt_v1_secure_zero(u, sizeof(u));
        yaza_rt_v1_secure_zero(t, sizeof(t));
    }
}
#endif

void yaza_crypto_v1_sha256(const unsigned char *data, uint64_t length, YazaBytesV1 *out_bytes) {
    if (out_bytes == NULL) {
        yaza_bytes_v1_fail("crypto_sha256 received a null output pointer");
    }
    const unsigned char *input_data = (data != NULL) ? data : (const unsigned char *)"";
    unsigned char digest[32];
#if defined(__APPLE__) && !defined(YAZA_TEST_SOFTWARE_SHA256)
    CC_SHA256_CTX ctx;
    (void)CC_SHA256_Init(&ctx);
    uint64_t offset = 0;
    while (offset < length) {
        const uint64_t remaining = length - offset;
        const CC_LONG chunk = remaining > (uint64_t)UINT32_MAX ? (CC_LONG)(1ULL << 30) : (CC_LONG)remaining;
        (void)CC_SHA256_Update(&ctx, input_data + offset, chunk);
        offset += (uint64_t)chunk;
    }
    (void)CC_SHA256_Final(digest, &ctx);
    yaza_rt_v1_secure_zero(&ctx, sizeof(ctx));
#else
    yaza_sw_sha256_raw_v1(input_data, length, digest);
#endif
    *out_bytes = yaza_bytes_v1_copy(digest, 32);
    yaza_rt_v1_secure_zero(digest, sizeof(digest));
}

void yaza_crypto_v1_hmac_sha256(const unsigned char *key, uint64_t key_length, const unsigned char *message, uint64_t message_length, YazaBytesV1 *out_bytes) {
    if (out_bytes == NULL) {
        yaza_bytes_v1_fail("crypto_hmac_sha256 received a null output pointer");
    }
    const unsigned char *k = (key != NULL) ? key : (const unsigned char *)"";
    const unsigned char *m = (message != NULL) ? message : (const unsigned char *)"";
    unsigned char digest[32];
#if defined(__APPLE__) && !defined(YAZA_TEST_SOFTWARE_SHA256)
    CCHmac(kCCHmacAlgSHA256, k, (size_t)key_length, m, (size_t)message_length, digest);
#else
    yaza_sw_hmac_sha256_parts_v1(k, key_length, m, message_length, NULL, 0, digest);
#endif
    *out_bytes = yaza_bytes_v1_copy(digest, 32);
    yaza_rt_v1_secure_zero(digest, sizeof(digest));
}

int32_t yaza_crypto_v1_pbkdf2_sha256(const unsigned char *password, uint64_t password_length, const unsigned char *salt, uint64_t salt_length, int64_t iterations, int64_t key_length, YazaBytesV1 *out_bytes, char **out_error) {
    if (out_bytes == NULL || out_error == NULL) {
        yaza_bytes_v1_fail("crypto_pbkdf2_sha256 received a null pointer");
    }
    *out_bytes = (YazaBytesV1){NULL, 0};
    *out_error = NULL;
    if (iterations < 1 || iterations > 1000000) {
        *out_error = (char *)"PBKDF2 iterations must be between 1 and 1000000";
        return 0;
    }
    if (key_length < 1 || key_length > 64) {
        *out_error = (char *)"PBKDF2-SHA-256 output must be between 1 and 64 bytes";
        return 0;
    }
    if (salt == NULL || salt_length < 1 || salt_length > 1024) {
        *out_error = (char *)"PBKDF2 salt must be between 1 and 1024 bytes";
        return 0;
    }
    const unsigned char *p = (password != NULL) ? password : (const unsigned char *)"";
    unsigned char derived[64];
#if defined(__APPLE__) && !defined(YAZA_TEST_SOFTWARE_SHA256)
    int status = CCKeyDerivationPBKDF(kCCPBKDF2, (const char *)p, (size_t)password_length,
                                      salt, (size_t)salt_length,
                                      kCCPRFHmacAlgSHA256, (uint32_t)iterations,
                                      derived, (size_t)key_length);
    if (status != 0) {
        yaza_rt_v1_secure_zero(derived, sizeof(derived));
        *out_error = (char *)"operating-system PBKDF2-SHA-256 provider failed";
        return 0;
    }
#else
    yaza_sw_pbkdf2_sha256_raw_v1(p, password_length, salt, salt_length, iterations, key_length, derived);
#endif
    *out_bytes = yaza_bytes_v1_copy(derived, (uint64_t)key_length);
    yaza_rt_v1_secure_zero(derived, sizeof(derived));
    return 1;
}

int32_t yaza_crypto_v1_argon2id_hash_format_is_current(const YazaTextV1 *encoded_hash) {
    static const char prefix[] = "$argon2id$v=19$m=19456,t=2,p=1$";
    if (encoded_hash == NULL || encoded_hash->data == NULL || encoded_hash->length < sizeof(prefix) ||
        encoded_hash->length > 256 || memchr(encoded_hash->data, '\0', (size_t)encoded_hash->length) != NULL) {
        return 0;
    }
    return memcmp(encoded_hash->data, prefix, sizeof(prefix) - 1) == 0 &&
           memchr(encoded_hash->data + sizeof(prefix) - 1, '$',
                  (size_t)(encoded_hash->length - (sizeof(prefix) - 1))) != NULL;
}

int32_t yaza_crypto_v1_argon2id_hash(const unsigned char *password, uint64_t password_length,
                                     const unsigned char *salt, uint64_t salt_length,
                                     YazaTextV1 **out_text, char **out_error) {
    if (out_text == NULL || out_error == NULL) {
        yaza_rt_v1_fail("crypto_argon2id_hash received a null output pointer");
    }
    *out_text = NULL;
    *out_error = NULL;
    if (password_length > 1024 || (password_length > 0 && password == NULL)) {
        *out_error = (char *)"Argon2id password exceeds the 1024-byte limit";
        return 0;
    }
    if (salt == NULL || salt_length != 16) {
        *out_error = (char *)"Argon2id salt must contain exactly 16 bytes";
        return 0;
    }
#if defined(YAZA_RT_V1_WITH_ARGON2)
    char encoded[256];
    memset(encoded, 0, sizeof(encoded));
    const int status = argon2id_hash_encoded(2, 19456, 1,
        password != NULL ? password : (const unsigned char *)"", (size_t)password_length,
        salt, (size_t)salt_length, 32, encoded, sizeof(encoded));
    if (status != ARGON2_OK) {
        yaza_rt_v1_secure_zero(encoded, sizeof(encoded));
        *out_error = (char *)"Argon2id password hashing failed";
        return 0;
    }
    const size_t encoded_length = strnlen(encoded, sizeof(encoded));
    if (encoded_length == 0 || encoded_length >= sizeof(encoded)) {
        yaza_rt_v1_secure_zero(encoded, sizeof(encoded));
        *out_error = (char *)"Argon2id provider returned an invalid encoded hash";
        return 0;
    }
    *out_text = yaza_text_v1_copy_to_arena(yaza_arena_v1_current(),
        (const unsigned char *)encoded, (uint64_t)encoded_length);
    yaza_rt_v1_secure_zero(encoded, sizeof(encoded));
    if (*out_text == NULL) {
        *out_error = (char *)"Argon2id output exceeded the active arena allocation limit";
        return 0;
    }
    if (!yaza_crypto_v1_argon2id_hash_format_is_current(*out_text)) {
        *out_text = NULL;
        *out_error = (char *)"Argon2id provider returned an unsupported parameter profile";
        return 0;
    }
    return 1;
#else
    *out_error = (char *)"Argon2id provider is unavailable; install the YAZA Argon2 provider package";
    return 0;
#endif
}

int32_t yaza_crypto_v1_argon2id_verify(const unsigned char *password, uint64_t password_length,
                                       const YazaTextV1 *encoded_hash) {
    if (password_length > 1024 || (password_length > 0 && password == NULL) ||
        !yaza_crypto_v1_argon2id_hash_format_is_current(encoded_hash)) {
        return 0;
    }
#if defined(YAZA_RT_V1_WITH_ARGON2)
    return argon2id_verify((const char *)encoded_hash->data,
        password != NULL ? password : (const unsigned char *)"", (size_t)password_length) == ARGON2_OK;
#else
    return 0;
#endif
}

void yaza_bytes_v1_slice(const unsigned char *data, uint64_t length, int64_t start, int64_t slice_length, YazaBytesV1 *out_bytes) {
    if (out_bytes == NULL || start < 0 || slice_length < 0 || (uint64_t)start > length || (uint64_t)slice_length > length - (uint64_t)start) {
        yaza_bytes_v1_fail("Bytes slice is out of range");
    }
    const unsigned char *source = data;
    if (slice_length > 0 && source == NULL) {
        yaza_bytes_v1_fail("Bytes slice source pointer is null");
    }
    if (slice_length > 0) {
        source += (uint64_t)start;
    }
    const YazaBytesV1 result = yaza_bytes_v1_copy(source, (uint64_t)slice_length);
    *out_bytes = result;
}

int32_t yaza_bytes_v1_try_slice(const unsigned char *data, uint64_t length, int64_t start, int64_t slice_length, YazaBytesV1 *out_bytes, char **out_error) {
    if (out_bytes == NULL || out_error == NULL) {
        yaza_bytes_v1_fail("bytes_try_slice received a null output pointer");
    }
    *out_bytes = (YazaBytesV1){NULL, 0};
    *out_error = NULL;
    if (start < 0 || slice_length < 0 || (uint64_t)start > length || (uint64_t)slice_length > length - (uint64_t)start) {
        *out_error = (char *)"Bytes slice is out of range";
        return 0;
    }
    if (length > YAZA_RT_V1_BYTES_MAX_BYTES || (length > 0 && data == NULL)) {
        *out_error = (char *)"Bytes slice source storage is invalid";
        return 0;
    }
    const uint64_t copy_length = (uint64_t)slice_length;
    YazaBytesV1 result = {NULL, copy_length};
    if (copy_length > 0) {
        result.data = (unsigned char *)yaza_rt_v1_try_alloc(copy_length);
        if (result.data == NULL) {
            result.length = 0;
            *out_error = (char *)"Could not allocate sliced Bytes in the active arena";
            return 0;
        }
        memcpy(result.data, data + (uint64_t)start, (size_t)copy_length);
    }
    *out_bytes = result;
    return 1;
}

void yaza_bytes_v1_concat(const unsigned char *left_data, uint64_t left_length, const unsigned char *right_data, uint64_t right_length, YazaBytesV1 *out_bytes) {
    if (out_bytes == NULL || left_length > YAZA_RT_V1_BYTES_MAX_BYTES || right_length > YAZA_RT_V1_BYTES_MAX_BYTES - left_length ||
        (left_length > 0 && left_data == NULL) || (right_length > 0 && right_data == NULL)) {
        yaza_bytes_v1_fail("Bytes concatenation exceeds the limit or contains invalid storage");
    }
    const uint64_t length = left_length + right_length;
    YazaBytesV1 result = {NULL, length};
    if (length > 0) {
        result.data = (unsigned char *)yaza_rt_v1_alloc(length);
        if (left_length > 0) memcpy(result.data, left_data, (size_t)left_length);
        if (right_length > 0) memcpy(result.data + left_length, right_data, (size_t)right_length);
    }
    *out_bytes = result;
}

int32_t yaza_bytes_v1_try_concat(const unsigned char *left_data, uint64_t left_length, const unsigned char *right_data, uint64_t right_length, YazaBytesV1 *out_bytes, char **out_error) {
    if (out_bytes == NULL || out_error == NULL) {
        yaza_bytes_v1_fail("bytes_try_concat received a null output pointer");
    }
    *out_bytes = (YazaBytesV1){NULL, 0};
    *out_error = NULL;
    if (left_length > YAZA_RT_V1_BYTES_MAX_BYTES || right_length > YAZA_RT_V1_BYTES_MAX_BYTES - left_length ||
        (left_length > 0 && left_data == NULL) || (right_length > 0 && right_data == NULL)) {
        *out_error = (char *)"Bytes concatenation exceeds the limit or contains invalid storage";
        return 0;
    }
    const uint64_t length = left_length + right_length;
    YazaBytesV1 result = {NULL, length};
    if (length > 0) {
        result.data = (unsigned char *)yaza_rt_v1_try_alloc(length);
        if (result.data == NULL) {
            result.length = 0;
            *out_error = (char *)"Could not allocate concatenated Bytes in the active arena";
            return 0;
        }
        if (left_length > 0) memcpy(result.data, left_data, (size_t)left_length);
        if (right_length > 0) memcpy(result.data + left_length, right_data, (size_t)right_length);
    }
    *out_bytes = result;
    return 1;
}

int32_t yaza_bytes_v1_equal(const unsigned char *left_data, uint64_t left_length, const unsigned char *right_data, uint64_t right_length) {
    if (left_length != right_length) return 0;
    if (left_length == 0) return 1;
    if (left_data == NULL || right_data == NULL) {
        yaza_bytes_v1_fail("Bytes equality received invalid storage");
    }
    return memcmp(left_data, right_data, (size_t)left_length) == 0;
}

int32_t yaza_bytes_v1_equal_constant_time(const unsigned char *left_data, uint64_t left_length, const unsigned char *right_data, uint64_t right_length) {
    if (left_length != right_length) return 0;
    if (left_length == 0) return 1;
    if (left_data == NULL || right_data == NULL) {
        yaza_bytes_v1_fail("constant-time Bytes equality received invalid storage");
    }
    volatile unsigned char difference = 0;
    for (uint64_t index = 0; index < left_length; ++index) {
        difference |= (unsigned char)(left_data[index] ^ right_data[index]);
    }
    atomic_signal_fence(memory_order_seq_cst);
    return difference == 0;
}

static int yaza_bytes_v1_is_valid_text(const unsigned char *data, uint64_t length) {
    uint64_t index = 0;
    while (index < length) {
        const unsigned char first = data[index];
        if (first == 0) return 0;
        if (first <= 0x7f) {
            index += 1;
            continue;
        }
        if (first >= 0xc2 && first <= 0xdf) {
            if (index + 1 >= length || data[index + 1] < 0x80 || data[index + 1] > 0xbf) return 0;
            index += 2;
            continue;
        }
        if (first >= 0xe0 && first <= 0xef) {
            if (index + 2 >= length) return 0;
            const unsigned char second = data[index + 1];
            const unsigned char third = data[index + 2];
            if (third < 0x80 || third > 0xbf) return 0;
            if (first == 0xe0) {
                if (second < 0xa0 || second > 0xbf) return 0;
            } else if (first == 0xed) {
                if (second < 0x80 || second > 0x9f) return 0;
            } else if (second < 0x80 || second > 0xbf) {
                return 0;
            }
            index += 3;
            continue;
        }
        if (first >= 0xf0 && first <= 0xf4) {
            if (index + 3 >= length) return 0;
            const unsigned char second = data[index + 1];
            const unsigned char third = data[index + 2];
            const unsigned char fourth = data[index + 3];
            if (third < 0x80 || third > 0xbf || fourth < 0x80 || fourth > 0xbf) return 0;
            if (first == 0xf0) {
                if (second < 0x90 || second > 0xbf) return 0;
            } else if (first == 0xf4) {
                if (second < 0x80 || second > 0x8f) return 0;
            } else if (second < 0x80 || second > 0xbf) {
                return 0;
            }
            index += 4;
            continue;
        }
        return 0;
    }
    return 1;
}

#if defined(_WIN32)
static int yaza_console_read_tty_line(unsigned char *buffer, size_t capacity, size_t *out_len) {
    (void)buffer;
    (void)capacity;
    (void)out_len;
    return -1;
}
#else
#define YAZA_CONSOLE_HISTORY_MAX 64

static char *yaza_console_history_entries[YAZA_CONSOLE_HISTORY_MAX];
static size_t yaza_console_history_count = 0;

static void yaza_console_history_push(const unsigned char *buf, size_t len) {
    if (len == 0) return;
    if (yaza_console_history_count > 0) {
        const char *last = yaza_console_history_entries[yaza_console_history_count - 1];
        if (last != NULL && strlen(last) == len && memcmp(last, buf, len) == 0) return;
    }
    char *copy = (char *)malloc(len + 1);
    if (copy == NULL) return;
    memcpy(copy, buf, len);
    copy[len] = '\0';
    if (yaza_console_history_count == YAZA_CONSOLE_HISTORY_MAX) {
        free(yaza_console_history_entries[0]);
        memmove(&yaza_console_history_entries[0], &yaza_console_history_entries[1],
                (YAZA_CONSOLE_HISTORY_MAX - 1) * sizeof(char *));
        yaza_console_history_entries[YAZA_CONSOLE_HISTORY_MAX - 1] = copy;
    } else {
        yaza_console_history_entries[yaza_console_history_count++] = copy;
    }
}

static size_t yaza_utf8_char_count(const unsigned char *buf, size_t byte_len) {
    size_t count = 0;
    for (size_t i = 0; i < byte_len; i++) {
        if ((buf[i] & 0xC0u) != 0x80u) count++;
    }
    return count;
}

static size_t yaza_utf8_prev_boundary(const unsigned char *buf, size_t pos) {
    if (pos == 0) return 0;
    size_t p = pos - 1;
    while (p > 0 && (buf[p] & 0xC0u) == 0x80u) p--;
    return p;
}

static size_t yaza_utf8_next_boundary(const unsigned char *buf, size_t len, size_t pos) {
    if (pos >= len) return len;
    size_t p = pos + 1;
    while (p < len && (buf[p] & 0xC0u) == 0x80u) p++;
    return p;
}

static void yaza_console_redraw_line(const unsigned char *buf, size_t len, size_t cursor) {
    fputs("\r\x1b[K", stdout);
    if (len > 0) (void)fwrite(buf, 1, len, stdout);
    size_t tail_chars = yaza_utf8_char_count(buf + cursor, len - cursor);
    if (tail_chars > 0) fprintf(stdout, "\x1b[%zuD", tail_chars);
    (void)fflush(stdout);
}

static int yaza_console_read_byte_raw(unsigned char *out) {
    for (;;) {
        ssize_t n = read(STDIN_FILENO, out, 1);
        if (n == 1) return 1;
        if (n < 0 && errno == EINTR) continue;
        return 0;
    }
}

static int yaza_console_read_tty_line(unsigned char *buffer, size_t capacity, size_t *out_len) {
    struct termios orig;
    if (tcgetattr(STDIN_FILENO, &orig) != 0) return -1;
    struct termios raw = orig;
    raw.c_iflag &= ~(unsigned long)(ICRNL | IXON);
    raw.c_lflag &= ~(unsigned long)(ICANON | ECHO | IEXTEN);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) return -1;

    size_t length = 0;
    size_t cursor = 0;
    size_t hist_idx = yaza_console_history_count;
    unsigned char draft[4096];
    size_t draft_len = 0;

    for (;;) {
        unsigned char ch = 0;
        if (!yaza_console_read_byte_raw(&ch)) {
            tcsetattr(STDIN_FILENO, TCSANOW, &orig);
            *out_len = 0;
            return 0;
        }
        if (ch == '\r' || ch == '\n') {
            fputs("\r\n", stdout);
            (void)fflush(stdout);
            break;
        }
        if (ch == 4) { /* Ctrl-D */
            if (length == 0) {
                fputs("\r\n", stdout);
                (void)fflush(stdout);
                tcsetattr(STDIN_FILENO, TCSANOW, &orig);
                *out_len = 0;
                return 0;
            }
            continue;
        }
        if (ch == 1) { /* Ctrl-A / Home */
            cursor = 0;
            yaza_console_redraw_line(buffer, length, cursor);
            continue;
        }
        if (ch == 5) { /* Ctrl-E / End */
            cursor = length;
            yaza_console_redraw_line(buffer, length, cursor);
            continue;
        }
        if (ch == 127 || ch == 8) { /* Backspace */
            if (cursor > 0) {
                size_t prev = yaza_utf8_prev_boundary(buffer, cursor);
                memmove(buffer + prev, buffer + cursor, length - cursor);
                length -= (cursor - prev);
                cursor = prev;
                yaza_console_redraw_line(buffer, length, cursor);
            }
            continue;
        }
        if (ch == 27) { /* ESC sequence */
            unsigned char seq[3] = {0, 0, 0};
            if (!yaza_console_read_byte_raw(&seq[0])) continue;
            if (seq[0] == '[' || seq[0] == 'O') {
                if (!yaza_console_read_byte_raw(&seq[1])) continue;
                if (seq[1] == 'A') { /* Up arrow */
                    if (hist_idx > 0) {
                        if (hist_idx == yaza_console_history_count) {
                            draft_len = length < sizeof(draft) ? length : sizeof(draft);
                            if (draft_len > 0) memcpy(draft, buffer, draft_len);
                        }
                        hist_idx--;
                        const char *entry = yaza_console_history_entries[hist_idx];
                        size_t elen = strlen(entry);
                        if (elen > capacity) elen = capacity;
                        memcpy(buffer, entry, elen);
                        length = elen;
                        cursor = length;
                        yaza_console_redraw_line(buffer, length, cursor);
                    }
                } else if (seq[1] == 'B') { /* Down arrow */
                    if (hist_idx < yaza_console_history_count) {
                        hist_idx++;
                        if (hist_idx == yaza_console_history_count) {
                            size_t dlen = draft_len > capacity ? capacity : draft_len;
                            if (dlen > 0) memcpy(buffer, draft, dlen);
                            length = dlen;
                        } else {
                            const char *entry = yaza_console_history_entries[hist_idx];
                            size_t elen = strlen(entry);
                            if (elen > capacity) elen = capacity;
                            memcpy(buffer, entry, elen);
                            length = elen;
                        }
                        cursor = length;
                        yaza_console_redraw_line(buffer, length, cursor);
                    }
                } else if (seq[1] == 'C') { /* Right arrow (next) */
                    size_t next = yaza_utf8_next_boundary(buffer, length, cursor);
                    if (next != cursor) {
                        cursor = next;
                        yaza_console_redraw_line(buffer, length, cursor);
                    }
                } else if (seq[1] == 'D') { /* Left arrow (prev) */
                    size_t prev = yaza_utf8_prev_boundary(buffer, cursor);
                    if (prev != cursor) {
                        cursor = prev;
                        yaza_console_redraw_line(buffer, length, cursor);
                    }
                } else if (seq[1] == 'H') { /* Home */
                    cursor = 0;
                    yaza_console_redraw_line(buffer, length, cursor);
                } else if (seq[1] == 'F') { /* End */
                    cursor = length;
                    yaza_console_redraw_line(buffer, length, cursor);
                } else if (seq[1] >= '0' && seq[1] <= '9') {
                    if (yaza_console_read_byte_raw(&seq[2]) && seq[2] == '~') {
                        if (seq[1] == '3' && cursor < length) { /* Delete */
                            size_t next = yaza_utf8_next_boundary(buffer, length, cursor);
                            memmove(buffer + cursor, buffer + next, length - next);
                            length -= (next - cursor);
                            yaza_console_redraw_line(buffer, length, cursor);
                        } else if (seq[1] == '1') {
                            cursor = 0;
                            yaza_console_redraw_line(buffer, length, cursor);
                        } else if (seq[1] == '4') {
                            cursor = length;
                            yaza_console_redraw_line(buffer, length, cursor);
                        }
                    }
                }
            }
            continue;
        }
        if (ch < 32 && ch != '\t') continue;
        if (length >= capacity) continue;
        if (cursor < length) memmove(buffer + cursor + 1, buffer + cursor, length - cursor);
        buffer[cursor++] = ch;
        length++;
        yaza_console_redraw_line(buffer, length, cursor);
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &orig);
    yaza_console_history_push(buffer, length);
    *out_len = length;
    return 1;
}
#endif

YazaTextV1 *yaza_rt_v1_console_read_line(void) {
    if (fflush(stdout) != 0) {
        fputs("yaza: console_read_line could not flush stdout\n", stderr);
        exit(1);
    }
    const size_t capacity = (size_t)YAZA_RT_V1_CONSOLE_LINE_MAX_BYTES;
    unsigned char *buffer = (unsigned char *)malloc(capacity + 1);
    if (buffer == NULL) {
        fputs("yaza: console_read_line could not allocate its bounded input buffer\n", stderr);
        exit(1);
    }

    size_t length = 0;
#if !defined(_WIN32)
    if (isatty(STDIN_FILENO)) {
#else
    if (0) {
#endif
        int tty_res = yaza_console_read_tty_line(buffer, capacity, &length);
        if (tty_res == 0) {
            free(buffer);
            return NULL;
        }
        if (tty_res > 0) {
            if (!yaza_bytes_v1_is_valid_text(buffer, (uint64_t)length)) {
                free(buffer);
                fputs("yaza: console input is not valid NUL-free UTF-8 Text\n", stderr);
                exit(1);
            }
            YazaTextV1 *line = yaza_text_v1_copy_to_arena(yaza_arena_v1_current(), buffer, (uint64_t)length);
            free(buffer);
            if (line == NULL) {
                fputs("yaza: console_read_line exceeded the active arena allocation limit\n", stderr);
                exit(1);
            }
            return line;
        }
    }

    int character = EOF;
    for (;;) {
        character = fgetc(stdin);
        if (character == EOF || character == '\n') break;
        if (length == capacity) {
            while (character != EOF && character != '\n') character = fgetc(stdin);
            free(buffer);
            fputs("yaza: console input line exceeds the 1 MiB limit\n", stderr);
            exit(1);
        }
        buffer[length++] = (unsigned char)character;
    }
    if (character == EOF && ferror(stdin)) {
        free(buffer);
        fputs("yaza: console_read_line failed while reading stdin\n", stderr);
        exit(1);
    }
    if (character == EOF && length == 0) {
        free(buffer);
        return NULL;
    }
    if (length > 0 && buffer[length - 1] == '\r') length -= 1;
    if (!yaza_bytes_v1_is_valid_text(buffer, (uint64_t)length)) {
        free(buffer);
        fputs("yaza: console input is not valid NUL-free UTF-8 Text\n", stderr);
        exit(1);
    }
    YazaTextV1 *line = yaza_text_v1_copy_to_arena(yaza_arena_v1_current(), buffer, (uint64_t)length);
    free(buffer);
    if (line == NULL) {
        fputs("yaza: console_read_line exceeded the active arena allocation limit\n", stderr);
        exit(1);
    }
    return line;
}

int32_t yaza_rt_v1_module_text_copy(const YazaTextV1 *source, YazaTextV1 **out_text) {
    if (out_text == NULL) return YAZA_MODULE_RESULT_V1_INVALID_INPUT;
    *out_text = NULL;
    if (!yaza_rt_v1_module_request_scope_active()) return YAZA_MODULE_RESULT_V1_NO_REQUEST_SCOPE;
    if (source == NULL || source->length > YAZA_RT_V1_BYTES_MAX_BYTES ||
        (source->length > 0 && source->data == NULL) ||
        !yaza_bytes_v1_is_valid_text(source->data, source->length)) {
        return YAZA_MODULE_RESULT_V1_INVALID_INPUT;
    }
    YazaTextV1 *copy = yaza_text_v1_copy_to_arena(yaza_rt_v1_current_arena, source->data, source->length);
    if (copy == NULL) return YAZA_MODULE_RESULT_V1_RESOURCE_LIMIT;
    *out_text = copy;
    return YAZA_MODULE_RESULT_V1_OK;
}

int32_t yaza_rt_v1_module_bytes_copy(const YazaBytesV1 *source, YazaBytesV1 *out_bytes) {
    if (out_bytes == NULL) return YAZA_MODULE_RESULT_V1_INVALID_INPUT;
    *out_bytes = (YazaBytesV1){NULL, 0};
    if (!yaza_rt_v1_module_request_scope_active()) return YAZA_MODULE_RESULT_V1_NO_REQUEST_SCOPE;
    if (source == NULL || source->length > YAZA_RT_V1_BYTES_MAX_BYTES ||
        (source->length > 0 && source->data == NULL)) {
        return YAZA_MODULE_RESULT_V1_INVALID_INPUT;
    }
    YazaBytesV1 copy = {NULL, source->length};
    if (source->length > 0) {
        copy.data = (unsigned char *)yaza_arena_v1_alloc(yaza_rt_v1_current_arena, source->length);
        if (copy.data == NULL) return YAZA_MODULE_RESULT_V1_RESOURCE_LIMIT;
        memcpy(copy.data, source->data, (size_t)source->length);
    }
    *out_bytes = copy;
    return YAZA_MODULE_RESULT_V1_OK;
}

static int32_t yaza_rt_v1_module_input_validate(const YazaModuleInputV1 *source, uint32_t expected_kind) {
    if (!yaza_rt_v1_module_request_scope_active()) return YAZA_MODULE_RESULT_V1_NO_REQUEST_SCOPE;
    if (source == NULL || source->abi_version != YAZA_MODULE_INPUT_V1_ABI_VERSION ||
        source->struct_size < sizeof(*source) || source->kind != expected_kind || source->reserved != 0) {
        return YAZA_MODULE_RESULT_V1_INVALID_INPUT;
    }
    return YAZA_MODULE_RESULT_V1_OK;
}

int32_t yaza_rt_v1_module_text_input_copy(const YazaModuleInputV1 *source, YazaTextV1 **out_text) {
    if (out_text == NULL) return YAZA_MODULE_RESULT_V1_INVALID_INPUT;
    *out_text = NULL;
    const int32_t status = yaza_rt_v1_module_input_validate(source, YAZA_MODULE_INPUT_V1_TEXT);
    if (status != YAZA_MODULE_RESULT_V1_OK) return status;
    const YazaTextV1 value = {source->data, source->length};
    return yaza_rt_v1_module_text_copy(&value, out_text);
}

int32_t yaza_rt_v1_module_bytes_input_copy(const YazaModuleInputV1 *source, YazaBytesV1 *out_bytes) {
    if (out_bytes == NULL) return YAZA_MODULE_RESULT_V1_INVALID_INPUT;
    *out_bytes = (YazaBytesV1){NULL, 0};
    const int32_t status = yaza_rt_v1_module_input_validate(source, YAZA_MODULE_INPUT_V1_BYTES);
    if (status != YAZA_MODULE_RESULT_V1_OK) return status;
    const YazaBytesV1 value = {(unsigned char *)source->data, source->length};
    return yaza_rt_v1_module_bytes_copy(&value, out_bytes);
}

int32_t yaza_bytes_v1_to_text_checked(const unsigned char *data, uint64_t length, YazaTextV1 **out_text) {
    if (out_text == NULL) {
        yaza_bytes_v1_fail("bytes_to_text_checked output pointer is null");
    }
    YazaTextV1 *text = NULL;
    if (length > YAZA_RT_V1_BYTES_MAX_BYTES || (length > 0 && data == NULL) || !yaza_bytes_v1_is_valid_text(data, length)) {
        *out_text = text;
        return 0;
    }
    text = yaza_text_v1_copy_to_arena(yaza_arena_v1_current(), data, length);
    if (text == NULL) yaza_bytes_v1_fail("arena limit exceeded converting Bytes to Text");
    *out_text = text;
    return 1;
}

int32_t yaza_bytes_v1_try_to_text(const unsigned char *data, uint64_t length, YazaTextV1 **out_text, char **out_error) {
    if (out_text == NULL || out_error == NULL) {
        yaza_bytes_v1_fail("bytes_try_to_text output pointer is null");
    }
    *out_text = NULL;
    *out_error = NULL;
    if (length > YAZA_RT_V1_BYTES_MAX_BYTES) {
        *out_error = (char *)"Bytes exceeds the 64 MiB Text limit";
        return 0;
    }
    if ((length > 0 && data == NULL) || !yaza_bytes_v1_is_valid_text(data, length)) {
        *out_error = (char *)"Bytes value is not valid non-NUL UTF-8 Text";
        return 0;
    }
    if (length > (uint64_t)SIZE_MAX - (uint64_t)sizeof(YazaTextV1) - 1) {
        *out_error = (char *)"Bytes is too large to convert to Text";
        return 0;
    }
    const uint64_t allocation_size = (uint64_t)sizeof(YazaTextV1) + length + 1;
    YazaTextV1 *text = (YazaTextV1 *)yaza_rt_v1_try_alloc(allocation_size);
    if (text == NULL) {
        *out_error = (char *)"Could not allocate Text in the active arena";
        return 0;
    }
    unsigned char *copy = (unsigned char *)(text + 1);
    if (length > 0) memcpy(copy, data, (size_t)length);
    copy[length] = '\0';
    text->data = copy;
    text->length = length;
    *out_text = text;
    return 1;
}

YazaTextV1 *yaza_bytes_v1_to_text(const unsigned char *data, uint64_t length) {
    if (length > YAZA_RT_V1_BYTES_MAX_BYTES || (length > 0 && data == NULL) || !yaza_bytes_v1_is_valid_text(data, length)) {
        fputs("yaza: Bytes value is not valid non-NUL UTF-8 Text\n", stderr);
        exit(1);
    }
    YazaTextV1 *text = yaza_text_v1_copy_to_arena(yaza_arena_v1_current(), data, length);
    if (text == NULL) yaza_bytes_v1_fail("arena limit exceeded converting Bytes to Text");
    return text;
}

int64_t yaza_bytes_v1_zeroize(unsigned char *data, uint64_t length) {
    if (data != NULL) {
        volatile unsigned char *cursor = (volatile unsigned char *)data;
        for (uint64_t index = 0; index < length; ++index) {
            cursor[index] = 0;
        }
        atomic_signal_fence(memory_order_seq_cst);
    }
    return 0;
}

static const uint32_t yaza_bytes_v1_ydbv1_nibble_table[16] = {
    0x00000000U, 0x105ec76fU, 0x20bd8edeU, 0x30e34991U,
    0x417b1dbcU, 0x5125dad3U, 0x61c69302U, 0x7198546dU,
    0x82f63b78U, 0x92a8fc17U, 0xa24bb596U, 0xb21572f9U,
    0xc38d26c4U, 0xd3d3e1abU, 0xe330a81aU, 0xf36e6f75U
};
static const uint32_t yaza_bytes_v1_crc32c_nibble_table[16] = {
    0x00000000U, 0x105ec76fU, 0x20bd8edeU, 0x30e349b1U,
    0x417b1dbcU, 0x5125dad3U, 0x61c69362U, 0x7198540dU,
    0x82f63b78U, 0x92a8fc17U, 0xa24bb5a6U, 0xb21572c9U,
    0xc38d26c4U, 0xd3d3e1abU, 0xe330a81aU, 0xf36e6f75U
};
static uint32_t yaza_bytes_v1_ydbv1_byte_table[256];
static uint32_t yaza_bytes_v1_ydbv1_slice_table[8][256];
static uint32_t yaza_bytes_v1_crc32c_byte_table[256];
static uint32_t yaza_bytes_v1_crc32c_slice_table[8][256];
static pthread_once_t yaza_bytes_v1_crc_tables_once = PTHREAD_ONCE_INIT;

static void yaza_bytes_v1_crc_byte_table_build(uint32_t output[256], const uint32_t nibble_table[16]) {
    for (uint32_t index = 0; index < 256; ++index) {
        uint32_t value = index;
        value = (value >> 4) ^ nibble_table[value & 0x0fU];
        value = (value >> 4) ^ nibble_table[value & 0x0fU];
        output[index] = value;
    }
}

static void yaza_bytes_v1_crc_tables_init(void) {
    yaza_bytes_v1_crc_byte_table_build(yaza_bytes_v1_ydbv1_byte_table, yaza_bytes_v1_ydbv1_nibble_table);
    yaza_bytes_v1_crc_byte_table_build(yaza_bytes_v1_crc32c_byte_table, yaza_bytes_v1_crc32c_nibble_table);
    for (uint32_t index = 0; index < 256; ++index) {
        yaza_bytes_v1_ydbv1_slice_table[0][index] = yaza_bytes_v1_ydbv1_byte_table[index];
        yaza_bytes_v1_crc32c_slice_table[0][index] = yaza_bytes_v1_crc32c_byte_table[index];
    }
    for (uint32_t slice = 1; slice < 8; ++slice) {
        for (uint32_t index = 0; index < 256; ++index) {
            const uint32_t ydbv1_previous = yaza_bytes_v1_ydbv1_slice_table[slice - 1][index];
            yaza_bytes_v1_ydbv1_slice_table[slice][index] =
                (ydbv1_previous >> 8) ^ yaza_bytes_v1_ydbv1_byte_table[ydbv1_previous & 0xffU];
        }
        for (uint32_t index = 0; index < 256; ++index) {
            const uint32_t crc32c_previous = yaza_bytes_v1_crc32c_slice_table[slice - 1][index];
            yaza_bytes_v1_crc32c_slice_table[slice][index] =
                (crc32c_previous >> 8) ^ yaza_bytes_v1_crc32c_byte_table[crc32c_previous & 0xffU];
        }
    }
}

static uint32_t yaza_bytes_v1_crc_update(uint32_t crc, const unsigned char *data, uint64_t length,
                                         const uint32_t table[256], const uint32_t slice_table[8][256]) {
    while (slice_table != NULL && length >= 8) {
        const uint32_t first_word = crc ^ (uint32_t)data[0] ^ ((uint32_t)data[1] << 8) ^
                                    ((uint32_t)data[2] << 16) ^ ((uint32_t)data[3] << 24);
        crc = slice_table[7][first_word & 0xffU] ^
              slice_table[6][(first_word >> 8) & 0xffU] ^
              slice_table[5][(first_word >> 16) & 0xffU] ^
              slice_table[4][(first_word >> 24) & 0xffU] ^
              slice_table[3][data[4]] ^ slice_table[2][data[5]] ^
              slice_table[1][data[6]] ^ slice_table[0][data[7]];
        data += 8;
        length -= 8;
    }
    while (length >= 8) {
        crc = (crc >> 8) ^ table[(crc ^ data[0]) & 0xffU];
        crc = (crc >> 8) ^ table[(crc ^ data[1]) & 0xffU];
        crc = (crc >> 8) ^ table[(crc ^ data[2]) & 0xffU];
        crc = (crc >> 8) ^ table[(crc ^ data[3]) & 0xffU];
        crc = (crc >> 8) ^ table[(crc ^ data[4]) & 0xffU];
        crc = (crc >> 8) ^ table[(crc ^ data[5]) & 0xffU];
        crc = (crc >> 8) ^ table[(crc ^ data[6]) & 0xffU];
        crc = (crc >> 8) ^ table[(crc ^ data[7]) & 0xffU];
        data += 8;
        length -= 8;
    }
    for (uint64_t index = 0; index < length; ++index) {
        crc = (crc >> 8) ^ table[(crc ^ data[index]) & 0xffU];
    }
    return crc;
}

static uint32_t yaza_bytes_v1_crc_update_zeros(uint32_t crc, uint64_t length,
                                              const uint32_t table[256], const uint32_t slice_table[8][256]) {
    while (slice_table != NULL && length >= 8) {
        crc = slice_table[7][crc & 0xffU] ^
              slice_table[6][(crc >> 8) & 0xffU] ^
              slice_table[5][(crc >> 16) & 0xffU] ^
              slice_table[4][crc >> 24] ^ slice_table[3][0] ^ slice_table[2][0] ^
              slice_table[1][0] ^ slice_table[0][0];
        length -= 8;
    }
    while (length > 0) {
        crc = (crc >> 8) ^ table[crc & 0xffU];
        --length;
    }
    return crc;
}

static uint32_t yaza_bytes_v1_crc_zeroed_range_impl(const unsigned char *data, uint64_t length,
                                                    uint64_t zero_offset, uint64_t zero_length,
                                                    const uint32_t table[256], const uint32_t slice_table[8][256]) {
    if (pthread_once(&yaza_bytes_v1_crc_tables_once, yaza_bytes_v1_crc_tables_init) != 0) {
        yaza_rt_v1_fail("CRC lookup table initialization failed");
    }
    uint32_t crc = UINT32_MAX;
    if (zero_length == 0) return yaza_bytes_v1_crc_update(crc, data, length, table, slice_table) ^ UINT32_MAX;
    crc = yaza_bytes_v1_crc_update(crc, data, zero_offset, table, slice_table);
    crc = yaza_bytes_v1_crc_update_zeros(crc, zero_length, table, slice_table);
    const uint64_t suffix_offset = zero_offset + zero_length;
    const unsigned char *suffix = data;
    if (suffix_offset > 0) suffix = data + suffix_offset;
    crc = yaza_bytes_v1_crc_update(crc, suffix, length - suffix_offset, table, slice_table);
    return crc ^ UINT32_MAX;
}

static uint32_t yaza_bytes_v1_ydbv1_checksum_impl(const unsigned char *data, uint64_t length,
                                                 uint64_t zero_offset, uint64_t zero_length) {
    return yaza_bytes_v1_crc_zeroed_range_impl(data, length, zero_offset, zero_length,
                                              yaza_bytes_v1_ydbv1_byte_table, NULL);
}

static uint32_t yaza_bytes_v1_crc32c_impl(const unsigned char *data, uint64_t length,
                                          uint64_t zero_offset, uint64_t zero_length) {
    return yaza_bytes_v1_crc_zeroed_range_impl(data, length, zero_offset, zero_length,
                                               yaza_bytes_v1_crc32c_byte_table, yaza_bytes_v1_crc32c_slice_table);
}

uint64_t yaza_bytes_v1_crc32c(const unsigned char *data, uint64_t length) {
    if (length > YAZA_RT_V1_BYTES_MAX_BYTES || (length > 0 && data == NULL)) {
        yaza_bytes_v1_fail("bytes_crc32c received an invalid Bytes value");
    }
    return yaza_bytes_v1_crc32c_impl(data, length, 0, 0);
}

uint64_t yaza_bytes_v1_crc32c_zeroed_range(const unsigned char *data, uint64_t length, int64_t zero_offset, int64_t zero_length) {
    if (length > YAZA_RT_V1_BYTES_MAX_BYTES || (length > 0 && data == NULL) || zero_offset < 0 || zero_length < 0 ||
        (uint64_t)zero_offset > length || (uint64_t)zero_length > length - (uint64_t)zero_offset) {
        yaza_bytes_v1_fail("bytes_crc32c_zeroed_range received an invalid Bytes range");
    }
    return yaza_bytes_v1_crc32c_impl(data, length, (uint64_t)zero_offset, (uint64_t)zero_length);
}

uint64_t yaza_bytes_v1_ydbv1_checksum(const unsigned char *data, uint64_t length) {
    if (length > YAZA_RT_V1_BYTES_MAX_BYTES || (length > 0 && data == NULL)) {
        yaza_bytes_v1_fail("bytes_ydbv1_checksum received an invalid Bytes value");
    }
    return yaza_bytes_v1_ydbv1_checksum_impl(data, length, 0, 0);
}

uint64_t yaza_bytes_v1_ydbv1_checksum_zeroed_range(const unsigned char *data, uint64_t length, int64_t zero_offset, int64_t zero_length) {
    if (length > YAZA_RT_V1_BYTES_MAX_BYTES || (length > 0 && data == NULL) || zero_offset < 0 || zero_length < 0 ||
        (uint64_t)zero_offset > length || (uint64_t)zero_length > length - (uint64_t)zero_offset) {
        yaza_bytes_v1_fail("bytes_ydbv1_checksum_zeroed_range received an invalid Bytes range");
    }
    return yaza_bytes_v1_ydbv1_checksum_impl(data, length, (uint64_t)zero_offset, (uint64_t)zero_length);
}

int32_t yaza_bytes_v1_from_text_bounded(const YazaTextV1 *text, int64_t maximum_bytes, YazaBytesV1 *out_bytes, char **out_error) {
    if (text == NULL || out_bytes == NULL || out_error == NULL) {
        yaza_bytes_v1_fail("bytes_from_text_bounded received a null pointer");
    }
    yaza_text_v1_require(text, "bytes_from_text_bounded");
    YazaBytesV1 empty = {NULL, 0};
    char *error = NULL;
    if (maximum_bytes < 0 || (uint64_t)maximum_bytes > YAZA_RT_V1_BYTES_MAX_BYTES) {
        error = (char *)"Text byte limit is outside the supported range";
    } else {
        const uint64_t length = text->length;
        if (length > (uint64_t)maximum_bytes) {
            error = (char *)"Text exceeds the configured byte limit";
        } else {
            empty.length = length;
            if (length > 0) {
                empty.data = (unsigned char *)yaza_arena_v1_alloc(yaza_arena_v1_current(), length);
                if (empty.data == NULL) {
                    empty = (YazaBytesV1){NULL, 0};
                    error = (char *)"Could not allocate bounded Bytes in the active arena";
                    *out_bytes = empty;
                    *out_error = error;
                    return 0;
                }
                memcpy(empty.data, text->data, (size_t)length);
            }
            *out_bytes = empty;
            *out_error = error;
            return 1;
        }
    }
    *out_bytes = empty;
    *out_error = error;
    return 0;
}

static void yaza_list_v1_fail(const char *message) {
    fprintf(stderr, "YAZA runtime v1: %s\n", message);
    abort();
}

static YazaListV1 *yaza_list_v1_begin(void *opaque, uint64_t expected_type) {
    YazaListV1 *list = (YazaListV1 *)opaque;
    if (list == NULL || list->owner == NULL) {
        yaza_list_v1_fail("invalid List handle");
    }
    if (!yaza_rt_v1_retain_for_current_access(list->owner)) {
        yaza_list_v1_fail("List owner region is closed");
    }
    yaza_rt_v1_lock(&list->lock);
    if (list->owner_generation != yaza_arena_v1_generation(list->owner)) {
        yaza_rt_v1_unlock(&list->lock);
        yaza_rt_v1_release_current_access(list->owner);
        yaza_list_v1_fail("List used after its arena generation changed");
    }
    if (list->collection_type != NULL || list->element_size != sizeof(uint64_t) || list->element_type_id != expected_type) {
        yaza_rt_v1_unlock(&list->lock);
        yaza_rt_v1_release_current_access(list->owner);
        yaza_list_v1_fail("List element type tag mismatch");
    }
    return list;
}

static void yaza_list_v1_end(YazaListV1 *list) {
    YazaArenaV1 *owner = list->owner;
    yaza_rt_v1_unlock(&list->lock);
    if (!yaza_rt_v1_release_current_access(owner)) {
        yaza_list_v1_fail("List owner lease accounting failed");
    }
}

/* Preserve active borrowed List buffers before any typed List mutation. */
static int32_t yaza_list_v1_prepare_typed_write(YazaListV1 *list, uint64_t target_capacity) {
    if (list->current_storage_borrows == 0) return 1;
    if (target_capacity < list->length || target_capacity > SIZE_MAX / list->element_size) return 0;
    if (target_capacity == 0) {
        list->items = NULL;
        list->capacity = 0;
    } else {
        const uint64_t allocation_size = target_capacity * list->element_size;
        unsigned char *items = (unsigned char *)yaza_arena_v1_alloc(list->owner, allocation_size);
        if (items == NULL) return 0;
        if (list->length > 0) {
            memcpy(items, list->items, (size_t)(list->length * list->element_size));
        }
        list->items = items;
        list->capacity = target_capacity;
    }
    list->storage_generation += 1;
    list->current_storage_borrows = 0;
    return 1;
}

static void yaza_list_v1_note_typed_mutation(YazaListV1 *list) {
    (void)atomic_fetch_add_explicit(&list->mutation_generation, 1, memory_order_release);
}

static YazaListV1 *yaza_list_v1_begin_typed(void *opaque, const char *collection_type, uint64_t element_size) {
    YazaListV1 *list = (YazaListV1 *)opaque;
    if (list == NULL || list->owner == NULL || collection_type == NULL || collection_type[0] == '\0' || element_size == 0) {
        yaza_list_v1_fail("invalid typed List descriptor");
    }
    if (!yaza_rt_v1_retain_for_current_access(list->owner)) {
        yaza_list_v1_fail("typed List owner region is closed");
    }
    yaza_rt_v1_lock(&list->lock);
    if (list->owner_generation != yaza_arena_v1_generation(list->owner)) {
        yaza_rt_v1_unlock(&list->lock);
        yaza_rt_v1_release_current_access(list->owner);
        yaza_list_v1_fail("typed List used after its arena generation changed");
    }
    if (list->collection_type == NULL || list->element_size != element_size ||
        (list->collection_type != collection_type && strcmp(list->collection_type, collection_type) != 0)) {
        yaza_rt_v1_unlock(&list->lock);
        yaza_rt_v1_release_current_access(list->owner);
        yaza_list_v1_fail("typed List collection signature mismatch");
    }
    return list;
}

static int32_t yaza_list_v1_try_store_typed_item(YazaListV1 *list, const void *item, unsigned char *destination) {
    if (strcmp(list->collection_type, "List[Text]") == 0 || strcmp(list->collection_type, "TextList") == 0) {
        if (list->element_size != sizeof(char *)) {
            yaza_list_v1_fail("Text list element size mismatch");
        }
        const YazaTextV1 *text = NULL;
        memcpy(&text, item, sizeof(text));
        if (text == NULL) {
            return 0;
        }
        yaza_text_v1_require(text, "typed List[Text] store");
        YazaTextV1 *copy = yaza_text_v1_copy_to_arena(list->owner, text->data, text->length);
        if (copy == NULL) {
            return 0;
        }
        memcpy(destination, &copy, sizeof(copy));
        return 1;
    }
    memcpy(destination, item, (size_t)list->element_size);
    return 1;
}

static void yaza_list_v1_store_typed_item(YazaListV1 *list, const void *item, unsigned char *destination) {
    if (!yaza_list_v1_try_store_typed_item(list, item, destination)) {
        yaza_list_v1_fail("arena limit exceeded or invalid typed List item");
    }
}

static uint64_t yaza_list_v1_legacy_max_items(void) {
    uint64_t maximum_items = yaza_rt_v1_config.max_list_vector_bytes / sizeof(uint64_t);
    if (maximum_items > yaza_rt_v1_config.max_list_items) {
        maximum_items = yaza_rt_v1_config.max_list_items;
    }
    return maximum_items;
}

void *yaza_list_v1_create_typed(const char *collection_type, uint64_t element_size) {
    if (collection_type == NULL || collection_type[0] == '\0' || element_size == 0 || element_size > yaza_rt_v1_config.max_list_vector_bytes) {
        yaza_list_v1_fail("invalid typed List element descriptor");
    }
    YazaArenaV1 *owner = yaza_arena_v1_current();
    YazaListV1 *list = (YazaListV1 *)yaza_arena_v1_alloc(owner, sizeof(YazaListV1));
    if (list == NULL) {
        yaza_list_v1_fail("arena limit exceeded creating typed List");
    }
    memset(list, 0, sizeof(*list));
    atomic_init(&list->lock, false);
    atomic_init(&list->mutation_generation, 0);
    list->owner = owner;
    list->owner_generation = yaza_arena_v1_generation(owner);
    const size_t signature_length = strlen(collection_type);
    char *owned_signature = (char *)yaza_arena_v1_alloc(owner, (uint64_t)signature_length + 1);
    if (owned_signature == NULL) {
        yaza_list_v1_fail("arena limit exceeded copying typed List signature");
    }
    memcpy(owned_signature, collection_type, signature_length + 1);
    list->collection_type = owned_signature;
    list->element_size = element_size;
    return list;
}

void *yaza_list_v1_try_create_typed(const char *collection_type, uint64_t element_size) {
    yaza_rt_v1_init();
    if (collection_type == NULL || collection_type[0] == '\0' || element_size == 0 || element_size > yaza_rt_v1_config.max_list_vector_bytes) {
        return NULL;
    }

    size_t signature_length = 0;
    const uint64_t configured_limit = yaza_rt_v1_config.max_list_vector_bytes;
    const size_t signature_limit = configured_limit > (uint64_t)SIZE_MAX ? SIZE_MAX : (size_t)configured_limit;
    while (signature_length < signature_limit && collection_type[signature_length] != '\0') {
        signature_length += 1;
    }
    if (signature_length == 0 || signature_length == signature_limit ||
        signature_length > SIZE_MAX - sizeof(YazaListV1) - 1U) {
        return NULL;
    }

    const size_t allocation_size = sizeof(YazaListV1) + signature_length + 1U;
    YazaArenaV1 *owner = yaza_arena_v1_current();
    YazaListV1 *list = (YazaListV1 *)yaza_arena_v1_alloc(owner, (uint64_t)allocation_size);
    if (list == NULL) {
        return NULL;
    }

    memset(list, 0, sizeof(*list));
    atomic_init(&list->lock, false);
    atomic_init(&list->mutation_generation, 0);
    list->owner = owner;
    list->owner_generation = yaza_arena_v1_generation(owner);
    char *owned_signature = (char *)(list + 1);
    memcpy(owned_signature, collection_type, signature_length + 1U);
    list->collection_type = owned_signature;
    list->element_size = element_size;
    return list;
}

uint64_t yaza_list_v1_count_typed(void *opaque, const char *collection_type, uint64_t element_size) {
    YazaListV1 *list = yaza_list_v1_begin_typed(opaque, collection_type, element_size);
    const uint64_t result = list->length;
    yaza_list_v1_end(list);
    return result;
}

void yaza_list_v1_iter_begin_typed(void *opaque, const char *collection_type, uint64_t element_size, YazaListBorrowV1 *borrow) {
    if (borrow == NULL || borrow->list != NULL || borrow->owner != NULL || borrow->items != NULL || borrow->mutation_generation_address != NULL || borrow->length != 0 || borrow->element_size != 0 || borrow->storage_generation != 0 || borrow->mutation_generation != 0) {
        yaza_list_v1_fail("invalid typed List iterator state");
    }
    YazaListV1 *list = yaza_list_v1_begin_typed(opaque, collection_type, element_size);
    borrow->list = list;
    borrow->owner = list->owner;
    borrow->items = list->items;
    borrow->mutation_generation_address = &list->mutation_generation;
    borrow->length = list->length;
    borrow->element_size = list->element_size;
    borrow->storage_generation = list->storage_generation;
    borrow->mutation_generation = atomic_load_explicit(&list->mutation_generation, memory_order_relaxed);
    list->current_storage_borrows += 1;
    /* Keep the arena lease and immutable backing buffer for the whole loop. */
    yaza_rt_v1_unlock(&list->lock);
}

uint64_t yaza_list_v1_iter_count(const YazaListBorrowV1 *borrow) {
    if (borrow == NULL || borrow->list == NULL || borrow->owner == NULL || borrow->element_size == 0) {
        yaza_list_v1_fail("invalid typed List iterator state");
    }
    return borrow->length;
}

int32_t yaza_list_v1_iter_get_typed(YazaListBorrowV1 *borrow, int64_t index, void *out_value) {
    if (borrow == NULL || borrow->list == NULL || borrow->owner == NULL || borrow->element_size == 0 || out_value == NULL) {
        yaza_list_v1_fail("invalid typed List iterator read");
    }
    YazaListV1 *list = (YazaListV1 *)borrow->list;
    if (index < 0 || (uint64_t)index >= borrow->length) {
        yaza_list_v1_fail("typed List iterator index is invalid");
    }
    if (atomic_load_explicit(&list->mutation_generation, memory_order_acquire) == borrow->mutation_generation) {
        memcpy(out_value, borrow->items + (uint64_t)index * borrow->element_size, (size_t)borrow->element_size);
        return 0;
    }
    /* Preserve live-update behavior when a List changes during iteration. */
    list = yaza_list_v1_begin_typed(list, list->collection_type, borrow->element_size);
    if (list->owner != borrow->owner || (uint64_t)index >= list->length) {
        yaza_list_v1_end(list);
        yaza_list_v1_fail("typed List iterator descriptor is invalid");
    }
    memcpy(out_value, list->items + (uint64_t)index * list->element_size, (size_t)list->element_size);
    yaza_list_v1_end(list);
    return 0;
}

void yaza_list_v1_iter_end(YazaListBorrowV1 *borrow) {
    if (borrow == NULL) {
        yaza_list_v1_fail("invalid typed List iterator state");
    }
    if (borrow->list == NULL && borrow->owner == NULL) return;
    if (borrow->list == NULL || borrow->owner == NULL) {
        yaza_list_v1_fail("invalid typed List iterator state");
    }
    YazaListV1 *list = (YazaListV1 *)borrow->list;
    yaza_rt_v1_lock(&list->lock);
    if (borrow->storage_generation == list->storage_generation) {
        if (list->current_storage_borrows == 0) {
            yaza_rt_v1_unlock(&list->lock);
            yaza_list_v1_fail("typed List iterator borrow accounting failed");
        }
        list->current_storage_borrows -= 1;
    }
    yaza_rt_v1_unlock(&list->lock);
    YazaArenaV1 *owner = borrow->owner;
    borrow->list = NULL;
    borrow->owner = NULL;
    borrow->items = NULL;
    borrow->mutation_generation_address = NULL;
    borrow->length = 0;
    borrow->element_size = 0;
    borrow->storage_generation = 0;
    borrow->mutation_generation = 0;
    if (!yaza_rt_v1_release_current_access(owner)) {
        yaza_list_v1_fail("typed List iterator lease accounting failed");
    }
}

void yaza_list_v1_read_begin_typed(void *opaque, const char *collection_type, uint64_t element_size, YazaListBorrowV1 *borrow) {
    yaza_list_v1_iter_begin_typed(opaque, collection_type, element_size, borrow);
}

YazaListBorrowV1 *yaza_list_v1_read_begin_or_share_typed(void *opaque, const char *collection_type, uint64_t element_size, YazaListBorrowV1 *parent_borrow, YazaListBorrowV1 *local_borrow) {
    if (local_borrow == NULL || local_borrow->list != NULL || local_borrow->owner != NULL || element_size == 0 || collection_type == NULL) {
        yaza_list_v1_fail("invalid shared typed List read view state");
    }
    if (parent_borrow != NULL && parent_borrow->list == opaque && parent_borrow->owner != NULL && parent_borrow->element_size == element_size) {
        YazaListV1 *list = (YazaListV1 *)parent_borrow->list;
        if (list->owner == parent_borrow->owner && list->collection_type != NULL && strcmp(list->collection_type, collection_type) == 0) {
            return parent_borrow;
        }
    }
    yaza_list_v1_read_begin_typed(opaque, collection_type, element_size, local_borrow);
    return local_borrow;
}

void yaza_list_v1_mut_begin_typed(void *opaque, const char *collection_type, uint64_t element_size, YazaListMutableBorrowV1 *borrow) {
    if (borrow == NULL || borrow->list != NULL || borrow->items != NULL || borrow->length != 0) {
        yaza_list_v1_fail("invalid typed List mutable view state");
    }
    YazaListV1 *list = yaza_list_v1_begin_typed(opaque, collection_type, element_size);
    if (!yaza_list_v1_prepare_typed_write(list, list->capacity)) {
        yaza_list_v1_end(list);
        yaza_list_v1_fail("arena limit exceeded preserving typed List read view");
    }
    /* Invalidate any existing lock-free readers before the exclusive writes begin. */
    (void)atomic_fetch_add_explicit(&list->mutation_generation, 1, memory_order_release);
    borrow->list = list;
    borrow->items = list->items;
    borrow->length = list->length;
    /* Keep both the List lock and the owner lease until mut_end. */
}

void yaza_list_v1_mut_end(YazaListMutableBorrowV1 *borrow) {
    if (borrow == NULL) {
        yaza_list_v1_fail("invalid typed List mutable view state");
    }
    if (borrow->list == NULL && borrow->items == NULL && borrow->length == 0) return;
    if (borrow->list == NULL) {
        yaza_list_v1_fail("invalid typed List mutable view state");
    }
    YazaListV1 *list = (YazaListV1 *)borrow->list;
    borrow->list = NULL;
    borrow->items = NULL;
    borrow->length = 0;
    yaza_list_v1_end(list);
}

static void yaza_list_v1_iter_end_locked(YazaListBorrowV1 *borrow, YazaListV1 *list) {
    if (borrow == NULL || list == NULL || borrow->list != list || borrow->owner != list->owner) {
        yaza_list_v1_fail("invalid typed List iterator state while ending nested borrow");
    }
    if (borrow->storage_generation == list->storage_generation) {
        if (list->current_storage_borrows == 0) {
            yaza_list_v1_fail("typed List iterator borrow accounting failed");
        }
        list->current_storage_borrows -= 1;
    }
    YazaArenaV1 *owner = borrow->owner;
    memset(borrow, 0, sizeof(*borrow));
    if (!yaza_rt_v1_release_current_access(owner)) {
        yaza_list_v1_fail("typed List iterator lease accounting failed");
    }
}

int32_t yaza_list_v1_nested_mut_begin_typed(void *opaque, const char *outer_type, uint64_t outer_element_size, const char *inner_type, uint64_t inner_element_size, YazaListNestedMutableBorrowV1 *borrow) {
    if (borrow == NULL || outer_type == NULL || inner_type == NULL || outer_element_size == 0 || inner_element_size == 0) {
        yaza_list_v1_fail("invalid nested typed List mutable view descriptor");
    }
    memset(borrow, 0, sizeof(*borrow));

    YazaListBorrowV1 snapshot = {0};
    yaza_list_v1_iter_begin_typed(opaque, outer_type, outer_element_size, &snapshot);
    if (snapshot.length > YAZA_LIST_V1_NESTED_MUT_VIEW_MAX) {
        yaza_list_v1_iter_end(&snapshot);
        return 0;
    }

    void *lock_order[YAZA_LIST_V1_NESTED_MUT_VIEW_MAX + 1ULL];
    const uint64_t lock_count = snapshot.length + 1;
    lock_order[0] = opaque;
    for (uint64_t index = 0; index < snapshot.length; ++index) {
        void *child = NULL;
        memcpy(&child, (const unsigned char *)snapshot.items + index * snapshot.element_size, sizeof(child));
        if (child == NULL || child == opaque) {
            yaza_list_v1_iter_end(&snapshot);
            yaza_list_v1_fail("nested typed List contains an invalid child List");
        }
        lock_order[index + 1] = child;
    }

    /* Acquire the outer List and distinct children in one global pointer
       order, avoiding reverse lock order across concurrently borrowed lists. */
    for (uint64_t index = 1; index < lock_count; ++index) {
        void *value = lock_order[index];
        uint64_t insert_at = index;
        while (insert_at > 0 && (uintptr_t)lock_order[insert_at - 1] > (uintptr_t)value) {
            lock_order[insert_at] = lock_order[insert_at - 1];
            insert_at -= 1;
        }
        lock_order[insert_at] = value;
    }

    YazaListV1 *outer = NULL;
    for (uint64_t index = 0; index < lock_count; ++index) {
        void *target = lock_order[index];
        if (index > 0 && target == lock_order[index - 1]) continue;
        if (target == opaque) {
            outer = yaza_list_v1_begin_typed(target, outer_type, outer_element_size);
            borrow->outer.list = outer;
            borrow->outer.items = outer->items;
            borrow->outer.length = outer->length;
            if (outer->items != snapshot.items || outer->length != snapshot.length) {
                yaza_list_v1_iter_end_locked(&snapshot, outer);
                while (borrow->child_count > 0) {
                    borrow->child_count -= 1;
                    yaza_list_v1_mut_end(&borrow->children[borrow->child_count]);
                }
                yaza_list_v1_end(outer);
                memset(borrow, 0, sizeof(*borrow));
                return 0;
            }
            yaza_list_v1_iter_end_locked(&snapshot, outer);
        } else {
            if (borrow->child_count >= YAZA_LIST_V1_NESTED_MUT_VIEW_MAX) {
                yaza_list_v1_fail("nested typed List mutable view capacity exceeded");
            }
            YazaListV1 *child = yaza_list_v1_begin_typed(target, inner_type, inner_element_size);
            YazaListMutableBorrowV1 *child_borrow = &borrow->children[borrow->child_count];
            child_borrow->list = child;
            child_borrow->items = child->items;
            child_borrow->length = child->length;
            borrow->child_count += 1;
        }
    }

    if (outer == NULL || snapshot.list != NULL) {
        yaza_list_v1_fail("nested typed List mutable view lock acquisition failed");
    }
    borrow->active = 1;

    for (uint64_t child_index = 0; child_index < borrow->child_count; ++child_index) {
        YazaListV1 *child = (YazaListV1 *)borrow->children[child_index].list;
        if (!yaza_list_v1_prepare_typed_write(child, child->capacity)) {
            yaza_list_v1_nested_mut_end(borrow);
            yaza_list_v1_fail("arena limit exceeded preserving nested typed List read view");
        }
        (void)atomic_fetch_add_explicit(&child->mutation_generation, 1, memory_order_release);
        borrow->children[child_index].items = child->items;
        borrow->children[child_index].length = child->length;
    }

    for (uint64_t outer_index = 0; outer_index < outer->length; ++outer_index) {
        YazaListV1 *child = NULL;
        memcpy(&child, outer->items + outer_index * outer->element_size, sizeof(child));
        uint64_t child_index = 0;
        while (child_index < borrow->child_count && borrow->children[child_index].list != child) {
            child_index += 1;
        }
        if (child_index >= borrow->child_count) {
            yaza_list_v1_nested_mut_end(borrow);
            yaza_list_v1_fail("nested typed List mutable view mapping failed");
        }
        borrow->items_by_outer[outer_index] = borrow->children[child_index].items;
        borrow->lengths_by_outer[outer_index] = borrow->children[child_index].length;
    }
    return 1;
}

void yaza_list_v1_nested_mut_end(YazaListNestedMutableBorrowV1 *borrow) {
    if (borrow == NULL) {
        yaza_list_v1_fail("invalid nested typed List mutable view state");
    }
    if (!borrow->active) {
        memset(borrow, 0, sizeof(*borrow));
        return;
    }
    if (borrow->child_count > YAZA_LIST_V1_NESTED_MUT_VIEW_MAX || borrow->outer.list == NULL) {
        yaza_list_v1_fail("invalid nested typed List mutable view state");
    }
    while (borrow->child_count > 0) {
        borrow->child_count -= 1;
        yaza_list_v1_mut_end(&borrow->children[borrow->child_count]);
    }
    yaza_list_v1_end((YazaListV1 *)borrow->outer.list);
    memset(borrow, 0, sizeof(*borrow));
}

int32_t yaza_list_v1_read_get_typed(YazaListBorrowV1 *borrow, int64_t index, void *out_value) {
    if (borrow == NULL || borrow->list == NULL || borrow->owner == NULL || borrow->element_size == 0 || out_value == NULL) {
        yaza_list_v1_fail("invalid typed List borrowed read");
    }

    YazaListV1 *list = (YazaListV1 *)borrow->list;
    if (atomic_load_explicit(&list->mutation_generation, memory_order_acquire) == borrow->mutation_generation) {
        if (index < 0 || (uint64_t)index >= borrow->length) {
            const char *message = strcmp(list->collection_type, "TextList") == 0
                ? "TextList index is out of range"
                : "typed List index is out of range";
            (void)yaza_rt_v1_fail(message);
            return 0;
        }
        memcpy(out_value, borrow->items + (uint64_t)index * borrow->element_size, (size_t)borrow->element_size);
        return 0;
    }

    /* A mutation may append or replace an item. Fall back under the List lock
       so indexed access observes the current List, including its current length. */
    list = yaza_list_v1_begin_typed(list, list->collection_type, borrow->element_size);
    if (list->owner != borrow->owner) {
        yaza_list_v1_end(list);
        yaza_list_v1_fail("typed List borrowed read owner mismatch");
    }
    if (index < 0 || (uint64_t)index >= list->length) {
        const char *message = strcmp(list->collection_type, "TextList") == 0
            ? "TextList index is out of range"
            : "typed List index is out of range";
        yaza_list_v1_end(list);
        (void)yaza_rt_v1_fail(message);
        return 0;
    }
    memcpy(out_value, list->items + (uint64_t)index * list->element_size, (size_t)list->element_size);
    yaza_list_v1_end(list);
    return 0;
}

void yaza_list_v1_read_fail_bounds(YazaListBorrowV1 *borrow, int64_t index) {
    if (borrow == NULL || borrow->list == NULL || borrow->owner == NULL || borrow->element_size == 0) {
        yaza_list_v1_fail("invalid typed List snapshot read view");
    }
    if (index >= 0 && (uint64_t)index < borrow->length) {
        yaza_list_v1_fail("typed List snapshot bounds failure was not out of range");
    }
    YazaListV1 *list = (YazaListV1 *)borrow->list;
    const char *message = list->collection_type != NULL && strcmp(list->collection_type, "TextList") == 0
        ? "TextList index is out of range"
        : "typed List index is out of range";
    (void)yaza_rt_v1_fail(message);
}

void yaza_list_v1_read_end(YazaListBorrowV1 *borrow) {
    yaza_list_v1_iter_end(borrow);
}

YazaListBorrowV1 *yaza_list_v1_read_view_begin_typed(void *opaque, const char *collection_type, uint64_t element_size) {
    YazaListBorrowV1 temporary = {0};
    yaza_list_v1_read_begin_typed(opaque, collection_type, element_size, &temporary);
    YazaListBorrowV1 *view = (YazaListBorrowV1 *)yaza_arena_v1_alloc(temporary.owner, sizeof(*view));
    if (view == NULL) {
        yaza_list_v1_read_end(&temporary);
        yaza_list_v1_fail("arena limit exceeded allocating a ListView descriptor");
    }
    *view = temporary;
    memset(&temporary, 0, sizeof(temporary));
    return view;
}

YazaListBorrowV1 *yaza_list_v1_read_view_prefix_typed(void *opaque, const char *collection_type, uint64_t element_size, int64_t prefix_length) {
    if (prefix_length < 0) yaza_list_v1_fail("ListView prefix length cannot be negative");
    YazaListBorrowV1 *view = yaza_list_v1_read_view_begin_typed(opaque, collection_type, element_size);
    if ((uint64_t)prefix_length > view->length) {
        yaza_list_v1_read_end(view);
        yaza_list_v1_fail("ListView prefix length exceeds the List length");
    }
    view->length = (uint64_t)prefix_length;
    return view;
}

uint64_t yaza_list_v1_read_view_count(YazaListBorrowV1 *view) {
    if (view == NULL || view->list == NULL || view->owner == NULL || view->element_size == 0) {
        yaza_list_v1_fail("invalid or ended ListView");
    }
    return view->length;
}

void yaza_list_v1_read_view_end(YazaListBorrowV1 *view) {
    if (view == NULL) yaza_list_v1_fail("invalid ListView while ending snapshot");
    yaza_list_v1_read_end(view);
}

void yaza_list_v1_read_release_shared(YazaListBorrowV1 *borrow, YazaListBorrowV1 *local_borrow) {
    if (borrow == NULL || local_borrow == NULL) {
        yaza_list_v1_fail("invalid shared typed List read view release");
    }
    if (borrow == local_borrow) {
        yaza_list_v1_read_end(local_borrow);
    }
}

int32_t yaza_list_v1_nested_read_begin_typed(void *opaque, const char *outer_type, uint64_t outer_element_size, const char *inner_type, uint64_t inner_element_size, YazaListNestedReadBorrowV1 *borrow) {
    if (borrow == NULL || outer_type == NULL || inner_type == NULL || outer_element_size != sizeof(void *) || inner_element_size == 0 || borrow->outer.list != NULL || borrow->outer.owner != NULL || borrow->child_count != 0 || borrow->active != 0) {
        yaza_list_v1_fail("invalid nested typed List read view descriptor");
    }
    yaza_list_v1_read_begin_typed(opaque, outer_type, outer_element_size, &borrow->outer);
    borrow->inner_type = inner_type;
    borrow->inner_element_size = inner_element_size;
    if (borrow->outer.length > YAZA_LIST_V1_NESTED_READ_VIEW_MAX) return 0;

    for (uint64_t outer_index = 0; outer_index < borrow->outer.length; ++outer_index) {
        YazaListV1 *child = NULL;
        memcpy(&child, borrow->outer.items + outer_index * borrow->outer.element_size, sizeof(child));
        if (child == NULL) {
            yaza_list_v1_nested_read_end(borrow);
            yaza_list_v1_fail("nested typed List read view contains an invalid child");
        }
        uint64_t child_index = 0;
        while (child_index < borrow->child_count && borrow->children[child_index].list != child) {
            child_index += 1;
        }
        if (child_index == borrow->child_count) {
            if (borrow->child_count >= YAZA_LIST_V1_NESTED_READ_VIEW_MAX) {
                yaza_list_v1_nested_read_end(borrow);
                yaza_list_v1_fail("nested typed List read view child limit exceeded");
            }
            yaza_list_v1_read_begin_typed(child, inner_type, inner_element_size, &borrow->children[child_index]);
            borrow->child_count += 1;
        }
        borrow->child_index_by_outer[outer_index] = child_index;
    }
    borrow->active = 1;
    return 1;
}

YazaListNestedReadBorrowV1 *yaza_list_v1_nested_read_begin_or_share_typed(void *opaque, const char *outer_type, uint64_t outer_element_size, const char *inner_type, uint64_t inner_element_size, YazaListNestedReadBorrowV1 *parent_borrow, YazaListNestedReadBorrowV1 *local_borrow) {
    if (local_borrow == NULL || local_borrow->outer.list != NULL || local_borrow->outer.owner != NULL || local_borrow->child_count != 0 || local_borrow->active != 0 || outer_type == NULL || inner_type == NULL) {
        yaza_list_v1_fail("invalid shared nested typed List read view state");
    }
    if (parent_borrow != NULL && parent_borrow->outer.list == opaque && parent_borrow->outer.owner != NULL && parent_borrow->outer.element_size == outer_element_size && parent_borrow->inner_element_size == inner_element_size && parent_borrow->inner_type != NULL && strcmp(parent_borrow->inner_type, inner_type) == 0) {
        YazaListV1 *outer = (YazaListV1 *)parent_borrow->outer.list;
        if (outer->owner == parent_borrow->outer.owner && outer->collection_type != NULL && strcmp(outer->collection_type, outer_type) == 0) {
            return parent_borrow;
        }
    }
    (void)yaza_list_v1_nested_read_begin_typed(opaque, outer_type, outer_element_size, inner_type, inner_element_size, local_borrow);
    return local_borrow;
}

void yaza_list_v1_nested_read_end(YazaListNestedReadBorrowV1 *borrow) {
    if (borrow == NULL) {
        yaza_list_v1_fail("invalid nested typed List read view state");
    }
    if (borrow->outer.list == NULL && borrow->outer.owner == NULL && borrow->child_count == 0) {
        memset(borrow, 0, sizeof(*borrow));
        return;
    }
    if (borrow->child_count > YAZA_LIST_V1_NESTED_READ_VIEW_MAX || borrow->outer.list == NULL || borrow->outer.owner == NULL) {
        yaza_list_v1_fail("invalid nested typed List read view state");
    }
    while (borrow->child_count > 0) {
        borrow->child_count -= 1;
        yaza_list_v1_read_end(&borrow->children[borrow->child_count]);
    }
    yaza_list_v1_read_end(&borrow->outer);
    memset(borrow, 0, sizeof(*borrow));
}

void yaza_list_v1_nested_read_release_shared(YazaListNestedReadBorrowV1 *borrow, YazaListNestedReadBorrowV1 *local_borrow) {
    if (borrow == NULL || local_borrow == NULL) {
        yaza_list_v1_fail("invalid shared nested typed List read view release");
    }
    if (borrow == local_borrow) {
        yaza_list_v1_nested_read_end(local_borrow);
    }
}

int64_t yaza_list_v1_push_typed(void *opaque, const void *value, const char *collection_type, uint64_t element_size) {
    YazaListV1 *list = yaza_list_v1_begin_typed(opaque, collection_type, element_size);
    if (value == NULL || list->length >= yaza_rt_v1_config.max_list_items) {
        yaza_list_v1_end(list);
        yaza_list_v1_fail("typed List item or capacity is invalid");
    }
    uint64_t maximum_capacity = yaza_rt_v1_config.max_list_vector_bytes / list->element_size;
    if (maximum_capacity > yaza_rt_v1_config.max_list_items) maximum_capacity = yaza_rt_v1_config.max_list_items;
    uint64_t detached_capacity = list->capacity;
    if (list->current_storage_borrows > 0 && list->length == list->capacity) {
        detached_capacity = list->capacity == 0 ? 8 : list->capacity;
        if (detached_capacity > maximum_capacity / 2) detached_capacity = maximum_capacity;
        else detached_capacity *= 2;
        if (detached_capacity <= list->length || detached_capacity > SIZE_MAX / list->element_size) {
            yaza_list_v1_end(list);
            yaza_list_v1_fail("typed List capacity overflow");
        }
    }
    if (!yaza_list_v1_prepare_typed_write(list, detached_capacity)) {
        yaza_list_v1_end(list);
        yaza_list_v1_fail("arena limit exceeded preserving typed List iterator snapshot");
    }
    if (list->length == list->capacity) {
        uint64_t capacity = list->capacity == 0 ? 8 : list->capacity;
        if (capacity > maximum_capacity / 2) capacity = maximum_capacity;
        else capacity *= 2;
        if (capacity <= list->length || capacity > SIZE_MAX / list->element_size) {
            yaza_list_v1_end(list);
            yaza_list_v1_fail("typed List capacity overflow");
        }
        const uint64_t allocation_size = capacity * list->element_size;
        unsigned char *items = (unsigned char *)yaza_arena_v1_alloc(list->owner, allocation_size);
        if (items == NULL) {
            yaza_list_v1_end(list);
            yaza_list_v1_fail("arena limit exceeded growing typed List");
        }
        if (list->length > 0) {
            memcpy(items, list->items, (size_t)(list->length * list->element_size));
        }
        list->items = items;
        list->capacity = capacity;
    }
    unsigned char *destination = list->items + list->length * list->element_size;
    yaza_list_v1_store_typed_item(list, value, destination);
    list->length += 1;
    yaza_list_v1_note_typed_mutation(list);
    yaza_list_v1_end(list);
    return 0;
}

int32_t yaza_list_v1_try_push_typed(void *opaque, const void *value, const char *collection_type, uint64_t element_size) {
    YazaListV1 *list = yaza_list_v1_begin_typed(opaque, collection_type, element_size);
    if (value == NULL || list->length >= yaza_rt_v1_config.max_list_items) {
        yaza_list_v1_end(list);
        return 0;
    }
    uint64_t maximum_capacity = yaza_rt_v1_config.max_list_vector_bytes / list->element_size;
    if (maximum_capacity > yaza_rt_v1_config.max_list_items) maximum_capacity = yaza_rt_v1_config.max_list_items;
    uint64_t detached_capacity = list->capacity;
    if (list->current_storage_borrows > 0 && list->length == list->capacity) {
        detached_capacity = list->capacity == 0 ? 8 : list->capacity;
        if (detached_capacity > maximum_capacity / 2) detached_capacity = maximum_capacity;
        else detached_capacity *= 2;
        if (detached_capacity <= list->length || detached_capacity > SIZE_MAX / list->element_size) {
            yaza_list_v1_end(list);
            return 0;
        }
    }
    if (!yaza_list_v1_prepare_typed_write(list, detached_capacity)) {
        yaza_list_v1_end(list);
        return 0;
    }
    if (list->length == list->capacity) {
        uint64_t capacity = list->capacity == 0 ? 8 : list->capacity;
        if (capacity > maximum_capacity / 2) capacity = maximum_capacity;
        else capacity *= 2;
        if (capacity <= list->length || capacity > SIZE_MAX / list->element_size) {
            yaza_list_v1_end(list);
            return 0;
        }
        const uint64_t allocation_size = capacity * list->element_size;
        unsigned char *items = (unsigned char *)yaza_arena_v1_alloc(list->owner, allocation_size);
        if (items == NULL) {
            yaza_list_v1_end(list);
            return 0;
        }
        if (list->length > 0) {
            memcpy(items, list->items, (size_t)(list->length * list->element_size));
        }
        list->items = items;
        list->capacity = capacity;
    }
    unsigned char *destination = list->items + list->length * list->element_size;
    if (!yaza_list_v1_try_store_typed_item(list, value, destination)) {
        yaza_list_v1_end(list);
        return 0;
    }
    list->length += 1;
    yaza_list_v1_note_typed_mutation(list);
    yaza_list_v1_end(list);
    return 1;
}

int32_t yaza_list_v1_try_reserve_typed(void *opaque, int64_t requested_capacity, const char *collection_type, uint64_t element_size) {
    YazaListV1 *list = yaza_list_v1_begin_typed(opaque, collection_type, element_size);
    if (requested_capacity < 0) {
        yaza_list_v1_end(list);
        return 0;
    }
    const uint64_t capacity = (uint64_t)requested_capacity;
    uint64_t maximum_capacity = yaza_rt_v1_config.max_list_vector_bytes / list->element_size;
    if (maximum_capacity > yaza_rt_v1_config.max_list_items) maximum_capacity = yaza_rt_v1_config.max_list_items;
    if (capacity > maximum_capacity || capacity > SIZE_MAX / list->element_size) {
        yaza_list_v1_end(list);
        return 0;
    }
    if (capacity <= list->capacity) {
        yaza_list_v1_end(list);
        return 1;
    }
    if (!yaza_list_v1_prepare_typed_write(list, capacity)) {
        yaza_list_v1_end(list);
        return 0;
    }
    if (capacity > list->capacity) {
        const uint64_t allocation_size = capacity * list->element_size;
        unsigned char *items = (unsigned char *)yaza_arena_v1_alloc(list->owner, allocation_size);
        if (items == NULL) {
            yaza_list_v1_end(list);
            return 0;
        }
        if (list->length > 0) {
            memcpy(items, list->items, (size_t)(list->length * list->element_size));
        }
        list->items = items;
        list->capacity = capacity;
    }
    yaza_list_v1_end(list);
    return 1;
}

int32_t yaza_list_v1_get_typed(void *opaque, int64_t index, const char *collection_type, uint64_t element_size, void *out_value) {
    YazaListV1 *list = yaza_list_v1_begin_typed(opaque, collection_type, element_size);
    if (out_value == NULL) {
        yaza_list_v1_end(list);
        yaza_list_v1_fail("typed List index or output is invalid");
    }
    if (index < 0 || (uint64_t)index >= list->length) {
        const char *message = strcmp(list->collection_type, "TextList") == 0
            ? "TextList index is out of range"
            : "typed List index is out of range";
        yaza_list_v1_end(list);
        (void)yaza_rt_v1_fail(message);
        return 0;
    }
    memcpy(out_value, list->items + (uint64_t)index * list->element_size, (size_t)list->element_size);
    yaza_list_v1_end(list);
    return 0;
}

int32_t yaza_list_v1_try_get_typed(void *opaque, int64_t index, const char *collection_type, uint64_t element_size, void *out_value) {
    if (out_value == NULL) {
        yaza_list_v1_fail("typed list_try_get output pointer cannot be null");
    }
    YazaListV1 *list = yaza_list_v1_begin_typed(opaque, collection_type, element_size);
    if (index < 0 || (uint64_t)index >= list->length) {
        memset(out_value, 0, (size_t)list->element_size);
        yaza_list_v1_end(list);
        return 0;
    }
    memcpy(out_value, list->items + (uint64_t)index * list->element_size, (size_t)list->element_size);
    yaza_list_v1_end(list);
    return 1;
}

int64_t yaza_list_v1_set_typed(void *opaque, int64_t index, const void *value, const char *collection_type, uint64_t element_size) {
    YazaListV1 *list = yaza_list_v1_begin_typed(opaque, collection_type, element_size);
    if (value == NULL) {
        yaza_list_v1_end(list);
        yaza_list_v1_fail("typed List item is invalid");
    }
    if (index < 0 || (uint64_t)index >= list->length) {
        const char *message = strcmp(list->collection_type, "TextList") == 0
            ? "TextList index is out of range"
            : "typed List index is out of range";
        yaza_list_v1_end(list);
        (void)yaza_rt_v1_fail(message);
        return 0;
    }
    if (!yaza_list_v1_prepare_typed_write(list, list->length)) {
        yaza_list_v1_end(list);
        yaza_list_v1_fail("arena limit exceeded preserving typed List iterator snapshot");
    }
    yaza_list_v1_store_typed_item(list, value, list->items + (uint64_t)index * list->element_size);
    yaza_list_v1_note_typed_mutation(list);
    yaza_list_v1_end(list);
    return 0;
}

int32_t yaza_list_v1_try_set_typed(void *opaque, int64_t index, const void *value, const char *collection_type, uint64_t element_size) {
    YazaListV1 *list = yaza_list_v1_begin_typed(opaque, collection_type, element_size);
    if (value == NULL) {
        yaza_list_v1_end(list);
        yaza_list_v1_fail("typed List item is invalid");
    }
    if (index < 0 || (uint64_t)index >= list->length) {
        yaza_list_v1_end(list);
        return 0;
    }
    if (!yaza_list_v1_prepare_typed_write(list, list->length)) {
        yaza_list_v1_end(list);
        return 0;
    }
    unsigned char *destination = list->items + (uint64_t)index * list->element_size;
    if (!yaza_list_v1_try_store_typed_item(list, value, destination)) {
        yaza_list_v1_end(list);
        return 0;
    }
    yaza_list_v1_note_typed_mutation(list);
    yaza_list_v1_end(list);
    return 1;
}

void *yaza_list_v1_create(uint64_t element_type_id) {
    if (element_type_id == 0) {
        yaza_list_v1_fail("invalid List element type id");
    }
    YazaArenaV1 *owner = yaza_arena_v1_current();
    YazaListV1 *list = (YazaListV1 *)yaza_arena_v1_alloc(owner, sizeof(YazaListV1));
    if (list == NULL) {
        yaza_list_v1_fail("arena limit exceeded creating List");
    }
    memset(list, 0, sizeof(*list));
    atomic_init(&list->lock, false);
    atomic_init(&list->mutation_generation, 0);
    list->owner = owner;
    list->owner_generation = yaza_arena_v1_generation(owner);
    list->element_type_id = element_type_id;
    list->element_size = sizeof(uint64_t);
    return list;
}

uint64_t yaza_list_v1_count(void *opaque, uint64_t element_type_id) {
    YazaListV1 *list = yaza_list_v1_begin(opaque, element_type_id);
    const uint64_t result = list->length;
    yaza_list_v1_end(list);
    return result;
}

int64_t yaza_list_v1_push(void *opaque, uint64_t value, uint64_t element_type_id) {
    YazaListV1 *list = yaza_list_v1_begin(opaque, element_type_id);
    const uint64_t maximum_items = yaza_list_v1_legacy_max_items();
    if (list->length >= maximum_items) {
        yaza_list_v1_end(list);
        yaza_list_v1_fail("List item limit exceeded");
    }
    if (list->length == list->capacity) {
        uint64_t capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        if (capacity > maximum_items) {
            capacity = maximum_items;
        }
        if (capacity <= list->length || capacity > UINT64_MAX / sizeof(uint64_t)) {
            yaza_list_v1_end(list);
            yaza_list_v1_fail("List capacity overflow");
        }
        unsigned char *items = (unsigned char *)yaza_arena_v1_alloc(list->owner, capacity * sizeof(uint64_t));
        if (items == NULL) {
            yaza_list_v1_end(list);
            yaza_list_v1_fail("arena limit exceeded growing List");
        }
        if (list->length > 0) {
            memcpy(items, list->items, (size_t)(list->length * sizeof(uint64_t)));
        }
        list->items = items;
        list->capacity = capacity;
    }
    if (element_type_id == 5 || element_type_id == YAZA_LIST_V1_TEXT_LIST_TYPE) {
        const YazaTextV1 *text = (const YazaTextV1 *)(uintptr_t)value;
        if (text == NULL) {
            yaza_list_v1_end(list);
            yaza_list_v1_fail("TextList cannot append a null Text value");
        }
        yaza_text_v1_require(text, "TextList append");
        YazaTextV1 *copy = yaza_text_v1_copy_to_arena(list->owner, text->data, text->length);
        if (copy == NULL) {
            yaza_list_v1_end(list);
            yaza_list_v1_fail("arena limit exceeded copying TextList value");
        }
        value = (uint64_t)(uintptr_t)copy;
    }
    memcpy(list->items + list->length * sizeof(uint64_t), &value, sizeof(value));
    list->length += 1;
    yaza_list_v1_end(list);
    return 0;
}

uint64_t yaza_list_v1_get(void *opaque, int64_t index, uint64_t element_type_id) {
    YazaListV1 *list = yaza_list_v1_begin(opaque, element_type_id);
    if (index < 0 || (uint64_t)index >= list->length) {
        yaza_list_v1_end(list);
        yaza_list_v1_fail("List index out of range");
    }
    uint64_t result = 0;
    memcpy(&result, list->items + (uint64_t)index * sizeof(uint64_t), sizeof(result));
    yaza_list_v1_end(list);
    return result;
}

int32_t yaza_list_v1_try_get(void *opaque, int64_t index, uint64_t element_type_id, uint64_t *out_value) {
    if (out_value == NULL) {
        yaza_list_v1_fail("list_try_get output pointer cannot be null");
    }
    YazaListV1 *list = yaza_list_v1_begin(opaque, element_type_id);
    if (index < 0 || (uint64_t)index >= list->length) {
        *out_value = 0;
        yaza_list_v1_end(list);
        return 0;
    }
    memcpy(out_value, list->items + (uint64_t)index * sizeof(uint64_t), sizeof(*out_value));
    yaza_list_v1_end(list);
    return 1;
}

int64_t yaza_list_v1_set(void *opaque, int64_t index, uint64_t value, uint64_t element_type_id) {
    YazaListV1 *list = yaza_list_v1_begin(opaque, element_type_id);
    if (index < 0 || (uint64_t)index >= list->length) {
        yaza_list_v1_end(list);
        yaza_list_v1_fail("List index out of range");
    }
    if (element_type_id == 5 || element_type_id == YAZA_LIST_V1_TEXT_LIST_TYPE) {
        const YazaTextV1 *text = (const YazaTextV1 *)(uintptr_t)value;
        if (text == NULL) {
            yaza_list_v1_end(list);
            yaza_list_v1_fail("TextList cannot store a null Text value");
        }
        yaza_text_v1_require(text, "TextList set");
        YazaTextV1 *copy = yaza_text_v1_copy_to_arena(list->owner, text->data, text->length);
        if (copy == NULL) {
            yaza_list_v1_end(list);
            yaza_list_v1_fail("arena limit exceeded copying TextList value");
        }
        value = (uint64_t)(uintptr_t)copy;
    }
    memcpy(list->items + (uint64_t)index * sizeof(uint64_t), &value, sizeof(value));
    yaza_list_v1_end(list);
    return 0;
}

static int32_t yaza_list_v1_try_push_text_slice(void *opaque, const unsigned char *text, uint64_t text_length) {
    YazaListV1 *list = yaza_list_v1_begin_typed(opaque, "TextList", sizeof(char *));
    const uint64_t maximum_items = yaza_rt_v1_config.max_list_items;
    uint64_t maximum_capacity = yaza_rt_v1_config.max_list_vector_bytes / sizeof(char *);
    if (maximum_capacity > maximum_items) maximum_capacity = maximum_items;
    if ((text == NULL && text_length > 0) || list->length >= maximum_capacity || text_length > SIZE_MAX - 1) {
        yaza_list_v1_end(list);
        return 0;
    }
    if (list->length == list->capacity) {
        uint64_t capacity = list->capacity == 0 ? 8 : list->capacity;
        if (capacity > maximum_capacity / 2) capacity = maximum_capacity;
        else capacity *= 2;
        if (capacity <= list->length || capacity > SIZE_MAX / sizeof(char *)) {
            yaza_list_v1_end(list);
            return 0;
        }
        const uint64_t allocation_size = capacity * sizeof(char *);
        unsigned char *items = (unsigned char *)yaza_arena_v1_alloc(list->owner, allocation_size);
        if (items == NULL) {
            yaza_list_v1_end(list);
            return 0;
        }
        if (list->length > 0) memcpy(items, list->items, (size_t)(list->length * sizeof(char *)));
        list->items = items;
        list->capacity = capacity;
    }
    YazaTextV1 *owned_text = yaza_text_v1_copy_to_arena(list->owner, text, text_length);
    if (owned_text == NULL) {
        yaza_list_v1_end(list);
        return 0;
    }
    memcpy(list->items + list->length * sizeof(char *), &owned_text, sizeof(owned_text));
    list->length += 1;
    yaza_list_v1_end(list);
    return 1;
}

const YazaTextV1 *yaza_rt_v1_text_split_delimiter_error(void) {
    static const unsigned char message[] = "text_split delimiter cannot be empty";
    static const YazaTextV1 error = {message, sizeof(message) - 1};
    return &error;
}

int32_t yaza_rt_v1_try_text_split(const YazaTextV1 *text, const YazaTextV1 *delimiter, void **out_parts) {
    if (out_parts == NULL) {
        fputs("YAZA runtime v1: null output passed to text_try_split\n", stderr);
        abort();
    }
    *out_parts = NULL;
    yaza_text_v1_require(text, "text_try_split");
    yaza_text_v1_require(delimiter, "text_try_split");
    const uint64_t delimiter_length = delimiter->length;
    if (delimiter_length == 0) return -1;
    void *parts = yaza_list_v1_try_create_typed("TextList", sizeof(char *));
    if (parts == NULL) return 0;
    uint64_t segment_start = 0;
    uint64_t index = 0;
    while (index <= text->length && delimiter_length <= text->length - index) {
        if (memcmp(text->data + index, delimiter->data, (size_t)delimiter_length) == 0) {
            if (!yaza_list_v1_try_push_text_slice(parts, text->data + segment_start, index - segment_start)) {
                return 0;
            }
            index += delimiter_length;
            segment_start = index;
        } else {
            index += 1;
        }
    }
    if (!yaza_list_v1_try_push_text_slice(parts,
            text->data == NULL ? (const unsigned char *)"" : text->data + segment_start,
            text->length - segment_start)) {
        return 0;
    }
    *out_parts = parts;
    return 1;
}

void *yaza_text_v1_split(const YazaTextV1 *text, const YazaTextV1 *delimiter) {
    void *parts = NULL;
    const int32_t status = yaza_rt_v1_try_text_split(text, delimiter, &parts);
    if (status == 1) return parts;
    if (status == -1) yaza_list_v1_fail("text_split delimiter cannot be empty");
    yaza_list_v1_fail("text_split exceeded the TextList or arena limit");
    return NULL;
}

int32_t yaza_rt_v1_try_text_lines(const YazaTextV1 *text, void **out_lines) {
    if (out_lines == NULL) {
        fputs("YAZA runtime v1: null output passed to text_try_lines\n", stderr);
        abort();
    }
    *out_lines = NULL;
    yaza_text_v1_require(text, "text_try_lines");
    void *lines = yaza_list_v1_try_create_typed("TextList", sizeof(char *));
    if (lines == NULL) return 0;
    if (text->length == 0) {
        *out_lines = lines;
        return 1;
    }
    uint64_t segment_start = 0;
    for (uint64_t index = 0; index < text->length; ++index) {
        if (text->data[index] == '\n') {
            if (!yaza_list_v1_try_push_text_slice(lines, text->data + segment_start, index - segment_start)) {
                return 0;
            }
            segment_start = index + 1;
        }
    }
    if (segment_start < text->length &&
        !yaza_list_v1_try_push_text_slice(lines, text->data + segment_start, text->length - segment_start)) {
        return 0;
    }
    *out_lines = lines;
    return 1;
}

void *yaza_rt_v1_text_lines(const YazaTextV1 *text) {
    void *lines = NULL;
    if (!yaza_rt_v1_try_text_lines(text, &lines)) {
        yaza_list_v1_fail("text_lines exceeded the TextList or arena limit");
    }
    return lines;
}

static YazaTextV1 *yaza_rt_v1_text_join_impl(
    void *opaque,
    const YazaTextV1 *separator,
    int32_t *out_failure
) {
    yaza_text_v1_require(separator, "text_join");
    if (out_failure != NULL) *out_failure = 0;
    YazaListV1 *list = yaza_list_v1_begin_typed(opaque, "TextList", sizeof(char *));
    const uint64_t maximum_length = (uint64_t)SIZE_MAX - sizeof(YazaTextV1) - 1;
    uint64_t total = 0;
    for (uint64_t index = 0; index < list->length; ++index) {
        const YazaTextV1 *item = NULL;
        memcpy(&item, list->items + index * sizeof(item), sizeof(item));
        if (item == NULL) {
            yaza_list_v1_end(list);
            yaza_list_v1_fail("text_join encountered a null Text value");
        }
        yaza_text_v1_require(item, "text_join element");
        if (total > maximum_length || item->length > maximum_length - total) {
            yaza_list_v1_end(list);
            if (out_failure != NULL) *out_failure = 2;
            return NULL;
        }
        total += item->length;
        if (index > 0) {
            if (separator->length > maximum_length - total) {
                yaza_list_v1_end(list);
                if (out_failure != NULL) *out_failure = 2;
                return NULL;
            }
            total += separator->length;
        }
    }

    const uint64_t allocation_size = (uint64_t)sizeof(YazaTextV1) + total + 1;
    YazaTextV1 *result = (YazaTextV1 *)yaza_rt_v1_try_alloc(allocation_size);
    if (result == NULL) {
        yaza_list_v1_end(list);
        if (out_failure != NULL) *out_failure = 1;
        return NULL;
    }
    unsigned char *joined = (unsigned char *)(result + 1);
    uint64_t cursor = 0;
    for (uint64_t index = 0; index < list->length; ++index) {
        const YazaTextV1 *item = NULL;
        memcpy(&item, list->items + index * sizeof(item), sizeof(item));
        if (index > 0) {
            if (separator->length > 0) memcpy(joined + cursor, separator->data, (size_t)separator->length);
            cursor += separator->length;
        }
        if (item->length > 0) memcpy(joined + cursor, item->data, (size_t)item->length);
        cursor += item->length;
    }
    joined[cursor] = '\0';
    result->data = joined;
    result->length = total;
    yaza_list_v1_end(list);
    return result;
}

YazaTextV1 *yaza_rt_v1_try_text_join(void *opaque, const YazaTextV1 *separator) {
    return yaza_rt_v1_text_join_impl(opaque, separator, NULL);
}

YazaTextV1 *yaza_rt_v1_text_join(void *opaque, const YazaTextV1 *separator) {
    int32_t failure = 0;
    YazaTextV1 *result = yaza_rt_v1_text_join_impl(opaque, separator, &failure);
    if (result != NULL) return result;
    if (failure == 2) yaza_list_v1_fail("joined Text is too large");
    yaza_list_v1_fail("arena limit or allocation failure joining Text");
    return NULL;
}

int32_t yaza_bytes_v1_join(void *typed_bytes_list, YazaBytesV1 *out_bytes, char **out_error) {
    if (typed_bytes_list == NULL || out_bytes == NULL || out_error == NULL) {
        yaza_bytes_v1_fail("bytes_join received a null pointer");
    }
    const uint64_t count = yaza_list_v1_count_typed(typed_bytes_list, "List[Bytes]", sizeof(YazaBytesV1));
    uint64_t total = 0;
    for (uint64_t index = 0; index < count; ++index) {
        YazaBytesV1 item = {NULL, 0};
        (void)yaza_list_v1_get_typed(typed_bytes_list, (int64_t)index, "List[Bytes]", sizeof(item), &item);
        if (item.length > YAZA_RT_V1_BYTES_MAX_BYTES - total || (item.length > 0 && item.data == NULL)) {
            YazaBytesV1 empty = {NULL, 0};
            char *error = (char *)"joined Bytes exceeds the 64 MiB limit or contains invalid storage";
            *out_bytes = empty;
            *out_error = error;
            return 0;
        }
        total += item.length;
    }

    YazaBytesV1 result = {NULL, total};
    if (total > 0) {
        result.data = (unsigned char *)yaza_arena_v1_alloc(yaza_arena_v1_current(), total);
        if (result.data == NULL) {
            YazaBytesV1 empty = {NULL, 0};
            char *error = (char *)"Could not allocate joined Bytes in the active arena";
            *out_bytes = empty;
            *out_error = error;
            return 0;
        }
        uint64_t offset = 0;
        for (uint64_t index = 0; index < count; ++index) {
            YazaBytesV1 item = {NULL, 0};
            (void)yaza_list_v1_get_typed(typed_bytes_list, (int64_t)index, "List[Bytes]", sizeof(item), &item);
            if (item.length > 0) memcpy(result.data + offset, item.data, (size_t)item.length);
            offset += item.length;
        }
    }
    char *no_error = NULL;
    *out_bytes = result;
    *out_error = no_error;
    return 1;
}

static int yaza_rt_v1_path_text(const YazaTextV1 *path, const char *operation) {
    if (path == NULL || path->data == NULL || path->length == 0 || path->length > 4096 ||
        memchr(path->data, '\0', (size_t)path->length) != NULL) {
        fprintf(stderr, "YAZA runtime v1: invalid path in %s\n", operation);
        return 0;
    }
    return 1;
}

static void yaza_rt_v1_file_read_open_fail(const YazaTextV1 *path, int error) {
    char escaped_path[4 * 4096 + 1];
    size_t escaped_length = 0;
    for (uint64_t index = 0; index < path->length; ++index) {
        const unsigned char byte = path->data[index];
        if (byte >= 0x20 && byte <= 0x7e && byte != '\\' && byte != '\'') {
            escaped_path[escaped_length++] = (char)byte;
        } else {
            const int written = snprintf(escaped_path + escaped_length,
                                         sizeof(escaped_path) - escaped_length,
                                         "\\x%02X", (unsigned int)byte);
            if (written != 4 || (size_t)written >= sizeof(escaped_path) - escaped_length) {
                yaza_rt_v1_fail("file_read could not open the requested file");
                return;
            }
            escaped_length += (size_t)written;
        }
    }
    escaped_path[escaped_length] = '\0';

    char message[4 * 4096 + 128];
    const char *reason = strerror(error);
    const int written = snprintf(message, sizeof(message), "file_read could not open '%s': %s",
                                 escaped_path, reason == NULL ? "unknown operating-system error" : reason);
    if (written < 0 || (size_t)written >= sizeof(message)) {
        yaza_rt_v1_fail("file_read could not open the requested file");
        return;
    }
    yaza_rt_v1_fail(message);
}

YazaTextV1 *yaza_rt_v1_env_get(const YazaTextV1 *name) {
    if (name == NULL || name->data == NULL || name->length == 0 || name->length > 4096 ||
        memchr(name->data, '\0', (size_t)name->length) != NULL ||
        memchr(name->data, '=', (size_t)name->length) != NULL) {
        yaza_rt_v1_fail("env_get requires a non-empty environment variable name without NUL or '='");
        return NULL;
    }
    char key[4097];
    memcpy(key, name->data, (size_t)name->length);
    key[name->length] = '\0';
    const char *value = getenv(key);
    if (value == NULL) value = "";
    const size_t length = strnlen(value, (size_t)YAZA_RT_V1_BYTES_MAX_BYTES + 1U);
    if (length > (size_t)YAZA_RT_V1_BYTES_MAX_BYTES ||
        !yaza_bytes_v1_is_valid_text((const unsigned char *)value, (uint64_t)length)) {
        yaza_rt_v1_fail("env_get value is invalid UTF-8 or exceeds the 64 MiB Text limit");
        return NULL;
    }
    YazaTextV1 *result = yaza_text_v1_copy_to_arena(yaza_arena_v1_current(), (const unsigned char *)value,
                                                     (uint64_t)length);
    if (result == NULL) yaza_rt_v1_fail("env_get exceeded the active arena allocation limit");
    return result;
}

int32_t yaza_rt_v1_file_exists(const YazaTextV1 *path) {
    if (!yaza_rt_v1_path_text(path, "file_exists")) return 0;
    yaza_stat_t status;
    return yaza_stat64((const char *)path->data, &status) == 0;
}

int32_t yaza_rt_v1_is_directory(const YazaTextV1 *path) {
    if (!yaza_rt_v1_path_text(path, "is_directory")) return 0;
    yaza_stat_t status;
#if defined(_WIN32)
    return yaza_stat64((const char *)path->data, &status) == 0 && (status.st_mode & _S_IFDIR) != 0;
#else
    return yaza_stat64((const char *)path->data, &status) == 0 && S_ISDIR(status.st_mode);
#endif
}

YazaTextV1 *yaza_rt_v1_file_read(const YazaTextV1 *path) {
    if (!yaza_rt_v1_path_text(path, "file_read")) {
        yaza_rt_v1_fail("file_read path is invalid or exceeds 4096 bytes");
        return NULL;
    }
#if defined(_WIN32)
    const int descriptor = _open((const char *)path->data, _O_RDONLY | _O_BINARY);
#else
    const int descriptor = open((const char *)path->data, O_RDONLY | O_CLOEXEC);
#endif
    if (descriptor < 0) {
        const int error = errno == 0 ? EIO : errno;
        yaza_rt_v1_file_read_open_fail(path, error);
        return NULL;
    }
    yaza_stat_t status;
#if defined(_WIN32)
    const int valid_regular = yaza_fstat64(descriptor, &status) == 0 && (status.st_mode & _S_IFREG) != 0;
#else
    const int valid_regular = yaza_fstat64(descriptor, &status) == 0 && S_ISREG(status.st_mode);
#endif
    if (!valid_regular || status.st_size < 0 || (uint64_t)status.st_size > YAZA_RT_V1_BYTES_MAX_BYTES) {
#if defined(_WIN32)
        (void)_close(descriptor);
#else
        (void)close(descriptor);
#endif
        yaza_rt_v1_fail("file_read requires a regular file no larger than 64 MiB");
        return NULL;
    }
    const uint64_t capacity = (uint64_t)status.st_size;
    unsigned char *buffer = capacity == 0 ? NULL : (unsigned char *)malloc((size_t)capacity);
    if (capacity > 0 && buffer == NULL) {
#if defined(_WIN32)
        (void)_close(descriptor);
#else
        (void)close(descriptor);
#endif
        yaza_rt_v1_fail("file_read could not allocate its bounded input buffer");
        return NULL;
    }
    uint64_t length = 0;
    while (length < capacity) {
#if defined(_WIN32)
        size_t want = capacity - length;
        unsigned int chunk = want > (size_t)INT_MAX ? (unsigned int)INT_MAX : (unsigned int)want;
        int amount = _read(descriptor, buffer + length, chunk);
#else
        ssize_t amount = read(descriptor, buffer + length, (size_t)(capacity - length));
#endif
        if (amount < 0 && errno == EINTR) continue;
        if (amount < 0) {
            free(buffer);
#if defined(_WIN32)
            (void)_close(descriptor);
#else
            (void)close(descriptor);
#endif
            yaza_rt_v1_fail("file_read failed while reading the requested file");
            return NULL;
        }
        if (amount == 0) break;
        length += (uint64_t)amount;
    }
#if defined(_WIN32)
    (void)_close(descriptor);
#else
    (void)close(descriptor);
#endif
    if (!yaza_bytes_v1_is_valid_text(buffer, length)) {
        free(buffer);
        yaza_rt_v1_fail("file_read input is not valid NUL-free UTF-8 Text");
        return NULL;
    }
    YazaTextV1 *result = yaza_text_v1_copy_to_arena(yaza_arena_v1_current(), buffer, length);
    free(buffer);
    if (result == NULL) {
        yaza_rt_v1_fail("file_read exceeded the active arena allocation limit");
        return NULL;
    }
    return result;
}

int64_t yaza_rt_v1_file_write(const YazaTextV1 *path, const YazaTextV1 *contents) {
    if (!yaza_rt_v1_path_text(path, "file_write") || contents == NULL ||
        (contents->length > 0 && contents->data == NULL) || contents->length > YAZA_RT_V1_BYTES_MAX_BYTES ||
        (contents->length > 0 && memchr(contents->data, '\0', (size_t)contents->length) != NULL)) {
        return EINVAL;
    }
#if defined(_WIN32)
    const int descriptor = _open((const char *)path->data, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, _S_IREAD | _S_IWRITE);
#else
    const int descriptor = open((const char *)path->data, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
#endif
    if (descriptor < 0) return errno == 0 ? EIO : errno;
    uint64_t written = 0;
    while (written < contents->length) {
#if defined(_WIN32)
        size_t left = (size_t)(contents->length - written);
        unsigned int chunk = left > (size_t)INT_MAX ? (unsigned int)INT_MAX : (unsigned int)left;
        int amount = _write(descriptor, contents->data + written, chunk);
#else
        ssize_t amount = write(descriptor, contents->data + written, (size_t)(contents->length - written));
#endif
        if (amount < 0 && errno == EINTR) continue;
        if (amount <= 0) {
            const int error = errno == 0 ? EIO : errno;
#if defined(_WIN32)
            (void)_close(descriptor);
#else
            (void)close(descriptor);
#endif
            return error;
        }
        written += (uint64_t)amount;
    }
#if defined(_WIN32)
    if (_close(descriptor) != 0) return errno == 0 ? EIO : errno;
#else
    if (close(descriptor) != 0) return errno == 0 ? EIO : errno;
#endif
    return 0;
}

int64_t yaza_rt_v1_file_remove(const YazaTextV1 *path) {
    if (!yaza_rt_v1_path_text(path, "file_remove")) return EINVAL;
#if defined(_WIN32)
    if (_unlink((const char *)path->data) == 0) return 0;
#else
    if (unlink((const char *)path->data) == 0) return 0;
#endif
    return errno == 0 ? EIO : errno;
}

int64_t yaza_rt_v1_env_set(const YazaTextV1 *name, const YazaTextV1 *value) {
    if (!yaza_rt_v1_path_text(name, "env_set") || value == NULL ||
        (value->length > 0 && (value->data == NULL || memchr(value->data, '\0', (size_t)value->length) != NULL))) {
        return 1;
    }
    const char *cval = (value->length > 0 && value->data != NULL) ? (const char *)value->data : "";
#if defined(_WIN32)
    return _putenv_s((const char *)name->data, cval) == 0 ? 0 : 1;
#else
    return setenv((const char *)name->data, cval, 1) == 0 ? 0 : 1;
#endif
}

int64_t yaza_rt_v1_directory_create(const YazaTextV1 *path) {
    if (!yaza_rt_v1_path_text(path, "directory_create")) return 1;
#if defined(_WIN32)
    if (_mkdir((const char *)path->data) == 0) return 0;
    if (errno == EEXIST) {
        yaza_stat_t details;
        if (yaza_stat64((const char *)path->data, &details) == 0 && (details.st_mode & _S_IFDIR) != 0) return 0;
    }
#else
    if (mkdir((const char *)path->data, 0755) == 0) return 0;
    if (errno == EEXIST) {
        yaza_stat_t details;
        if (yaza_stat64((const char *)path->data, &details) == 0 && S_ISDIR(details.st_mode)) return 0;
    }
#endif
    return 1;
}

void *yaza_rt_v1_directory_list(const YazaTextV1 *path) {
    if (!yaza_rt_v1_path_text(path, "directory_list")) return NULL;
#if defined(_WIN32)
    size_t plen = strlen((const char *)path->data);
    if (plen + 3 > 4096) return NULL;
    char pattern[4096];
    memcpy(pattern, path->data, plen);
    if (plen > 0 && pattern[plen - 1] != '/' && pattern[plen - 1] != '\\') {
        pattern[plen++] = '\\';
    }
    pattern[plen++] = '*';
    pattern[plen] = '\0';
    WIN32_FIND_DATAA find_data;
    HANDLE find_handle = FindFirstFileA(pattern, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE) {
        return NULL;
    }
    void *entries = yaza_list_v1_try_create_typed("TextList", sizeof(char *));
    if (entries == NULL) {
        FindClose(find_handle);
        return NULL;
    }
    do {
        if (strcmp(find_data.cFileName, ".") != 0 && strcmp(find_data.cFileName, "..") != 0) {
            if (!yaza_list_v1_try_push_text_slice(entries, (const unsigned char *)find_data.cFileName, strlen(find_data.cFileName))) {
                FindClose(find_handle);
                return NULL;
            }
        }
    } while (FindNextFileA(find_handle, &find_data));
    FindClose(find_handle);
    return entries;
#else
    DIR *directory = opendir((const char *)path->data);
    if (directory == NULL) {
        return NULL;
    }
    void *entries = yaza_list_v1_try_create_typed("TextList", sizeof(char *));
    if (entries == NULL) {
        closedir(directory);
        return NULL;
    }
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            if (!yaza_list_v1_try_push_text_slice(entries, (const unsigned char *)entry->d_name, strlen(entry->d_name))) {
                closedir(directory);
                return NULL;
            }
        }
    }
    closedir(directory);
    return entries;
#endif
}

YazaTextV1 *yaza_rt_v1_temp_directory(void) {
#if defined(_WIN32)
    char temp_base[4096];
    DWORD base_len = GetTempPathA((DWORD)sizeof(temp_base), temp_base);
    if (base_len == 0 || base_len >= sizeof(temp_base) - 64) {
        return NULL;
    }
    uint64_t seed = (uint64_t)GetCurrentProcessId() * 2654435761ULL ^ (uint64_t)GetTickCount64();
    unsigned char rand_bytes[4] = {0};
    if (BCryptGenRandom(NULL, rand_bytes, sizeof(rand_bytes), BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0) {
        uint32_t r = ((uint32_t)rand_bytes[0] << 24) | ((uint32_t)rand_bytes[1] << 16) |
                     ((uint32_t)rand_bytes[2] << 8) | (uint32_t)rand_bytes[3];
        seed ^= (uint64_t)r;
    }
    for (int attempt = 0; attempt < 64; attempt++) {
        char candidate[4096];
        int written = snprintf(candidate, sizeof(candidate), "%syaza-run-%08llx",
                               temp_base, (unsigned long long)((seed + (uint64_t)attempt) & 0xffffffffULL));
        if (written > 0 && (size_t)written < sizeof(candidate)) {
            if (CreateDirectoryA(candidate, NULL)) {
                return yaza_rt_v1_text_from_cstr(candidate);
            }
            if (GetLastError() != ERROR_ALREADY_EXISTS) {
                return NULL;
            }
        }
    }
    return NULL;
#else
    char path[] = "/tmp/yaza-run.XXXXXX";
    char *created = mkdtemp(path);
    if (created == NULL) {
        return NULL;
    }
    return yaza_rt_v1_text_from_cstr(created);
#endif
}

#define YAZA_REMOVE_TREE_MAX_DEPTH 64U
#define YAZA_REMOVE_TREE_MAX_ENTRIES 100000U

#if defined(_WIN32)
static int64_t yaza_win32_remove_tree_recursive(const char *dir_path, size_t depth, size_t *entries) {
    DWORD attrs = GetFileAttributesA(dir_path);
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
        (attrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        return 1;
    }
    size_t len = strlen(dir_path);
    if (len + 3 > 4096) return 1;
    char pattern[4096];
    memcpy(pattern, dir_path, len);
    if (len > 0 && pattern[len - 1] != '\\' && pattern[len - 1] != '/') pattern[len++] = '\\';
    pattern[len++] = '*';
    pattern[len] = '\0';
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return GetLastError() == ERROR_FILE_NOT_FOUND ? 0 : 1;
    }
    int64_t result = 0;
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        *entries += 1;
        if (*entries > YAZA_REMOVE_TREE_MAX_ENTRIES) {
            result = 1;
            break;
        }
        size_t name_len = strlen(fd.cFileName);
        if (len + name_len >= 4096) {
            result = 1;
            break;
        }
        char child[4096];
        memcpy(child, pattern, len - 1);
        memcpy(child + len - 1, fd.cFileName, name_len + 1);
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                if (!RemoveDirectoryA(child)) { result = 1; break; }
            } else {
                if (!DeleteFileA(child)) { result = 1; break; }
            }
        } else if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            if (depth >= YAZA_REMOVE_TREE_MAX_DEPTH) { result = 1; break; }
            result = yaza_win32_remove_tree_recursive(child, depth + 1, entries);
            if (result != 0) break;
            if (!RemoveDirectoryA(child)) { result = 1; break; }
        } else {
            if (!DeleteFileA(child)) { result = 1; break; }
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return result;
}

static bool yaza_is_managed_test_temp_path(const char *path) {
    char temp_base[4096];
    DWORD base_len = GetTempPathA((DWORD)sizeof(temp_base), temp_base);
    if (base_len == 0 || base_len >= sizeof(temp_base) || path == NULL) return false;
    static const char prefix[] = "yaza-run-";
    if (strncmp(path, temp_base, (size_t)base_len) != 0) return false;
    const char *rest = path + (size_t)base_len;
    if (strncmp(rest, prefix, sizeof(prefix) - 1) != 0) return false;
    const char *suffix = rest + sizeof(prefix) - 1;
    if (strlen(suffix) != 8) return false;
    for (size_t i = 0; i < 8; i++) {
        char c = suffix[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

int64_t yaza_rt_v1_test_temp_directory_remove(const YazaTextV1 *path) {
    if (!yaza_rt_v1_path_text(path, "test_temp_directory_remove")) return 1;
    const char *c_path = (const char *)path->data;
    if (!yaza_is_managed_test_temp_path(c_path)) return 1;
    size_t entries = 0;
    int64_t result = yaza_win32_remove_tree_recursive(c_path, 0, &entries);
    if (result != 0) return result;
    return RemoveDirectoryA(c_path) ? 0 : 1;
}
#else
static int64_t yaza_remove_tree_contents(int directory_fd, size_t depth, size_t *entries,
                                         dev_t root_device, const char *root_path) {
    int scan_fd = dup(directory_fd);
    if (scan_fd < 0) return 1;
    DIR *directory = fdopendir(scan_fd);
    if (directory == NULL) {
        close(scan_fd);
        return 1;
    }
    int64_t result = 0;
    for (;;) {
        errno = 0;
        struct dirent *entry = readdir(directory);
        if (entry == NULL) {
            if (errno != 0) result = 1;
            break;
        }
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        *entries += 1;
        if (*entries > YAZA_REMOVE_TREE_MAX_ENTRIES) {
            result = 1;
            break;
        }
        struct stat details;
        if (fstatat(directory_fd, entry->d_name, &details, AT_SYMLINK_NOFOLLOW) != 0) {
            if (errno == ENOENT) continue;
            result = 1;
            break;
        }
        if (S_ISDIR(details.st_mode)) {
            if (depth >= YAZA_REMOVE_TREE_MAX_DEPTH || details.st_dev != root_device) {
                result = 1;
                break;
            }
            int child_fd = openat(directory_fd, entry->d_name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
            if (child_fd < 0) {
                result = 1;
                break;
            }
            struct stat opened_details;
            if (fstat(child_fd, &opened_details) != 0 || opened_details.st_dev != details.st_dev ||
                opened_details.st_ino != details.st_ino || !S_ISDIR(opened_details.st_mode)) {
                close(child_fd);
                result = 1;
                break;
            }
            result = yaza_remove_tree_contents(child_fd, depth + 1, entries, root_device, root_path);
            if (close(child_fd) != 0 && result == 0) result = 1;
            if (result != 0) break;
            if (unlinkat(directory_fd, entry->d_name, AT_REMOVEDIR) != 0) {
                result = 1;
                break;
            }
        } else if (unlinkat(directory_fd, entry->d_name, 0) != 0) {
            result = 1;
            break;
        }
    }
    if (closedir(directory) != 0 && result == 0) result = 1;
    return result;
}

static bool yaza_is_managed_test_temp_path(const char *path) {
    static const char prefix[] = "/tmp/yaza-run.";
    if (path == NULL || strlen(path) != sizeof(prefix) - 1 + 6 ||
        memcmp(path, prefix, sizeof(prefix) - 1) != 0) return false;
    const char *suffix = path + sizeof(prefix) - 1;
    for (size_t index = 0; index < 6; index++) {
        unsigned char value = (unsigned char)suffix[index];
        if (!((value >= '0' && value <= '9') || (value >= 'A' && value <= 'Z') ||
              (value >= 'a' && value <= 'z'))) return false;
    }
    return true;
}

int64_t yaza_rt_v1_test_temp_directory_remove(const YazaTextV1 *path) {
    if (!yaza_rt_v1_path_text(path, "test_temp_directory_remove")) return 1;
    const char *c_path = (const char *)path->data;
    if (!yaza_is_managed_test_temp_path(c_path)) return 1;
    struct stat path_details;
    if (lstat(c_path, &path_details) != 0 || !S_ISDIR(path_details.st_mode) ||
        path_details.st_uid != geteuid() || (path_details.st_mode & 0777) != 0700) {
        return 1;
    }
    int directory_fd = open(c_path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    if (directory_fd < 0) return 1;
    struct stat details;
    int stat_status = fstat(directory_fd, &details);
    if (stat_status != 0 || !S_ISDIR(details.st_mode) || details.st_dev != path_details.st_dev ||
        details.st_ino != path_details.st_ino || details.st_uid != geteuid() ||
        (details.st_mode & 0777) != 0700) {
        close(directory_fd);
        return 1;
    }
    size_t entries = 0;
    int64_t result = yaza_remove_tree_contents(directory_fd, 0, &entries, details.st_dev, c_path);
    if (close(directory_fd) != 0 && result == 0) result = 1;
    if (result != 0) return result;
    if (rmdir(c_path) != 0) return 1;
    return 0;
}
#endif

static void yaza_vfs_v1_lock(void) {
    while (atomic_flag_test_and_set_explicit(&yaza_vfs_v1_registry_lock, memory_order_acquire)) {
    }
}

static void yaza_vfs_v1_unlock(void) {
    atomic_flag_clear_explicit(&yaza_vfs_v1_registry_lock, memory_order_release);
}

static uint64_t yaza_vfs_v1_temporary_limit(void) {
#if defined(YAZA_VFS_V1_TESTING)
    return atomic_load_explicit(&yaza_vfs_v1_test_temporary_limit, memory_order_relaxed);
#else
    return YAZA_VFS_V1_MAX_TEMPORARY_BYTES;
#endif
}

static int yaza_vfs_v1_reserve_temporary(uint64_t bytes) {
    if (bytes == 0) return 1;
    const uint64_t limit = yaza_vfs_v1_temporary_limit();
    uint64_t current = atomic_load_explicit(&yaza_vfs_v1_temporary_bytes, memory_order_relaxed);
    while (current <= limit && bytes <= limit - current) {
        if (atomic_compare_exchange_weak_explicit(&yaza_vfs_v1_temporary_bytes, &current, current + bytes,
                memory_order_acq_rel, memory_order_relaxed)) return 1;
    }
    return 0;
}

static void yaza_vfs_v1_release_temporary(uint64_t bytes) {
    if (bytes > 0) atomic_fetch_sub_explicit(&yaza_vfs_v1_temporary_bytes, bytes, memory_order_acq_rel);
}

static int yaza_vfs_v1_valid_text(const YazaTextV1 *text, uint64_t maximum, int allow_empty) {
    if (text == NULL || (text->length > 0 && text->data == NULL) || text->length > maximum ||
        (!allow_empty && text->length == 0) || text->length > SIZE_MAX ||
        (text->length > 0 && memchr(text->data, '\0', (size_t)text->length) != NULL)) return 0;
    return yaza_bytes_v1_is_valid_text(text->data, text->length);
}

static int yaza_vfs_v1_name(const YazaTextV1 *name, char buffer[YAZA_VFS_V1_MAX_NAME_BYTES + 1U]) {
    if (!yaza_vfs_v1_valid_text(name, YAZA_VFS_V1_MAX_NAME_BYTES, 0) ||
        (name->length == 1 && name->data[0] == '.') ||
        (name->length == 2 && name->data[0] == '.' && name->data[1] == '.')) return 0;
#if defined(_WIN32)
    for (uint64_t index = 0; index < name->length; ++index) {
        const unsigned char byte = name->data[index];
        if (byte == '/' || byte == '\\' || byte == ':' || byte < 0x20U || byte == 0x7fU) return 0;
    }
#else
    for (uint64_t index = 0; index < name->length; ++index) {
        const unsigned char byte = name->data[index];
        if (byte == '/' || byte == '\\' || byte < 0x20U || byte == 0x7fU) return 0;
    }
#endif
    memcpy(buffer, name->data, (size_t)name->length);
    buffer[name->length] = '\0';
    return 1;
}

static int32_t yaza_vfs_v1_register(int descriptor, uint32_t kind, uint32_t access,
                                    uint32_t parent_slot, uint64_t parent_generation,
                                    int64_t *out_handle, char **out_error) {
    if (out_error == NULL) return 0;
    *out_error = (char *)"invalid VFS handle registration";
    if (descriptor < 0 || out_handle == NULL) {
        return 0;
    }
    *out_handle = 0;
    *out_error = NULL;
    yaza_vfs_v1_lock();
    if (parent_slot != UINT32_MAX) {
        YazaVfsV1Slot *parent = &yaza_vfs_v1_slots[parent_slot];
        if (parent->kind != YAZA_VFS_V1_KIND_DIRECTORY || parent->generation != parent_generation ||
            parent->active_operations == 0 || parent->child_handles == UINT32_MAX) {
            yaza_vfs_v1_unlock();
            *out_error = (char *)"VFS root capability is stale or busy";
            return 0;
        }
    }
    uint32_t index = 0;
    while (index < YAZA_VFS_V1_ACTIVE_HANDLE_LIMIT && yaza_vfs_v1_slots[index].kind != YAZA_VFS_V1_KIND_FREE) index += 1;
    const uint64_t maximum_generation = (uint64_t)INT64_MAX >> 8;
    if (index == YAZA_VFS_V1_ACTIVE_HANDLE_LIMIT || yaza_vfs_v1_next_generation == 0 || yaza_vfs_v1_next_generation > maximum_generation) {
        yaza_vfs_v1_unlock();
        *out_error = (char *)"VFS handle capacity or generation limit reached";
        return 0;
    }
    const uint64_t generation = yaza_vfs_v1_next_generation++;
    YazaVfsV1Slot *slot = &yaza_vfs_v1_slots[index];
    slot->descriptor = descriptor;
    slot->generation = generation;
    slot->active_operations = 0;
    slot->child_handles = 0;
    slot->kind = kind;
    slot->access = access;
    slot->parent_slot = parent_slot;
    slot->parent_generation = parent_generation;
    if (parent_slot != UINT32_MAX) yaza_vfs_v1_slots[parent_slot].child_handles += 1;
    *out_handle = (int64_t)((generation << 8) | index);
    yaza_vfs_v1_unlock();
    return 1;
}

static int32_t yaza_vfs_v1_acquire(int64_t handle, uint32_t expected_kind, uint32_t required_access,
                                  uint32_t *out_slot, int *out_descriptor, char **out_error) {
    if (out_slot == NULL || out_descriptor == NULL || out_error == NULL) return 0;
    *out_error = (char *)"invalid or stale VFS capability";
    if (handle <= 0) return 0;
    const uint64_t token = (uint64_t)handle;
    const uint32_t index = (uint32_t)(token & 0xffU);
    const uint64_t generation = token >> 8;
    if (index >= YAZA_VFS_V1_ACTIVE_HANDLE_LIMIT) return 0;
    yaza_vfs_v1_lock();
    YazaVfsV1Slot *slot = &yaza_vfs_v1_slots[index];
    if (slot->kind == YAZA_VFS_V1_KIND_FREE || slot->generation != generation ||
        (expected_kind != 0 && slot->kind != expected_kind) ||
        (slot->access & required_access) != required_access || slot->active_operations == UINT32_MAX) {
        yaza_vfs_v1_unlock();
        return 0;
    }
    slot->active_operations += 1;
    *out_slot = index;
    *out_descriptor = slot->descriptor;
    *out_error = NULL;
    yaza_vfs_v1_unlock();
    return 1;
}

static void yaza_vfs_v1_release(uint32_t index, uint64_t generation) {
    yaza_vfs_v1_lock();
    YazaVfsV1Slot *slot = &yaza_vfs_v1_slots[index];
    if (slot->kind != YAZA_VFS_V1_KIND_FREE && slot->generation == generation && slot->active_operations > 0) {
        slot->active_operations -= 1;
    }
    yaza_vfs_v1_unlock();
}

#if defined(YAZA_VFS_V1_TESTING)
void yaza_vfs_v1_test_configure(uint32_t pread_eintr_count, uint32_t pwrite_eintr_count,
                                uint64_t max_io_chunk_bytes, int64_t pwrite_fail_after_bytes) {
    atomic_store_explicit(&yaza_vfs_v1_test_pread_eintr_count, pread_eintr_count, memory_order_relaxed);
    atomic_store_explicit(&yaza_vfs_v1_test_pwrite_eintr_count, pwrite_eintr_count, memory_order_relaxed);
    atomic_store_explicit(&yaza_vfs_v1_test_max_io_chunk_bytes, max_io_chunk_bytes, memory_order_relaxed);
    atomic_store_explicit(&yaza_vfs_v1_test_pwrite_fail_after_bytes, pwrite_fail_after_bytes, memory_order_relaxed);
}

void yaza_vfs_v1_test_set_temporary_limit(uint64_t limit_bytes) {
    atomic_store_explicit(&yaza_vfs_v1_test_temporary_limit, limit_bytes, memory_order_relaxed);
}

void yaza_vfs_v1_test_fail_next(uint32_t syscall_id) {
    atomic_fetch_or_explicit(&yaza_vfs_v1_test_failure_mask, syscall_id, memory_order_relaxed);
}

void yaza_vfs_v1_test_crash_after_fsync(uint64_t successful_sync_count) {
    atomic_store_explicit(&yaza_vfs_v1_test_fsync_counter, 0, memory_order_relaxed);
    atomic_store_explicit(&yaza_vfs_v1_test_fsync_crash_target, successful_sync_count, memory_order_relaxed);
}

uint64_t yaza_vfs_v1_test_fsync_count(void) {
    return atomic_load_explicit(&yaza_vfs_v1_test_fsync_counter, memory_order_relaxed);
}

static void yaza_vfs_v1_test_after_fsync(void) {
    const uint64_t count = atomic_fetch_add_explicit(&yaza_vfs_v1_test_fsync_counter, 1, memory_order_relaxed) + 1;
    const uint64_t target = atomic_load_explicit(&yaza_vfs_v1_test_fsync_crash_target, memory_order_relaxed);
    if (target != 0 && count == target) _exit(86);
}

static int yaza_vfs_v1_test_consume_failure(uint32_t syscall_id) {
    const uint32_t previous = atomic_fetch_and_explicit(&yaza_vfs_v1_test_failure_mask, ~syscall_id, memory_order_relaxed);
    return (previous & syscall_id) != 0;
}

static int yaza_vfs_v1_test_consume_eintr(_Atomic uint32_t *counter) {
    uint32_t remaining = atomic_load_explicit(counter, memory_order_relaxed);
    while (remaining != 0) {
        if (atomic_compare_exchange_weak_explicit(counter, &remaining, remaining - 1,
                memory_order_relaxed, memory_order_relaxed)) return 1;
    }
    return 0;
}

static size_t yaza_vfs_v1_test_limit_io_chunk(size_t requested) {
    const uint64_t maximum = atomic_load_explicit(&yaza_vfs_v1_test_max_io_chunk_bytes, memory_order_relaxed);
    return maximum > 0 && (uint64_t)requested > maximum ? (size_t)maximum : requested;
}
#endif

#if defined(_WIN32)
#ifndef O_DIRECTORY
#define O_DIRECTORY 0x100000
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0x200000
#endif
#ifndef O_NONBLOCK
#define O_NONBLOCK 0x400000
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC 0x800000
#endif
#ifndef LOCK_EX
#define LOCK_EX 2
#endif
#ifndef LOCK_NB
#define LOCK_NB 4
#endif
typedef int yaza_vfs_mode_t;
typedef int64_t yaza_vfs_off_t;

static int yaza_win32_dir_child_path(int dir_fd, const char *child, char *out, size_t out_cap) {
    HANDLE h = (HANDLE)_get_osfhandle(dir_fd);
    if (h == INVALID_HANDLE_VALUE) { errno = EBADF; return 0; }
    char dir_path[4096];
    DWORD n = GetFinalPathNameByHandleA(h, dir_path, (DWORD)(sizeof(dir_path) - 1U), FILE_NAME_NORMALIZED);
    if (n == 0 || (size_t)n >= sizeof(dir_path) - 1U) { errno = EIO; return 0; }
    dir_path[n] = '\0';
    DWORD dir_attrs = GetFileAttributesA(dir_path);
    if (dir_attrs == INVALID_FILE_ATTRIBUTES || (dir_attrs & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
        (dir_attrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        errno = ENOTDIR;
        return 0;
    }
    if (child == NULL || strcmp(child, ".") == 0) {
        if ((size_t)n >= out_cap) { errno = ENAMETOOLONG; return 0; }
        memcpy(out, dir_path, (size_t)n + 1U);
        return 1;
    }
    size_t clen = strlen(child);
    int need_sep = (n > 0 && dir_path[n - 1] != '\\' && dir_path[n - 1] != '/');
    if ((size_t)n + (need_sep ? 1U : 0U) + clen + 1U > out_cap) { errno = ENAMETOOLONG; return 0; }
    memcpy(out, dir_path, (size_t)n);
    size_t pos = (size_t)n;
    if (need_sep) out[pos++] = '\\';
    memcpy(out + pos, child, clen + 1U);
    return 1;
}

static int yaza_vfs_v1_open_os(const char *path, int flags) {
    (void)flags;
    HANDLE h = CreateFileA(path, GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           NULL, OPEN_EXISTING,
                           FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        errno = GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND ? ENOENT : EACCES;
        return -1;
    }
    BY_HANDLE_FILE_INFORMATION info;
    if (!GetFileInformationByHandle(h, &info) ||
        (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 ||
        (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        CloseHandle(h);
        errno = ENOTDIR;
        return -1;
    }
    int fd = _open_osfhandle((intptr_t)h, _O_RDONLY | _O_BINARY);
    if (fd < 0) CloseHandle(h);
    return fd;
}

static int yaza_vfs_v1_openat_os(int directory, const char *path, int flags, yaza_vfs_mode_t mode) {
    (void)mode;
    char full[4096];
    if (!yaza_win32_dir_child_path(directory, path, full, sizeof(full))) return -1;
    if ((flags & O_DIRECTORY) != 0) {
        return yaza_vfs_v1_open_os(full, flags);
    }
    int acc = flags & (_O_RDONLY | _O_WRONLY | _O_RDWR);
    DWORD desired = (acc == _O_RDONLY) ? GENERIC_READ : (GENERIC_READ | GENERIC_WRITE);
    DWORD disp = OPEN_EXISTING;
    if ((flags & _O_CREAT) != 0) disp = (flags & _O_EXCL) != 0 ? CREATE_NEW : OPEN_ALWAYS;
    HANDLE h = CreateFileA(full, desired,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           NULL, disp,
                           FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) errno = ENOENT;
        else if (err == ERROR_FILE_EXISTS || err == ERROR_ALREADY_EXISTS) errno = EEXIST;
        else errno = EACCES;
        return -1;
    }
    BY_HANDLE_FILE_INFORMATION info;
    if (!GetFileInformationByHandle(h, &info) ||
        (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE)) != 0) {
        CloseHandle(h);
        errno = ELOOP;
        return -1;
    }
    int fd = _open_osfhandle((intptr_t)h, (acc == _O_RDONLY ? _O_RDONLY : _O_RDWR) | _O_BINARY);
    if (fd < 0) CloseHandle(h);
    return fd;
}

static int yaza_vfs_v1_mkdirat_os(int directory, const char *path, yaza_vfs_mode_t mode) {
    (void)mode;
    char full[4096];
    if (!yaza_win32_dir_child_path(directory, path, full, sizeof(full))) return -1;
    if (CreateDirectoryA(full, NULL)) return 0;
    errno = GetLastError() == ERROR_ALREADY_EXISTS ? EEXIST : EIO;
    return -1;
}

static int yaza_vfs_v1_fstat_os(int descriptor, yaza_stat_t *status) {
    HANDLE h = (HANDLE)_get_osfhandle(descriptor);
    if (h == INVALID_HANDLE_VALUE) { errno = EBADF; return -1; }
    BY_HANDLE_FILE_INFORMATION info;
    if (!GetFileInformationByHandle(h, &info)) { errno = EBADF; return -1; }
    if ((info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) { errno = ELOOP; return -1; }
    memset(status, 0, sizeof(*status));
    status->st_mode = (unsigned short)((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ? _S_IFDIR : _S_IFREG);
    status->st_nlink = (short)(info.nNumberOfLinks > SHRT_MAX ? SHRT_MAX : info.nNumberOfLinks);
    uint64_t fsize = ((uint64_t)info.nFileSizeHigh << 32) | (uint64_t)info.nFileSizeLow;
    status->st_size = fsize > (uint64_t)INT64_MAX ? -1 : (__int64)fsize;
    return 0;
}

static int yaza_vfs_v1_fsync_os(int descriptor) {
    HANDLE h = (HANDLE)_get_osfhandle(descriptor);
    if (h == INVALID_HANDLE_VALUE) { errno = EBADF; return -1; }
    BY_HANDLE_FILE_INFORMATION info;
    if (GetFileInformationByHandle(h, &info) && (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return 0;
    }
    if (!FlushFileBuffers(h)) { errno = EIO; return -1; }
    return 0;
}

static int yaza_vfs_v1_ftruncate_os(int descriptor, yaza_vfs_off_t length) {
    HANDLE h = (HANDLE)_get_osfhandle(descriptor);
    if (h == INVALID_HANDLE_VALUE || length < 0) { errno = EBADF; return -1; }
    LARGE_INTEGER li;
    li.QuadPart = (LONGLONG)length;
    if (!SetFilePointerEx(h, li, NULL, FILE_BEGIN) || !SetEndOfFile(h)) {
        errno = EIO;
        return -1;
    }
    return 0;
}

static int yaza_vfs_v1_unlinkat_os(int directory, const char *path, int flags) {
    (void)flags;
    char full[4096];
    if (!yaza_win32_dir_child_path(directory, path, full, sizeof(full))) return -1;
    DWORD attrs = GetFileAttributesA(full);
    if (attrs == INVALID_FILE_ATTRIBUTES) { errno = ENOENT; return -1; }
    if ((attrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0 || (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        errno = EPERM;
        return -1;
    }
    if (!DeleteFileA(full)) { errno = EIO; return -1; }
    return 0;
}

static int yaza_vfs_v1_renameat_os(int old_directory, const char *old_path,
                                   int new_directory, const char *new_path) {
    char old_full[4096];
    char new_full[4096];
    if (!yaza_win32_dir_child_path(old_directory, old_path, old_full, sizeof(old_full))) return -1;
    if (!yaza_win32_dir_child_path(new_directory, new_path, new_full, sizeof(new_full))) return -1;
    if (!MoveFileExA(old_full, new_full, MOVEFILE_REPLACE_EXISTING)) {
        errno = EIO;
        return -1;
    }
    return 0;
}

static int yaza_vfs_v1_flock_os(int descriptor, int operation) {
    (void)operation;
    HANDLE h = (HANDLE)_get_osfhandle(descriptor);
    if (h == INVALID_HANDLE_VALUE) { errno = EBADF; return -1; }
    OVERLAPPED ov;
    memset(&ov, 0, sizeof(ov));
    if (!LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &ov)) {
        DWORD err = GetLastError();
        errno = (err == ERROR_LOCK_VIOLATION) ? EWOULDBLOCK : EIO;
        return -1;
    }
    return 0;
}

static int yaza_vfs_v1_close_os(int descriptor) {
    return _close(descriptor);
}

static ssize_t yaza_vfs_v1_pread_os(int descriptor, void *buffer, size_t length, yaza_vfs_off_t offset) {
    HANDLE h = (HANDLE)_get_osfhandle(descriptor);
    if (h == INVALID_HANDLE_VALUE || offset < 0) { errno = EBADF; return -1; }
    OVERLAPPED ov;
    memset(&ov, 0, sizeof(ov));
    ov.Offset = (DWORD)((uint64_t)offset & 0xffffffffULL);
    ov.OffsetHigh = (DWORD)((uint64_t)offset >> 32);
    DWORD chunk = length > 0x7fffffffULL ? 0x7fffffffUL : (DWORD)length;
    DWORD done = 0;
    if (!ReadFile(h, buffer, chunk, &done, &ov)) {
        if (GetLastError() == ERROR_HANDLE_EOF) return 0;
        errno = EIO;
        return -1;
    }
    return (ssize_t)done;
}

static ssize_t yaza_vfs_v1_pwrite_os(int descriptor, const void *buffer, size_t length, yaza_vfs_off_t offset) {
    HANDLE h = (HANDLE)_get_osfhandle(descriptor);
    if (h == INVALID_HANDLE_VALUE || offset < 0) { errno = EBADF; return -1; }
    OVERLAPPED ov;
    memset(&ov, 0, sizeof(ov));
    ov.Offset = (DWORD)((uint64_t)offset & 0xffffffffULL);
    ov.OffsetHigh = (DWORD)((uint64_t)offset >> 32);
    DWORD chunk = length > 0x7fffffffULL ? 0x7fffffffUL : (DWORD)length;
    DWORD done = 0;
    if (!WriteFile(h, buffer, chunk, &done, &ov)) {
        errno = EIO;
        return -1;
    }
    return (ssize_t)done;
}
#else
typedef mode_t yaza_vfs_mode_t;
typedef off_t yaza_vfs_off_t;
#define yaza_vfs_v1_open_os open
#define yaza_vfs_v1_openat_os openat
#define yaza_vfs_v1_mkdirat_os mkdirat
#define yaza_vfs_v1_fstat_os fstat
#define yaza_vfs_v1_fsync_os fsync
#define yaza_vfs_v1_ftruncate_os ftruncate
#define yaza_vfs_v1_unlinkat_os unlinkat
#define yaza_vfs_v1_renameat_os renameat
#define yaza_vfs_v1_flock_os flock
#define yaza_vfs_v1_close_os close
#define yaza_vfs_v1_pread_os pread
#define yaza_vfs_v1_pwrite_os pwrite
#endif

static int yaza_vfs_v1_open(const char *path, int flags) {
#if defined(YAZA_VFS_V1_TESTING)
    if (yaza_vfs_v1_test_consume_failure(YAZA_VFS_V1_TEST_FAIL_OPEN)) {
        errno = EIO;
        return -1;
    }
#endif
    return yaza_vfs_v1_open_os(path, flags);
}

static int yaza_vfs_v1_openat(int directory, const char *path, int flags, yaza_vfs_mode_t mode) {
#if defined(YAZA_VFS_V1_TESTING)
    if (yaza_vfs_v1_test_consume_failure(YAZA_VFS_V1_TEST_FAIL_OPENAT)) {
        errno = EIO;
        return -1;
    }
#endif
    return yaza_vfs_v1_openat_os(directory, path, flags, mode);
}

static int yaza_vfs_v1_fstat(int descriptor, yaza_stat_t *status) {
#if defined(YAZA_VFS_V1_TESTING)
    if (yaza_vfs_v1_test_consume_failure(YAZA_VFS_V1_TEST_FAIL_FSTAT)) {
        errno = EIO;
        return -1;
    }
#endif
    return yaza_vfs_v1_fstat_os(descriptor, status);
}

static int yaza_vfs_v1_fsync(int descriptor) {
#if defined(YAZA_VFS_V1_TESTING)
    if (yaza_vfs_v1_test_consume_failure(YAZA_VFS_V1_TEST_FAIL_FSYNC)) {
        errno = EIO;
        return -1;
    }
#endif
    const int result = yaza_vfs_v1_fsync_os(descriptor);
#if defined(YAZA_VFS_V1_TESTING)
    if (result == 0) yaza_vfs_v1_test_after_fsync();
#endif
    return result;
}

static int yaza_vfs_v1_ftruncate(int descriptor, yaza_vfs_off_t length) {
#if defined(YAZA_VFS_V1_TESTING)
    if (yaza_vfs_v1_test_consume_failure(YAZA_VFS_V1_TEST_FAIL_FTRUNCATE)) {
        errno = EIO;
        return -1;
    }
#endif
    return yaza_vfs_v1_ftruncate_os(descriptor, length);
}

static int yaza_vfs_v1_unlinkat(int directory, const char *path, int flags) {
#if defined(YAZA_VFS_V1_TESTING)
    if (yaza_vfs_v1_test_consume_failure(YAZA_VFS_V1_TEST_FAIL_UNLINKAT)) {
        errno = EIO;
        return -1;
    }
#endif
    return yaza_vfs_v1_unlinkat_os(directory, path, flags);
}

static int yaza_vfs_v1_renameat(int old_directory, const char *old_path,
                                int new_directory, const char *new_path) {
#if defined(YAZA_VFS_V1_TESTING)
    if (yaza_vfs_v1_test_consume_failure(YAZA_VFS_V1_TEST_FAIL_RENAMEAT)) {
        errno = EIO;
        return -1;
    }
#endif
    return yaza_vfs_v1_renameat_os(old_directory, old_path, new_directory, new_path);
}

static int yaza_vfs_v1_flock_descriptor(int descriptor, int operation) {
#if defined(YAZA_VFS_V1_TESTING)
    if (yaza_vfs_v1_test_consume_failure(YAZA_VFS_V1_TEST_FAIL_FLOCK)) {
        errno = EIO;
        return -1;
    }
#endif
    return yaza_vfs_v1_flock_os(descriptor, operation);
}

static int yaza_vfs_v1_close_descriptor(int descriptor) {
    const int result = yaza_vfs_v1_close_os(descriptor);
#if defined(YAZA_VFS_V1_TESTING)
    if (result == 0 && yaza_vfs_v1_test_consume_failure(YAZA_VFS_V1_TEST_FAIL_CLOSE)) {
        errno = EIO;
        return -1;
    }
#endif
    return result;
}

static ssize_t yaza_vfs_v1_pread(int descriptor, void *buffer, size_t length, yaza_vfs_off_t offset) {
#if defined(YAZA_VFS_V1_TESTING)
    if (yaza_vfs_v1_test_consume_eintr(&yaza_vfs_v1_test_pread_eintr_count)) {
        errno = EINTR;
        return -1;
    }
    length = yaza_vfs_v1_test_limit_io_chunk(length);
#endif
    return yaza_vfs_v1_pread_os(descriptor, buffer, length, offset);
}

static ssize_t yaza_vfs_v1_pwrite(int descriptor, const void *buffer, size_t length, yaza_vfs_off_t offset) {
#if defined(YAZA_VFS_V1_TESTING)
    if (yaza_vfs_v1_test_consume_eintr(&yaza_vfs_v1_test_pwrite_eintr_count)) {
        errno = EINTR;
        return -1;
    }
    int64_t remaining_before_failure = atomic_load_explicit(&yaza_vfs_v1_test_pwrite_fail_after_bytes, memory_order_relaxed);
    if (remaining_before_failure == 0) {
        atomic_store_explicit(&yaza_vfs_v1_test_pwrite_fail_after_bytes, -1, memory_order_relaxed);
        errno = EIO;
        return -1;
    }
    size_t limited_length = yaza_vfs_v1_test_limit_io_chunk(length);
    if (remaining_before_failure > 0 && (uint64_t)limited_length > (uint64_t)remaining_before_failure) {
        limited_length = (size_t)remaining_before_failure;
    }
    const ssize_t amount = yaza_vfs_v1_pwrite_os(descriptor, buffer, limited_length, offset);
    if (amount > 0 && remaining_before_failure > 0) {
        atomic_fetch_sub_explicit(&yaza_vfs_v1_test_pwrite_fail_after_bytes, amount, memory_order_relaxed);
    }
    return amount;
#else
    return yaza_vfs_v1_pwrite_os(descriptor, buffer, length, offset);
#endif
}

static int yaza_vfs_v1_preallocate_descriptor(int descriptor, yaza_vfs_off_t minimum_size) {
#if defined(YAZA_VFS_V1_TESTING)
    if (yaza_vfs_v1_test_consume_failure(YAZA_VFS_V1_TEST_FAIL_PREALLOCATE)) {
        errno = EIO;
        return -1;
    }
#endif
    if (minimum_size == 0) return 0;
#if defined(_WIN32)
    yaza_stat_t status;
    if (yaza_vfs_v1_fstat_os(descriptor, &status) != 0) return -1;
    if (status.st_size < minimum_size) return yaza_vfs_v1_ftruncate(descriptor, minimum_size);
    return 0;
#elif defined(__APPLE__)
    fstore_t allocation;
    memset(&allocation, 0, sizeof(allocation));
    allocation.fst_flags = F_ALLOCATECONTIG;
    allocation.fst_posmode = F_PEOFPOSMODE;
    allocation.fst_offset = 0;
    allocation.fst_length = minimum_size;
    if (fcntl(descriptor, F_PREALLOCATE, &allocation) == -1) {
        allocation.fst_flags = F_ALLOCATEALL;
        if (fcntl(descriptor, F_PREALLOCATE, &allocation) == -1) return -1;
    }
    yaza_stat_t status;
    if (yaza_vfs_v1_fstat_os(descriptor, &status) != 0) return -1;
    if (status.st_size < minimum_size) return yaza_vfs_v1_ftruncate(descriptor, minimum_size);
    return 0;
#else
    const int result = posix_fallocate(descriptor, 0, minimum_size);
    if (result != 0) {
        errno = result;
        return -1;
    }
    return 0;
#endif
}

int32_t yaza_vfs_v1_root_open(const YazaTextV1 *path, int64_t *out_handle, char **out_error) {
    if (out_error == NULL) return 0;
    *out_error = (char *)"invalid VFS root-open arguments";
    if (out_handle == NULL) return 0;
    *out_handle = 0;
    *out_error = NULL;
    if (!yaza_vfs_v1_valid_text(path, 4096, 0)) {
        *out_error = (char *)"VFS root path is invalid or exceeds 4096 bytes";
        return 0;
    }
    char path_buffer[4097];
    memcpy(path_buffer, path->data, (size_t)path->length);
    path_buffer[path->length] = '\0';
    uint64_t path_length = path->length;
    while (path_length > 1 && (path_buffer[path_length - 1] == '/' || path_buffer[path_length - 1] == '\\')) {
#if defined(_WIN32)
        if (path_length == 3 && path_buffer[1] == ':') break;
#endif
        path_length -= 1;
        path_buffer[path_length] = '\0';
    }
    const int descriptor = yaza_vfs_v1_open(path_buffer, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (descriptor < 0) {
        *out_error = (char *)"VFS root could not be opened as a non-symlink directory";
        return 0;
    }
    yaza_stat_t status;
#if defined(_WIN32)
    const int is_dir = yaza_vfs_v1_fstat(descriptor, &status) == 0 && (status.st_mode & _S_IFDIR) != 0;
#else
    const int is_dir = yaza_vfs_v1_fstat(descriptor, &status) == 0 && S_ISDIR(status.st_mode);
#endif
    if (!is_dir) {
        (void)yaza_vfs_v1_close_os(descriptor);
        *out_error = (char *)"VFS root is not a directory";
        return 0;
    }
    if (!yaza_vfs_v1_register(descriptor, YAZA_VFS_V1_KIND_DIRECTORY, 0, UINT32_MAX, 0, out_handle, out_error)) {
        (void)yaza_vfs_v1_close_os(descriptor);
        return 0;
    }
    return 1;
}

static int32_t yaza_vfs_v1_open_child_mode(int64_t root, const YazaTextV1 *name, int flags, uint32_t access,
                                           int require_private, int64_t *out_handle, char **out_error) {
    if (out_error == NULL) return 0;
    *out_error = (char *)"invalid VFS open arguments";
    if (out_handle == NULL) return 0;
    *out_handle = 0;
    *out_error = NULL;
    char name_buffer[YAZA_VFS_V1_MAX_NAME_BYTES + 1U];
    if (yaza_vfs_v1_name(name, name_buffer) == 0) {
        *out_error = (char *)"VFS file name must be one safe UTF-8 path component";
        return 0;
    }
    uint32_t root_slot = 0;
    int root_descriptor = -1;
    if (!yaza_vfs_v1_acquire(root, YAZA_VFS_V1_KIND_DIRECTORY, 0, &root_slot, &root_descriptor, out_error)) return 0;
    const uint64_t root_generation = (uint64_t)root >> 8;
    const int descriptor = yaza_vfs_v1_openat(root_descriptor, name_buffer,
        flags | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK, 0600);
    if (descriptor < 0) {
        yaza_vfs_v1_release(root_slot, root_generation);
        *out_error = (char *)"VFS file could not be opened relative to its root";
        return 0;
    }
    yaza_stat_t status;
#if defined(_WIN32)
    (void)require_private;
    const int child_ok = yaza_vfs_v1_fstat(descriptor, &status) == 0 && (status.st_mode & _S_IFREG) != 0 &&
        status.st_size >= 0 && (uint64_t)status.st_size <= YAZA_VFS_V1_MAX_FILE_BYTES;
#else
    const int base_ok = yaza_vfs_v1_fstat(descriptor, &status) == 0 && S_ISREG(status.st_mode) &&
        status.st_size >= 0 && (uint64_t)status.st_size <= YAZA_VFS_V1_MAX_FILE_BYTES;
    const int child_ok = base_ok && (!require_private || (status.st_uid == geteuid() && (status.st_mode & 077U) == 0));
#endif
    if (!child_ok) {
        (void)yaza_vfs_v1_close_os(descriptor);
        yaza_vfs_v1_release(root_slot, root_generation);
        *out_error = require_private
            ? (char *)"VFS child must be a private regular file owned by the current user and no larger than 1 TiB"
            : (char *)"VFS child must be a regular file no larger than 1 TiB";
        return 0;
    }
    const int32_t registered = yaza_vfs_v1_register(descriptor, YAZA_VFS_V1_KIND_FILE, access,
                                                     root_slot, root_generation, out_handle, out_error);
    yaza_vfs_v1_release(root_slot, root_generation);
    if (!registered) (void)yaza_vfs_v1_close_os(descriptor);
    return registered;
}

static int32_t yaza_vfs_v1_open_child(int64_t root, const YazaTextV1 *name, int flags, uint32_t access,
                                     int64_t *out_handle, char **out_error) {
    return yaza_vfs_v1_open_child_mode(root, name, flags, access, 1, out_handle, out_error);
}

int32_t yaza_vfs_v1_open_read(int64_t root, const YazaTextV1 *name, int64_t *out_handle, char **out_error) {
    return yaza_vfs_v1_open_child(root, name, O_RDONLY, YAZA_VFS_V1_ACCESS_READ, out_handle, out_error);
}

int32_t yaza_vfs_v1_open_read_public(int64_t root, const YazaTextV1 *name, int64_t *out_handle, char **out_error) {
    return yaza_vfs_v1_open_child_mode(root, name, O_RDONLY, YAZA_VFS_V1_ACCESS_READ, 0, out_handle, out_error);
}

int32_t yaza_vfs_v1_open_rw(int64_t root, const YazaTextV1 *name, int64_t *out_handle, char **out_error) {
    return yaza_vfs_v1_open_child(root, name, O_RDWR, YAZA_VFS_V1_ACCESS_READ | YAZA_VFS_V1_ACCESS_WRITE, out_handle, out_error);
}

int32_t yaza_vfs_v1_create_exclusive(int64_t root, const YazaTextV1 *name, int64_t *out_handle, char **out_error) {
    return yaza_vfs_v1_open_child(root, name, O_RDWR | O_CREAT | O_EXCL, YAZA_VFS_V1_ACCESS_READ | YAZA_VFS_V1_ACCESS_WRITE, out_handle, out_error);
}

int32_t yaza_vfs_v1_list_names(int64_t root, YazaBytesV1 *out_bytes, char **out_error) {
    if (out_error == NULL) return 0;
    *out_error = (char *)"invalid VFS directory-list arguments";
    if (out_bytes == NULL) return 0;
    *out_bytes = (YazaBytesV1){NULL, 0};
    uint32_t slot = 0;
    int descriptor = -1;
    if (!yaza_vfs_v1_acquire(root, YAZA_VFS_V1_KIND_DIRECTORY, 0, &slot, &descriptor, out_error)) return 0;
#if defined(_WIN32)
    char dir_pattern[4096];
    if (!yaza_win32_dir_child_path(descriptor, "*", dir_pattern, sizeof(dir_pattern))) {
        yaza_vfs_v1_release(slot, (uint64_t)root >> 8);
        *out_error = (char *)"VFS directory-list descriptor open failed";
        return 0;
    }
    WIN32_FIND_DATAA find_data;
    HANDLE find_handle = FindFirstFileA(dir_pattern, &find_data);
    int find_first = 1;
    if (find_handle == INVALID_HANDLE_VALUE) {
        yaza_vfs_v1_release(slot, (uint64_t)root >> 8);
        *out_error = (char *)"VFS directory-list stream could not be opened";
        return 0;
    }
#else
    const int duplicate = yaza_vfs_v1_openat(descriptor, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW, 0);
    if (duplicate < 0) {
        yaza_vfs_v1_release(slot, (uint64_t)root >> 8);
        *out_error = (char *)"VFS directory-list descriptor open failed";
        return 0;
    }
    DIR *directory = fdopendir(duplicate);
    if (directory == NULL) {
        (void)yaza_vfs_v1_close_os(duplicate);
        yaza_vfs_v1_release(slot, (uint64_t)root >> 8);
        *out_error = (char *)"VFS directory-list stream could not be opened";
        return 0;
    }
#endif
    size_t capacity = 4096U;
    size_t length = 0;
    uint64_t reserved = 0;
    int failed = !yaza_vfs_v1_reserve_temporary((uint64_t)capacity);
    if (!failed) reserved = (uint64_t)capacity;
    unsigned char *temporary = failed ? NULL : (unsigned char *)malloc(capacity);
    if (temporary == NULL) failed = 1;
    int iteration_error = 0;
    while (!failed) {
        const char *entry_name = NULL;
#if defined(_WIN32)
        if (find_first) {
            find_first = 0;
            entry_name = find_data.cFileName;
        } else if (FindNextFileA(find_handle, &find_data)) {
            entry_name = find_data.cFileName;
        } else {
            if (GetLastError() != ERROR_NO_MORE_FILES) iteration_error = EIO;
            break;
        }
#else
        errno = 0;
        struct dirent *entry = readdir(directory);
        if (entry == NULL) {
            iteration_error = errno;
            break;
        }
        entry_name = entry->d_name;
#endif
        if ((entry_name[0] == '.' && entry_name[1] == '\0') ||
            (entry_name[0] == '.' && entry_name[1] == '.' && entry_name[2] == '\0')) continue;
        const size_t name_length = strlen(entry_name);
        if (name_length == 0 || name_length > YAZA_VFS_V1_MAX_NAME_BYTES ||
            name_length + 1U > YAZA_VFS_V1_MAX_DIRECTORY_LIST_BYTES - length) {
            failed = 1;
            break;
        }
        const size_t required = length + name_length + 1U;
        if (required > capacity) {
            size_t new_capacity = capacity;
            while (new_capacity < required && new_capacity < YAZA_VFS_V1_MAX_DIRECTORY_LIST_BYTES / 2U) new_capacity *= 2U;
            if (new_capacity < required) new_capacity = required;
            const uint64_t growth = (uint64_t)(new_capacity - capacity);
            if (!yaza_vfs_v1_reserve_temporary(growth)) {
                failed = 1;
                break;
            }
            reserved += growth;
            unsigned char *grown = (unsigned char *)realloc(temporary, new_capacity);
            if (grown == NULL) {
                failed = 1;
                break;
            }
            temporary = grown;
            capacity = new_capacity;
        }
        memcpy(temporary + length, entry_name, name_length);
        temporary[length + name_length] = 0;
        length = required;
    }
#if defined(_WIN32)
    const int close_status = FindClose(find_handle) ? 0 : -1;
#else
    const int close_status = closedir(directory);
#endif
    yaza_vfs_v1_release(slot, (uint64_t)root >> 8);
    if (failed || iteration_error != 0 || close_status != 0) {
        free(temporary);
        yaza_vfs_v1_release_temporary(reserved);
        *out_error = failed ? (char *)"VFS directory listing exceeded its bounded temporary quota or allocation failed" :
            (iteration_error != 0 ? (char *)"VFS directory iteration failed" : (char *)"VFS directory-list stream close failed");
        return 0;
    }
    if (length != 0) {
        unsigned char *result = (unsigned char *)yaza_rt_v1_try_alloc((uint64_t)length);
        if (result == NULL) {
            free(temporary);
            yaza_vfs_v1_release_temporary(reserved);
            *out_error = (char *)"VFS directory listing exceeded the active arena quota";
            return 0;
        }
        memcpy(result, temporary, length);
        out_bytes->data = result;
        out_bytes->length = (uint64_t)length;
    }
    free(temporary);
    yaza_vfs_v1_release_temporary(reserved);
    *out_error = NULL;
    return 1;
}

int32_t yaza_vfs_v1_lock_exclusive(int64_t root, const YazaTextV1 *name, int64_t *out_handle, char **out_error) {
    if (out_error == NULL) return 0;
    *out_error = (char *)"invalid VFS lock arguments";
    if (out_handle == NULL) return 0;
    *out_handle = 0;
    *out_error = NULL;
    char name_buffer[YAZA_VFS_V1_MAX_NAME_BYTES + 1U];
    if (!yaza_vfs_v1_name(name, name_buffer)) {
        *out_error = (char *)"VFS lock name must be one safe UTF-8 path component";
        return 0;
    }
    uint32_t root_slot = 0;
    int root_descriptor = -1;
    if (!yaza_vfs_v1_acquire(root, YAZA_VFS_V1_KIND_DIRECTORY, 0, &root_slot, &root_descriptor, out_error)) return 0;
    const uint64_t root_generation = (uint64_t)root >> 8;
    yaza_stat_t root_status;
#if defined(_WIN32)
    const int root_ok = yaza_vfs_v1_fstat(root_descriptor, &root_status) == 0 && (root_status.st_mode & _S_IFDIR) != 0;
#else
    const int root_ok = yaza_vfs_v1_fstat(root_descriptor, &root_status) == 0 && S_ISDIR(root_status.st_mode) &&
        root_status.st_uid == geteuid() && (root_status.st_mode & (S_IWGRP | S_IWOTH)) == 0;
#endif
    if (!root_ok) {
        yaza_vfs_v1_release(root_slot, root_generation);
        *out_error = (char *)"VFS database root must be owned by this user and not group/world writable";
        return 0;
    }

    const char *lock_directory_name = ".yaza-locks";
    int lock_directory = yaza_vfs_v1_openat(root_descriptor, lock_directory_name,
        O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW, 0);
    if (lock_directory < 0 && errno == ENOENT) {
        if (yaza_vfs_v1_mkdirat_os(root_descriptor, lock_directory_name, 0700) != 0 && errno != EEXIST) {
            yaza_vfs_v1_release(root_slot, root_generation);
            *out_error = (char *)"VFS lock directory could not be created";
            return 0;
        }
        lock_directory = yaza_vfs_v1_openat(root_descriptor, lock_directory_name,
            O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW, 0);
    }
    if (lock_directory < 0) {
        yaza_vfs_v1_release(root_slot, root_generation);
        *out_error = (char *)"VFS lock directory is missing or is not a safe directory";
        return 0;
    }
    yaza_stat_t lock_directory_status;
#if defined(_WIN32)
    const int lock_dir_ok = yaza_vfs_v1_fstat(lock_directory, &lock_directory_status) == 0 &&
        (lock_directory_status.st_mode & _S_IFDIR) != 0;
#else
    const int lock_dir_ok = yaza_vfs_v1_fstat(lock_directory, &lock_directory_status) == 0 &&
        S_ISDIR(lock_directory_status.st_mode) && lock_directory_status.st_uid == geteuid() &&
        (lock_directory_status.st_mode & 077U) == 0;
#endif
    if (!lock_dir_ok) {
        (void)yaza_vfs_v1_close_os(lock_directory);
        yaza_vfs_v1_release(root_slot, root_generation);
        *out_error = (char *)"VFS lock directory must be owned by this user and not group/world writable";
        return 0;
    }
    const int descriptor = yaza_vfs_v1_openat(lock_directory, name_buffer,
        O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK, 0600);
    (void)yaza_vfs_v1_close_os(lock_directory);
    if (descriptor < 0) {
        yaza_vfs_v1_release(root_slot, root_generation);
        *out_error = (char *)"VFS lock file could not be opened safely";
        return 0;
    }
    yaza_stat_t lock_file_status;
#if defined(_WIN32)
    const int lock_file_ok = yaza_vfs_v1_fstat(descriptor, &lock_file_status) == 0 &&
        (lock_file_status.st_mode & _S_IFREG) != 0 && lock_file_status.st_nlink == 1;
#else
    const int lock_file_ok = yaza_vfs_v1_fstat(descriptor, &lock_file_status) == 0 &&
        S_ISREG(lock_file_status.st_mode) && lock_file_status.st_uid == geteuid() &&
        (lock_file_status.st_mode & 077U) == 0 && lock_file_status.st_nlink == 1;
#endif
    if (!lock_file_ok) {
        (void)yaza_vfs_v1_close_os(descriptor);
        yaza_vfs_v1_release(root_slot, root_generation);
        *out_error = (char *)"VFS lock file must be a private regular file owned by this user";
        return 0;
    }
    if (yaza_vfs_v1_flock_descriptor(descriptor, LOCK_EX | LOCK_NB) != 0) {
        const int lock_error = errno;
        (void)yaza_vfs_v1_close_os(descriptor);
        yaza_vfs_v1_release(root_slot, root_generation);
        *out_error = (char *)(lock_error == EWOULDBLOCK || lock_error == EAGAIN ?
            "database is already locked by another owner" : "VFS exclusive lock operation failed");
        return 0;
    }
    const int32_t registered = yaza_vfs_v1_register(descriptor, YAZA_VFS_V1_KIND_FILE, 0,
        root_slot, root_generation, out_handle, out_error);
    yaza_vfs_v1_release(root_slot, root_generation);
    if (!registered) (void)yaza_vfs_v1_close_os(descriptor);
    return registered;
}

int32_t yaza_vfs_v1_file_size(int64_t file, int64_t *out_size, char **out_error) {
    if (out_error == NULL) return 0;
    *out_error = (char *)"invalid VFS size arguments";
    if (out_size == NULL) return 0;
    *out_size = 0;
    uint32_t slot = 0;
    int descriptor = -1;
    if (!yaza_vfs_v1_acquire(file, YAZA_VFS_V1_KIND_FILE, 0, &slot, &descriptor, out_error)) return 0;
    yaza_stat_t status;
#if defined(_WIN32)
    const int ok = yaza_vfs_v1_fstat(descriptor, &status) == 0 && (status.st_mode & _S_IFREG) != 0 &&
        status.st_size >= 0 && (uint64_t)status.st_size <= YAZA_VFS_V1_MAX_FILE_BYTES;
#else
    const int ok = yaza_vfs_v1_fstat(descriptor, &status) == 0 && S_ISREG(status.st_mode) &&
        status.st_size >= 0 && (uint64_t)status.st_size <= YAZA_VFS_V1_MAX_FILE_BYTES;
#endif
    const uint64_t generation = (uint64_t)file >> 8;
    yaza_vfs_v1_release(slot, generation);
    if (!ok) {
        *out_error = (char *)"VFS file size query failed or exceeds 1 TiB";
        return 0;
    }
    *out_size = (int64_t)status.st_size;
    *out_error = NULL;
    return 1;
}

int32_t yaza_vfs_v1_read_exact(int64_t file, int64_t offset, int64_t length, YazaBytesV1 *out_bytes, char **out_error) {
    if (out_error == NULL) return 0;
    *out_error = (char *)"invalid VFS read arguments";
    if (out_bytes == NULL) return 0;
    *out_bytes = (YazaBytesV1){NULL, 0};
    *out_error = NULL;
    if (offset < 0 || length < 0 || (uint64_t)length > YAZA_VFS_V1_MAX_TRANSFER_BYTES ||
        (uint64_t)offset > YAZA_VFS_V1_MAX_FILE_BYTES - (uint64_t)length) {
        *out_error = (char *)"VFS read range is negative, over 16 MiB, or exceeds 1 TiB";
        return 0;
    }
    uint32_t slot = 0;
    int descriptor = -1;
    if (!yaza_vfs_v1_acquire(file, YAZA_VFS_V1_KIND_FILE, YAZA_VFS_V1_ACCESS_READ, &slot, &descriptor, out_error)) return 0;
    unsigned char *temporary = NULL;
    if (length > 0 && !yaza_vfs_v1_reserve_temporary((uint64_t)length)) {
        yaza_vfs_v1_release(slot, (uint64_t)file >> 8);
        *out_error = (char *)"VFS read exceeds the 64 MiB aggregate in-flight temporary-buffer quota";
        return 0;
    }
    if (length > 0) temporary = (unsigned char *)malloc((size_t)length);
    if (length > 0 && temporary == NULL) {
        yaza_vfs_v1_release_temporary((uint64_t)length);
        yaza_vfs_v1_release(slot, (uint64_t)file >> 8);
        *out_error = (char *)"VFS read could not allocate its bounded temporary buffer";
        return 0;
    }
    uint64_t received = 0;
    while (received < (uint64_t)length) {
        const uint64_t current_offset = (uint64_t)offset + received;
        const size_t chunk = (size_t)(((uint64_t)length - received) > (uint64_t)SSIZE_MAX ? (uint64_t)SSIZE_MAX : (uint64_t)length - received);
        const ssize_t amount = yaza_vfs_v1_pread(descriptor, temporary + received, chunk, (yaza_vfs_off_t)current_offset);
        if (amount < 0 && errno == EINTR) continue;
        if (amount <= 0) {
            free(temporary);
            yaza_vfs_v1_release_temporary((uint64_t)length);
            yaza_vfs_v1_release(slot, (uint64_t)file >> 8);
            *out_error = (char *)(amount == 0 ? "VFS exact read reached end of file" : "VFS exact read failed");
            return 0;
        }
        received += (uint64_t)amount;
    }
    unsigned char *buffer = NULL;
    if (length > 0) buffer = (unsigned char *)yaza_rt_v1_try_alloc((uint64_t)length);
    if (length > 0 && buffer == NULL) {
        free(temporary);
        yaza_vfs_v1_release_temporary((uint64_t)length);
        yaza_vfs_v1_release(slot, (uint64_t)file >> 8);
        *out_error = (char *)"VFS read buffer exceeds the active arena quota";
        return 0;
    }
    if (length > 0) memcpy(buffer, temporary, (size_t)length);
    free(temporary);
    yaza_vfs_v1_release_temporary((uint64_t)length);
    yaza_vfs_v1_release(slot, (uint64_t)file >> 8);
    *out_bytes = (YazaBytesV1){buffer, (uint64_t)length};
    return 1;
}

int32_t yaza_vfs_v1_write_exact(int64_t file, int64_t offset, const YazaBytesV1 *bytes, int64_t *out_written, char **out_error) {
    if (out_error == NULL) return 0;
    *out_error = (char *)"invalid VFS write arguments";
    if (out_written == NULL) return 0;
    *out_written = 0;
    *out_error = NULL;
    if (bytes == NULL || (bytes->length > 0 && bytes->data == NULL) || bytes->length > YAZA_VFS_V1_MAX_TRANSFER_BYTES ||
        offset < 0 || (uint64_t)offset > YAZA_VFS_V1_MAX_FILE_BYTES - bytes->length) {
        *out_error = (char *)"VFS write range or Bytes descriptor is invalid";
        return 0;
    }
    uint32_t slot = 0;
    int descriptor = -1;
    if (!yaza_vfs_v1_acquire(file, YAZA_VFS_V1_KIND_FILE, YAZA_VFS_V1_ACCESS_WRITE, &slot, &descriptor, out_error)) return 0;
    uint64_t written = 0;
    while (written < bytes->length) {
        const uint64_t current_offset = (uint64_t)offset + written;
        const size_t chunk = (size_t)((bytes->length - written) > (uint64_t)SSIZE_MAX ? (uint64_t)SSIZE_MAX : bytes->length - written);
        const ssize_t amount = yaza_vfs_v1_pwrite(descriptor, bytes->data + written, chunk, (yaza_vfs_off_t)current_offset);
        if (amount < 0 && errno == EINTR) continue;
        if (amount <= 0) {
            *out_written = (int64_t)written;
            yaza_vfs_v1_release(slot, (uint64_t)file >> 8);
            *out_error = (char *)"VFS exact write failed after a possibly partial write";
            return 0;
        }
        written += (uint64_t)amount;
    }
    yaza_vfs_v1_release(slot, (uint64_t)file >> 8);
    *out_written = (int64_t)written;
    return 1;
}

int32_t yaza_vfs_v1_truncate(int64_t file, int64_t size, char **out_error) {
    if (out_error == NULL) return 0;
    *out_error = (char *)"invalid VFS truncate arguments";
    if (size < 0 || (uint64_t)size > YAZA_VFS_V1_MAX_FILE_BYTES) {
        *out_error = (char *)"VFS truncate size is negative or exceeds 1 TiB";
        return 0;
    }
    uint32_t slot = 0;
    int descriptor = -1;
    if (!yaza_vfs_v1_acquire(file, YAZA_VFS_V1_KIND_FILE, YAZA_VFS_V1_ACCESS_WRITE, &slot, &descriptor, out_error)) return 0;
    const int result = yaza_vfs_v1_ftruncate(descriptor, (yaza_vfs_off_t)size);
    yaza_vfs_v1_release(slot, (uint64_t)file >> 8);
    if (result != 0) {
        *out_error = (char *)"VFS truncate failed";
        return 0;
    }
    *out_error = NULL;
    return 1;
}

int32_t yaza_vfs_v1_preallocate(int64_t file, int64_t minimum_size, char **out_error) {
    if (out_error == NULL) return 0;
    *out_error = (char *)"invalid VFS preallocation arguments";
    if (minimum_size < 0 || (uint64_t)minimum_size > YAZA_VFS_V1_MAX_FILE_BYTES) {
        *out_error = (char *)"VFS preallocation size is negative or exceeds 1 TiB";
        return 0;
    }
    uint32_t slot = 0;
    int descriptor = -1;
    if (!yaza_vfs_v1_acquire(file, YAZA_VFS_V1_KIND_FILE, YAZA_VFS_V1_ACCESS_WRITE, &slot, &descriptor, out_error)) return 0;
    yaza_stat_t status;
#if defined(_WIN32)
    const int prealloc_ok = yaza_vfs_v1_fstat(descriptor, &status) == 0 && (status.st_mode & _S_IFREG) != 0 &&
        status.st_size >= 0 && (uint64_t)status.st_size <= YAZA_VFS_V1_MAX_FILE_BYTES;
#else
    const int prealloc_ok = yaza_vfs_v1_fstat(descriptor, &status) == 0 && S_ISREG(status.st_mode) &&
        status.st_size >= 0 && (uint64_t)status.st_size <= YAZA_VFS_V1_MAX_FILE_BYTES;
#endif
    if (!prealloc_ok) {
        yaza_vfs_v1_release(slot, (uint64_t)file >> 8);
        *out_error = (char *)"VFS preallocation could not inspect the regular file";
        return 0;
    }
    const int result = yaza_vfs_v1_preallocate_descriptor(descriptor, (yaza_vfs_off_t)minimum_size);
    yaza_vfs_v1_release(slot, (uint64_t)file >> 8);
    if (result != 0) {
        *out_error = (char *)"VFS preallocation failed or the filesystem could not reserve the requested space";
        return 0;
    }
    *out_error = NULL;
    return 1;
}

int32_t yaza_vfs_v1_sync(int64_t handle, char **out_error) {
    if (out_error == NULL) return 0;
    *out_error = (char *)"invalid VFS sync arguments";
    uint32_t slot = 0;
    int descriptor = -1;
    if (!yaza_vfs_v1_acquire(handle, 0, 0, &slot, &descriptor, out_error)) return 0;
    const int result = yaza_vfs_v1_fsync(descriptor);
    yaza_vfs_v1_release(slot, (uint64_t)handle >> 8);
    if (result != 0) {
        *out_error = (char *)"VFS sync failed";
        return 0;
    }
    *out_error = NULL;
    return 1;
}

int32_t yaza_vfs_v1_sync_directory(int64_t root, char **out_error) {
    if (out_error == NULL) return 0;
    *out_error = (char *)"invalid VFS directory-sync arguments";
    uint32_t slot = 0;
    int descriptor = -1;
    if (!yaza_vfs_v1_acquire(root, YAZA_VFS_V1_KIND_DIRECTORY, 0, &slot, &descriptor, out_error)) return 0;
    const int result = yaza_vfs_v1_fsync(descriptor);
    yaza_vfs_v1_release(slot, (uint64_t)root >> 8);
    if (result != 0) {
        *out_error = (char *)"VFS directory sync failed";
        return 0;
    }
    *out_error = NULL;
    return 1;
}

int32_t yaza_vfs_v1_remove(int64_t root, const YazaTextV1 *name, char **out_error) {
    if (out_error == NULL) return 0;
    *out_error = (char *)"invalid VFS remove arguments";
    char name_buffer[YAZA_VFS_V1_MAX_NAME_BYTES + 1U];
    if (!yaza_vfs_v1_name(name, name_buffer)) {
        *out_error = (char *)"VFS remove name must be one safe UTF-8 path component";
        return 0;
    }
    uint32_t slot = 0;
    int descriptor = -1;
    if (!yaza_vfs_v1_acquire(root, YAZA_VFS_V1_KIND_DIRECTORY, 0, &slot, &descriptor, out_error)) return 0;
    const int result = yaza_vfs_v1_unlinkat(descriptor, name_buffer, 0);
    yaza_vfs_v1_release(slot, (uint64_t)root >> 8);
    if (result != 0) {
        *out_error = (char *)"VFS remove failed";
        return 0;
    }
    *out_error = NULL;
    return 1;
}

int32_t yaza_vfs_v1_rename(int64_t root, const YazaTextV1 *old_name, const YazaTextV1 *new_name, char **out_error) {
    if (out_error == NULL) return 0;
    *out_error = (char *)"invalid VFS rename arguments";
    char old_buffer[YAZA_VFS_V1_MAX_NAME_BYTES + 1U];
    char new_buffer[YAZA_VFS_V1_MAX_NAME_BYTES + 1U];
    if (!yaza_vfs_v1_name(old_name, old_buffer) || !yaza_vfs_v1_name(new_name, new_buffer)) {
        *out_error = (char *)"VFS rename names must be safe UTF-8 path components";
        return 0;
    }
    uint32_t slot = 0;
    int descriptor = -1;
    if (!yaza_vfs_v1_acquire(root, YAZA_VFS_V1_KIND_DIRECTORY, 0, &slot, &descriptor, out_error)) return 0;
    const int result = yaza_vfs_v1_renameat(descriptor, old_buffer, descriptor, new_buffer);
    yaza_vfs_v1_release(slot, (uint64_t)root >> 8);
    if (result != 0) {
        *out_error = (char *)"VFS rename failed";
        return 0;
    }
    *out_error = NULL;
    return 1;
}

int32_t yaza_vfs_v1_close(int64_t handle, char **out_error) {
    if (out_error == NULL) return 0;
    *out_error = (char *)"invalid VFS close arguments";
    if (handle <= 0) return 0;
    const uint64_t token = (uint64_t)handle;
    const uint32_t index = (uint32_t)(token & 0xffU);
    const uint64_t generation = token >> 8;
    if (index >= YAZA_VFS_V1_ACTIVE_HANDLE_LIMIT) {
        *out_error = (char *)"invalid or stale VFS capability";
        return 0;
    }
    yaza_vfs_v1_lock();
    YazaVfsV1Slot *slot = &yaza_vfs_v1_slots[index];
    if (slot->kind == YAZA_VFS_V1_KIND_FREE || slot->generation != generation) {
        yaza_vfs_v1_unlock();
        *out_error = (char *)"invalid or stale VFS capability";
        return 0;
    }
    if (slot->active_operations != 0 || (slot->kind == YAZA_VFS_V1_KIND_DIRECTORY && slot->child_handles != 0)) {
        yaza_vfs_v1_unlock();
        *out_error = (char *)"VFS capability is busy or still has open children";
        return 0;
    }
    const int descriptor = slot->descriptor;
    const uint32_t parent_slot = slot->parent_slot;
    const uint64_t parent_generation = slot->parent_generation;
    const int is_file = slot->kind == YAZA_VFS_V1_KIND_FILE;
    memset(slot, 0, sizeof(*slot));
    slot->descriptor = -1;
    slot->kind = YAZA_VFS_V1_KIND_FREE;
    if (is_file && parent_slot < YAZA_VFS_V1_MAX_HANDLES) {
        YazaVfsV1Slot *parent = &yaza_vfs_v1_slots[parent_slot];
        if (parent->kind == YAZA_VFS_V1_KIND_DIRECTORY && parent->generation == parent_generation && parent->child_handles > 0) {
            parent->child_handles -= 1;
        }
    }
    yaza_vfs_v1_unlock();
    if (yaza_vfs_v1_close_descriptor(descriptor) != 0) {
        *out_error = (char *)"VFS close reported an operating-system error; the capability is now closed";
        return 0;
    }
    *out_error = NULL;
    return 1;
}

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

typedef struct YazaNetV1Slot {
    yaza_net_sock_t descriptor;
    uint32_t kind;
    uint32_t flags;
    uint32_t active_operations;
    uint32_t closing;
    uint32_t peer_slot;
    uint32_t pg_cancel_pid;
    uint32_t pg_cancel_secret;
    uint32_t has_pg_cancel_key;
    uint64_t peer_generation;
    uint64_t generation;
    void *tls_ssl;
    void *tls_ctx;
} YazaNetV1Slot;

enum {
    YAZA_NET_V1_KIND_FREE = 0,
    YAZA_NET_V1_KIND_LISTENER = 1,
    YAZA_NET_V1_KIND_CONNECTION = 2,
};

static atomic_flag yaza_net_v1_registry_lock = ATOMIC_FLAG_INIT;
static YazaNetV1Slot yaza_net_v1_slots[YAZA_NET_V1_MAX_HANDLES];
static uint64_t yaza_net_v1_next_generation = 1;

typedef struct YazaNetTlsV1Api {
    int ready;
    void *lib_handle;
    const void *(*TLS_client_method)(void);
    void *(*SSL_CTX_new)(const void *method);
    void (*SSL_CTX_free)(void *ctx);
    void (*SSL_CTX_set_verify)(void *ctx, int mode, void *cb);
    int (*SSL_CTX_set_default_verify_paths)(void *ctx);
    int (*SSL_CTX_load_verify_locations)(void *ctx, const char *cafile, const char *capath);
    long (*SSL_CTX_ctrl)(void *ctx, int cmd, long larg, void *parg);
    void *(*SSL_CTX_get0_param)(void *ctx);
    int (*X509_VERIFY_PARAM_set1_ip_asc)(void *param, const char *ipasc);
    void *(*SSL_new)(void *ctx);
    void (*SSL_free)(void *ssl);
    int (*SSL_set_fd)(void *ssl, int fd);
    long (*SSL_ctrl)(void *ssl, int cmd, long larg, void *parg);
    int (*SSL_set1_host)(void *ssl, const char *hostname);
    int (*SSL_connect)(void *ssl);
    int (*SSL_read)(void *ssl, void *buf, int num);
    int (*SSL_write)(void *ssl, const void *buf, int num);
    int (*SSL_shutdown)(void *ssl);
    int (*SSL_get_error)(const void *ssl, int ret);
    long (*SSL_get_verify_result)(const void *ssl);
    void *(*SSL_get1_peer_certificate)(const void *ssl);
    void (*X509_free)(void *x);
} YazaNetTlsV1Api;

static YazaNetTlsV1Api yaza_net_tls_v1 = {0};
static pthread_once_t yaza_net_tls_v1_once = PTHREAD_ONCE_INIT;

#define YAZA_SSL_VERIFY_PEER 0x01
#define YAZA_SSL_ERROR_WANT_READ 2
#define YAZA_SSL_ERROR_WANT_WRITE 3
#define YAZA_SSL_CTRL_SET_TLSEXT_HOSTNAME 55
#define YAZA_SSL_CTRL_SET_MIN_PROTO_VERSION 123
#define YAZA_TLS1_2_VERSION 0x0303L

static void yaza_net_v1_tls_init_once(void) {
#if !defined(_WIN32)
    static const char *const candidates[] = {
        "/opt/homebrew/opt/openssl/lib/libssl.dylib",
        "/opt/homebrew/opt/openssl@3/lib/libssl.dylib",
        "/opt/homebrew/lib/libssl.dylib",
        "/usr/local/opt/openssl/lib/libssl.dylib",
        "libssl.4.dylib",
        "libssl.3.dylib",
        "libssl.dylib",
        "libssl.so.3",
        "libssl.so.1.1",
        "libssl.so",
        NULL
    };
    void *handle = NULL;
    for (size_t i = 0; candidates[i] != NULL; i++) {
        handle = dlopen(candidates[i], RTLD_LAZY | RTLD_LOCAL);
        if (handle != NULL) break;
    }
    if (handle == NULL) return;
    struct {
        const char *name;
        void *dst;
    } syms[] = {
        {"TLS_client_method", &yaza_net_tls_v1.TLS_client_method},
        {"SSL_CTX_new", &yaza_net_tls_v1.SSL_CTX_new},
        {"SSL_CTX_free", &yaza_net_tls_v1.SSL_CTX_free},
        {"SSL_CTX_set_verify", &yaza_net_tls_v1.SSL_CTX_set_verify},
        {"SSL_CTX_set_default_verify_paths", &yaza_net_tls_v1.SSL_CTX_set_default_verify_paths},
        {"SSL_CTX_load_verify_locations", &yaza_net_tls_v1.SSL_CTX_load_verify_locations},
        {"SSL_CTX_ctrl", &yaza_net_tls_v1.SSL_CTX_ctrl},
        {"SSL_CTX_get0_param", &yaza_net_tls_v1.SSL_CTX_get0_param},
        {"X509_VERIFY_PARAM_set1_ip_asc", &yaza_net_tls_v1.X509_VERIFY_PARAM_set1_ip_asc},
        {"SSL_new", &yaza_net_tls_v1.SSL_new},
        {"SSL_free", &yaza_net_tls_v1.SSL_free},
        {"SSL_set_fd", &yaza_net_tls_v1.SSL_set_fd},
        {"SSL_ctrl", &yaza_net_tls_v1.SSL_ctrl},
        {"SSL_set1_host", &yaza_net_tls_v1.SSL_set1_host},
        {"SSL_connect", &yaza_net_tls_v1.SSL_connect},
        {"SSL_read", &yaza_net_tls_v1.SSL_read},
        {"SSL_write", &yaza_net_tls_v1.SSL_write},
        {"SSL_shutdown", &yaza_net_tls_v1.SSL_shutdown},
        {"SSL_get_error", &yaza_net_tls_v1.SSL_get_error},
        {"SSL_get_verify_result", &yaza_net_tls_v1.SSL_get_verify_result},
        {"X509_free", &yaza_net_tls_v1.X509_free},
    };
    for (size_t i = 0; i < sizeof(syms) / sizeof(syms[0]); i++) {
        void *s = dlsym(handle, syms[i].name);
        if (s == NULL) {
            dlclose(handle);
            memset(&yaza_net_tls_v1, 0, sizeof(yaza_net_tls_v1));
            return;
        }
        memcpy(syms[i].dst, &s, sizeof(s));
    }
    void *peer_fn = dlsym(handle, "SSL_get1_peer_certificate");
    if (peer_fn == NULL) peer_fn = dlsym(handle, "SSL_get_peer_certificate");
    if (peer_fn == NULL) {
        dlclose(handle);
        memset(&yaza_net_tls_v1, 0, sizeof(yaza_net_tls_v1));
        return;
    }
    memcpy(&yaza_net_tls_v1.SSL_get1_peer_certificate, &peer_fn, sizeof(peer_fn));
    yaza_net_tls_v1.lib_handle = handle;
    yaza_net_tls_v1.ready = 1;
#endif
}

static int yaza_net_v1_tls_ensure(void) {
    (void)pthread_once(&yaza_net_tls_v1_once, yaza_net_v1_tls_init_once);
    return yaza_net_tls_v1.ready;
}

static void yaza_net_v1_tls_cleanup(void *ssl, void *ctx) {
    if (ssl != NULL && yaza_net_tls_v1.ready) {
        (void)yaza_net_tls_v1.SSL_shutdown(ssl);
        yaza_net_tls_v1.SSL_free(ssl);
    }
    if (ctx != NULL && yaza_net_tls_v1.ready) {
        yaza_net_tls_v1.SSL_CTX_free(ctx);
    }
}

static void yaza_net_v1_reset_slot_locked(YazaNetV1Slot *slot) {
    slot->descriptor = YAZA_NET_INVALID_SOCKET;
    slot->kind = YAZA_NET_V1_KIND_FREE;
    slot->flags = 0;
    slot->active_operations = 0;
    slot->closing = 0;
    slot->peer_slot = YAZA_NET_V1_MAX_HANDLES;
    slot->pg_cancel_pid = 0;
    slot->pg_cancel_secret = 0;
    slot->has_pg_cancel_key = 0;
    slot->peer_generation = 0;
    slot->tls_ssl = NULL;
    slot->tls_ctx = NULL;
}

#if defined(_WIN32)
static pthread_once_t yaza_net_v1_wsa_once = PTHREAD_ONCE_INIT;
static void yaza_net_v1_wsa_init(void) {
    WSADATA wsa;
    (void)WSAStartup(MAKEWORD(2, 2), &wsa);
}
static void yaza_net_v1_ensure_init(void) {
    (void)pthread_once(&yaza_net_v1_wsa_once, yaza_net_v1_wsa_init);
}
static int yaza_net_v1_close_sock(yaza_net_sock_t s) {
    return closesocket(s);
}
static void yaza_net_v1_set_cloexec(yaza_net_sock_t s) {
    (void)s;
}
static void yaza_net_v1_set_nonblocking(yaza_net_sock_t s) {
    u_long mode = 1;
    (void)ioctlsocket(s, FIONBIO, &mode);
}
static void yaza_net_v1_shutdown_sock(yaza_net_sock_t s) {
    (void)shutdown(s, SD_BOTH);
}
static int yaza_net_v1_last_wouldblock(void) {
    int err = WSAGetLastError();
    return err == WSAEWOULDBLOCK;
}
static int yaza_net_v1_last_eintr(void) {
    return WSAGetLastError() == WSAEINTR;
}
static int yaza_net_v1_poll_one(yaza_net_sock_t s, short events, int timeout_ms) {
    WSAPOLLFD pfd;
    pfd.fd = s;
    pfd.events = events;
    pfd.revents = 0;
    return WSAPoll(&pfd, 1, timeout_ms);
}
#else
static void yaza_net_v1_ensure_init(void) {}
static int yaza_net_v1_close_sock(yaza_net_sock_t s) {
    return close(s);
}
static void yaza_net_v1_set_cloexec(yaza_net_sock_t s) {
    int fd_flags = fcntl(s, F_GETFD, 0);
    if (fd_flags >= 0) (void)fcntl(s, F_SETFD, fd_flags | FD_CLOEXEC);
}
static void yaza_net_v1_set_nonblocking(yaza_net_sock_t s) {
    int fl = fcntl(s, F_GETFL, 0);
    if (fl >= 0) (void)fcntl(s, F_SETFL, fl | O_NONBLOCK);
}
static void yaza_net_v1_shutdown_sock(yaza_net_sock_t s) {
    (void)shutdown(s, SHUT_RDWR);
}
static int yaza_net_v1_last_wouldblock(void) {
    return errno == EAGAIN || errno == EWOULDBLOCK;
}
static int yaza_net_v1_last_eintr(void) {
    return errno == EINTR;
}
static int yaza_net_v1_poll_one(yaza_net_sock_t s, short events, int timeout_ms) {
    struct pollfd pfd;
    pfd.fd = s;
    pfd.events = events;
    pfd.revents = 0;
    return poll(&pfd, 1, timeout_ms);
}
#endif

static void yaza_net_v1_lock(void) {
    while (atomic_flag_test_and_set_explicit(&yaza_net_v1_registry_lock, memory_order_acquire)) {
    }
}

static void yaza_net_v1_unlock(void) {
    atomic_flag_clear_explicit(&yaza_net_v1_registry_lock, memory_order_release);
}

static uint64_t yaza_net_v1_alloc_generation_locked(void) {
    if (yaza_net_v1_next_generation > ((1ULL << 55) - 1ULL) || yaza_net_v1_next_generation == 0) {
        yaza_net_v1_next_generation = 1;
    }
    return yaza_net_v1_next_generation++;
}

static int yaza_net_v1_acquire(int64_t handle, uint32_t expected_kind, const char *err_msg,
                               uint32_t *out_index, yaza_net_sock_t *out_fd, char **out_error) {
    if (handle <= 0) {
        *out_error = (char *)err_msg;
        return 0;
    }
    const uint64_t token = (uint64_t)handle;
    const uint32_t index = (uint32_t)(token & 0xffU);
    const uint64_t generation = token >> 8;
    if (index >= YAZA_NET_V1_MAX_HANDLES) {
        *out_error = (char *)err_msg;
        return 0;
    }
    yaza_net_v1_lock();
    YazaNetV1Slot *slot = &yaza_net_v1_slots[index];
    if (slot->kind != expected_kind || slot->closing || slot->generation != generation ||
        slot->descriptor == YAZA_NET_INVALID_SOCKET) {
        yaza_net_v1_unlock();
        *out_error = (char *)err_msg;
        return 0;
    }
    slot->active_operations += 1U;
    *out_index = index;
    *out_fd = slot->descriptor;
    yaza_net_v1_unlock();
    return 1;
}

static void yaza_net_v1_release(uint32_t index) {
    yaza_net_sock_t fd_to_close = YAZA_NET_INVALID_SOCKET;
    void *tls_ssl_to_free = NULL;
    void *tls_ctx_to_free = NULL;
    yaza_net_v1_lock();
    YazaNetV1Slot *slot = &yaza_net_v1_slots[index];
    if (slot->active_operations > 0U) {
        slot->active_operations -= 1U;
    }
    if (slot->closing && slot->active_operations == 0U) {
        fd_to_close = slot->descriptor;
        tls_ssl_to_free = slot->tls_ssl;
        tls_ctx_to_free = slot->tls_ctx;
        yaza_net_v1_reset_slot_locked(slot);
    }
    yaza_net_v1_unlock();
    if (fd_to_close != YAZA_NET_INVALID_SOCKET) {
        yaza_net_v1_tls_cleanup(tls_ssl_to_free, tls_ctx_to_free);
        (void)yaza_net_v1_close_sock(fd_to_close);
    }
}

int32_t yaza_net_v1_listen(const YazaTextV1 *host, int64_t port, int64_t backlog, int64_t *out_handle, char **out_error) {
    if (out_error == NULL) return 0;
    *out_error = (char *)"invalid network listen arguments";
    if (out_handle == NULL) return 0;
    *out_handle = 0;
    *out_error = NULL;

    if (port < 1 || port > 65535) {
        *out_error = (char *)"network listen port must be between 1 and 65535";
        return 0;
    }
    int listen_backlog = (int)backlog;
    if (listen_backlog <= 0) listen_backlog = 128;
    if (listen_backlog > 1024) listen_backlog = 1024;

    if (host != NULL && host->length > 0 &&
        (host->data == NULL || host->length >= 256 ||
         memchr(host->data, '\0', (size_t)host->length) != NULL)) {
        *out_error = (char *)"network listen host address is invalid or too long";
        return 0;
    }

    yaza_net_v1_ensure_init();
    yaza_net_sock_t fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == YAZA_NET_INVALID_SOCKET) {
        *out_error = (char *)"network socket creation failed";
        return 0;
    }
    yaza_net_v1_set_cloexec(fd);

    int opt = 1;
    (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
#if defined(SO_NOSIGPIPE)
    (void)setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, (const char *)&opt, sizeof(opt));
#endif
    yaza_net_v1_set_nonblocking(fd);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);

    char host_buf[256];
    if (host != NULL && host->data != NULL && host->length > 0) {
        memcpy(host_buf, host->data, (size_t)host->length);
        host_buf[host->length] = '\0';
        if (strcmp(host_buf, "0.0.0.0") == 0 || strcmp(host_buf, "*") == 0) {
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
        } else if (strcmp(host_buf, "127.0.0.1") == 0 || strcmp(host_buf, "localhost") == 0) {
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        } else {
            if (inet_pton(AF_INET, host_buf, &addr.sin_addr) <= 0) {
                (void)yaza_net_v1_close_sock(fd);
                *out_error = (char *)"network listen host address is invalid IPv4";
                return 0;
            }
        }
    } else {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        (void)yaza_net_v1_close_sock(fd);
        *out_error = (char *)"network listen bind failed";
        return 0;
    }

    if (listen(fd, listen_backlog) != 0) {
        (void)yaza_net_v1_close_sock(fd);
        *out_error = (char *)"network listen failed";
        return 0;
    }

    yaza_net_v1_lock();
    uint32_t chosen = YAZA_NET_V1_MAX_HANDLES;
    for (uint32_t i = 0; i < YAZA_NET_V1_MAX_HANDLES; i++) {
        if (yaza_net_v1_slots[i].kind == YAZA_NET_V1_KIND_FREE) {
            chosen = i;
            break;
        }
    }
    if (chosen >= YAZA_NET_V1_MAX_HANDLES) {
        yaza_net_v1_unlock();
        (void)yaza_net_v1_close_sock(fd);
        *out_error = (char *)"network handle table exhausted";
        return 0;
    }
    const uint64_t gen = yaza_net_v1_alloc_generation_locked();
    YazaNetV1Slot *slot = &yaza_net_v1_slots[chosen];
    yaza_net_v1_reset_slot_locked(slot);
    slot->descriptor = fd;
    slot->kind = YAZA_NET_V1_KIND_LISTENER;
    slot->generation = gen;
    *out_handle = (int64_t)((gen << 8) | (uint64_t)chosen);
    yaza_net_v1_unlock();
    return 1;
}

int32_t yaza_net_v1_connect(const YazaTextV1 *host, int64_t port, int64_t timeout_ms, int64_t *out_handle, char **out_error) {
    if (out_error == NULL) return 0;
    *out_error = (char *)"invalid network connect arguments";
    if (out_handle == NULL) return 0;
    *out_handle = 0;
    *out_error = NULL;

    if (port < 1 || port > 65535) {
        *out_error = (char *)"network connect port must be between 1 and 65535";
        return 0;
    }
    if (host != NULL && host->length > 0 &&
        (host->data == NULL || host->length >= 256 ||
         memchr(host->data, '\0', (size_t)host->length) != NULL)) {
        *out_error = (char *)"network connect host address is invalid or too long";
        return 0;
    }
    int effective_timeout = (int)timeout_ms;
    if (effective_timeout <= 0) effective_timeout = 5000;

    yaza_net_v1_ensure_init();
    yaza_net_sock_t fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == YAZA_NET_INVALID_SOCKET) {
        *out_error = (char *)"network socket creation failed";
        return 0;
    }
    yaza_net_v1_set_cloexec(fd);

#if defined(SO_NOSIGPIPE)
    int opt = 1;
    (void)setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, (const char *)&opt, sizeof(opt));
#endif
    yaza_net_v1_set_nonblocking(fd);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);

    char host_buf[256];
    if (host != NULL && host->data != NULL && host->length > 0) {
        memcpy(host_buf, host->data, (size_t)host->length);
        host_buf[host->length] = '\0';
        if (strcmp(host_buf, "127.0.0.1") == 0 || strcmp(host_buf, "localhost") == 0) {
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        } else {
            if (inet_pton(AF_INET, host_buf, &addr.sin_addr) <= 0) {
                (void)yaza_net_v1_close_sock(fd);
                *out_error = (char *)"network connect host address is invalid IPv4";
                return 0;
            }
        }
    } else {
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    }

    int rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (rc != 0) {
#if defined(_WIN32)
        const int in_progress = (WSAGetLastError() == WSAEWOULDBLOCK || WSAGetLastError() == WSAEINPROGRESS);
#else
        const int in_progress = (errno == EINPROGRESS);
#endif
        if (in_progress) {
            int pr = yaza_net_v1_poll_one(fd, POLLOUT, effective_timeout);
            if (pr <= 0) {
                (void)yaza_net_v1_close_sock(fd);
                *out_error = pr == 0 ? (char *)"network connect timed out" : (char *)"network connect poll failed";
                return 0;
            }
            int sock_err = 0;
            socklen_t err_len = sizeof(sock_err);
            if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (char *)&sock_err, &err_len) != 0 || sock_err != 0) {
                (void)yaza_net_v1_close_sock(fd);
                *out_error = (char *)"network connection refused or unreachable";
                return 0;
            }
        } else {
            (void)yaza_net_v1_close_sock(fd);
            *out_error = (char *)"network connect failed immediately";
            return 0;
        }
    }

    yaza_net_v1_lock();
    uint32_t chosen = YAZA_NET_V1_MAX_HANDLES;
    for (uint32_t i = 0; i < YAZA_NET_V1_MAX_HANDLES; i++) {
        if (yaza_net_v1_slots[i].kind == YAZA_NET_V1_KIND_FREE) {
            chosen = i;
            break;
        }
    }
    if (chosen >= YAZA_NET_V1_MAX_HANDLES) {
        yaza_net_v1_unlock();
        (void)yaza_net_v1_close_sock(fd);
        *out_error = (char *)"network handle table exhausted";
        return 0;
    }
    const uint64_t gen = yaza_net_v1_alloc_generation_locked();
    YazaNetV1Slot *slot = &yaza_net_v1_slots[chosen];
    yaza_net_v1_reset_slot_locked(slot);
    slot->descriptor = fd;
    slot->kind = YAZA_NET_V1_KIND_CONNECTION;
    slot->generation = gen;
    *out_handle = (int64_t)((gen << 8) | (uint64_t)chosen);
    yaza_net_v1_unlock();
    return 1;
}

int32_t yaza_net_v1_accept(int64_t listener, int64_t timeout_ms, int64_t *out_handle, char **out_error) {
    if (out_error == NULL) return 0;
    *out_error = (char *)"invalid network accept arguments";
    if (out_handle == NULL) return 0;
    *out_handle = 0;
    *out_error = NULL;

    uint32_t index = 0;
    yaza_net_sock_t server_fd = YAZA_NET_INVALID_SOCKET;
    if (!yaza_net_v1_acquire(listener, YAZA_NET_V1_KIND_LISTENER, "invalid or closed network listener",
                             &index, &server_fd, out_error)) {
        return 0;
    }

    if (timeout_ms >= 0) {
        int poll_timeout = timeout_ms > 60000 ? 60000 : (int)timeout_ms;
        int pr = yaza_net_v1_poll_one(server_fd, POLLIN, poll_timeout);
        if (pr == 0) {
            yaza_net_v1_release(index);
            *out_error = (char *)"network accept timed out";
            return 0;
        }
        if (pr < 0) {
            yaza_net_v1_release(index);
            if (yaza_net_v1_last_eintr()) {
                *out_error = (char *)"network accept interrupted";
            } else {
                *out_error = (char *)"network accept poll failed";
            }
            return 0;
        }
    }

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    yaza_net_sock_t client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
    yaza_net_v1_release(index);
    if (client_fd == YAZA_NET_INVALID_SOCKET) {
        if (yaza_net_v1_last_wouldblock()) {
            *out_error = (char *)"network accept timed out";
        } else {
            *out_error = (char *)"network accept failed";
        }
        return 0;
    }
    yaza_net_v1_set_cloexec(client_fd);

    int opt = 1;
    (void)setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&opt, sizeof(opt));
#if defined(SO_NOSIGPIPE)
    (void)setsockopt(client_fd, SOL_SOCKET, SO_NOSIGPIPE, (const char *)&opt, sizeof(opt));
#endif
    yaza_net_v1_set_nonblocking(client_fd);

    yaza_net_v1_lock();
    uint32_t chosen = YAZA_NET_V1_MAX_HANDLES;
    for (uint32_t i = 0; i < YAZA_NET_V1_MAX_HANDLES; i++) {
        if (yaza_net_v1_slots[i].kind == YAZA_NET_V1_KIND_FREE) {
            chosen = i;
            break;
        }
    }
    if (chosen >= YAZA_NET_V1_MAX_HANDLES) {
        yaza_net_v1_unlock();
        (void)yaza_net_v1_close_sock(client_fd);
        *out_error = (char *)"network handle table exhausted";
        return 0;
    }
    const uint64_t gen = yaza_net_v1_alloc_generation_locked();
    YazaNetV1Slot *client_slot = &yaza_net_v1_slots[chosen];
    yaza_net_v1_reset_slot_locked(client_slot);
    client_slot->descriptor = client_fd;
    client_slot->kind = YAZA_NET_V1_KIND_CONNECTION;
    client_slot->generation = gen;
    *out_handle = (int64_t)((gen << 8) | (uint64_t)chosen);
    yaza_net_v1_unlock();
    return 1;
}

int32_t yaza_net_v1_read(int64_t connection, int64_t max_bytes, int64_t timeout_ms, YazaBytesV1 *out_bytes, char **out_error) {
    if (out_error == NULL) return 0;
    *out_error = (char *)"invalid network read arguments";
    if (out_bytes == NULL) return 0;
    out_bytes->data = NULL;
    out_bytes->length = 0;
    *out_error = NULL;

    uint32_t index = 0;
    yaza_net_sock_t fd = YAZA_NET_INVALID_SOCKET;
    if (!yaza_net_v1_acquire(connection, YAZA_NET_V1_KIND_CONNECTION, "invalid or closed network connection",
                             &index, &fd, out_error)) {
        return 0;
    }

    if (max_bytes <= 0) {
        yaza_net_v1_release(index);
        return 1;
    }
    if ((uint64_t)max_bytes > YAZA_NET_V1_MAX_TRANSFER_BYTES) {
        max_bytes = (int64_t)YAZA_NET_V1_MAX_TRANSFER_BYTES;
    }

    unsigned char stack_buf[65536];
    unsigned char *temp_buf = stack_buf;
    if ((uint64_t)max_bytes > sizeof(stack_buf)) {
        temp_buf = (unsigned char *)malloc((size_t)max_bytes);
        if (temp_buf == NULL) {
            yaza_net_v1_release(index);
            *out_error = (char *)"network read buffer allocation failed";
            return 0;
        }
    }

    yaza_net_v1_lock();
    void *tls_ssl = yaza_net_v1_slots[index].tls_ssl;
    yaza_net_v1_unlock();

    ssize_t bytes_read = -1;
    if (tls_ssl != NULL && yaza_net_tls_v1.ready) {
        int read_cap = max_bytes > (int64_t)INT_MAX ? INT_MAX : (int)max_bytes;
        for (;;) {
            int n = yaza_net_tls_v1.SSL_read(tls_ssl, temp_buf, read_cap);
            if (n >= 0) {
                bytes_read = (ssize_t)n;
                break;
            }
            int ssl_err = yaza_net_tls_v1.SSL_get_error(tls_ssl, n);
            if (ssl_err == YAZA_SSL_ERROR_WANT_READ || ssl_err == YAZA_SSL_ERROR_WANT_WRITE) {
                short ev = (ssl_err == YAZA_SSL_ERROR_WANT_READ) ? POLLIN : POLLOUT;
                int poll_timeout = timeout_ms < 0 ? -1 : (timeout_ms > 60000 ? 60000 : (int)timeout_ms);
                int pr = yaza_net_v1_poll_one(fd, ev, poll_timeout);
                if (pr == 0) {
                    if (temp_buf != stack_buf) free(temp_buf);
                    yaza_net_v1_release(index);
                    *out_error = (char *)"network read timed out";
                    return 0;
                }
                if (pr < 0 && !yaza_net_v1_last_eintr()) {
                    if (temp_buf != stack_buf) free(temp_buf);
                    yaza_net_v1_release(index);
                    *out_error = (char *)"network read poll failed";
                    return 0;
                }
                continue;
            }
            if (temp_buf != stack_buf) free(temp_buf);
            yaza_net_v1_release(index);
            *out_error = (char *)"network TLS read failed";
            return 0;
        }
    } else {
        for (;;) {
            if (timeout_ms >= 0) {
                int poll_timeout = timeout_ms > 60000 ? 60000 : (int)timeout_ms;
                int pr = yaza_net_v1_poll_one(fd, POLLIN, poll_timeout);
                if (pr == 0) {
                    if (temp_buf != stack_buf) free(temp_buf);
                    yaza_net_v1_release(index);
                    *out_error = (char *)"network read timed out";
                    return 0;
                }
                if (pr < 0) {
                    if (yaza_net_v1_last_eintr()) continue;
                    if (temp_buf != stack_buf) free(temp_buf);
                    yaza_net_v1_release(index);
                    *out_error = (char *)"network read poll failed";
                    return 0;
                }
            }
#if defined(_WIN32)
            bytes_read = (ssize_t)recv(fd, (char *)temp_buf, (int)max_bytes, 0);
#else
            bytes_read = recv(fd, (char *)temp_buf, (size_t)max_bytes, 0);
#endif
            if (bytes_read >= 0) break;
            if (yaza_net_v1_last_eintr()) continue;
            if (yaza_net_v1_last_wouldblock()) {
                if (timeout_ms < 0) {
                    int pr = yaza_net_v1_poll_one(fd, POLLIN, -1);
                    if (pr > 0 || (pr < 0 && yaza_net_v1_last_eintr())) continue;
                }
                if (temp_buf != stack_buf) free(temp_buf);
                yaza_net_v1_release(index);
                *out_error = (char *)"network read timed out";
                return 0;
            }
            if (temp_buf != stack_buf) free(temp_buf);
            yaza_net_v1_release(index);
            *out_error = (char *)"network read failed";
            return 0;
        }
    }

    yaza_net_v1_release(index);

    if (bytes_read == 0) {
        if (temp_buf != stack_buf) free(temp_buf);
        out_bytes->data = NULL;
        out_bytes->length = 0;
        return 1;
    }

    unsigned char *buffer = (unsigned char *)yaza_rt_v1_try_alloc((uint64_t)bytes_read);
    if (buffer == NULL) {
        if (temp_buf != stack_buf) free(temp_buf);
        *out_error = (char *)"network read exceeded active arena limit";
        return 0;
    }
    memcpy(buffer, temp_buf, (size_t)bytes_read);
    if (temp_buf != stack_buf) free(temp_buf);

    out_bytes->data = buffer;
    out_bytes->length = (uint64_t)bytes_read;
    return 1;
}

int32_t yaza_net_v1_write(int64_t connection, const YazaBytesV1 *bytes, int64_t timeout_ms, int64_t *out_written, char **out_error) {
    if (out_error == NULL) return 0;
    *out_error = (char *)"invalid network write arguments";
    if (out_written == NULL) return 0;
    *out_written = 0;
    *out_error = NULL;

    uint32_t index = 0;
    yaza_net_sock_t fd = YAZA_NET_INVALID_SOCKET;
    if (!yaza_net_v1_acquire(connection, YAZA_NET_V1_KIND_CONNECTION, "invalid or closed network connection",
                             &index, &fd, out_error)) {
        return 0;
    }

    if (bytes == NULL || bytes->data == NULL || bytes->length == 0) {
        yaza_net_v1_release(index);
        return 1;
    }
    if (bytes->length > YAZA_NET_V1_MAX_TRANSFER_BYTES) {
        yaza_net_v1_release(index);
        *out_error = (char *)"network write exceeds maximum transfer bytes";
        return 0;
    }

    yaza_net_v1_lock();
    void *tls_ssl = yaza_net_v1_slots[index].tls_ssl;
    yaza_net_v1_unlock();

    size_t total_written = 0;
    const size_t total_to_write = (size_t)bytes->length;

    while (total_written < total_to_write) {
        size_t chunk = total_to_write - total_written;
        if (chunk > (size_t)INT_MAX) chunk = (size_t)INT_MAX;

        if (tls_ssl != NULL && yaza_net_tls_v1.ready) {
            int n = yaza_net_tls_v1.SSL_write(tls_ssl, bytes->data + total_written, (int)chunk);
            if (n > 0) {
                total_written += (size_t)n;
                continue;
            }
            int ssl_err = yaza_net_tls_v1.SSL_get_error(tls_ssl, n);
            if (ssl_err == YAZA_SSL_ERROR_WANT_READ || ssl_err == YAZA_SSL_ERROR_WANT_WRITE) {
                short ev = (ssl_err == YAZA_SSL_ERROR_WANT_READ) ? POLLIN : POLLOUT;
                int poll_timeout = timeout_ms < 0 ? -1 : (timeout_ms > 60000 ? 60000 : (int)timeout_ms);
                int pr = yaza_net_v1_poll_one(fd, ev, poll_timeout);
                if (pr == 0) {
                    yaza_net_v1_release(index);
                    *out_error = (char *)"network write timed out";
                    *out_written = (int64_t)total_written;
                    return 0;
                }
                if (pr < 0 && !yaza_net_v1_last_eintr()) {
                    yaza_net_v1_release(index);
                    *out_error = (char *)"network write poll failed";
                    *out_written = (int64_t)total_written;
                    return 0;
                }
                continue;
            }
            yaza_net_v1_release(index);
            *out_error = (char *)"network TLS write failed";
            *out_written = (int64_t)total_written;
            return 0;
        }

        if (timeout_ms >= 0) {
            int poll_timeout = timeout_ms > 60000 ? 60000 : (int)timeout_ms;
            int pr = yaza_net_v1_poll_one(fd, POLLOUT, poll_timeout);
            if (pr == 0) {
                yaza_net_v1_release(index);
                *out_error = (char *)"network write timed out";
                *out_written = (int64_t)total_written;
                return 0;
            }
            if (pr < 0) {
                if (yaza_net_v1_last_eintr()) continue;
                yaza_net_v1_release(index);
                *out_error = (char *)"network write poll failed";
                *out_written = (int64_t)total_written;
                return 0;
            }
        }

#if defined(_WIN32)
        ssize_t n = (ssize_t)send(fd, (const char *)(bytes->data + total_written), (int)chunk, MSG_NOSIGNAL);
#else
        ssize_t n = send(fd, (const char *)(bytes->data + total_written), chunk, MSG_NOSIGNAL);
#endif
        if (n < 0) {
            if (yaza_net_v1_last_eintr()) continue;
            if (yaza_net_v1_last_wouldblock()) {
                if (timeout_ms < 0) {
                    int pr = yaza_net_v1_poll_one(fd, POLLOUT, -1);
                    if (pr > 0 || (pr < 0 && yaza_net_v1_last_eintr())) continue;
                } else {
                    continue;
                }
            }
            yaza_net_v1_release(index);
            *out_error = (char *)"network write failed";
            *out_written = (int64_t)total_written;
            return 0;
        }
        if (n == 0) {
            yaza_net_v1_release(index);
            *out_error = (char *)"network write connection closed";
            *out_written = (int64_t)total_written;
            return 0;
        }
        total_written += (size_t)n;
    }

    yaza_net_v1_release(index);
    *out_written = (int64_t)total_written;
    return 1;
}

int32_t yaza_net_v1_close(int64_t handle, char **out_error) {
    if (out_error == NULL) return 0;
    *out_error = (char *)"invalid network close arguments";
    if (handle <= 0) return 0;
    const uint64_t token = (uint64_t)handle;
    const uint32_t index = (uint32_t)(token & 0xffU);
    const uint64_t generation = token >> 8;
    if (index >= YAZA_NET_V1_MAX_HANDLES) {
        *out_error = (char *)"invalid network handle";
        return 0;
    }

    yaza_net_v1_lock();
    YazaNetV1Slot *slot = &yaza_net_v1_slots[index];
    if (slot->kind == YAZA_NET_V1_KIND_FREE || slot->closing || slot->generation != generation ||
        slot->descriptor == YAZA_NET_INVALID_SOCKET) {
        yaza_net_v1_unlock();
        *out_error = (char *)"invalid or closed network handle";
        return 0;
    }
    const yaza_net_sock_t fd = slot->descriptor;
    if (slot->active_operations > 0U) {
        slot->closing = 1;
        yaza_net_v1_unlock();
        yaza_net_v1_shutdown_sock(fd);
        *out_error = NULL;
        return 1;
    }
    void *tls_ssl_to_free = slot->tls_ssl;
    void *tls_ctx_to_free = slot->tls_ctx;
    yaza_net_v1_reset_slot_locked(slot);
    yaza_net_v1_unlock();

    yaza_net_v1_tls_cleanup(tls_ssl_to_free, tls_ctx_to_free);
    if (yaza_net_v1_close_sock(fd) != 0) {
        *out_error = (char *)"network close reported operating-system error";
        return 0;
    }
    *out_error = NULL;
    return 1;
}

int32_t yaza_net_v1_pair_create(int64_t *out_handle, char **out_error) {
    if (out_error == NULL) return 0;
    *out_error = (char *)"invalid socketpair arguments";
    if (out_handle == NULL) return 0;
    *out_handle = 0;
    *out_error = NULL;

    yaza_net_v1_ensure_init();
    yaza_net_sock_t fds[2];
#if defined(_WIN32)
    yaza_net_sock_t listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == YAZA_NET_INVALID_SOCKET) {
        *out_error = (char *)"socketpair creation failed";
        return 0;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    socklen_t addr_len = sizeof(addr);
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) != 0 ||
        listen(listener, 1) != 0 ||
        getsockname(listener, (struct sockaddr *)&addr, &addr_len) != 0) {
        (void)yaza_net_v1_close_sock(listener);
        *out_error = (char *)"socketpair creation failed";
        return 0;
    }
    fds[0] = socket(AF_INET, SOCK_STREAM, 0);
    if (fds[0] == YAZA_NET_INVALID_SOCKET ||
        connect(fds[0], (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        if (fds[0] != YAZA_NET_INVALID_SOCKET) (void)yaza_net_v1_close_sock(fds[0]);
        (void)yaza_net_v1_close_sock(listener);
        *out_error = (char *)"socketpair creation failed";
        return 0;
    }
    fds[1] = accept(listener, NULL, NULL);
    (void)yaza_net_v1_close_sock(listener);
    if (fds[1] == YAZA_NET_INVALID_SOCKET) {
        (void)yaza_net_v1_close_sock(fds[0]);
        *out_error = (char *)"socketpair creation failed";
        return 0;
    }
#else
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        *out_error = (char *)"socketpair creation failed";
        return 0;
    }
#endif

    for (int i = 0; i < 2; i++) {
        yaza_net_v1_set_cloexec(fds[i]);
        yaza_net_v1_set_nonblocking(fds[i]);
#if defined(SO_NOSIGPIPE)
        int opt = 1;
        (void)setsockopt(fds[i], SOL_SOCKET, SO_NOSIGPIPE, (const char *)&opt, sizeof(opt));
#endif
    }

    yaza_net_v1_lock();
    uint32_t slot_a = YAZA_NET_V1_MAX_HANDLES;
    uint32_t slot_b = YAZA_NET_V1_MAX_HANDLES;
    for (uint32_t i = 0; i < YAZA_NET_V1_MAX_HANDLES; i++) {
        if (yaza_net_v1_slots[i].kind == YAZA_NET_V1_KIND_FREE) {
            if (slot_a == YAZA_NET_V1_MAX_HANDLES) slot_a = i;
            else if (slot_b == YAZA_NET_V1_MAX_HANDLES) { slot_b = i; break; }
        }
    }
    if (slot_a == YAZA_NET_V1_MAX_HANDLES || slot_b == YAZA_NET_V1_MAX_HANDLES) {
        yaza_net_v1_unlock();
        (void)yaza_net_v1_close_sock(fds[0]);
        (void)yaza_net_v1_close_sock(fds[1]);
        *out_error = (char *)"network handle table exhausted";
        return 0;
    }

    const uint64_t gen_a = yaza_net_v1_alloc_generation_locked();
    const uint64_t gen_b = yaza_net_v1_alloc_generation_locked();

    YazaNetV1Slot *sa = &yaza_net_v1_slots[slot_a];
    yaza_net_v1_reset_slot_locked(sa);
    sa->descriptor = fds[0];
    sa->kind = YAZA_NET_V1_KIND_CONNECTION;
    sa->generation = gen_a;
    sa->peer_slot = slot_b;
    sa->peer_generation = gen_b;

    YazaNetV1Slot *sb = &yaza_net_v1_slots[slot_b];
    yaza_net_v1_reset_slot_locked(sb);
    sb->descriptor = fds[1];
    sb->kind = YAZA_NET_V1_KIND_CONNECTION;
    sb->generation = gen_b;
    sb->peer_slot = slot_a;
    sb->peer_generation = gen_a;

    *out_handle = (int64_t)((gen_a << 8) | (uint64_t)slot_a);
    yaza_net_v1_unlock();
    return 1;
}

int32_t yaza_net_v1_pair_peer(int64_t handle, int64_t *out_peer_handle, char **out_error) {
    if (out_error == NULL) return 0;
    *out_error = (char *)"invalid socketpair peer arguments";
    if (out_peer_handle == NULL) return 0;
    *out_peer_handle = 0;
    *out_error = NULL;

    if (handle <= 0) {
        *out_error = (char *)"invalid network handle";
        return 0;
    }
    const uint64_t token = (uint64_t)handle;
    const uint32_t index = (uint32_t)(token & 0xffU);
    const uint64_t generation = token >> 8;
    if (index >= YAZA_NET_V1_MAX_HANDLES) {
        *out_error = (char *)"invalid network handle";
        return 0;
    }

    yaza_net_v1_lock();
    YazaNetV1Slot *slot = &yaza_net_v1_slots[index];
    if (slot->kind != YAZA_NET_V1_KIND_CONNECTION || slot->closing || slot->generation != generation ||
        slot->descriptor == YAZA_NET_INVALID_SOCKET ||
        slot->peer_slot >= YAZA_NET_V1_MAX_HANDLES || slot->peer_generation == 0) {
        yaza_net_v1_unlock();
        *out_error = (char *)"invalid or closed network handle";
        return 0;
    }
    *out_peer_handle = (int64_t)((slot->peer_generation << 8) | (uint64_t)slot->peer_slot);
    yaza_net_v1_unlock();
    return 1;
}

int32_t yaza_net_v1_peer_is_loopback(int64_t connection, char **out_error) {
    if (out_error == NULL) return 0;
    *out_error = NULL;
    uint32_t index = 0;
    yaza_net_sock_t fd = YAZA_NET_INVALID_SOCKET;
    if (!yaza_net_v1_acquire(connection, YAZA_NET_V1_KIND_CONNECTION, "invalid or closed network connection",
                             &index, &fd, out_error)) {
        return 0;
    }
    yaza_net_v1_lock();
    const uint32_t peer_slot = yaza_net_v1_slots[index].peer_slot;
    const uint64_t peer_gen = yaza_net_v1_slots[index].peer_generation;
    yaza_net_v1_unlock();
    if (peer_slot < YAZA_NET_V1_MAX_HANDLES && peer_gen != 0) {
        yaza_net_v1_release(index);
        return 1;
    }

    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);
    memset(&addr, 0, sizeof(addr));
    int rc = getpeername(fd, (struct sockaddr *)&addr, &addr_len);
    yaza_net_v1_release(index);
    if (rc != 0) {
        *out_error = (char *)"network peer lookup failed";
        return 0;
    }
    if (addr.ss_family == AF_UNIX) return 1;
    if (addr.ss_family == AF_INET) {
        const struct sockaddr_in *in4 = (const struct sockaddr_in *)&addr;
        uint32_t ip = ntohl(in4->sin_addr.s_addr);
        if ((ip >> 24) == 127U) return 1;
        *out_error = (char *)"network peer is not loopback";
        return 0;
    }
    if (addr.ss_family == AF_INET6) {
        const struct sockaddr_in6 *in6 = (const struct sockaddr_in6 *)&addr;
        if (IN6_IS_ADDR_LOOPBACK(&in6->sin6_addr) ||
            (IN6_IS_ADDR_V4MAPPED(&in6->sin6_addr) && in6->sin6_addr.s6_addr[12] == 127U)) {
            return 1;
        }
        *out_error = (char *)"network peer is not loopback";
        return 0;
    }
    *out_error = (char *)"network peer is not loopback";
    return 0;
}

int32_t yaza_net_v1_tls_is_active(int64_t connection, char **out_error) {
    if (out_error == NULL) return 0;
    *out_error = NULL;
    uint32_t index = 0;
    yaza_net_sock_t fd = YAZA_NET_INVALID_SOCKET;
    if (!yaza_net_v1_acquire(connection, YAZA_NET_V1_KIND_CONNECTION, "invalid or closed network connection",
                             &index, &fd, out_error)) {
        return 0;
    }
    (void)fd;
    yaza_net_v1_lock();
    int active = yaza_net_v1_slots[index].tls_ssl != NULL;
    yaza_net_v1_unlock();
    yaza_net_v1_release(index);
    if (!active) {
        *out_error = (char *)"TLS is not active";
        return 0;
    }
    return 1;
}

int32_t yaza_net_v1_tls_client_with_ca(int64_t connection, const YazaTextV1 *host, const YazaTextV1 *root_cert,
                                       int64_t timeout_ms, char **out_error) {
    if (out_error == NULL) return 0;
    *out_error = NULL;
    if (timeout_ms < 1 || timeout_ms > 3600000 || host == NULL || host->data == NULL ||
        host->length < 1 || host->length > 255 ||
        memchr(host->data, '\0', (size_t)host->length) != NULL) {
        *out_error = (char *)"invalid TLS host or timeout arguments";
        return 0;
    }
    if (root_cert != NULL && root_cert->length > 0 &&
        (root_cert->data == NULL || root_cert->length > 4095 ||
         memchr(root_cert->data, '\0', (size_t)root_cert->length) != NULL)) {
        *out_error = (char *)"invalid TLS CA certificate path";
        return 0;
    }
    if (!yaza_net_v1_tls_ensure()) {
        *out_error = (char *)"OpenSSL shared library is unavailable on this host";
        return 0;
    }

    char host_buf[256];
    memcpy(host_buf, host->data, (size_t)host->length);
    host_buf[host->length] = '\0';

    char ca_buf[4096];
    const char *ca_path = NULL;
    if (root_cert != NULL && root_cert->data != NULL && root_cert->length > 0) {
        memcpy(ca_buf, root_cert->data, (size_t)root_cert->length);
        ca_buf[root_cert->length] = '\0';
        ca_path = ca_buf;
    } else {
        const char *env_ca = getenv("YAZA_TLS_CA_BUNDLE");
        if (env_ca != NULL && env_ca[0] != '\0') ca_path = env_ca;
    }

    uint32_t index = 0;
    yaza_net_sock_t fd = YAZA_NET_INVALID_SOCKET;
    if (!yaza_net_v1_acquire(connection, YAZA_NET_V1_KIND_CONNECTION, "invalid or closed network connection",
                             &index, &fd, out_error)) {
        return 0;
    }
    yaza_net_v1_lock();
    int already_tls = yaza_net_v1_slots[index].tls_ssl != NULL;
    yaza_net_v1_unlock();
    if (already_tls) {
        yaza_net_v1_release(index);
        *out_error = (char *)"TLS is already active on this connection";
        return 0;
    }

    const void *method = yaza_net_tls_v1.TLS_client_method();
    void *ctx = method != NULL ? yaza_net_tls_v1.SSL_CTX_new(method) : NULL;
    if (ctx == NULL) {
        yaza_net_v1_release(index);
        *out_error = (char *)"TLS context initialization failed";
        return 0;
    }
    (void)yaza_net_tls_v1.SSL_CTX_ctrl(ctx, YAZA_SSL_CTRL_SET_MIN_PROTO_VERSION, YAZA_TLS1_2_VERSION, NULL);
    yaza_net_tls_v1.SSL_CTX_set_verify(ctx, YAZA_SSL_VERIFY_PEER, NULL);
    if (ca_path != NULL) {
        if (yaza_net_tls_v1.SSL_CTX_load_verify_locations(ctx, ca_path, NULL) != 1) {
            yaza_net_tls_v1.SSL_CTX_free(ctx);
            yaza_net_v1_release(index);
            *out_error = (char *)"failed to load TLS CA certificate bundle";
            return 0;
        }
    } else if (yaza_net_tls_v1.SSL_CTX_set_default_verify_paths(ctx) != 1) {
#if defined(__APPLE__)
        if (yaza_net_tls_v1.SSL_CTX_load_verify_locations(ctx, "/etc/ssl/cert.pem", NULL) != 1)
#endif
        {
            yaza_net_tls_v1.SSL_CTX_free(ctx);
            yaza_net_v1_release(index);
            *out_error = (char *)"failed to load system default TLS CA certificates";
            return 0;
        }
    }

    void *ssl = yaza_net_tls_v1.SSL_new(ctx);
    if (ssl == NULL) {
        yaza_net_tls_v1.SSL_CTX_free(ctx);
        yaza_net_v1_release(index);
        *out_error = (char *)"TLS session allocation failed";
        return 0;
    }
    if (yaza_net_tls_v1.SSL_set_fd(ssl, (int)fd) != 1) {
        yaza_net_tls_v1.SSL_free(ssl);
        yaza_net_tls_v1.SSL_CTX_free(ctx);
        yaza_net_v1_release(index);
        *out_error = (char *)"TLS socket attachment failed";
        return 0;
    }

    struct in_addr ip4;
    struct in6_addr ip6;
    if (inet_pton(AF_INET, host_buf, &ip4) == 1 || inet_pton(AF_INET6, host_buf, &ip6) == 1) {
        void *param = yaza_net_tls_v1.SSL_CTX_get0_param(ctx);
        if (param == NULL || yaza_net_tls_v1.X509_VERIFY_PARAM_set1_ip_asc(param, host_buf) != 1) {
            yaza_net_tls_v1.SSL_free(ssl);
            yaza_net_tls_v1.SSL_CTX_free(ctx);
            yaza_net_v1_release(index);
            *out_error = (char *)"TLS IP verification configuration failed";
            return 0;
        }
    } else {
        (void)yaza_net_tls_v1.SSL_ctrl(ssl, YAZA_SSL_CTRL_SET_TLSEXT_HOSTNAME, 0, host_buf);
        if (yaza_net_tls_v1.SSL_set1_host(ssl, host_buf) != 1) {
            yaza_net_tls_v1.SSL_free(ssl);
            yaza_net_tls_v1.SSL_CTX_free(ctx);
            yaza_net_v1_release(index);
            *out_error = (char *)"TLS hostname verification configuration failed";
            return 0;
        }
    }

    int64_t deadline = yaza_rt_v1_monotonic_ms() + timeout_ms;
    for (;;) {
        int rc = yaza_net_tls_v1.SSL_connect(ssl);
        if (rc == 1) break;
        int ssl_err = yaza_net_tls_v1.SSL_get_error(ssl, rc);
        if (ssl_err == YAZA_SSL_ERROR_WANT_READ || ssl_err == YAZA_SSL_ERROR_WANT_WRITE) {
            int64_t remaining = deadline - yaza_rt_v1_monotonic_ms();
            if (remaining <= 0) {
                yaza_net_tls_v1.SSL_free(ssl);
                yaza_net_tls_v1.SSL_CTX_free(ctx);
                yaza_net_v1_release(index);
                *out_error = (char *)"TLS handshake timed out";
                return 0;
            }
            int poll_ms = remaining > 60000 ? 60000 : (int)remaining;
            int pr = yaza_net_v1_poll_one(fd, (ssl_err == YAZA_SSL_ERROR_WANT_READ) ? POLLIN : POLLOUT, poll_ms);
            if (pr == 0) {
                yaza_net_tls_v1.SSL_free(ssl);
                yaza_net_tls_v1.SSL_CTX_free(ctx);
                yaza_net_v1_release(index);
                *out_error = (char *)"TLS handshake timed out";
                return 0;
            }
            if (pr < 0 && !yaza_net_v1_last_eintr()) {
                yaza_net_tls_v1.SSL_free(ssl);
                yaza_net_tls_v1.SSL_CTX_free(ctx);
                yaza_net_v1_release(index);
                *out_error = (char *)"TLS handshake poll failed";
                return 0;
            }
            continue;
        }
        yaza_net_tls_v1.SSL_free(ssl);
        yaza_net_tls_v1.SSL_CTX_free(ctx);
        yaza_net_v1_release(index);
        *out_error = (char *)"TLS handshake or certificate verification failed";
        return 0;
    }

    void *peer_cert = yaza_net_tls_v1.SSL_get1_peer_certificate(ssl);
    if (peer_cert == NULL || yaza_net_tls_v1.SSL_get_verify_result(ssl) != 0) {
        if (peer_cert != NULL) yaza_net_tls_v1.X509_free(peer_cert);
        yaza_net_tls_v1.SSL_free(ssl);
        yaza_net_tls_v1.SSL_CTX_free(ctx);
        yaza_net_v1_release(index);
        *out_error = (char *)"TLS peer certificate verification failed";
        return 0;
    }
    yaza_net_tls_v1.X509_free(peer_cert);

    yaza_net_v1_lock();
    YazaNetV1Slot *slot = &yaza_net_v1_slots[index];
    if (slot->closing) {
        yaza_net_v1_unlock();
        yaza_net_tls_v1.SSL_free(ssl);
        yaza_net_tls_v1.SSL_CTX_free(ctx);
        yaza_net_v1_release(index);
        *out_error = (char *)"network connection closed during TLS handshake";
        return 0;
    }
    slot->tls_ssl = ssl;
    slot->tls_ctx = ctx;
    yaza_net_v1_unlock();
    yaza_net_v1_release(index);
    return 1;
}

int32_t yaza_net_v1_pg_set_cancel_key(int64_t connection, int64_t pid, int64_t secret, char **out_error) {
    if (out_error == NULL) return 0;
    *out_error = NULL;
    if (pid < 1 || pid > (int64_t)0xffffffffLL || secret < INT32_MIN || secret > (int64_t)0xffffffffLL) {
        *out_error = (char *)"invalid PostgreSQL backend cancel key";
        return 0;
    }
    uint32_t index = 0;
    yaza_net_sock_t fd = YAZA_NET_INVALID_SOCKET;
    if (!yaza_net_v1_acquire(connection, YAZA_NET_V1_KIND_CONNECTION, "invalid or closed network connection",
                             &index, &fd, out_error)) {
        return 0;
    }
    (void)fd;
    yaza_net_v1_lock();
    yaza_net_v1_slots[index].pg_cancel_pid = (uint32_t)pid;
    yaza_net_v1_slots[index].pg_cancel_secret = secret < 0 ? (uint32_t)(int32_t)secret : (uint32_t)secret;
    yaza_net_v1_slots[index].has_pg_cancel_key = 1U;
    yaza_net_v1_unlock();
    yaza_net_v1_release(index);
    return 1;
}

int32_t yaza_net_v1_pg_cancel(int64_t connection, int64_t timeout_ms, char **out_error) {
    if (out_error == NULL) return 0;
    *out_error = NULL;
    if (timeout_ms < 1 || timeout_ms > 60000) {
        *out_error = (char *)"invalid PostgreSQL cancel timeout";
        return 0;
    }
    uint32_t index = 0;
    yaza_net_sock_t fd = YAZA_NET_INVALID_SOCKET;
    if (!yaza_net_v1_acquire(connection, YAZA_NET_V1_KIND_CONNECTION, "invalid or closed network connection",
                             &index, &fd, out_error)) {
        return 0;
    }
    yaza_net_v1_lock();
    uint32_t has_key = yaza_net_v1_slots[index].has_pg_cancel_key;
    uint32_t pid = yaza_net_v1_slots[index].pg_cancel_pid;
    uint32_t secret = yaza_net_v1_slots[index].pg_cancel_secret;
    yaza_net_v1_unlock();
    if (!has_key) {
        yaza_net_v1_release(index);
        *out_error = (char *)"PostgreSQL cancel key has not been registered";
        return 0;
    }

    struct sockaddr_storage peer_addr;
    socklen_t peer_len = sizeof(peer_addr);
    memset(&peer_addr, 0, sizeof(peer_addr));
    int peer_rc = getpeername(fd, (struct sockaddr *)&peer_addr, &peer_len);
    yaza_net_v1_release(index);
    if (peer_rc != 0 || (peer_addr.ss_family != AF_INET && peer_addr.ss_family != AF_INET6)) {
        *out_error = (char *)"PostgreSQL cancel peer address is unavailable";
        return 0;
    }

    yaza_net_sock_t cancel_fd = socket(peer_addr.ss_family, SOCK_STREAM, 0);
    if (cancel_fd == YAZA_NET_INVALID_SOCKET) {
        *out_error = (char *)"PostgreSQL cancel socket creation failed";
        return 0;
    }
    yaza_net_v1_set_cloexec(cancel_fd);
    yaza_net_v1_set_nonblocking(cancel_fd);

    int rc = connect(cancel_fd, (const struct sockaddr *)&peer_addr, peer_len);
    if (rc != 0) {
#if defined(_WIN32)
        int in_prog = (WSAGetLastError() == WSAEWOULDBLOCK || WSAGetLastError() == WSAEINPROGRESS);
#else
        int in_prog = (errno == EINPROGRESS);
#endif
        if (!in_prog || yaza_net_v1_poll_one(cancel_fd, POLLOUT, (int)timeout_ms) <= 0) {
            (void)yaza_net_v1_close_sock(cancel_fd);
            *out_error = (char *)"PostgreSQL cancel connect failed or timed out";
            return 0;
        }
        int sock_err = 0;
        socklen_t err_len = sizeof(sock_err);
        if (getsockopt(cancel_fd, SOL_SOCKET, SO_ERROR, (char *)&sock_err, &err_len) != 0 || sock_err != 0) {
            (void)yaza_net_v1_close_sock(cancel_fd);
            *out_error = (char *)"PostgreSQL cancel connect refused";
            return 0;
        }
    }

    uint8_t pkt[16];
    uint32_t len_be = htonl(16U);
    uint32_t code_be = htonl(80877102U);
    uint32_t pid_be = htonl(pid);
    uint32_t sec_be = htonl(secret);
    memcpy(pkt + 0, &len_be, 4);
    memcpy(pkt + 4, &code_be, 4);
    memcpy(pkt + 8, &pid_be, 4);
    memcpy(pkt + 12, &sec_be, 4);

    size_t sent = 0;
    while (sent < sizeof(pkt)) {
        int pr = yaza_net_v1_poll_one(cancel_fd, POLLOUT, (int)timeout_ms);
        if (pr <= 0) {
            if (pr < 0 && yaza_net_v1_last_eintr()) continue;
            (void)yaza_net_v1_close_sock(cancel_fd);
            *out_error = (char *)"PostgreSQL cancel write timed out";
            return 0;
        }
        ssize_t n = send(cancel_fd, (const char *)(pkt + sent), sizeof(pkt) - sent, MSG_NOSIGNAL);
        if (n < 0 && yaza_net_v1_last_eintr()) continue;
        if (n <= 0) {
            (void)yaza_net_v1_close_sock(cancel_fd);
            *out_error = (char *)"PostgreSQL cancel write failed";
            return 0;
        }
        sent += (size_t)n;
    }

    (void)yaza_net_v1_close_sock(cancel_fd);
    return 1;
}

#if defined(_WIN32)
static int yaza_win32_append_quoted_arg(char **buf, size_t *len, size_t *cap, const char *arg, int add_space) {
    size_t arg_len = strlen(arg);
    int needs_quotes = (arg_len == 0);
    for (size_t i = 0; i < arg_len && !needs_quotes; i++) {
        if (arg[i] == ' ' || arg[i] == '\t' || arg[i] == '"') needs_quotes = 1;
    }
    size_t worst_case = *len + (add_space ? 1U : 0U) + (arg_len * 2U) + 4U;
    if (worst_case > *cap) {
        size_t new_cap = *cap == 0 ? 1024U : *cap * 2U;
        while (new_cap < worst_case) new_cap *= 2U;
        char *grown = (char *)realloc(*buf, new_cap);
        if (grown == NULL) return 0;
        *buf = grown;
        *cap = new_cap;
    }
    if (add_space) (*buf)[(*len)++] = ' ';
    if (!needs_quotes) {
        memcpy(*buf + *len, arg, arg_len);
        *len += arg_len;
        (*buf)[*len] = '\0';
        return 1;
    }
    (*buf)[(*len)++] = '"';
    size_t backslashes = 0;
    for (size_t i = 0; i < arg_len; i++) {
        char c = arg[i];
        if (c == '\\') {
            backslashes++;
        } else if (c == '"') {
            for (size_t b = 0; b < backslashes * 2U + 1U; b++) (*buf)[(*len)++] = '\\';
            (*buf)[(*len)++] = '"';
            backslashes = 0;
        } else {
            for (size_t b = 0; b < backslashes; b++) (*buf)[(*len)++] = '\\';
            (*buf)[(*len)++] = c;
            backslashes = 0;
        }
    }
    for (size_t b = 0; b < backslashes * 2U; b++) (*buf)[(*len)++] = '\\';
    (*buf)[(*len)++] = '"';
    (*buf)[*len] = '\0';
    return 1;
}
#endif

int64_t yaza_rt_v1_process(const YazaTextV1 *command, void *typed_text_list) {
    if (command == NULL || command->data == NULL || command->length == 0 || command->length > 4096 ||
        memchr(command->data, '\0', (size_t)command->length) != NULL || typed_text_list == NULL) {
        return 127;
    }
    const uint64_t argument_count = yaza_list_v1_count_typed(typed_text_list, "TextList", sizeof(YazaTextV1 *));
    if (argument_count > 4096 || argument_count > SIZE_MAX / sizeof(char *) - 2) return 127;
    char **arguments = (char **)calloc((size_t)argument_count + 2, sizeof(char *));
    if (arguments == NULL) return 127;
    arguments[0] = (char *)command->data;
    uint64_t total_bytes = command->length;
    for (uint64_t index = 0; index < argument_count; ++index) {
        YazaTextV1 *argument = NULL;
        if (!yaza_list_v1_try_get_typed(typed_text_list, (int64_t)index, "TextList", sizeof(argument), &argument) ||
            argument == NULL || (argument->length > 0 && argument->data == NULL) || argument->length > 65536 ||
            (argument->length > 0 && memchr(argument->data, '\0', (size_t)argument->length) != NULL) ||
            argument->length > 1024ULL * 1024ULL - total_bytes) {
            free(arguments);
            return 127;
        }
        total_bytes += argument->length;
        arguments[index + 1] = (argument->length > 0 && argument->data != NULL) ? (char *)argument->data : (char *)"";
    }
    /* Keep parent status lines visible and ordered before child output. */
    (void)fflush(stdout);
#if defined(YAZA_RT_V1_DISABLE_PROCESS_SPAWN)
    free(arguments);
    return 127;
#elif defined(_WIN32)
    char *cmdline = NULL;
    size_t cmdlen = 0;
    size_t cmdcap = 0;
    int ok = 1;
    for (uint64_t index = 0; index <= argument_count; ++index) {
        if (!yaza_win32_append_quoted_arg(&cmdline, &cmdlen, &cmdcap,
                                          arguments[index] == NULL ? "" : arguments[index], index > 0)) {
            ok = 0;
            break;
        }
    }
    free(arguments);
    if (!ok || cmdline == NULL) {
        free(cmdline);
        return 127;
    }
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    BOOL created = CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    free(cmdline);
    if (!created) return 127;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 127;
    if (!GetExitCodeProcess(pi.hProcess, &exit_code)) exit_code = 127;
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int64_t)exit_code;
#else
    struct sigaction ignore_sigint;
    struct sigaction prev_sigint;
    memset(&ignore_sigint, 0, sizeof(ignore_sigint));
    ignore_sigint.sa_handler = SIG_IGN;
    sigemptyset(&ignore_sigint.sa_mask);
    const int sig_saved = (sigaction(SIGINT, &ignore_sigint, &prev_sigint) == 0);

    posix_spawnattr_t spawn_attr;
    posix_spawnattr_t *attr_ptr = NULL;
    if (posix_spawnattr_init(&spawn_attr) == 0) {
        sigset_t default_sigs;
        sigemptyset(&default_sigs);
        sigaddset(&default_sigs, SIGINT);
        if (posix_spawnattr_setsigdefault(&spawn_attr, &default_sigs) == 0 &&
            posix_spawnattr_setflags(&spawn_attr, POSIX_SPAWN_SETSIGDEF) == 0) {
            attr_ptr = &spawn_attr;
        } else {
            posix_spawnattr_destroy(&spawn_attr);
        }
    }

    pid_t child = 0;
    const int spawn_status = posix_spawnp(&child, (const char *)command->data, NULL, attr_ptr, arguments, environ);
    if (attr_ptr != NULL) posix_spawnattr_destroy(&spawn_attr);
    free(arguments);
    if (spawn_status != 0) {
        if (sig_saved) (void)sigaction(SIGINT, &prev_sigint, NULL);
        return 127;
    }
    int wait_status = 0;
    for (;;) {
        if (waitpid(child, &wait_status, 0) >= 0) break;
        if (errno != EINTR) {
            if (sig_saved) (void)sigaction(SIGINT, &prev_sigint, NULL);
            return 127;
        }
    }
    if (sig_saved) (void)sigaction(SIGINT, &prev_sigint, NULL);
    if (WIFEXITED(wait_status)) return WEXITSTATUS(wait_status);
    if (WIFSIGNALED(wait_status)) return 128 + WTERMSIG(wait_status);
    return 127;
#endif
}

#if defined(__linux__)
static bool yaza_rt_v1_read_cpu_file(const char *path, char *buffer, size_t capacity) {
    FILE *file = fopen(path, "r");
    if (file == NULL) return false;
    const bool read_ok = fgets(buffer, (int)capacity, file) != NULL;
    (void)fclose(file);
    return read_ok;
}

static bool yaza_rt_v1_parse_cpu_u64(char **cursor, uint64_t *value) {
    while (isspace((unsigned char)**cursor)) ++*cursor;
    if (**cursor < '0' || **cursor > '9') return false;
    errno = 0;
    char *end = NULL;
    const unsigned long long parsed = strtoull(*cursor, &end, 10);
    if (errno == ERANGE || end == *cursor) return false;
    *cursor = end;
    *value = (uint64_t)parsed;
    return true;
}

static uint64_t yaza_rt_v1_cpu_quota_limit(void) {
    char line[128];
    if (yaza_rt_v1_read_cpu_file("/sys/fs/cgroup/cpu.max", line, sizeof(line))) {
        char *cursor = line;
        while (isspace((unsigned char)*cursor)) ++cursor;
        if (strncmp(cursor, "max", 3) != 0 || !isspace((unsigned char)cursor[3])) {
            uint64_t quota = 0;
            uint64_t period = 0;
            if (yaza_rt_v1_parse_cpu_u64(&cursor, &quota) &&
                yaza_rt_v1_parse_cpu_u64(&cursor, &period) && quota > 0 && period > 0) {
                return quota / period + (quota % period != 0 ? 1 : 0);
            }
        }
    }

    static const char *const quota_paths[] = {
        "/sys/fs/cgroup/cpu/cpu.cfs_quota_us",
        "/sys/fs/cgroup/cpu,cpuacct/cpu.cfs_quota_us"
    };
    static const char *const period_paths[] = {
        "/sys/fs/cgroup/cpu/cpu.cfs_period_us",
        "/sys/fs/cgroup/cpu,cpuacct/cpu.cfs_period_us"
    };
    for (size_t index = 0; index < sizeof(quota_paths) / sizeof(quota_paths[0]); ++index) {
        char quota_line[64];
        char period_line[64];
        if (!yaza_rt_v1_read_cpu_file(quota_paths[index], quota_line, sizeof(quota_line)) ||
            !yaza_rt_v1_read_cpu_file(period_paths[index], period_line, sizeof(period_line))) continue;
        char *quota_cursor = quota_line;
        char *period_cursor = period_line;
        while (isspace((unsigned char)*quota_cursor)) ++quota_cursor;
        while (isspace((unsigned char)*period_cursor)) ++period_cursor;
        errno = 0;
        char *quota_end = NULL;
        const long long quota_signed = strtoll(quota_cursor, &quota_end, 10);
        if (errno == ERANGE || quota_end == quota_cursor || quota_signed <= 0) continue;
        uint64_t period = 0;
        if (!yaza_rt_v1_parse_cpu_u64(&period_cursor, &period) || period == 0) continue;
        const uint64_t quota = (uint64_t)quota_signed;
        return quota / period + (quota % period != 0 ? 1 : 0);
    }
    return 0;
}
#endif

YazaTextV1 *yaza_rt_v1_os_platform(void) {
#if defined(__APPLE__)
    static const unsigned char platform[] = "macos";
#elif defined(_WIN32)
    static const unsigned char platform[] = "windows";
#elif defined(__linux__)
    static const unsigned char platform[] = "linux";
#else
    static const unsigned char platform[] = "unknown";
#endif
    YazaTextV1 *result = yaza_text_v1_copy_to_arena(yaza_arena_v1_current(), platform, sizeof(platform) - 1U);
    if (result == NULL) yaza_rt_v1_fail("rt_os_platform exceeded the active arena allocation limit");
    return result;
}

YazaTextV1 *yaza_rt_v1_os_arch(void) {
#if defined(__aarch64__) || defined(_M_ARM64)
    static const unsigned char arch[] = "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
    static const unsigned char arch[] = "x86_64";
#elif defined(__i386__) || defined(_M_IX86)
    static const unsigned char arch[] = "x86";
#elif defined(__arm__) || defined(_M_ARM)
    static const unsigned char arch[] = "arm";
#else
    static const unsigned char arch[] = "unknown";
#endif
    YazaTextV1 *result = yaza_text_v1_copy_to_arena(yaza_arena_v1_current(), arch, sizeof(arch) - 1U);
    if (result == NULL) yaza_rt_v1_fail("rt_os_arch exceeded the active arena allocation limit");
    return result;
}

int64_t yaza_rt_v1_processor_count(void) {
    uint64_t count = 0;
#if defined(__linux__)
    size_t cpu_capacity = CPU_SETSIZE;
    while (cpu_capacity <= 1048576ULL) {
        cpu_set_t *affinity = CPU_ALLOC(cpu_capacity);
        if (affinity == NULL) break;
        const size_t affinity_size = CPU_ALLOC_SIZE(cpu_capacity);
        CPU_ZERO_S(affinity_size, affinity);
        if (sched_getaffinity(0, affinity_size, affinity) == 0) {
            count = (uint64_t)CPU_COUNT_S(affinity_size, affinity);
            CPU_FREE(affinity);
            break;
        }
        const int affinity_error = errno;
        CPU_FREE(affinity);
        if (affinity_error != EINVAL || cpu_capacity == 1048576ULL) break;
        cpu_capacity *= 2;
    }
#elif defined(__APPLE__)
    unsigned int active = 0;
    size_t active_size = sizeof(active);
    if (sysctlbyname("hw.activecpu", &active, &active_size, NULL, 0) == 0 && active > 0) {
        count = active;
    } else {
        unsigned int logical = 0;
        size_t logical_size = sizeof(logical);
        if (sysctlbyname("hw.logicalcpu", &logical, &logical_size, NULL, 0) == 0 && logical > 0) {
            count = logical;
        }
    }
#elif defined(_WIN32)
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    if (sys_info.dwNumberOfProcessors > 0) count = (uint64_t)sys_info.dwNumberOfProcessors;
#endif
#if !defined(_WIN32)
    if (count == 0) {
        const long online = sysconf(_SC_NPROCESSORS_ONLN);
        if (online > 0) count = (uint64_t)online;
    }
#endif
#if defined(__linux__)
    const uint64_t quota_limit = yaza_rt_v1_cpu_quota_limit();
    if (quota_limit > 0 && (count == 0 || quota_limit < count)) count = quota_limit;
#endif
    if (count == 0) count = 1;
    if (count > (uint64_t)INT64_MAX) count = (uint64_t)INT64_MAX;
    return (int64_t)count;
}

static void *yaza_rt_v1_thread_entry(void *opaque) {
    YazaThreadV1 *thread = (YazaThreadV1 *)opaque;
    YazaArenaV1 *task_arena = yaza_arena_v1_create(yaza_rt_v1_config.process_limit_bytes);
    if (task_arena == NULL) {
        yaza_rt_v1_fail("worker thread failed to allocate isolated task arena");
        return NULL;
    }
    YazaArenaContextV1 task_context = {0};
    if (!yaza_arena_v1_scope_begin(task_arena, &task_context)) {
        (void)yaza_arena_v1_destroy(task_arena);
        yaza_rt_v1_fail("worker thread failed to enter isolated task arena scope");
        return NULL;
    }
    thread->entry(thread->context);
    (void)yaza_arena_v1_scope_end(&task_context);
    (void)yaza_arena_v1_destroy(task_arena);
    return NULL;
}

int32_t yaza_rt_v1_thread_spawn(YazaThreadEntryV1 entry, void *context, YazaThreadV1 **out_thread) {
    if (out_thread == NULL) return EINVAL;
    *out_thread = NULL;
    if (entry == NULL) return EINVAL;
    yaza_rt_v1_init();
    YazaThreadV1 *thread = (YazaThreadV1 *)calloc(1, sizeof(*thread));
    if (thread == NULL) return ENOMEM;
    thread->entry = entry;
    thread->context = context;
    const int status = pthread_create(&thread->native_thread, NULL, yaza_rt_v1_thread_entry, thread);
    if (status != 0) {
        free(thread);
        return status;
    }
    *out_thread = thread;
    return 0;
}

int32_t yaza_rt_v1_thread_join(YazaThreadV1 *thread) {
    if (thread == NULL) return EINVAL;
    const int status = pthread_join(thread->native_thread, NULL);
    if (status != 0) return status;
    free(thread);
    return 0;
}

YazaArenaV1 *yaza_rt_v1_task_arena_handle(void) {
    yaza_rt_v1_init();
    return yaza_rt_v1_current_arena;
}

int32_t yaza_rt_v1_mutex_create(YazaMutexV1 **out_mutex) {
    if (out_mutex == NULL) return EINVAL;
    *out_mutex = NULL;
    YazaMutexV1 *mutex = (YazaMutexV1 *)calloc(1, sizeof(*mutex));
    if (mutex == NULL) return ENOMEM;
    const int status = pthread_mutex_init(&mutex->native_mutex, NULL);
    if (status != 0) {
        free(mutex);
        return status;
    }
    *out_mutex = mutex;
    return 0;
}

int32_t yaza_rt_v1_mutex_destroy(YazaMutexV1 *mutex) {
    if (mutex == NULL) return EINVAL;
    const int status = pthread_mutex_destroy(&mutex->native_mutex);
    if (status != 0) return status;
    free(mutex);
    return 0;
}

int32_t yaza_rt_v1_mutex_lock(YazaMutexV1 *mutex) {
    if (mutex == NULL) return EINVAL;
    return pthread_mutex_lock(&mutex->native_mutex);
}

int32_t yaza_rt_v1_mutex_unlock(YazaMutexV1 *mutex) {
    if (mutex == NULL) return EINVAL;
    return pthread_mutex_unlock(&mutex->native_mutex);
}

int32_t yaza_rt_v1_condition_create(YazaConditionV1 **out_condition) {
    if (out_condition == NULL) return EINVAL;
    *out_condition = NULL;
    YazaConditionV1 *condition = (YazaConditionV1 *)calloc(1, sizeof(*condition));
    if (condition == NULL) return ENOMEM;
    const int status = pthread_cond_init(&condition->native_condition, NULL);
    if (status != 0) {
        free(condition);
        return status;
    }
    *out_condition = condition;
    return 0;
}

int32_t yaza_rt_v1_condition_destroy(YazaConditionV1 *condition) {
    if (condition == NULL) return EINVAL;
    const int status = pthread_cond_destroy(&condition->native_condition);
    if (status != 0) return status;
    free(condition);
    return 0;
}

int32_t yaza_rt_v1_condition_wait(YazaConditionV1 *condition, YazaMutexV1 *mutex) {
    if (condition == NULL || mutex == NULL) return EINVAL;
    return pthread_cond_wait(&condition->native_condition, &mutex->native_mutex);
}

int32_t yaza_rt_v1_condition_signal(YazaConditionV1 *condition) {
    if (condition == NULL) return EINVAL;
    return pthread_cond_signal(&condition->native_condition);
}

int32_t yaza_rt_v1_condition_broadcast(YazaConditionV1 *condition) {
    if (condition == NULL) return EINVAL;
    return pthread_cond_broadcast(&condition->native_condition);
}

int64_t yaza_rt_v1_monotonic_ms(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0 || now.tv_sec < 0 ||
        (uint64_t)now.tv_sec > ((uint64_t)INT64_MAX - (uint64_t)now.tv_nsec / 1000000ULL) / 1000ULL) {
        return 0;
    }
    return (int64_t)((uint64_t)now.tv_sec * 1000ULL + (uint64_t)now.tv_nsec / 1000000ULL);
}

int64_t yaza_rt_v1_monotonic_ns(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0 || now.tv_sec < 0 || now.tv_nsec < 0 || now.tv_nsec >= 1000000000L ||
        (uint64_t)now.tv_sec > ((uint64_t)INT64_MAX - (uint64_t)now.tv_nsec) / 1000000000ULL) {
        return 0;
    }
    return (int64_t)((uint64_t)now.tv_sec * 1000000000ULL + (uint64_t)now.tv_nsec);
}

YazaTextV1 *yaza_rt_v1_executable_path(void) {
    char path[PATH_MAX];
    size_t length = 0;
#if defined(_WIN32)
    DWORD win_len = GetModuleFileNameA(NULL, path, (DWORD)(sizeof(path) - 1U));
    if (win_len == 0 || (size_t)win_len >= sizeof(path) - 1U) {
        yaza_rt_v1_fail("executable_path could not read the executable path");
        return NULL;
    }
    length = (size_t)win_len;
    path[length] = '\0';
    return yaza_text_v1_copy_to_arena(yaza_arena_v1_current(), (const unsigned char *)path, length);
#elif defined(__APPLE__)
    uint32_t path_size = (uint32_t)sizeof(path);
    if (_NSGetExecutablePath(path, &path_size) != 0) {
        yaza_rt_v1_fail("executable_path could not read the executable path");
        return NULL;
    }
    char resolved[PATH_MAX];
    const char *selected = realpath(path, resolved) == NULL ? path : resolved;
    length = strlen(selected);
    return yaza_text_v1_copy_to_arena(yaza_arena_v1_current(), (const unsigned char *)selected, length);
#elif defined(__linux__)
    ssize_t amount = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (amount <= 0 || (size_t)amount >= sizeof(path) - 1) {
        yaza_rt_v1_fail("executable_path could not read /proc/self/exe");
        return NULL;
    }
    length = (size_t)amount;
    path[length] = '\0';
    return yaza_text_v1_copy_to_arena(yaza_arena_v1_current(), (const unsigned char *)path, length);
#else
    yaza_rt_v1_fail("executable_path is not supported on this platform");
    return NULL;
#endif
}


int64_t yaza_rt_v1_unix_ms(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_REALTIME, &now) != 0 || now.tv_sec < 0 ||
        (uint64_t)now.tv_sec > ((uint64_t)INT64_MAX - (uint64_t)now.tv_nsec / 1000000ULL) / 1000ULL) {
        return 0;
    }
    return (int64_t)((uint64_t)now.tv_sec * 1000ULL + (uint64_t)now.tv_nsec / 1000000ULL);
}

int32_t yaza_rt_v1_sleep_ms(int64_t ms) {
    if (ms <= 0) {
        return 1;
    }
    struct timespec req;
    req.tv_sec = (time_t)(ms / 1000);
    req.tv_nsec = (long)((ms % 1000) * 1000000L);
    struct timespec rem = {0, 0};
    while (nanosleep(&req, &rem) == -1) {
        if (errno != EINTR) {
            return 0;
        }
        req = rem;
    }
    return 1;
}

int32_t yaza_rt_v1_yield(void) {
    (void)sched_yield();
    return 1;
}

struct YazaTaskV1 {
    pthread_t thread;
    pthread_mutex_t join_mutex;
    YazaTaskThunkV1 thunk;
    void *context;
    void *result_buffer;
    uint64_t result_size;
    YazaArenaV1 *task_arena;
    int32_t result_has_pointers;
    int32_t joined;
    int32_t started;
    struct YazaTaskV1 *registry_next;
};

typedef struct YazaRtMutexHandle {
    pthread_mutex_t native_mutex;
    struct YazaRtMutexHandle *next;
} YazaRtMutexHandle;

typedef struct YazaRtAtomicHandle {
    _Atomic int64_t value;
    struct YazaRtAtomicHandle *next;
} YazaRtAtomicHandle;

typedef struct YazaRtChannelHandle {
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    int64_t *buffer;
    int64_t capacity;
    int64_t head;
    int64_t tail;
    int64_t count;
    int32_t closed;
    struct YazaRtChannelHandle *next;
} YazaRtChannelHandle;

static pthread_mutex_t yaza_rt_v1_async_registry_lock = PTHREAD_MUTEX_INITIALIZER;
static struct YazaTaskV1 *yaza_rt_v1_task_registry_head = NULL;
static YazaRtMutexHandle *yaza_rt_v1_mutex_registry_head = NULL;
static YazaRtAtomicHandle *yaza_rt_v1_atomic_registry_head = NULL;
static YazaRtChannelHandle *yaza_rt_v1_channel_registry_head = NULL;

void *yaza_rt_v1_task_ctx_alloc(uint64_t size) {
    if (size == 0) {
        return NULL;
    }
    void *mem = yaza_rt_v1_system_calloc(1, (size_t)size);
    if (mem == NULL) {
        yaza_rt_v1_fail("out of memory allocating spawned task context");
    }
    return mem;
}

static void *yaza_rt_v1_task_worker_entry(void *opaque) {
    struct YazaTaskV1 *task = (struct YazaTaskV1 *)opaque;
    YazaArenaV1 *task_arena = yaza_arena_v1_create(yaza_rt_v1_config.process_limit_bytes);
    if (task_arena == NULL) {
        yaza_rt_v1_fail("spawned task failed to allocate isolated task arena");
        return NULL;
    }
    YazaArenaContextV1 task_context = {0};
    if (!yaza_arena_v1_scope_begin(task_arena, &task_context)) {
        (void)yaza_arena_v1_destroy(task_arena);
        yaza_rt_v1_fail("spawned task failed to enter isolated task arena scope");
        return NULL;
    }
    task->thunk(task->context, task->result_buffer);
    if (!task->result_has_pointers) {
        (void)yaza_arena_v1_scope_end(&task_context);
        (void)yaza_arena_v1_destroy(task_arena);
        task->task_arena = NULL;
    } else {
        /* Detach thread-local context while keeping chunks alive for returned references. */
        YazaArenaV1 *root = task_arena->root;
        yaza_rt_v1_tree_lock(root);
        if (task_arena->active_leases > 0) task_arena->active_leases -= 1;
        if (task_arena->active_contexts > 0) task_arena->active_contexts -= 1;
        task_arena->scope_active = 0;
        task_arena->scope_context = NULL;
        yaza_rt_v1_current_arena = task_context.previous;
        yaza_rt_v1_current_context = task_context.previous_context;
        yaza_rt_v1_tree_unlock(root);
        task->task_arena = task_arena;
    }
    return NULL;
}

YazaTaskV1 *yaza_rt_v1_task_spawn(YazaTaskThunkV1 thunk, void *context, uint64_t result_size, int32_t result_has_pointers) {
    if (thunk == NULL) {
        yaza_rt_v1_fail("task_spawn requires a non-null task entry function");
        return NULL;
    }
    yaza_rt_v1_init();
    YazaTaskV1 *task = (YazaTaskV1 *)yaza_rt_v1_system_calloc(1, sizeof(YazaTaskV1));
    if (task == NULL) {
        free(context);
        yaza_rt_v1_fail("out of memory allocating task handle");
        return NULL;
    }
    if (pthread_mutex_init(&task->join_mutex, NULL) != 0) {
        free(context);
        free(task);
        yaza_rt_v1_fail("failed to initialize task synchronization mutex");
        return NULL;
    }
    if (result_size > 0) {
        task->result_buffer = yaza_rt_v1_system_calloc(1, (size_t)result_size);
        if (task->result_buffer == NULL) {
            (void)pthread_mutex_destroy(&task->join_mutex);
            free(context);
            free(task);
            yaza_rt_v1_fail("out of memory allocating task result buffer");
            return NULL;
        }
    }
    task->thunk = thunk;
    task->context = context;
    task->result_size = result_size;
    task->result_has_pointers = result_has_pointers;
    task->started = 1;

    const int status = pthread_create(&task->thread, NULL, yaza_rt_v1_task_worker_entry, task);
    if (status != 0) {
        (void)pthread_mutex_destroy(&task->join_mutex);
        free(task->result_buffer);
        free(context);
        free(task);
        yaza_rt_v1_fail("failed to spawn worker thread for task");
        return NULL;
    }

    pthread_mutex_lock(&yaza_rt_v1_async_registry_lock);
    task->registry_next = yaza_rt_v1_task_registry_head;
    yaza_rt_v1_task_registry_head = task;
    pthread_mutex_unlock(&yaza_rt_v1_async_registry_lock);
    return task;
}

void yaza_rt_v1_task_await(YazaTaskV1 *task, void *out_result, uint64_t result_size) {
    if (task == NULL) {
        yaza_rt_v1_fail("await called on null task handle");
        return;
    }
    pthread_mutex_lock(&task->join_mutex);
    if (task->started && !task->joined) {
        const int status = pthread_join(task->thread, NULL);
        if (status != 0) {
            pthread_mutex_unlock(&task->join_mutex);
            yaza_rt_v1_fail("await failed joining worker thread");
            return;
        }
        task->joined = 1;
        if (task->context != NULL) {
            free(task->context);
            task->context = NULL;
        }
    }
    if (result_size > 0 && out_result != NULL && task->result_buffer != NULL) {
        const uint64_t copy_bytes = result_size < task->result_size ? result_size : task->result_size;
        memcpy(out_result, task->result_buffer, (size_t)copy_bytes);
    }
    pthread_mutex_unlock(&task->join_mutex);
}

int64_t yaza_rt_v1_mutex_handle_create(void) {
    yaza_rt_v1_init();
    YazaRtMutexHandle *handle = (YazaRtMutexHandle *)yaza_rt_v1_system_calloc(1, sizeof(YazaRtMutexHandle));
    if (handle == NULL) {
        yaza_rt_v1_fail("out of memory creating Mutex");
        return 0;
    }
    if (pthread_mutex_init(&handle->native_mutex, NULL) != 0) {
        free(handle);
        yaza_rt_v1_fail("failed to initialize Mutex");
        return 0;
    }
    pthread_mutex_lock(&yaza_rt_v1_async_registry_lock);
    handle->next = yaza_rt_v1_mutex_registry_head;
    yaza_rt_v1_mutex_registry_head = handle;
    pthread_mutex_unlock(&yaza_rt_v1_async_registry_lock);
    return (int64_t)(intptr_t)handle;
}

int32_t yaza_rt_v1_mutex_handle_lock(int64_t handle_val) {
    if (handle_val == 0) return 0;
    YazaRtMutexHandle *handle = (YazaRtMutexHandle *)(intptr_t)handle_val;
    return pthread_mutex_lock(&handle->native_mutex) == 0 ? 1 : 0;
}

int32_t yaza_rt_v1_mutex_handle_unlock(int64_t handle_val) {
    if (handle_val == 0) return 0;
    YazaRtMutexHandle *handle = (YazaRtMutexHandle *)(intptr_t)handle_val;
    return pthread_mutex_unlock(&handle->native_mutex) == 0 ? 1 : 0;
}

int32_t yaza_rt_v1_mutex_handle_destroy(int64_t handle_val) {
    if (handle_val == 0) return 0;
    YazaRtMutexHandle *target = (YazaRtMutexHandle *)(intptr_t)handle_val;
    pthread_mutex_lock(&yaza_rt_v1_async_registry_lock);
    YazaRtMutexHandle **link = &yaza_rt_v1_mutex_registry_head;
    int found = 0;
    while (*link != NULL) {
        if (*link == target) {
            *link = target->next;
            found = 1;
            break;
        }
        link = &(*link)->next;
    }
    pthread_mutex_unlock(&yaza_rt_v1_async_registry_lock);
    if (!found) return 0;
    (void)pthread_mutex_destroy(&target->native_mutex);
    free(target);
    return 1;
}

int64_t yaza_rt_v1_atomic_handle_create(int64_t initial) {
    yaza_rt_v1_init();
    YazaRtAtomicHandle *handle = (YazaRtAtomicHandle *)yaza_rt_v1_system_calloc(1, sizeof(YazaRtAtomicHandle));
    if (handle == NULL) {
        yaza_rt_v1_fail("out of memory creating AtomicNumber");
        return 0;
    }
    atomic_init(&handle->value, initial);
    pthread_mutex_lock(&yaza_rt_v1_async_registry_lock);
    handle->next = yaza_rt_v1_atomic_registry_head;
    yaza_rt_v1_atomic_registry_head = handle;
    pthread_mutex_unlock(&yaza_rt_v1_async_registry_lock);
    return (int64_t)(intptr_t)handle;
}

int64_t yaza_rt_v1_atomic_handle_load(int64_t handle_val) {
    if (handle_val == 0) return 0;
    YazaRtAtomicHandle *handle = (YazaRtAtomicHandle *)(intptr_t)handle_val;
    return atomic_load_explicit(&handle->value, memory_order_seq_cst);
}

int32_t yaza_rt_v1_atomic_handle_store(int64_t handle_val, int64_t value) {
    if (handle_val == 0) return 0;
    YazaRtAtomicHandle *handle = (YazaRtAtomicHandle *)(intptr_t)handle_val;
    atomic_store_explicit(&handle->value, value, memory_order_seq_cst);
    return 1;
}

int64_t yaza_rt_v1_atomic_handle_add(int64_t handle_val, int64_t delta) {
    if (handle_val == 0) return 0;
    YazaRtAtomicHandle *handle = (YazaRtAtomicHandle *)(intptr_t)handle_val;
    const uint64_t prev = (uint64_t)atomic_fetch_add_explicit(&handle->value, delta, memory_order_seq_cst);
    return (int64_t)(prev + (uint64_t)delta);
}

int64_t yaza_rt_v1_channel_create(int64_t capacity) {
    yaza_rt_v1_init();
    int64_t effective_capacity = capacity > 0 ? capacity : 1;
    if ((uint64_t)effective_capacity > 1048576ULL) {
        effective_capacity = 1048576;
    }
    YazaRtChannelHandle *ch = (YazaRtChannelHandle *)yaza_rt_v1_system_calloc(1, sizeof(YazaRtChannelHandle));
    if (ch == NULL) {
        yaza_rt_v1_fail("out of memory creating AsyncChannel");
        return 0;
    }
    ch->buffer = (int64_t *)yaza_rt_v1_system_calloc((size_t)effective_capacity, sizeof(int64_t));
    if (ch->buffer == NULL) {
        free(ch);
        yaza_rt_v1_fail("out of memory creating AsyncChannel buffer");
        return 0;
    }
    if (pthread_mutex_init(&ch->lock, NULL) != 0) {
        free(ch->buffer);
        free(ch);
        yaza_rt_v1_fail("failed to initialize AsyncChannel mutex");
        return 0;
    }
    if (pthread_cond_init(&ch->not_empty, NULL) != 0) {
        (void)pthread_mutex_destroy(&ch->lock);
        free(ch->buffer);
        free(ch);
        yaza_rt_v1_fail("failed to initialize AsyncChannel condition");
        return 0;
    }
    if (pthread_cond_init(&ch->not_full, NULL) != 0) {
        (void)pthread_cond_destroy(&ch->not_empty);
        (void)pthread_mutex_destroy(&ch->lock);
        free(ch->buffer);
        free(ch);
        yaza_rt_v1_fail("failed to initialize AsyncChannel condition");
        return 0;
    }
    ch->capacity = effective_capacity;
    pthread_mutex_lock(&yaza_rt_v1_async_registry_lock);
    ch->next = yaza_rt_v1_channel_registry_head;
    yaza_rt_v1_channel_registry_head = ch;
    pthread_mutex_unlock(&yaza_rt_v1_async_registry_lock);
    return (int64_t)(intptr_t)ch;
}

int32_t yaza_rt_v1_channel_send(int64_t handle_val, int64_t value) {
    if (handle_val == 0) return 0;
    YazaRtChannelHandle *ch = (YazaRtChannelHandle *)(intptr_t)handle_val;
    pthread_mutex_lock(&ch->lock);
    while (ch->count == ch->capacity && !ch->closed) {
        (void)pthread_cond_wait(&ch->not_full, &ch->lock);
    }
    if (ch->closed) {
        pthread_mutex_unlock(&ch->lock);
        return 0;
    }
    ch->buffer[ch->tail] = value;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count += 1;
    (void)pthread_cond_signal(&ch->not_empty);
    pthread_mutex_unlock(&ch->lock);
    return 1;
}

int32_t yaza_rt_v1_channel_recv(int64_t handle_val, int64_t *out_value) {
    if (handle_val == 0 || out_value == NULL) return 0;
    YazaRtChannelHandle *ch = (YazaRtChannelHandle *)(intptr_t)handle_val;
    pthread_mutex_lock(&ch->lock);
    while (ch->count == 0 && !ch->closed) {
        (void)pthread_cond_wait(&ch->not_empty, &ch->lock);
    }
    if (ch->count == 0 && ch->closed) {
        pthread_mutex_unlock(&ch->lock);
        return 0;
    }
    *out_value = ch->buffer[ch->head];
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count -= 1;
    (void)pthread_cond_signal(&ch->not_full);
    pthread_mutex_unlock(&ch->lock);
    return 1;
}

int32_t yaza_rt_v1_channel_close(int64_t handle_val) {
    if (handle_val == 0) return 0;
    YazaRtChannelHandle *ch = (YazaRtChannelHandle *)(intptr_t)handle_val;
    pthread_mutex_lock(&ch->lock);
    ch->closed = 1;
    (void)pthread_cond_broadcast(&ch->not_empty);
    (void)pthread_cond_broadcast(&ch->not_full);
    pthread_mutex_unlock(&ch->lock);
    return 1;
}

int32_t yaza_rt_v1_channel_is_full(int64_t handle_val) {
    if (handle_val == 0) return 0;
    YazaRtChannelHandle *ch = (YazaRtChannelHandle *)(intptr_t)handle_val;
    pthread_mutex_lock(&ch->lock);
    const int32_t full = ch->count >= ch->capacity ? 1 : 0;
    pthread_mutex_unlock(&ch->lock);
    return full;
}

int32_t yaza_rt_v1_channel_is_closed(int64_t handle_val) {
    if (handle_val == 0) return 1;
    YazaRtChannelHandle *ch = (YazaRtChannelHandle *)(intptr_t)handle_val;
    pthread_mutex_lock(&ch->lock);
    const int32_t closed = ch->closed;
    pthread_mutex_unlock(&ch->lock);
    return closed;
}

static void yaza_rt_v1_cleanup_async_resources(void) {
    pthread_mutex_lock(&yaza_rt_v1_async_registry_lock);
    struct YazaTaskV1 *task = yaza_rt_v1_task_registry_head;
    yaza_rt_v1_task_registry_head = NULL;
    YazaRtMutexHandle *mutex = yaza_rt_v1_mutex_registry_head;
    yaza_rt_v1_mutex_registry_head = NULL;
    YazaRtAtomicHandle *atomic_handle = yaza_rt_v1_atomic_registry_head;
    yaza_rt_v1_atomic_registry_head = NULL;
    YazaRtChannelHandle *channel = yaza_rt_v1_channel_registry_head;
    yaza_rt_v1_channel_registry_head = NULL;
    pthread_mutex_unlock(&yaza_rt_v1_async_registry_lock);

    while (task != NULL) {
        struct YazaTaskV1 *next = task->registry_next;
        if (task->started && !task->joined) {
            (void)pthread_join(task->thread, NULL);
            task->joined = 1;
        }
        if (task->task_arena != NULL) {
            (void)yaza_arena_v1_destroy(task->task_arena);
            task->task_arena = NULL;
        }
        if (task->context != NULL) {
            free(task->context);
        }
        if (task->result_buffer != NULL) {
            free(task->result_buffer);
        }
        (void)pthread_mutex_destroy(&task->join_mutex);
        free(task);
        task = next;
    }
    while (mutex != NULL) {
        YazaRtMutexHandle *next = mutex->next;
        (void)pthread_mutex_destroy(&mutex->native_mutex);
        free(mutex);
        mutex = next;
    }
    while (atomic_handle != NULL) {
        YazaRtAtomicHandle *next = atomic_handle->next;
        free(atomic_handle);
        atomic_handle = next;
    }
    while (channel != NULL) {
        YazaRtChannelHandle *next = channel->next;
        (void)pthread_cond_destroy(&channel->not_empty);
        (void)pthread_cond_destroy(&channel->not_full);
        (void)pthread_mutex_destroy(&channel->lock);
        free(channel->buffer);
        free(channel);
        channel = next;
    }
}

#include "yaza_transport_curl_v1.inc"
#include "yaza_security_login_v1.inc"

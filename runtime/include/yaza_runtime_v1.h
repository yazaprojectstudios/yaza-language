#ifndef YAZA_RUNTIME_V1_H
#define YAZA_RUNTIME_V1_H

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct YazaArenaV1 YazaArenaV1;
typedef struct YazaArenaScopeV1 YazaArenaScopeV1;
typedef struct YazaArenaCycleV1 YazaArenaCycleV1;
typedef struct YazaThreadV1 YazaThreadV1;
typedef struct YazaMutexV1 YazaMutexV1;
typedef struct YazaConditionV1 YazaConditionV1;
typedef void (*YazaThreadEntryV1)(void *context);
/* Text is immutable, byte-length-aware UTF-8 with a trailing NUL for C interop. */
typedef struct YazaTextV1 {
    const unsigned char *data;
    uint64_t length;
} YazaTextV1;
typedef struct YazaBytesV1 {
    unsigned char *data;
    uint64_t length;
} YazaBytesV1;

#define YAZA_MODULE_RESULT_V1_ABI_VERSION 1U
#define YAZA_MODULE_INPUT_V1_ABI_VERSION 1U
#define YAZA_RT_V1_CONSOLE_LINE_MAX_BYTES (1ULL << 20)
#define YAZA_RT_V1_SCOPE_INLINE_STORAGE_BYTES 512U
#define YAZA_RT_V1_SCOPE_INLINE_STORAGE_ALIGN 16U
typedef enum YazaModuleInputKindV1 {
    YAZA_MODULE_INPUT_V1_TEXT = 1,
    YAZA_MODULE_INPUT_V1_BYTES = 2
} YazaModuleInputKindV1;

/* One typed host value for a generated request-module entrypoint. */
typedef struct YazaModuleInputV1 {
    uint32_t abi_version;
    uint32_t struct_size;
    uint32_t kind;
    uint32_t reserved;
    const unsigned char *data;
    uint64_t length;
} YazaModuleInputV1;

typedef enum YazaModuleResultStatusV1 {
    YAZA_MODULE_RESULT_V1_OK = 0,
    YAZA_MODULE_RESULT_V1_APPLICATION_ERROR = 1,
    YAZA_MODULE_RESULT_V1_INVALID_INPUT = 2,
    YAZA_MODULE_RESULT_V1_NO_REQUEST_SCOPE = 3,
    YAZA_MODULE_RESULT_V1_RESOURCE_LIMIT = 4
} YazaModuleResultStatusV1;

/* Output from a generated request-module entrypoint. data remains valid only
   until the active reusable request cycle ends. Initialize abi_version and
   struct_size before calling a generated entrypoint. */
typedef struct YazaModuleResultV1 {
    uint32_t abi_version;
    uint32_t struct_size;
    int32_t status;
    uint32_t reserved;
    const unsigned char *data;
    uint64_t length;
} YazaModuleResultV1;

/* Module input adapters copy caller-owned buffers into the active reusable
   request arena. They return YazaModuleResultStatusV1 values and never abort
   for invalid, oversized, out-of-scope, or quota-exhausted input. */
int32_t yaza_rt_v1_module_text_copy(const YazaTextV1 *source, YazaTextV1 **out_text);
int32_t yaza_rt_v1_module_bytes_copy(const YazaBytesV1 *source, YazaBytesV1 *out_bytes);
/* Versioned argument-array adapters for generated Text/Bytes request functions. */
int32_t yaza_rt_v1_module_text_input_copy(const YazaModuleInputV1 *source, YazaTextV1 **out_text);
int32_t yaza_rt_v1_module_bytes_input_copy(const YazaModuleInputV1 *source, YazaBytesV1 *out_bytes);

/* Counts/byte limits are frozen at runtime init; struct_size must cover this ABI. */
typedef struct YazaRuntimeConfigV1 {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t process_limit_bytes;
    uint64_t preferred_chunk_bytes;
    uint64_t max_live_scopes;
    uint64_t max_list_items;
    uint64_t max_list_vector_bytes;
} YazaRuntimeConfigV1;

#define YAZA_RUNTIME_CONFIG_V1_ABI_VERSION 1U

/* Coherent metrics captured by the most recent successful reusable-scope end. */
#define YAZA_ARENA_CYCLE_METRICS_V1_ABI_VERSION 1U
typedef struct YazaArenaCycleMetricsV1 {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t generation;
    uint64_t successful_allocations;
    uint64_t allocation_failures;
    uint64_t peak_used_bytes;
    uint64_t peak_storage_used_bytes;
    uint64_t peak_reserved_bytes;
    uint64_t retained_reserved_bytes;
} YazaArenaCycleMetricsV1;

typedef struct YazaArenaContextV1 {
    YazaArenaV1 *previous;
    struct YazaArenaContextV1 *previous_context;
    YazaArenaV1 *entered;
    uint64_t generation;
    int active;
} YazaArenaContextV1;

/* Internal borrowed view for generated for-each and indexed-loop reads; zero-initialize before begin. */
typedef struct YazaListBorrowV1 {
    void *list;
    YazaArenaV1 *owner;
    const void *items;
    const void *mutation_generation_address;
    uint64_t length;
    uint64_t element_size;
    uint64_t storage_generation;
    uint64_t mutation_generation;
} YazaListBorrowV1;

/* Internal exclusive view for compiler-proven local mutable Number Lists.
   The runtime lock and arena lease stay held until mut_end. */
typedef struct YazaListMutableBorrowV1 {
    void *list;
    void *items;
    uint64_t length;
} YazaListMutableBorrowV1;

/* Internal bounded snapshot for read-only List[List[Number]] hot paths. */
#define YAZA_LIST_V1_NESTED_READ_VIEW_MAX 32ULL
typedef struct YazaListNestedReadBorrowV1 {
    YazaListBorrowV1 outer;
    YazaListBorrowV1 children[YAZA_LIST_V1_NESTED_READ_VIEW_MAX];
    uint64_t child_index_by_outer[YAZA_LIST_V1_NESTED_READ_VIEW_MAX];
    uint64_t child_count;
    int32_t active;
    const char *inner_type;
    uint64_t inner_element_size;
} YazaListNestedReadBorrowV1;

/* Internal bounded exclusive view for a List[List[Number]] hot loop.
   The runtime holds the outer List and each distinct child List until end. */
#define YAZA_LIST_V1_NESTED_MUT_VIEW_MAX 32ULL
typedef struct YazaListNestedMutableBorrowV1 {
    YazaListMutableBorrowV1 outer;
    YazaListMutableBorrowV1 children[YAZA_LIST_V1_NESTED_MUT_VIEW_MAX];
    void *items_by_outer[YAZA_LIST_V1_NESTED_MUT_VIEW_MAX];
    uint64_t lengths_by_outer[YAZA_LIST_V1_NESTED_MUT_VIEW_MAX];
    uint64_t child_count;
    int32_t active;
} YazaListNestedMutableBorrowV1;

/* ABI v1: process-lifetime allocations owned by the YAZA program arena. */
/* Call before any other runtime operation; explicit C config overrides YAZA_RT_V1_* env vars. */
int yaza_rt_v1_configure(const YazaRuntimeConfigV1 *config);
int yaza_rt_v1_get_config(YazaRuntimeConfigV1 *out_config);
void yaza_rt_v1_init(void);
/* Called by generated main once before user code or worker creation. */
void yaza_rt_v1_set_args(int32_t argc, char **argv);
int64_t yaza_rt_v1_arg_count(void);
YazaTextV1 *yaza_rt_v1_arg(int64_t index);
/* Reads one UTF-8 console line, returning NULL at clean EOF. Flushes stdout before blocking. */
YazaTextV1 *yaza_rt_v1_console_read_line(void);
YazaTextV1 *yaza_rt_v1_env_get(const YazaTextV1 *name);
int64_t yaza_rt_v1_env_set(const YazaTextV1 *name, const YazaTextV1 *value);
YazaTextV1 *yaza_rt_v1_os_platform(void);
YazaTextV1 *yaza_rt_v1_os_arch(void);
int32_t yaza_rt_v1_file_exists(const YazaTextV1 *path);
int32_t yaza_rt_v1_is_directory(const YazaTextV1 *path);
YazaTextV1 *yaza_rt_v1_file_read(const YazaTextV1 *path);
int64_t yaza_rt_v1_file_write(const YazaTextV1 *path, const YazaTextV1 *contents);
int64_t yaza_rt_v1_file_remove(const YazaTextV1 *path);
int64_t yaza_rt_v1_directory_create(const YazaTextV1 *path);
void *yaza_rt_v1_directory_list(const YazaTextV1 *path);
YazaTextV1 *yaza_rt_v1_temp_directory(void);
int64_t yaza_rt_v1_test_temp_directory_remove(const YazaTextV1 *path);
/* Capability-rooted synchronous VFS ABI v1. Handles are process-local Number tokens. */
#define YAZA_VFS_V1_ABI_VERSION 1U
#define YAZA_VFS_V1_MAX_HANDLES 256U
#define YAZA_VFS_V1_MAX_NAME_BYTES 255U
#define YAZA_VFS_V1_MAX_TRANSFER_BYTES (16ULL * 1024ULL * 1024ULL)
#define YAZA_VFS_V1_MAX_FILE_BYTES (1ULL << 40)
#define YAZA_VFS_V1_MAX_TEMPORARY_BYTES (64ULL * 1024ULL * 1024ULL)
int32_t yaza_vfs_v1_root_open(const YazaTextV1 *path, int64_t *out_handle, char **out_error);
int32_t yaza_vfs_v1_open_read(int64_t root, const YazaTextV1 *name, int64_t *out_handle, char **out_error);
int32_t yaza_vfs_v1_open_read_public(int64_t root, const YazaTextV1 *name, int64_t *out_handle, char **out_error);
int32_t yaza_vfs_v1_open_rw(int64_t root, const YazaTextV1 *name, int64_t *out_handle, char **out_error);
int32_t yaza_vfs_v1_create_exclusive(int64_t root, const YazaTextV1 *name, int64_t *out_handle, char **out_error);
/* Returns bounded root-child names as NUL-separated bytes; the final name is also NUL-terminated. */
int32_t yaza_vfs_v1_list_names(int64_t root, YazaBytesV1 *out_bytes, char **out_error);
int32_t yaza_vfs_v1_lock_exclusive(int64_t root, const YazaTextV1 *name, int64_t *out_handle, char **out_error);
int32_t yaza_vfs_v1_file_size(int64_t file, int64_t *out_size, char **out_error);
int32_t yaza_vfs_v1_read_exact(int64_t file, int64_t offset, int64_t length, YazaBytesV1 *out_bytes, char **out_error);
int32_t yaza_vfs_v1_write_exact(int64_t file, int64_t offset, const YazaBytesV1 *bytes, int64_t *out_written, char **out_error);
int32_t yaza_vfs_v1_truncate(int64_t file, int64_t size, char **out_error);
int32_t yaza_vfs_v1_preallocate(int64_t file, int64_t minimum_size, char **out_error);
int32_t yaza_vfs_v1_sync(int64_t handle, char **out_error);
int32_t yaza_vfs_v1_sync_directory(int64_t root, char **out_error);
int32_t yaza_vfs_v1_remove(int64_t root, const YazaTextV1 *name, char **out_error);
int32_t yaza_vfs_v1_rename(int64_t root, const YazaTextV1 *old_name, const YazaTextV1 *new_name, char **out_error);
int32_t yaza_vfs_v1_close(int64_t handle, char **out_error);
#define YAZA_NET_V1_MAX_HANDLES 256U
#define YAZA_NET_V1_MAX_TRANSFER_BYTES (16ULL * 1024ULL * 1024ULL)
int32_t yaza_net_v1_listen(const YazaTextV1 *host, int64_t port, int64_t backlog, int64_t *out_handle, char **out_error);
int32_t yaza_net_v1_connect(const YazaTextV1 *host, int64_t port, int64_t timeout_ms, int64_t *out_handle, char **out_error);
int32_t yaza_net_v1_accept(int64_t listener, int64_t timeout_ms, int64_t *out_handle, char **out_error);
int32_t yaza_net_v1_read(int64_t connection, int64_t max_bytes, int64_t timeout_ms, YazaBytesV1 *out_bytes, char **out_error);
int32_t yaza_net_v1_write(int64_t connection, const YazaBytesV1 *bytes, int64_t timeout_ms, int64_t *out_written, char **out_error);
/* HTTPS only; result starts with a two-byte big-endian HTTP status. */
int32_t yaza_net_v1_https_request(const YazaTextV1 *method, const YazaTextV1 *url,
    const YazaBytesV1 *body, const YazaTextV1 *bearer_token,
    int64_t timeout_ms, int64_t max_response_bytes, YazaBytesV1 *out_wire, char **out_error);
/* Fixed Gmail/Brevo endpoints; STARTTLS and certificate verification required. */
/* Process-local, bounded username-hash login limiter. Returns zero when denied. */
int32_t yaza_security_v1_login_attempt(const YazaTextV1 *key_hex, int64_t now_ms, int32_t reset);
int32_t yaza_net_v1_smtp_secure_send(const YazaTextV1 *provider, const YazaTextV1 *auth_mode,
    const YazaTextV1 *username, const YazaTextV1 *credential,
    const YazaTextV1 *from_address, const YazaTextV1 *to_address,
    const YazaTextV1 *message, int64_t timeout_ms, char **out_error);
int32_t yaza_net_v1_close(int64_t handle, char **out_error);
int32_t yaza_net_v1_pair_create(int64_t *out_handle, char **out_error);
int32_t yaza_net_v1_pair_peer(int64_t handle, int64_t *out_peer_handle, char **out_error);
void yaza_crypto_v1_sha256(const unsigned char *data, uint64_t length, YazaBytesV1 *out_bytes);
void yaza_crypto_v1_hmac_sha256(const unsigned char *key, uint64_t key_length, const unsigned char *message, uint64_t message_length, YazaBytesV1 *out_bytes);
int32_t yaza_crypto_v1_pbkdf2_sha256(const unsigned char *password, uint64_t password_length, const unsigned char *salt, uint64_t salt_length, int64_t iterations, int64_t key_length, YazaBytesV1 *out_bytes, char **out_error);
int32_t yaza_crypto_v1_argon2id_hash(const unsigned char *password, uint64_t password_length, const unsigned char *salt, uint64_t salt_length, YazaTextV1 **out_text, char **out_error);
int32_t yaza_crypto_v1_argon2id_verify(const unsigned char *password, uint64_t password_length, const YazaTextV1 *encoded_hash);
int32_t yaza_crypto_v1_argon2id_hash_format_is_current(const YazaTextV1 *encoded_hash);
#if defined(YAZA_VFS_V1_TESTING)
#define YAZA_VFS_V1_TEST_FAIL_OPEN (1U << 0)
#define YAZA_VFS_V1_TEST_FAIL_OPENAT (1U << 1)
#define YAZA_VFS_V1_TEST_FAIL_FSTAT (1U << 2)
#define YAZA_VFS_V1_TEST_FAIL_FSYNC (1U << 3)
#define YAZA_VFS_V1_TEST_FAIL_FTRUNCATE (1U << 4)
#define YAZA_VFS_V1_TEST_FAIL_UNLINKAT (1U << 5)
#define YAZA_VFS_V1_TEST_FAIL_RENAMEAT (1U << 6)
#define YAZA_VFS_V1_TEST_FAIL_CLOSE (1U << 7)
#define YAZA_VFS_V1_TEST_FAIL_FLOCK (1U << 8)
#define YAZA_VFS_V1_TEST_FAIL_PREALLOCATE (1U << 9)
void yaza_vfs_v1_test_configure(uint32_t pread_eintr_count, uint32_t pwrite_eintr_count,
                                uint64_t max_io_chunk_bytes, int64_t pwrite_fail_after_bytes);
void yaza_vfs_v1_test_set_temporary_limit(uint64_t limit_bytes);
void yaza_vfs_v1_test_fail_next(uint32_t syscall_id);
/* Test-only process-crash hook: _exit(86) immediately after the selected
   successful fsync call. A zero target disables the hook and resets the count. */
void yaza_vfs_v1_test_crash_after_fsync(uint64_t successful_sync_count);
uint64_t yaza_vfs_v1_test_fsync_count(void);
#endif
int64_t yaza_rt_v1_process(const YazaTextV1 *command, void *typed_text_list);
int64_t yaza_rt_v1_monotonic_ms(void);
/* Monotonic clock in nanoseconds; zero signals a clock/overflow failure. */
int64_t yaza_rt_v1_monotonic_ns(void);
/* Wall-clock UNIX epoch time in milliseconds (CLOCK_REALTIME); zero on failure. */
int64_t yaza_rt_v1_unix_ms(void);
/* Non-busy-wait OS sleep for ms milliseconds; returns 1 on success. */
int32_t yaza_rt_v1_sleep_ms(int64_t ms);
/* Cooperative OS thread yield; returns 1. */
int32_t yaza_rt_v1_yield(void);
/* Snapshot of CPUs available to this process; a scheduler sizing hint, not a guarantee. */
int64_t yaza_rt_v1_processor_count(void);

/* Language-level asynchronous task runtime (spawn / await). */
typedef void (*YazaTaskThunkV1)(void *context, void *out_result);
typedef struct YazaTaskV1 YazaTaskV1;
void *yaza_rt_v1_task_ctx_alloc(uint64_t size);
YazaTaskV1 *yaza_rt_v1_task_spawn(YazaTaskThunkV1 thunk, void *context, uint64_t result_size, int32_t result_has_pointers);
void yaza_rt_v1_task_await(YazaTaskV1 *task, void *out_result, uint64_t result_size);

/* YAZA handle-based synchronization and bounded channel primitives. */
int64_t yaza_rt_v1_mutex_handle_create(void);
int32_t yaza_rt_v1_mutex_handle_lock(int64_t handle);
int32_t yaza_rt_v1_mutex_handle_unlock(int64_t handle);
int32_t yaza_rt_v1_mutex_handle_destroy(int64_t handle);

int64_t yaza_rt_v1_atomic_handle_create(int64_t initial);
int64_t yaza_rt_v1_atomic_handle_load(int64_t handle);
int32_t yaza_rt_v1_atomic_handle_store(int64_t handle, int64_t value);
int64_t yaza_rt_v1_atomic_handle_add(int64_t handle, int64_t delta);

int64_t yaza_rt_v1_channel_create(int64_t capacity);
int32_t yaza_rt_v1_channel_send(int64_t handle, int64_t value);
int32_t yaza_rt_v1_channel_recv(int64_t handle, int64_t *out_value);
int32_t yaza_rt_v1_channel_close(int64_t handle);
int32_t yaza_rt_v1_channel_is_full(int64_t handle);
int32_t yaza_rt_v1_channel_is_closed(int64_t handle);
/* POSIX thread primitive: spawn returns 0 on success or a pthread error code.
   join blocks, consumes the handle only on success, and returns 0 or an error.
   The context remains caller-owned and must stay valid until successful join.
   New threads automatically establish an isolated task arena scope reclaimed on
   exit, preventing thread-local allocations from overwriting or pinning the
   process arena. Cancellation is not provided. */
int32_t yaza_rt_v1_thread_spawn(YazaThreadEntryV1 entry, void *context, YazaThreadV1 **out_thread);
int32_t yaza_rt_v1_thread_join(YazaThreadV1 *thread);
/* Returns the active task/thread-local arena if one is active, or NULL. */
YazaArenaV1 *yaza_rt_v1_task_arena_handle(void);
/* POSIX synchronization handles use system-heap storage. Do not destroy them
   while locked, in use, or referenced by waiters. Condition waits can wake
   spuriously; callers must retest their predicate after every successful wait. */
int32_t yaza_rt_v1_mutex_create(YazaMutexV1 **out_mutex);
int32_t yaza_rt_v1_mutex_destroy(YazaMutexV1 *mutex);
int32_t yaza_rt_v1_mutex_lock(YazaMutexV1 *mutex);
int32_t yaza_rt_v1_mutex_unlock(YazaMutexV1 *mutex);
int32_t yaza_rt_v1_condition_create(YazaConditionV1 **out_condition);
int32_t yaza_rt_v1_condition_destroy(YazaConditionV1 *condition);
int32_t yaza_rt_v1_condition_wait(YazaConditionV1 *condition, YazaMutexV1 *mutex);
int32_t yaza_rt_v1_condition_signal(YazaConditionV1 *condition);
int32_t yaza_rt_v1_condition_broadcast(YazaConditionV1 *condition);
YazaTextV1 *yaza_rt_v1_executable_path(void);
YazaArenaV1 *yaza_rt_v1_program_arena_handle(void);
void *yaza_rt_v1_alloc(uint64_t size);
/* Fallible allocation for APIs that can propagate resource exhaustion. */
void *yaza_rt_v1_try_alloc(uint64_t size);
/* Copy a C string into the current arena, returning NULL on allocation failure. */
YazaTextV1 *yaza_rt_v1_try_text_from_cstr(const char *value);
/* Static, allocation-free error value for fallible Text construction. */
const YazaTextV1 *yaza_rt_v1_text_allocation_error(void);
/* Static, allocation-free error value for fallible List construction. */
const YazaTextV1 *yaza_rt_v1_list_allocation_error(void);
/* Process used/storage are root-shard sums: exact after quiescence, mixed snapshots during updates. */
uint64_t yaza_rt_v1_arena_used(void);
/* Payload bytes and bump-space bytes differ because bump-space includes alignment padding. */
uint64_t yaza_rt_v1_arena_storage_used(void);
uint64_t yaza_rt_v1_arena_reserved(void);
/* Process peaks are conservative sums of shard peaks, not exact simultaneous peaks. */
uint64_t yaza_rt_v1_arena_peak_used(void);
uint64_t yaza_rt_v1_arena_peak_storage_used(void);
/* Reserved and peak_reserved remain exact process-wide quota counters. */
uint64_t yaza_rt_v1_arena_peak_reserved(void);
uint64_t yaza_rt_v1_arena_scope_count(void);
/* Cumulative successful allocations; exact after worker quiescence, sampled across shards during concurrency. */
uint64_t yaza_rt_v1_arena_allocation_count(void);
uint64_t yaza_rt_v1_arena_allocation_failures(void);
YazaTextV1 *yaza_rt_v1_text_from_cstr(const char *text);
/* Returns NULL if the combined descriptor and payload cannot fit in the active arena. */
YazaTextV1 *yaza_rt_v1_try_text_concat(const YazaTextV1 *left, const YazaTextV1 *right);
YazaTextV1 *yaza_rt_v1_text_concat(const YazaTextV1 *left, const YazaTextV1 *right);
/* Status: 1 success, 0 arena/allocation failure, -1 negative count, -2 output size overflow. */
int32_t yaza_rt_v1_try_text_repeat(const YazaTextV1 *text, int64_t count, YazaTextV1 **out_text);
const YazaTextV1 *yaza_rt_v1_text_repeat_count_error(void);
const YazaTextV1 *yaza_rt_v1_text_repeat_size_error(void);
int64_t yaza_rt_v1_text_length(const YazaTextV1 *text);
int64_t yaza_rt_v1_text_byte(const YazaTextV1 *text, int64_t index);
YazaTextV1 *yaza_rt_v1_text_slice(const YazaTextV1 *text, int64_t start, int64_t length);
/* Returns 1 on success, 0 on allocation failure, and -1 when the range is invalid. */
int32_t yaza_rt_v1_try_text_slice(const YazaTextV1 *text, int64_t start, int64_t length, YazaTextV1 **out_slice);
const YazaTextV1 *yaza_rt_v1_text_slice_range_error(void);
int64_t yaza_rt_v1_text_find(const YazaTextV1 *text, const YazaTextV1 *needle);
int32_t yaza_rt_v1_text_starts_with(const YazaTextV1 *text, const YazaTextV1 *prefix);
int32_t yaza_rt_v1_text_ends_with(const YazaTextV1 *text, const YazaTextV1 *suffix);
int64_t yaza_rt_v1_text_compare(const YazaTextV1 *left, const YazaTextV1 *right);
YazaTextV1 *yaza_rt_v1_text_trim(const YazaTextV1 *text);
/* Returns NULL if the trimmed Text cannot fit in the active arena. */
YazaTextV1 *yaza_rt_v1_try_text_trim(const YazaTextV1 *text);
int32_t yaza_rt_v1_say_text(const YazaTextV1 *text);
void *yaza_rt_v1_text_lines(const YazaTextV1 *text);
/* Returns 1 on success and 0 on arena or List-limit failure. */
int32_t yaza_rt_v1_try_text_lines(const YazaTextV1 *text, void **out_lines);
/* Returns 1 on success, 0 on arena or List-limit failure, and -1 for an empty delimiter. */
int32_t yaza_rt_v1_try_text_split(const YazaTextV1 *text, const YazaTextV1 *delimiter, void **out_parts);
const YazaTextV1 *yaza_rt_v1_text_split_delimiter_error(void);
YazaTextV1 *yaza_rt_v1_text_join(void *text_list, const YazaTextV1 *separator);
/* Returns NULL if the joined Text cannot fit in the active arena. */
YazaTextV1 *yaza_rt_v1_try_text_join(void *text_list, const YazaTextV1 *separator);
int32_t yaza_rt_v1_say_number(int64_t value);
int32_t yaza_rt_v1_say_int32(int32_t value);
YazaTextV1 *yaza_rt_v1_number_text(int64_t value);
YazaTextV1 *yaza_rt_v1_int32_text(int32_t value);
/* Recoverable formatting variants return NULL only when active-arena allocation fails. */
YazaTextV1 *yaza_rt_v1_try_number_text(int64_t value);
YazaTextV1 *yaza_rt_v1_try_int32_text(int32_t value);
int64_t yaza_rt_v1_number_parse(const YazaTextV1 *text);
/* Recoverable parsers return 1 on success and 0 for invalid/out-of-range text. */
int32_t yaza_rt_v1_try_number_parse(const YazaTextV1 *text, int64_t *out_value);
int32_t yaza_rt_v1_try_int32_parse(const YazaTextV1 *text, int32_t *out_value);
int64_t yaza_rt_v1_fail(const char *message);
int64_t yaza_rt_v1_assert_fail(const char *message);
int64_t yaza_rt_v1_fail_text(const YazaTextV1 *message);
int64_t yaza_rt_v1_assert_fail_text(const YazaTextV1 *message);
int64_t yaza_rt_v1_exit(int64_t status);
double yaza_rt_v1_float64_parse(const YazaTextV1 *text);
int32_t yaza_rt_v1_try_float64_parse(const YazaTextV1 *text, double *out_value);
YazaTextV1 *yaza_rt_v1_float64_text(double value);
YazaTextV1 *yaza_rt_v1_try_float64_text(double value);
void *yaza_text_v1_split(const YazaTextV1 *text, const YazaTextV1 *delimiter);

/* Arena-owned Bytes ABI. Data/length are passed separately to avoid C aggregate ABI differences. */
void yaza_bytes_v1_from_text(const YazaTextV1 *text, YazaBytesV1 *out_bytes);
int32_t yaza_bytes_v1_try_from_text(const YazaTextV1 *text, YazaBytesV1 *out_bytes, char **out_error);
int32_t yaza_bytes_v1_try_new_zeroed(int64_t length, YazaBytesV1 *out_bytes, char **out_error);
void yaza_bytes_v1_from_number(int64_t value, YazaBytesV1 *out_bytes);
int32_t yaza_bytes_v1_try_from_number(int64_t value, YazaBytesV1 *out_bytes, char **out_error);
void yaza_bytes_v1_secure_random(int64_t requested_length, YazaBytesV1 *out_bytes);
int32_t yaza_bytes_v1_try_secure_random(int64_t requested_length, YazaBytesV1 *out_bytes, char **out_error);
void yaza_bytes_v1_slice(const unsigned char *data, uint64_t length, int64_t start, int64_t slice_length, YazaBytesV1 *out_bytes);
int32_t yaza_bytes_v1_try_slice(const unsigned char *data, uint64_t length, int64_t start, int64_t slice_length, YazaBytesV1 *out_bytes, char **out_error);
void yaza_bytes_v1_concat(const unsigned char *left_data, uint64_t left_length, const unsigned char *right_data, uint64_t right_length, YazaBytesV1 *out_bytes);
int32_t yaza_bytes_v1_try_concat(const unsigned char *left_data, uint64_t left_length, const unsigned char *right_data, uint64_t right_length, YazaBytesV1 *out_bytes, char **out_error);
int32_t yaza_bytes_v1_equal(const unsigned char *left_data, uint64_t left_length, const unsigned char *right_data, uint64_t right_length);
/* Content scan avoids data-dependent early exit only when lengths are equal and public; this is not a cryptographic primitive. */
int32_t yaza_bytes_v1_equal_constant_time(const unsigned char *left_data, uint64_t left_length, const unsigned char *right_data, uint64_t right_length);
int32_t yaza_bytes_v1_to_text_checked(const unsigned char *data, uint64_t length, YazaTextV1 **out_text);
int32_t yaza_bytes_v1_try_to_text(const unsigned char *data, uint64_t length, YazaTextV1 **out_text, char **out_error);
YazaTextV1 *yaza_bytes_v1_to_text(const unsigned char *data, uint64_t length);
int64_t yaza_bytes_v1_zeroize(unsigned char *data, uint64_t length);
/* CRC32C (Castagnoli), reflected polynomial 0x82f63b78, init/final XOR 0xffffffff. */
uint64_t yaza_bytes_v1_crc32c(const unsigned char *data, uint64_t length);
uint64_t yaza_bytes_v1_crc32c_zeroed_range(const unsigned char *data, uint64_t length, int64_t zero_offset, int64_t zero_length);
uint64_t yaza_bytes_v1_ydbv1_checksum(const unsigned char *data, uint64_t length);
uint64_t yaza_bytes_v1_ydbv1_checksum_zeroed_range(const unsigned char *data, uint64_t length, int64_t zero_offset, int64_t zero_length);
int32_t yaza_bytes_v1_from_text_bounded(const YazaTextV1 *text, int64_t maximum_bytes, YazaBytesV1 *out_bytes, char **out_error);
int32_t yaza_bytes_v1_join(void *typed_bytes_list, YazaBytesV1 *out_bytes, char **out_error);

/*
 * Thread-safety and lifetime contract:
 * - Arena metadata, allocation, and quota accounting are synchronized; concurrent
 *   allocations into a non-reusable shared arena are permitted.
 * - enter/leave contexts are per-thread, stack ordered, and never inherited by a
 *   new thread. A reusable scope_begin/end cycle is exclusive to its entering
 *   thread; same-thread nested descendant scopes may access ancestor-owned arenas
 *   and Lists, while other threads cannot allocate into or access Lists owned by
 *   that active request arena.
 * - Individual typed-List operations are synchronized. A sequence of operations
 *   is not a transaction, and arbitrary pointers returned from arena storage
 *   still require caller synchronization.
 * - reset/destroy require external quiescence. A retained lease prevents cleanup
 *   while held, but does not itself synchronize reads or writes to payload data.
 */
YazaArenaV1 *yaza_arena_v1_create(uint64_t limit_bytes);
YazaArenaV1 *yaza_arena_v1_create_child(YazaArenaV1 *parent, uint64_t limit_bytes);
void *yaza_arena_v1_alloc(YazaArenaV1 *arena, uint64_t size);
int yaza_arena_v1_retain(YazaArenaV1 *arena);
/* Releases only a retain() lease; it cannot consume a context or in-flight List-access lease. */
int yaza_arena_v1_release(YazaArenaV1 *arena);
int yaza_arena_v1_enter(YazaArenaV1 *arena, YazaArenaContextV1 *context);
int yaza_arena_v1_leave(YazaArenaContextV1 *context);
/* Reusable exclusive worker/request lifecycle. scope_end fails without leaving if children or leases remain. */
int yaza_arena_v1_scope_begin(YazaArenaV1 *arena, YazaArenaContextV1 *context);
int yaza_arena_v1_scope_end(YazaArenaContextV1 *context);
/* Host compatibility wrapper for a nested scope; allocates an opaque handle
   from the system heap. Enters a bounded child of the current arena.
   The handle is thread-affine and must be closed once on the entering thread.
   If scope_end fails, the active context remains for retry; if destruction fails
   after scope_end, the inactive handle remains valid for a destruction retry. */
YazaArenaScopeV1 *yaza_rt_v1_scope_enter(uint64_t limit_bytes);
/* Compiler-generated lexical scopes supply aligned stack storage for control metadata,
   so entering a language arena_scope does not allocate control blocks from the system heap.
   storage must be at least YAZA_RT_V1_SCOPE_INLINE_STORAGE_BYTES bytes, aligned to
   YAZA_RT_V1_SCOPE_INLINE_STORAGE_ALIGN, and remain alive until scope_leave succeeds. */
YazaArenaScopeV1 *yaza_rt_v1_scope_enter_inplace(uint64_t limit_bytes, void *storage, uint64_t storage_bytes);
int32_t yaza_rt_v1_scope_leave(YazaArenaScopeV1 *scope);
/* Opaque reusable child arena for host-controlled request/worker cycles.
   begin/end must be paired and run serially on the same thread. A failed end
   leaves the cycle active for cleanup and retry; destroy requires an inactive
   cycle and no outstanding child scopes or leases. */
YazaArenaCycleV1 *yaza_arena_v1_cycle_create(YazaArenaV1 *parent, uint64_t limit_bytes);
int32_t yaza_arena_v1_cycle_begin(YazaArenaCycleV1 *cycle);
int32_t yaza_arena_v1_cycle_end(YazaArenaCycleV1 *cycle, YazaArenaCycleMetricsV1 *out_metrics);
/* Read the last completed cycle snapshot without exposing the child arena handle.
   The cycle must remain alive for the duration of the call; concurrent reads
   are safe while begin/end run on its owning worker thread. */
int32_t yaza_arena_v1_cycle_last_metrics(YazaArenaCycleV1 *cycle, YazaArenaCycleMetricsV1 *out_metrics);
int32_t yaza_arena_v1_cycle_destroy(YazaArenaCycleV1 *cycle);
/* Read-only snapshots for the arena active on the calling thread. */
uint64_t yaza_rt_v1_current_arena_used(void);
uint64_t yaza_rt_v1_current_arena_storage_used(void);
uint64_t yaza_rt_v1_current_arena_reserved(void);
uint64_t yaza_rt_v1_current_arena_peak_used(void);
uint64_t yaza_rt_v1_current_arena_peak_storage_used(void);
uint64_t yaza_rt_v1_current_arena_peak_reserved(void);
uint64_t yaza_rt_v1_current_arena_allocation_count(void);
uint64_t yaza_rt_v1_current_arena_generation(void);
uint64_t yaza_rt_v1_live_scope_count(void);
YazaArenaV1 *yaza_arena_v1_current(void);
int yaza_arena_v1_reset(YazaArenaV1 *arena);
int yaza_arena_v1_destroy(YazaArenaV1 *arena);
uint64_t yaza_arena_v1_used(YazaArenaV1 *arena);
uint64_t yaza_arena_v1_storage_used(YazaArenaV1 *arena);
uint64_t yaza_arena_v1_reserved(YazaArenaV1 *arena);
uint64_t yaza_arena_v1_peak_used(YazaArenaV1 *arena);
uint64_t yaza_arena_v1_peak_storage_used(YazaArenaV1 *arena);
uint64_t yaza_arena_v1_peak_reserved(YazaArenaV1 *arena);
/* Last successful scope_end() snapshot, retained across the next scope_begin(). */
uint64_t yaza_arena_v1_last_cycle_generation(YazaArenaV1 *arena);
uint64_t yaza_arena_v1_last_cycle_peak_used(YazaArenaV1 *arena);
uint64_t yaza_arena_v1_last_cycle_peak_storage_used(YazaArenaV1 *arena);
uint64_t yaza_arena_v1_last_cycle_peak_reserved(YazaArenaV1 *arena);
/* Set struct_size to sizeof(*out_metrics) before calling. Returns 0 for an
   undersized output, null arena, or destroyed arena; generation == 0 means no
   reusable scope cycle has completed. */
int yaza_arena_v1_last_cycle_metrics(YazaArenaV1 *arena, YazaArenaCycleMetricsV1 *out_metrics);
uint64_t yaza_arena_v1_generation(YazaArenaV1 *arena);
uint64_t yaza_arena_v1_allocation_count(YazaArenaV1 *arena);

#if defined(YAZA_RT_V1_TESTING)
/* Fail one system allocation after the requested number of successful attempts. */
void yaza_rt_v1_test_fail_alloc_after(uint64_t successful_allocations);
void yaza_rt_v1_test_disable_alloc_failures(void);
#endif

/* Legacy scalar-list ABI retained for compatibility with early harnesses. */
void *yaza_list_v1_create(uint64_t element_type_id);
uint64_t yaza_list_v1_count(void *list, uint64_t element_type_id);
int64_t yaza_list_v1_push(void *list, uint64_t value, uint64_t element_type_id);
uint64_t yaza_list_v1_get(void *list, int64_t index, uint64_t element_type_id);
int32_t yaza_list_v1_try_get(void *list, int64_t index, uint64_t element_type_id, uint64_t *out_value);
int64_t yaza_list_v1_set(void *list, int64_t index, uint64_t value, uint64_t element_type_id);

/* Exact-signature list ABI. Text elements are copied; aggregate pointer fields are shallow copies. */
void *yaza_list_v1_create_typed(const char *collection_type, uint64_t element_size);
/* Returns NULL on invalid metadata or allocation/arena-limit failure. */
void *yaza_list_v1_try_create_typed(const char *collection_type, uint64_t element_size);
uint64_t yaza_list_v1_count_typed(void *list, const char *collection_type, uint64_t element_size);
void yaza_list_v1_iter_begin_typed(void *list, const char *collection_type, uint64_t element_size, YazaListBorrowV1 *borrow);
uint64_t yaza_list_v1_iter_count(const YazaListBorrowV1 *borrow);
int32_t yaza_list_v1_iter_get_typed(YazaListBorrowV1 *borrow, int64_t index, void *out_value);
void yaza_list_v1_iter_end(YazaListBorrowV1 *borrow);
/* Borrowed indexed reads preserve ordinary live List length and mutation semantics. */
void yaza_list_v1_read_begin_typed(void *list, const char *collection_type, uint64_t element_size, YazaListBorrowV1 *borrow);
YazaListBorrowV1 *yaza_list_v1_read_begin_or_share_typed(void *list, const char *collection_type, uint64_t element_size, YazaListBorrowV1 *parent_borrow, YazaListBorrowV1 *local_borrow);
int32_t yaza_list_v1_read_get_typed(YazaListBorrowV1 *borrow, int64_t index, void *out_value);
void yaza_list_v1_read_fail_bounds(YazaListBorrowV1 *borrow, int64_t index);
void yaza_list_v1_read_end(YazaListBorrowV1 *borrow);
void yaza_list_v1_read_release_shared(YazaListBorrowV1 *borrow, YazaListBorrowV1 *local_borrow);
/* Immutable COW snapshot handle. It pins the source arena until read_view_end(). */
YazaListBorrowV1 *yaza_list_v1_read_view_begin_typed(void *list, const char *collection_type, uint64_t element_size);
YazaListBorrowV1 *yaza_list_v1_read_view_prefix_typed(void *list, const char *collection_type, uint64_t element_size, int64_t prefix_length);
uint64_t yaza_list_v1_read_view_count(YazaListBorrowV1 *view);
void yaza_list_v1_read_view_end(YazaListBorrowV1 *view);
/* Exclusive, fixed-length mutation view. Begin detaches existing read snapshots,
   validates the exact List signature, and holds the list lock/arena lease. */
void yaza_list_v1_mut_begin_typed(void *list, const char *collection_type, uint64_t element_size, YazaListMutableBorrowV1 *borrow);
void yaza_list_v1_mut_end(YazaListMutableBorrowV1 *borrow);
int32_t yaza_list_v1_nested_read_begin_typed(void *list, const char *outer_type, uint64_t outer_element_size, const char *inner_type, uint64_t inner_element_size, YazaListNestedReadBorrowV1 *borrow);
YazaListNestedReadBorrowV1 *yaza_list_v1_nested_read_begin_or_share_typed(void *list, const char *outer_type, uint64_t outer_element_size, const char *inner_type, uint64_t inner_element_size, YazaListNestedReadBorrowV1 *parent_borrow, YazaListNestedReadBorrowV1 *local_borrow);
void yaza_list_v1_nested_read_end(YazaListNestedReadBorrowV1 *borrow);
void yaza_list_v1_nested_read_release_shared(YazaListNestedReadBorrowV1 *borrow, YazaListNestedReadBorrowV1 *local_borrow);
int32_t yaza_list_v1_nested_mut_begin_typed(void *list, const char *outer_type, uint64_t outer_element_size, const char *inner_type, uint64_t inner_element_size, YazaListNestedMutableBorrowV1 *borrow);
void yaza_list_v1_nested_mut_end(YazaListNestedMutableBorrowV1 *borrow);
int64_t yaza_list_v1_push_typed(void *list, const void *value, const char *collection_type, uint64_t element_size);
int32_t yaza_list_v1_try_push_typed(void *list, const void *value, const char *collection_type, uint64_t element_size);
/* Returns 0 without changing logical List contents when reservation is invalid or cannot allocate. */
int32_t yaza_list_v1_try_reserve_typed(void *list, int64_t requested_capacity, const char *collection_type, uint64_t element_size);
int32_t yaza_list_v1_get_typed(void *list, int64_t index, const char *collection_type, uint64_t element_size, void *out_value);
int32_t yaza_list_v1_try_get_typed(void *list, int64_t index, const char *collection_type, uint64_t element_size, void *out_value);
int64_t yaza_list_v1_set_typed(void *list, int64_t index, const void *value, const char *collection_type, uint64_t element_size);
/* Returns 0 for an invalid index or when copy-on-write/Text storage cannot be allocated. */
int32_t yaza_list_v1_try_set_typed(void *list, int64_t index, const void *value, const char *collection_type, uint64_t element_size);

#if defined(__cplusplus)
}
#endif

#endif

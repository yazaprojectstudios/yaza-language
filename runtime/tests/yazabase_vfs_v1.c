#define _POSIX_C_SOURCE 200809L
#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE
#endif

#include "yaza_runtime_v1.h"

#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(YAZA_VFS_V1_TEST_MAX_HANDLES)
#define YAZA_VFS_V1_TEST_HANDLE_LIMIT YAZA_VFS_V1_TEST_MAX_HANDLES
#else
#define YAZA_VFS_V1_TEST_HANDLE_LIMIT YAZA_VFS_V1_MAX_HANDLES
#endif

static YazaTextV1 text_view(const char *value) {
    return (YazaTextV1){(const unsigned char *)value, (uint64_t)strlen(value)};
}

static int bytes_list_contains_name(const YazaBytesV1 *listing, const char *name) {
    if (listing == NULL || (listing->length > 0 && listing->data == NULL)) return 0;
    const size_t name_length = strlen(name);
    uint64_t offset = 0;
    while (offset < listing->length) {
        uint64_t end = offset;
        while (end < listing->length && listing->data[end] != 0) end += 1;
        if ((size_t)(end - offset) == name_length && memcmp(listing->data + offset, name, name_length) == 0) return 1;
        offset = end + 1;
    }
    return 0;
}

typedef struct VfsThreadCase {
    int64_t handle;
    uint64_t lane;
} VfsThreadCase;

static void *exercise_shared_handle(void *raw_case) {
    VfsThreadCase *test_case = (VfsThreadCase *)raw_case;
    for (uint64_t iteration = 0; iteration < 128; ++iteration) {
        unsigned char payload[8] = {
            (unsigned char)test_case->lane,
            (unsigned char)iteration,
            (unsigned char)(iteration >> 8),
            0x59, 0x41, 0x5a, 0x41, 0xff
        };
        YazaBytesV1 input = {payload, sizeof(payload)};
        const int64_t offset = (int64_t)(test_case->lane * 64);
        int64_t written = -1;
        char *error = NULL;
        assert(yaza_vfs_v1_write_exact(test_case->handle, offset, &input, &written, &error) == 1);
        assert(written == (int64_t)sizeof(payload) && error == NULL);
        YazaBytesV1 output = {NULL, 0};
        assert(yaza_vfs_v1_read_exact(test_case->handle, offset, (int64_t)sizeof(payload), &output, &error) == 1);
        assert(error == NULL && output.length == sizeof(payload));
        assert(memcmp(output.data, payload, sizeof(payload)) == 0);
    }
    return NULL;
}

int main(void) {
    yaza_rt_v1_init();

    char directory_template[] = "/tmp/yaza-vfs-v1-XXXXXX";
    char *directory = mkdtemp(directory_template);
    assert(directory != NULL);
    char symlink_path[4096];
    const int path_length = snprintf(symlink_path, sizeof(symlink_path), "%s-root-link", directory);
    assert(path_length > 0 && (size_t)path_length < sizeof(symlink_path));
    assert(symlink(directory, symlink_path) == 0);
    char symlink_path_trailing[4096];
    const int trailing_path_length = snprintf(symlink_path_trailing, sizeof(symlink_path_trailing), "%s/", symlink_path);
    assert(trailing_path_length > 0 && (size_t)trailing_path_length < sizeof(symlink_path_trailing));

    YazaTextV1 root_path = text_view(directory);
    YazaTextV1 root_link = text_view(symlink_path);
    YazaTextV1 root_link_trailing = text_view(symlink_path_trailing);
    char *error = NULL;
    int64_t root = 0;
    assert(yaza_vfs_v1_root_open(&root_link, &root, &error) == 0);
    assert(root == 0 && error != NULL);
    assert(yaza_vfs_v1_root_open(&root_link_trailing, &root, &error) == 0);
    assert(root == 0 && error != NULL);
    yaza_vfs_v1_test_fail_next(YAZA_VFS_V1_TEST_FAIL_OPEN);
    assert(yaza_vfs_v1_root_open(&root_path, &root, &error) == 0);
    assert(root == 0 && error != NULL);
    yaza_vfs_v1_test_fail_next(YAZA_VFS_V1_TEST_FAIL_FSTAT);
    assert(yaza_vfs_v1_root_open(&root_path, &root, &error) == 0);
    assert(root == 0 && error != NULL);
    assert(yaza_vfs_v1_root_open(&root_path, &root, &error) == 1);
    assert(root > 0 && error == NULL);
    char lock_directory_path[4096];
    const int lock_directory_path_length = snprintf(lock_directory_path, sizeof(lock_directory_path), "%s/.yaza-locks", directory);
    assert(lock_directory_path_length > 0 && (size_t)lock_directory_path_length < sizeof(lock_directory_path));
    char lock_file_path[4096];
    const int lock_file_path_length = snprintf(lock_file_path, sizeof(lock_file_path), "%s/.yaza-locks/database-one", directory);
    assert(lock_file_path_length > 0 && (size_t)lock_file_path_length < sizeof(lock_file_path));
    YazaTextV1 database_lock_name = text_view("database-one");
    int64_t lock_handle = 0;
    int64_t competing_lock_handle = 0;
    yaza_vfs_v1_test_fail_next(YAZA_VFS_V1_TEST_FAIL_FLOCK);
    assert(yaza_vfs_v1_lock_exclusive(root, &database_lock_name, &lock_handle, &error) == 0);
    assert(lock_handle == 0 && error != NULL);
    assert(yaza_vfs_v1_lock_exclusive(root, &database_lock_name, &lock_handle, &error) == 1);
    assert(lock_handle > 0 && error == NULL);
    assert(yaza_vfs_v1_lock_exclusive(root, &database_lock_name, &competing_lock_handle, &error) == 0);
    assert(competing_lock_handle == 0 && error != NULL);
    assert(yaza_vfs_v1_close(lock_handle, &error) == 1 && error == NULL);
    assert(yaza_vfs_v1_lock_exclusive(root, &database_lock_name, &competing_lock_handle, &error) == 1);
    assert(competing_lock_handle > 0 && error == NULL);
    assert(yaza_vfs_v1_close(competing_lock_handle, &error) == 1 && error == NULL);
    char lock_directory_symlink[4096];
    const int lock_symlink_length = snprintf(lock_directory_symlink, sizeof(lock_directory_symlink), "%s/.yaza-locks", directory);
    assert(lock_symlink_length > 0 && (size_t)lock_symlink_length < sizeof(lock_directory_symlink));
    assert(unlink(lock_file_path) == 0);
    assert(rmdir(lock_directory_path) == 0);
    assert(symlink("/tmp", lock_directory_symlink) == 0);
    assert(yaza_vfs_v1_lock_exclusive(root, &database_lock_name, &lock_handle, &error) == 0);
    assert(lock_handle == 0 && error != NULL);
    assert(unlink(lock_directory_symlink) == 0);
    assert(yaza_vfs_v1_lock_exclusive(root, &database_lock_name, &lock_handle, &error) == 1);
    assert(yaza_vfs_v1_close(lock_handle, &error) == 1 && error == NULL);
    assert(chmod(lock_file_path, 0644) == 0);
    assert(yaza_vfs_v1_lock_exclusive(root, &database_lock_name, &lock_handle, &error) == 0);
    assert(lock_handle == 0 && error != NULL);
    assert(chmod(lock_file_path, 0600) == 0);
    assert(chmod(lock_directory_path, 0755) == 0);
    assert(yaza_vfs_v1_lock_exclusive(root, &database_lock_name, &lock_handle, &error) == 0);
    assert(lock_handle == 0 && error != NULL);
    assert(chmod(lock_directory_path, 0700) == 0);
    assert(unlink(lock_file_path) == 0);
    assert(rmdir(lock_directory_path) == 0);
    const int64_t out_of_range_slot_handle = ((int64_t)1 << 8) | 0xff;
    assert(yaza_vfs_v1_close(out_of_range_slot_handle, &error) == 0);
    assert(error != NULL);

    int64_t invalid_handle = 0;
    int64_t size = -1;
    YazaTextV1 traversal = text_view("../outside");
    assert(yaza_vfs_v1_open_read(root, &traversal, &invalid_handle, &error) == 0);
    assert(invalid_handle == 0 && error != NULL);
    const unsigned char embedded_nul_name[] = {'a', '\0', 'b'};
    const unsigned char invalid_utf8_name[] = {0xc0, 0xaf};
    YazaTextV1 invalid_names[] = {
        text_view("."), text_view(".."), text_view("nested/name"), text_view("back\\slash"),
        {(const unsigned char *)embedded_nul_name, sizeof(embedded_nul_name)},
        {invalid_utf8_name, sizeof(invalid_utf8_name)}
    };
    for (size_t index = 0; index < sizeof(invalid_names) / sizeof(invalid_names[0]); ++index) {
        assert(yaza_vfs_v1_open_read(root, &invalid_names[index], &invalid_handle, &error) == 0);
        assert(invalid_handle == 0 && error != NULL);
    }
    assert(yaza_vfs_v1_open_read(root, &root_path, &invalid_handle, &error) == 0);
    assert(error != NULL);
    assert(yaza_vfs_v1_file_size(root, &size, &error) == 0);
    assert(error != NULL);

    char fifo_path[4096];
    const int fifo_path_length = snprintf(fifo_path, sizeof(fifo_path), "%s/blocked-reader.fifo", directory);
    assert(fifo_path_length > 0 && (size_t)fifo_path_length < sizeof(fifo_path));
    assert(mkfifo(fifo_path, 0600) == 0);
    YazaTextV1 fifo_name = text_view("blocked-reader.fifo");
    int64_t fifo_handle = 0;
    assert(yaza_vfs_v1_open_read(root, &fifo_name, &fifo_handle, &error) == 0);
    assert(fifo_handle == 0 && error != NULL);
    assert(unlink(fifo_path) == 0);

    YazaTextV1 name = text_view("page-0001.ydb");
    char database_file_path[4096];
    const int database_file_path_length = snprintf(database_file_path, sizeof(database_file_path), "%s/page-0001.ydb", directory);
    assert(database_file_path_length > 0 && (size_t)database_file_path_length < sizeof(database_file_path));
    int64_t file = 0;
    yaza_vfs_v1_test_fail_next(YAZA_VFS_V1_TEST_FAIL_OPENAT);
    assert(yaza_vfs_v1_create_exclusive(root, &name, &file, &error) == 0);
    assert(file == 0 && error != NULL);
    assert(yaza_vfs_v1_create_exclusive(root, &name, &file, &error) == 1);
    assert(file > 0 && error == NULL);
    YazaBytesV1 directory_names = {NULL, 0};
    assert(yaza_vfs_v1_list_names(root, &directory_names, &error) == 1);
    assert(error == NULL && bytes_list_contains_name(&directory_names, "page-0001.ydb"));
    YazaBytesV1 repeated_directory_names = {NULL, 0};
    assert(yaza_vfs_v1_list_names(root, &repeated_directory_names, &error) == 1);
    assert(error == NULL && bytes_list_contains_name(&repeated_directory_names, "page-0001.ydb"));
    assert(chmod(database_file_path, 0644) == 0);
    assert(yaza_vfs_v1_open_read(root, &name, &invalid_handle, &error) == 0);
    assert(invalid_handle == 0 && error != NULL);
    assert(chmod(database_file_path, 0600) == 0);
    assert(yaza_vfs_v1_open_read(root, &name, &invalid_handle, &error) == 1);
    assert(invalid_handle > 0 && error == NULL);
    assert(yaza_vfs_v1_close(invalid_handle, &error) == 1 && error == NULL);
    const uint64_t before_listing_temporary_quota_failure = yaza_arena_v1_used(yaza_arena_v1_current());
    yaza_vfs_v1_test_set_temporary_limit(0);
    YazaBytesV1 quota_limited_directory_names = {(unsigned char *)1, 9};
    assert(yaza_vfs_v1_list_names(root, &quota_limited_directory_names, &error) == 0);
    assert(error != NULL && quota_limited_directory_names.data == NULL && quota_limited_directory_names.length == 0);
    assert(yaza_arena_v1_used(yaza_arena_v1_current()) == before_listing_temporary_quota_failure);
    yaza_vfs_v1_test_set_temporary_limit(YAZA_VFS_V1_MAX_TEMPORARY_BYTES);
    yaza_vfs_v1_test_fail_next(YAZA_VFS_V1_TEST_FAIL_FSTAT);
    assert(yaza_vfs_v1_open_read(root, &name, &invalid_handle, &error) == 0);
    assert(invalid_handle == 0 && error != NULL);
    int64_t duplicate = 0;
    assert(yaza_vfs_v1_create_exclusive(root, &name, &duplicate, &error) == 0);
    assert(duplicate == 0 && error != NULL);
    assert(YAZA_VFS_V1_TEST_HANDLE_LIMIT >= 3);
    int64_t extra_handles[YAZA_VFS_V1_TEST_HANDLE_LIMIT];
    const size_t extra_handle_count = YAZA_VFS_V1_TEST_HANDLE_LIMIT - 2U;
    for (size_t index = 0; index < extra_handle_count; ++index) {
        assert(yaza_vfs_v1_open_read(root, &name, &extra_handles[index], &error) == 1);
        assert(extra_handles[index] > 0 && error == NULL);
    }
    assert(yaza_vfs_v1_open_read(root, &name, &invalid_handle, &error) == 0);
    assert(invalid_handle == 0 && error != NULL);
    for (size_t index = 0; index < extra_handle_count; ++index) {
        assert(yaza_vfs_v1_close(extra_handles[index], &error) == 1);
        assert(error == NULL);
    }

    assert(yaza_vfs_v1_close(root, &error) == 0);
    assert(error != NULL);

    const unsigned char payload[] = {0x59, 0x44, 0x42, 0x31, 0x00, 0xff};
    YazaBytesV1 input = {(unsigned char *)payload, sizeof(payload)};
    YazaBytesV1 read_back = {NULL, 0};
    int64_t written = -1;
    assert(yaza_vfs_v1_read_exact(file, 0, (int64_t)YAZA_VFS_V1_MAX_TRANSFER_BYTES + 1, &read_back, &error) == 0);
    assert(error != NULL && read_back.data == NULL && read_back.length == 0);
    YazaBytesV1 oversized_input = {(unsigned char *)payload, YAZA_VFS_V1_MAX_TRANSFER_BYTES + 1};
    assert(yaza_vfs_v1_write_exact(file, 0, &oversized_input, &written, &error) == 0);
    assert(error != NULL && written == 0);
    assert(yaza_vfs_v1_write_exact(file, 8, &input, &written, &error) == 1);
    assert(written == (int64_t)sizeof(payload) && error == NULL);
    YazaBytesV1 empty = {NULL, 0};
    assert(yaza_vfs_v1_write_exact(file, 0, &empty, &written, &error) == 1);
    assert(written == 0 && error == NULL);

    assert(yaza_vfs_v1_file_size(file, &size, &error) == 1);
    assert(size == 8 + (int64_t)sizeof(payload) && error == NULL);
    yaza_vfs_v1_test_fail_next(YAZA_VFS_V1_TEST_FAIL_FSTAT);
    assert(yaza_vfs_v1_file_size(file, &size, &error) == 0);
    assert(size == 0 && error != NULL);
    assert(yaza_vfs_v1_file_size(file, &size, &error) == 1);
    assert(size == 8 + (int64_t)sizeof(payload) && error == NULL);
    read_back = (YazaBytesV1){(unsigned char *)1, 99};
    assert(yaza_vfs_v1_read_exact(file, 8, (int64_t)sizeof(payload), &read_back, &error) == 1);
    assert(error == NULL && read_back.length == sizeof(payload));
    assert(memcmp(read_back.data, payload, sizeof(payload)) == 0);
    assert(yaza_vfs_v1_read_exact(file, 0, 0, &read_back, &error) == 1);
    assert(error == NULL && read_back.data == NULL && read_back.length == 0);
    const uint64_t before_temporary_quota_failure = yaza_arena_v1_used(yaza_arena_v1_current());
    yaza_vfs_v1_test_set_temporary_limit(sizeof(payload) - 1);
    assert(yaza_vfs_v1_read_exact(file, 8, (int64_t)sizeof(payload), &read_back, &error) == 0);
    assert(error != NULL && read_back.data == NULL && read_back.length == 0);
    assert(yaza_arena_v1_used(yaza_arena_v1_current()) == before_temporary_quota_failure);
    yaza_vfs_v1_test_set_temporary_limit(YAZA_VFS_V1_MAX_TEMPORARY_BYTES);
    const uint64_t before_short_read = yaza_arena_v1_used(yaza_arena_v1_current());
    assert(yaza_vfs_v1_read_exact(file, 8, (int64_t)sizeof(payload) + 1, &read_back, &error) == 0);
    assert(error != NULL && read_back.data == NULL && read_back.length == 0);
    assert(yaza_arena_v1_used(yaza_arena_v1_current()) == before_short_read);
    YazaArenaV1 *tiny_arena = yaza_arena_v1_create(4);
    assert(tiny_arena != NULL);
    YazaArenaContextV1 tiny_context = {0};
    assert(yaza_arena_v1_enter(tiny_arena, &tiny_context) == 1);
    YazaBytesV1 quota_limited_names = {(unsigned char *)1, 9};
    assert(yaza_vfs_v1_list_names(root, &quota_limited_names, &error) == 0);
    assert(error != NULL && quota_limited_names.data == NULL && quota_limited_names.length == 0);
    assert(yaza_vfs_v1_read_exact(file, 8, (int64_t)sizeof(payload), &read_back, &error) == 0);
    assert(error != NULL && read_back.data == NULL && read_back.length == 0);
    assert(yaza_arena_v1_leave(&tiny_context) == 1);
    assert(yaza_arena_v1_destroy(tiny_arena) == 1);
    assert(yaza_vfs_v1_read_exact(file, INT64_MAX, 1, &read_back, &error) == 0);
    assert(error != NULL);
    assert(yaza_vfs_v1_write_exact(file, INT64_MAX, &input, &written, &error) == 0);
    assert(error != NULL && written == 0);
    yaza_vfs_v1_test_fail_next(YAZA_VFS_V1_TEST_FAIL_FSYNC);
    assert(yaza_vfs_v1_sync(file, &error) == 0 && error != NULL);
    assert(yaza_vfs_v1_sync(file, &error) == 1 && error == NULL);
    yaza_vfs_v1_test_fail_next(YAZA_VFS_V1_TEST_FAIL_FTRUNCATE);
    assert(yaza_vfs_v1_truncate(file, 6, &error) == 0 && error != NULL);
    assert(yaza_vfs_v1_file_size(file, &size, &error) == 1 && size == 14);
    yaza_vfs_v1_test_fail_next(YAZA_VFS_V1_TEST_FAIL_PREALLOCATE);
    assert(yaza_vfs_v1_preallocate(file, 65536, &error) == 0 && error != NULL);
    assert(yaza_vfs_v1_file_size(file, &size, &error) == 1 && size == 14);
    assert(yaza_vfs_v1_preallocate(file, 65536, &error) == 1 && error == NULL);
    assert(yaza_vfs_v1_file_size(file, &size, &error) == 1 && size == 65536);
    assert(yaza_vfs_v1_preallocate(file, 1024, &error) == 1 && error == NULL);
    assert(yaza_vfs_v1_file_size(file, &size, &error) == 1 && size == 65536);
    assert(yaza_vfs_v1_preallocate(file, -1, &error) == 0 && error != NULL);
    assert(yaza_vfs_v1_preallocate(file, (int64_t)YAZA_VFS_V1_MAX_FILE_BYTES + 1, &error) == 0 && error != NULL);
    assert(yaza_vfs_v1_truncate(file, 6, &error) == 1 && error == NULL);
    assert(yaza_vfs_v1_file_size(file, &size, &error) == 1 && size == 6);

    yaza_vfs_v1_test_configure(1, 1, 2, -1);
    assert(yaza_vfs_v1_write_exact(file, 16, &input, &written, &error) == 1);
    assert(written == (int64_t)sizeof(payload) && error == NULL);
    YazaBytesV1 retried_read = {NULL, 0};
    assert(yaza_vfs_v1_read_exact(file, 16, (int64_t)sizeof(payload), &retried_read, &error) == 1);
    assert(error == NULL && retried_read.length == sizeof(payload));
    assert(memcmp(retried_read.data, payload, sizeof(payload)) == 0);

    yaza_vfs_v1_test_configure(0, 0, 2, 2);
    assert(yaza_vfs_v1_write_exact(file, 32, &input, &written, &error) == 0);
    assert(written == 2 && error != NULL);
    yaza_vfs_v1_test_configure(0, 0, 0, -1);
    YazaBytesV1 partial_write = {NULL, 0};
    assert(yaza_vfs_v1_read_exact(file, 32, 2, &partial_write, &error) == 1);
    assert(error == NULL && partial_write.length == 2);
    assert(memcmp(partial_write.data, payload, 2) == 0);

    YazaTextV1 symlink_name = text_view("escape-link");
    char child_symlink[4096];
    const int child_path_length = snprintf(child_symlink, sizeof(child_symlink), "%s/escape-link", directory);
    assert(child_path_length > 0 && (size_t)child_path_length < sizeof(child_symlink));
    assert(symlink("/etc/passwd", child_symlink) == 0);
    int64_t symlink_file = 0;
    assert(yaza_vfs_v1_open_read(root, &symlink_name, &symlink_file, &error) == 0);
    assert(symlink_file == 0 && error != NULL);

    pthread_t threads[4];
    VfsThreadCase thread_cases[4];
    for (uint64_t index = 0; index < 4; ++index) {
        thread_cases[index] = (VfsThreadCase){file, index};
        assert(pthread_create(&threads[index], NULL, exercise_shared_handle, &thread_cases[index]) == 0);
    }
    for (size_t index = 0; index < 4; ++index) assert(pthread_join(threads[index], NULL) == 0);

    int64_t read_only = 0;
    assert(yaza_vfs_v1_open_read(root, &name, &read_only, &error) == 1);
    assert(yaza_vfs_v1_write_exact(read_only, 0, &input, &written, &error) == 0);
    assert(error != NULL && written == 0);

    const int32_t close_read_status = yaza_vfs_v1_close(read_only, &error);
    if (close_read_status != 1 || error != NULL) {
        fprintf(stderr, "close read-only handle failed: status=%d error=%s handle=%lld\n",
                close_read_status, error == NULL ? "(null)" : error, (long long)read_only);
    }
    assert(close_read_status == 1 && error == NULL);
    assert(yaza_vfs_v1_close(file, &error) == 1 && error == NULL);
    assert(yaza_vfs_v1_close(file, &error) == 0 && error != NULL);
    assert(yaza_vfs_v1_open_rw(file, &name, &invalid_handle, &error) == 0);
    assert(error != NULL);

    YazaTextV1 renamed = text_view("page-0001.new");
    assert(yaza_vfs_v1_rename(root, &name, &renamed, &error) == 1 && error == NULL);
    assert(yaza_vfs_v1_rename(root, &renamed, &name, &error) == 1 && error == NULL);
    yaza_vfs_v1_test_fail_next(YAZA_VFS_V1_TEST_FAIL_RENAMEAT);
    assert(yaza_vfs_v1_rename(root, &name, &renamed, &error) == 0 && error != NULL);
    int64_t still_original = 0;
    assert(yaza_vfs_v1_open_read(root, &name, &still_original, &error) == 1);
    assert(yaza_vfs_v1_close(still_original, &error) == 1 && error == NULL);
    assert(yaza_vfs_v1_rename(root, &name, &renamed, &error) == 1 && error == NULL);
    yaza_vfs_v1_test_fail_next(YAZA_VFS_V1_TEST_FAIL_FSYNC);
    assert(yaza_vfs_v1_sync_directory(root, &error) == 0 && error != NULL);
    assert(yaza_vfs_v1_sync_directory(root, &error) == 1 && error == NULL);
    assert(yaza_vfs_v1_open_read(root, &name, &invalid_handle, &error) == 0 && error != NULL);
    int64_t renamed_file = 0;
    assert(yaza_vfs_v1_open_read(root, &renamed, &renamed_file, &error) == 1);
    assert(yaza_vfs_v1_close(renamed_file, &error) == 1);
    yaza_vfs_v1_test_fail_next(YAZA_VFS_V1_TEST_FAIL_UNLINKAT);
    assert(yaza_vfs_v1_remove(root, &renamed, &error) == 0 && error != NULL);
    renamed_file = 0;
    assert(yaza_vfs_v1_open_read(root, &renamed, &renamed_file, &error) == 1);
    yaza_vfs_v1_test_fail_next(YAZA_VFS_V1_TEST_FAIL_CLOSE);
    assert(yaza_vfs_v1_close(renamed_file, &error) == 0 && error != NULL);
    assert(yaza_vfs_v1_close(renamed_file, &error) == 0 && error != NULL);
    assert(yaza_vfs_v1_remove(root, &renamed, &error) == 1 && error == NULL);
    assert(yaza_vfs_v1_remove(root, &renamed, &error) == 0 && error != NULL);
    assert(yaza_vfs_v1_remove(root, &symlink_name, &error) == 1 && error == NULL);
    assert(yaza_vfs_v1_sync_directory(root, &error) == 1 && error == NULL);

    assert(yaza_vfs_v1_close(root, &error) == 1 && error == NULL);
    assert(yaza_vfs_v1_sync(root, &error) == 0 && error != NULL);
    assert(unlink(symlink_path) == 0);
    assert(rmdir(directory) == 0);
    puts("yazabase-vfs-v1: capability, confinement, I/O, and lifecycle smoke passed");
    return 0;
}

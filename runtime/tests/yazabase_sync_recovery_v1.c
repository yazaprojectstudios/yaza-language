#define _DARWIN_C_SOURCE 1
#define _XOPEN_SOURCE 700
#include "yaza_runtime_v1.h"

#include <assert.h>
#include <errno.h>
#include <ftw.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define YAZA_TEST_CRASH_EXIT 86
#define YAZA_TEST_MAX_SYNC_BOUNDARIES 128ULL

extern int64_t yazabase_sync_recovery_operation_v1(int64_t root, int64_t action, int64_t value)
    __asm__("_yaza.core.fn.yazabase_sync_recovery_operation_v1");

static int write_all(int descriptor, const void *data, size_t length) {
    const unsigned char *bytes = (const unsigned char *)data;
    size_t offset = 0;
    while (offset < length) {
        const ssize_t amount = write(descriptor, bytes + offset, length - offset);
        if (amount < 0 && errno == EINTR) continue;
        if (amount <= 0) return 0;
        offset += (size_t)amount;
    }
    return 1;
}

static int read_all(int descriptor, void *data, size_t length) {
    unsigned char *bytes = (unsigned char *)data;
    size_t offset = 0;
    while (offset < length) {
        const ssize_t amount = read(descriptor, bytes + offset, length - offset);
        if (amount < 0 && errno == EINTR) continue;
        if (amount <= 0) return 0;
        offset += (size_t)amount;
    }
    return 1;
}

static int run_child_operation_with_faults(const char *root_path, int64_t action, int64_t value,
                                           uint64_t crash_after_fsync, int64_t pwrite_fail_after_bytes,
                                           uint32_t fail_next_syscall, uint64_t *out_fsync_count) {
    int count_pipe[2] = {-1, -1};
    if (out_fsync_count != NULL && pipe(count_pipe) != 0) return 253;

    const pid_t child = fork();
    if (child < 0) {
        if (count_pipe[0] >= 0) close(count_pipe[0]);
        if (count_pipe[1] >= 0) close(count_pipe[1]);
        return 253;
    }
    if (child == 0) {
        if (count_pipe[0] >= 0) close(count_pipe[0]);
        if (setenv("YAZA_RT_V1_PROCESS_LIMIT_BYTES", "2147483648", 1) != 0) _exit(250);
        yaza_rt_v1_init();

        YazaTextV1 path = {(const unsigned char *)root_path, (uint64_t)strlen(root_path)};
        int64_t root = 0;
        char *error = NULL;
        if (yaza_vfs_v1_root_open(&path, &root, &error) != 1) _exit(251);

        if (pwrite_fail_after_bytes >= 0) yaza_vfs_v1_test_configure(0, 0, 0, pwrite_fail_after_bytes);
        if (fail_next_syscall != 0) yaza_vfs_v1_test_fail_next(fail_next_syscall);
        if (crash_after_fsync != 0) yaza_vfs_v1_test_crash_after_fsync(crash_after_fsync);
        const int64_t result = yazabase_sync_recovery_operation_v1(root, action, value);
        if (out_fsync_count != NULL) {
            const uint64_t count = yaza_vfs_v1_test_fsync_count();
            if (!write_all(count_pipe[1], &count, sizeof(count))) _exit(252);
        }
        if (count_pipe[1] >= 0) close(count_pipe[1]);

        char *close_error = NULL;
        if (yaza_vfs_v1_close(root, &close_error) != 1) _exit(249);
        if (result < 0 || result > 248) _exit(248);
        (void)fflush(NULL);
        _exit((int)result);
    }

    if (count_pipe[1] >= 0) close(count_pipe[1]);
    if (out_fsync_count != NULL) {
        const int read_ok = read_all(count_pipe[0], out_fsync_count, sizeof(*out_fsync_count));
        close(count_pipe[0]);
        if (!read_ok) {
            int ignored_status = 0;
            (void)waitpid(child, &ignored_status, 0);
            return 253;
        }
    }

    int status = 0;
    if (waitpid(child, &status, 0) != child || !WIFEXITED(status)) return 253;
    return WEXITSTATUS(status);
}

static int run_child_operation(const char *root_path, int64_t action, int64_t value,
                               uint64_t crash_after_fsync, uint64_t *out_fsync_count) {
    return run_child_operation_with_faults(root_path, action, value, crash_after_fsync, -1, 0, out_fsync_count);
}

static int remove_tree_entry(const char *path, const struct stat *status, int type, struct FTW *walk) {
    (void)status;
    (void)type;
    (void)walk;
    return remove(path);
}

static int index_expect_action(const char *root_path, int64_t action, int64_t value, int expected,
                               const char *operation) {
    const int status = run_child_operation(root_path, action, value, 0, NULL);
    if (status == expected) return 1;
    fprintf(stderr, "YazaBase secondary-index %s returned %d, expected %d\n", operation, status, expected);
    return 0;
}

static int index_measure_syncs(const char *root_path, int64_t action, int64_t value,
                              uint64_t *out_count, const char *operation) {
    if (run_child_operation(root_path, action, value, UINT64_MAX, out_count) == 0 &&
        *out_count > 0 && *out_count <= YAZA_TEST_MAX_SYNC_BOUNDARIES) return 1;
    fprintf(stderr, "YazaBase secondary-index %s reported invalid fsync count: %" PRIu64 "\n",
            operation, *out_count);
    return 0;
}

static int index_test_crash_boundaries(const char *root_path, int64_t action, int64_t verify_action,
                                       int64_t value_base, uint64_t boundary_count, int64_t cleanup_action,
                                       const char *operation) {
    for (uint64_t boundary = 1; boundary <= boundary_count; ++boundary) {
        const int64_t value = value_base + (int64_t)boundary;
        const int status = run_child_operation(root_path, action, value, boundary, NULL);
        if (status != YAZA_TEST_CRASH_EXIT) {
            fprintf(stderr, "YazaBase secondary-index %s did not crash at fsync boundary %" PRIu64 ": %d\n",
                    operation, boundary, status);
            return 0;
        }
        const int verify_status = run_child_operation(root_path, verify_action, value, 0, NULL);
        if (verify_status != 0) {
            fprintf(stderr, "YazaBase secondary-index %s recovery check failed at fsync boundary %" PRIu64 ": %d\n",
                    operation, boundary, verify_status);
            return 0;
        }
        if (cleanup_action >= 0 &&
            !index_expect_action(root_path, cleanup_action, value, 0, "per-boundary cleanup")) return 0;
    }
    return 1;
}

static int unique_index_test_dml_boundaries(const char *root_path, int64_t action,
                                            int64_t verify_action, int64_t seed_action,
                                            int64_t active_unique_index_id, int64_t value_base,
                                            uint64_t boundary_count, const char *operation) {
    for (uint64_t boundary = 1; boundary <= boundary_count; ++boundary) {
        const int64_t value = value_base + (int64_t)boundary;
        if (seed_action >= 0 &&
            !index_expect_action(root_path, seed_action, value, 0, "unique DML boundary setup INSERT")) return 0;
        const int status = run_child_operation(root_path, action, value, boundary, NULL);
        if (status != YAZA_TEST_CRASH_EXIT) {
            fprintf(stderr, "YazaBase unique-index %s did not crash at fsync boundary %" PRIu64 ": %d\n",
                    operation, boundary, status);
            return 0;
        }
        const int verify_status = run_child_operation(root_path, verify_action, value, 0, NULL);
        if (verify_status != 0) {
            fprintf(stderr, "YazaBase unique-index %s recovery check failed at fsync boundary %" PRIu64 ": %d\n",
                    operation, boundary, verify_status);
            return 0;
        }
        if (!index_expect_action(root_path, 31, active_unique_index_id, 0,
                                 "unique constraint after DML recovery")) return 0;
    }
    return 1;
}

static int index_test_indexed_batch_crash_boundaries(const char *root_path, uint64_t boundary_count) {
    for (uint64_t boundary = 1; boundary <= boundary_count; ++boundary) {
        const int64_t first_id = 9800000 + (int64_t)boundary * 10;
        const int status = run_child_operation(root_path, 49, first_id, boundary, NULL);
        if (status != YAZA_TEST_CRASH_EXIT) {
            fprintf(stderr, "YazaBase indexed multi-row INSERT did not crash at fsync boundary %" PRIu64 ": %d\n",
                    boundary, status);
            return 0;
        }
        const int verify_status = run_child_operation(root_path, 50, first_id, 0, NULL);
        if (verify_status != 0) {
            fprintf(stderr, "YazaBase indexed multi-row INSERT recovery check failed at fsync boundary %" PRIu64 ": %d\n",
                    boundary, verify_status);
            return 0;
        }
    }
    return 1;
}

static int index_test_indexed_batch_delete_crash_boundaries(const char *root_path, uint64_t boundary_count) {
    for (uint64_t boundary = 1; boundary <= boundary_count; ++boundary) {
        const int64_t first_id = 9400000 + (int64_t)boundary * 10;
        if (!index_expect_action(root_path, 49, first_id, 0, "indexed multi-row DELETE setup INSERT")) return 0;
        const int status = run_child_operation(root_path, 51, first_id, boundary, NULL);
        if (status != YAZA_TEST_CRASH_EXIT) {
            fprintf(stderr, "YazaBase indexed multi-row DELETE did not crash at fsync boundary %" PRIu64 ": %d\n",
                    boundary, status);
            return 0;
        }
        const int verify_status = run_child_operation(root_path, 52, first_id, 0, NULL);
        if (verify_status != 0) {
            fprintf(stderr, "YazaBase indexed multi-row DELETE recovery check failed at fsync boundary %" PRIu64 ": %d\n",
                    boundary, verify_status);
            return 0;
        }
    }
    return 1;
}

static int run_secondary_index_crash_matrix(void) {
    char root_path[] = "/tmp/yazabase-index-sync-recovery.XXXXXX";
    if (mkdtemp(root_path) == NULL ||
        run_child_operation(root_path, 0, 0, 0, NULL) != 0 ||
        run_child_operation(root_path, 1, 0, 0, NULL) != 0 ||
        run_child_operation(root_path, 2, 1, 0, NULL) != 0 ||
        run_child_operation(root_path, 23, 8000000, 0, NULL) != 0) {
        fprintf(stderr, "YazaBase secondary-index crash fixture setup failed\n");
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }

    uint64_t create_sync_count = 0;
    if (!index_measure_syncs(root_path, 18, 970000, &create_sync_count, "CREATE INDEX") ||
        !index_expect_action(root_path, 20, 970000, 0, "probe DROP INDEX")) goto failed;
    if (!index_test_crash_boundaries(root_path, 18, 19, 1000000, create_sync_count, 20,
                                     "CREATE INDEX")) goto failed;

    if (!index_expect_action(root_path, 18, 970001, 0, "DROP INDEX probe CREATE INDEX")) goto failed;
    uint64_t drop_sync_count = 0;
    if (!index_measure_syncs(root_path, 20, 970001, &drop_sync_count, "DROP INDEX")) goto failed;
    for (uint64_t boundary = 1; boundary <= drop_sync_count; ++boundary) {
        const int64_t index_id = 1100000 + (int64_t)boundary;
        if (!index_expect_action(root_path, 18, index_id, 0, "DROP INDEX setup CREATE INDEX")) goto failed;
        const int status = run_child_operation(root_path, 20, index_id, boundary, NULL);
        if (status != YAZA_TEST_CRASH_EXIT) {
            fprintf(stderr, "YazaBase secondary-index DROP INDEX did not crash at fsync boundary %" PRIu64 ": %d\n",
                    boundary, status);
            goto failed;
        }
        const int verify_status = run_child_operation(root_path, 21, index_id, 0, NULL);
        if (verify_status != 0) {
            fprintf(stderr, "YazaBase secondary-index DROP INDEX recovery check failed at fsync boundary %" PRIu64 ": %d\n",
                    boundary, verify_status);
            goto failed;
        }
        if (!index_expect_action(root_path, 29, index_id, 0, "DROP INDEX cleanup")) goto failed;
    }

    if (!index_expect_action(root_path, 22, 999999, 0, "DML index CREATE INDEX")) goto failed;

    uint64_t indexed_batch_sync_count = 0;
    if (!index_measure_syncs(root_path, 49, 9700000, &indexed_batch_sync_count, "indexed multi-row INSERT") ||
        !index_test_indexed_batch_crash_boundaries(root_path, indexed_batch_sync_count)) goto failed;

    if (!index_expect_action(root_path, 49, 9350000, 0, "indexed multi-row DELETE measurement setup INSERT")) goto failed;
    uint64_t indexed_batch_delete_sync_count = 0;
    if (!index_measure_syncs(root_path, 51, 9350000, &indexed_batch_delete_sync_count, "indexed multi-row DELETE") ||
        !index_test_indexed_batch_delete_crash_boundaries(root_path, indexed_batch_delete_sync_count)) goto failed;

    uint64_t insert_sync_count = 0;
    if (!index_measure_syncs(root_path, 23, 910000, &insert_sync_count, "indexed INSERT") ||
        !index_test_crash_boundaries(root_path, 23, 24, 920000, insert_sync_count, -1,
                                     "indexed INSERT")) goto failed;

    uint64_t update_sync_count = 0;
    if (!index_measure_syncs(root_path, 25, 910000, &update_sync_count, "indexed UPDATE")) goto failed;
    for (uint64_t boundary = 1; boundary <= update_sync_count; ++boundary) {
        const int64_t row_id = 930000 + (int64_t)boundary;
        if (!index_expect_action(root_path, 23, row_id, 0, "indexed UPDATE setup INSERT")) goto failed;
        const int status = run_child_operation(root_path, 25, row_id, boundary, NULL);
        if (status != YAZA_TEST_CRASH_EXIT) {
            fprintf(stderr, "YazaBase secondary-index UPDATE did not crash at fsync boundary %" PRIu64 ": %d\n",
                    boundary, status);
            goto failed;
        }
        if (!index_expect_action(root_path, 26, row_id, 0, "indexed UPDATE recovery check")) goto failed;
    }

    if (!index_expect_action(root_path, 23, 940000, 0, "indexed DELETE probe INSERT")) goto failed;
    uint64_t delete_sync_count = 0;
    if (!index_measure_syncs(root_path, 27, 940000, &delete_sync_count, "indexed DELETE")) goto failed;
    for (uint64_t boundary = 1; boundary <= delete_sync_count; ++boundary) {
        const int64_t row_id = 950000 + (int64_t)boundary;
        if (!index_expect_action(root_path, 23, row_id, 0, "indexed DELETE setup INSERT")) goto failed;
        const int status = run_child_operation(root_path, 27, row_id, boundary, NULL);
        if (status != YAZA_TEST_CRASH_EXIT) {
            fprintf(stderr, "YazaBase secondary-index DELETE did not crash at fsync boundary %" PRIu64 ": %d\n",
                    boundary, status);
            goto failed;
        }
        if (!index_expect_action(root_path, 28, row_id, 0, "indexed DELETE recovery check")) goto failed;
    }

    if (!index_expect_action(root_path, 29, 999999, 0, "DML index cleanup DROP INDEX")) goto failed;
    (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
    printf("YazaBase secondary-index recovery: CREATE INDEX %" PRIu64 ", DROP INDEX %" PRIu64
           ", indexed multi-row INSERT %" PRIu64 ", indexed multi-row DELETE %" PRIu64
           ", indexed INSERT %" PRIu64 ", UPDATE %" PRIu64 ", DELETE %" PRIu64
           " successful-fsync crash boundaries; recovery preserved index/query consistency\n",
           create_sync_count, drop_sync_count, indexed_batch_sync_count, indexed_batch_delete_sync_count,
           insert_sync_count, update_sync_count, delete_sync_count);
    return 0;

failed:
    (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
    return 1;
}

static int run_unique_index_crash_matrix(void) {
    char root_path[] = "/tmp/yazabase-unique-index-sync-recovery.XXXXXX";
    if (mkdtemp(root_path) == NULL ||
        run_child_operation(root_path, 0, 0, 0, NULL) != 0 ||
        run_child_operation(root_path, 1, 0, 0, NULL) != 0 ||
        run_child_operation(root_path, 2, 1, 0, NULL) != 0 ||
        run_child_operation(root_path, 23, 8000000, 0, NULL) != 0) {
        fprintf(stderr, "YazaBase UNIQUE INDEX crash fixture setup failed\n");
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }

    uint64_t create_sync_count = 0;
    if (!index_measure_syncs(root_path, 30, 970002, &create_sync_count, "CREATE UNIQUE INDEX") ||
        !index_expect_action(root_path, 32, 970002, 0, "probe DROP UNIQUE INDEX") ||
        !index_test_crash_boundaries(root_path, 30, 31, 980000, create_sync_count, 32,
                                     "CREATE UNIQUE INDEX")) {
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }

    const int64_t active_unique_index_id = 999998;
    if (!index_expect_action(root_path, 30, active_unique_index_id, 0,
                             "CREATE UNIQUE INDEX for DML crash coverage") ||
        !index_expect_action(root_path, 31, active_unique_index_id, 0,
                             "initial unique constraint check")) goto failed_unique;

    uint64_t insert_sync_count = 0;
    if (!index_measure_syncs(root_path, 23, 900000, &insert_sync_count, "unique indexed INSERT") ||
        !index_expect_action(root_path, 24, 900000, 0, "unique indexed INSERT measurement check") ||
        !index_expect_action(root_path, 27, 900000, 0, "unique indexed INSERT measurement cleanup") ||
        !unique_index_test_dml_boundaries(root_path, 23, 24, -1, active_unique_index_id,
                                          1000000, insert_sync_count, "INSERT")) goto failed_unique;

    const int64_t update_measure_row = 950000;
    if (!index_expect_action(root_path, 23, update_measure_row, 0, "unique indexed UPDATE measurement setup")) goto failed_unique;
    uint64_t update_sync_count = 0;
    if (!index_measure_syncs(root_path, 25, update_measure_row, &update_sync_count, "unique indexed UPDATE") ||
        !index_expect_action(root_path, 26, update_measure_row, 0, "unique indexed UPDATE measurement check") ||
        !unique_index_test_dml_boundaries(root_path, 25, 26, 23, active_unique_index_id,
                                          1200000, update_sync_count, "UPDATE")) goto failed_unique;

    const int64_t delete_measure_row = 970000;
    if (!index_expect_action(root_path, 23, delete_measure_row, 0, "unique indexed DELETE measurement setup")) goto failed_unique;
    uint64_t delete_sync_count = 0;
    if (!index_measure_syncs(root_path, 27, delete_measure_row, &delete_sync_count, "unique indexed DELETE") ||
        !index_expect_action(root_path, 28, delete_measure_row, 0, "unique indexed DELETE measurement check") ||
        !unique_index_test_dml_boundaries(root_path, 27, 28, 23, active_unique_index_id,
                                          1400000, delete_sync_count, "DELETE")) goto failed_unique;

    if (!index_expect_action(root_path, 32, active_unique_index_id, 0,
                             "DROP UNIQUE INDEX after DML crash coverage")) goto failed_unique;
    (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
    printf("YazaBase UNIQUE INDEX recovery: CREATE UNIQUE INDEX %" PRIu64
           ", unique indexed INSERT %" PRIu64 ", UPDATE %" PRIu64 ", DELETE %" PRIu64
           " successful-fsync crash boundaries; recovery preserved rows/indexes and rejected duplicate inserts\n",
           create_sync_count, insert_sync_count, update_sync_count, delete_sync_count);
    return 0;

failed_unique:
    (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
    return 1;
}

static int run_secondary_index_drop_boundary(uint64_t boundary) {
    char root_path[] = "/tmp/yazabase-index-drop-boundary.XXXXXX";
    if (boundary == 0 || boundary > YAZA_TEST_MAX_SYNC_BOUNDARIES || mkdtemp(root_path) == NULL ||
        run_child_operation(root_path, 0, 0, 0, NULL) != 0 ||
        run_child_operation(root_path, 1, 0, 0, NULL) != 0 ||
        run_child_operation(root_path, 2, 1, 0, NULL) != 0 ||
        run_child_operation(root_path, 23, 8000000, 0, NULL) != 0) {
        fprintf(stderr, "YazaBase DROP INDEX boundary fixture setup failed\n");
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }

    const int64_t index_id = 1100000 + (int64_t)boundary;
    if (!index_expect_action(root_path, 18, index_id, 0, "boundary CREATE INDEX")) goto failed;
    const int crash_status = run_child_operation(root_path, 20, index_id, boundary, NULL);
    if (crash_status != YAZA_TEST_CRASH_EXIT) {
        fprintf(stderr, "YazaBase DROP INDEX did not crash at fsync boundary %" PRIu64 ": %d\n",
                boundary, crash_status);
        goto failed;
    }
    if (!index_expect_action(root_path, 21, index_id, 0, "boundary recovery check")) goto failed;
    if (!index_expect_action(root_path, 29, index_id, 0, "boundary cleanup")) goto failed;
    (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
    printf("YazaBase secondary-index DROP INDEX fsync boundary %" PRIu64 " recovered and verified\n", boundary);
    return 0;

failed:
    (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
    return 1;
}

static int run_alter_boundary(uint64_t boundary) {
    char root_path[] = "/tmp/yazabase-alter-boundary.XXXXXX";
    uint64_t sync_count = 0;
    const int64_t probe_id = 850000;
    if (mkdtemp(root_path) == NULL ||
        run_child_operation(root_path, 0, 0, 0, NULL) != 0 ||
        run_child_operation(root_path, 1, 0, 0, NULL) != 0 ||
        run_child_operation(root_path, 2, 1, 0, NULL) != 0 ||
        run_child_operation(root_path, 58, probe_id, 0, NULL) != 0 ||
        run_child_operation(root_path, 16, probe_id, UINT64_MAX, &sync_count) != 0 ||
        sync_count == 0 || sync_count > YAZA_TEST_MAX_SYNC_BOUNDARIES ||
        run_child_operation(root_path, 17, probe_id, 0, NULL) != 0) {
        fprintf(stderr, "YazaBase ALTER boundary probe setup failed; fsync count=%" PRIu64 "\n", sync_count);
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }
    if (boundary == 0 || boundary > sync_count) {
        fprintf(stderr, "YazaBase ALTER boundary must be in 1..%" PRIu64 ", got %" PRIu64 "\n",
                sync_count, boundary);
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 2;
    }

    const int64_t table_id = 860000 + (int64_t)boundary;
    if (run_child_operation(root_path, 58, table_id, 0, NULL) != 0) {
        fprintf(stderr, "YazaBase ALTER boundary fixture creation failed at boundary %" PRIu64 "\n", boundary);
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }
    const int crash_status = run_child_operation(root_path, 16, table_id, boundary, NULL);
    if (crash_status != YAZA_TEST_CRASH_EXIT) {
        fprintf(stderr, "YazaBase ALTER did not crash after fsync boundary %" PRIu64 ": status %d\n",
                boundary, crash_status);
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }
    const int schema_status = run_child_operation(root_path, 17, table_id, 0, NULL);
    if (schema_status != 0 && schema_status != 1) {
        fprintf(stderr, "YazaBase ALTER recovery at fsync boundary %" PRIu64 " returned status %d\n",
                boundary, schema_status);
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }
    printf("YazaBase ALTER recovery at fsync boundary %" PRIu64 ": complete %s schema\n",
           boundary, schema_status == 0 ? "new" : "old");
    (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
    return 0;
}

static int run_alter_crash_matrix(void) {
    char root_path[] = "/tmp/yazabase-alter-sync-recovery.XXXXXX";
    uint64_t sync_count = 0;
    const int64_t probe_id = 850000;
    if (mkdtemp(root_path) == NULL ||
        run_child_operation(root_path, 0, 0, 0, NULL) != 0 ||
        run_child_operation(root_path, 1, 0, 0, NULL) != 0 ||
        run_child_operation(root_path, 2, 1, 0, NULL) != 0 ||
        run_child_operation(root_path, 58, probe_id, 0, NULL) != 0 ||
        run_child_operation(root_path, 16, probe_id, UINT64_MAX, &sync_count) != 0 ||
        sync_count == 0 || sync_count > YAZA_TEST_MAX_SYNC_BOUNDARIES ||
        run_child_operation(root_path, 17, probe_id, 0, NULL) != 0) {
        fprintf(stderr, "YazaBase ALTER TABLE reported invalid baseline recovery/fsync count: %" PRIu64 "\n",
                sync_count);
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }

    for (uint64_t boundary = 1; boundary <= sync_count; ++boundary) {
        const int64_t table_id = (int64_t)(860000 + boundary);
        if (run_child_operation(root_path, 58, table_id, 0, NULL) != 0) {
            fprintf(stderr, "YazaBase ALTER TABLE fixture setup failed at fsync boundary %" PRIu64 "\n",
                    boundary);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        const int crash_status = run_child_operation(root_path, 16, table_id, boundary, NULL);
        if (crash_status != YAZA_TEST_CRASH_EXIT) {
            fprintf(stderr, "YazaBase ALTER TABLE did not crash after successful fsync boundary %" PRIu64
                    ": status %d\n", boundary, crash_status);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        const int schema_status = run_child_operation(root_path, 17, table_id, 0, NULL);
        if (schema_status != 0 && schema_status != 1) {
            fprintf(stderr, "YazaBase ALTER TABLE recovery after fsync boundary %" PRIu64
                    " exposed a partial schema state: status %d\n", boundary, schema_status);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
    }

    printf("YazaBase ALTER TABLE: %" PRIu64
           " successful-fsync crash boundaries recovered to a complete old or new schema\n", sync_count);
    (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
    return 0;
}

static int run_overflow_crash_matrix(void) {
    char root_path[] = "/tmp/yazabase-overflow-recovery.XXXXXX";
    if (mkdtemp(root_path) == NULL ||
        run_child_operation(root_path, 0, 0, 0, NULL) != 0 ||
        run_child_operation(root_path, 1, 0, 0, NULL) != 0 ||
        run_child_operation(root_path, 33, 0, 0, NULL) != 0) {
        fprintf(stderr, "YazaBase overflow recovery fixture setup failed\n");
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }

    uint64_t sync_count = 0;
    if (run_child_operation(root_path, 34, 900000, UINT64_MAX, &sync_count) != 0 ||
        sync_count == 0 || sync_count > YAZA_TEST_MAX_SYNC_BOUNDARIES ||
        run_child_operation(root_path, 35, 900000, 0, NULL) != 1) {
        fprintf(stderr, "YazaBase overflow INSERT reported invalid baseline recovery or fsync count: %" PRIu64 "\n", sync_count);
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }

    for (uint64_t boundary = 1; boundary <= sync_count; ++boundary) {
        const int64_t row_id = (int64_t)(1000000 + boundary);
        const int crash_status = run_child_operation(root_path, 34, row_id, boundary, NULL);
        if (crash_status != YAZA_TEST_CRASH_EXIT) {
            fprintf(stderr, "YazaBase overflow INSERT did not crash after successful fsync boundary %" PRIu64 ": status %d\n", boundary, crash_status);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        const int visible = run_child_operation(root_path, 35, row_id, 0, NULL);
        if (visible != 0 && visible != 1) {
            fprintf(stderr, "YazaBase overflow INSERT recovery failed after fsync boundary %" PRIu64 ": status %d\n", boundary, visible);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
    }

    const int64_t update_probe_id = 1900000;
    uint64_t update_sync_count = 0;
    if (run_child_operation(root_path, 34, update_probe_id, 0, NULL) != 0 ||
        run_child_operation(root_path, 36, update_probe_id, UINT64_MAX, &update_sync_count) != 0 ||
        update_sync_count == 0 || update_sync_count > YAZA_TEST_MAX_SYNC_BOUNDARIES ||
        run_child_operation(root_path, 37, update_probe_id, 0, NULL) != 0) {
        fprintf(stderr, "YazaBase overflow UPDATE reported invalid baseline recovery or fsync count: %" PRIu64 "\n", update_sync_count);
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }

    for (uint64_t boundary = 1; boundary <= update_sync_count; ++boundary) {
        const int64_t row_id = (int64_t)(2000000 + boundary);
        if (run_child_operation(root_path, 34, row_id, 0, NULL) != 0) {
            fprintf(stderr, "YazaBase overflow UPDATE baseline INSERT failed before boundary %" PRIu64 "\n", boundary);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        const int crash_status = run_child_operation(root_path, 36, row_id, boundary, NULL);
        if (crash_status != YAZA_TEST_CRASH_EXIT) {
            fprintf(stderr, "YazaBase overflow UPDATE did not crash after successful fsync boundary %" PRIu64 ": status %d\n", boundary, crash_status);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        if (run_child_operation(root_path, 37, row_id, 0, NULL) != 0) {
            fprintf(stderr, "YazaBase overflow UPDATE recovery produced a partial/corrupt value after fsync boundary %" PRIu64 "\n", boundary);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
    }

    (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
    printf("YazaBase overflow INSERT: %" PRIu64 " successful-fsync crash boundaries recovered to a complete row or no row\n", sync_count);
    printf("YazaBase overflow UPDATE: %" PRIu64 " successful-fsync crash boundaries recovered to the complete old or new value\n", update_sync_count);
    return 0;
}

static int prepare_overflow_drop_root(const char *root_path, int64_t test_id) {
    if (run_child_operation(root_path, 0, 0, 0, NULL) != 0 ||
        run_child_operation(root_path, 1, 0, 0, NULL) != 0 ||
        run_child_operation(root_path, 38, 0, 0, NULL) != 0 ||
        run_child_operation(root_path, 39, test_id, 0, NULL) != 0) {
        fprintf(stderr, "YazaBase overflow DROP TABLE fixture setup failed for id %" PRId64 "\n", test_id);
        return 0;
    }
    return 1;
}

static int run_overflow_drop_crash_matrix(void) {
    char probe_root[] = "/tmp/yazabase-overflow-drop-probe.XXXXXX";
    uint64_t sync_count = 0;
    const int64_t probe_id = 7800000;
    if (mkdtemp(probe_root) == NULL || !prepare_overflow_drop_root(probe_root, probe_id) ||
        run_child_operation(probe_root, 40, probe_id, UINT64_MAX, &sync_count) != 0 ||
        sync_count == 0 || sync_count > YAZA_TEST_MAX_SYNC_BOUNDARIES ||
        run_child_operation(probe_root, 41, probe_id, 0, NULL) != 0) {
        fprintf(stderr, "YazaBase overflow DROP TABLE reported invalid baseline recovery/fsync count: %" PRIu64 "\n", sync_count);
        (void)nftw(probe_root, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }
    (void)nftw(probe_root, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);

    for (uint64_t boundary = 1; boundary <= sync_count; ++boundary) {
        char root_path[] = "/tmp/yazabase-overflow-drop-boundary.XXXXXX";
        const int64_t test_id = (int64_t)(7801000 + boundary);
        if (mkdtemp(root_path) == NULL || !prepare_overflow_drop_root(root_path, test_id)) {
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        const int crash_status = run_child_operation(root_path, 40, test_id, boundary, NULL);
        if (crash_status != YAZA_TEST_CRASH_EXIT) {
            fprintf(stderr, "YazaBase overflow DROP TABLE did not crash after successful fsync boundary %" PRIu64 ": status %d\n",
                    boundary, crash_status);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        const int verify_status = run_child_operation(root_path, 41, test_id, 0, NULL);
        if (verify_status != 0) {
            fprintf(stderr, "YazaBase overflow DROP TABLE recovery after fsync boundary %" PRIu64 " exposed partial state: status %d\n",
                    boundary, verify_status);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
    }

    printf("YazaBase DROP TABLE with current and historical overflow TEXT: %" PRIu64
           " successful-fsync crash boundaries recovered to an intact table or fully reclaimed page set\n", sync_count);
    return 0;
}

static int run_range_delete_crash_matrix(uint64_t only_boundary) {
    char probe_path[] = "/tmp/yazabase-range-delete-probe.XXXXXX";
    if (mkdtemp(probe_path) == NULL ||
        run_child_operation(probe_path, 0, 0, 0, NULL) != 0 ||
        run_child_operation(probe_path, 1, 0, 0, NULL) != 0 ||
        run_child_operation(probe_path, 42, 10000, 0, NULL) != 0) {
        fprintf(stderr, "YazaBase range DELETE fixture setup failed\n");
        (void)nftw(probe_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }

    uint64_t sync_count = 0;
    if (run_child_operation(probe_path, 43, 10000, UINT64_MAX, &sync_count) != 0 ||
        sync_count == 0 || sync_count >= 100 || (only_boundary != 0 && only_boundary > sync_count) ||
        run_child_operation(probe_path, 44, 10000, 0, NULL) != 0) {
        fprintf(stderr, "YazaBase 100-row DELETE had an invalid fsync count or result: %" PRIu64 "\n", sync_count);
        (void)nftw(probe_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }
    (void)nftw(probe_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);

    uint64_t first_boundary = only_boundary == 0 ? 1 : only_boundary;
    uint64_t last_boundary = only_boundary == 0 ? sync_count : only_boundary;
    for (uint64_t boundary = first_boundary; boundary <= last_boundary; ++boundary) {
        char root_path[] = "/tmp/yazabase-range-delete-crash.XXXXXX";
        if (mkdtemp(root_path) == NULL ||
            run_child_operation(root_path, 0, 0, 0, NULL) != 0 ||
            run_child_operation(root_path, 1, 0, 0, NULL) != 0 ||
            run_child_operation(root_path, 42, 10000, 0, NULL) != 0) {
            fprintf(stderr, "YazaBase range DELETE crash fixture setup failed at boundary %" PRIu64 "\n", boundary);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        const int crash_status = run_child_operation(root_path, 43, 10000, boundary, NULL);
        if (crash_status != YAZA_TEST_CRASH_EXIT) {
            fprintf(stderr, "YazaBase 100-row DELETE did not crash at successful-fsync boundary %" PRIu64 ": %d\n",
                    boundary, crash_status);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        const int visible_rows = run_child_operation(root_path, 44, 10000, 0, NULL);
        if (visible_rows != 0 && visible_rows != 100) {
            fprintf(stderr, "YazaBase 100-row DELETE recovery exposed a partial result at boundary %" PRIu64 ": %d rows/status\n",
                    boundary, visible_rows);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
    }

    printf("YazaBase 100-row range DELETE: %" PRIu64
           " successful-fsync boundaries recovered atomically; DELETE uses fewer fsyncs than rows\n",
           only_boundary == 0 ? sync_count : (uint64_t)1);
    return 0;
}

static int run_multi_leaf_insert_crash_matrix(void) {
    char root_path[] = "/tmp/yazabase-multi-leaf-insert.XXXXXX";
    if (mkdtemp(root_path) == NULL ||
        run_child_operation(root_path, 0, 0, 0, NULL) != 0 ||
        run_child_operation(root_path, 1, 0, 0, NULL) != 0 ||
        run_child_operation(root_path, 2, 1, 0, NULL) != 0 ||
        run_child_operation(root_path, 45, 0, 0, NULL) != 0) {
        fprintf(stderr, "YazaBase multi-leaf INSERT fixture setup failed\n");
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }
    if (run_child_operation(root_path, 48, 1, 0, NULL) != 0) {
        fprintf(stderr, "YazaBase multi-leaf INSERT fixture did not route its three ordered groups to distinct leaf pages\n");
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }

    uint64_t sync_count = 0;
    if (run_child_operation(root_path, 46, 1, UINT64_MAX, &sync_count) != 0 ||
        sync_count == 0 || sync_count > YAZA_TEST_MAX_SYNC_BOUNDARIES ||
        run_child_operation(root_path, 47, 1, 0, NULL) != 6) {
        fprintf(stderr, "YazaBase multi-leaf INSERT reported invalid baseline state or fsync count: %" PRIu64 "\n", sync_count);
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }

    for (uint64_t boundary = 1; boundary <= sync_count; ++boundary) {
        const int64_t batch_id = (int64_t)(100 + boundary);
        const int crash_status = run_child_operation(root_path, 46, batch_id, boundary, NULL);
        if (crash_status != YAZA_TEST_CRASH_EXIT) {
            fprintf(stderr, "YazaBase multi-leaf INSERT did not crash at successful-fsync boundary %" PRIu64 ": %d\n",
                    boundary, crash_status);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        const int visible_rows = run_child_operation(root_path, 47, batch_id, 0, NULL);
        if (visible_rows != 0 && visible_rows != 6) {
            fprintf(stderr, "YazaBase multi-leaf INSERT recovery exposed a partial batch at boundary %" PRIu64 ": %d rows/status\n",
                    boundary, visible_rows);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
    }

    (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
    printf("YazaBase grouped multi-leaf INSERT: %" PRIu64
           " successful-fsync crash boundaries recovered atomically across a 600-row split B+Tree\n",
           sync_count);
    return 0;
}

static int run_catalog_migration_crash_matrix(void) {
    char baseline_root[] = "/tmp/yazabase-catalog-migration-probe.XXXXXX";
    if (mkdtemp(baseline_root) == NULL || run_child_operation(baseline_root, 0, 0, 0, NULL) != 0) {
        fprintf(stderr, "YazaBase catalog migration fsync probe setup failed\n");
        if (baseline_root[0] != '\0') (void)nftw(baseline_root, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }
    uint64_t sync_count = 0;
    const int probe_status = run_child_operation(baseline_root, 53, 0, UINT64_MAX, &sync_count);
    (void)nftw(baseline_root, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
    if (probe_status != 0 || sync_count == 0 || sync_count > YAZA_TEST_MAX_SYNC_BOUNDARIES) {
        fprintf(stderr, "YazaBase catalog migration reported invalid baseline status/count: %d/%" PRIu64 "\n", probe_status, sync_count);
        return 1;
    }

    for (uint64_t boundary = 1; boundary <= sync_count; ++boundary) {
        char root_path[] = "/tmp/yazabase-catalog-migration-boundary.XXXXXX";
        if (mkdtemp(root_path) == NULL || run_child_operation(root_path, 0, 0, 0, NULL) != 0) {
            fprintf(stderr, "YazaBase catalog migration boundary setup failed at fsync %" PRIu64 "\n", boundary);
            if (root_path[0] != '\0') (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        const int crash_status = run_child_operation(root_path, 53, 0, boundary, NULL);
        if (crash_status != YAZA_TEST_CRASH_EXIT) {
            fprintf(stderr, "YazaBase catalog migration did not crash after fsync %" PRIu64 ": %d\n", boundary, crash_status);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        const int verify_status = run_child_operation(root_path, 54, 0, 0, NULL);
        if (verify_status != 0) {
            fprintf(stderr, "YazaBase catalog migration recovery failed at fsync %" PRIu64 ": %d\n", boundary, verify_status);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
    }
    printf("YazaBase v1-to-v2 catalog migration recovered to a complete legacy or v2 catalog across %" PRIu64 " successful-fsync crash boundaries\n", sync_count);
    return 0;
}

static int foreign_key_prepare_database(const char *root_path) {
    return run_child_operation(root_path, 0, 0, 0, NULL) == 0 &&
           run_child_operation(root_path, 62, 0, 0, NULL) == 0;
}

static int run_foreign_key_crash_matrix(void) {
    const int64_t probe_id = 4300000;
    char probe_root[] = "/tmp/yazabase-foreign-key-create-probe.XXXXXX";
    uint64_t sync_count = 0;
    if (mkdtemp(probe_root) == NULL || !foreign_key_prepare_database(probe_root)) {
        fprintf(stderr, "YazaBase FOREIGN KEY CREATE crash probe setup failed\n");
        if (probe_root[0] != '\0') (void)nftw(probe_root, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }
    const int probe_status = run_child_operation(probe_root, 63, probe_id, UINT64_MAX, &sync_count);
    const int probe_verify = run_child_operation(probe_root, 64, probe_id, 0, NULL);
    (void)nftw(probe_root, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
    if (probe_status != 0 || probe_verify != 1 || sync_count == 0 ||
        sync_count > YAZA_TEST_MAX_SYNC_BOUNDARIES) {
        fprintf(stderr, "YazaBase FOREIGN KEY CREATE probe returned invalid status/state/fsync count: %d/%d/%" PRIu64 "\n",
                probe_status, probe_verify, sync_count);
        return 1;
    }

    for (uint64_t boundary = 1; boundary <= sync_count; ++boundary) {
        char root_path[] = "/tmp/yazabase-foreign-key-create-boundary.XXXXXX";
        const int64_t child_id = probe_id + (int64_t)boundary;
        if (mkdtemp(root_path) == NULL || !foreign_key_prepare_database(root_path)) {
            fprintf(stderr, "YazaBase FOREIGN KEY CREATE setup failed at fsync boundary %" PRIu64 "\n", boundary);
            if (root_path[0] != '\0') (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        const int crash_status = run_child_operation(root_path, 63, child_id, boundary, NULL);
        if (crash_status != YAZA_TEST_CRASH_EXIT) {
            fprintf(stderr, "YazaBase FOREIGN KEY CREATE did not crash after successful fsync boundary %" PRIu64 ": %d\n",
                    boundary, crash_status);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        const int recovered_state = run_child_operation(root_path, 64, child_id, 0, NULL);
        if (recovered_state != 0 && recovered_state != 1) {
            fprintf(stderr, "YazaBase FOREIGN KEY CREATE recovery exposed invalid/partial state at fsync boundary %" PRIu64 ": %d\n",
                    boundary, recovered_state);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
    }

    printf("YazaBase FOREIGN KEY CREATE recovered to complete absent/present schema states across %" PRIu64
           " successful-fsync crash boundaries; present states enforced child lookup and RESTRICT\n", sync_count);
    return 0;
}

static int view_prepare_database(const char *root_path, int64_t view_id, int create_view) {
    if (run_child_operation(root_path, 0, 0, 0, NULL) != 0 ||
        run_child_operation(root_path, 1, 0, 0, NULL) != 0) return 0;
    if (create_view && run_child_operation(root_path, 55, view_id, 0, NULL) != 0) return 0;
    return 1;
}

static int run_view_crash_matrix(void) {
    const int64_t create_view_id = 4100000;
    char create_probe_root[] = "/tmp/yazabase-view-create-probe.XXXXXX";
    uint64_t create_sync_count = 0;
    if (mkdtemp(create_probe_root) == NULL ||
        !view_prepare_database(create_probe_root, create_view_id, 0)) {
        fprintf(stderr, "YazaBase CREATE VIEW crash probe setup failed\n");
        if (create_probe_root[0] != '\0') (void)nftw(create_probe_root, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }
    const int create_probe_status = run_child_operation(create_probe_root, 55, create_view_id,
                                                        UINT64_MAX, &create_sync_count);
    const int create_probe_verify = run_child_operation(create_probe_root, 56, create_view_id, 0, NULL);
    (void)nftw(create_probe_root, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
    if (create_probe_status != 0 || create_probe_verify != 0 || create_sync_count == 0 ||
        create_sync_count > YAZA_TEST_MAX_SYNC_BOUNDARIES) {
        fprintf(stderr, "YazaBase CREATE VIEW reported invalid probe status/recovery/fsync count: %d/%d/%" PRIu64 "\n",
                create_probe_status, create_probe_verify, create_sync_count);
        return 1;
    }

    for (uint64_t boundary = 1; boundary <= create_sync_count; ++boundary) {
        char root_path[] = "/tmp/yazabase-view-create-boundary.XXXXXX";
        const int64_t view_id = create_view_id + (int64_t)boundary;
        if (mkdtemp(root_path) == NULL || !view_prepare_database(root_path, view_id, 0)) {
            fprintf(stderr, "YazaBase CREATE VIEW setup failed at fsync boundary %" PRIu64 "\n", boundary);
            if (root_path[0] != '\0') (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        const int crash_status = run_child_operation(root_path, 55, view_id, boundary, NULL);
        if (crash_status != YAZA_TEST_CRASH_EXIT) {
            fprintf(stderr, "YazaBase CREATE VIEW did not crash after successful fsync boundary %" PRIu64 ": %d\n",
                    boundary, crash_status);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        const int visible = run_child_operation(root_path, 56, view_id, 0, NULL);
        if (visible != 0 && visible != 1) {
            fprintf(stderr, "YazaBase CREATE VIEW recovery exposed invalid/partial catalog state at boundary %" PRIu64 ": %d\n",
                    boundary, visible);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
    }

    const int64_t drop_view_id = 4200000;
    char drop_probe_root[] = "/tmp/yazabase-view-drop-probe.XXXXXX";
    uint64_t drop_sync_count = 0;
    if (mkdtemp(drop_probe_root) == NULL ||
        !view_prepare_database(drop_probe_root, drop_view_id, 1)) {
        fprintf(stderr, "YazaBase DROP VIEW crash probe setup failed\n");
        if (drop_probe_root[0] != '\0') (void)nftw(drop_probe_root, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }
    const int drop_probe_status = run_child_operation(drop_probe_root, 57, drop_view_id,
                                                      UINT64_MAX, &drop_sync_count);
    const int drop_probe_verify = run_child_operation(drop_probe_root, 56, drop_view_id, 0, NULL);
    (void)nftw(drop_probe_root, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
    if (drop_probe_status != 0 || drop_probe_verify != 1 || drop_sync_count == 0 ||
        drop_sync_count > YAZA_TEST_MAX_SYNC_BOUNDARIES) {
        fprintf(stderr, "YazaBase DROP VIEW reported invalid probe status/recovery/fsync count: %d/%d/%" PRIu64 "\n",
                drop_probe_status, drop_probe_verify, drop_sync_count);
        return 1;
    }

    for (uint64_t boundary = 1; boundary <= drop_sync_count; ++boundary) {
        char root_path[] = "/tmp/yazabase-view-drop-boundary.XXXXXX";
        if (mkdtemp(root_path) == NULL || !view_prepare_database(root_path, drop_view_id, 1)) {
            fprintf(stderr, "YazaBase DROP VIEW setup failed at fsync boundary %" PRIu64 "\n", boundary);
            if (root_path[0] != '\0') (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        const int crash_status = run_child_operation(root_path, 57, drop_view_id, boundary, NULL);
        if (crash_status != YAZA_TEST_CRASH_EXIT) {
            fprintf(stderr, "YazaBase DROP VIEW did not crash after successful fsync boundary %" PRIu64 ": %d\n",
                    boundary, crash_status);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        const int visible = run_child_operation(root_path, 56, drop_view_id, 0, NULL);
        if (visible != 0 && visible != 1) {
            fprintf(stderr, "YazaBase DROP VIEW recovery exposed invalid/partial catalog state at boundary %" PRIu64 ": %d\n",
                    boundary, visible);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
    }

    printf("YazaBase CREATE VIEW %" PRIu64 " and DROP VIEW %" PRIu64
           " successful-fsync crash boundaries recovered to complete before/after catalog states\n",
           create_sync_count, drop_sync_count);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "multi-leaf-insert") == 0)
        return run_multi_leaf_insert_crash_matrix();
    if (argc == 2 && strcmp(argv[1], "catalog-migration") == 0)
        return run_catalog_migration_crash_matrix();
    if (argc == 2 && strcmp(argv[1], "views") == 0)
        return run_view_crash_matrix();
    if (argc == 2 && strcmp(argv[1], "foreign-key") == 0)
        return run_foreign_key_crash_matrix();
    if (argc == 2 && strcmp(argv[1], "range-delete") == 0)
        return run_range_delete_crash_matrix(0);
    if (argc == 3 && strcmp(argv[1], "range-delete-boundary") == 0) {
        char *end = NULL;
        errno = 0;
        const unsigned long long boundary = strtoull(argv[2], &end, 10);
        if (errno != 0 || end == argv[2] || *end != '\0' || boundary == 0) {
            fprintf(stderr, "invalid range DELETE boundary: %s\n", argv[2]);
            return 2;
        }
        return run_range_delete_crash_matrix((uint64_t)boundary);
    }
    if (argc == 2 && strcmp(argv[1], "overflow") == 0)
        return run_overflow_crash_matrix();
    if (argc == 2 && strcmp(argv[1], "overflow-drop") == 0)
        return run_overflow_drop_crash_matrix();
    if (argc == 2 && strcmp(argv[1], "secondary-index") == 0)
        return run_secondary_index_crash_matrix();
    if (argc == 2 && strcmp(argv[1], "unique-index") == 0)
        return run_unique_index_crash_matrix();
    if (argc == 2 && strcmp(argv[1], "alter") == 0)
        return run_alter_crash_matrix();
    if (argc == 3 && strcmp(argv[1], "secondary-index-drop-boundary") == 0) {
        char *end = NULL;
        errno = 0;
        const unsigned long long boundary = strtoull(argv[2], &end, 10);
        if (errno != 0 || end == argv[2] || *end != '\0') {
            fprintf(stderr, "invalid secondary-index DROP boundary: %s\n", argv[2]);
            return 2;
        }
        return run_secondary_index_drop_boundary((uint64_t)boundary);
    }
    if (argc == 3 && strcmp(argv[1], "alter-boundary") == 0) {
        char *end = NULL;
        errno = 0;
        const unsigned long long boundary = strtoull(argv[2], &end, 10);
        if (errno != 0 || end == argv[2] || *end != '\0') {
            fprintf(stderr, "invalid ALTER TABLE boundary: %s\n", argv[2]);
            return 2;
        }
        return run_alter_boundary((uint64_t)boundary);
    }
    if (argc > 2 || (argc == 2 && strcmp(argv[1], "full") != 0)) {
        fprintf(stderr, "usage: %s [full|overflow|overflow-drop|secondary-index|unique-index|alter|range-delete|range-delete-boundary N|multi-leaf-insert|catalog-migration|views|foreign-key|secondary-index-drop-boundary N|alter-boundary N]\n", argv[0]);
        return 2;
    }
    char root_path[] = "/tmp/yazabase-sync-recovery.XXXXXX";
    if (mkdtemp(root_path) == NULL) {
        perror("mkdtemp");
        return 1;
    }

    if (run_child_operation(root_path, 0, 0, 0, NULL) != 0 ||
        run_child_operation(root_path, 1, 0, 0, NULL) != 0 ||
        run_child_operation(root_path, 2, 1, 0, NULL) != 0) {
        fprintf(stderr, "YazaBase sync recovery fixture setup failed\n");
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }

    uint64_t insert_sync_boundary_count = 0;
    if (run_child_operation(root_path, 2, 2, UINT64_MAX, &insert_sync_boundary_count) != 0 ||
        insert_sync_boundary_count == 0 || insert_sync_boundary_count > YAZA_TEST_MAX_SYNC_BOUNDARIES) {
        fprintf(stderr, "YazaBase INSERT reported an invalid successful-fsync boundary count: %" PRIu64 "\n", insert_sync_boundary_count);
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }

    for (uint64_t boundary = 1; boundary <= insert_sync_boundary_count; ++boundary) {
        const int64_t value = (int64_t)(1000 + boundary);
        const int crash_status = run_child_operation(root_path, 2, value, boundary, NULL);
        if (crash_status != YAZA_TEST_CRASH_EXIT) {
            fprintf(stderr, "YazaBase INSERT did not crash after successful fsync boundary %" PRIu64 ": status %d\n", boundary, crash_status);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }

        const int visible_rows = run_child_operation(root_path, 3, value, 0, NULL);
        if (visible_rows != 0 && visible_rows != 1) {
            fprintf(stderr, "YazaBase recovery failed or produced a duplicate after fsync boundary %" PRIu64 ": status %d\n", boundary, visible_rows);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
    }

    uint64_t multi_insert_sync_boundary_count = 0;
    if (run_child_operation(root_path, 10, 800000, UINT64_MAX, &multi_insert_sync_boundary_count) != 0 ||
        multi_insert_sync_boundary_count == 0 || multi_insert_sync_boundary_count > YAZA_TEST_MAX_SYNC_BOUNDARIES ||
        run_child_operation(root_path, 11, 800000, 0, NULL) != 3) {
        fprintf(stderr, "YazaBase multi-row INSERT reported an invalid successful-fsync boundary count: %" PRIu64 "\n", multi_insert_sync_boundary_count);
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }

    for (uint64_t boundary = 1; boundary <= multi_insert_sync_boundary_count; ++boundary) {
        const int64_t first_key = (int64_t)(920000 + boundary * 10);
        const int crash_status = run_child_operation(root_path, 10, first_key, boundary, NULL);
        if (crash_status != YAZA_TEST_CRASH_EXIT) {
            fprintf(stderr, "YazaBase multi-row INSERT did not crash after successful fsync boundary %" PRIu64 ": status %d\n", boundary, crash_status);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }

        const int visible_rows = run_child_operation(root_path, 11, first_key, 0, NULL);
        if (visible_rows != 0 && visible_rows != 3) {
            fprintf(stderr, "YazaBase multi-row INSERT recovery after fsync boundary %" PRIu64 " exposed a partial batch: rows=%d\n",
                    boundary, visible_rows);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
    }

    uint64_t update_sync_boundary_count = 0;
    if (run_child_operation(root_path, 4, 5000, UINT64_MAX, &update_sync_boundary_count) != 0 ||
        update_sync_boundary_count == 0 || update_sync_boundary_count > YAZA_TEST_MAX_SYNC_BOUNDARIES ||
        run_child_operation(root_path, 5, 5000, 0, NULL) != 0) {
        fprintf(stderr, "YazaBase UPDATE reported an invalid successful-fsync boundary count: %" PRIu64 "\n", update_sync_boundary_count);
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }

    int64_t expected_update_value = 5000;
    for (uint64_t boundary = 1; boundary <= update_sync_boundary_count; ++boundary) {
        const int64_t next_value = (int64_t)(6000 + boundary);
        const int crash_status = run_child_operation(root_path, 4, next_value, boundary, NULL);
        if (crash_status != YAZA_TEST_CRASH_EXIT) {
            fprintf(stderr, "YazaBase UPDATE did not crash after successful fsync boundary %" PRIu64 ": status %d\n", boundary, crash_status);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }

        const int old_value_visible = run_child_operation(root_path, 5, expected_update_value, 0, NULL);
        const int new_value_visible = run_child_operation(root_path, 5, next_value, 0, NULL);
        if (old_value_visible == 0 && new_value_visible == 1) continue;
        if (old_value_visible == 1 && new_value_visible == 0) {
            expected_update_value = next_value;
            continue;
        }
        fprintf(stderr, "YazaBase UPDATE recovery after fsync boundary %" PRIu64 " did not expose exactly the before or committed after value (old=%d new=%d)\n",
                boundary, old_value_visible, new_value_visible);
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }

    uint64_t delete_sync_boundary_count = 0;
    const int64_t delete_count_probe_key = 200000;
    if (run_child_operation(root_path, 2, delete_count_probe_key, 0, NULL) != 0 ||
        run_child_operation(root_path, 6, delete_count_probe_key, UINT64_MAX, &delete_sync_boundary_count) != 0 ||
        delete_sync_boundary_count == 0 || delete_sync_boundary_count > YAZA_TEST_MAX_SYNC_BOUNDARIES ||
        run_child_operation(root_path, 7, delete_count_probe_key, 0, NULL) != 0) {
        fprintf(stderr, "YazaBase DELETE reported an invalid successful-fsync boundary count: %" PRIu64 "\n", delete_sync_boundary_count);
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }

    for (uint64_t boundary = 1; boundary <= delete_sync_boundary_count; ++boundary) {
        const int64_t delete_key = (int64_t)(3000 + boundary);
        if (run_child_operation(root_path, 2, delete_key, 0, NULL) != 0) {
            fprintf(stderr, "YazaBase DELETE could not prepare row for fsync boundary %" PRIu64 "\n", boundary);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        const int crash_status = run_child_operation(root_path, 6, delete_key, boundary, NULL);
        if (crash_status != YAZA_TEST_CRASH_EXIT) {
            fprintf(stderr, "YazaBase DELETE did not crash after successful fsync boundary %" PRIu64 ": status %d\n", boundary, crash_status);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        const int visible_rows = run_child_operation(root_path, 7, delete_key, 0, NULL);
        if (visible_rows != 0 && visible_rows != 1) {
            fprintf(stderr, "YazaBase DELETE recovery after fsync boundary %" PRIu64 " returned an invalid row count/status: %d\n", boundary, visible_rows);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
    }

    uint64_t create_sync_boundary_count = 0;
    if (run_child_operation(root_path, 8, 700000, UINT64_MAX, &create_sync_boundary_count) != 0 ||
        create_sync_boundary_count == 0 || create_sync_boundary_count > YAZA_TEST_MAX_SYNC_BOUNDARIES ||
        run_child_operation(root_path, 9, 700000, 0, NULL) != 0) {
        fprintf(stderr, "YazaBase CREATE TABLE reported an invalid successful-fsync boundary count: %" PRIu64 "\n", create_sync_boundary_count);
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }

    for (uint64_t boundary = 1; boundary <= create_sync_boundary_count; ++boundary) {
        const int64_t table_id = (int64_t)(710000 + boundary);
        const int crash_status = run_child_operation(root_path, 8, table_id, boundary, NULL);
        if (crash_status != YAZA_TEST_CRASH_EXIT) {
            fprintf(stderr, "YazaBase CREATE TABLE did not crash after successful fsync boundary %" PRIu64 ": status %d\n", boundary, crash_status);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        const int table_exists_status = run_child_operation(root_path, 9, table_id, 0, NULL);
        if (table_exists_status != 0 && table_exists_status != 1) {
            fprintf(stderr, "YazaBase CREATE TABLE recovery after fsync boundary %" PRIu64 " exposed a partial table/catalog state: status %d\n",
                    boundary, table_exists_status);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
    }

    uint64_t drop_sync_boundary_count = 0;
    const int64_t drop_probe_id = 800000;
    if (run_child_operation(root_path, 8, drop_probe_id, 0, NULL) != 0 ||
        run_child_operation(root_path, 15, drop_probe_id, UINT64_MAX, &drop_sync_boundary_count) != 0 ||
        drop_sync_boundary_count == 0 || drop_sync_boundary_count > YAZA_TEST_MAX_SYNC_BOUNDARIES ||
        run_child_operation(root_path, 9, drop_probe_id, 0, NULL) != 1) {
        fprintf(stderr, "YazaBase DROP TABLE reported an invalid successful-fsync boundary count: %" PRIu64 "\n", drop_sync_boundary_count);
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }

    for (uint64_t boundary = 1; boundary <= drop_sync_boundary_count; ++boundary) {
        const int64_t table_id = (int64_t)(810000 + boundary);
        if (run_child_operation(root_path, 8, table_id, 0, NULL) != 0) {
            fprintf(stderr, "YazaBase DROP TABLE could not prepare table for fsync boundary %" PRIu64 "\n", boundary);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        const int crash_status = run_child_operation(root_path, 15, table_id, boundary, NULL);
        if (crash_status != YAZA_TEST_CRASH_EXIT) {
            fprintf(stderr, "YazaBase DROP TABLE did not crash after successful fsync boundary %" PRIu64 ": status %d\n", boundary, crash_status);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        const int table_exists_status = run_child_operation(root_path, 9, table_id, 0, NULL);
        if (table_exists_status != 0 && table_exists_status != 1) {
            fprintf(stderr, "YazaBase DROP TABLE recovery after fsync boundary %" PRIu64 " exposed a partial catalog state: status %d\n",
                    boundary, table_exists_status);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
    }

    uint64_t alter_sync_boundary_count = 0;
    const int64_t alter_probe_id = 850000;
    if (run_child_operation(root_path, 58, alter_probe_id, 0, NULL) != 0 ||
        run_child_operation(root_path, 16, alter_probe_id, UINT64_MAX, &alter_sync_boundary_count) != 0 ||
        alter_sync_boundary_count == 0 || alter_sync_boundary_count > YAZA_TEST_MAX_SYNC_BOUNDARIES ||
        run_child_operation(root_path, 17, alter_probe_id, 0, NULL) != 0) {
        fprintf(stderr, "YazaBase ALTER TABLE reported an invalid successful-fsync boundary count: %" PRIu64 "\n", alter_sync_boundary_count);
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }

    for (uint64_t boundary = 1; boundary <= alter_sync_boundary_count; ++boundary) {
        const int64_t table_id = (int64_t)(860000 + boundary);
        if (run_child_operation(root_path, 58, table_id, 0, NULL) != 0) {
            fprintf(stderr, "YazaBase ALTER TABLE could not prepare table for fsync boundary %" PRIu64 "\n", boundary);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        const int crash_status = run_child_operation(root_path, 16, table_id, boundary, NULL);
        if (crash_status != YAZA_TEST_CRASH_EXIT) {
            fprintf(stderr, "YazaBase ALTER TABLE did not crash after successful fsync boundary %" PRIu64 ": status %d\n", boundary, crash_status);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        const int schema_status = run_child_operation(root_path, 17, table_id, 0, NULL);
        if (schema_status != 0 && schema_status != 1) {
            fprintf(stderr, "YazaBase ALTER TABLE recovery after fsync boundary %" PRIu64 " exposed a partial schema state: status %d\n",
                    boundary, schema_status);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
    }

    const int64_t partial_wal_key = 900001;
    const int partial_write_status = run_child_operation_with_faults(root_path, 2, partial_wal_key, 0, 256, 0, NULL);
    const int partial_write_rows = run_child_operation(root_path, 3, partial_wal_key, 0, NULL);
    if (partial_write_status == 0 || partial_write_rows != 0) {
        fprintf(stderr, "YazaBase partial WAL fault status=%d recovered row count/status=%d\n", partial_write_status, partial_write_rows);
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }

    const int64_t failed_sync_key = 900002;
    const int failed_sync_status = run_child_operation_with_faults(root_path, 2, failed_sync_key, 0, -1,
                                                                    YAZA_VFS_V1_TEST_FAIL_FSYNC, NULL);
    const int failed_sync_rows = run_child_operation(root_path, 3, failed_sync_key, 0, NULL);
    if (failed_sync_status == 0 || failed_sync_rows != 0) {
        fprintf(stderr, "YazaBase fsync EIO fault status=%d recovered row count/status=%d\n", failed_sync_status, failed_sync_rows);
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }

    char growth_probe_path[] = "/tmp/yazabase-growth-fsync-probe.XXXXXX";
    if (mkdtemp(growth_probe_path) == NULL ||
        run_child_operation(growth_probe_path, 12, 0, 0, NULL) != 0) {
        fprintf(stderr, "YazaBase capacity-growth crash fixture setup failed\n");
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        (void)nftw(growth_probe_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }

    uint64_t capacity_growth_sync_boundary_count = 0;
    if (run_child_operation(growth_probe_path, 13, 0, UINT64_MAX,
                            &capacity_growth_sync_boundary_count) != 0 ||
        capacity_growth_sync_boundary_count == 0 ||
        capacity_growth_sync_boundary_count > YAZA_TEST_MAX_SYNC_BOUNDARIES ||
        run_child_operation(growth_probe_path, 14, 0, 0, NULL) != 0) {
        fprintf(stderr, "YazaBase capacity growth reported an invalid successful-fsync boundary count: %" PRIu64 "\n",
                capacity_growth_sync_boundary_count);
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        (void)nftw(growth_probe_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }
    (void)nftw(growth_probe_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);

    for (uint64_t boundary = 1; boundary <= capacity_growth_sync_boundary_count; ++boundary) {
        char growth_crash_path[] = "/tmp/yazabase-growth-fsync-crash.XXXXXX";
        if (mkdtemp(growth_crash_path) == NULL ||
            run_child_operation(growth_crash_path, 12, 0, 0, NULL) != 0) {
            fprintf(stderr, "YazaBase capacity-growth fixture setup failed at boundary %" PRIu64 "\n", boundary);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            (void)nftw(growth_crash_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        const int crash_status = run_child_operation(growth_crash_path, 13, 0, boundary, NULL);
        if (crash_status != YAZA_TEST_CRASH_EXIT) {
            fprintf(stderr, "YazaBase capacity growth did not crash after successful fsync boundary %" PRIu64 ": status %d\n",
                    boundary, crash_status);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            (void)nftw(growth_crash_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        const int verify_status = run_child_operation(growth_crash_path, 14, 0, 0, NULL);
        if (verify_status != 0) {
            fprintf(stderr, "YazaBase capacity growth recovery failed after fsync boundary %" PRIu64 ": status %d\n",
                    boundary, verify_status);
            (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            (void)nftw(growth_crash_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
            return 1;
        }
        (void)nftw(growth_crash_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
    }

    if (run_child_operation(root_path, 3, 1, 0, NULL) != 1 ||
        run_child_operation(root_path, 3, 2, 0, NULL) != 1 ||
        run_child_operation(root_path, 2, partial_wal_key, 0, NULL) != 0 ||
        run_child_operation(root_path, 3, partial_wal_key, 0, NULL) != 1 ||
        run_child_operation(root_path, 2, failed_sync_key, 0, NULL) != 0 ||
        run_child_operation(root_path, 3, failed_sync_key, 0, NULL) != 1 ||
        run_child_operation(root_path, 2, 999999, 0, NULL) != 0 ||
        run_child_operation(root_path, 3, 999999, 0, NULL) != 1) {
        fprintf(stderr, "YazaBase database did not retain committed rows and accept later writes after recovery\n");
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }

    if (run_secondary_index_crash_matrix() != 0) {
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }
    if (run_overflow_crash_matrix() != 0) {
        (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
        return 1;
    }

    (void)nftw(root_path, remove_tree_entry, 32, FTW_DEPTH | FTW_PHYS);
    printf("YazaBase recovery: single-row INSERT %" PRIu64 ", multi-row INSERT %" PRIu64 ", UPDATE %" PRIu64 ", DELETE %" PRIu64 ", CREATE TABLE %" PRIu64 ", DROP TABLE %" PRIu64 ", ALTER TABLE %" PRIu64 ", capacity growth %" PRIu64 " fsync-boundary crashes plus partial-WAL and fsync-EIO injection; database remained usable\n",
           insert_sync_boundary_count, multi_insert_sync_boundary_count, update_sync_boundary_count, delete_sync_boundary_count, create_sync_boundary_count,
           drop_sync_boundary_count, alter_sync_boundary_count, capacity_growth_sync_boundary_count);
    return 0;
}

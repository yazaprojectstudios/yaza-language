/*
 * YazaBase C API — Standalone Embedded Database Engine
 * High-performance, crash-safe, memory-bounded SQL & KV engine.
 * Usable from C/C++, Python, Node.js, Go, Rust, Java, and other runtimes.
 */

#ifndef YAZABASE_H
#define YAZABASE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) || defined(__CYGWIN__)
  #ifdef YAZABASE_BUILD_DLL
    #define YAZA_API __declspec(dllexport)
  #else
    #define YAZA_API __declspec(dllimport)
  #endif
#else
  #define YAZA_API __attribute__((visibility("default")))
#endif

/* Return codes */
#define YAZABASE_OK           0
#define YAZABASE_ERROR        1
#define YAZABASE_BUSY         2
#define YAZABASE_CORRUPT      3
#define YAZABASE_NOTFOUND     4
#define YAZABASE_NOMEM        5
#define YAZABASE_MISUSE       6

/* Opaque handles */
typedef struct yazabase yazabase_t;
typedef struct yazabase_result yazabase_result_t;
typedef struct yazabase_stmt yazabase_stmt_t;

/* Version and Metadata */
YAZA_API const char *yazabase_version(void);
YAZA_API int yazabase_version_number(void);

/* Lifecycle */
YAZA_API int yazabase_open(const char *dir_path, const char *db_name, yazabase_t **out_db);
YAZA_API int yazabase_close(yazabase_t *db);
YAZA_API const char *yazabase_errmsg(yazabase_t *db);

/* Direct Query Execution */
YAZA_API int yazabase_exec(yazabase_t *db, const char *sql, yazabase_result_t **out_result);

/* Result Inspection */
YAZA_API int yazabase_result_row_count(const yazabase_result_t *res);
YAZA_API int yazabase_result_column_count(const yazabase_result_t *res);
YAZA_API const char *yazabase_result_column_name(const yazabase_result_t *res, int col_idx);
YAZA_API const char *yazabase_result_value(const yazabase_result_t *res, int row_idx, int col_idx);
YAZA_API int yazabase_result_is_null(const yazabase_result_t *res, int row_idx, int col_idx);
YAZA_API int64_t yazabase_result_affected_rows(const yazabase_result_t *res);
YAZA_API void yazabase_free_result(yazabase_result_t *res);

/* Direct Binary YBIF Result Encoding (zero JSON intermediate) */
YAZA_API int yazabase_result_encode_ybif(const yazabase_result_t *res,
                                         uint8_t **out_buf,
                                         size_t *out_len);
YAZA_API void yazabase_free_buffer(void *buf);

/* Backup and Maintenance */
YAZA_API int yazabase_backup(yazabase_t *db, const char *dest_dir);
YAZA_API int yazabase_checkpoint(yazabase_t *db);

#ifdef __cplusplus
}
#endif

#endif /* YAZABASE_H */

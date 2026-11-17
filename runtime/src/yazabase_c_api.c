/*
 * YazaBase C API Implementation
 * Provides standard C interface for embedding YazaBase into other languages.
 */

#include "../include/yazabase.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define YAZABASE_VERSION_STR "1.0.0"
#define YAZABASE_VERSION_NUM 10000

struct yazabase {
    char dir[1024];
    char name[256];
    char last_error[1024];
};

struct yazabase_result {
    int row_count;
    int col_count;
    char **col_names;
    char ***cells;
    int **null_flags;
    int64_t affected_rows;
};

const char *yazabase_version(void) {
    return YAZABASE_VERSION_STR;
}

int yazabase_version_number(void) {
    return YAZABASE_VERSION_NUM;
}

int yazabase_open(const char *dir_path, const char *db_name, yazabase_t **out_db) {
    if (!dir_path || !db_name || !out_db) {
        return YAZABASE_MISUSE;
    }
    yazabase_t *db = (yazabase_t *)calloc(1, sizeof(yazabase_t));
    if (!db) {
        return YAZABASE_NOMEM;
    }
    strncpy(db->dir, dir_path, sizeof(db->dir) - 1);
    strncpy(db->name, db_name, sizeof(db->name) - 1);
    db->last_error[0] = '\0';
    *out_db = db;
    return YAZABASE_OK;
}

int yazabase_close(yazabase_t *db) {
    if (!db) return YAZABASE_OK;
    free(db);
    return YAZABASE_OK;
}

const char *yazabase_errmsg(yazabase_t *db) {
    if (!db) return "Database handle is NULL";
    return db->last_error;
}

int yazabase_result_row_count(const yazabase_result_t *res) {
    return res ? res->row_count : 0;
}

int yazabase_result_column_count(const yazabase_result_t *res) {
    return res ? res->col_count : 0;
}

const char *yazabase_result_column_name(const yazabase_result_t *res, int col_idx) {
    if (!res || col_idx < 0 || col_idx >= res->col_count) return "";
    return res->col_names[col_idx] ? res->col_names[col_idx] : "";
}

const char *yazabase_result_value(const yazabase_result_t *res, int row_idx, int col_idx) {
    if (!res || row_idx < 0 || row_idx >= res->row_count || col_idx < 0 || col_idx >= res->col_count) {
        return "";
    }
    if (res->null_flags[row_idx][col_idx]) return NULL;
    return res->cells[row_idx][col_idx] ? res->cells[row_idx][col_idx] : "";
}

int yazabase_result_is_null(const yazabase_result_t *res, int row_idx, int col_idx) {
    if (!res || row_idx < 0 || row_idx >= res->row_count || col_idx < 0 || col_idx >= res->col_count) {
        return 1;
    }
    return res->null_flags[row_idx][col_idx];
}

int64_t yazabase_result_affected_rows(const yazabase_result_t *res) {
    return res ? res->affected_rows : 0;
}

void yazabase_free_result(yazabase_result_t *res) {
    if (!res) return;
    if (res->col_names) {
        for (int c = 0; c < res->col_count; c++) {
            free(res->col_names[c]);
        }
        free(res->col_names);
    }
    if (res->cells && res->null_flags) {
        for (int r = 0; r < res->row_count; r++) {
            for (int c = 0; c < res->col_count; c++) {
                free(res->cells[r][c]);
            }
            free(res->cells[r]);
            free(res->null_flags[r]);
        }
        free(res->cells);
        free(res->null_flags);
    }
    free(res);
}

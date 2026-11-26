/*
 * YazaBase C API Implementation
 * Provides standard C interface for embedding YazaBase into other languages.
 */

#include "../include/yazabase.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define YAZABASE_VERSION_STR "1.1.0"
#define YAZABASE_VERSION_NUM 10100

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
    *out_db = NULL;
    yazabase_t *db = (yazabase_t *)calloc(1, sizeof(yazabase_t));
    if (!db) {
        return YAZABASE_NOMEM;
    }
    const int dir_len = snprintf(db->dir, sizeof(db->dir), "%s", dir_path);
    const int name_len = snprintf(db->name, sizeof(db->name), "%s", db_name);
    if (dir_len < 0 || (size_t)dir_len >= sizeof(db->dir) ||
        name_len < 0 || (size_t)name_len >= sizeof(db->name)) {
        free(db);
        return YAZABASE_MISUSE;
    }
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
    if (!res || !res->col_names || col_idx < 0 || col_idx >= res->col_count) return "";
    return res->col_names[col_idx] ? res->col_names[col_idx] : "";
}

const char *yazabase_result_value(const yazabase_result_t *res, int row_idx, int col_idx) {
    if (!res || !res->cells || !res->null_flags ||
        row_idx < 0 || row_idx >= res->row_count || col_idx < 0 || col_idx >= res->col_count) {
        return "";
    }
    if (res->null_flags[row_idx] && res->null_flags[row_idx][col_idx]) return NULL;
    if (!res->cells[row_idx]) return "";
    return res->cells[row_idx][col_idx] ? res->cells[row_idx][col_idx] : "";
}

int yazabase_result_is_null(const yazabase_result_t *res, int row_idx, int col_idx) {
    if (!res || !res->null_flags ||
        row_idx < 0 || row_idx >= res->row_count || col_idx < 0 || col_idx >= res->col_count) {
        return 1;
    }
    if (!res->null_flags[row_idx]) return 1;
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
    if (res->cells) {
        for (int r = 0; r < res->row_count; r++) {
            if (res->cells[r]) {
                for (int c = 0; c < res->col_count; c++) {
                    free(res->cells[r][c]);
                }
                free(res->cells[r]);
            }
        }
        free(res->cells);
    }
    if (res->null_flags) {
        for (int r = 0; r < res->row_count; r++) {
            free(res->null_flags[r]);
        }
        free(res->null_flags);
    }
    free(res);
}

static int ybif_buf_ensure(uint8_t **buf, size_t *cap, size_t needed) {
    if (needed <= *cap) {
        return 1;
    }
    size_t new_cap = (*cap == 0) ? 256 : *cap;
    while (new_cap < needed) {
        if (new_cap > (SIZE_MAX / 2)) {
            return 0;
        }
        new_cap *= 2;
    }
    uint8_t *grown = (uint8_t *)realloc(*buf, new_cap);
    if (!grown) {
        return 0;
    }
    *buf = grown;
    *cap = new_cap;
    return 1;
}

static void ybif_write_u32_le(uint8_t *dst, uint32_t v) {
    dst[0] = (uint8_t)(v & 0xFFu);
    dst[1] = (uint8_t)((v >> 8) & 0xFFu);
    dst[2] = (uint8_t)((v >> 16) & 0xFFu);
    dst[3] = (uint8_t)((v >> 24) & 0xFFu);
}

int yazabase_result_encode_ybif(const yazabase_result_t *res,
                                uint8_t **out_buf,
                                size_t *out_len) {
    if (!res || !out_buf || !out_len || res->col_count < 0 || res->row_count < 0) {
        return YAZABASE_MISUSE;
    }
    *out_buf = NULL;
    *out_len = 0;

    /* Pre-compute exact payload size for a single-allocation linear write. */
    size_t total = 6 + 1 + 4 + 4; /* header(6) + tag9(1) + col_count(4) + row_count(4) */
    for (int c = 0; c < res->col_count; c++) {
        const char *name = (res->col_names && res->col_names[c]) ? res->col_names[c] : "";
        size_t nlen = strlen(name);
        if (nlen > UINT32_MAX || total > SIZE_MAX - 4 - nlen) {
            return YAZABASE_NOMEM;
        }
        total += 4 + nlen;
    }
    for (int r = 0; r < res->row_count; r++) {
        for (int c = 0; c < res->col_count; c++) {
            int is_null = (res->null_flags && res->null_flags[r] && res->null_flags[r][c]) ||
                          !res->cells || !res->cells[r] || !res->cells[r][c];
            if (is_null) {
                if (total > SIZE_MAX - 1) return YAZABASE_NOMEM;
                total += 1;
            } else {
                size_t vlen = strlen(res->cells[r][c]);
                if (vlen > UINT32_MAX || total > SIZE_MAX - 5 - vlen) {
                    return YAZABASE_NOMEM;
                }
                total += 5 + vlen;
            }
        }
    }

    uint8_t *buf = NULL;
    size_t cap = 0;
    if (!ybif_buf_ensure(&buf, &cap, total)) {
        return YAZABASE_NOMEM;
    }

    size_t pos = 0;
    memcpy(buf + pos, "YBIF\x01\x00", 6);
    pos += 6;
    buf[pos++] = 9; /* Tag 9: SQL Result */
    ybif_write_u32_le(buf + pos, (uint32_t)res->col_count);
    pos += 4;

    for (int c = 0; c < res->col_count; c++) {
        const char *name = (res->col_names && res->col_names[c]) ? res->col_names[c] : "";
        size_t nlen = strlen(name);
        ybif_write_u32_le(buf + pos, (uint32_t)nlen);
        pos += 4;
        if (nlen > 0) {
            memcpy(buf + pos, name, nlen);
            pos += nlen;
        }
    }

    ybif_write_u32_le(buf + pos, (uint32_t)res->row_count);
    pos += 4;

    for (int r = 0; r < res->row_count; r++) {
        for (int c = 0; c < res->col_count; c++) {
            int is_null = (res->null_flags && res->null_flags[r] && res->null_flags[r][c]) ||
                          !res->cells || !res->cells[r] || !res->cells[r][c];
            if (is_null) {
                buf[pos++] = 0; /* Tag 0: NULL */
            } else {
                const char *val = res->cells[r][c];
                size_t vlen = strlen(val);
                buf[pos++] = 3; /* Tag 3: Text */
                ybif_write_u32_le(buf + pos, (uint32_t)vlen);
                pos += 4;
                if (vlen > 0) {
                    memcpy(buf + pos, val, vlen);
                    pos += vlen;
                }
            }
        }
    }

    *out_buf = buf;
    *out_len = pos;
    return YAZABASE_OK;
}

void yazabase_free_buffer(void *buf) {
    free(buf);
}

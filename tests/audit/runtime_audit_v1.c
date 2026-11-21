#include "yaza_runtime_v1.h"
#include "yazabase.h"

#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    int64_t read_handle;
    int32_t read_ok;
    uint64_t out_len;
} CloseRaceArgs;

static void *net_close_race_reader(void *opaque) {
    CloseRaceArgs *args = (CloseRaceArgs *)opaque;
    YazaBytesV1 out = {NULL, 0};
    char *err = NULL;
    args->read_ok = yaza_net_v1_read(args->read_handle, 64, 5000, &out, &err);
    args->out_len = out.length;
    return NULL;
}

typedef struct {
    int64_t expected;
    int64_t value;
} TaskCtx;

static void sample_task_thunk(void *context, void *out_result) {
    TaskCtx *ctx = (TaskCtx *)context;
    int64_t *res = (int64_t *)out_result;
    *res = ctx->value * 2;
}

static int text_equals_cstr(const YazaTextV1 *t, const char *cstr) {
    const size_t n = strlen(cstr);
    return t != NULL && t->length == n && (n == 0 || memcmp(t->data, cstr, n) == 0);
}

int main(void) {
    yaza_rt_v1_init();

    /* 1. Arena destroy while scope active must fail gracefully and not free memory */
    YazaArenaV1 *worker_arena = yaza_arena_v1_create(256ULL * 1024ULL);
    assert(worker_arena != NULL);
    YazaArenaContextV1 ctx = {0};
    assert(yaza_arena_v1_enter(worker_arena, &ctx) == 1);
    assert(yaza_arena_v1_destroy(worker_arena) == 0);
    assert(yaza_arena_v1_leave(&ctx) == 1);
    assert(yaza_arena_v1_destroy(worker_arena) == 1);

    YazaArenaV1 *scoped_arena = yaza_arena_v1_create(256ULL * 1024ULL);
    assert(scoped_arena != NULL);
    YazaArenaContextV1 sctx = {0};
    assert(yaza_arena_v1_scope_begin(scoped_arena, &sctx) == 1);
    assert(yaza_arena_v1_destroy(scoped_arena) == 0);
    assert(yaza_arena_v1_scope_end(&sctx) == 1);
    assert(yaza_arena_v1_destroy(scoped_arena) == 1);

    /* 2. Fallible float64 formatting must return NULL for NaN/Inf without aborting */
    assert(yaza_rt_v1_try_float64_text(NAN) == NULL);
    assert(yaza_rt_v1_try_float64_text(INFINITY) == NULL);
    assert(yaza_rt_v1_try_float64_text(-INFINITY) == NULL);
    assert(text_equals_cstr(yaza_rt_v1_try_float64_text(3.5), "3.5"));

    /* 3. Env set: allow empty slice, reject embedded NUL */
    const YazaTextV1 *env_key = yaza_rt_v1_text_from_cstr("YAZA_AUDIT_ENV_TEST");
    YazaTextV1 empty_val = {NULL, 0};
    assert(yaza_rt_v1_env_set(env_key, &empty_val) == 0);
    assert(text_equals_cstr(yaza_rt_v1_env_get(env_key), ""));
    const unsigned char nul_bytes[3] = {'a', '\0', 'b'};
    YazaTextV1 nul_val = {nul_bytes, 3};
    assert(yaza_rt_v1_env_set(env_key, &nul_val) == 1);

    /* 4. Process spawning: empty argument must become "" and not truncate argv[] */
    void *arg_list = yaza_list_v1_create_typed("TextList", sizeof(YazaTextV1 *));
    assert(arg_list != NULL);
    YazaTextV1 *a0 = yaza_rt_v1_text_from_cstr("-c");
    YazaTextV1 *a1 = yaza_rt_v1_text_from_cstr("test \"$1\" = \"\" && test \"$2\" = \"second-arg\"");
    YazaTextV1 *a2 = yaza_rt_v1_text_from_cstr("sh");
    YazaTextV1 *a3 = (YazaTextV1 *)yaza_rt_v1_alloc(sizeof(YazaTextV1));
    a3->data = NULL;
    a3->length = 0;
    YazaTextV1 *a4 = yaza_rt_v1_text_from_cstr("second-arg");
    assert(yaza_list_v1_push_typed(arg_list, &a0, "TextList", sizeof(YazaTextV1 *)) == 0);
    assert(yaza_list_v1_push_typed(arg_list, &a1, "TextList", sizeof(YazaTextV1 *)) == 0);
    assert(yaza_list_v1_push_typed(arg_list, &a2, "TextList", sizeof(YazaTextV1 *)) == 0);
    assert(yaza_list_v1_push_typed(arg_list, &a3, "TextList", sizeof(YazaTextV1 *)) == 0);
    assert(yaza_list_v1_push_typed(arg_list, &a4, "TextList", sizeof(YazaTextV1 *)) == 0);
    assert(yaza_rt_v1_process(yaza_rt_v1_text_from_cstr("/bin/sh"), arg_list) == 0);

    /* 5. Net hardening: NUL host rejected, non-pair rejected by pair_peer, close-vs-read race safe */
    char *net_err = NULL;
    int64_t bad_listener = 0;
    assert(yaza_net_v1_listen(&nul_val, 48231, 1, &bad_listener, &net_err) == 0);
    int64_t listener = 0;
    int32_t listen_ok = 0;
    for (int64_t p = 48231; p <= 48260; ++p) {
        if (yaza_net_v1_listen(yaza_rt_v1_text_from_cstr("127.0.0.1"), p, 4, &listener, &net_err) == 1) {
            listen_ok = 1;
            break;
        }
    }
    assert(listen_ok == 1);
    int64_t bogus_peer = 0;
    assert(yaza_net_v1_pair_peer(listener, &bogus_peer, &net_err) == 0);
    assert(yaza_net_v1_close(listener, &net_err) == 1);

    for (int iter = 0; iter < 50; ++iter) {
        int64_t h1 = 0, h2 = 0;
        assert(yaza_net_v1_pair_create(&h1, &net_err) == 1);
        assert(yaza_net_v1_pair_peer(h1, &h2, &net_err) == 1);
        CloseRaceArgs rargs = {h1, -1, 999};
        pthread_t tid;
        assert(pthread_create(&tid, NULL, net_close_race_reader, &rargs) == 0);
        struct timespec ts = {0, 2000000L};
        nanosleep(&ts, NULL);
        assert(yaza_net_v1_close(h1, &net_err) == 1);
        assert(pthread_join(tid, NULL) == 0);
        assert(rargs.read_ok == 0 || rargs.out_len == 0);
        assert(yaza_net_v1_close(h2, &net_err) == 1);
    }

    /* 6. Atomic add near INT64_MAX wraps without signed overflow UB */
    int64_t ah = yaza_rt_v1_atomic_handle_create(INT64_MAX);
    assert(ah > 0);
    int64_t next = yaza_rt_v1_atomic_handle_add(ah, 1);
    assert(next == INT64_MIN);
    assert(yaza_rt_v1_atomic_handle_load(ah) == INT64_MIN);

    /* 7. Async task spawn + await & channel round-trip */
    TaskCtx *tctx = (TaskCtx *)yaza_rt_v1_task_ctx_alloc(sizeof(TaskCtx));
    assert(tctx != NULL);
    tctx->value = 21;
    YazaTaskV1 *task = yaza_rt_v1_task_spawn(sample_task_thunk, tctx, sizeof(int64_t), 0);
    assert(task != NULL);
    int64_t task_out = 0;
    yaza_rt_v1_task_await(task, &task_out, sizeof(int64_t));
    assert(task_out == 42);

    int64_t ch = yaza_rt_v1_channel_create(4);
    assert(ch > 0);
    assert(yaza_rt_v1_channel_send(ch, 1234) == 1);
    int64_t ch_val = 0;
    assert(yaza_rt_v1_channel_recv(ch, &ch_val) == 1 && ch_val == 1234);
    assert(yaza_rt_v1_channel_close(ch) == 1);

    /* 8. Crypto HMAC + constant-time compare */
    const unsigned char k[] = "audit-secret-key";
    const unsigned char m1[] = "message-payload-1";
    const unsigned char m2[] = "message-payload-2";
    YazaBytesV1 mac1 = {NULL, 0}, mac2 = {NULL, 0};
    yaza_crypto_v1_hmac_sha256(k, sizeof(k) - 1, m1, sizeof(m1) - 1, &mac1);
    yaza_crypto_v1_hmac_sha256(k, sizeof(k) - 1, m2, sizeof(m2) - 1, &mac2);
    assert(mac1.length == 32 && mac2.length == 32);
    assert(yaza_bytes_v1_equal_constant_time(mac1.data, mac1.length, mac1.data, mac1.length) == 1);
    assert(yaza_bytes_v1_equal_constant_time(mac1.data, mac1.length, mac2.data, mac2.length) == 0);

    /* 9. YazaBase C API path overflow & NULL result cleanup safety */
    char long_dir[5000];
    memset(long_dir, 'a', sizeof(long_dir) - 1);
    long_dir[sizeof(long_dir) - 1] = '\0';
    yazabase_t *db = (yazabase_t *)(uintptr_t)0x1;
    assert(yazabase_open(long_dir, "testdb", &db) == YAZABASE_MISUSE);
    assert(db == NULL);
    yazabase_free_result(NULL);

    puts("runtime-audit-v1: ALL Phase 1 security, memory, concurrency & ABI checks PASSED");
    return 0;
}

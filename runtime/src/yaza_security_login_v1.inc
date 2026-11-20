/* Bounded process-local login attempt limiter. Callers hash account keys first. */
typedef struct YazaLoginAttemptV1 {
    unsigned char key[64];
    int64_t started_ms;
    uint32_t count;
    int used;
} YazaLoginAttemptV1;

static pthread_mutex_t yaza_login_attempt_lock_v1 = PTHREAD_MUTEX_INITIALIZER;
static YazaLoginAttemptV1 yaza_login_attempts_v1[256];

int32_t yaza_security_v1_login_attempt(const YazaTextV1 *key_hex, int64_t now_ms, int32_t reset) {
    if (key_hex == NULL || key_hex->data == NULL || key_hex->length != 64 || now_ms < 0) return 0;
    for (size_t i = 0; i < 64; i++) {
        unsigned char ch = key_hex->data[i];
        if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))) return 0;
    }
    if (pthread_mutex_lock(&yaza_login_attempt_lock_v1) != 0) return 0;
    int match = -1;
    int spare = -1;
    for (int i = 0; i < 256; i++) {
        YazaLoginAttemptV1 *item = &yaza_login_attempts_v1[i];
        if (item->used && memcmp(item->key, key_hex->data, 64) == 0) {
            match = i;
            break;
        }
        if (spare < 0 && (!item->used || now_ms < item->started_ms ||
            now_ms - item->started_ms >= 900000)) spare = i;
    }
    int allowed = 1;
    if (match >= 0) {
        YazaLoginAttemptV1 *item = &yaza_login_attempts_v1[match];
        if (reset || now_ms < item->started_ms || now_ms - item->started_ms >= 900000) {
            item->count = reset ? 0 : 1;
            item->started_ms = now_ms;
        } else if (item->count >= 5) {
            allowed = 0;
        } else {
            item->count++;
        }
    } else if (!reset) {
        if (spare < 0) allowed = 0;
        else {
            YazaLoginAttemptV1 *item = &yaza_login_attempts_v1[spare];
            memcpy(item->key, key_hex->data, 64);
            item->used = 1;
            item->started_ms = now_ms;
            item->count = 1;
        }
    }
    (void)pthread_mutex_unlock(&yaza_login_attempt_lock_v1);
    return allowed;
}

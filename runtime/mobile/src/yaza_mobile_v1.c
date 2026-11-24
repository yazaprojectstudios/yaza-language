#define _POSIX_C_SOURCE 200809L
#include "yaza_mobile_v1.h"
#include "yaza_runtime_v1.h"

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define YAZA_MOBILE_ENGINE_MAGIC UINT64_C(0x59415a414d4f4231)
#define YAZA_MOBILE_PROCESS_ARENA_BYTES (16ULL * 1024ULL * 1024ULL)
#define YAZA_MOBILE_REQUEST_ARENA_BYTES (4ULL * 1024ULL * 1024ULL)
#define YAZA_MOBILE_INITIAL_STATE "0"
#define YAZA_MOBILE_INITIAL_STATE_LENGTH 1ULL
#define YAZA_MOBILE_MAX_OUTSTANDING_BUFFERS 4096ULL

typedef struct YazaMobileOwnedBuffer {
    struct YazaMobileOwnedBuffer *next;
    uint64_t length;
    uint64_t lease_id;
    size_t allocation_size;
    unsigned char data[];
} YazaMobileOwnedBuffer;

typedef struct YazaMobileEngine YazaMobileEngine;

struct YazaMobileEngine {
    struct YazaMobileEngine *registry_next;
    YazaMobileEngineHandleV1 handle_id;
    uint64_t magic;
    pthread_mutex_t lock;
    pthread_cond_t idle;
    uint64_t active_calls;
    YazaMobileHostV1 host;
    YazaArenaV1 *arena;
    YazaArenaCycleV1 *cycle;
    unsigned char *state;
    uint64_t state_length;
    uint32_t state_input_kind;
    int closed;
    int closing;
};

static pthread_once_t yaza_mobile_runtime_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t yaza_mobile_engines_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t yaza_mobile_buffers_lock = PTHREAD_MUTEX_INITIALIZER;
static YazaMobileEngine *yaza_mobile_engines;
static uint32_t yaza_mobile_engine_count;
static YazaMobileOwnedBuffer *yaza_mobile_buffers;
static uint64_t yaza_mobile_outstanding_storage_bytes;
static uint64_t yaza_mobile_outstanding_buffer_count;
static YazaMobileEngineHandleV1 yaza_mobile_next_engine_handle = 1;
static uint64_t yaza_mobile_next_buffer_lease_id = 1;

static void yaza_mobile_initialize_runtime(void) {
    yaza_rt_v1_init();
}

static YazaMobileEngine *yaza_mobile_engine_find_locked(YazaMobileEngineHandleV1 candidate) {
    YazaMobileEngine *engine = yaza_mobile_engines;
    while (engine != NULL && engine->handle_id != candidate) engine = engine->registry_next;
    return engine;
}

static int yaza_mobile_engine_slot_reserve(YazaMobileEngineHandleV1 *out_handle) {
    if (out_handle == NULL) return 0;
    pthread_mutex_lock(&yaza_mobile_engines_lock);
    if (yaza_mobile_engine_count >= YAZA_MOBILE_V1_MAX_ENGINES ||
        yaza_mobile_next_engine_handle <= 0) {
        pthread_mutex_unlock(&yaza_mobile_engines_lock);
        return 0;
    }
    yaza_mobile_engine_count++;
    *out_handle = yaza_mobile_next_engine_handle;
    if (yaza_mobile_next_engine_handle == INT64_MAX) yaza_mobile_next_engine_handle = 0;
    else yaza_mobile_next_engine_handle++;
    pthread_mutex_unlock(&yaza_mobile_engines_lock);
    return 1;
}

static void yaza_mobile_engine_slot_release(void) {
    pthread_mutex_lock(&yaza_mobile_engines_lock);
    if (yaza_mobile_engine_count > 0) yaza_mobile_engine_count--;
    pthread_mutex_unlock(&yaza_mobile_engines_lock);
}

/* Registry membership is checked before dereferencing a caller-provided handle. */
static YazaMobileEngine *yaza_mobile_engine_acquire(YazaMobileEngineHandleV1 candidate) {
    if (candidate <= 0) return NULL;
    pthread_mutex_lock(&yaza_mobile_engines_lock);
    YazaMobileEngine *engine = yaza_mobile_engine_find_locked(candidate);
    if (engine == NULL || engine->closing || engine->magic != YAZA_MOBILE_ENGINE_MAGIC) {
        pthread_mutex_unlock(&yaza_mobile_engines_lock);
        return NULL;
    }
    engine->active_calls++;
    pthread_mutex_unlock(&yaza_mobile_engines_lock);
    return engine;
}

static void yaza_mobile_engine_release(YazaMobileEngine *engine) {
    pthread_mutex_lock(&yaza_mobile_engines_lock);
    if (engine->active_calls > 0) engine->active_calls--;
    if (engine->closing && engine->active_calls == 0) pthread_cond_broadcast(&engine->idle);
    pthread_mutex_unlock(&yaza_mobile_engines_lock);
}

static void yaza_mobile_engine_register(YazaMobileEngine *engine) {
    pthread_mutex_lock(&yaza_mobile_engines_lock);
    engine->closing = 0;
    engine->registry_next = yaza_mobile_engines;
    yaza_mobile_engines = engine;
    pthread_mutex_unlock(&yaza_mobile_engines_lock);
}

static int32_t yaza_mobile_buffer_create(const unsigned char *data, uint64_t length,
                                         YazaMobileBufferV1 *out_buffer) {
    if (out_buffer == NULL || (length != 0 && data == NULL)) return YAZA_MOBILE_V1_INVALID_ARGUMENT;
    if (length > YAZA_MOBILE_V1_MAX_RESULT_BYTES ||
        length > (uint64_t)SIZE_MAX - sizeof(YazaMobileOwnedBuffer) - 1ULL) {
        return YAZA_MOBILE_V1_RESOURCE_LIMIT;
    }

    const size_t allocation_size = sizeof(YazaMobileOwnedBuffer) + (size_t)length + 1U;
    pthread_mutex_lock(&yaza_mobile_buffers_lock);
    if (yaza_mobile_outstanding_buffer_count >= YAZA_MOBILE_MAX_OUTSTANDING_BUFFERS ||
        (uint64_t)allocation_size > YAZA_MOBILE_V1_MAX_OUTSTANDING_BYTES -
            yaza_mobile_outstanding_storage_bytes ||
        yaza_mobile_next_buffer_lease_id == 0) {
        pthread_mutex_unlock(&yaza_mobile_buffers_lock);
        return YAZA_MOBILE_V1_RESOURCE_LIMIT;
    }
    YazaMobileOwnedBuffer *entry = (YazaMobileOwnedBuffer *)malloc(allocation_size);
    if (entry == NULL) {
        pthread_mutex_unlock(&yaza_mobile_buffers_lock);
        return YAZA_MOBILE_V1_OUT_OF_MEMORY;
    }
    entry->length = length;
    entry->lease_id = yaza_mobile_next_buffer_lease_id;
    if (yaza_mobile_next_buffer_lease_id == UINT64_MAX) yaza_mobile_next_buffer_lease_id = 0;
    else yaza_mobile_next_buffer_lease_id++;
    entry->allocation_size = allocation_size;
    if (length != 0) memcpy(entry->data, data, (size_t)length);
    entry->data[length] = 0;
    entry->next = yaza_mobile_buffers;
    yaza_mobile_buffers = entry;
    yaza_mobile_outstanding_storage_bytes += (uint64_t)allocation_size;
    yaza_mobile_outstanding_buffer_count++;
    out_buffer->data = entry->data;
    out_buffer->length = length;
    out_buffer->lease_id = entry->lease_id;
    pthread_mutex_unlock(&yaza_mobile_buffers_lock);
    return YAZA_MOBILE_V1_OK;
}

static int yaza_mobile_is_ybif(const uint8_t *data, uint64_t length) {
    return data != NULL && length >= 4ULL &&
           data[0] == 0x59U && data[1] == 0x42U && data[2] == 0x49U && data[3] == 0x46U;
}

static int32_t yaza_mobile_event_text(uint32_t event_id, YazaMobileView *out_view) {
    static const unsigned char increment[] = "increment";
    static const unsigned char reset[] = "reset";
    static const unsigned char snapshot[] = "snapshot";
    static const unsigned char action[] = "action";
    if (out_view == NULL) return YAZA_MOBILE_V1_INVALID_ARGUMENT;
    if (event_id == YAZA_MOBILE_EVENT_V1_INCREMENT) {
        out_view->data = increment;
        out_view->length = sizeof(increment) - 1ULL;
        return YAZA_MOBILE_V1_OK;
    }
    if (event_id == YAZA_MOBILE_EVENT_V1_RESET) {
        out_view->data = reset;
        out_view->length = sizeof(reset) - 1ULL;
        return YAZA_MOBILE_V1_OK;
    }
    if (event_id == YAZA_MOBILE_EVENT_V1_SNAPSHOT) {
        out_view->data = snapshot;
        out_view->length = sizeof(snapshot) - 1ULL;
        return YAZA_MOBILE_V1_OK;
    }
    if (event_id == YAZA_MOBILE_EVENT_V1_ACTION) {
        out_view->data = action;
        out_view->length = sizeof(action) - 1ULL;
        return YAZA_MOBILE_V1_OK;
    }
    out_view->data = NULL;
    out_view->length = 0;
    return YAZA_MOBILE_V1_INVALID_ARGUMENT;
}

static int yaza_mobile_utf8_is_valid(const uint8_t *data, uint64_t length) {
    uint64_t offset = 0;
    while (offset < length) {
        const uint8_t lead = data[offset++];
        if (lead <= 0x7fU) continue;

        uint64_t continuation_count = 0;
        uint8_t second_min = 0x80U;
        uint8_t second_max = 0xbfU;
        if (lead >= 0xc2U && lead <= 0xdfU) {
            continuation_count = 1;
        } else if (lead >= 0xe0U && lead <= 0xefU) {
            continuation_count = 2;
            if (lead == 0xe0U) second_min = 0xa0U;
            if (lead == 0xedU) second_max = 0x9fU;
        } else if (lead >= 0xf0U && lead <= 0xf4U) {
            continuation_count = 3;
            if (lead == 0xf0U) second_min = 0x90U;
            if (lead == 0xf4U) second_max = 0x8fU;
        } else {
            return 0;
        }

        if (length - offset < continuation_count) return 0;
        const uint8_t second = data[offset++];
        if (second < second_min || second > second_max) return 0;
        for (uint64_t i = 1; i < continuation_count; ++i) {
            const uint8_t continuation = data[offset++];
            if (continuation < 0x80U || continuation > 0xbfU) return 0;
        }
    }
    return 1;
}

extern int32_t yaza_export_mobile_reduce_v1(const YazaModuleInputV1 *inputs,
                                            uint64_t input_count,
                                            YazaModuleResultV1 *out_result);

int32_t yaza_mobile_v1_create(const YazaMobileHostV1 *host, YazaMobileEngineHandleV1 *out_engine) {
    if (out_engine == NULL) return YAZA_MOBILE_V1_INVALID_ARGUMENT;
    *out_engine = 0;
    if (host == NULL || host->struct_size < sizeof(YazaMobileHostV1)) return YAZA_MOBILE_V1_INVALID_ARGUMENT;
    if (host->abi_version != YAZA_MOBILE_V1_ABI_VERSION) return YAZA_MOBILE_V1_ABI_MISMATCH;
    YazaMobileEngineHandleV1 handle_id = 0;
    if (!yaza_mobile_engine_slot_reserve(&handle_id)) return YAZA_MOBILE_V1_RESOURCE_LIMIT;

    YazaMobileEngine *engine = (YazaMobileEngine *)calloc(1, sizeof(*engine));
    if (engine == NULL) {
        yaza_mobile_engine_slot_release();
        return YAZA_MOBILE_V1_OUT_OF_MEMORY;
    }
    if (pthread_mutex_init(&engine->lock, NULL) != 0) {
        free(engine);
        yaza_mobile_engine_slot_release();
        return YAZA_MOBILE_V1_INTERNAL_ERROR;
    }
    if (pthread_cond_init(&engine->idle, NULL) != 0) {
        pthread_mutex_destroy(&engine->lock);
        free(engine);
        yaza_mobile_engine_slot_release();
        return YAZA_MOBILE_V1_INTERNAL_ERROR;
    }
    if (pthread_once(&yaza_mobile_runtime_once, yaza_mobile_initialize_runtime) != 0) {
        pthread_cond_destroy(&engine->idle);
        pthread_mutex_destroy(&engine->lock);
        free(engine);
        yaza_mobile_engine_slot_release();
        return YAZA_MOBILE_V1_INTERNAL_ERROR;
    }

    engine->arena = yaza_arena_v1_create(YAZA_MOBILE_PROCESS_ARENA_BYTES);
    if (engine->arena == NULL) {
        pthread_cond_destroy(&engine->idle);
        pthread_mutex_destroy(&engine->lock);
        free(engine);
        yaza_mobile_engine_slot_release();
        return YAZA_MOBILE_V1_OUT_OF_MEMORY;
    }
    engine->cycle = yaza_arena_v1_cycle_create(engine->arena, YAZA_MOBILE_REQUEST_ARENA_BYTES);
    engine->state = (unsigned char *)malloc(YAZA_MOBILE_INITIAL_STATE_LENGTH + 1ULL);
    if (engine->cycle == NULL || engine->state == NULL) {
        if (engine->cycle != NULL) yaza_arena_v1_cycle_destroy(engine->cycle);
        yaza_arena_v1_destroy(engine->arena);
        free(engine->state);
        pthread_cond_destroy(&engine->idle);
        pthread_mutex_destroy(&engine->lock);
        free(engine);
        yaza_mobile_engine_slot_release();
        return YAZA_MOBILE_V1_OUT_OF_MEMORY;
    }
    memcpy(engine->state, YAZA_MOBILE_INITIAL_STATE, YAZA_MOBILE_INITIAL_STATE_LENGTH + 1ULL);
    engine->state_length = YAZA_MOBILE_INITIAL_STATE_LENGTH;
    engine->host = *host;
    engine->handle_id = handle_id;
    engine->magic = YAZA_MOBILE_ENGINE_MAGIC;
    yaza_mobile_engine_register(engine);
    *out_engine = handle_id;
    return YAZA_MOBILE_V1_OK;
}

int32_t yaza_mobile_v1_dispatch(YazaMobileEngineHandleV1 candidate, uint32_t event_id,
                                YazaMobileView payload, YazaMobileBufferV1 *out_state) {
    if (out_state == NULL) return YAZA_MOBILE_V1_INVALID_ARGUMENT;
    if (out_state->data != NULL || out_state->length != 0 || out_state->lease_id != 0) {
        return YAZA_MOBILE_V1_INVALID_ARGUMENT;
    }
    if (payload.length != 0 && payload.data == NULL) return YAZA_MOBILE_V1_INVALID_ARGUMENT;
    if (payload.length > YAZA_MOBILE_V1_MAX_EVENT_BYTES) return YAZA_MOBILE_V1_RESOURCE_LIMIT;
    if (payload.length != 0 && !yaza_mobile_is_ybif(payload.data, payload.length) &&
        !yaza_mobile_utf8_is_valid(payload.data, payload.length)) {
        return YAZA_MOBILE_V1_INVALID_ARGUMENT;
    }

    YazaMobileView event = {0};
    int32_t status = yaza_mobile_event_text(event_id, &event);
    if (status != YAZA_MOBILE_V1_OK) return status;

    YazaMobileEngine *engine = yaza_mobile_engine_acquire(candidate);
    if (engine == NULL) return YAZA_MOBILE_V1_INVALID_ARGUMENT;
    if (pthread_mutex_lock(&engine->lock) != 0) {
        yaza_mobile_engine_release(engine);
        return YAZA_MOBILE_V1_INTERNAL_ERROR;
    }
    if (engine->closed) {
        pthread_mutex_unlock(&engine->lock);
        yaza_mobile_engine_release(engine);
        return YAZA_MOBILE_V1_INVALID_ARGUMENT;
    }
    if (yaza_arena_v1_cycle_begin(engine->cycle) != 1) {
        pthread_mutex_unlock(&engine->lock);
        yaza_mobile_engine_release(engine);
        return YAZA_MOBILE_V1_BUSY;
    }

    const uint32_t payload_kind = yaza_mobile_is_ybif(payload.data, payload.length)
        ? YAZA_MODULE_INPUT_V1_BYTES
        : YAZA_MODULE_INPUT_V1_TEXT;
    const uint32_t initial_state_kind = engine->state_input_kind != 0
        ? engine->state_input_kind
        : (yaza_mobile_is_ybif(engine->state, engine->state_length)
               ? YAZA_MODULE_INPUT_V1_BYTES
               : YAZA_MODULE_INPUT_V1_TEXT);

    YazaModuleInputV1 inputs[3] = {
        {YAZA_MODULE_INPUT_V1_ABI_VERSION, sizeof(YazaModuleInputV1), YAZA_MODULE_INPUT_V1_TEXT, 0, event.data, event.length},
        {YAZA_MODULE_INPUT_V1_ABI_VERSION, sizeof(YazaModuleInputV1), payload_kind, 0, payload.data, payload.length},
        {YAZA_MODULE_INPUT_V1_ABI_VERSION, sizeof(YazaModuleInputV1), initial_state_kind, 0, engine->state, engine->state_length}
    };
    YazaModuleResultV1 result = {
        YAZA_MODULE_RESULT_V1_ABI_VERSION, sizeof(YazaModuleResultV1),
        YAZA_MODULE_RESULT_V1_OK, 0, NULL, 0
    };

    int32_t finish_status = yaza_export_mobile_reduce_v1(inputs, 3, &result);
    if (engine->state_input_kind == 0 && finish_status == YAZA_MODULE_RESULT_V1_INVALID_INPUT) {
        inputs[2].kind = (initial_state_kind == YAZA_MODULE_INPUT_V1_TEXT)
            ? YAZA_MODULE_INPUT_V1_BYTES
            : YAZA_MODULE_INPUT_V1_TEXT;
        result = (YazaModuleResultV1){
            YAZA_MODULE_RESULT_V1_ABI_VERSION, sizeof(YazaModuleResultV1),
            YAZA_MODULE_RESULT_V1_OK, 0, NULL, 0
        };
        finish_status = yaza_export_mobile_reduce_v1(inputs, 3, &result);
    }
    if (finish_status == YAZA_MODULE_RESULT_V1_OK) {
        if (engine->state_input_kind == 0) engine->state_input_kind = inputs[2].kind;
        if (result.abi_version != YAZA_MODULE_RESULT_V1_ABI_VERSION ||
            result.struct_size < sizeof(YazaModuleResultV1)) {
            finish_status = YAZA_MOBILE_V1_ABI_MISMATCH;
        } else if (result.length > YAZA_MOBILE_V1_MAX_RESULT_BYTES ||
                   (result.length != 0 && result.data == NULL)) {
            finish_status = YAZA_MOBILE_V1_RESOURCE_LIMIT;
        } else if (result.length != 0 && !yaza_mobile_is_ybif(result.data, result.length) &&
                   !yaza_mobile_utf8_is_valid(result.data, result.length)) {
            finish_status = YAZA_MOBILE_V1_INTERNAL_ERROR;
        } else if (result.status == YAZA_MODULE_RESULT_V1_OK) {
            finish_status = YAZA_MOBILE_V1_OK;
        } else if (result.status == YAZA_MODULE_RESULT_V1_APPLICATION_ERROR) {
            finish_status = YAZA_MOBILE_V1_APPLICATION_ERROR;
        } else if (result.status == YAZA_MODULE_RESULT_V1_INVALID_INPUT) {
            finish_status = YAZA_MOBILE_V1_INVALID_ARGUMENT;
        } else if (result.status == YAZA_MODULE_RESULT_V1_RESOURCE_LIMIT) {
            finish_status = YAZA_MOBILE_V1_RESOURCE_LIMIT;
        } else {
            finish_status = YAZA_MOBILE_V1_INTERNAL_ERROR;
        }
    } else if (finish_status == YAZA_MODULE_RESULT_V1_INVALID_INPUT) {
        finish_status = YAZA_MOBILE_V1_INVALID_ARGUMENT;
    } else if (finish_status == YAZA_MODULE_RESULT_V1_NO_REQUEST_SCOPE) {
        finish_status = YAZA_MOBILE_V1_INTERNAL_ERROR;
    } else if (finish_status == YAZA_MODULE_RESULT_V1_RESOURCE_LIMIT) {
        finish_status = YAZA_MOBILE_V1_RESOURCE_LIMIT;
    } else {
        finish_status = YAZA_MOBILE_V1_INTERNAL_ERROR;
    }

    unsigned char *pending_state = NULL;
    if (finish_status == YAZA_MOBILE_V1_OK) {
        if (result.length > (uint64_t)SIZE_MAX - 1ULL) {
            finish_status = YAZA_MOBILE_V1_RESOURCE_LIMIT;
        } else {
            pending_state = (unsigned char *)malloc((size_t)result.length + 1U);
            if (pending_state == NULL) {
                finish_status = YAZA_MOBILE_V1_OUT_OF_MEMORY;
            } else {
                if (result.length != 0) memcpy(pending_state, result.data, (size_t)result.length);
                pending_state[result.length] = 0;
                finish_status = yaza_mobile_buffer_create(result.data, result.length, out_state);
            }
        }
    } else if (finish_status == YAZA_MOBILE_V1_APPLICATION_ERROR) {
        finish_status = yaza_mobile_buffer_create(result.data, result.length, out_state);
        if (finish_status == YAZA_MOBILE_V1_OK) finish_status = YAZA_MOBILE_V1_APPLICATION_ERROR;
    }

    YazaArenaCycleMetricsV1 metrics = {0};
    metrics.abi_version = YAZA_ARENA_CYCLE_METRICS_V1_ABI_VERSION;
    metrics.struct_size = sizeof(metrics);
    const int32_t cycle_status = yaza_arena_v1_cycle_end(engine->cycle, &metrics);
    if (cycle_status != 1) {
        finish_status = YAZA_MOBILE_V1_INTERNAL_ERROR;
        engine->closed = 1;
    } else if (finish_status == YAZA_MOBILE_V1_OK) {
        free(engine->state);
        engine->state = pending_state;
        engine->state_length = result.length;
        pending_state = NULL;
    }

    free(pending_state);
    if (finish_status != YAZA_MOBILE_V1_OK &&
        finish_status != YAZA_MOBILE_V1_APPLICATION_ERROR &&
        out_state->lease_id != 0) {
        yaza_mobile_v1_buffer_release(out_state);
    }
    pthread_mutex_unlock(&engine->lock);
    yaza_mobile_engine_release(engine);
    return finish_status;
}

int32_t yaza_mobile_v1_buffer_release(YazaMobileBufferV1 *buffer) {
    if (buffer == NULL) return YAZA_MOBILE_V1_INVALID_ARGUMENT;
    if (buffer->data == NULL && buffer->length == 0 && buffer->lease_id == 0) return YAZA_MOBILE_V1_OK;
    if (buffer->lease_id == 0) return YAZA_MOBILE_V1_INVALID_ARGUMENT;

    pthread_mutex_lock(&yaza_mobile_buffers_lock);
    YazaMobileOwnedBuffer *previous = NULL;
    YazaMobileOwnedBuffer *entry = yaza_mobile_buffers;
    while (entry != NULL &&
           (entry->lease_id != buffer->lease_id || entry->data != buffer->data ||
            entry->length != buffer->length)) {
        previous = entry;
        entry = entry->next;
    }
    if (entry == NULL) {
        pthread_mutex_unlock(&yaza_mobile_buffers_lock);
        return YAZA_MOBILE_V1_INVALID_ARGUMENT;
    }
    if (previous == NULL) yaza_mobile_buffers = entry->next;
    else previous->next = entry->next;
    yaza_mobile_outstanding_storage_bytes -= (uint64_t)entry->allocation_size;
    yaza_mobile_outstanding_buffer_count--;
    pthread_mutex_unlock(&yaza_mobile_buffers_lock);

    free(entry);
    memset(buffer, 0, sizeof(*buffer));
    return YAZA_MOBILE_V1_OK;
}

int32_t yaza_mobile_v1_destroy(YazaMobileEngineHandleV1 candidate) {
    if (candidate <= 0) return YAZA_MOBILE_V1_INVALID_ARGUMENT;
    pthread_mutex_lock(&yaza_mobile_engines_lock);
    YazaMobileEngine *engine = yaza_mobile_engine_find_locked(candidate);
    if (engine == NULL || engine->closing || engine->magic != YAZA_MOBILE_ENGINE_MAGIC) {
        pthread_mutex_unlock(&yaza_mobile_engines_lock);
        return YAZA_MOBILE_V1_INVALID_ARGUMENT;
    }
    engine->closing = 1;
    YazaMobileEngine **link = &yaza_mobile_engines;
    while (*link != engine) link = &(*link)->registry_next;
    *link = engine->registry_next;
    engine->registry_next = NULL;
    while (engine->active_calls != 0) pthread_cond_wait(&engine->idle, &yaza_mobile_engines_lock);
    pthread_mutex_unlock(&yaza_mobile_engines_lock);

    engine->closed = 1;
    if (engine->cycle != NULL) {
        if (yaza_arena_v1_cycle_destroy(engine->cycle) != 1) {
            yaza_mobile_engine_register(engine);
            return YAZA_MOBILE_V1_INTERNAL_ERROR;
        }
        engine->cycle = NULL;
    }
    if (engine->arena != NULL) {
        if (yaza_arena_v1_destroy(engine->arena) != 1) {
            yaza_mobile_engine_register(engine);
            return YAZA_MOBILE_V1_INTERNAL_ERROR;
        }
        engine->arena = NULL;
    }

    engine->magic = 0;
    free(engine->state);
    pthread_cond_destroy(&engine->idle);
    pthread_mutex_destroy(&engine->lock);
    free(engine);
    yaza_mobile_engine_slot_release();
    return YAZA_MOBILE_V1_OK;
}


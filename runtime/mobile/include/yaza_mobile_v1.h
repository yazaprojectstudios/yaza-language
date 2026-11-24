#ifndef YAZA_MOBILE_V1_H
#define YAZA_MOBILE_V1_H

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define YAZA_MOBILE_V1_ABI_VERSION 1U
#define YAZA_MOBILE_V1_MAX_EVENT_BYTES (512ULL * 1024ULL)
#define YAZA_MOBILE_V1_MAX_RESULT_BYTES (1ULL * 1024ULL * 1024ULL)
#define YAZA_MOBILE_V1_MAX_OUTSTANDING_BYTES (16ULL * 1024ULL * 1024ULL)
#define YAZA_MOBILE_V1_MAX_ENGINES 4U

/* Positive process-local ID; zero is invalid. IDs are never reused. */
typedef int64_t YazaMobileEngineHandleV1;

typedef struct YazaMobileView {
    const uint8_t *data;
    uint64_t length;
} YazaMobileView;

typedef void (*YazaMobilePostToMainThreadV1)(void *context, uint64_t callback_id);

typedef struct YazaMobileHostV1 {
    uint32_t struct_size;
    uint32_t abi_version;
    void *context;
    YazaMobilePostToMainThreadV1 post_to_main_thread;
} YazaMobileHostV1;

typedef struct YazaMobileBufferV1 {
    const uint8_t *data;
    uint64_t length;
    uint64_t lease_id;
} YazaMobileBufferV1;

typedef enum YazaMobileStatusV1 {
    YAZA_MOBILE_V1_OK = 0,
    YAZA_MOBILE_V1_APPLICATION_ERROR = 1,
    YAZA_MOBILE_V1_INVALID_ARGUMENT = 2,
    YAZA_MOBILE_V1_ABI_MISMATCH = 3,
    YAZA_MOBILE_V1_OUT_OF_MEMORY = 4,
    YAZA_MOBILE_V1_RESOURCE_LIMIT = 5,
    YAZA_MOBILE_V1_BUSY = 6,
    YAZA_MOBILE_V1_INTERNAL_ERROR = 7
} YazaMobileStatusV1;

typedef enum YazaMobileEventV1 {
    YAZA_MOBILE_EVENT_V1_INCREMENT = 1,
    YAZA_MOBILE_EVENT_V1_RESET = 2,
    YAZA_MOBILE_EVENT_V1_SNAPSHOT = 3,
    YAZA_MOBILE_EVENT_V1_ACTION = 4
} YazaMobileEventV1;

/* The host struct is copied during create. ABI v1 is synchronous and does not
   invoke the reserved callback; a later async ABI must define its thread rules. */
int32_t yaza_mobile_v1_create(const YazaMobileHostV1 *host, YazaMobileEngineHandleV1 *out_engine);
/* payload is borrowed for this call. out_state must be zero-initialized or a
   previously released buffer. Calls on one engine are serialized. */
int32_t yaza_mobile_v1_dispatch(YazaMobileEngineHandleV1 engine, uint32_t event_id,
                                YazaMobileView payload, YazaMobileBufferV1 *out_state);
/* Releases only buffers issued by this library; forged and duplicate lease IDs
   are rejected. A zero-initialized buffer may be released repeatedly. */
int32_t yaza_mobile_v1_buffer_release(YazaMobileBufferV1 *buffer);
/* The host must stop starting calls before destroy. Destroy waits for an active
   dispatch to finish, then invalidates the handle. */
int32_t yaza_mobile_v1_destroy(YazaMobileEngineHandleV1 engine);

#if defined(__cplusplus)
}
#endif
#endif

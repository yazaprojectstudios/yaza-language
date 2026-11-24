#include <dlfcn.h>
#include <jni.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "yaza_mobile_v1.h"

typedef int32_t (*yaza_mobile_create_fn)(const YazaMobileHostV1 *host, YazaMobileEngineHandleV1 *out_engine);
typedef int32_t (*yaza_mobile_dispatch_fn)(YazaMobileEngineHandleV1 engine, uint32_t event_id,
                                           YazaMobileView payload, YazaMobileBufferV1 *out_state);
typedef int32_t (*yaza_mobile_release_fn)(YazaMobileBufferV1 *buffer);
typedef int32_t (*yaza_mobile_destroy_fn)(YazaMobileEngineHandleV1 engine);

static void *g_hotswap_dl = NULL;
static yaza_mobile_create_fn g_create = yaza_mobile_v1_create;
static yaza_mobile_dispatch_fn g_dispatch = yaza_mobile_v1_dispatch;
static yaza_mobile_release_fn g_release = yaza_mobile_v1_buffer_release;
static yaza_mobile_destroy_fn g_destroy = yaza_mobile_v1_destroy;

static jbyteArray yaza_jni_result(JNIEnv *env, int32_t status,
                                  const uint8_t *data, uint64_t length) {
    if (length > YAZA_MOBILE_V1_MAX_RESULT_BYTES ||
        length > (uint64_t)INT32_MAX - (uint64_t)sizeof(int32_t)) {
        status = YAZA_MOBILE_V1_RESOURCE_LIMIT;
        data = NULL;
        length = 0;
    }
    const jsize total = (jsize)(sizeof(int32_t) + (size_t)length);
    jbyteArray result = (*env)->NewByteArray(env, total);
    if (result == NULL) return NULL;

    const uint32_t encoded_status = (uint32_t)status;
    const jbyte prefix[4] = {
        (jbyte)((encoded_status >> 24) & 0xffU),
        (jbyte)((encoded_status >> 16) & 0xffU),
        (jbyte)((encoded_status >> 8) & 0xffU),
        (jbyte)(encoded_status & 0xffU)
    };
    (*env)->SetByteArrayRegion(env, result, 0, 4, prefix);
    if (length != 0) {
        (*env)->SetByteArrayRegion(env, result, 4, (jsize)length, (const jbyte *)data);
    }
    if ((*env)->ExceptionCheck(env)) {
        (*env)->DeleteLocalRef(env, result);
        return NULL;
    }
    return result;
}

JNIEXPORT jlong JNICALL
Java_dev_yaza_mobile_NativeEngine_nativeCreate(JNIEnv *env, jobject self) {
    (void)env;
    (void)self;
    const YazaMobileHostV1 host = {
        (uint32_t)sizeof(YazaMobileHostV1),
        YAZA_MOBILE_V1_ABI_VERSION,
        NULL,
        NULL
    };
    YazaMobileEngineHandleV1 engine = 0;
    if (g_create(&host, &engine) != YAZA_MOBILE_V1_OK) return 0;
    return (jlong)engine;
}

JNIEXPORT jbyteArray JNICALL
Java_dev_yaza_mobile_NativeEngine_nativeDispatch(JNIEnv *env, jobject self,
                                                  jlong handle, jint event_id,
                                                  jbyteArray payload_array) {
    (void)self;
    if (payload_array == NULL) return yaza_jni_result(env, YAZA_MOBILE_V1_INVALID_ARGUMENT, NULL, 0);
    const jsize payload_length = (*env)->GetArrayLength(env, payload_array);
    if (payload_length < 0 || (uint64_t)payload_length > YAZA_MOBILE_V1_MAX_EVENT_BYTES) {
        return yaza_jni_result(env, YAZA_MOBILE_V1_RESOURCE_LIMIT, NULL, 0);
    }
    uint8_t *payload = NULL;
    if (payload_length > 0) {
        payload = (uint8_t *)malloc((size_t)payload_length);
        if (payload == NULL) return yaza_jni_result(env, YAZA_MOBILE_V1_OUT_OF_MEMORY, NULL, 0);
        (*env)->GetByteArrayRegion(env, payload_array, 0, payload_length, (jbyte *)payload);
        if ((*env)->ExceptionCheck(env)) {
            free(payload);
            return NULL;
        }
    }

    YazaMobileBufferV1 output = {NULL, 0, 0};
    const YazaMobileView view = {payload, (uint64_t)payload_length};
    const int32_t status = g_dispatch(
        (YazaMobileEngineHandleV1)handle, (uint32_t)event_id, view, &output);
    free(payload);
    jbyteArray encoded = yaza_jni_result(env, status, output.data, output.length);
    (void)g_release(&output);
    return encoded;
}

JNIEXPORT jlong JNICALL
Java_dev_yaza_mobile_NativeEngine_nativeHotSwap(JNIEnv *env, jobject self,
                                                 jlong old_handle, jbyteArray path_utf8) {
    (void)self;
    if (path_utf8 == NULL) return 0;
    const jsize path_len = (*env)->GetArrayLength(env, path_utf8);
    if (path_len <= 0 || path_len >= 4096) return 0;
    char *path = (char *)malloc((size_t)path_len + 1U);
    if (path == NULL) return 0;
    (*env)->GetByteArrayRegion(env, path_utf8, 0, path_len, (jbyte *)path);
    if ((*env)->ExceptionCheck(env)) {
        free(path);
        return 0;
    }
    path[path_len] = '\0';

    void *next_dl = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    free(path);
    if (next_dl == NULL) return 0;

    void *sym_create = dlsym(next_dl, "yaza_mobile_v1_create");
    void *sym_dispatch = dlsym(next_dl, "yaza_mobile_v1_dispatch");
    void *sym_release = dlsym(next_dl, "yaza_mobile_v1_buffer_release");
    void *sym_destroy = dlsym(next_dl, "yaza_mobile_v1_destroy");
    if (sym_create == NULL || sym_dispatch == NULL || sym_release == NULL || sym_destroy == NULL) {
        dlclose(next_dl);
        return 0;
    }

    yaza_mobile_create_fn next_create = NULL;
    yaza_mobile_dispatch_fn next_dispatch = NULL;
    yaza_mobile_release_fn next_release = NULL;
    yaza_mobile_destroy_fn next_destroy = NULL;
    memcpy(&next_create, &sym_create, sizeof(next_create));
    memcpy(&next_dispatch, &sym_dispatch, sizeof(next_dispatch));
    memcpy(&next_release, &sym_release, sizeof(next_release));
    memcpy(&next_destroy, &sym_destroy, sizeof(next_destroy));

    const YazaMobileHostV1 host = {
        (uint32_t)sizeof(YazaMobileHostV1),
        YAZA_MOBILE_V1_ABI_VERSION,
        NULL,
        NULL
    };
    YazaMobileEngineHandleV1 next_engine = 0;
    if (next_create(&host, &next_engine) != YAZA_MOBILE_V1_OK || next_engine <= 0) {
        dlclose(next_dl);
        return 0;
    }

    if (old_handle > 0) (void)g_destroy((YazaMobileEngineHandleV1)old_handle);
    if (g_hotswap_dl != NULL) (void)dlclose(g_hotswap_dl);
    g_hotswap_dl = next_dl;
    g_create = next_create;
    g_dispatch = next_dispatch;
    g_release = next_release;
    g_destroy = next_destroy;
    return (jlong)next_engine;
}

JNIEXPORT void JNICALL
Java_dev_yaza_mobile_NativeEngine_nativeDestroy(JNIEnv *env, jobject self, jlong handle) {
    (void)env;
    (void)self;
    if (handle > 0) (void)g_destroy((YazaMobileEngineHandleV1)handle);
}


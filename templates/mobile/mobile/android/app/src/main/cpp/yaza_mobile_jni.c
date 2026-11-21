#include <jni.h>
#include <stdint.h>
#include <stdlib.h>

#include "yaza_mobile_v1.h"

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
    if (yaza_mobile_v1_create(&host, &engine) != YAZA_MOBILE_V1_OK) return 0;
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
    const int32_t status = yaza_mobile_v1_dispatch(
        (YazaMobileEngineHandleV1)handle, (uint32_t)event_id, view, &output);
    free(payload);
    jbyteArray encoded = yaza_jni_result(env, status, output.data, output.length);
    (void)yaza_mobile_v1_buffer_release(&output);
    return encoded;
}

JNIEXPORT void JNICALL
Java_dev_yaza_mobile_NativeEngine_nativeDestroy(JNIEnv *env, jobject self, jlong handle) {
    (void)env;
    (void)self;
    if (handle > 0) (void)yaza_mobile_v1_destroy((YazaMobileEngineHandleV1)handle);
}


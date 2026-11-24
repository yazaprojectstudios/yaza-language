#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jni.h"
#include "yaza_mobile_v1.h"

JNIEXPORT jlong JNICALL Java_dev_yaza_mobile_NativeEngine_nativeCreate(JNIEnv *env, jobject self);
JNIEXPORT jbyteArray JNICALL Java_dev_yaza_mobile_NativeEngine_nativeDispatch(
    JNIEnv *env, jobject self, jlong handle, jint event_id, jbyteArray payload_array);
JNIEXPORT void JNICALL Java_dev_yaza_mobile_NativeEngine_nativeDestroy(
    JNIEnv *env, jobject self, jlong handle);

static int g_fail_new_array = 0;
static int g_throw_on_get_region = 0;
static int g_throw_on_set_region = 0;
static int g_exception_pending = 0;
static long g_live_arrays = 0;

static jbyteArray mock_new_byte_array(JNIEnv *env, jsize length) {
    (void)env;
    if (g_fail_new_array || length < 0) return NULL;
    YazaMockByteArray *arr = (YazaMockByteArray *)calloc(1, sizeof(*arr));
    assert(arr != NULL);
    arr->length = length;
    if (length > 0) {
        arr->bytes = (jbyte *)calloc((size_t)length, 1);
        assert(arr->bytes != NULL);
    }
    arr->alive = 1;
    g_live_arrays++;
    return arr;
}

static void mock_free_byte_array(jbyteArray arr) {
    if (arr == NULL) return;
    assert(arr->alive == 1);
    arr->alive = 0;
    free(arr->bytes);
    free(arr);
    g_live_arrays--;
}

static jsize mock_get_array_length(JNIEnv *env, jbyteArray array) {
    (void)env;
    assert(array != NULL && array->alive == 1);
    return array->length;
}

static void mock_get_byte_array_region(JNIEnv *env, jbyteArray array, jsize start, jsize len, jbyte *buf) {
    (void)env;
    assert(array != NULL && array->alive == 1);
    if (g_throw_on_get_region) {
        g_exception_pending = 1;
        return;
    }
    assert(start >= 0 && len >= 0 && start + len <= array->length);
    if (len > 0) memcpy(buf, array->bytes + start, (size_t)len);
}

static void mock_set_byte_array_region(JNIEnv *env, jbyteArray array, jsize start, jsize len, const jbyte *buf) {
    (void)env;
    assert(array != NULL && array->alive == 1);
    if (g_throw_on_set_region) {
        g_exception_pending = 1;
        return;
    }
    assert(start >= 0 && len >= 0 && start + len <= array->length);
    if (len > 0) memcpy(array->bytes + start, buf, (size_t)len);
}

static jboolean mock_exception_check(JNIEnv *env) {
    (void)env;
    return g_exception_pending ? JNI_TRUE : JNI_FALSE;
}

static void mock_delete_local_ref(JNIEnv *env, jobject ref) {
    (void)env;
    mock_free_byte_array((jbyteArray)ref);
}

static const struct JNINativeInterface_ g_jni_table = {
    mock_new_byte_array,
    mock_get_array_length,
    mock_get_byte_array_region,
    mock_set_byte_array_region,
    mock_exception_check,
    mock_delete_local_ref
};

static jbyteArray make_input(JNIEnv *env, const char *text) {
    jsize len = (jsize)strlen(text);
    jbyteArray arr = (*env)->NewByteArray(env, len);
    if (len > 0) (*env)->SetByteArrayRegion(env, arr, 0, len, (const jbyte *)text);
    return arr;
}

static int32_t decode_status(jbyteArray arr) {
    assert(arr != NULL && arr->length >= 4);
    uint32_t raw = ((uint32_t)(uint8_t)arr->bytes[0] << 24) |
                   ((uint32_t)(uint8_t)arr->bytes[1] << 16) |
                   ((uint32_t)(uint8_t)arr->bytes[2] << 8) |
                   (uint32_t)(uint8_t)arr->bytes[3];
    return (int32_t)raw;
}

int main(void) {
    JNIEnv env_ptr = &g_jni_table;
    JNIEnv *env = &env_ptr;

    for (int cycle = 0; cycle < 100; ++cycle) {
        jlong handle = Java_dev_yaza_mobile_NativeEngine_nativeCreate(env, NULL);
        assert(handle > 0);

        jbyteArray p1 = make_input(env, "3");
        jbyteArray r1 = Java_dev_yaza_mobile_NativeEngine_nativeDispatch(env, NULL, handle, YAZA_MOBILE_EVENT_V1_INCREMENT, p1);
        assert(decode_status(r1) == YAZA_MOBILE_V1_OK);
        assert(r1->length == 5 && memcmp(r1->bytes + 4, "3", 1) == 0);
        mock_free_byte_array(p1);
        mock_free_byte_array(r1);

        jbyteArray p_empty = make_input(env, "");
        jbyteArray r_snap = Java_dev_yaza_mobile_NativeEngine_nativeDispatch(env, NULL, handle, YAZA_MOBILE_EVENT_V1_SNAPSHOT, p_empty);
        assert(decode_status(r_snap) == YAZA_MOBILE_V1_OK);
        assert(r_snap->length == 5 && memcmp(r_snap->bytes + 4, "3", 1) == 0);
        mock_free_byte_array(r_snap);

        jbyteArray p_act = make_input(env, "INCREMENT|2");
        jbyteArray r_act = Java_dev_yaza_mobile_NativeEngine_nativeDispatch(env, NULL, handle, YAZA_MOBILE_EVENT_V1_ACTION, p_act);
        assert(decode_status(r_act) == YAZA_MOBILE_V1_OK);
        assert(r_act->length > 9 && memcmp(r_act->bytes + 4, "YBIF", 4) == 0 && (uint8_t)r_act->bytes[8] == 1U);
        mock_free_byte_array(p_act);
        mock_free_byte_array(r_act);

        jbyteArray p_err = make_input(env, "1001");
        jbyteArray r_err = Java_dev_yaza_mobile_NativeEngine_nativeDispatch(env, NULL, handle, YAZA_MOBILE_EVENT_V1_INCREMENT, p_err);
        assert(decode_status(r_err) == YAZA_MOBILE_V1_APPLICATION_ERROR);
        mock_free_byte_array(p_err);
        mock_free_byte_array(r_err);

        jbyteArray r_null = Java_dev_yaza_mobile_NativeEngine_nativeDispatch(env, NULL, handle, YAZA_MOBILE_EVENT_V1_INCREMENT, NULL);
        assert(decode_status(r_null) == YAZA_MOBILE_V1_INVALID_ARGUMENT);
        mock_free_byte_array(r_null);

        g_fail_new_array = 1;
        jbyteArray r_oom = Java_dev_yaza_mobile_NativeEngine_nativeDispatch(env, NULL, handle, YAZA_MOBILE_EVENT_V1_INCREMENT, p_empty);
        assert(r_oom == NULL);
        g_fail_new_array = 0;

        g_throw_on_set_region = 1;
        jbyteArray r_set_ex = Java_dev_yaza_mobile_NativeEngine_nativeDispatch(env, NULL, handle, YAZA_MOBILE_EVENT_V1_INCREMENT, p_empty);
        assert(r_set_ex == NULL);
        g_throw_on_set_region = 0;
        g_exception_pending = 0;

        jbyteArray p_get = make_input(env, "1");
        g_throw_on_get_region = 1;
        jbyteArray r_get_ex = Java_dev_yaza_mobile_NativeEngine_nativeDispatch(env, NULL, handle, YAZA_MOBILE_EVENT_V1_INCREMENT, p_get);
        assert(r_get_ex == NULL);
        g_throw_on_get_region = 0;
        g_exception_pending = 0;
        mock_free_byte_array(p_get);
        mock_free_byte_array(p_empty);

        Java_dev_yaza_mobile_NativeEngine_nativeDestroy(env, NULL, handle);
    }

    assert(g_live_arrays == 0);
    puts("mobile JNI adapter v1: round-trip, OOM/exception local-ref cleanup, and leak check passed");
    return 0;
}

#ifndef YAZA_MOCK_JNI_H
#define YAZA_MOCK_JNI_H

#include <stdint.h>
#include <stddef.h>

#define JNIEXPORT
#define JNICALL
#define JNI_TRUE 1
#define JNI_FALSE 0

typedef int8_t jbyte;
typedef int32_t jint;
typedef int32_t jsize;
typedef int64_t jlong;
typedef uint8_t jboolean;

typedef struct YazaMockByteArray {
    jsize length;
    jbyte *bytes;
    int alive;
} YazaMockByteArray;

typedef void *jobject;
typedef YazaMockByteArray *jbyteArray;

typedef const struct JNINativeInterface_ *JNIEnv;

struct JNINativeInterface_ {
    jbyteArray (*NewByteArray)(JNIEnv *env, jsize length);
    jsize (*GetArrayLength)(JNIEnv *env, jbyteArray array);
    void (*GetByteArrayRegion)(JNIEnv *env, jbyteArray array, jsize start, jsize len, jbyte *buf);
    void (*SetByteArrayRegion)(JNIEnv *env, jbyteArray array, jsize start, jsize len, const jbyte *buf);
    jboolean (*ExceptionCheck)(JNIEnv *env);
    void (*DeleteLocalRef)(JNIEnv *env, jobject ref);
};

#endif

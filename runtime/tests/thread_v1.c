#include "yaza_runtime_v1.h"

#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>

#define WORKER_COUNT 8
#define ITERATIONS_PER_WORKER 100000ULL

typedef struct ThreadContext {
    _Atomic uint64_t *counter;
    uint64_t iterations;
} ThreadContext;

typedef struct ConditionContext {
    YazaMutexV1 *mutex;
    YazaConditionV1 *condition;
    int *ready;
    int *value;
} ConditionContext;

static void count_work(void *opaque) {
    ThreadContext *context = (ThreadContext *)opaque;
    for (uint64_t index = 0; index < context->iterations; ++index) {
        atomic_fetch_add_explicit(context->counter, 1, memory_order_relaxed);
    }
}

static void publish_condition_value(void *opaque) {
    ConditionContext *context = (ConditionContext *)opaque;
    assert(yaza_rt_v1_mutex_lock(context->mutex) == 0);
    *context->value = 42;
    *context->ready = 1;
    assert(yaza_rt_v1_condition_signal(context->condition) == 0);
    assert(yaza_rt_v1_mutex_unlock(context->mutex) == 0);
}

int main(void) {
    YazaThreadV1 *invalid_handle = (YazaThreadV1 *)(uintptr_t)1;
    assert(yaza_rt_v1_thread_spawn(NULL, NULL, &invalid_handle) == EINVAL);
    assert(invalid_handle == NULL);
    assert(yaza_rt_v1_thread_spawn(count_work, NULL, NULL) == EINVAL);

    _Atomic uint64_t counter = 0;
    ThreadContext contexts[WORKER_COUNT];
    YazaThreadV1 *threads[WORKER_COUNT] = {0};
    for (uint32_t index = 0; index < WORKER_COUNT; ++index) {
        contexts[index].counter = &counter;
        contexts[index].iterations = ITERATIONS_PER_WORKER;
        assert(yaza_rt_v1_thread_spawn(count_work, &contexts[index], &threads[index]) == 0);
        assert(threads[index] != NULL);
    }
    for (uint32_t index = 0; index < WORKER_COUNT; ++index) {
        assert(yaza_rt_v1_thread_join(threads[index]) == 0);
    }
    assert(atomic_load_explicit(&counter, memory_order_relaxed) == WORKER_COUNT * ITERATIONS_PER_WORKER);

    YazaMutexV1 *mutex = NULL;
    YazaConditionV1 *condition = NULL;
    assert(yaza_rt_v1_mutex_create(NULL) == EINVAL);
    assert(yaza_rt_v1_condition_create(NULL) == EINVAL);
    assert(yaza_rt_v1_mutex_create(&mutex) == 0 && mutex != NULL);
    assert(yaza_rt_v1_condition_create(&condition) == 0 && condition != NULL);
    assert(yaza_rt_v1_condition_wait(NULL, mutex) == EINVAL);
    assert(yaza_rt_v1_condition_signal(NULL) == EINVAL);
    assert(yaza_rt_v1_condition_broadcast(condition) == 0);
    int ready = 0;
    int value = 0;
    ConditionContext condition_context = {mutex, condition, &ready, &value};
    YazaThreadV1 *publisher = NULL;
    assert(yaza_rt_v1_mutex_lock(mutex) == 0);
    assert(yaza_rt_v1_thread_spawn(publish_condition_value, &condition_context, &publisher) == 0);
    while (!ready) assert(yaza_rt_v1_condition_wait(condition, mutex) == 0);
    assert(value == 42);
    assert(yaza_rt_v1_mutex_unlock(mutex) == 0);
    assert(yaza_rt_v1_thread_join(publisher) == 0);
    assert(yaza_rt_v1_condition_destroy(condition) == 0);
    assert(yaza_rt_v1_mutex_destroy(mutex) == 0);
    puts("thread-spawn-join-ok");
    return 0;
}

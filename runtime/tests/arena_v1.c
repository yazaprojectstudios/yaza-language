#include "yaza_runtime_v1.h"

#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define WORKER_COUNT 8
#define ALLOCATIONS_PER_WORKER 500
#define REQUEST_CHURN_COUNT 10000
#define SHARED_LIST_ITEMS_PER_WORKER 5000

typedef struct WorkerInput {
    YazaArenaV1 *arena;
    int failed;
} WorkerInput;

typedef struct SharedListWorkerInput {
    void *list;
    uint64_t start;
    uint64_t count;
} SharedListWorkerInput;

typedef struct SharedArenaWorkerInput {
    YazaArenaV1 *arena;
    uintptr_t allocations[ALLOCATIONS_PER_WORKER];
    uint64_t pattern_start;
    int failed;
} SharedArenaWorkerInput;

static void *allocate_repeatedly(void *opaque) {
    WorkerInput *input = (WorkerInput *)opaque;
    for (int index = 0; index < ALLOCATIONS_PER_WORKER; ++index) {
        void *allocation = yaza_arena_v1_alloc(input->arena, 24);
        if (allocation == NULL || (uintptr_t)allocation % _Alignof(max_align_t) != 0) {
            input->failed = 1;
            return NULL;
        }
    }
    return NULL;
}

static void *allocate_shared_arena_repeatedly(void *opaque) {
    SharedArenaWorkerInput *input = (SharedArenaWorkerInput *)opaque;
    for (int index = 0; index < ALLOCATIONS_PER_WORKER; ++index) {
        unsigned char *allocation = (unsigned char *)yaza_arena_v1_alloc(input->arena, 24);
        if (allocation == NULL || (uintptr_t)allocation % _Alignof(max_align_t) != 0) {
            input->failed = 1;
            return NULL;
        }
        const uint64_t marker = input->pattern_start + (uint64_t)index;
        memcpy(allocation, &marker, sizeof(marker));
        input->allocations[index] = (uintptr_t)allocation;
    }
    return NULL;
}

static void *push_shared_list_values(void *opaque) {
    SharedListWorkerInput *input = (SharedListWorkerInput *)opaque;
    for (uint64_t index = 0; index < input->count; ++index) {
        uint64_t value = input->start + index;
        assert(yaza_list_v1_push_typed(input->list, &value, "List[Number]", sizeof(value)) == 0);
    }
    return NULL;
}

int main(void) {
    yaza_rt_v1_init();
    assert(yaza_arena_v1_current() == yaza_rt_v1_program_arena_handle());
    YazaArenaContextV1 process_context = {0};
    assert(yaza_arena_v1_reset(yaza_rt_v1_program_arena_handle()) == 0);
    assert(yaza_arena_v1_scope_begin(yaza_rt_v1_program_arena_handle(), &process_context) == 0);
    YazaArenaV1 *process = yaza_arena_v1_create(8ULL * 1024ULL * 1024ULL);
    assert(process != NULL);
    assert(yaza_rt_v1_arena_scope_count() == 1);

    YazaArenaV1 *request_context_arena = yaza_arena_v1_create(128ULL * 1024ULL);
    assert(request_context_arena != NULL);
    assert(yaza_rt_v1_arena_scope_count() == 2);
    YazaArenaContextV1 request_context = {0};
    assert(yaza_arena_v1_enter(request_context_arena, &request_context) == 1);
    assert(yaza_arena_v1_release(request_context_arena) == 0);
    assert(yaza_arena_v1_current() == request_context_arena);
    void *aligned_allocations[] = {
        yaza_rt_v1_alloc(1), yaza_rt_v1_alloc(3), yaza_rt_v1_alloc(7), yaza_rt_v1_alloc(15), yaza_rt_v1_alloc(17)
    };
    const uint64_t requested_sizes[] = {1, 3, 7, 15, 17};
    uint64_t expected_storage_used = 0;
    for (size_t index = 0; index < sizeof(aligned_allocations) / sizeof(aligned_allocations[0]); ++index) {
        assert(aligned_allocations[index] != NULL);
        assert((uintptr_t)aligned_allocations[index] % _Alignof(max_align_t) == 0);
        const uint64_t alignment = (uint64_t)_Alignof(max_align_t);
        const uint64_t remainder = expected_storage_used % alignment;
        if (remainder != 0) expected_storage_used += alignment - remainder;
        expected_storage_used += requested_sizes[index];
    }
    assert(yaza_arena_v1_used(request_context_arena) == 43);
    assert(yaza_arena_v1_storage_used(request_context_arena) == expected_storage_used);
    assert(expected_storage_used > yaza_arena_v1_used(request_context_arena));
    assert(yaza_arena_v1_peak_storage_used(request_context_arena) == expected_storage_used);
    assert(yaza_arena_v1_peak_reserved(request_context_arena) == 64ULL * 1024ULL);
    assert(yaza_arena_v1_used(yaza_rt_v1_program_arena_handle()) == 0);
    assert(yaza_rt_v1_arena_used() == 43);
    assert(yaza_rt_v1_arena_storage_used() == expected_storage_used);
    assert(yaza_rt_v1_arena_reserved() == 64ULL * 1024ULL);
    assert(yaza_rt_v1_arena_peak_storage_used() >= expected_storage_used);
    assert(yaza_rt_v1_arena_peak_reserved() >= 64ULL * 1024ULL);
    assert(yaza_arena_v1_reset(request_context_arena) == 0);
    assert(yaza_arena_v1_leave(&request_context) == 1);
    assert(yaza_arena_v1_current() == yaza_rt_v1_program_arena_handle());
    assert(yaza_arena_v1_reset(request_context_arena) == 1);
    assert(yaza_arena_v1_storage_used(request_context_arena) == 0);
    assert(yaza_arena_v1_destroy(request_context_arena) == 1);
    assert(yaza_rt_v1_arena_storage_used() == 0);
    assert(yaza_rt_v1_arena_scope_count() == 1);

    YazaArenaV1 *list_scope = yaza_arena_v1_create(256ULL * 1024ULL);
    assert(list_scope != NULL);
    YazaArenaContextV1 list_context = {0};
    assert(yaza_arena_v1_enter(list_scope, &list_context) == 1);
    void *numbers = yaza_list_v1_create(1);
    assert(yaza_list_v1_count(numbers, 1) == 0);
    assert(yaza_list_v1_push(numbers, 42, 1) == 0);
    assert(yaza_list_v1_push(numbers, (uint64_t)-7, 1) == 0);
    assert(yaza_list_v1_count(numbers, 1) == 2);
    assert((int64_t)yaza_list_v1_get(numbers, 1, 1) == -7);
    uint64_t optional_raw = UINT64_MAX;
    assert(yaza_list_v1_try_get(numbers, -1, 1, &optional_raw) == 0);
    assert(optional_raw == 0);
    assert(yaza_list_v1_try_get(numbers, 1, 1, &optional_raw) == 1);
    assert((int64_t)optional_raw == -7);
    assert(yaza_list_v1_set(numbers, 1, 9, 1) == 0);
    assert(yaza_list_v1_get(numbers, 1, 1) == 9);

    char temporary_text[] = "copied into the owning region";
    void *texts = yaza_list_v1_create(5);
    YazaTextV1 borrowed_text = {(const unsigned char *)temporary_text, sizeof(temporary_text) - 1};
    assert(yaza_list_v1_push(texts, (uint64_t)(uintptr_t)&borrowed_text, 5) == 0);
    temporary_text[0] = 'X';
    const YazaTextV1 *stored_text = (const YazaTextV1 *)(uintptr_t)yaza_list_v1_get(texts, 0, 5);
    assert(stored_text->length == sizeof(temporary_text) - 1);
    assert(memcmp(stored_text->data, "copied into the owning region", (size_t)stored_text->length) == 0);
    assert(yaza_list_v1_try_get(texts, 0, 5, &optional_raw) == 1);
    stored_text = (const YazaTextV1 *)(uintptr_t)optional_raw;
    assert(stored_text->length == sizeof(temporary_text) - 1);
    assert(memcmp(stored_text->data, "copied into the owning region", (size_t)stored_text->length) == 0);
    assert(yaza_list_v1_try_get(texts, 10, 5, &optional_raw) == 0);
    assert(optional_raw == 0);
    assert(yaza_arena_v1_leave(&list_context) == 1);
    assert(yaza_arena_v1_reset(list_scope) == 1);
    assert(yaza_arena_v1_destroy(list_scope) == 1);

    for (int index = 0; index < REQUEST_CHURN_COUNT; ++index) {
        YazaArenaV1 *scope = yaza_arena_v1_create_child(process, 64ULL * 1024ULL);
        assert(scope != NULL);
        YazaArenaContextV1 context = {0};
        assert(yaza_arena_v1_enter(scope, &context) == 1);
        assert(yaza_rt_v1_alloc(128) != NULL);
        assert(yaza_arena_v1_leave(&context) == 1);
        assert(yaza_arena_v1_destroy(scope) == 1);
        assert(yaza_rt_v1_arena_used() == 0);
        assert(yaza_rt_v1_arena_reserved() == 0);
        assert(yaza_arena_v1_used(process) == 0);
        assert(yaza_arena_v1_reserved(process) == 0);
    }
    assert(yaza_rt_v1_arena_allocation_count() >= REQUEST_CHURN_COUNT);

    YazaArenaV1 *request = yaza_arena_v1_create_child(process, 2ULL * 1024ULL * 1024ULL);
    assert(request != NULL);
    YazaArenaV1 *task = yaza_arena_v1_create_child(request, 256ULL * 1024ULL);
    assert(task != NULL);
    assert(yaza_arena_v1_alloc(task, 32) != NULL);
    assert(yaza_arena_v1_used(task) == 32);
    assert(yaza_arena_v1_used(request) == 32);
    assert(yaza_arena_v1_used(process) == 32);
    assert(yaza_arena_v1_reset(request) == 0);
    assert(yaza_arena_v1_destroy(request) == 0);

    assert(yaza_arena_v1_retain(task) == 1);
    assert(yaza_arena_v1_reset(task) == 0);
    assert(yaza_arena_v1_destroy(task) == 0);
    assert(yaza_arena_v1_release(task) == 1);
    assert(yaza_arena_v1_reset(task) == 1);
    assert(yaza_arena_v1_generation(task) == 2);
    assert(yaza_arena_v1_used(request) == 0);
    assert(yaza_arena_v1_peak_used(task) == 0);

    pthread_t threads[WORKER_COUNT];
    WorkerInput inputs[WORKER_COUNT];
    for (int index = 0; index < WORKER_COUNT; ++index) {
        inputs[index].arena = yaza_arena_v1_create_child(request, 256ULL * 1024ULL);
        inputs[index].failed = 0;
        assert(inputs[index].arena != NULL);
        assert(pthread_create(&threads[index], NULL, allocate_repeatedly, &inputs[index]) == 0);
    }
    for (int index = 0; index < WORKER_COUNT; ++index) {
        assert(pthread_join(threads[index], NULL) == 0);
        assert(inputs[index].failed == 0);
        assert(yaza_arena_v1_allocation_count(inputs[index].arena) == ALLOCATIONS_PER_WORKER);
    }
    assert(yaza_rt_v1_arena_used() == (uint64_t)WORKER_COUNT * ALLOCATIONS_PER_WORKER * 24ULL);
    assert(yaza_rt_v1_arena_peak_used() >= yaza_rt_v1_arena_used());
    for (int index = 0; index < WORKER_COUNT; ++index) {
        assert(yaza_arena_v1_destroy(inputs[index].arena) == 1);
    }
    assert(yaza_arena_v1_used(request) == 0);
    assert(yaza_arena_v1_reserved(request) == 64ULL * 1024ULL);

    YazaArenaV1 *small = yaza_arena_v1_create_child(request, 128);
    assert(small != NULL);
    assert(yaza_arena_v1_alloc(small, 96) != NULL);
    assert(yaza_arena_v1_alloc(small, 40) == NULL);
    assert(yaza_arena_v1_used(small) == 96);
    assert(yaza_arena_v1_storage_used(small) >= 96);
    assert(yaza_arena_v1_peak_reserved(small) == 128);
    assert(yaza_arena_v1_reset(small) == 1);
    assert(yaza_arena_v1_used(small) == 0);
    assert(yaza_arena_v1_destroy(small) == 1);
    assert(yaza_rt_v1_arena_allocation_failures() > 0);

    assert(yaza_arena_v1_destroy(task) == 1);
    assert(yaza_arena_v1_used(request) == 0);
    assert(yaza_arena_v1_reserved(request) == 0);
    assert(yaza_arena_v1_destroy(request) == 1);
    assert(yaza_arena_v1_used(process) == 0);
    assert(yaza_arena_v1_destroy(process) == 1);
    assert(yaza_rt_v1_arena_scope_count() == 0);
    assert(yaza_rt_v1_arena_used() == 0);
    assert(yaza_rt_v1_arena_storage_used() == 0);
    assert(yaza_rt_v1_arena_reserved() == 0);

    YazaArenaV1 *allocation_count_scope = yaza_arena_v1_create(4096);
    assert(allocation_count_scope != NULL);
    const uint64_t allocations_before_reset = yaza_rt_v1_arena_allocation_count();
    assert(yaza_arena_v1_alloc(allocation_count_scope, 24) != NULL);
    assert(yaza_rt_v1_arena_allocation_count() == allocations_before_reset + 1);
    assert(yaza_arena_v1_reset(allocation_count_scope) == 1);
    assert(yaza_rt_v1_arena_allocation_count() == allocations_before_reset + 1);
    assert(yaza_arena_v1_alloc(allocation_count_scope, 24) != NULL);
    assert(yaza_rt_v1_arena_allocation_count() == allocations_before_reset + 2);
    assert(yaza_arena_v1_destroy(allocation_count_scope) == 1);
    assert(yaza_rt_v1_arena_allocation_count() == allocations_before_reset + 2);

    YazaArenaV1 *shared_allocation_scope = yaza_arena_v1_create(256ULL * 1024ULL);
    assert(shared_allocation_scope != NULL);
    const uint64_t process_allocations_before_shared_workers = yaza_rt_v1_arena_allocation_count();
    pthread_t shared_allocation_threads[WORKER_COUNT];
    SharedArenaWorkerInput shared_allocation_inputs[WORKER_COUNT];
    for (int worker = 0; worker < WORKER_COUNT; ++worker) {
        shared_allocation_inputs[worker].arena = shared_allocation_scope;
        shared_allocation_inputs[worker].pattern_start = (uint64_t)worker * ALLOCATIONS_PER_WORKER;
        shared_allocation_inputs[worker].failed = 0;
        assert(pthread_create(&shared_allocation_threads[worker], NULL, allocate_shared_arena_repeatedly,
            &shared_allocation_inputs[worker]) == 0);
    }
    for (int worker = 0; worker < WORKER_COUNT; ++worker) {
        assert(pthread_join(shared_allocation_threads[worker], NULL) == 0);
        assert(shared_allocation_inputs[worker].failed == 0);
    }
    for (int worker = 0; worker < WORKER_COUNT; ++worker) {
        for (int index = 0; index < ALLOCATIONS_PER_WORKER; ++index) {
            const uintptr_t address = shared_allocation_inputs[worker].allocations[index];
            uint64_t marker = 0;
            memcpy(&marker, (const void *)address, sizeof(marker));
            assert(marker == (uint64_t)worker * ALLOCATIONS_PER_WORKER + (uint64_t)index);
            for (int earlier_worker = 0; earlier_worker <= worker; ++earlier_worker) {
                const int earlier_count = earlier_worker == worker ? index : ALLOCATIONS_PER_WORKER;
                for (int earlier_index = 0; earlier_index < earlier_count; ++earlier_index) {
                    assert(address != shared_allocation_inputs[earlier_worker].allocations[earlier_index]);
                }
            }
        }
    }
    assert(yaza_arena_v1_allocation_count(shared_allocation_scope) ==
        (uint64_t)WORKER_COUNT * ALLOCATIONS_PER_WORKER);
    assert(yaza_rt_v1_arena_allocation_count() == process_allocations_before_shared_workers +
        (uint64_t)WORKER_COUNT * ALLOCATIONS_PER_WORKER);
    assert(yaza_arena_v1_used(shared_allocation_scope) ==
        (uint64_t)WORKER_COUNT * ALLOCATIONS_PER_WORKER * 24ULL);
    assert(yaza_arena_v1_destroy(shared_allocation_scope) == 1);

    pthread_t independent_threads[WORKER_COUNT];
    WorkerInput independent_inputs[WORKER_COUNT];
    for (int index = 0; index < WORKER_COUNT; ++index) {
        independent_inputs[index].arena = yaza_arena_v1_create(256ULL * 1024ULL);
        independent_inputs[index].failed = 0;
        assert(independent_inputs[index].arena != NULL);
        assert(pthread_create(&independent_threads[index], NULL, allocate_repeatedly, &independent_inputs[index]) == 0);
    }
    for (int index = 0; index < WORKER_COUNT; ++index) {
        assert(pthread_join(independent_threads[index], NULL) == 0);
        assert(independent_inputs[index].failed == 0);
    }
    assert(yaza_rt_v1_arena_used() == (uint64_t)WORKER_COUNT * ALLOCATIONS_PER_WORKER * 24ULL);
    for (int index = 0; index < WORKER_COUNT; ++index) {
        assert(yaza_arena_v1_destroy(independent_inputs[index].arena) == 1);
    }
    assert(yaza_rt_v1_arena_used() == 0);
    assert(yaza_rt_v1_arena_storage_used() == 0);
    assert(yaza_rt_v1_arena_reserved() == 0);

    YazaArenaV1 *shared_list_scope = yaza_arena_v1_create(2ULL * 1024ULL * 1024ULL);
    assert(shared_list_scope != NULL);
    YazaArenaContextV1 shared_list_context = {0};
    assert(yaza_arena_v1_enter(shared_list_scope, &shared_list_context) == 1);
    void *shared_list = yaza_list_v1_create_typed("List[Number]", sizeof(uint64_t));
    assert(yaza_arena_v1_leave(&shared_list_context) == 1);
    pthread_t list_threads[WORKER_COUNT];
    SharedListWorkerInput list_inputs[WORKER_COUNT];
    for (uint64_t worker = 0; worker < WORKER_COUNT; ++worker) {
        list_inputs[worker].list = shared_list;
        list_inputs[worker].start = worker * SHARED_LIST_ITEMS_PER_WORKER;
        list_inputs[worker].count = SHARED_LIST_ITEMS_PER_WORKER;
        assert(pthread_create(&list_threads[worker], NULL, push_shared_list_values, &list_inputs[worker]) == 0);
    }
    for (int worker = 0; worker < WORKER_COUNT; ++worker) {
        assert(pthread_join(list_threads[worker], NULL) == 0);
    }
    const uint64_t expected_shared_count = (uint64_t)WORKER_COUNT * SHARED_LIST_ITEMS_PER_WORKER;
    assert(yaza_list_v1_count_typed(shared_list, "List[Number]", sizeof(uint64_t)) == expected_shared_count);
    uint64_t shared_sum = 0;
    for (uint64_t index = 0; index < expected_shared_count; ++index) {
        uint64_t value = 0;
        assert(yaza_list_v1_get_typed(shared_list, (int64_t)index, "List[Number]", sizeof(value), &value) == 0);
        shared_sum += value;
    }
    assert(shared_sum == expected_shared_count * (expected_shared_count - 1) / 2);
    assert(yaza_arena_v1_reset(shared_list_scope) == 1);
    assert(yaza_arena_v1_destroy(shared_list_scope) == 1);
    assert(yaza_rt_v1_arena_used() == 0);
    assert(yaza_rt_v1_arena_reserved() == 0);
    puts("arena-v1: bounded allocation, typed lists, concurrent List access, 10,000 request scopes, leases, and reclamation passed");
    return 0;
}

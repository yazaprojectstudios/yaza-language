#include "yaza_runtime_v1.h"

#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <signal.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct TestProduct {
    int64_t id;
    const char *name;
} TestProduct;

typedef struct TestOption {
    int32_t tag;
    void *payload;
} TestOption;

typedef struct TestResult {
    int32_t tag;
    uint32_t padding;
    union {
        int64_t number;
        void *text;
    } payload;
} TestResult;

typedef struct TypedListMutation {
    void *list;
    atomic_bool done;
} TypedListMutation;

static void *mutate_typed_list_during_iteration(void *opaque) {
    TypedListMutation *mutation = (TypedListMutation *)opaque;
    for (int64_t value = 1; value <= 5000; ++value) {
        (void)yaza_list_v1_set_typed(mutation->list, 0, &value, "List[Number]", sizeof(value));
    }
    atomic_store_explicit(&mutation->done, true, memory_order_release);
    return NULL;
}

typedef enum WrongSignatureOperation {
    WRONG_SIGNATURE_COUNT,
    WRONG_SIGNATURE_PUSH,
    WRONG_SIGNATURE_TRY_PUSH,
    WRONG_SIGNATURE_TRY_RESERVE,
    WRONG_SIGNATURE_GET,
    WRONG_SIGNATURE_TRY_GET,
    WRONG_SIGNATURE_SET,
    WRONG_SIGNATURE_TRY_SET,
} WrongSignatureOperation;

static void assert_wrong_signature_rejected(void *list, WrongSignatureOperation operation) {
    pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        struct rlimit no_core = {0, 0};
        (void)setrlimit(RLIMIT_CORE, &no_core);
        (void)freopen("/dev/null", "w", stderr);
        int32_t value = 7;
        int32_t out_value = 0;
        switch (operation) {
        case WRONG_SIGNATURE_COUNT:
            (void)yaza_list_v1_count_typed(list, "List[Int32]", sizeof(value));
            break;
        case WRONG_SIGNATURE_PUSH:
            (void)yaza_list_v1_push_typed(list, &value, "List[Int32]", sizeof(value));
            break;
        case WRONG_SIGNATURE_TRY_PUSH:
            (void)yaza_list_v1_try_push_typed(list, &value, "List[Int32]", sizeof(value));
            break;
        case WRONG_SIGNATURE_TRY_RESERVE:
            (void)yaza_list_v1_try_reserve_typed(list, 32, "List[Int32]", sizeof(value));
            break;
        case WRONG_SIGNATURE_GET:
            (void)yaza_list_v1_get_typed(list, 0, "List[Int32]", sizeof(value), &out_value);
            break;
        case WRONG_SIGNATURE_TRY_GET:
            (void)yaza_list_v1_try_get_typed(list, 0, "List[Int32]", sizeof(value), &out_value);
            break;
        case WRONG_SIGNATURE_SET:
            (void)yaza_list_v1_set_typed(list, 0, &value, "List[Int32]", sizeof(value));
            break;
        case WRONG_SIGNATURE_TRY_SET:
            (void)yaza_list_v1_try_set_typed(list, 0, &value, "List[Int32]", sizeof(value));
            break;
        }
        _exit(0);
    }
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status));
    assert(WTERMSIG(status) == SIGABRT);
}

int main(void) {
    yaza_rt_v1_init();
    YazaArenaV1 *arena = yaza_arena_v1_create(256ULL * 1024ULL);
    assert(arena != NULL);
    YazaArenaContextV1 context = {0};
    assert(yaza_arena_v1_scope_begin(arena, &context) == 1);

    char number_signature[] = "List[Number]";
    void *numbers = yaza_list_v1_create_typed(number_signature, sizeof(int64_t));
    memset(number_signature, 'x', sizeof(number_signature) - 1);
    int64_t first_number = 41;
    int64_t second_number = 42;
    assert(yaza_list_v1_push_typed(numbers, &first_number, "List[Number]", sizeof(first_number)) == 0);
    assert(yaza_list_v1_push_typed(numbers, &second_number, "List[Number]", sizeof(second_number)) == 0);
    assert(yaza_list_v1_count_typed(numbers, "List[Number]", sizeof(first_number)) == 2);
    int64_t copied_number = 0;
    assert(yaza_list_v1_get_typed(numbers, 1, "List[Number]", sizeof(copied_number), &copied_number) == 0);
    assert(copied_number == 42);
    copied_number = 123;
    assert(yaza_list_v1_try_get_typed(numbers, -1, "List[Number]", sizeof(copied_number), &copied_number) == 0);
    assert(copied_number == 0);
    copied_number = 123;
    assert(yaza_list_v1_try_get_typed(numbers, 2, "List[Number]", sizeof(copied_number), &copied_number) == 0);
    assert(copied_number == 0);
    assert(yaza_list_v1_try_get_typed(numbers, 0, "List[Number]", sizeof(copied_number), &copied_number) == 1);
    assert(copied_number == 41);
    assert_wrong_signature_rejected(numbers, WRONG_SIGNATURE_COUNT);
    assert_wrong_signature_rejected(numbers, WRONG_SIGNATURE_PUSH);
    assert_wrong_signature_rejected(numbers, WRONG_SIGNATURE_TRY_PUSH);
    assert_wrong_signature_rejected(numbers, WRONG_SIGNATURE_TRY_RESERVE);
    assert_wrong_signature_rejected(numbers, WRONG_SIGNATURE_GET);
    assert_wrong_signature_rejected(numbers, WRONG_SIGNATURE_TRY_GET);
    assert_wrong_signature_rejected(numbers, WRONG_SIGNATURE_SET);
    assert_wrong_signature_rejected(numbers, WRONG_SIGNATURE_TRY_SET);
    second_number = 99;
    assert(yaza_list_v1_set_typed(numbers, 1, &second_number, "List[Number]", sizeof(second_number)) == 0);
    assert(yaza_list_v1_get_typed(numbers, 1, "List[Number]", sizeof(copied_number), &copied_number) == 0);
    assert(copied_number == 99);
    assert(yaza_list_v1_try_reserve_typed(numbers, 64, "List[Number]", sizeof(int64_t)) == 1);
    assert(yaza_list_v1_try_reserve_typed(numbers, 16, "List[Number]", sizeof(int64_t)) == 1);
    assert(yaza_list_v1_try_reserve_typed(numbers, -1, "List[Number]", sizeof(int64_t)) == 0);
    assert(yaza_list_v1_try_reserve_typed(numbers, INT64_MAX, "List[Number]", sizeof(int64_t)) == 0);
    assert(yaza_list_v1_count_typed(numbers, "List[Number]", sizeof(int64_t)) == 2);
    assert(yaza_list_v1_get_typed(numbers, 1, "List[Number]", sizeof(copied_number), &copied_number) == 0);
    assert(copied_number == 99);

    YazaListBorrowV1 number_borrow = {0};
    yaza_list_v1_iter_begin_typed(numbers, "List[Number]", sizeof(int64_t), &number_borrow);
    assert(yaza_list_v1_iter_count(&number_borrow) == 2);
    assert(yaza_arena_v1_scope_end(&context) == 0);
    assert(yaza_list_v1_iter_get_typed(&number_borrow, 0, &copied_number) == 0);
    assert(copied_number == 41);
    second_number = 100;
    assert(yaza_list_v1_set_typed(numbers, 1, &second_number, "List[Number]", sizeof(second_number)) == 0);
    assert(yaza_list_v1_iter_get_typed(&number_borrow, 1, &copied_number) == 0);
    assert(copied_number == 100);
    int64_t appended_number = 101;
    assert(yaza_list_v1_push_typed(numbers, &appended_number, "List[Number]", sizeof(appended_number)) == 0);
    assert(yaza_list_v1_iter_count(&number_borrow) == 2);
    yaza_list_v1_iter_end(&number_borrow);
    assert(number_borrow.list == NULL && number_borrow.owner == NULL && number_borrow.length == 0 && number_borrow.element_size == 0);

    YazaListBorrowV1 number_read_view = {0};
    yaza_list_v1_read_begin_typed(numbers, "List[Number]", sizeof(int64_t), &number_read_view);
    assert(yaza_list_v1_read_get_typed(&number_read_view, 2, &copied_number) == 0);
    assert(copied_number == 101);
    assert(yaza_list_v1_try_reserve_typed(numbers, 128, "List[Number]", sizeof(int64_t)) == 1);
    assert(yaza_list_v1_read_get_typed(&number_read_view, 2, &copied_number) == 0);
    assert(copied_number == 101);
    int64_t updated_number = 102;
    assert(yaza_list_v1_try_set_typed(numbers, 2, &updated_number, "List[Number]", sizeof(updated_number)) == 1);
    assert(yaza_list_v1_read_get_typed(&number_read_view, 2, &copied_number) == 0);
    assert(copied_number == 102);
    assert(yaza_list_v1_try_set_typed(numbers, 99, &updated_number, "List[Number]", sizeof(updated_number)) == 0);
    int64_t appended_after_read_view = 103;
    assert(yaza_list_v1_push_typed(numbers, &appended_after_read_view, "List[Number]", sizeof(appended_after_read_view)) == 0);
    assert(yaza_list_v1_read_get_typed(&number_read_view, 3, &copied_number) == 0);
    assert(copied_number == 103);
    yaza_list_v1_read_end(&number_read_view);
    assert(number_read_view.list == NULL && number_read_view.owner == NULL && number_read_view.length == 0 && number_read_view.element_size == 0);

    void *products = yaza_list_v1_create_typed("List[Product]", sizeof(TestProduct));
    TestProduct product = {7, "typed-product"};
    assert(yaza_list_v1_push_typed(products, &product, "List[Product]", sizeof(product)) == 0);
    TestProduct copied_product = {0};
    assert(yaza_list_v1_get_typed(products, 0, "List[Product]", sizeof(copied_product), &copied_product) == 0);
    assert(copied_product.id == 7);
    assert(copied_product.name == product.name);

    void *options = yaza_list_v1_create_typed("List[Option[Number]]", sizeof(TestOption));
    int64_t payload = 17;
    TestOption option = {1, &payload};
    assert(yaza_list_v1_push_typed(options, &option, "List[Option[Number]]", sizeof(option)) == 0);
    TestOption copied_option = {0};
    assert(yaza_list_v1_get_typed(options, 0, "List[Option[Number]]", sizeof(copied_option), &copied_option) == 0);
    assert(copied_option.tag == 1);
    assert(copied_option.payload == &payload);

    void *texts = yaza_list_v1_create_typed("TextList", sizeof(const YazaTextV1 *));
    char source_text[] = "arena-owned";
    YazaTextV1 borrowed_text = {(const unsigned char *)source_text, sizeof(source_text) - 1};
    const YazaTextV1 *text_pointer = &borrowed_text;
    assert(yaza_list_v1_push_typed(texts, &text_pointer, "TextList", sizeof(text_pointer)) == 0);
    memset(source_text, 'x', sizeof(source_text) - 1);
    const YazaTextV1 *copied_text = NULL;
    assert(yaza_list_v1_get_typed(texts, 0, "TextList", sizeof(copied_text), &copied_text) == 0);
    assert(copied_text->length == 11 && memcmp(copied_text->data, "arena-owned", 11) == 0);

    assert(yaza_arena_v1_used(arena) > 0);
    assert(yaza_arena_v1_scope_end(&context) == 1);
    assert(yaza_arena_v1_used(arena) == 0);
    assert(yaza_arena_v1_destroy(arena) == 1);
    assert(yaza_rt_v1_arena_used() == 0);
    assert(yaza_rt_v1_arena_reserved() == 0);

    YazaArenaV1 *small_arena = yaza_arena_v1_create(64ULL * 1024ULL);
    assert(small_arena != NULL);
    YazaArenaContextV1 small_context = {0};
    assert(yaza_arena_v1_scope_begin(small_arena, &small_context) == 1);
    void *bounded_numbers = yaza_list_v1_create_typed("List[Number]", sizeof(uint64_t));
    uint64_t accepted = 0;
    int32_t appended = 1;
    while (appended != 0) {
        uint64_t candidate = accepted;
        appended = yaza_list_v1_try_push_typed(bounded_numbers, &candidate, "List[Number]", sizeof(candidate));
        if (appended != 0) accepted += 1;
    }
    assert(accepted > 0);
    assert(yaza_list_v1_count_typed(bounded_numbers, "List[Number]", sizeof(uint64_t)) == accepted);
    uint64_t last_number = UINT64_MAX;
    assert(yaza_list_v1_get_typed(bounded_numbers, (int64_t)(accepted - 1), "List[Number]", sizeof(last_number), &last_number) == 0);
    assert(last_number == accepted - 1);
    uint64_t rejected_number = accepted;
    assert(yaza_list_v1_try_push_typed(bounded_numbers, &rejected_number, "List[Number]", sizeof(rejected_number)) == 0);
    assert(yaza_list_v1_try_reserve_typed(bounded_numbers, (int64_t)(accepted + 100), "List[Number]", sizeof(rejected_number)) == 0);
    assert(yaza_list_v1_count_typed(bounded_numbers, "List[Number]", sizeof(uint64_t)) == accepted);
    uint64_t last_preserved_after_reserve_failure = 0;
    assert(yaza_list_v1_get_typed(bounded_numbers, (int64_t)(accepted - 1), "List[Number]", sizeof(last_preserved_after_reserve_failure), &last_preserved_after_reserve_failure) == 0);
    assert(last_preserved_after_reserve_failure == accepted - 1);
    assert(yaza_rt_v1_arena_allocation_failures() > 0);
    assert(yaza_arena_v1_scope_end(&small_context) == 1);
    assert(yaza_arena_v1_used(small_arena) == 0);
    assert(yaza_arena_v1_destroy(small_arena) == 1);
    assert(yaza_rt_v1_arena_used() == 0);
    assert(yaza_rt_v1_arena_reserved() == 0);

    YazaArenaV1 *aggregate_quota_arena = yaza_arena_v1_create(64ULL * 1024ULL);
    assert(aggregate_quota_arena != NULL);
    YazaArenaContextV1 aggregate_quota_context = {0};
    assert(yaza_arena_v1_scope_begin(aggregate_quota_arena, &aggregate_quota_context) == 1);
    static const struct {
        const char *signature;
        uint64_t element_size;
    } aggregate_quota_cases[] = {
        {"List[Number]", sizeof(int64_t)},
        {"List[Int32]", sizeof(int32_t)},
        {"List[Float64]", sizeof(double)},
        {"List[Boolean]", sizeof(uint8_t)},
        {"List[Text]", sizeof(const YazaTextV1 *)},
        {"TextList", sizeof(const YazaTextV1 *)},
        {"List[Bytes]", sizeof(YazaBytesV1)},
        {"List[Product]", sizeof(TestProduct)},
        {"List[Variant]", sizeof(TestOption)},
        {"List[Option[Number]]", sizeof(TestOption)},
        {"List[Result[Number, Text]]", sizeof(TestResult)},
        {"List[List[Number]]", sizeof(void *)},
    };
    void *aggregate_quota_lists[sizeof(aggregate_quota_cases) / sizeof(aggregate_quota_cases[0])];
    unsigned char aggregate_values[sizeof(aggregate_quota_cases) / sizeof(aggregate_quota_cases[0])][64] = {{0}};
    for (size_t index = 0; index < sizeof(aggregate_quota_cases) / sizeof(aggregate_quota_cases[0]); ++index) {
        aggregate_quota_lists[index] = yaza_list_v1_create_typed(aggregate_quota_cases[index].signature, aggregate_quota_cases[index].element_size);
    }
    void *quota_filler = yaza_list_v1_create_typed("List[Number]", sizeof(uint64_t));
    void *quota_texts = yaza_list_v1_create_typed("TextList", sizeof(const YazaTextV1 *));
    static const unsigned char original_quota_text_data[] = "original";
    static const unsigned char replacement_quota_text_data[] = "replacement";
    YazaTextV1 original_quota_text = {original_quota_text_data, sizeof(original_quota_text_data) - 1};
    YazaTextV1 replacement_quota_text = {replacement_quota_text_data, sizeof(replacement_quota_text_data) - 1};
    const YazaTextV1 *original_quota_text_pointer = &original_quota_text;
    const YazaTextV1 *replacement_quota_text_pointer = &replacement_quota_text;
    assert(yaza_list_v1_push_typed(quota_texts, &original_quota_text_pointer, "TextList", sizeof(original_quota_text_pointer)) == 0);
    uint64_t filler_value = 0;
    while (yaza_list_v1_try_push_typed(quota_filler, &filler_value, "List[Number]", sizeof(filler_value)) != 0) {
        filler_value += 1;
    }
    while (yaza_arena_v1_alloc(aggregate_quota_arena, 8) != NULL) {
    }
    for (size_t index = 0; index < sizeof(aggregate_quota_cases) / sizeof(aggregate_quota_cases[0]); ++index) {
        assert(yaza_list_v1_try_push_typed(aggregate_quota_lists[index], aggregate_values[index], aggregate_quota_cases[index].signature, aggregate_quota_cases[index].element_size) == 0);
        assert(yaza_list_v1_count_typed(aggregate_quota_lists[index], aggregate_quota_cases[index].signature, aggregate_quota_cases[index].element_size) == 0);
    }
    assert(yaza_list_v1_try_set_typed(quota_texts, 0, &replacement_quota_text_pointer, "TextList", sizeof(replacement_quota_text_pointer)) == 0);
    const YazaTextV1 *preserved_quota_text = NULL;
    assert(yaza_list_v1_get_typed(quota_texts, 0, "TextList", sizeof(preserved_quota_text), &preserved_quota_text) == 0);
    assert(preserved_quota_text->length == sizeof(original_quota_text_data) - 1);
    assert(memcmp(preserved_quota_text->data, original_quota_text_data, sizeof(original_quota_text_data) - 1) == 0);
    assert(yaza_arena_v1_scope_end(&aggregate_quota_context) == 1);
    assert(yaza_arena_v1_used(aggregate_quota_arena) == 0);
    assert(yaza_arena_v1_destroy(aggregate_quota_arena) == 1);
    assert(yaza_rt_v1_arena_used() == 0);
    assert(yaza_rt_v1_arena_reserved() == 0);

    YazaArenaContextV1 program_context = {0};
    assert(yaza_arena_v1_enter(yaza_rt_v1_program_arena_handle(), &program_context) == 1);
    void *concurrent_numbers = yaza_list_v1_create_typed("List[Number]", sizeof(int64_t));
    int64_t concurrent_initial = 0;
    assert(yaza_list_v1_push_typed(concurrent_numbers, &concurrent_initial, "List[Number]", sizeof(concurrent_initial)) == 0);
    YazaListBorrowV1 concurrent_borrow = {0};
    yaza_list_v1_iter_begin_typed(concurrent_numbers, "List[Number]", sizeof(int64_t), &concurrent_borrow);
    YazaListBorrowV1 concurrent_read_view = {0};
    yaza_list_v1_read_begin_typed(concurrent_numbers, "List[Number]", sizeof(int64_t), &concurrent_read_view);
    TypedListMutation mutation = {.list = concurrent_numbers};
    atomic_init(&mutation.done, false);
    pthread_t mutation_thread;
    assert(pthread_create(&mutation_thread, NULL, mutate_typed_list_during_iteration, &mutation) == 0);
    int64_t concurrent_read = 0;
    do {
        assert(yaza_list_v1_iter_get_typed(&concurrent_borrow, 0, &concurrent_read) == 0);
        assert(concurrent_read >= 0 && concurrent_read <= 5000);
        assert(yaza_list_v1_read_get_typed(&concurrent_read_view, 0, &concurrent_read) == 0);
        assert(concurrent_read >= 0 && concurrent_read <= 5000);
    } while (!atomic_load_explicit(&mutation.done, memory_order_acquire));
    assert(pthread_join(mutation_thread, NULL) == 0);
    yaza_list_v1_iter_end(&concurrent_borrow);
    assert(yaza_list_v1_get_typed(concurrent_numbers, 0, "List[Number]", sizeof(concurrent_read), &concurrent_read) == 0);
    assert(concurrent_read == 5000);
    yaza_list_v1_read_end(&concurrent_read_view);
    assert(yaza_arena_v1_leave(&program_context) == 1);

    puts("runtime-v1: typed scalar and aggregate List operations, Text ownership, exact signatures, reserved capacity, borrowed iteration and indexed reads with concurrent mutation, live indexed length, fallible update and reservation bounds, quota-exhausted fallible append/reserve, and scope reclamation passed");
    return 0;
}

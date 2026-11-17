#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
RUNTIME_DIR="$PROJECT_DIR/runtime"
RUNTIME_CC=${CC:-clang}
LLVM_CLANG=${LLVM_CLANG:-clang}
LLVM_COMPILER=${YAZA_LLVM_BIN:-"$PROJECT_DIR/build/yaza-llvm"}
TEMP_ROOT=${TMPDIR:-/tmp}
TEMP_ROOT=${TEMP_ROOT%/}
BUILD_DIR=$(mktemp -d "$TEMP_ROOT/yaza-runtime-v1-sanitizers.XXXXXX")
trap 'rm -rf "$BUILD_DIR"' EXIT HUP INT TERM

ASAN_OPTIONS=${ASAN_OPTIONS:+$ASAN_OPTIONS:}detect_leaks=0
export ASAN_OPTIONS

validate_llvm_scope_asan() {
  source_name=$1
  YAZA_STDLIB_DIR="$PROJECT_DIR/std" "$LLVM_COMPILER" emit-llvm \
    "$PROJECT_DIR/examples/$source_name.yz" -o "$BUILD_DIR/$source_name.ll"
  "$LLVM_CLANG" -Wno-override-module -fsanitize=address,undefined -fno-omit-frame-pointer \
    "$BUILD_DIR/$source_name.ll" "$RUNTIME_DIR/src/yaza_runtime_v1.c" \
    -I"$RUNTIME_DIR/include" -pthread -lm -o "$BUILD_DIR/$source_name-asan"
  "$BUILD_DIR/$source_name-asan" >"$BUILD_DIR/$source_name.out" 2>"$BUILD_DIR/$source_name.err"
}

validate_llvm_fallible_bytes_result_oom() {
  source="$RUNTIME_DIR/tests/fixtures/fallible-bytes-result-oom.yz"
  YAZA_STDLIB_DIR="$PROJECT_DIR/std" "$LLVM_COMPILER" emit-llvm \
    "$source" -o "$BUILD_DIR/fallible-bytes-result-oom.ll"
  "$LLVM_CLANG" -Wno-override-module -fsanitize=address,undefined -fno-omit-frame-pointer \
    "$BUILD_DIR/fallible-bytes-result-oom.ll" "$RUNTIME_DIR/src/yaza_runtime_v1.c" \
    -I"$RUNTIME_DIR/include" -pthread -lm -o "$BUILD_DIR/fallible-bytes-result-oom-asan"
  printf 'oom-result-recovered\n' >"$BUILD_DIR/fallible-bytes-result-oom.expected.out"
  YAZA_RT_V1_PROCESS_LIMIT_BYTES=65536 "$BUILD_DIR/fallible-bytes-result-oom-asan" \
    >"$BUILD_DIR/fallible-bytes-result-oom.out" 2>"$BUILD_DIR/fallible-bytes-result-oom.err"
  cmp "$BUILD_DIR/fallible-bytes-result-oom.expected.out" "$BUILD_DIR/fallible-bytes-result-oom.out"
  if [ -s "$BUILD_DIR/fallible-bytes-result-oom.err" ]; then
    cat "$BUILD_DIR/fallible-bytes-result-oom.err" >&2
    exit 1
  fi
}

validate_llvm_fallible_text_concat_oom() {
  source="$RUNTIME_DIR/tests/fixtures/fallible-text-concat-oom.yz"
  YAZA_STDLIB_DIR="$PROJECT_DIR/std" "$LLVM_COMPILER" emit-llvm \
    "$source" -o "$BUILD_DIR/fallible-text-concat-oom.ll"
  "$LLVM_CLANG" -Wno-override-module -fsanitize=address,undefined -fno-omit-frame-pointer \
    "$BUILD_DIR/fallible-text-concat-oom.ll" "$RUNTIME_DIR/src/yaza_runtime_v1.c" \
    -I"$RUNTIME_DIR/include" -pthread -lm -o "$BUILD_DIR/fallible-text-concat-oom-asan"
  printf 'text-oom-result-recovered\n' >"$BUILD_DIR/fallible-text-concat-oom.expected.out"
  YAZA_RT_V1_PROCESS_LIMIT_BYTES=65536 "$BUILD_DIR/fallible-text-concat-oom-asan" \
    >"$BUILD_DIR/fallible-text-concat-oom.out" 2>"$BUILD_DIR/fallible-text-concat-oom.err"
  cmp "$BUILD_DIR/fallible-text-concat-oom.expected.out" "$BUILD_DIR/fallible-text-concat-oom.out"
  if [ -s "$BUILD_DIR/fallible-text-concat-oom.err" ]; then
    cat "$BUILD_DIR/fallible-text-concat-oom.err" >&2
    exit 1
  fi
}

validate_llvm_fallible_text_repeat_oom() {
  source="$RUNTIME_DIR/tests/fixtures/fallible-text-repeat-oom.yz"
  YAZA_STDLIB_DIR="$PROJECT_DIR/std" "$LLVM_COMPILER" emit-llvm \
    "$source" -o "$BUILD_DIR/fallible-text-repeat-oom.ll"
  "$LLVM_CLANG" -Wno-override-module -fsanitize=address,undefined -fno-omit-frame-pointer \
    "$BUILD_DIR/fallible-text-repeat-oom.ll" "$RUNTIME_DIR/src/yaza_runtime_v1.c" \
    -I"$RUNTIME_DIR/include" -pthread -lm -o "$BUILD_DIR/fallible-text-repeat-oom-asan"
  printf 'text-repeat-oom-result-recovered\n' >"$BUILD_DIR/fallible-text-repeat-oom.expected.out"
  YAZA_RT_V1_PROCESS_LIMIT_BYTES=65536 "$BUILD_DIR/fallible-text-repeat-oom-asan" \
    >"$BUILD_DIR/fallible-text-repeat-oom.out" 2>"$BUILD_DIR/fallible-text-repeat-oom.err"
  cmp "$BUILD_DIR/fallible-text-repeat-oom.expected.out" "$BUILD_DIR/fallible-text-repeat-oom.out"
  if [ -s "$BUILD_DIR/fallible-text-repeat-oom.err" ]; then
    cat "$BUILD_DIR/fallible-text-repeat-oom.err" >&2
    exit 1
  fi
}

validate_llvm_fallible_text_join_oom() {
  source="$RUNTIME_DIR/tests/fixtures/fallible-text-join-oom.yz"
  YAZA_STDLIB_DIR="$PROJECT_DIR/std" "$LLVM_COMPILER" emit-llvm \
    "$source" -o "$BUILD_DIR/fallible-text-join-oom.ll"
  "$LLVM_CLANG" -Wno-override-module -fsanitize=address,undefined -fno-omit-frame-pointer \
    "$BUILD_DIR/fallible-text-join-oom.ll" "$RUNTIME_DIR/src/yaza_runtime_v1.c" \
    -I"$RUNTIME_DIR/include" -pthread -lm -o "$BUILD_DIR/fallible-text-join-oom-asan"
  printf 'text-join-oom-result-recovered\n' >"$BUILD_DIR/fallible-text-join-oom.expected.out"
  YAZA_RT_V1_PROCESS_LIMIT_BYTES=65536 "$BUILD_DIR/fallible-text-join-oom-asan" \
    >"$BUILD_DIR/fallible-text-join-oom.out" 2>"$BUILD_DIR/fallible-text-join-oom.err"
  cmp "$BUILD_DIR/fallible-text-join-oom.expected.out" "$BUILD_DIR/fallible-text-join-oom.out"
  if [ -s "$BUILD_DIR/fallible-text-join-oom.err" ]; then
    cat "$BUILD_DIR/fallible-text-join-oom.err" >&2
    exit 1
  fi
}

validate_llvm_fallible_text_slice_oom() {
  source="$RUNTIME_DIR/tests/fixtures/fallible-text-slice-oom.yz"
  YAZA_STDLIB_DIR="$PROJECT_DIR/std" "$LLVM_COMPILER" emit-llvm \
    "$source" -o "$BUILD_DIR/fallible-text-slice-oom.ll"
  "$LLVM_CLANG" -Wno-override-module -fsanitize=address,undefined -fno-omit-frame-pointer \
    "$BUILD_DIR/fallible-text-slice-oom.ll" "$RUNTIME_DIR/src/yaza_runtime_v1.c" \
    -I"$RUNTIME_DIR/include" -pthread -lm -o "$BUILD_DIR/fallible-text-slice-oom-asan"
  printf 'text-slice-oom-recovered\n' >"$BUILD_DIR/fallible-text-slice-oom.expected.out"
  "$BUILD_DIR/fallible-text-slice-oom-asan" \
    >"$BUILD_DIR/fallible-text-slice-oom.out" 2>"$BUILD_DIR/fallible-text-slice-oom.err"
  cmp "$BUILD_DIR/fallible-text-slice-oom.expected.out" "$BUILD_DIR/fallible-text-slice-oom.out"
  if [ -s "$BUILD_DIR/fallible-text-slice-oom.err" ]; then
    cat "$BUILD_DIR/fallible-text-slice-oom.err" >&2
    exit 1
  fi
}

validate_llvm_fallible_text_collections_oom() {
  source="$RUNTIME_DIR/tests/fixtures/fallible-text-collections-oom.yz"
  YAZA_STDLIB_DIR="$PROJECT_DIR/std" "$LLVM_COMPILER" emit-llvm \
    "$source" -o "$BUILD_DIR/fallible-text-collections-oom.ll"
  "$LLVM_CLANG" -Wno-override-module -fsanitize=address,undefined -fno-omit-frame-pointer \
    "$BUILD_DIR/fallible-text-collections-oom.ll" "$RUNTIME_DIR/src/yaza_runtime_v1.c" \
    -I"$RUNTIME_DIR/include" -pthread -lm -o "$BUILD_DIR/fallible-text-collections-oom-asan"
  printf 'text-collections-oom-recovered\n' >"$BUILD_DIR/fallible-text-collections-oom.expected.out"
  "$BUILD_DIR/fallible-text-collections-oom-asan" \
    >"$BUILD_DIR/fallible-text-collections-oom.out" 2>"$BUILD_DIR/fallible-text-collections-oom.err"
  cmp "$BUILD_DIR/fallible-text-collections-oom.expected.out" "$BUILD_DIR/fallible-text-collections-oom.out"
  if [ -s "$BUILD_DIR/fallible-text-collections-oom.err" ]; then
    cat "$BUILD_DIR/fallible-text-collections-oom.err" >&2
    exit 1
  fi
}

validate_llvm_fallible_text_list_create_oom() {
  source="$RUNTIME_DIR/tests/fixtures/fallible-text-list-construction-oom.yz"
  YAZA_STDLIB_DIR="$PROJECT_DIR/std" "$LLVM_COMPILER" emit-llvm \
    "$source" -o "$BUILD_DIR/fallible-text-list-construction-oom.ll"
  "$LLVM_CLANG" -Wno-override-module -fsanitize=address,undefined -fno-omit-frame-pointer \
    "$BUILD_DIR/fallible-text-list-construction-oom.ll" "$RUNTIME_DIR/src/yaza_runtime_v1.c" \
    -I"$RUNTIME_DIR/include" -pthread -lm -o "$BUILD_DIR/fallible-text-list-construction-oom-asan"
  printf 'text-list-construction-oom-recovered\n' >"$BUILD_DIR/fallible-text-list-construction-oom.expected.out"
  "$BUILD_DIR/fallible-text-list-construction-oom-asan" \
    >"$BUILD_DIR/fallible-text-list-construction-oom.out" 2>"$BUILD_DIR/fallible-text-list-construction-oom.err"
  cmp "$BUILD_DIR/fallible-text-list-construction-oom.expected.out" "$BUILD_DIR/fallible-text-list-construction-oom.out"
  if [ -s "$BUILD_DIR/fallible-text-list-construction-oom.err" ]; then
    cat "$BUILD_DIR/fallible-text-list-construction-oom.err" >&2
    exit 1
  fi
}

"$RUNTIME_CC" -std=c11 -Wall -Wextra -Werror -pthread \
  -DYAZA_RT_V1_TESTING -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$RUNTIME_DIR/tests/allocator_failure_v1.c" \
  "$RUNTIME_DIR/src/yaza_runtime_v1.c" -I"$RUNTIME_DIR/include" -lm \
  -o "$BUILD_DIR/allocator-failure-asan"
"$BUILD_DIR/allocator-failure-asan"

"$RUNTIME_CC" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$RUNTIME_DIR/tests/processor_count_v1.c" \
  "$RUNTIME_DIR/src/yaza_runtime_v1.c" -I"$RUNTIME_DIR/include" -lm \
  -o "$BUILD_DIR/processor-count-asan"
"$BUILD_DIR/processor-count-asan"

"$RUNTIME_CC" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$RUNTIME_DIR/tests/thread_v1.c" \
  "$RUNTIME_DIR/src/yaza_runtime_v1.c" -I"$RUNTIME_DIR/include" -lm \
  -o "$BUILD_DIR/thread-asan"
"$BUILD_DIR/thread-asan"

"$RUNTIME_CC" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$RUNTIME_DIR/tests/task_arena_v1.c" \
  "$RUNTIME_DIR/src/yaza_runtime_v1.c" -I"$RUNTIME_DIR/include" -lm \
  -o "$BUILD_DIR/task-arena-asan"
"$BUILD_DIR/task-arena-asan"

"$RUNTIME_CC" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$RUNTIME_DIR/tests/arena_asan_poison_v1.c" \
  "$RUNTIME_DIR/src/yaza_runtime_v1.c" -I"$RUNTIME_DIR/include" -lm \
  -o "$BUILD_DIR/arena-asan-poison"
"$BUILD_DIR/arena-asan-poison"
ASAN_OPTIONS="${ASAN_OPTIONS}:halt_on_error=0:abort_on_error=0" \
  "$BUILD_DIR/arena-asan-poison" --read-after-reset >"$BUILD_DIR/arena-use-after-reset.log" 2>&1 || :
if ! grep -F 'use-after-poison' "$BUILD_DIR/arena-use-after-reset.log" >/dev/null; then
  cat "$BUILD_DIR/arena-use-after-reset.log" >&2
  printf 'error: stale arena read did not produce an ASan use-after-poison report\n' >&2
  exit 1
fi

"$RUNTIME_CC" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$RUNTIME_DIR/tests/request_scope_v1.c" \
  "$RUNTIME_DIR/src/yaza_runtime_v1.c" -I"$RUNTIME_DIR/include" -lm \
  -o "$BUILD_DIR/request-scope-asan"
"$BUILD_DIR/request-scope-asan"

"$RUNTIME_CC" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$RUNTIME_DIR/tests/request_cycle_handle_v1.c" \
  "$RUNTIME_DIR/src/yaza_runtime_v1.c" -I"$RUNTIME_DIR/include" -lm \
  -o "$BUILD_DIR/request-cycle-handle-asan"
"$BUILD_DIR/request-cycle-handle-asan"

"$RUNTIME_CC" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$RUNTIME_DIR/tests/sibling_cycle_copy_v1.c" \
  "$RUNTIME_DIR/src/yaza_runtime_v1.c" -I"$RUNTIME_DIR/include" -lm \
  -o "$BUILD_DIR/sibling-cycle-copy-asan"
"$BUILD_DIR/sibling-cycle-copy-asan"

"$PROJECT_DIR/scripts/validate-llvm-worker-cycle.sh" "$LLVM_COMPILER" asan
"$PROJECT_DIR/scripts/validate-llvm-request-cycle.sh" "$LLVM_COMPILER" asan
validate_llvm_scope_asan yazabase-hardware-processor-count-smoke
printf 'processor-count-ok\n' >"$BUILD_DIR/yazabase-hardware-processor-count-smoke.expected.out"
cmp "$BUILD_DIR/yazabase-hardware-processor-count-smoke.expected.out" "$BUILD_DIR/yazabase-hardware-processor-count-smoke.out"
validate_llvm_scope_asan core-arena-scope-defer-capture-smoke
printf 'body\ndeferred:7\nscope-closed\n' >"$BUILD_DIR/core-arena-scope-defer-capture-smoke.expected.out"
cmp "$BUILD_DIR/core-arena-scope-defer-capture-smoke.expected.out" "$BUILD_DIR/core-arena-scope-defer-capture-smoke.out"
validate_llvm_scope_asan core-arena-scope-repeated-smoke
printf 'reclaimed=10000\n' >"$BUILD_DIR/core-arena-scope-repeated-smoke.expected.out"
cmp "$BUILD_DIR/core-arena-scope-repeated-smoke.expected.out" "$BUILD_DIR/core-arena-scope-repeated-smoke.out"
validate_llvm_scope_asan core-arena-scope-text-list-copy-smoke
printf 'parent-list-text-copies-survived\n' >"$BUILD_DIR/core-arena-scope-text-list-copy-smoke.expected.out"
cmp "$BUILD_DIR/core-arena-scope-text-list-copy-smoke.expected.out" "$BUILD_DIR/core-arena-scope-text-list-copy-smoke.out"
validate_llvm_scope_asan core-arena-scope-alias-text-list-smoke
printf 'aggregate-alias-text-copy-ok\n' >"$BUILD_DIR/core-arena-scope-alias-text-list-smoke.expected.out"
cmp "$BUILD_DIR/core-arena-scope-alias-text-list-smoke.expected.out" "$BUILD_DIR/core-arena-scope-alias-text-list-smoke.out"
validate_llvm_scope_asan core-arena-scope-shape-field-precision-smoke
printf 'field-precise-list-store-ok\n' >"$BUILD_DIR/core-arena-scope-shape-field-precision-smoke.expected.out"
cmp "$BUILD_DIR/core-arena-scope-shape-field-precision-smoke.expected.out" "$BUILD_DIR/core-arena-scope-shape-field-precision-smoke.out"
validate_llvm_scope_asan core-arena-scope-nested-shape-field-precision-smoke
printf 'nested-field-precise-list-store-ok\n' >"$BUILD_DIR/core-arena-scope-nested-shape-field-precision-smoke.expected.out"
cmp "$BUILD_DIR/core-arena-scope-nested-shape-field-precision-smoke.expected.out" "$BUILD_DIR/core-arena-scope-nested-shape-field-precision-smoke.out"
validate_llvm_scope_asan core-arena-scope-interproc-shape-fields-smoke
printf 'interprocedural-field-ownership-ok\n' >"$BUILD_DIR/core-arena-scope-interproc-shape-fields-smoke.expected.out"
cmp "$BUILD_DIR/core-arena-scope-interproc-shape-fields-smoke.expected.out" "$BUILD_DIR/core-arena-scope-interproc-shape-fields-smoke.out"
validate_llvm_scope_asan core-arena-scope-match-payload-field-precision-smoke
printf 'match-payload-field-precision-ok\n' >"$BUILD_DIR/core-arena-scope-match-payload-field-precision-smoke.expected.out"
cmp "$BUILD_DIR/core-arena-scope-match-payload-field-precision-smoke.expected.out" "$BUILD_DIR/core-arena-scope-match-payload-field-precision-smoke.out"
validate_llvm_scope_asan core-arena-scope-list-element-field-precision-smoke
printf 'list-element-field-precision-ok\n' >"$BUILD_DIR/core-arena-scope-list-element-field-precision-smoke.expected.out"
cmp "$BUILD_DIR/core-arena-scope-list-element-field-precision-smoke.expected.out" "$BUILD_DIR/core-arena-scope-list-element-field-precision-smoke.out"
validate_llvm_scope_asan core-arena-scope-helper-list-summary-precision-smoke
printf 'parent-safe\n' >"$BUILD_DIR/core-arena-scope-helper-list-summary-precision-smoke.expected.out"
cmp "$BUILD_DIR/core-arena-scope-helper-list-summary-precision-smoke.expected.out" "$BUILD_DIR/core-arena-scope-helper-list-summary-precision-smoke.out"
validate_llvm_scope_asan core-arena-scope-helper-shape-list-summary-precision-smoke
printf 'parent-safe\n' >"$BUILD_DIR/core-arena-scope-helper-shape-list-summary-precision-smoke.expected.out"
cmp "$BUILD_DIR/core-arena-scope-helper-shape-list-summary-precision-smoke.expected.out" "$BUILD_DIR/core-arena-scope-helper-shape-list-summary-precision-smoke.out"
validate_llvm_scope_asan core-arena-scope-helper-variant-list-summary-precision-smoke
printf 'parent-safe\n' >"$BUILD_DIR/core-arena-scope-helper-variant-list-summary-precision-smoke.expected.out"
cmp "$BUILD_DIR/core-arena-scope-helper-variant-list-summary-precision-smoke.expected.out" "$BUILD_DIR/core-arena-scope-helper-variant-list-summary-precision-smoke.out"
validate_llvm_scope_asan core-arena-scope-list-identity-precision-smoke
printf 'parent-safe\n' >"$BUILD_DIR/core-arena-scope-list-identity-precision-smoke.expected.out"
cmp "$BUILD_DIR/core-arena-scope-list-identity-precision-smoke.expected.out" "$BUILD_DIR/core-arena-scope-list-identity-precision-smoke.out"
validate_llvm_scope_asan core-arena-scope-aggregate-list-identity-precision-smoke
printf 'parent-safe\n' >"$BUILD_DIR/core-arena-scope-aggregate-list-identity-precision-smoke.expected.out"
cmp "$BUILD_DIR/core-arena-scope-aggregate-list-identity-precision-smoke.expected.out" "$BUILD_DIR/core-arena-scope-aggregate-list-identity-precision-smoke.out"
validate_llvm_scope_asan core-arena-scope-list-aggregate-nested-list-identity-smoke
printf 'parent-safe\n' >"$BUILD_DIR/core-arena-scope-list-aggregate-nested-list-identity-smoke.expected.out"
cmp "$BUILD_DIR/core-arena-scope-list-aggregate-nested-list-identity-smoke.expected.out" "$BUILD_DIR/core-arena-scope-list-aggregate-nested-list-identity-smoke.out"
validate_llvm_scope_asan core-arena-scope-recursive-aggregate-list-precision-smoke
printf 'parent-safe\n' >"$BUILD_DIR/core-arena-scope-recursive-aggregate-list-precision-smoke.expected.out"
cmp "$BUILD_DIR/core-arena-scope-recursive-aggregate-list-precision-smoke.expected.out" "$BUILD_DIR/core-arena-scope-recursive-aggregate-list-precision-smoke.out"
validate_llvm_scope_asan core-arena-scope-list-dynamic-index-aggregate-precision-smoke
printf 'dynamic-child-safe\n' >"$BUILD_DIR/core-arena-scope-list-dynamic-index-aggregate-precision-smoke.expected.out"
cmp "$BUILD_DIR/core-arena-scope-list-dynamic-index-aggregate-precision-smoke.expected.out" "$BUILD_DIR/core-arena-scope-list-dynamic-index-aggregate-precision-smoke.out"
validate_llvm_fallible_bytes_result_oom
validate_llvm_fallible_text_concat_oom
validate_llvm_fallible_text_repeat_oom
validate_llvm_fallible_text_join_oom
validate_llvm_fallible_text_slice_oom
validate_llvm_fallible_text_collections_oom
validate_llvm_fallible_text_list_create_oom
validate_llvm_scope_asan core-list-try-new-smoke
printf 'fallible-list-construction-ok\n' >"$BUILD_DIR/core-list-try-new-smoke.expected.out"
cmp "$BUILD_DIR/core-list-try-new-smoke.expected.out" "$BUILD_DIR/core-list-try-new-smoke.out"
validate_llvm_scope_asan core-text-list-try-new-smoke
printf 'fallible-text-list-construction-ok\n' >"$BUILD_DIR/core-text-list-try-new-smoke.expected.out"
cmp "$BUILD_DIR/core-text-list-try-new-smoke.expected.out" "$BUILD_DIR/core-text-list-try-new-smoke.out"
validate_llvm_scope_asan core-list-try-set-smoke
printf 'fallible-list-update-ok\n' >"$BUILD_DIR/core-list-try-set-smoke.expected.out"
cmp "$BUILD_DIR/core-list-try-set-smoke.expected.out" "$BUILD_DIR/core-list-try-set-smoke.out"
validate_llvm_scope_asan core-list-try-reserve-smoke
printf 'list-try-reserve-ok\n' >"$BUILD_DIR/core-list-try-reserve-smoke.expected.out"
cmp "$BUILD_DIR/core-list-try-reserve-smoke.expected.out" "$BUILD_DIR/core-list-try-reserve-smoke.out"
validate_llvm_scope_asan core-fallible-sum-smoke
printf 'variant-try-empty-ok\nfallible-list-literal-ok\nfallible-list-read-ok\nfallible-interpolation-ok\nfallible-list-read-owner-ok\nfallible-sums-ok\n' >"$BUILD_DIR/core-fallible-sum-smoke.expected.out"
cmp "$BUILD_DIR/core-fallible-sum-smoke.expected.out" "$BUILD_DIR/core-fallible-sum-smoke.out"
validate_llvm_scope_asan core-arena-scope-try-safe-smoke
printf 'scoped-postfix-question-cleanup-ok\n' >"$BUILD_DIR/core-arena-scope-try-safe-smoke.expected.out"
cmp "$BUILD_DIR/core-arena-scope-try-safe-smoke.expected.out" "$BUILD_DIR/core-arena-scope-try-safe-smoke.out"
validate_llvm_scope_asan core-arena-scope-inline-sums-smoke
printf '17\nnone\nkept\n23\nhandled\ninline-sums-ok\n' >"$BUILD_DIR/core-arena-scope-inline-sums-smoke.expected.out"
cmp "$BUILD_DIR/core-arena-scope-inline-sums-smoke.expected.out" "$BUILD_DIR/core-arena-scope-inline-sums-smoke.out"

"$RUNTIME_CC" -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=thread \
  "$RUNTIME_DIR/tests/arena_v1.c" \
  "$RUNTIME_DIR/src/yaza_runtime_v1.c" -I"$RUNTIME_DIR/include" -lm \
  -o "$BUILD_DIR/arena-tsan"
"$BUILD_DIR/arena-tsan"

"$RUNTIME_CC" -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=thread \
  "$RUNTIME_DIR/tests/typed_list_v1.c" \
  "$RUNTIME_DIR/src/yaza_runtime_v1.c" -I"$RUNTIME_DIR/include" -lm \
  -o "$BUILD_DIR/typed-list-tsan"
"$BUILD_DIR/typed-list-tsan"

"$RUNTIME_CC" -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=thread \
  "$RUNTIME_DIR/tests/request_scope_concurrent_v1.c" \
  "$RUNTIME_DIR/src/yaza_runtime_v1.c" -I"$RUNTIME_DIR/include" -lm \
  -o "$BUILD_DIR/request-scope-tsan"
"$BUILD_DIR/request-scope-tsan"

"$RUNTIME_CC" -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=thread \
  "$RUNTIME_DIR/tests/request_cycle_handle_v1.c" \
  "$RUNTIME_DIR/src/yaza_runtime_v1.c" -I"$RUNTIME_DIR/include" -lm \
  -o "$BUILD_DIR/request-cycle-handle-tsan"
"$BUILD_DIR/request-cycle-handle-tsan"

"$RUNTIME_CC" -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=thread \
  "$RUNTIME_DIR/tests/thread_v1.c" \
  "$RUNTIME_DIR/src/yaza_runtime_v1.c" -I"$RUNTIME_DIR/include" -lm \
  -o "$BUILD_DIR/thread-tsan"
"$BUILD_DIR/thread-tsan"

"$RUNTIME_CC" -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=thread \
  "$RUNTIME_DIR/tests/task_arena_v1.c" \
  "$RUNTIME_DIR/src/yaza_runtime_v1.c" -I"$RUNTIME_DIR/include" -lm \
  -o "$BUILD_DIR/task-arena-tsan"
"$BUILD_DIR/task-arena-tsan"

"$PROJECT_DIR/scripts/validate-llvm-worker-cycle.sh" "$LLVM_COMPILER" tsan
"$PROJECT_DIR/scripts/validate-llvm-request-cycle.sh" "$LLVM_COMPILER" tsan

printf 'runtime v1 sanitizers: ASan/UBSan allocator, reclaimed-arena poisoning, request, thread spawn/join and mutex/condition primitives, compiled YAZA workers, sibling-cycle Text copies, arena_scope cleanup, Text List copies through aggregate aliases, field-precise Shape/List-element ownership, direct and helper-chain List effects, nested/Option/Result/Variant paths, and fallible List/Bytes/Text/numeric Result quota recovery; TSan shared arenas, thread spawn/join and condition synchronization, concurrent typed-List borrow/read during mutation, concurrent scopes, reusable cycle handles, and compiled YAZA workers passed\n'

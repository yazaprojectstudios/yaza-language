#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
LLVM_COMPILER=${1:-"$PROJECT_DIR/build/yaza-llvm"}
SANITIZER_MODE=${2:-none}
RUNTIME_CC=${CC:-clang}
TEMP_ROOT=${TMPDIR:-/tmp}
TEMP_ROOT=${TEMP_ROOT%/}
BUILD_DIR=$(mktemp -d "$TEMP_ROOT/yaza-llvm-request-cycle.XXXXXX")
trap 'rm -rf "$BUILD_DIR"' EXIT HUP INT TERM

YAZA_STDLIB_DIR="$PROJECT_DIR/std" "$LLVM_COMPILER" emit-llvm-module \
  "$PROJECT_DIR/examples/core-arena-request-module-smoke.yz" \
  --export-request transform_request -o "$BUILD_DIR/request-module.ll"

if YAZA_STDLIB_DIR="$PROJECT_DIR/std" "$LLVM_COMPILER" emit-llvm-module \
  "$PROJECT_DIR/examples/core-arena-request-module-smoke.yz" \
  --export-request invalid_request_signature -o "$BUILD_DIR/invalid-module.ll" >"$BUILD_DIR/invalid-export.log" 2>&1; then
  printf 'error: request-module emitter accepted an invalid host ABI signature\n' >&2
  exit 1
fi
if ! grep -F 'LLVM request export: handler must return Result[Text,Text] or Result[Bytes,Text]' "$BUILD_DIR/invalid-export.log" >/dev/null; then
  cat "$BUILD_DIR/invalid-export.log" >&2
  printf 'error: invalid request export reported the wrong diagnostic\n' >&2
  exit 1
fi

check_failfast_request_export() {
  source_path=$1
  function_name=$2
  expected_effect=$3
  log_path="$BUILD_DIR/$function_name.failfast.log"
  if YAZA_STDLIB_DIR="$PROJECT_DIR/std" "$LLVM_COMPILER" emit-llvm-module \
    "$source_path" --export-request "$function_name" -o "$BUILD_DIR/$function_name.ll" >"$log_path" 2>&1; then
    printf 'error: request export accepted a fail-fast allocation path: %s\n' "$function_name" >&2
    exit 1
  fi
  if ! grep -F "$expected_effect" "$log_path" >/dev/null; then
    cat "$log_path" >&2
    printf 'error: request export did not identify its fail-fast allocation path: %s\n' "$function_name" >&2
    exit 1
  fi
}

check_failfast_request_export "$PROJECT_DIR/examples/core-request-failfast-direct-errors.yz" \
  failfast_direct_request "Text '+' concatenation allocates"
check_failfast_request_export "$PROJECT_DIR/examples/core-request-failfast-transitive-errors.yz" \
  failfast_transitive_request "text_trim allocates Text/List storage"
check_failfast_request_export "$PROJECT_DIR/examples/core-request-failfast-list-errors.yz" \
  failfast_list_request "list_push can allocate or grow a List"
check_failfast_request_export "$PROJECT_DIR/examples/core-request-failfast-aggregate-errors.yz" \
  failfast_aggregate_request "some boxes its aggregate payload and can fail fast"

set -- -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread
case "$SANITIZER_MODE" in
  none) ;;
  asan) set -- "$@" -fsanitize=address,undefined -fno-omit-frame-pointer ;;
  tsan) set -- "$@" -fsanitize=thread ;;
  *) printf 'error: unknown sanitizer mode: %s\n' "$SANITIZER_MODE" >&2; exit 2 ;;
esac
"$RUNTIME_CC" "$@" -I"$PROJECT_DIR/runtime/include" \
  "$BUILD_DIR/request-module.ll" \
  "$PROJECT_DIR/runtime/tests/yaza_request_module_v1.c" \
  "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" -lm \
  -o "$BUILD_DIR/yaza-request-module-test"
"$BUILD_DIR/yaza-request-module-test"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" "$LLVM_COMPILER" emit-llvm-module \
  "$PROJECT_DIR/examples/core-arena-request-module-smoke.yz" \
  --export-request transform_text_request -o "$BUILD_DIR/text-request-module.ll"
"$RUNTIME_CC" "$@" -I"$PROJECT_DIR/runtime/include" \
  "$BUILD_DIR/text-request-module.ll" \
  "$PROJECT_DIR/runtime/tests/yaza_text_request_module_v1.c" \
  "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" -lm \
  -o "$BUILD_DIR/yaza-text-request-module-test"
"$BUILD_DIR/yaza-text-request-module-test"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" "$LLVM_COMPILER" emit-llvm-module \
  "$PROJECT_DIR/examples/core-arena-request-module-smoke.yz" \
  --export-request try_numeric_text_request -o "$BUILD_DIR/numeric-text-request-module.ll"
"$RUNTIME_CC" "$@" -I"$PROJECT_DIR/runtime/include" \
  "$BUILD_DIR/numeric-text-request-module.ll" \
  "$PROJECT_DIR/runtime/tests/yaza_numeric_text_request_module_v1.c" \
  "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" -lm \
  -o "$BUILD_DIR/yaza-numeric-text-request-module-test"
"$BUILD_DIR/yaza-numeric-text-request-module-test"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" "$LLVM_COMPILER" emit-llvm-module \
  "$PROJECT_DIR/examples/core-arena-request-module-smoke.yz" \
  --export-request try_bytes_concat_request -o "$BUILD_DIR/bytes-request-module.ll"
"$RUNTIME_CC" "$@" -I"$PROJECT_DIR/runtime/include" \
  "$BUILD_DIR/bytes-request-module.ll" \
  "$PROJECT_DIR/runtime/tests/yaza_bytes_request_module_v1.c" \
  "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" -lm \
  -o "$BUILD_DIR/yaza-bytes-request-module-test"
"$BUILD_DIR/yaza-bytes-request-module-test"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" "$LLVM_COMPILER" emit-llvm-module \
  "$PROJECT_DIR/examples/core-arena-request-module-smoke.yz" \
  --export-request try_bytes_from_text_request -o "$BUILD_DIR/bytes-from-text-request-module.ll"
"$RUNTIME_CC" "$@" -I"$PROJECT_DIR/runtime/include" \
  "$BUILD_DIR/bytes-from-text-request-module.ll" \
  "$PROJECT_DIR/runtime/tests/yaza_bytes_from_text_request_module_v1.c" \
  "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" -lm \
  -o "$BUILD_DIR/yaza-bytes-from-text-request-module-test"
"$BUILD_DIR/yaza-bytes-from-text-request-module-test"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" "$LLVM_COMPILER" emit-llvm-module \
  "$PROJECT_DIR/examples/core-arena-request-module-smoke.yz" \
  --export-request try_bytes_primitives_request -o "$BUILD_DIR/bytes-primitives-request-module.ll"
"$RUNTIME_CC" "$@" -I"$PROJECT_DIR/runtime/include" \
  "$BUILD_DIR/bytes-primitives-request-module.ll" \
  "$PROJECT_DIR/runtime/tests/yaza_bytes_primitives_request_module_v1.c" \
  "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" -lm \
  -o "$BUILD_DIR/yaza-bytes-primitives-request-module-test"
"$BUILD_DIR/yaza-bytes-primitives-request-module-test"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" "$LLVM_COMPILER" emit-llvm-module \
  "$PROJECT_DIR/examples/core-arena-request-module-smoke.yz" \
  --export-request try_text_from_bytes_request -o "$BUILD_DIR/text-from-bytes-request-module.ll"
"$RUNTIME_CC" "$@" -I"$PROJECT_DIR/runtime/include" \
  "$BUILD_DIR/text-from-bytes-request-module.ll" \
  "$PROJECT_DIR/runtime/tests/yaza_text_from_bytes_request_module_v1.c" \
  "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" -lm \
  -o "$BUILD_DIR/yaza-text-from-bytes-request-module-test"
"$BUILD_DIR/yaza-text-from-bytes-request-module-test"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" "$LLVM_COMPILER" emit-llvm-module \
  "$PROJECT_DIR/examples/core-arena-request-module-smoke.yz" \
  --export-request try_list_request -o "$BUILD_DIR/list-request-module.ll"
"$RUNTIME_CC" "$@" -I"$PROJECT_DIR/runtime/include" \
  "$BUILD_DIR/list-request-module.ll" \
  "$PROJECT_DIR/runtime/tests/yaza_list_request_module_v1.c" \
  "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" -lm \
  -o "$BUILD_DIR/yaza-list-request-module-test"
"$BUILD_DIR/yaza-list-request-module-test"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" "$LLVM_COMPILER" emit-llvm-module \
  "$PROJECT_DIR/examples/core-arena-request-module-smoke.yz" \
  --export-request try_list_get_result_request -o "$BUILD_DIR/list-get-result-request-module.ll"
"$RUNTIME_CC" "$@" -I"$PROJECT_DIR/runtime/include" \
  "$BUILD_DIR/list-get-result-request-module.ll" \
  "$PROJECT_DIR/runtime/tests/yaza_list_try_get_result_request_module_v1.c" \
  "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" -lm \
  -o "$BUILD_DIR/yaza-list-get-result-request-module-test"
"$BUILD_DIR/yaza-list-get-result-request-module-test"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" "$LLVM_COMPILER" emit-llvm-module \
  "$PROJECT_DIR/examples/core-arena-request-module-smoke.yz" \
  --export-request try_fallible_sum_request -o "$BUILD_DIR/fallible-sum-request-module.ll"
"$RUNTIME_CC" "$@" -I"$PROJECT_DIR/runtime/include" \
  "$BUILD_DIR/fallible-sum-request-module.ll" \
  "$PROJECT_DIR/runtime/tests/yaza_fallible_sum_request_module_v1.c" \
  "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" -lm \
  -o "$BUILD_DIR/yaza-fallible-sum-request-module-test"
"$BUILD_DIR/yaza-fallible-sum-request-module-test"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" "$LLVM_COMPILER" emit-llvm-module \
  "$PROJECT_DIR/examples/core-arena-request-module-smoke.yz" \
  --export-request try_text_join_request -o "$BUILD_DIR/text-join-request-module.ll"
"$RUNTIME_CC" "$@" -I"$PROJECT_DIR/runtime/include" \
  "$BUILD_DIR/text-join-request-module.ll" \
  "$PROJECT_DIR/runtime/tests/yaza_text_join_request_module_v1.c" \
  "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" -lm \
  -o "$BUILD_DIR/yaza-text-join-request-module-test"
"$BUILD_DIR/yaza-text-join-request-module-test"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" "$LLVM_COMPILER" emit-llvm-module \
  "$PROJECT_DIR/examples/core-arena-request-module-smoke.yz" \
  --export-request try_text_allocations_request -o "$BUILD_DIR/text-allocations-request-module.ll"
"$RUNTIME_CC" "$@" -I"$PROJECT_DIR/runtime/include" \
  "$BUILD_DIR/text-allocations-request-module.ll" \
  "$PROJECT_DIR/runtime/tests/yaza_text_allocations_request_module_v1.c" \
  "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" -lm \
  -o "$BUILD_DIR/yaza-text-allocations-request-module-test"
"$BUILD_DIR/yaza-text-allocations-request-module-test"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" "$LLVM_COMPILER" emit-llvm-module \
  "$PROJECT_DIR/examples/core-arena-request-module-smoke.yz" \
  --export-request try_secure_random_request -o "$BUILD_DIR/secure-random-request-module.ll"
"$RUNTIME_CC" "$@" -I"$PROJECT_DIR/runtime/include" \
  "$BUILD_DIR/secure-random-request-module.ll" \
  "$PROJECT_DIR/runtime/tests/yaza_secure_random_request_module_v1.c" \
  "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" -lm \
  -o "$BUILD_DIR/yaza-secure-random-request-module-test"
"$BUILD_DIR/yaza-secure-random-request-module-test"

#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
COMPILER=${YAZA_LLVM_COMPILER:-"$PROJECT_DIR/build/yaza-llvm"}
CLANG=${CLANG:-clang}
MODE=${1:-both}
BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/yazabase-delete-index-batch.XXXXXX")
trap 'rm -rf "$BUILD_DIR"' EXIT HUP INT TERM

case "$COMPILER" in
  /*) ;;
  *) COMPILER="$(pwd)/$COMPILER" ;;
esac
if [ ! -x "$COMPILER" ]; then
  printf 'error: executable YAZA LLVM compiler not found: %s\n' "$COMPILER" >&2
  exit 127
fi

case "$MODE" in
  normal|asan|both) ;;
  *) printf 'error: expected mode normal, asan, or both, got: %s\n' "$MODE" >&2; exit 2 ;;
esac

cd "$PROJECT_DIR"
export YAZA_COMPILER_PROCESS_LIMIT_BYTES=${YAZA_COMPILER_PROCESS_LIMIT_BYTES:-34359738368}
export YAZA_RT_V1_PROCESS_LIMIT_BYTES=$YAZA_COMPILER_PROCESS_LIMIT_BYTES
export YAZA_RT_V1_MAX_LIST_VECTOR_BYTES=${YAZA_COMPILER_LIST_VECTOR_LIMIT_BYTES:-34359738368}
export YAZA_RT_V1_MAX_LIST_ITEMS=${YAZA_COMPILER_MAX_LIST_ITEMS:-32000000}
export YAZA_STDLIB_DIR="$PROJECT_DIR/std"

run_and_check() {
  executable=$1
  root=$2
  mkdir "$root"
  result=$("$executable" "$root")
  printf '%s\n' "$result"
  case "$result" in
    *yazabase-sql-delete-index-batch-v1-ok*) ;;
    *) printf 'error: multi-leaf secondary-index DELETE fixture failed\n' >&2; exit 1 ;;
  esac
}

if [ "$MODE" = normal ] || [ "$MODE" = both ]; then
  "$COMPILER" build "$PROJECT_DIR/examples/yazabase-sql-delete-index-batch-v1-smoke.yz" \
    -o "$BUILD_DIR/delete-index-batch"
  run_and_check "$BUILD_DIR/delete-index-batch" "$BUILD_DIR/normal-root"
fi

if [ "$MODE" = asan ] || [ "$MODE" = both ]; then
  "$COMPILER" emit-llvm "$PROJECT_DIR/examples/yazabase-sql-delete-index-batch-v1-smoke.yz" \
    -o "$BUILD_DIR/delete-index-batch.ll"
  "$CLANG" -Wno-override-module -fsanitize=address,undefined \
    -x ir -c "$BUILD_DIR/delete-index-batch.ll" -o "$BUILD_DIR/delete-index-batch.o"
  "$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    "$BUILD_DIR/delete-index-batch.o" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
    -I"$PROJECT_DIR/runtime/include" -lm -o "$BUILD_DIR/delete-index-batch-asan"
  ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} \
    run_and_check "$BUILD_DIR/delete-index-batch-asan" "$BUILD_DIR/asan-root"
fi

printf 'yazabase-delete-index-batch-validation-ok\n'

#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
COMPILER=${YAZA_LLVM_COMPILER:-"$PROJECT_DIR/build/yaza-llvm"}
CLANG=${CLANG:-clang}
TEMP_ROOT=${TMPDIR:-/tmp}
TEMP_ROOT=${TEMP_ROOT%/}
BUILD_DIR=$(mktemp -d "$TEMP_ROOT/yazabase-overflow-sql.XXXXXX")
trap 'rm -rf "$BUILD_DIR"' EXIT HUP INT TERM

case "$COMPILER" in
  /*) ;;
  *) COMPILER="$(pwd)/$COMPILER" ;;
esac
if [ ! -x "$COMPILER" ]; then
  printf 'error: executable YAZA LLVM compiler not found: %s\n' "$COMPILER" >&2
  exit 127
fi

cd "$PROJECT_DIR"
YAZA_RT_V1_PROCESS_LIMIT_BYTES=${YAZA_COMPILER_PROCESS_LIMIT_BYTES:-34359738368} \
YAZA_RT_V1_MAX_LIST_VECTOR_BYTES=${YAZA_COMPILER_MAX_LIST_VECTOR_BYTES:-34359738368} \
YAZA_RT_V1_MAX_LIST_ITEMS=${YAZA_COMPILER_MAX_LIST_ITEMS:-32000000} \
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm \
    "$PROJECT_DIR/examples/yazabase-overflow-sql-v1-smoke.yz" \
    -o "$BUILD_DIR/overflow.ll"

compile_and_run() {
  mode=$1
  if [ "$mode" = asan ]; then
    "$CLANG" -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread \
      -fsanitize=address,undefined -fno-omit-frame-pointer \
      "$BUILD_DIR/overflow.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
      -I"$PROJECT_DIR/runtime/include" -lm -o "$BUILD_DIR/overflow-$mode"
    mkdir "$BUILD_DIR/root-$mode"
    output=$(ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} \
      "$BUILD_DIR/overflow-$mode" "$BUILD_DIR/root-$mode")
  elif [ "$mode" = optimized ]; then
    "$CLANG" -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread -O2 \
      "$BUILD_DIR/overflow.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
      -I"$PROJECT_DIR/runtime/include" -lm -o "$BUILD_DIR/overflow-$mode"
    mkdir "$BUILD_DIR/root-$mode"
    output=$("$BUILD_DIR/overflow-$mode" "$BUILD_DIR/root-$mode")
  else
    "$CLANG" -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread \
      "$BUILD_DIR/overflow.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
      -I"$PROJECT_DIR/runtime/include" -lm -o "$BUILD_DIR/overflow-$mode"
    mkdir "$BUILD_DIR/root-$mode"
    output=$("$BUILD_DIR/overflow-$mode" "$BUILD_DIR/root-$mode")
  fi
  printf '%s\n' "$output"
  case "$output" in
    *yazabase-overflow-sql-v1-ok*) ;;
    *) printf 'error: overflow SQL fixture failed in %s mode\n' "$mode" >&2; exit 1 ;;
  esac
}

compile_and_run normal
compile_and_run optimized
compile_and_run asan
printf 'yazabase-overflow-sql-focused-validation-ok\n'

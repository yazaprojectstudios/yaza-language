#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
COMPILER=${YAZA_LLVM_COMPILER:-"$PROJECT_DIR/build/yaza-llvm"}
CLANG=${CLANG:-clang}
TEMP_ROOT=${TMPDIR:-/tmp}
TEMP_ROOT=${TEMP_ROOT%/}
BUILD_DIR=$(mktemp -d "$TEMP_ROOT/yazabase-storage-decoder-fuzz.XXXXXX")
trap 'rm -rf "$BUILD_DIR"' EXIT HUP INT TERM

case "$COMPILER" in
  /*) ;;
  *) COMPILER="$(pwd)/$COMPILER" ;;
esac
if [ ! -x "$COMPILER" ]; then
  printf 'error: executable YAZA LLVM compiler not found: %s\n' "$COMPILER" >&2
  exit 127
fi
if ! command -v "$CLANG" >/dev/null 2>&1; then
  printf 'error: Clang executable not found: %s\n' "$CLANG" >&2
  exit 127
fi

cd "$PROJECT_DIR"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
YAZA_RT_V1_PROCESS_LIMIT_BYTES=${YAZA_COMPILER_PROCESS_LIMIT_BYTES:-34359738368} \
YAZA_RT_V1_MAX_LIST_VECTOR_BYTES=${YAZA_COMPILER_MAX_LIST_VECTOR_BYTES:-34359738368} \
YAZA_RT_V1_MAX_LIST_ITEMS=${YAZA_COMPILER_MAX_LIST_ITEMS:-32000000} \
  "$COMPILER" emit-llvm \
    "$PROJECT_DIR/examples/yazabase-storage-decoder-fuzz-v1-smoke.yz" \
    -o "$BUILD_DIR/storage-decoder-fuzz.ll"

compile_and_run() {
  mode=$1
  set -- -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread
  if [ "$mode" = release ]; then
    set -- "$@" -O2
  elif [ "$mode" = asan ]; then
    set -- "$@" -fsanitize=address,undefined -fno-omit-frame-pointer
  fi
  "$CLANG" "$@" "$BUILD_DIR/storage-decoder-fuzz.ll" \
    "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
    -I"$PROJECT_DIR/runtime/include" -lm -o "$BUILD_DIR/storage-decoder-fuzz-$mode"
  if [ "$mode" = asan ]; then
    output=$(ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} "$BUILD_DIR/storage-decoder-fuzz-$mode")
  else
    output=$("$BUILD_DIR/storage-decoder-fuzz-$mode")
  fi
  printf '%s\n' "$output"
  case "$output" in
    *'yazabase-storage-decoder-fuzz-v1-ok cases=1920 formats=15'*) ;;
    *) printf 'error: storage decoder fuzz fixture failed in %s mode\n' "$mode" >&2; exit 1 ;;
  esac
}

compile_and_run debug
compile_and_run release
compile_and_run asan
printf 'yazabase-storage-decoder-fuzz-focused-validation-ok\n'

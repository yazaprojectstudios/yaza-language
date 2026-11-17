#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
COMPILER=${YAZA_LLVM_COMPILER:-"$PROJECT_DIR/build/yaza-llvm"}
CLANG=${CLANG:-clang}
TEMP_ROOT=${TMPDIR:-/tmp}
TEMP_ROOT=${TEMP_ROOT%/}
BUILD_DIR=$(mktemp -d "$TEMP_ROOT/yazabase-alter-table.XXXXXX")
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
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
YAZA_RT_V1_PROCESS_LIMIT_BYTES=${YAZA_COMPILER_PROCESS_LIMIT_BYTES:-34359738368} \
  "$COMPILER" emit-llvm examples/yazabase-sql-alter-table-v1-smoke.yz -o "$BUILD_DIR/alter-table.ll"

build_and_run() {
  mode=$1
  shift
  output="$BUILD_DIR/alter-table-$mode"
  "$CLANG" -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread "$@" \
    "$BUILD_DIR/alter-table.ll" runtime/src/yaza_runtime_v1.c -Iruntime/include -lm -o "$output"
  mkdir "$BUILD_DIR/root-$mode"
  if [ "$mode" = asan-ubsan ]; then
    run_output=$(ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} UBSAN_OPTIONS=${UBSAN_OPTIONS:-print_stacktrace=1} "$output" "$BUILD_DIR/root-$mode")
  else
    run_output=$("$output" "$BUILD_DIR/root-$mode")
  fi
  printf '%s\n' "$run_output"
  case "$run_output" in
    *yazabase-sql-alter-table-v1-ok*) ;;
    *) printf 'error: ALTER TABLE fixture failed in %s mode\n' "$mode" >&2; return 1 ;;
  esac
}

build_and_run debug -O0
build_and_run optimized -O2
build_and_run asan-ubsan -O1 -fsanitize=address,undefined -fno-omit-frame-pointer
printf 'yazabase-alter-table-focused-validation-ok\n'

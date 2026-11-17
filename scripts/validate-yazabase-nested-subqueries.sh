#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
COMPILER=${YAZA_LLVM_COMPILER:-"$PROJECT_DIR/build/yaza-llvm"}
CLANG=${CLANG:-clang}
TEMP_ROOT=${TMPDIR:-/tmp}
TEMP_ROOT=${TEMP_ROOT%/}
BUILD_DIR=$(mktemp -d "$TEMP_ROOT/yazabase-nested-subqueries.XXXXXX")
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
YAZA_RT_V1_MAX_LIST_VECTOR_BYTES=${YAZA_COMPILER_LIST_VECTOR_LIMIT_BYTES:-34359738368} \
YAZA_RT_V1_MAX_LIST_ITEMS=${YAZA_COMPILER_MAX_LIST_ITEMS:-32000000} \
  YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/yazabase-sql-inner-join-persist-v1-smoke.yz \
  -o "$BUILD_DIR/nested-subqueries.ll"

build_and_run() {
  mode=$1
  shift
  output="$BUILD_DIR/nested-subqueries-$mode"
  "$CLANG" -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread "$@" \
    "$BUILD_DIR/nested-subqueries.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
    -I"$PROJECT_DIR/runtime/include" -lm -o "$output"
  mkdir "$BUILD_DIR/root-$mode"
  if [ "$mode" = asan ]; then
    if run_output=$(ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} \
      UBSAN_OPTIONS=${UBSAN_OPTIONS:-print_stacktrace=1} \
      "$output" "$BUILD_DIR/root-$mode"); then
      :
    else
      status=$?
      printf '%s\n' "$run_output" >&2
      return "$status"
    fi
  else
    if run_output=$("$output" "$BUILD_DIR/root-$mode"); then
      :
    else
      status=$?
      printf '%s\n' "$run_output" >&2
      return "$status"
    fi
  fi
  printf '%s\n' "$run_output"
  case "$run_output" in
    *yazabase-sql-inner-join-persist-v1-ok*) ;;
    *) printf 'error: nested-subquery fixture failed in %s mode\n' "$mode" >&2; return 1 ;;
  esac
}

build_and_run debug
build_and_run optimized -O2
build_and_run asan -fsanitize=address,undefined -fno-omit-frame-pointer
printf 'yazabase-nested-subqueries-focused-validation-ok\n'

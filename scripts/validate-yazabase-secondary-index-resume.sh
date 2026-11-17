#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
COMPILER=${YAZA_LLVM_COMPILER:-"$PROJECT_DIR/build/yaza-llvm"}
CLANG=${CLANG:-clang}
MODE=${1:-both}
TEMP_ROOT=${TMPDIR:-/tmp}
TEMP_ROOT=${TEMP_ROOT%/}
BUILD_DIR=$(mktemp -d "$TEMP_ROOT/yazabase-index-resume.XXXXXX")
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
  *) printf 'usage: %s [normal|asan|both]\n' "$0" >&2; exit 2 ;;
esac

cd "$PROJECT_DIR"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  YAZA_RT_V1_PROCESS_LIMIT_BYTES=${YAZA_COMPILER_PROCESS_LIMIT_BYTES:-34359738368} \
  YAZA_RT_V1_MAX_LIST_VECTOR_BYTES=${YAZA_COMPILER_MAX_LIST_VECTOR_BYTES:-34359738368} \
  YAZA_RT_V1_MAX_LIST_ITEMS=${YAZA_COMPILER_MAX_LIST_ITEMS:-32000000} \
  "$COMPILER" emit-llvm \
    "$PROJECT_DIR/examples/yazabase-sql-create-index-resume-v1-smoke.yz" \
    -o "$BUILD_DIR/create-index-resume.ll"

compile_and_run() {
  sanitizer=$1
  set -- -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread
  if [ "$sanitizer" = asan ]; then
    set -- "$@" -fsanitize=address,undefined -fno-omit-frame-pointer
  fi
  "$CLANG" "$@" "$BUILD_DIR/create-index-resume.ll" \
    "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
    -I"$PROJECT_DIR/runtime/include" -lm \
    -o "$BUILD_DIR/create-index-resume-$sanitizer"
  root="$BUILD_DIR/database-$sanitizer"
  mkdir "$root"
  if [ "$sanitizer" = asan ]; then
    output=$(ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} "$BUILD_DIR/create-index-resume-$sanitizer" "$root")
  else
    output=$("$BUILD_DIR/create-index-resume-$sanitizer" "$root")
  fi
  printf '%s\n' "$output"
  case "$output" in
    *yazabase-sql-create-index-resume-v1-ok*) ;;
    *) printf 'error: CREATE INDEX multi-batch restart fixture failed (%s)\n' "$sanitizer" >&2; exit 1 ;;
  esac
}

case "$MODE" in
  normal) compile_and_run normal ;;
  asan) compile_and_run asan ;;
  both)
    compile_and_run normal
    compile_and_run asan
    ;;
esac
printf 'yazabase-secondary-index-resume-validation-ok\n'

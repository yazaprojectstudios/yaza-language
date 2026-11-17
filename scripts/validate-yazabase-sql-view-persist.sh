#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
COMPILER=${YAZA_LLVM_COMPILER:-"$PROJECT_DIR/build/yaza-llvm"}
CLANG=${CLANG:-clang}
BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/yazabase-view-persist.XXXXXX")
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
YAZA_RT_V1_MAX_LIST_VECTOR_BYTES=${YAZA_COMPILER_LIST_VECTOR_LIMIT_BYTES:-34359738368} \
YAZA_RT_V1_MAX_LIST_ITEMS=${YAZA_COMPILER_MAX_LIST_ITEMS:-32000000} \
  "$COMPILER" emit-llvm "$PROJECT_DIR/examples/yazabase-sql-view-persist-v1-smoke.yz" \
    -o "$BUILD_DIR/view-persist.ll"

compile_and_run() {
  mode=$1
  shift
  "$CLANG" -Wno-override-module "$@" -x ir -c "$BUILD_DIR/view-persist.ll" \
    -o "$BUILD_DIR/view-persist-$mode.o"
  link_flags=
  if [ "$mode" = asan-ubsan ]; then
    link_flags='-fsanitize=address,undefined -fno-omit-frame-pointer'
  fi
  # Intentional splitting keeps the optional sanitizer flags separate.
  # shellcheck disable=SC2086
  "$CLANG" -std=c11 -Wall -Wextra -Werror -pthread $link_flags \
    "$BUILD_DIR/view-persist-$mode.o" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
    -I"$PROJECT_DIR/runtime/include" -lm -o "$BUILD_DIR/view-persist-$mode"
  mkdir "$BUILD_DIR/root-$mode"
  if [ "$mode" = asan-ubsan ]; then
    output=$(ASAN_OPTIONS=detect_leaks=0 "$BUILD_DIR/view-persist-$mode" "$BUILD_DIR/root-$mode")
  else
    output=$("$BUILD_DIR/view-persist-$mode" "$BUILD_DIR/root-$mode")
  fi
  printf '%s\n' "$output"
  case "$output" in
    *yazabase-sql-view-persist-v1-ok*) ;;
    *) printf 'error: persistent SQL view fixture failed in %s mode\n' "$mode" >&2; exit 1 ;;
  esac
}

compile_and_run debug -O0
compile_and_run release -O2
compile_and_run asan-ubsan -O1 -fsanitize=address,undefined -fno-omit-frame-pointer
printf 'yazabase-sql-view-persist-focused-validation-ok\n'

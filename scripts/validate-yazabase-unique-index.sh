#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
COMPILER=${YAZA_LLVM_COMPILER:-"$PROJECT_DIR/build/yaza-llvm"}
CLANG=${CLANG:-clang}
MODE=${1:-normal}
BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/yazabase-unique-index.XXXXXX")
trap 'rm -rf "$BUILD_DIR"' EXIT HUP INT TERM

case "$COMPILER" in
  /*) ;;
  *) COMPILER="$(pwd)/$COMPILER" ;;
esac

if [ ! -x "$COMPILER" ]; then
  printf 'error: executable YAZA LLVM compiler not found: %s\n' "$COMPILER" >&2
  exit 127
fi

export YAZA_STDLIB_DIR="$PROJECT_DIR/std"
COMPILER_LIMIT_BYTES=${YAZA_RT_V1_PROCESS_LIMIT_BYTES:-34359738368}
COMPILER_MAX_VECTOR_BYTES=${YAZA_RT_V1_MAX_LIST_VECTOR_BYTES:-34359738368}
COMPILER_MAX_ITEMS=${YAZA_RT_V1_MAX_LIST_ITEMS:-32000000}
FIXTURE="$PROJECT_DIR/examples/yazabase-sql-unique-index-v1-smoke.yz"
cd "$PROJECT_DIR"

run_and_check() {
  executable=$1
  root="$BUILD_DIR/root-$2"
  mkdir "$root"
  result=$("$executable" "$root")
  printf '%s\n' "$result"
  case "$result" in
    *yazabase-sql-unique-index-v1-ok*) ;;
    *) printf 'error: UNIQUE INDEX fixture returned an unexpected result\n' >&2; exit 1 ;;
  esac
}

case "$MODE" in
  normal|both)
    YAZA_RT_V1_PROCESS_LIMIT_BYTES="$COMPILER_LIMIT_BYTES" \
      YAZA_RT_V1_MAX_LIST_VECTOR_BYTES="$COMPILER_MAX_VECTOR_BYTES" \
      YAZA_RT_V1_MAX_LIST_ITEMS="$COMPILER_MAX_ITEMS" \
      "$COMPILER" build "$FIXTURE" -o "$BUILD_DIR/unique-index"
    run_and_check "$BUILD_DIR/unique-index" normal
    ;;
esac

case "$MODE" in
  asan|both)
    if [ ! -f "$BUILD_DIR/unique-index.ll" ]; then
      YAZA_RT_V1_PROCESS_LIMIT_BYTES="$COMPILER_LIMIT_BYTES" \
        YAZA_RT_V1_MAX_LIST_VECTOR_BYTES="$COMPILER_MAX_VECTOR_BYTES" \
        YAZA_RT_V1_MAX_LIST_ITEMS="$COMPILER_MAX_ITEMS" \
        "$COMPILER" emit-llvm "$FIXTURE" -o "$BUILD_DIR/unique-index.ll"
    fi
    "$CLANG" -Wno-override-module -fsanitize=address,undefined \
      -x ir -c "$BUILD_DIR/unique-index.ll" -o "$BUILD_DIR/unique-index.o"
    "$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
      -fsanitize=address,undefined -fno-omit-frame-pointer \
      "$BUILD_DIR/unique-index.o" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
      -I"$PROJECT_DIR/runtime/include" -lm -o "$BUILD_DIR/unique-index-asan"
    run_and_check "$BUILD_DIR/unique-index-asan" asan
    ;;
  normal|both) ;;
  *) printf 'usage: %s [normal|asan|both]\n' "$0" >&2; exit 2 ;;
esac

printf 'yazabase-unique-index-focused-validation-ok\n'

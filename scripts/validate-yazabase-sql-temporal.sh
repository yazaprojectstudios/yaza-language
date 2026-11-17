#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
COMPILER=${YAZA_LLVM_COMPILER:-"$PROJECT_DIR/build/yaza-llvm"}
CLANG=${CLANG:-clang}
TEMP_ROOT=${TMPDIR:-/tmp}
TEMP_ROOT=${TEMP_ROOT%/}
BUILD_DIR=$(mktemp -d "$TEMP_ROOT/yazabase-temporal.XXXXXX")
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
YAZA_RT_V1_MAX_LIST_VECTOR_BYTES=${YAZA_COMPILER_MAX_LIST_VECTOR_BYTES:-34359738368} \
YAZA_RT_V1_MAX_LIST_ITEMS=${YAZA_COMPILER_MAX_LIST_ITEMS:-32000000} \
  "$COMPILER" emit-llvm \
    "$PROJECT_DIR/examples/yazabase-sql-temporal-v1-smoke.yz" \
    -o "$BUILD_DIR/temporal.ll"

for MODE in debug release sanitized; do
  OUTPUT="$BUILD_DIR/temporal-$MODE"
  set -- -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread
  if [ "$MODE" = release ]; then
    set -- "$@" -O2
  elif [ "$MODE" = sanitized ]; then
    set -- "$@" -fsanitize=address,undefined -fno-omit-frame-pointer
  fi
  "$CLANG" "$@" "$BUILD_DIR/temporal.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
    -I"$PROJECT_DIR/runtime/include" -lm -o "$OUTPUT"
  RESULT=$(ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} "$OUTPUT")
  if [ "$RESULT" != "yazabase-sql-temporal-v1-ok" ]; then
    printf '%s\n' "$RESULT"
    printf 'error: YazaBase temporal codec fixture failed in %s mode\n' "$MODE" >&2
    exit 1
  fi
  printf '%s\n' "$RESULT"
done

printf 'yazabase-temporal-codec-focused-validation-ok\n'

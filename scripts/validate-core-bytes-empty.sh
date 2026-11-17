#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
COMPILER=${YAZA_LLVM_COMPILER:-"$PROJECT_DIR/build/yaza-llvm"}
CLANG=${CLANG:-clang}
TEMP_ROOT=${TMPDIR:-/tmp}
TEMP_ROOT=${TEMP_ROOT%/}
BUILD_DIR=$(mktemp -d "$TEMP_ROOT/yaza-bytes-empty.XXXXXX")
trap 'rm -rf "$BUILD_DIR"' EXIT HUP INT TERM

case "$COMPILER" in
  /*) ;;
  *) COMPILER="$(pwd)/$COMPILER" ;;
esac
if [ ! -x "$COMPILER" ]; then
  printf 'error: executable YAZA LLVM compiler not found: %s\n' "$COMPILER" >&2
  exit 127
fi

YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
YAZA_RT_V1_PROCESS_LIMIT_BYTES=${YAZA_COMPILER_PROCESS_LIMIT_BYTES:-17179869184} \
  "$COMPILER" emit-llvm \
    "$PROJECT_DIR/examples/core-bytes-empty-smoke.yz" \
    -o "$BUILD_DIR/bytes-empty.ll"

for MODE in debug release sanitized; do
  OUTPUT="$BUILD_DIR/bytes-empty-$MODE"
  set -- -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread
  if [ "$MODE" = release ]; then
    set -- "$@" -O2
  elif [ "$MODE" = sanitized ]; then
    set -- "$@" -fsanitize=address,undefined -fno-omit-frame-pointer
  fi
  "$CLANG" "$@" "$BUILD_DIR/bytes-empty.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
    -I"$PROJECT_DIR/runtime/include" -lm -o "$OUTPUT"
  RESULT=$(ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} "$OUTPUT")
  if [ "$RESULT" != "core-bytes-empty-smoke-ok" ]; then
    printf '%s\n' "$RESULT"
    printf 'error: empty Bytes fixture failed in %s mode\n' "$MODE" >&2
    exit 1
  fi
  printf '%s\n' "$RESULT"
done

printf 'core-bytes-empty-focused-validation-ok\n'

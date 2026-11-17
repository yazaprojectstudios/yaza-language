#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
LLVM_COMPILER=${YAZA_LLVM_COMPILER:-"$PROJECT_DIR/build/yaza-llvm"}
TEMP_ROOT=${TMPDIR:-/tmp}
TEMP_ROOT=${TEMP_ROOT%/}
WORK_DIR=$(mktemp -d "$TEMP_ROOT/yaza-monotonic-ns.XXXXXX")
trap 'rm -rf "$WORK_DIR"' EXIT HUP INT TERM

if [ ! -x "$LLVM_COMPILER" ]; then
  printf 'error: executable YAZA LLVM compiler not found: %s\n' "$LLVM_COMPILER" >&2
  exit 127
fi

IR="$WORK_DIR/monotonic-ns.ll"
YAZA_STDLIB_DIR="$PROJECT_DIR" "$LLVM_COMPILER" emit-llvm \
  "$PROJECT_DIR/examples/core-monotonic-ns-smoke.yz" -o "$IR"

for MODE in debug release sanitized; do
  OUTPUT="$WORK_DIR/monotonic-ns-$MODE"
  set -- -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread
  if [ "$MODE" = release ]; then
    set -- "$@" -O2
  elif [ "$MODE" = sanitized ]; then
    set -- "$@" -fsanitize=address,undefined -fno-omit-frame-pointer
  fi
  clang "$@" "$IR" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
    -I"$PROJECT_DIR/runtime/include" -lm -o "$OUTPUT"
  RESULT=$(ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} "$OUTPUT")
  if [ "$RESULT" != "core-monotonic-ns-ok" ]; then
    printf '%s\n' "$RESULT"
    printf 'error: monotonic_ns fixture failed in %s mode\n' "$MODE" >&2
    exit 1
  fi
  printf '%s\n' "$RESULT"
done

printf 'core-monotonic-ns-focused-validation-ok\n'

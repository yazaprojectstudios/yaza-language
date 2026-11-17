#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
COMPILER=${YAZA_LLVM_COMPILER:-"$PROJECT_DIR/build/yaza-llvm"}
CLANG=${CLANG:-clang}
COMPILER_PROCESS_LIMIT_BYTES=${YAZA_COMPILER_PROCESS_LIMIT_BYTES:-34359738368}
PROGRAM_PROCESS_LIMIT_BYTES=${YAZA_PROGRAM_PROCESS_LIMIT_BYTES:-536870912}
case "$COMPILER" in
  /*) ;;
  *) COMPILER="$(pwd)/$COMPILER" ;;
esac
BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/yazabase-catalog-growth.XXXXXX")
trap 'rm -rf "$BUILD_DIR"' EXIT HUP INT TERM

if [ ! -x "$COMPILER" ]; then
  printf 'error: executable YAZA LLVM compiler not found: %s\n' "$COMPILER" >&2
  exit 127
fi

cd "$PROJECT_DIR"
mkdir "$BUILD_DIR/root"
OUTPUT="$BUILD_DIR/yazabase-catalog-tree-growth-v1-smoke"
if [ -n "${YAZABASE_SANITIZERS:-}" ]; then
  if ! command -v "$CLANG" >/dev/null 2>&1; then
    printf 'error: Clang executable not found: %s\n' "$CLANG" >&2
    exit 127
  fi
  LLVM_IR="$OUTPUT.ll"
  OBJECT="$OUTPUT.o"
  YAZA_RT_V1_PROCESS_LIMIT_BYTES="$COMPILER_PROCESS_LIMIT_BYTES" \
  YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
    "$COMPILER" emit-llvm examples/yazabase-catalog-tree-growth-v1-smoke.yz \
    -o "$LLVM_IR"
  "$CLANG" -Wno-override-module -fsanitize="$YAZABASE_SANITIZERS" \
    -x ir -c "$LLVM_IR" -o "$OBJECT"
  "$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
    -fsanitize="$YAZABASE_SANITIZERS" -fno-omit-frame-pointer \
    "$OBJECT" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
    -I"$PROJECT_DIR/runtime/include" -lm -o "$OUTPUT"
  mkdir -p "$BUILD_DIR/root"
  YAZA_RT_V1_PROCESS_LIMIT_BYTES="$PROGRAM_PROCESS_LIMIT_BYTES" \
    ASAN_OPTIONS=detect_leaks=0 "$OUTPUT" "$BUILD_DIR/root"
else
  YAZA_RT_V1_PROCESS_LIMIT_BYTES="$COMPILER_PROCESS_LIMIT_BYTES" \
  YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
    "$COMPILER" build examples/yazabase-catalog-tree-growth-v1-smoke.yz \
    -o "$OUTPUT"
  YAZA_RT_V1_PROCESS_LIMIT_BYTES="$PROGRAM_PROCESS_LIMIT_BYTES" \
    "$OUTPUT" "$BUILD_DIR/root"
fi

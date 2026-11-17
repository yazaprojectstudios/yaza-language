#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
CLANG=${CLANG:-clang}
CC=${CC:-cc}
BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/yaza-llvm-linux-ir-smoke.XXXXXX")
trap 'rm -rf "$BUILD_DIR"' EXIT HUP INT TERM

HOST_OS=$(uname -s)
HOST_ARCH=$(uname -m)
if [ "$HOST_OS" != Linux ]; then
  printf 'error: this CI smoke requires Linux, got %s/%s\n' "$HOST_OS" "$HOST_ARCH" >&2
  exit 2
fi
case "$HOST_ARCH" in
  x86_64|amd64)
    ARCH_LABEL=x86_64
    EXPECTED_TRIPLE=x86_64-unknown-linux-gnu
    LLVM_IR="$PROJECT_DIR/examples/core-arena-scope-repeated-smoke.x86_64-linux.ll"
    ;;
  aarch64|arm64)
    ARCH_LABEL=aarch64
    EXPECTED_TRIPLE=aarch64-unknown-linux-gnu
    LLVM_IR="$PROJECT_DIR/examples/core-arena-scope-repeated-smoke.aarch64-linux.ll"
    ;;
  *)
    printf 'error: unsupported Linux architecture for this CI smoke: %s\n' "$HOST_ARCH" >&2
    exit 2
    ;;
esac
if ! command -v "$CLANG" >/dev/null 2>&1; then
  printf 'error: Clang executable not found: %s\n' "$CLANG" >&2
  exit 127
fi

ACTUAL_TRIPLE=$(sed -n 's/^target triple = "\([^"]*\)"$/\1/p' "$LLVM_IR")
if [ "$ACTUAL_TRIPLE" != "$EXPECTED_TRIPLE" ]; then
  printf 'error: LLVM fixture target mismatch; expected %s, got %s\n' \
    "$EXPECTED_TRIPLE" "${ACTUAL_TRIPLE:-<missing>}" >&2
  exit 1
fi
EXPECTED='reclaimed=10000'
for opt_level in 0 2; do
  LLVM_OBJECT="$BUILD_DIR/core-arena-scope-repeated-O$opt_level.o"
  OUTPUT_BIN="$BUILD_DIR/core-arena-scope-repeated-O$opt_level"

  "$CLANG" --target="$EXPECTED_TRIPLE" "-O$opt_level" -Wno-override-module \
    -x ir -c "$LLVM_IR" -o "$LLVM_OBJECT"
  "$CC" -std=c11 "-O$opt_level" -Wall -Wextra -Werror -pthread \
    "$LLVM_OBJECT" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
    -I"$PROJECT_DIR/runtime/include" -lm -o "$OUTPUT_BIN"

  ACTUAL=$("$OUTPUT_BIN")
  if [ "$ACTUAL" != "$EXPECTED" ]; then
    printf 'error: arena-scope output mismatch at -O%s\nexpected: %s\nactual:   %s\n' \
      "$opt_level" "$EXPECTED" "$ACTUAL" >&2
    exit 1
  fi

  printf 'LLVM/runtime Linux %s -O%s smoke passed: %s\n' "$ARCH_LABEL" "$opt_level" "$ACTUAL"
done

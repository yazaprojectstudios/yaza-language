#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
LLVM_COMPILER=${YAZA_LLVM_COMPILER:-"$PROJECT_DIR/build/yaza-llvm-checksum"}
TEMP_ROOT=${TMPDIR:-/tmp}
TEMP_ROOT=${TEMP_ROOT%/}
WORK_DIR=$(mktemp -d "$TEMP_ROOT/yaza-convert.XXXXXX")
trap 'rm -rf "$WORK_DIR"' EXIT HUP INT TERM

if [ ! -x "$LLVM_COMPILER" ]; then
  printf 'error: executable YAZA LLVM compiler not found: %s\n' "$LLVM_COMPILER" >&2
  exit 127
fi

SOURCE="$PROJECT_DIR/examples/core-convert-base64-smoke.yz"
IR="$WORK_DIR/convert.ll"
NORMAL="$WORK_DIR/convert-normal"
RELEASE="$WORK_DIR/convert-release"
SANITIZED="$WORK_DIR/convert-sanitized"

YAZA_STDLIB_DIR="$PROJECT_DIR" "$LLVM_COMPILER" emit-llvm "$SOURCE" -o "$IR"
clang -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread \
  "$IR" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$NORMAL"
clang -Wno-override-module -std=c11 -O2 -Wall -Wextra -Werror -pthread \
  "$IR" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$RELEASE"
clang -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$IR" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$SANITIZED"

for PROGRAM in "$NORMAL" "$RELEASE" "$SANITIZED"; do
  OUTPUT=$(ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} "$PROGRAM")
  if [ "$OUTPUT" != "base64-conversion-ok" ]; then
    printf '%s\n' "$OUTPUT"
    printf 'error: conversion fixture failed: %s\n' "$PROGRAM" >&2
    exit 1
  fi
  printf '%s\n' "$OUTPUT"
done

printf 'std-convert-focused-validation-ok\n'

HEX_SOURCE="$PROJECT_DIR/examples/core-convert-hex-smoke.yz"
HEX_IR="$WORK_DIR/convert-hex.ll"
HEX_NORMAL="$WORK_DIR/convert-hex-normal"
HEX_RELEASE="$WORK_DIR/convert-hex-release"
HEX_SANITIZED="$WORK_DIR/convert-hex-sanitized"

YAZA_STDLIB_DIR="$PROJECT_DIR" "$LLVM_COMPILER" emit-llvm "$HEX_SOURCE" -o "$HEX_IR"
clang -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread \
  "$HEX_IR" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$HEX_NORMAL"
clang -Wno-override-module -std=c11 -O2 -Wall -Wextra -Werror -pthread \
  "$HEX_IR" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$HEX_RELEASE"
clang -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$HEX_IR" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$HEX_SANITIZED"

for PROGRAM in "$HEX_NORMAL" "$HEX_RELEASE" "$HEX_SANITIZED"; do
  OUTPUT=$(ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} "$PROGRAM")
  if [ "$OUTPUT" != "hex-conversion-ok" ]; then
    printf '%s\n' "$OUTPUT"
    printf 'error: Hex conversion fixture failed: %s\n' "$PROGRAM" >&2
    exit 1
  fi
  printf '%s\n' "$OUTPUT"
done

printf 'std-convert-hex-focused-validation-ok\n'

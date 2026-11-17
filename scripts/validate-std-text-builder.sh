#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
LLVM_COMPILER=${YAZA_LLVM_COMPILER:-"$PROJECT_DIR/build/yaza-llvm-checksum"}
TEMP_ROOT=${TMPDIR:-/tmp}
TEMP_ROOT=${TEMP_ROOT%/}
WORK_DIR=$(mktemp -d "$TEMP_ROOT/yaza-text-builder.XXXXXX")
trap 'rm -rf "$WORK_DIR"' EXIT HUP INT TERM

if [ ! -x "$LLVM_COMPILER" ]; then
  printf 'error: executable YAZA LLVM compiler not found: %s\n' "$LLVM_COMPILER" >&2
  exit 127
fi

SOURCE="$PROJECT_DIR/examples/core-text-builder-smoke.yz"
IR="$WORK_DIR/text-builder.ll"
NORMAL="$WORK_DIR/text-builder-normal"
RELEASE="$WORK_DIR/text-builder-release"
SANITIZED="$WORK_DIR/text-builder-sanitized"

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
  if [ "$OUTPUT" != "text-builder-ok" ]; then
    printf '%s\n' "$OUTPUT"
    printf 'error: TextBuilder fixture failed: %s\n' "$PROGRAM" >&2
    exit 1
  fi
  printf '%s\n' "$OUTPUT"
done

printf 'std-text-builder-focused-validation-ok\n'

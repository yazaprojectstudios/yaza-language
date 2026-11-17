#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
CLANG=${CLANG:-clang}
BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/yaza-llvm-smoke.XXXXXX")
OUTPUT_BIN="$BUILD_DIR/yaza-llvm-hello"

if ! command -v "$CLANG" >/dev/null 2>&1; then
  printf 'error: Clang executable not found: %s\n' "$CLANG" >&2
  exit 127
fi

"$CLANG" -Wno-override-module -x ir "$PROJECT_DIR/examples/hello.ll" -o "$OUTPUT_BIN"
ACTUAL=$("$OUTPUT_BIN")
EXPECTED='Hello from YAZA LLVM prototype'

if [ "$ACTUAL" != "$EXPECTED" ]; then
  printf 'error: unexpected output\nexpected: %s\nactual:   %s\n' "$EXPECTED" "$ACTUAL" >&2
  exit 1
fi

BRIDGE_IR="$BUILD_DIR/legacy-c.ll"
BRIDGE_BIN="$BUILD_DIR/legacy-c"
BRIDGE_BC="$BUILD_DIR/legacy-c.bc"
BRIDGE_BC_BIN="$BUILD_DIR/legacy-c-bitcode"
CLANG="$CLANG" sh "$PROJECT_DIR/scripts/c-to-llvm-ir.sh" \
  "$PROJECT_DIR/examples/legacy-c.c" "$BRIDGE_IR"
"$CLANG" "$BRIDGE_IR" -o "$BRIDGE_BIN"
BRIDGE_ACTUAL=$("$BRIDGE_BIN")
BRIDGE_EXPECTED='Legacy C compiled through LLVM IR'
if [ "$BRIDGE_ACTUAL" != "$BRIDGE_EXPECTED" ]; then
  printf 'error: C-to-LLVM bridge output mismatch\nexpected: %s\nactual:   %s\n' \
    "$BRIDGE_EXPECTED" "$BRIDGE_ACTUAL" >&2
  exit 1
fi
CLANG="$CLANG" sh "$PROJECT_DIR/scripts/c-to-llvm-ir.sh" --bitcode \
  "$PROJECT_DIR/examples/legacy-c.c" "$BRIDGE_BC"
"$CLANG" "$BRIDGE_BC" -o "$BRIDGE_BC_BIN"
BRIDGE_BC_ACTUAL=$("$BRIDGE_BC_BIN")
if [ "$BRIDGE_BC_ACTUAL" != "$BRIDGE_EXPECTED" ]; then
  printf 'error: C-to-bitcode bridge output mismatch\nexpected: %s\nactual:   %s\n' \
    "$BRIDGE_EXPECTED" "$BRIDGE_BC_ACTUAL" >&2
  exit 1
fi

printf 'LLVM IR smoke passed with %s\n' "$CLANG"
printf 'LLVM prototype output: %s\n' "$ACTUAL"
printf 'C-to-LLVM bridge output: %s\n' "$BRIDGE_ACTUAL"
printf 'C-to-bitcode bridge output: %s\n' "$BRIDGE_BC_ACTUAL"
printf 'Executable: %s\n' "$OUTPUT_BIN"

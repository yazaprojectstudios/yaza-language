#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
LLVM_COMPILER=${YAZA_LLVM_COMPILER:-"$PROJECT_DIR/build/yaza-llvm"}
CLANG=${CLANG:-clang}
TEMP_ROOT=${TMPDIR:-/tmp}
TEMP_ROOT=${TEMP_ROOT%/}
WORK_DIR=$(mktemp -d "$TEMP_ROOT/yazabase-sql-cte-parser.XXXXXX")
trap 'rm -rf "$WORK_DIR"' EXIT HUP INT TERM

case "$LLVM_COMPILER" in
  /*) ;;
  *) LLVM_COMPILER="$(pwd)/$LLVM_COMPILER" ;;
esac
if [ ! -x "$LLVM_COMPILER" ]; then
  printf 'error: executable YAZA LLVM compiler not found: %s\n' "$LLVM_COMPILER" >&2
  exit 127
fi
if ! command -v "$CLANG" >/dev/null 2>&1; then
  printf 'error: Clang executable not found: %s\n' "$CLANG" >&2
  exit 127
fi

cd "$PROJECT_DIR"
IR="$WORK_DIR/yazabase-sql-cte-parser.ll"
SOURCE="$PROJECT_DIR/examples/yazabase-sql-cte-parser-v1-smoke.yz"
YAZA_RT_V1_PROCESS_LIMIT_BYTES=${YAZA_COMPILER_PROCESS_LIMIT_BYTES:-34359738368} \
YAZA_RT_V1_MAX_LIST_VECTOR_BYTES=${YAZA_COMPILER_MAX_LIST_VECTOR_BYTES:-34359738368} \
YAZA_RT_V1_MAX_LIST_ITEMS=${YAZA_COMPILER_MAX_LIST_ITEMS:-32000000} \
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$LLVM_COMPILER" emit-llvm "$SOURCE" -o "$IR"

compile_fixture() {
  output=$1
  shift
  "$CLANG" -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread "$@" \
    "$IR" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
    -I"$PROJECT_DIR/runtime/include" -lm -o "$output"
}

compile_fixture "$WORK_DIR/parser-debug"
compile_fixture "$WORK_DIR/parser-o2" -O2
compile_fixture "$WORK_DIR/parser-sanitized" -fsanitize=address,undefined -fno-omit-frame-pointer

for PROGRAM in "$WORK_DIR/parser-debug" "$WORK_DIR/parser-o2" "$WORK_DIR/parser-sanitized"; do
  OUTPUT=$(ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} "$PROGRAM")
  if [ "$OUTPUT" != "yazabase-sql-cte-parser-ok" ]; then
    printf '%s\n' "$OUTPUT"
    printf 'error: YazaBase CTE parser fixture failed: %s\n' "$PROGRAM" >&2
    exit 1
  fi
  printf '%s\n' "$OUTPUT"
done

printf 'yazabase-sql-cte-parser-focused-validation-ok\n'

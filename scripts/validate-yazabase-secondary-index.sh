#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
COMPILER=${YAZA_LLVM_COMPILER:-"$PROJECT_DIR/build/yaza-llvm"}
CLANG=${CLANG:-clang}
BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/yazabase-secondary-index.XXXXXX")
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
COMPILER_LIMIT_BYTES=${YAZA_COMPILER_PROCESS_LIMIT_BYTES:-34359738368}
COMPILER_MAX_VECTOR_BYTES=${YAZA_RT_V1_MAX_LIST_VECTOR_BYTES:-34359738368}
COMPILER_MAX_ITEMS=${YAZA_RT_V1_MAX_LIST_ITEMS:-32000000}
cd "$PROJECT_DIR"

build_and_run() {
  fixture=$1
  marker=$2
  output=$3
  YAZA_RT_V1_PROCESS_LIMIT_BYTES="$COMPILER_LIMIT_BYTES" \
    YAZA_RT_V1_MAX_LIST_VECTOR_BYTES="$COMPILER_MAX_VECTOR_BYTES" \
    YAZA_RT_V1_MAX_LIST_ITEMS="$COMPILER_MAX_ITEMS" \
    "$COMPILER" build "$PROJECT_DIR/examples/$fixture.yz" -o "$BUILD_DIR/$output"
  mkdir "$BUILD_DIR/$output-root"
  result=$("$BUILD_DIR/$output" "$BUILD_DIR/$output-root")
  printf '%s\n' "$result"
  case "$result" in
    *"$marker"*) ;;
    *)
      printf 'error: expected marker %s from %s\n' "$marker" "$fixture" >&2
      exit 1
      ;;
  esac
}

sanitize_and_run() {
  fixture=$1
  marker=$2
  output=$3
  ir="$BUILD_DIR/$output.ll"
  object="$BUILD_DIR/$output.o"
  executable="$BUILD_DIR/$output"
  YAZA_RT_V1_PROCESS_LIMIT_BYTES="$COMPILER_LIMIT_BYTES" \
    YAZA_RT_V1_MAX_LIST_VECTOR_BYTES="$COMPILER_MAX_VECTOR_BYTES" \
    YAZA_RT_V1_MAX_LIST_ITEMS="$COMPILER_MAX_ITEMS" \
    "$COMPILER" emit-llvm "$PROJECT_DIR/examples/$fixture.yz" -o "$ir"
  "$CLANG" -Wno-override-module -fsanitize=address,undefined \
    -x ir -c "$ir" -o "$object"
  "$CLANG" -std=c11 -Wall -Wextra -Werror -pthread \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    "$object" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
    -I"$PROJECT_DIR/runtime/include" -lm -o "$executable"
  mkdir "$BUILD_DIR/$output-root"
  result=$(ASAN_OPTIONS=detect_leaks=0 "$executable" "$BUILD_DIR/$output-root")
  printf '%s\n' "$result"
  case "$result" in
    *"$marker"*) ;;
    *)
      printf 'error: expected marker %s from sanitized %s\n' "$marker" "$fixture" >&2
      exit 1
      ;;
  esac
}

build_and_run yazabase-sql-create-index-parser-v1-smoke \
  yazabase-sql-create-index-parser-v1-ok create-index-parser
build_and_run yazabase-sql-create-index-v1-smoke \
  yazabase-sql-create-index-v1-ok create-index
build_and_run yazabase-sql-drop-table-v1-smoke \
  yazabase-sql-drop-table-v1-ok drop-table

sanitize_and_run yazabase-sql-create-index-v1-smoke \
  yazabase-sql-create-index-v1-ok create-index-asan
sanitize_and_run yazabase-sql-drop-table-v1-smoke \
  yazabase-sql-drop-table-v1-ok drop-table-asan

printf 'yazabase-secondary-index-focused-validation-ok\n'

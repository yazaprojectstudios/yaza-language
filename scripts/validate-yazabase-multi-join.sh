#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
COMPILER=${YAZA_LLVM_COMPILER:-"$PROJECT_DIR/build/yaza-llvm"}
CLANG=${CLANG:-clang}
TEMP_ROOT=${TMPDIR:-/tmp}
TEMP_ROOT=${TEMP_ROOT%/}
BUILD_DIR=$(mktemp -d "$TEMP_ROOT/yazabase-multi-join.XXXXXX")
trap 'rm -rf "$BUILD_DIR"' EXIT HUP INT TERM

case "$COMPILER" in
  /*) ;;
  *) COMPILER="$(pwd)/$COMPILER" ;;
esac
if [ ! -x "$COMPILER" ]; then
  printf 'error: executable YAZA LLVM compiler not found: %s\n' "$COMPILER" >&2
  exit 127
fi

COMPILER_PROCESS_LIMIT_BYTES=${YAZA_COMPILER_PROCESS_LIMIT_BYTES:-34359738368}
COMPILER_LIST_VECTOR_LIMIT_BYTES=${YAZA_COMPILER_LIST_VECTOR_LIMIT_BYTES:-34359738368}
COMPILER_MAX_LIST_ITEMS=${YAZA_COMPILER_MAX_LIST_ITEMS:-32000000}
export YAZA_COMPILER_PROCESS_LIMIT_BYTES
export COMPILER_LIST_VECTOR_LIMIT_BYTES
export COMPILER_MAX_LIST_ITEMS

if [ "$#" -eq 0 ]; then
  set -- \
    yazabase-sql-parser-v3-smoke \
    yazabase-sql-binder-v1-smoke \
    yazabase-sql-explain-v1-smoke \
    yazabase-sql-multi-join-memory-v1-smoke \
    yazabase-sql-inner-join-persist-v1-smoke \
    yazabase-sql-multi-join-persist-v1-smoke
fi

for fixture in "$@"; do
  case "$fixture" in
    yazabase-sql-parser-v3-smoke|yazabase-sql-binder-v1-smoke|yazabase-sql-explain-v1-smoke|yazabase-sql-multi-join-memory-v1-smoke|yazabase-sql-inner-join-persist-v1-smoke|yazabase-sql-multi-join-persist-v1-smoke) ;;
    *) printf 'error: unsupported focused fixture: %s\n' "$fixture" >&2; exit 2 ;;
  esac
done

run_fixture() {
  mode=$1
  fixture=$2
  expected=$3
  executable="$BUILD_DIR/$fixture-$mode"
  if [ "$fixture" = "yazabase-sql-multi-join-persist-v1-smoke" ] || [ "$fixture" = "yazabase-sql-inner-join-persist-v1-smoke" ]; then
    root="$BUILD_DIR/$fixture-root-$mode"
    mkdir "$root"
    if [ "$mode" = asan ]; then
      output=$(ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} "$executable" "$root")
    else
      output=$("$executable" "$root")
    fi
  elif [ "$mode" = asan ]; then
    output=$(ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} "$executable")
  else
    output=$("$executable")
  fi
  printf '%s\n' "$output"
  case "$output" in
    *"$expected"*) ;;
    *) printf 'error: %s %s fixture failed\n' "$fixture" "$mode" >&2; exit 1 ;;
  esac
}

cd "$PROJECT_DIR"
for fixture in "$@"; do
  ir="$BUILD_DIR/$fixture.ll"
  YAZA_RT_V1_PROCESS_LIMIT_BYTES="$COMPILER_PROCESS_LIMIT_BYTES" \
    YAZA_RT_V1_MAX_LIST_VECTOR_BYTES="$COMPILER_LIST_VECTOR_LIMIT_BYTES" \
    YAZA_RT_V1_MAX_LIST_ITEMS="$COMPILER_MAX_LIST_ITEMS" \
    YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
    "$COMPILER" emit-llvm "examples/$fixture.yz" -o "$ir"

  "$CLANG" -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread \
    "$ir" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
    -I"$PROJECT_DIR/runtime/include" -lm -o "$BUILD_DIR/$fixture-normal"
  "$CLANG" -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    "$ir" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
    -I"$PROJECT_DIR/runtime/include" -lm -o "$BUILD_DIR/$fixture-asan"

  expected="${fixture%-smoke}-ok"
  if [ "$fixture" = "yazabase-sql-multi-join-memory-v1-smoke" ]; then
    expected="yazabase-sql-multi-join-memory-v1-ok"
  elif [ "$fixture" = "yazabase-sql-multi-join-persist-v1-smoke" ]; then
    expected="yazabase-sql-multi-join-persist-v1-ok"
  fi
  run_fixture normal "$fixture" "$expected"
  run_fixture asan "$fixture" "$expected"
done

printf 'yazabase-multi-join-focused-validation-ok\n'

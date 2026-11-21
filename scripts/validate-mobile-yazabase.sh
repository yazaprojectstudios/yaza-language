#!/bin/sh
set -eu

PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
YAZA_BIN=${YAZA_BIN:-"$PROJECT_DIR/build/yaza-llvm"}
if [ ! -x "$YAZA_BIN" ]; then
  printf 'Missing executable compiler: %s\n' "$YAZA_BIN" >&2
  exit 1
fi

WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/yaza-mobile-yazabase.XXXXXX")
trap 'rm -rf "$WORK_DIR"' EXIT HUP INT TERM
export YAZA_RUNTIME_DIR="$PROJECT_DIR/runtime"
export YAZA_STDLIB_DIR="$PROJECT_DIR/std"

YAZA_RT_V1_PROCESS_LIMIT_BYTES=${YAZA_COMPILER_PROCESS_LIMIT_BYTES:-34359738368} \
  "$YAZA_BIN" emit-llvm "$PROJECT_DIR/examples/mobile-yazabase-smoke.yz" -o "$WORK_DIR/mobile-yazabase-smoke.ll"

mkdir -p "$WORK_DIR/data-normal" "$WORK_DIR/data-asan" "$WORK_DIR/data-leaks"

clang -Wno-override-module -std=c11 -O2 -pthread -DYAZA_RT_V1_DISABLE_PROCESS_SPAWN=1 \
  "$WORK_DIR/mobile-yazabase-smoke.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$WORK_DIR/mobile-yazabase-smoke"
YAZA_MOBILE_DB_TEST_DIR="$WORK_DIR/data-normal" "$WORK_DIR/mobile-yazabase-smoke"

if [ "$(uname -s)" = "Linux" ]; then
  MOBILE_ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1}
else
  MOBILE_ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0}
fi

clang -Wno-override-module -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -pthread \
  -DYAZA_RT_V1_DISABLE_PROCESS_SPAWN=1 \
  "$WORK_DIR/mobile-yazabase-smoke.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$WORK_DIR/mobile-yazabase-smoke-asan"
ASAN_OPTIONS="$MOBILE_ASAN_OPTIONS" YAZA_MOBILE_DB_TEST_DIR="$WORK_DIR/data-asan" "$WORK_DIR/mobile-yazabase-smoke-asan"

if [ "$(uname -s)" = "Darwin" ] && command -v leaks >/dev/null 2>&1; then
  clang -Wno-override-module -std=c11 -O1 -g -pthread -DYAZA_RT_V1_DISABLE_PROCESS_SPAWN=1 \
    "$WORK_DIR/mobile-yazabase-smoke.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
    -I"$PROJECT_DIR/runtime/include" -lm -o "$WORK_DIR/mobile-yazabase-smoke-leaks"
  YAZA_MOBILE_DB_TEST_DIR="$WORK_DIR/data-leaks" MallocStackLogging=1 leaks --atExit -- "$WORK_DIR/mobile-yazabase-smoke-leaks"
fi

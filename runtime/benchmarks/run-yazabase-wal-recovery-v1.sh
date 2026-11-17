#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)
COMPILER=${YAZA_LLVM_COMPILER:-"$PROJECT_DIR/build/yaza-llvm"}
CLANG=${CLANG:-clang}
CC=${CC:-clang}
TEMP_ROOT=${TMPDIR:-/tmp}
TEMP_ROOT=${TEMP_ROOT%/}
BUILD_DIR=$(mktemp -d "$TEMP_ROOT/yazabase-wal-recovery.XXXXXX")
trap 'rm -rf "$BUILD_DIR"' EXIT HUP INT TERM

case "$COMPILER" in
  /*) ;;
  *) COMPILER="$(pwd)/$COMPILER" ;;
esac
if [ ! -x "$COMPILER" ]; then
  printf 'error: executable YAZA LLVM compiler not found: %s\n' "$COMPILER" >&2
  exit 127
fi

cd "$PROJECT_DIR"
mkdir "$BUILD_DIR/root"

printf '== Building valid synthetic WAL prefix ==\n'
"$CC" -std=c11 -O2 -Wall -Wextra -Werror \
  "$SCRIPT_DIR/wal_prefix_fixture_v1.c" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -pthread -lm -o "$BUILD_DIR/wal-prefix-fixture"
"$BUILD_DIR/wal-prefix-fixture" "$BUILD_DIR/root" 200000

printf '\n== Building YAZA recovery scanner benchmark ==\n'
YAZA_RT_V1_PROCESS_LIMIT_BYTES=${YAZA_COMPILER_PROCESS_LIMIT_BYTES:-17179869184} \
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build --release "$SCRIPT_DIR/yazabase_wal_recovery_v1.yz" -o "$BUILD_DIR/wal-recovery-bench"

printf '\n== Measuring scanner-only recovery paths ==\n'
YAZA_RT_V1_PROCESS_LIMIT_BYTES=${YAZA_RT_V1_PROCESS_LIMIT_BYTES:-536870912} \
  "$BUILD_DIR/wal-recovery-bench" "$BUILD_DIR/root"

printf '\nNote: synthetic 8 MB prefix; warm OS file cache; scanner-only, not database open/replay latency.\n'

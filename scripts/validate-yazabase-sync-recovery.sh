#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
COMPILER=${YAZA_LLVM_COMPILER:-"$PROJECT_DIR/build/yaza-llvm"}
RUNTIME_CC=${CC:-clang}
SANITIZER_MODE=${1:-none}
HARNESS_MODE=${2:-full}
HARNESS_BOUNDARY=${3:-}
TEMP_ROOT=${TMPDIR:-/tmp}
TEMP_ROOT=${TEMP_ROOT%/}
BUILD_DIR=$(mktemp -d "$TEMP_ROOT/yazabase-sync-recovery.XXXXXX")
if [ "${YAZA_KEEP_BUILD_DIR:-0}" = 1 ]; then
  printf 'keeping YazaBase recovery build artifacts: %s\n' "$BUILD_DIR" >&2
else
  trap 'rm -rf "$BUILD_DIR"' EXIT HUP INT TERM
fi

case "$COMPILER" in
  /*) ;;
  *) COMPILER="$(pwd)/$COMPILER" ;;
esac
if [ ! -x "$COMPILER" ]; then
  printf 'error: executable YAZA LLVM compiler not found: %s\n' "$COMPILER" >&2
  exit 127
fi

cd "$PROJECT_DIR"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  YAZA_RT_V1_PROCESS_LIMIT_BYTES=${YAZA_COMPILER_PROCESS_LIMIT_BYTES:-34359738368} \
  YAZA_RT_V1_MAX_LIST_VECTOR_BYTES=${YAZA_COMPILER_LIST_VECTOR_LIMIT_BYTES:-34359738368} \
  YAZA_RT_V1_MAX_LIST_ITEMS=${YAZA_COMPILER_MAX_LIST_ITEMS:-32000000} \
  "$COMPILER" emit-llvm \
    "$PROJECT_DIR/examples/yazabase-sync-recovery-module.yz" \
    -o "$BUILD_DIR/yazabase-sync-recovery.ll"

# This is a test-only embedding shim: expose the selected YAZA function and
# rename the generated process entry point so the C crash harness owns main().
sed \
  -e 's/^define internal i64 @yaza\.core\.fn\.yazabase_sync_recovery_operation_v1(/define i64 @yaza.core.fn.yazabase_sync_recovery_operation_v1(/' \
  -e 's/^define i32 @main(/define i32 @yazabase_sync_recovery_module_main(/' \
  "$BUILD_DIR/yazabase-sync-recovery.ll" >"$BUILD_DIR/yazabase-sync-recovery-link.ll"

case "$HARNESS_MODE" in
  full|overflow|overflow-drop|secondary-index|unique-index|alter|range-delete|multi-leaf-insert|catalog-migration|views|foreign-key) ;;
  range-delete-boundary)
    case "$HARNESS_BOUNDARY" in
      ''|*[!0-9]*|0) printf 'error: range-delete-boundary requires a positive integer fsync boundary\n' >&2; exit 2 ;;
    esac
    ;;
  secondary-index-drop-boundary)
    case "$HARNESS_BOUNDARY" in
      ''|*[!0-9]*) printf 'error: secondary-index-drop-boundary requires a positive integer fsync boundary\n' >&2; exit 2 ;;
    esac
    ;;
  alter-boundary)
    case "$HARNESS_BOUNDARY" in
      ''|*[!0-9]*) printf 'error: alter-boundary requires a positive integer fsync boundary\n' >&2; exit 2 ;;
    esac
    ;;
  *) printf 'error: expected harness mode full, overflow, overflow-drop, secondary-index, unique-index, alter, range-delete, multi-leaf-insert, catalog-migration, views, foreign-key, range-delete-boundary, secondary-index-drop-boundary, or alter-boundary, got: %s\n' "$HARNESS_MODE" >&2; exit 2 ;;
esac
case "$SANITIZER_MODE" in
  none|asan|both) ;;
  *) printf 'error: expected sanitizer mode none, asan, or both, got: %s\n' "$SANITIZER_MODE" >&2; exit 2 ;;
esac

compile_harness() {
  output=$1
  sanitizer=$2
  set -- -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread \
    -DYAZA_VFS_V1_TESTING -I"$PROJECT_DIR/runtime/include"
  if [ "$sanitizer" = asan ]; then
    set -- "$@" -fsanitize=address,undefined -fno-omit-frame-pointer
  fi
  "$RUNTIME_CC" "$@" \
    "$BUILD_DIR/yazabase-sync-recovery-link.ll" \
    "$PROJECT_DIR/runtime/tests/yazabase_sync_recovery_v1.c" \
    "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" -lm \
    -o "$BUILD_DIR/yazabase-sync-recovery-$output"
}

run_harness() {
  executable=$1
  if [ "$HARNESS_MODE" = secondary-index-drop-boundary ] || [ "$HARNESS_MODE" = alter-boundary ] || [ "$HARNESS_MODE" = range-delete-boundary ]; then
    ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} "$BUILD_DIR/yazabase-sync-recovery-$executable" "$HARNESS_MODE" "$HARNESS_BOUNDARY"
  else
    ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} "$BUILD_DIR/yazabase-sync-recovery-$executable" "$HARNESS_MODE"
  fi
}

case "$SANITIZER_MODE" in
  none) compile_harness normal none; run_harness normal ;;
  asan) compile_harness asan asan; run_harness asan ;;
  both)
    compile_harness normal none
    compile_harness asan asan
    run_harness normal
    run_harness asan
    ;;
esac

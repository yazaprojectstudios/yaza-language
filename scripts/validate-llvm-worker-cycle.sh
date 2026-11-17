#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
LLVM_COMPILER=${1:-"$PROJECT_DIR/build/yaza-llvm"}
SANITIZER_MODE=${2:-none}
RUNTIME_CC=${CC:-clang}
TEMP_ROOT=${TMPDIR:-/tmp}
TEMP_ROOT=${TEMP_ROOT%/}
BUILD_DIR=$(mktemp -d "$TEMP_ROOT/yaza-llvm-worker-cycle.XXXXXX")
trap 'rm -rf "$BUILD_DIR"' EXIT HUP INT TERM

YAZA_STDLIB_DIR="$PROJECT_DIR/std" "$LLVM_COMPILER" emit-llvm-module \
  "$PROJECT_DIR/examples/core-arena-worker-module-smoke.yz" \
  --export-number request_length -o "$BUILD_DIR/request-module.ll"

if YAZA_STDLIB_DIR="$PROJECT_DIR/std" "$LLVM_COMPILER" emit-llvm-module \
  "$PROJECT_DIR/examples/core-arena-worker-module-smoke.yz" \
  --export-number request_text -o "$BUILD_DIR/invalid-module.ll" >"$BUILD_DIR/invalid-export.log" 2>&1; then
  printf 'error: module emitter accepted a non-Number C ABI export\n' >&2
  exit 1
fi
if ! grep -F 'LLVM module export: exported function parameters must all be Number' "$BUILD_DIR/invalid-export.log" >/dev/null; then
  cat "$BUILD_DIR/invalid-export.log" >&2
  printf 'error: invalid C ABI export reported the wrong diagnostic\n' >&2
  exit 1
fi

if YAZA_STDLIB_DIR="$PROJECT_DIR/std" "$LLVM_COMPILER" emit-llvm-module \
  "$PROJECT_DIR/examples/core-worker-failfast-errors.yz" \
  --export-number failfast_number_worker -o "$BUILD_DIR/failfast-module.ll" >"$BUILD_DIR/failfast-export.log" 2>&1; then
  printf 'error: Number worker export accepted a transitive fail-fast allocation path\n' >&2
  exit 1
fi
if ! grep -F 'number_text allocates formatted Text' "$BUILD_DIR/failfast-export.log" >/dev/null; then
  cat "$BUILD_DIR/failfast-export.log" >&2
  printf 'error: Number worker export did not identify its fail-fast allocation path\n' >&2
  exit 1
fi

set -- -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread
case "$SANITIZER_MODE" in
  none) ;;
  asan) set -- "$@" -fsanitize=address,undefined -fno-omit-frame-pointer ;;
  tsan) set -- "$@" -fsanitize=thread ;;
  *) printf 'error: unknown sanitizer mode: %s\n' "$SANITIZER_MODE" >&2; exit 2 ;;
esac
"$RUNTIME_CC" "$@" -I"$PROJECT_DIR/runtime/include" \
  "$BUILD_DIR/request-module.ll" \
  "$PROJECT_DIR/runtime/tests/yaza_worker_module_v1.c" \
  "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" -lm \
  -o "$BUILD_DIR/yaza-worker-module-test"
"$BUILD_DIR/yaza-worker-module-test"

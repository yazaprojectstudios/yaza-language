#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
RUNTIME_DIR="$PROJECT_DIR/runtime"
RUNTIME_CC=${CC:-clang}
REQUEST_COUNT=${1:-200000}

BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/yaza-soak-load.XXXXXX")
cleanup() {
  rm -rf "$BUILD_DIR"
}
trap cleanup EXIT INT TERM

printf '==> Building Soak Workload (-O2)...\n'
"$RUNTIME_CC" -std=c11 -O2 -Wall -Wextra -Werror -pthread \
  "$RUNTIME_DIR/tests/soak_workload_v1.c" \
  "$RUNTIME_DIR/src/yaza_runtime_v1.c" \
  -I"$RUNTIME_DIR/include" -lm -o "$BUILD_DIR/soak_release"

printf '==> Running Soak Workload (%s requests across 8 workers)...\n' "$REQUEST_COUNT"
"$BUILD_DIR/soak_release" "$REQUEST_COUNT"

printf '==> Building Soak Workload with AddressSanitizer...\n'
"$RUNTIME_CC" -std=c11 -fsanitize=address,undefined -fno-omit-frame-pointer -pthread \
  "$RUNTIME_DIR/tests/soak_workload_v1.c" \
  "$RUNTIME_DIR/src/yaza_runtime_v1.c" \
  -I"$RUNTIME_DIR/include" -lm -o "$BUILD_DIR/soak_asan"

printf '==> Running Sanitized Soak Workload (20,000 requests)...\n'
ASAN_OPTIONS="detect_leaks=0" "$BUILD_DIR/soak_asan" 20000

printf '\n[PASS] LLVM Microservice Soak and Load Qualification Succeeded!\n'

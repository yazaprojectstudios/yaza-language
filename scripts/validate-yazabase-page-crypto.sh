#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
COMPILER=${YAZA_LLVM_COMPILER:-"$PROJECT_DIR/build/yaza-llvm"}
CLANG=${CLANG:-clang}
TEMP_ROOT=${TMPDIR:-/tmp}
TEMP_ROOT=${TEMP_ROOT%/}
BUILD_DIR=$(mktemp -d "$TEMP_ROOT/yazabase-page-crypto.XXXXXX")
trap 'rm -rf "$BUILD_DIR"' EXIT HUP INT TERM

cd "$PROJECT_DIR"
printf '==> Page Crypto Gate 1: Emitting LLVM IR...\n'
YAZA_RT_V1_PROCESS_LIMIT_BYTES=${YAZA_COMPILER_PROCESS_LIMIT_BYTES:-34359738368} \
YAZA_RT_V1_MAX_LIST_VECTOR_BYTES=${YAZA_COMPILER_LIST_VECTOR_LIMIT_BYTES:-34359738368} \
YAZA_RT_V1_MAX_LIST_ITEMS=${YAZA_COMPILER_MAX_LIST_ITEMS:-32000000} \
YAZA_STDLIB_DIR=${YAZA_STDLIB_DIR:-"$PROJECT_DIR/std"} \
  "$COMPILER" emit-llvm examples/yazabase-page-crypto-v1-smoke.yz -o "$BUILD_DIR/page-crypto.ll"

printf '==> Page Crypto Gate 2: Compiling normal, -O2, and ASan+UBSan binaries...\n'
"$CLANG" -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread \
  "$BUILD_DIR/page-crypto.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$BUILD_DIR/pc-normal"
"$CLANG" -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread -O2 \
  "$BUILD_DIR/page-crypto.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$BUILD_DIR/pc-o2"
"$CLANG" -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$BUILD_DIR/page-crypto.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$BUILD_DIR/pc-asan"

for mode in normal o2 asan; do
  mkdir -p "$BUILD_DIR/root-$mode"
  if [ "$mode" = asan ]; then
    output=$(ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} "$BUILD_DIR/pc-$mode" "$BUILD_DIR/root-$mode")
  else
    output=$("$BUILD_DIR/pc-$mode" "$BUILD_DIR/root-$mode")
  fi
  printf '%s (%s)\n' "$output" "$mode"
  case "$output" in
    *"yazabase-page-crypto-v1-smoke: PASS"*) ;;
    *) printf 'error: page crypto smoke failed in %s mode\n' "$mode" >&2; exit 1 ;;
  esac
done

printf 'yazabase-page-crypto-v1-validation-ok\n'

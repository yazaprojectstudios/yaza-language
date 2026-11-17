#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
COMPILER=${YAZA_LLVM_COMPILER:-"$PROJECT_DIR/build/yaza-llvm"}
CLANG=${CLANG:-clang}
TEMP_ROOT=${TMPDIR:-/tmp}
TEMP_ROOT=${TEMP_ROOT%/}
BUILD_DIR=$(mktemp -d "$TEMP_ROOT/yazabase-session.XXXXXX")
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
YAZA_RT_V1_PROCESS_LIMIT_BYTES=${YAZA_COMPILER_PROCESS_LIMIT_BYTES:-34359738368} \
YAZA_RT_V1_MAX_LIST_VECTOR_BYTES=${YAZA_COMPILER_LIST_VECTOR_LIMIT_BYTES:-34359738368} \
YAZA_RT_V1_MAX_LIST_ITEMS=${YAZA_COMPILER_MAX_LIST_ITEMS:-32000000} \
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/yazabase-session-v1-smoke.yz -o "$BUILD_DIR/session.ll"

"$CLANG" -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread \
  "$BUILD_DIR/session.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$BUILD_DIR/session-normal"
"$CLANG" -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread -O2 \
  "$BUILD_DIR/session.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$BUILD_DIR/session-o2"
"$CLANG" -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$BUILD_DIR/session.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$BUILD_DIR/session-asan"

mkdir "$BUILD_DIR/root-normal" "$BUILD_DIR/root-o2" "$BUILD_DIR/root-asan"
for mode in normal o2 asan; do
  status=0
  if [ "$mode" = asan ]; then
    output=$(ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} "$BUILD_DIR/session-$mode" "$BUILD_DIR/root-$mode") || status=$?
  else
    output=$("$BUILD_DIR/session-$mode" "$BUILD_DIR/root-$mode") || status=$?
  fi
  printf '%s\n' "$output"
  if [ "$status" -ne 0 ]; then
    printf 'error: session fixture exited with status %s in %s mode\n' "$status" "$mode" >&2
    exit "$status"
  fi
  case "$output" in
    *yazabase-session-v1-ok*) ;;
    *) printf 'error: YazaBase session fixture failed in %s mode\n' "$mode" >&2; exit 1 ;;
  esac
done

printf 'yazabase-session-focused-validation-ok\n'

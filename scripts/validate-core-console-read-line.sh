#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
COMPILER=${YAZA_LLVM_COMPILER:-"$PROJECT_DIR/build/yaza-llvm"}
CLANG=${CLANG:-clang}
TEMP_ROOT=${TMPDIR:-/tmp}
TEMP_ROOT=${TEMP_ROOT%/}
BUILD_DIR=$(mktemp -d "$TEMP_ROOT/yaza-console-read-line.XXXXXX")
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
YAZA_RT_V1_PROCESS_LIMIT_BYTES=${YAZA_COMPILER_PROCESS_LIMIT_BYTES:-17179869184} \
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/core-console-read-line-smoke.yz -o "$BUILD_DIR/console-read-line.ll"
"$CLANG" -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread -O2 \
  "$BUILD_DIR/console-read-line.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$BUILD_DIR/console-read-line"
"$CLANG" -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread -O1 \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$BUILD_DIR/console-read-line.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$BUILD_DIR/console-read-line-asan"

printf 'alpha\n\nβeta\r\nfinal' > "$BUILD_DIR/input"
printf 'alpha\n\nβeta\nfinal\n' > "$BUILD_DIR/expected"
"$BUILD_DIR/console-read-line" < "$BUILD_DIR/input" > "$BUILD_DIR/output"
cmp "$BUILD_DIR/expected" "$BUILD_DIR/output"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} "$BUILD_DIR/console-read-line-asan" \
  < "$BUILD_DIR/input" > "$BUILD_DIR/sanitized-output"
cmp "$BUILD_DIR/expected" "$BUILD_DIR/sanitized-output"

: > "$BUILD_DIR/empty"
"$BUILD_DIR/console-read-line" < "$BUILD_DIR/empty" > "$BUILD_DIR/empty-output"
test ! -s "$BUILD_DIR/empty-output"

printf '\377\n' > "$BUILD_DIR/invalid-utf8"
status=0
"$BUILD_DIR/console-read-line" < "$BUILD_DIR/invalid-utf8" > "$BUILD_DIR/invalid-output" 2> "$BUILD_DIR/invalid-error" || status=$?
if [ "$status" -eq 0 ] || ! rg -q 'not valid NUL-free UTF-8 Text' "$BUILD_DIR/invalid-error"; then
  printf 'error: invalid UTF-8 console input did not fail with the expected diagnostic\n' >&2
  exit 1
fi

head -c 1048577 /dev/zero > "$BUILD_DIR/overlong"
status=0
"$BUILD_DIR/console-read-line" < "$BUILD_DIR/overlong" > "$BUILD_DIR/overlong-output" 2> "$BUILD_DIR/overlong-error" || status=$?
if [ "$status" -eq 0 ] || ! rg -q 'exceeds the 1 MiB limit' "$BUILD_DIR/overlong-error"; then
  printf 'error: over-limit console input did not fail with the expected diagnostic\n' >&2
  exit 1
fi

printf 'core-console-read-line-focused-validation-ok\n'

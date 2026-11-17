#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
COMPILER=${YAZA_LLVM_COMPILER:-"$PROJECT_DIR/build/yaza-llvm"}
CLANG=${CLANG:-clang}
TEMP_ROOT=${TMPDIR:-/tmp}
TEMP_ROOT=${TEMP_ROOT%/}
BUILD_DIR=$(mktemp -d "$TEMP_ROOT/yazabase-sql-cte.XXXXXX")
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
  "$COMPILER" emit-llvm examples/yazabase-sql-cte-v1-smoke.yz -o "$BUILD_DIR/cte.ll"

"$CLANG" -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread \
  "$BUILD_DIR/cte.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$BUILD_DIR/cte-normal"
"$CLANG" -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread -O2 \
  "$BUILD_DIR/cte.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$BUILD_DIR/cte-o2"
"$CLANG" -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$BUILD_DIR/cte.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$BUILD_DIR/cte-asan"

mkdir "$BUILD_DIR/normal-root" "$BUILD_DIR/o2-root" "$BUILD_DIR/asan-root"
if normal_output=$("$BUILD_DIR/cte-normal" "$BUILD_DIR/normal-root"); then
  :
else
  normal_status=$?
  printf '%s\n' "$normal_output" >&2
  exit "$normal_status"
fi
printf '%s\n' "$normal_output"
case "$normal_output" in
  *yazabase-sql-cte-v1-ok*) ;;
  *) printf 'error: normal YazaBase CTE fixture failed\n' >&2; exit 1 ;;
esac
if o2_output=$("$BUILD_DIR/cte-o2" "$BUILD_DIR/o2-root"); then
  :
else
  o2_status=$?
  printf '%s\n' "$o2_output" >&2
  exit "$o2_status"
fi
printf '%s\n' "$o2_output"
case "$o2_output" in
  *yazabase-sql-cte-v1-ok*) ;;
  *) printf 'error: optimized YazaBase CTE fixture failed\n' >&2; exit 1 ;;
esac
if asan_output=$(ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} \
  "$BUILD_DIR/cte-asan" "$BUILD_DIR/asan-root"); then
  :
else
  asan_status=$?
  printf '%s\n' "$asan_output" >&2
  exit "$asan_status"
fi
printf '%s\n' "$asan_output"
case "$asan_output" in
  *yazabase-sql-cte-v1-ok*) ;;
  *) printf 'error: ASan/UBSan YazaBase CTE fixture failed\n' >&2; exit 1 ;;
esac

printf 'yazabase-sql-cte-focused-validation-ok\n'

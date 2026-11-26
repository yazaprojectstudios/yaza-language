#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
COMPILER=${YAZA_LLVM_COMPILER:-"$PROJECT_DIR/build/yaza-llvm"}
CLANG=${CLANG:-clang}
TEMP_ROOT=${TMPDIR:-/tmp}
TEMP_ROOT=${TEMP_ROOT%/}
WORK_DIR=$(mktemp -d "$TEMP_ROOT/yaza-tier2-search.XXXXXX")
trap 'rm -rf "$WORK_DIR"' EXIT HUP INT TERM

cd "$PROJECT_DIR"
printf '==> Tier 2.2 Search: Emitting LLVM IR...\n'
YAZA_RT_V1_PROCESS_LIMIT_BYTES=${YAZA_COMPILER_PROCESS_LIMIT_BYTES:-34359738368} \
YAZA_RT_V1_MAX_LIST_VECTOR_BYTES=${YAZA_COMPILER_LIST_VECTOR_LIMIT_BYTES:-34359738368} \
YAZA_RT_V1_MAX_LIST_ITEMS=${YAZA_COMPILER_MAX_LIST_ITEMS:-32000000} \
YAZA_STDLIB_DIR=${YAZA_STDLIB_DIR:-"$PROJECT_DIR/std"} \
  "$COMPILER" emit-llvm examples/yazabase-tier2-search-smoke.yz -o "$WORK_DIR/t2s.ll"

for mode in normal o2 asan; do
  case "$mode" in
    normal) set -- ;;
    o2) set -- -O2 ;;
    asan) set -- -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer ;;
  esac
  printf '==> Tier 2.2 Search: Compiling and running %s...\n' "$mode"
  "$CLANG" -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread "$@" \
    "$WORK_DIR/t2s.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
    -I"$PROJECT_DIR/runtime/include" -lm -o "$WORK_DIR/t2s-$mode"
  mkdir -p "$WORK_DIR/run-$mode"
  if [ "$mode" = asan ]; then
    ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} UBSAN_OPTIONS=halt_on_error=1 \
      "$WORK_DIR/t2s-$mode" "$WORK_DIR/run-$mode" > "$WORK_DIR/out-$mode.txt" 2>&1 || { cat "$WORK_DIR/out-$mode.txt" >&2; exit 1; }
  else
    "$WORK_DIR/t2s-$mode" "$WORK_DIR/run-$mode" > "$WORK_DIR/out-$mode.txt" 2>&1 || { cat "$WORK_DIR/out-$mode.txt" >&2; exit 1; }
  fi
  cat "$WORK_DIR/out-$mode.txt"
  if ! grep -Fq "yazabase-tier2-search-smoke: PASS" "$WORK_DIR/out-$mode.txt"; then
    printf 'error: Tier 2 search smoke failed in %s mode\n' "$mode" >&2
    exit 1
  fi
done

printf 'yazabase-tier2-search-validation-ok\n'

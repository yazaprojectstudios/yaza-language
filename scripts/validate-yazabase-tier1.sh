#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
COMPILER=${YAZA_LLVM_COMPILER:-"$ROOT_DIR/build/yaza-llvm"}
CLANG=${CLANG:-clang}
WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/yaza-tier1-smoke.XXXXXX")
trap 'rm -rf "$WORK_DIR"' EXIT HUP INT TERM

cd "$ROOT_DIR"

case "${1:-all}" in
  all) FIXTURE=yazabase-tier1-smoke ;;
  write) FIXTURE=yazabase-tier1-write-smoke ;;
  *) printf 'usage: %s [all|write]\n' "$0" >&2; exit 2 ;;
esac

# 1. Authenticated Page-Level & WAL/Backup Encryption-at-Rest (Tier 1 #3)
if [ "$FIXTURE" = yazabase-tier1-smoke ]; then
  sh "$ROOT_DIR/scripts/validate-yazabase-page-crypto.sh"
fi

# 2. Delta WAL + Write Plan Cache + Bidirectional Logical Sync (Tier 1 #1 & #2)
printf '==> Tier 1: Emitting integration LLVM IR...\n'
YAZA_RT_V1_PROCESS_LIMIT_BYTES=${YAZA_COMPILER_PROCESS_LIMIT_BYTES:-34359738368} \
YAZA_RT_V1_MAX_LIST_VECTOR_BYTES=${YAZA_COMPILER_LIST_VECTOR_LIMIT_BYTES:-34359738368} \
YAZA_RT_V1_MAX_LIST_ITEMS=${YAZA_COMPILER_MAX_LIST_ITEMS:-32000000} \
YAZA_STDLIB_DIR=${YAZA_STDLIB_DIR:-"$ROOT_DIR/std"} \
  "$COMPILER" emit-llvm "examples/$FIXTURE.yz" -o "$WORK_DIR/tier1.ll"

for mode in normal o2 asan; do
  case "$mode" in
    normal) set -- ;;
    o2) set -- -O2 ;;
    asan) set -- -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer ;;
  esac
  printf '==> Tier 1: Compiling and running %s...\n' "$mode"
  "$CLANG" -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread "$@" \
    "$WORK_DIR/tier1.ll" "$ROOT_DIR/runtime/src/yaza_runtime_v1.c" \
    -I"$ROOT_DIR/runtime/include" -lm -o "$WORK_DIR/tier1-$mode"
  mkdir -p "$WORK_DIR/run-$mode/dw" "$WORK_DIR/run-$mode/srv" "$WORK_DIR/run-$mode/mob"
  if [ "$mode" = asan ]; then
    ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} UBSAN_OPTIONS=halt_on_error=1 \
      "$WORK_DIR/tier1-$mode" "$WORK_DIR/run-$mode" > "$WORK_DIR/output-$mode.txt"
  else
    "$WORK_DIR/tier1-$mode" "$WORK_DIR/run-$mode" > "$WORK_DIR/output-$mode.txt"
  fi
  cat "$WORK_DIR/output-$mode.txt"
  if ! grep -Fq "$FIXTURE: PASS" "$WORK_DIR/output-$mode.txt"; then
    printf 'error: Tier 1 smoke did not report PASS in %s mode\n' "$mode" >&2
    exit 1
  fi
done

printf '%s-validation-ok\n' "$FIXTURE"

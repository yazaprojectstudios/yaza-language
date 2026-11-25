#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
COMPILER=${YAZA_LLVM_COMPILER:-"$PROJECT_DIR/build/yaza-llvm"}
CLANG=${CLANG:-clang}
TEMP_ROOT=${TMPDIR:-/tmp}
TEMP_ROOT=${TEMP_ROOT%/}
BUILD_DIR=$(mktemp -d "$TEMP_ROOT/yazabase-phase-c.XXXXXX")
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
printf '==> Phase C Gate 1: Emitting LLVM IR...\n'
YAZA_RT_V1_PROCESS_LIMIT_BYTES=${YAZA_COMPILER_PROCESS_LIMIT_BYTES:-34359738368} \
YAZA_RT_V1_MAX_LIST_VECTOR_BYTES=${YAZA_COMPILER_LIST_VECTOR_LIMIT_BYTES:-34359738368} \
YAZA_RT_V1_MAX_LIST_ITEMS=${YAZA_COMPILER_MAX_LIST_ITEMS:-32000000} \
YAZA_STDLIB_DIR=${YAZA_STDLIB_DIR:-"$PROJECT_DIR/std"} \
  "$COMPILER" emit-llvm examples/yazabase-phase-c-smoke.yz -o "$BUILD_DIR/phase-c.ll"
printf '==> Phase C Gate 2: Compiling normal, -O2, and ASan binaries...\n'

ARGON2_CFLAGS=""
ARGON2_LDFLAGS=""
for root in "${YAZA_ARGON2_ROOT:-}" /opt/homebrew/opt/argon2 /usr/local/opt/argon2 /usr; do
  [ -n "$root" ] || continue
  if [ -f "$root/include/argon2.h" ]; then
    for lib in "$root/lib/libargon2.a" "$root/lib/x86_64-linux-gnu/libargon2.a" "$root/lib/aarch64-linux-gnu/libargon2.a"; do
      if [ -f "$lib" ]; then
        ARGON2_CFLAGS="-DYAZA_RT_V1_WITH_ARGON2=1 -I$root/include"
        ARGON2_LDFLAGS="$lib"
        break
      fi
    done
    [ -n "$ARGON2_LDFLAGS" ] && break
  fi
done

"$CLANG" -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread $ARGON2_CFLAGS \
  "$BUILD_DIR/phase-c.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" $ARGON2_LDFLAGS -lm -o "$BUILD_DIR/pc-normal"
"$CLANG" -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread -O2 $ARGON2_CFLAGS \
  "$BUILD_DIR/phase-c.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" $ARGON2_LDFLAGS -lm -o "$BUILD_DIR/pc-o2"
"$CLANG" -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer $ARGON2_CFLAGS \
  "$BUILD_DIR/phase-c.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" $ARGON2_LDFLAGS -lm -o "$BUILD_DIR/pc-asan"

for mode in normal o2 asan; do
  for sub in primary backup replica1 replica2 replica3 shard0 shard1 shard2; do
    mkdir -p "$BUILD_DIR/root-$mode/$sub"
  done
  status=0
  if [ "$mode" = asan ]; then
    output=$(ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} "$BUILD_DIR/pc-$mode" "$BUILD_DIR/root-$mode") || status=$?
  else
    output=$("$BUILD_DIR/pc-$mode" "$BUILD_DIR/root-$mode") || status=$?
  fi
  printf '%s\n' "$output"
  if [ "$status" -ne 0 ]; then
    printf 'error: phase-c fixture exited with status %s in %s mode\n' "$status" "$mode" >&2
    exit "$status"
  fi
  case "$output" in
    *yazabase-phase-c-smoke:\ PASS*) ;;
    *) printf 'error: YazaBase Phase C fixture failed in %s mode\n' "$mode" >&2; exit 1 ;;
  esac
done

printf 'yazabase-phase-c-focused-validation-ok\n'

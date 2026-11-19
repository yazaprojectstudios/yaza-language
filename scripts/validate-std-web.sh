#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
COMPILER=${YAZA_LLVM_COMPILER:-"$PROJECT_DIR/build/yaza-llvm"}
CLANG=${CLANG:-clang}
TEMP_ROOT=${TMPDIR:-/tmp}
TEMP_ROOT=${TEMP_ROOT%/}
WORK_DIR=$(mktemp -d "$TEMP_ROOT/yaza-std-web.XXXXXX")
trap 'rm -rf "$WORK_DIR"' EXIT HUP INT TERM

if [ ! -x "$COMPILER" ]; then
  printf 'error: executable YAZA LLVM compiler not found: %s\n' "$COMPILER" >&2
  exit 127
fi

mkdir -p "$WORK_DIR/public" "$WORK_DIR/std_public"
chmod 700 "$WORK_DIR/public"
printf 'body { color: #0c1f24; }\n' > "$WORK_DIR/public/app.css"
chmod 600 "$WORK_DIR/public/app.css"
printf 'SECRET=1\n' > "$WORK_DIR/secret.txt"
printf 'SECRET=2\n' > "$WORK_DIR/public/config.env"
chmod 600 "$WORK_DIR/public/config.env"
ln -s "$WORK_DIR/secret.txt" "$WORK_DIR/public/link.css"

printf ':root { --brand: #00e5a8; }\n' > "$WORK_DIR/std_public/yaza-ui.css"
printf 'SECRET=3\n' > "$WORK_DIR/std_public/config.env"

cd "$PROJECT_DIR"
printf '==> std/web Gate 1: Emitting LLVM IR...\n'
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" emit-llvm examples/std-web-smoke.yz -o "$WORK_DIR/std_web.ll"

printf '==> std/web Gate 2: Compiling & running in debug, -O2, and ASan/UBSan...\n'
for MODE in debug release sanitized; do
  OUT="$WORK_DIR/std-web-$MODE"
  set -- -Wno-override-module -std=c11 -Wall -Wextra -Werror -pthread
  if [ "$MODE" = release ]; then
    set -- "$@" -O2
  elif [ "$MODE" = sanitized ]; then
    set -- "$@" -fsanitize=address,undefined -fno-omit-frame-pointer
  fi
  "$CLANG" "$@" "$WORK_DIR/std_web.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
    -I"$PROJECT_DIR/runtime/include" -lm -o "$OUT"
  RES=$(ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} YAZA_WEB_SMOKE_PUBLIC_DIR="$WORK_DIR/public" YAZA_WEB_SMOKE_STD_PUBLIC_DIR="$WORK_DIR/std_public" "$OUT")
  if [ "$RES" != "std-web-smoke: PASS" ]; then
    printf '%s\n' "$RES"
    printf 'error: std-web-smoke failed in %s mode\n' "$MODE" >&2
    exit 1
  fi
  printf '  [%s] %s\n' "$MODE" "$RES"
done

printf 'std-web-focused-validation-ok\n'

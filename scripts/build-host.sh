#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
BUILD_DIR="$PROJECT_DIR/build"

for tool in clang xz ar; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    printf 'error: required build tool not found: %s\n' "$tool" >&2
    exit 127
  fi
done

HOST_OS=$(uname -s)
HOST_ARCH=$(uname -m)
case "$HOST_OS/$HOST_ARCH" in
  Darwin/arm64) TRIPLE=arm64-apple-darwin ;;
  Darwin/x86_64) TRIPLE=x86_64-apple-darwin ;;
  Linux/x86_64) TRIPLE=x86_64-unknown-linux-gnu ;;
  Linux/aarch64|Linux/arm64) TRIPLE=aarch64-unknown-linux-gnu ;;
  *)
    printf 'error: no checked-in bootstrap for %s/%s\n' "$HOST_OS" "$HOST_ARCH" >&2
    exit 1
    ;;
esac

ARGON2_ROOT=${YAZA_ARGON2_ROOT:-}
if [ -z "$ARGON2_ROOT" ]; then
  if [ "$HOST_OS" = Darwin ] && command -v brew >/dev/null 2>&1; then
    ARGON2_ROOT=$(brew --prefix argon2 2>/dev/null || true)
  elif [ "$HOST_OS" = Linux ]; then
    ARGON2_ROOT=/usr
  fi
fi

if [ -z "$ARGON2_ROOT" ] || [ ! -f "$ARGON2_ROOT/include/argon2.h" ]; then
  printf '%s\n' 'error: Argon2 development files are required to build the YazaBase CLI.' >&2
  printf '%s\n' 'Install libargon2-dev (Linux), brew install argon2 (macOS), or set YAZA_ARGON2_ROOT.' >&2
  exit 1
fi
if [ ! -f "$ARGON2_ROOT/lib/libargon2.a" ] && \
   [ ! -f "$ARGON2_ROOT/lib/x86_64-linux-gnu/libargon2.a" ] && \
   [ ! -f "$ARGON2_ROOT/lib/aarch64-linux-gnu/libargon2.a" ]; then
  printf 'error: static Argon2 library not found under %s/lib\n' "$ARGON2_ROOT" >&2
  exit 1
fi

BOOTSTRAP="$PROJECT_DIR/bootstrap/llvm_cli.$TRIPLE.ll.xz"
if [ ! -f "$BOOTSTRAP" ]; then
  printf 'error: bootstrap IR missing for %s: %s\n' "$TRIPLE" "$BOOTSTRAP" >&2
  exit 1
fi

mkdir -p "$BUILD_DIR"
printf '==> Building YAZA for %s from the checked-in bootstrap...\n' "$TRIPLE"
xz -dc "$BOOTSTRAP" > "$BUILD_DIR/llvm_cli.bootstrap.ll"
clang -Wno-override-module -std=c11 -O2 \
  "$BUILD_DIR/llvm_cli.bootstrap.ll" \
  "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -pthread -lm \
  -o "$BUILD_DIR/yaza-bootstrap"
rm -f "$BUILD_DIR/llvm_cli.bootstrap.ll"

printf '==> Rebuilding the compiler from YAZA source...\n'
cd "$PROJECT_DIR"
YAZA_RT_V1_PROCESS_LIMIT_BYTES=${YAZA_COMPILER_PROCESS_LIMIT_BYTES:-34359738368} \
YAZA_RT_V1_MAX_LIST_VECTOR_BYTES=${YAZA_COMPILER_LIST_VECTOR_LIMIT_BYTES:-34359738368} \
YAZA_RT_V1_MAX_LIST_ITEMS=${YAZA_COMPILER_MAX_LIST_ITEMS:-32000000} \
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
YAZA_RUNTIME_DIR="$PROJECT_DIR/runtime" \
  "$BUILD_DIR/yaza-bootstrap" build --release \
  "$PROJECT_DIR/compiler/llvm_cli.yz" -o "$BUILD_DIR/yaza-llvm"
ln -sf yaza-llvm "$BUILD_DIR/yaza"

printf '==> Building YazaBase CLI with Argon2id support...\n'
YAZA_ARGON2_ROOT="$ARGON2_ROOT" \
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
YAZA_RUNTIME_DIR="$PROJECT_DIR/runtime" \
YAZA_COMPILER_PROCESS_LIMIT_BYTES=${YAZA_COMPILER_PROCESS_LIMIT_BYTES:-34359738368} \
YAZA_COMPILER_LIST_VECTOR_LIMIT_BYTES=${YAZA_COMPILER_LIST_VECTOR_LIMIT_BYTES:-34359738368} \
YAZA_COMPILER_MAX_LIST_ITEMS=${YAZA_COMPILER_MAX_LIST_ITEMS:-32000000} \
  "$SCRIPT_DIR/build-yazabase-cli.sh"
ln -sf yazabase-cli "$BUILD_DIR/yazabase"

"$BUILD_DIR/yaza-llvm" --version
printf 'Built: %s and %s\n' "$BUILD_DIR/yaza-llvm" "$BUILD_DIR/yazabase-cli"

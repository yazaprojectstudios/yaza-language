#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
COMPILER=${YAZA_LLVM_COMPILER:-"$PROJECT_DIR/build/yaza-llvm"}
case "$COMPILER" in
  /*) ;;
  *) COMPILER="$(pwd)/$COMPILER" ;;
esac
CLANG=${CLANG:-clang}
DOCKER=${DOCKER:-docker}
BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/yaza-llvm-linux-target-smoke.XXXXXX")
trap 'rm -rf "$BUILD_DIR"' EXIT HUP INT TERM

if [ ! -x "$COMPILER" ]; then
  printf 'error: executable YAZA LLVM compiler not found: %s\n' "$COMPILER" >&2
  printf 'hint: build it with scripts/build-host.sh first\n' >&2
  exit 127
fi
if ! command -v "$CLANG" >/dev/null 2>&1; then
  printf 'error: Clang executable not found: %s\n' "$CLANG" >&2
  exit 127
fi
if ! command -v "$DOCKER" >/dev/null 2>&1; then
  printf 'error: Docker executable not found: %s\n' "$DOCKER" >&2
  exit 127
fi

cd "$PROJECT_DIR"
SOURCE=examples/core-arena-scope-repeated-smoke.yz
EXPECTED='reclaimed=10000'

validate_target() {
  target_name=$1
  target_triple=$2
  docker_platform=$3
  llvm_path="$BUILD_DIR/$target_name.ll"
  object_path="$BUILD_DIR/$target_name.o"

  YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
    "$COMPILER" emit-llvm --target "$target_triple" "$SOURCE" -o "$llvm_path"
  "$CLANG" --target="$target_triple" -x ir -c "$llvm_path" -o "$object_path"

  "$DOCKER" run --rm --platform "$docker_platform" \
    -e DEBIAN_FRONTEND=noninteractive \
    -e SMOKE_TARGET="$target_name" \
    -e SMOKE_EXPECTED="$EXPECTED" \
    -v "$PROJECT_DIR:/work:ro" \
    -v "$BUILD_DIR:/out" \
    -w /work debian:13 sh -ec '
      apt-get update -qq
      apt-get install -y -qq --no-install-recommends gcc libc6-dev >/dev/null
      gcc "/out/$SMOKE_TARGET.o" runtime/src/yaza_runtime_v1.c \
        -Iruntime/include -pthread -lm -o "/out/$SMOKE_TARGET"
      actual=$("/out/$SMOKE_TARGET")
      if [ "$actual" != "$SMOKE_EXPECTED" ]; then
        printf "error: output mismatch; expected %s, got %s\n" \
          "$SMOKE_EXPECTED" "$actual" >&2
        exit 1
      fi
      printf "%s\n" "$actual"
    '

  printf 'LLVM target smoke passed: %s (%s)\n' "$target_name" "$target_triple"
}

validate_target linux-arm64 aarch64-unknown-linux-gnu linux/arm64
validate_target linux-x86_64 x86_64-unknown-linux-gnu linux/amd64

printf 'LLVM Linux target smoke passed for ARM64 and x86_64 with Debian 13 runtime links.\n'
printf 'The x86_64 Docker run may be emulated when the host CPU is ARM64.\n'

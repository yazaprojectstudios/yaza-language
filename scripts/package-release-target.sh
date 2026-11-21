#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
  printf 'Usage: %s <linux-x64|linux-arm64|darwin-x64|darwin-arm64>\n' "$0" >&2
  exit 2
fi

PLATFORM=$1
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
BUILD_DIR=${YAZA_BUILD_DIR:-"$PROJECT_DIR/build"}
DIST_DIR=${YAZA_DIST_DIR:-"$PROJECT_DIR/dist"}
VERSION=${YAZA_VERSION:-$(cat "$PROJECT_DIR/VERSION")}

copy_template_tree() {
  tar -C "$1" \
    --exclude='build' --exclude='*/build' \
    --exclude='.gradle' --exclude='*/.gradle' \
    --exclude='.idea' --exclude='*/.idea' \
    --exclude='DerivedData' --exclude='*/DerivedData' \
    --exclude='node_modules' --exclude='*/node_modules' \
    --exclude='target' --exclude='*/target' \
    --exclude='out' --exclude='*/out' \
    -cf - . | tar -C "$2" -xf -
}

case "$PLATFORM" in
  linux-x64) EXPECTED_OS=Linux; EXPECTED_ARCH=x86_64 ;;
  linux-arm64) EXPECTED_OS=Linux; EXPECTED_ARCH=arm64 ;;
  darwin-x64) EXPECTED_OS=Darwin; EXPECTED_ARCH=x86_64 ;;
  darwin-arm64) EXPECTED_OS=Darwin; EXPECTED_ARCH=arm64 ;;
  *) printf 'error: unsupported release platform: %s\n' "$PLATFORM" >&2; exit 2 ;;
esac

HOST_OS=$(uname -s)
HOST_ARCH=$(uname -m)
if [ "$HOST_ARCH" = aarch64 ]; then HOST_ARCH=arm64; fi
if [ "$HOST_OS" != "$EXPECTED_OS" ] || [ "$HOST_ARCH" != "$EXPECTED_ARCH" ]; then
  printf 'error: refusing to label %s/%s binaries as %s\n' "$HOST_OS" "$HOST_ARCH" "$PLATFORM" >&2
  exit 1
fi

for binary in yaza-llvm yazabase-cli; do
  if [ ! -x "$BUILD_DIR/$binary" ]; then
    printf 'error: expected built executable is missing: %s\n' "$BUILD_DIR/$binary" >&2
    exit 1
  fi
done

if [ "$EXPECTED_OS" = Linux ]; then
  LIB_SUFFIX=so
  LIB_FLAGS='-shared -fPIC'
else
  LIB_SUFFIX=dylib
  LIB_FLAGS='-dynamiclib -fPIC'
fi

mkdir -p "$DIST_DIR"
clang $LIB_FLAGS "$PROJECT_DIR/runtime/src/yazabase_c_api.c" \
  -I"$PROJECT_DIR/runtime/include" -pthread -lm \
  -o "$BUILD_DIR/libyazabase.$LIB_SUFFIX"

YAZA_STAGE="$DIST_DIR/yaza-$VERSION-$PLATFORM"
YB_STAGE="$DIST_DIR/yazabase-$VERSION-$PLATFORM"
rm -rf "$YAZA_STAGE" "$YB_STAGE"
mkdir -p "$YAZA_STAGE/bin" "$YAZA_STAGE/runtime/src" \
  "$YAZA_STAGE/runtime/include" "$YAZA_STAGE/runtime/mobile" \
  "$YAZA_STAGE/scripts/mobile" "$YAZA_STAGE/std" "$YAZA_STAGE/templates" \
  "$YAZA_STAGE/yazabase" "$YAZA_STAGE/include" "$YAZA_STAGE/lib" "$YAZA_STAGE/bindings"

cp "$BUILD_DIR/yaza-llvm" "$YAZA_STAGE/bin/yaza"
cp "$BUILD_DIR/yaza-llvm" "$YAZA_STAGE/bin/yaza-llvm"
cp "$BUILD_DIR/yazabase-cli" "$YAZA_STAGE/bin/yazabase"
cp "$BUILD_DIR/yazabase-cli" "$YAZA_STAGE/bin/yazabase-cli"
cp "$BUILD_DIR/libyazabase.$LIB_SUFFIX" "$YAZA_STAGE/lib/"
cp "$PROJECT_DIR/runtime/include/yazabase.h" "$YAZA_STAGE/include/"
cp "$PROJECT_DIR/runtime/include/yaza_runtime_v1.h" "$YAZA_STAGE/runtime/include/"
cp "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" "$YAZA_STAGE/runtime/src/"
cp "$PROJECT_DIR/runtime/src/yaza_transport_curl_v1.inc" "$YAZA_STAGE/runtime/src/"
cp "$PROJECT_DIR/runtime/src/yaza_security_login_v1.inc" "$YAZA_STAGE/runtime/src/"
cp "$PROJECT_DIR/runtime/watch-run.sh" "$YAZA_STAGE/runtime/"
cp -R "$PROJECT_DIR/runtime/mobile/." "$YAZA_STAGE/runtime/mobile/"
cp -R "$PROJECT_DIR/scripts/mobile/." "$YAZA_STAGE/scripts/mobile/"
cp -R "$PROJECT_DIR/std/." "$YAZA_STAGE/std/"
cp -R "$PROJECT_DIR/yazabase/." "$YAZA_STAGE/yazabase/"
copy_template_tree "$PROJECT_DIR/templates" "$YAZA_STAGE/templates"
cp -R "$PROJECT_DIR/bindings/." "$YAZA_STAGE/bindings/"
cp "$PROJECT_DIR/README.md" "$YAZA_STAGE/"
cp "$PROJECT_DIR/VERSION" "$YAZA_STAGE/"
chmod 755 "$YAZA_STAGE/bin/"*
chmod 755 "$YAZA_STAGE/scripts/mobile/"*.sh

YB_STAGE_BIN="$YB_STAGE/bin"
mkdir -p "$YB_STAGE_BIN" "$YB_STAGE/lib" "$YB_STAGE/include" "$YB_STAGE/bindings"
cp "$BUILD_DIR/yazabase-cli" "$YB_STAGE_BIN/yazabase"
cp "$BUILD_DIR/libyazabase.$LIB_SUFFIX" "$YB_STAGE/lib/"
cp "$PROJECT_DIR/runtime/include/yazabase.h" "$YB_STAGE/include/"
cp -R "$PROJECT_DIR/bindings/." "$YB_STAGE/bindings/"
cp "$PROJECT_DIR/scripts/install-yazabase.sh" "$YB_STAGE/install.sh"
chmod 755 "$YB_STAGE_BIN/yazabase" "$YB_STAGE/install.sh"

tar -czf "$DIST_DIR/yaza-$VERSION-$PLATFORM.tar.gz" -C "$DIST_DIR" "yaza-$VERSION-$PLATFORM"
tar -czf "$DIST_DIR/yazabase-$VERSION-$PLATFORM.tar.gz" -C "$DIST_DIR" "yazabase-$VERSION-$PLATFORM"
rm -rf "$YAZA_STAGE" "$YB_STAGE"

file "$BUILD_DIR/yaza-llvm" "$BUILD_DIR/yazabase-cli"
printf 'Packaged %s release artifacts in %s\n' "$PLATFORM" "$DIST_DIR"

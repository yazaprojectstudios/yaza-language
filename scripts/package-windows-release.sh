#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
BUILD_DIR=${YAZA_WINDOWS_BUILD_DIR:-"$PROJECT_DIR/build/win64"}
DIST_DIR=${YAZA_DIST_DIR:-"$PROJECT_DIR/dist"}
VERSION=${YAZA_VERSION:-$(cat "$PROJECT_DIR/VERSION")}
PLATFORM=windows-x64
ARGON2_ROOT=${YAZA_WINDOWS_ARGON2_ROOT:-${YAZA_ARGON2_ROOT:-/opt/yaza-mingw-argon2}}
CC_BIN=${YAZA_WINDOWS_CC:-clang}

for tool in xz zip; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    printf 'error: required packaging tool not found: %s\n' "$tool" >&2
    exit 127
  fi
done

mkdir -p "$BUILD_DIR" "$DIST_DIR"

for req in \
  "$PROJECT_DIR/bootstrap/llvm_cli.x86_64-w64-mingw32.ll.xz" \
  "$PROJECT_DIR/bootstrap/yazabase_cli.x86_64-w64-mingw32.ll.xz"; do
  if [ ! -f "$req" ]; then
    printf 'error: missing Windows bootstrap IR: %s\n' "$req" >&2
    exit 1
  fi
done

if [ ! -f "$ARGON2_ROOT/include/argon2.h" ] || [ ! -f "$ARGON2_ROOT/lib/libargon2.a" ]; then
  printf 'error: Windows static Argon2 archive not found under %s\n' "$ARGON2_ROOT" >&2
  exit 1
fi

printf '==> Cross-compiling Windows runtime v1 & DLL...\n'
"$CC_BIN" --target=x86_64-w64-mingw32 -std=c11 -O2 -pthread \
  -DYAZA_RT_V1_WITH_ARGON2=1 \
  -I"$PROJECT_DIR/runtime/include" -I"$ARGON2_ROOT/include" \
  -c "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -o "$BUILD_DIR/yaza_runtime_v1.o"

"$CC_BIN" --target=x86_64-w64-mingw32 -std=c11 -O2 -Wall -Wextra -Werror \
  -DYAZABASE_BUILD_DLL -shared \
  -I"$PROJECT_DIR/runtime/include" \
  "$PROJECT_DIR/runtime/src/yazabase_c_api.c" \
  -static -lm \
  -o "$BUILD_DIR/libyazabase.dll"

printf '==> Linking yaza-llvm.exe from Windows bootstrap IR...\n'
xz -dc "$PROJECT_DIR/bootstrap/llvm_cli.x86_64-w64-mingw32.ll.xz" > "$BUILD_DIR/llvm_cli.ll"
"$CC_BIN" --target=x86_64-w64-mingw32 -Wno-override-module -O2 \
  -x ir "$BUILD_DIR/llvm_cli.ll" \
  -x none "$BUILD_DIR/yaza_runtime_v1.o" "$ARGON2_ROOT/lib/libargon2.a" \
  -static -pthread -lws2_32 -lbcrypt -lm \
  -o "$BUILD_DIR/yaza-llvm.exe"
rm -f "$BUILD_DIR/llvm_cli.ll"

printf '==> Linking yazabase-cli.exe from Windows bootstrap IR...\n'
xz -dc "$PROJECT_DIR/bootstrap/yazabase_cli.x86_64-w64-mingw32.ll.xz" > "$BUILD_DIR/yazabase_cli.ll"
"$CC_BIN" --target=x86_64-w64-mingw32 -Wno-override-module -O2 \
  -x ir "$BUILD_DIR/yazabase_cli.ll" \
  -x none "$BUILD_DIR/yaza_runtime_v1.o" "$ARGON2_ROOT/lib/libargon2.a" \
  -static -pthread -lws2_32 -lbcrypt -lm \
  -o "$BUILD_DIR/yazabase-cli.exe"
rm -f "$BUILD_DIR/yazabase_cli.ll"

YAZA_STAGE="$DIST_DIR/yaza-$VERSION-$PLATFORM"
YB_STAGE="$DIST_DIR/yazabase-$VERSION-$PLATFORM"
rm -rf "$YAZA_STAGE" "$YB_STAGE"

mkdir -p "$YAZA_STAGE/bin" "$YAZA_STAGE/runtime/src" \
  "$YAZA_STAGE/runtime/include" "$YAZA_STAGE/std" "$YAZA_STAGE/templates" \
  "$YAZA_STAGE/include" "$YAZA_STAGE/lib" "$YAZA_STAGE/bindings" \
  "$YAZA_STAGE/share/yaza/providers/argon2/include" \
  "$YAZA_STAGE/share/yaza/providers/argon2/lib"

cp "$BUILD_DIR/yaza-llvm.exe" "$YAZA_STAGE/bin/yaza.exe"
cp "$BUILD_DIR/yaza-llvm.exe" "$YAZA_STAGE/bin/yaza-llvm.exe"
cp "$BUILD_DIR/yazabase-cli.exe" "$YAZA_STAGE/bin/yazabase.exe"
cp "$BUILD_DIR/yazabase-cli.exe" "$YAZA_STAGE/bin/yazabase-cli.exe"
cp "$BUILD_DIR/libyazabase.dll" "$YAZA_STAGE/lib/"
cp "$PROJECT_DIR/runtime/include/yazabase.h" "$YAZA_STAGE/include/"
cp "$PROJECT_DIR/runtime/include/yaza_runtime_v1.h" "$YAZA_STAGE/runtime/include/"
cp "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" "$YAZA_STAGE/runtime/src/"
cp "$PROJECT_DIR/runtime/src/yaza_transport_curl_v1.inc" "$YAZA_STAGE/runtime/src/"
cp "$PROJECT_DIR/runtime/src/yaza_security_login_v1.inc" "$YAZA_STAGE/runtime/src/"
cp "$ARGON2_ROOT/include/argon2.h" "$YAZA_STAGE/share/yaza/providers/argon2/include/"
cp "$ARGON2_ROOT/lib/libargon2.a" "$YAZA_STAGE/share/yaza/providers/argon2/lib/"
cp -R "$PROJECT_DIR/std/." "$YAZA_STAGE/std/"
cp -R "$PROJECT_DIR/templates/." "$YAZA_STAGE/templates/"
cp -R "$PROJECT_DIR/bindings/." "$YAZA_STAGE/bindings/"
cp "$PROJECT_DIR/README.md" "$YAZA_STAGE/"
cp "$PROJECT_DIR/VERSION" "$YAZA_STAGE/"

mkdir -p "$YB_STAGE/bin" "$YB_STAGE/lib" "$YB_STAGE/include" "$YB_STAGE/bindings"
cp "$BUILD_DIR/yazabase-cli.exe" "$YB_STAGE/bin/yazabase.exe"
cp "$BUILD_DIR/libyazabase.dll" "$YB_STAGE/lib/"
cp "$PROJECT_DIR/runtime/include/yazabase.h" "$YB_STAGE/include/"
cp -R "$PROJECT_DIR/bindings/." "$YB_STAGE/bindings/"
cp "$PROJECT_DIR/README.md" "$YB_STAGE/"
cp "$PROJECT_DIR/VERSION" "$YB_STAGE/"

rm -f "$DIST_DIR/yaza-$VERSION-$PLATFORM.zip" "$DIST_DIR/yazabase-$VERSION-$PLATFORM.zip"
(cd "$DIST_DIR" && zip -r -q "yaza-$VERSION-$PLATFORM.zip" "yaza-$VERSION-$PLATFORM")
(cd "$DIST_DIR" && zip -r -q "yazabase-$VERSION-$PLATFORM.zip" "yazabase-$VERSION-$PLATFORM")
rm -rf "$YAZA_STAGE" "$YB_STAGE"

if command -v file >/dev/null 2>&1; then
  file "$BUILD_DIR/yaza-llvm.exe" "$BUILD_DIR/yazabase-cli.exe" "$BUILD_DIR/libyazabase.dll"
fi
printf 'Packaged %s release artifacts in %s\n' "$PLATFORM" "$DIST_DIR"

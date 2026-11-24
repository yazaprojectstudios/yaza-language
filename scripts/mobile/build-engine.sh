#!/bin/sh
set -eu

if [ "$#" -lt 2 ]; then
  printf 'Usage: %s <compiler> <android-arm64|ios-arm64|ios-simulator-arm64> [source.yz]\n' "$0" >&2
  exit 2
fi

YAZA_BIN=$1
TARGET_NAME=$2
SOURCE=src/mobile_engine.yz
if [ "$#" -ge 3 ]; then SOURCE=$3; fi
if [ ! -x "$YAZA_BIN" ]; then
  printf 'error: YAZA compiler is not executable: %s\n' "$YAZA_BIN" >&2
  exit 1
fi
if [ ! -f "$SOURCE" ]; then
  printf 'error: mobile engine source was not found: %s\n' "$SOURCE" >&2
  exit 1
fi

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_ROOT=$(pwd -P)
RUNTIME_DIR=$(printenv YAZA_RUNTIME_DIR || true)
if [ -z "$RUNTIME_DIR" ]; then RUNTIME_DIR="$SCRIPT_DIR/../../runtime"; fi
STDLIB_DIR=$(printenv YAZA_STDLIB_DIR || true)
if [ -z "$STDLIB_DIR" ]; then STDLIB_DIR="$RUNTIME_DIR/../std"; fi
BRIDGE_SOURCE="$RUNTIME_DIR/mobile/src/yaza_mobile_v1.c"
BRIDGE_INCLUDE="$RUNTIME_DIR/mobile/include"
RUNTIME_SOURCE="$RUNTIME_DIR/src/yaza_runtime_v1.c"
if [ ! -f "$BRIDGE_SOURCE" ] || [ ! -f "$RUNTIME_SOURCE" ] || [ ! -f "$RUNTIME_DIR/include/yaza_runtime_v1.h" ]; then
  printf 'error: mobile bridge/runtime sources are missing under %s\n' "$RUNTIME_DIR" >&2
  exit 1
fi
if [ ! -d "$STDLIB_DIR" ]; then
  printf 'error: YAZA standard library is missing under %s\n' "$STDLIB_DIR" >&2
  exit 1
fi
export YAZA_RUNTIME_DIR="$RUNTIME_DIR"
export YAZA_STDLIB_DIR="$STDLIB_DIR"

OUT_DIR="$PROJECT_ROOT/build/mobile/$TARGET_NAME"
mkdir -p "$OUT_DIR/include"
cp "$BRIDGE_INCLUDE/yaza_mobile_v1.h" "$OUT_DIR/include/yaza_mobile_v1.h"
IR="$OUT_DIR/mobile_engine.ll"
BUILD_MODE=$(printenv YAZA_MOBILE_BUILD_MODE || true)
if [ -z "$BUILD_MODE" ]; then BUILD_MODE=release; fi
case "$BUILD_MODE" in
  release|Release) BUILD_MODE=release; OPT_FLAGS=-O2 ;;
  debug|Debug) BUILD_MODE=debug; OPT_FLAGS=-O0 ;;
  *) printf 'error: YAZA_MOBILE_BUILD_MODE must be release or debug\n' >&2; exit 2 ;;
esac
MODE_STAMP="$OUT_DIR/.build-mode"
PREV_MODE=
if [ -f "$MODE_STAMP" ]; then PREV_MODE=$(cat "$MODE_STAMP" || true); fi
MODE_CHANGED=0
if [ "$PREV_MODE" != "$BUILD_MODE" ]; then MODE_CHANGED=1; fi
case "$TARGET_NAME" in
  ios-arm64)
    SDK=iphoneos
    MIN_VERSION=$(printenv YAZA_IOS_MIN_VERSION || true)
    if [ -z "$MIN_VERSION" ]; then MIN_VERSION=16.0; fi
    TRIPLE="arm64-apple-ios$MIN_VERSION"
    CLANG=$(xcrun --sdk "$SDK" --find clang)
    SDKROOT=$(xcrun --sdk "$SDK" --show-sdk-path)
    AR=$(xcrun --sdk "$SDK" --find ar)
    IS_IOS=1
    IS_ANDROID=0
    ;;
  ios-simulator-arm64)
    SDK=iphonesimulator
    MIN_VERSION=$(printenv YAZA_IOS_MIN_VERSION || true)
    if [ -z "$MIN_VERSION" ]; then MIN_VERSION=16.0; fi
    TRIPLE="arm64-apple-ios$MIN_VERSION-simulator"
    CLANG=$(xcrun --sdk "$SDK" --find clang)
    SDKROOT=$(xcrun --sdk "$SDK" --show-sdk-path)
    AR=$(xcrun --sdk "$SDK" --find ar)
    IS_IOS=1
    IS_ANDROID=0
    ;;
  android-arm64)
    NDK=$(printenv ANDROID_NDK_HOME || true)
    if [ -z "$NDK" ]; then NDK=$(printenv ANDROID_NDK_ROOT || true); fi
    ANDROID_SDK=$(printenv ANDROID_HOME || true)
    if [ -z "$ANDROID_SDK" ]; then ANDROID_SDK=$(printenv ANDROID_SDK_ROOT || true); fi
    if [ -z "$ANDROID_SDK" ]; then
      case "$(uname -s)" in
        Darwin) ANDROID_SDK="${HOME:-}/Library/Android/sdk" ;;
        Linux) ANDROID_SDK="${HOME:-}/Android/Sdk" ;;
      esac
    fi
    if { [ -z "$NDK" ] || [ ! -d "$NDK/toolchains/llvm/prebuilt" ]; } && [ -d "$ANDROID_SDK/ndk" ]; then
      NDK=
      for candidate in "$ANDROID_SDK"/ndk/*; do
        [ -d "$candidate" ] || continue
        if [ -d "$candidate/toolchains/llvm/prebuilt" ]; then NDK=$candidate; fi
      done
    fi
    if { [ -z "$NDK" ] || [ ! -d "$NDK/toolchains/llvm/prebuilt" ]; } && [ -d "$ANDROID_SDK/ndk-bundle/toolchains/llvm/prebuilt" ]; then NDK="$ANDROID_SDK/ndk-bundle"; fi
    if [ -z "$NDK" ] || [ ! -d "$NDK/toolchains/llvm/prebuilt" ]; then
      printf 'error: install an Android NDK or set ANDROID_NDK_HOME\n' >&2
      exit 1
    fi
    API=$(printenv ANDROID_API_LEVEL || true)
    if [ -z "$API" ]; then API=24; fi
    case "$API" in *[!0-9]*|'') printf 'error: ANDROID_API_LEVEL must be an integer\n' >&2; exit 2 ;; esac
    case "$(uname -s)/$(uname -m)" in
      Darwin/arm64)
        if [ -d "$NDK/toolchains/llvm/prebuilt/darwin-arm64" ]; then
          HOST_TAG=darwin-arm64
        else
          HOST_TAG=darwin-x86_64
        fi
        ;;
      Darwin/x86_64) HOST_TAG=darwin-x86_64 ;;
      Linux/x86_64) HOST_TAG=linux-x86_64 ;;
      Linux/aarch64|Linux/arm64) HOST_TAG=linux-aarch64 ;;
      *) printf 'error: unsupported Android NDK host %s/%s\n' "$(uname -s)" "$(uname -m)" >&2; exit 1 ;;
    esac
    TOOLCHAIN="$NDK/toolchains/llvm/prebuilt/$HOST_TAG"
    CLANG="$TOOLCHAIN/bin/aarch64-linux-android$API-clang"
    AR="$TOOLCHAIN/bin/llvm-ar"
    if [ ! -x "$CLANG" ] || [ ! -x "$AR" ]; then
      printf 'error: Android NDK toolchain is incomplete for host %s\n' "$HOST_TAG" >&2
      exit 1
    fi
    TRIPLE="aarch64-linux-android$API"
    SDKROOT=
    IS_IOS=0
    IS_ANDROID=1
    ;;
  *)
    printf 'error: unsupported mobile target: %s\n' "$TARGET_NAME" >&2
    exit 2
    ;;
esac

printf 'YAZA Mobile | target %s\n' "$TARGET_NAME"
printf '  Compiler : %s\n' "$YAZA_BIN"
printf '  Source   : %s\n' "$SOURCE"
printf '  Output   : %s\n' "$OUT_DIR"
"$YAZA_BIN" emit-llvm-module --target "$TRIPLE" "$SOURCE" --export-request mobile_reduce -o "$IR"

compile_object() {
  input=$1
  output=$2
  case "$input" in
    *.ll)
      if [ "$IS_IOS" = 1 ]; then
        "$CLANG" --target="$TRIPLE" -isysroot "$SDKROOT" -Wno-override-module \
          "$OPT_FLAGS" -fPIC -c "$input" -o "$output"
      else
        "$CLANG" --target="$TRIPLE" -Wno-override-module "$OPT_FLAGS" \
          -fPIC -c "$input" -o "$output"
      fi
      ;;
    *)
      if [ "$IS_IOS" = 1 ]; then
        "$CLANG" --target="$TRIPLE" -isysroot "$SDKROOT" -std=c11 -DYAZA_RT_V1_DISABLE_PROCESS_SPAWN=1 "$OPT_FLAGS" -fPIC -pthread \
          -I"$RUNTIME_DIR/include" -I"$BRIDGE_INCLUDE" -c "$input" -o "$output"
      else
        "$CLANG" --target="$TRIPLE" -std=c11 -DYAZA_RT_V1_DISABLE_PROCESS_SPAWN=1 "$OPT_FLAGS" -fPIC -pthread \
          -I"$RUNTIME_DIR/include" -I"$BRIDGE_INCLUDE" -c "$input" -o "$output"
      fi
      ;;
  esac
}
needs_c_rebuild() {
  src=$1
  obj=$2
  if [ "$MODE_CHANGED" = 1 ] || [ ! -f "$obj" ] || [ "$src" -nt "$obj" ]; then
    return 0
  fi
  if [ "$RUNTIME_DIR/include/yaza_runtime_v1.h" -nt "$obj" ] || [ "$BRIDGE_INCLUDE/yaza_mobile_v1.h" -nt "$obj" ]; then
    return 0
  fi
  return 1
}

compile_object "$IR" "$OUT_DIR/mobile_engine.o"
if needs_c_rebuild "$RUNTIME_SOURCE" "$OUT_DIR/yaza_runtime_v1.o"; then
  compile_object "$RUNTIME_SOURCE" "$OUT_DIR/yaza_runtime_v1.o"
fi
if needs_c_rebuild "$BRIDGE_SOURCE" "$OUT_DIR/yaza_mobile_v1.o"; then
  compile_object "$BRIDGE_SOURCE" "$OUT_DIR/yaza_mobile_v1.o"
fi

if [ "$IS_ANDROID" = 1 ]; then
  JNI_SOURCE="$PROJECT_ROOT/mobile/android/app/src/main/cpp/yaza_mobile_jni.c"
  if [ ! -f "$JNI_SOURCE" ]; then
    printf 'error: Android JNI adapter is missing: %s\n' "$JNI_SOURCE" >&2
    exit 1
  fi
  if needs_c_rebuild "$JNI_SOURCE" "$OUT_DIR/yaza_mobile_jni.o"; then
    compile_object "$JNI_SOURCE" "$OUT_DIR/yaza_mobile_jni.o"
  fi
  "$CLANG" --target="$TRIPLE" "$OPT_FLAGS" -shared -fPIC -pthread -Wl,--no-undefined \
    "$OUT_DIR/mobile_engine.o" "$OUT_DIR/yaza_runtime_v1.o" \
    "$OUT_DIR/yaza_mobile_v1.o" "$OUT_DIR/yaza_mobile_jni.o" -lm -ldl \
    -o "$OUT_DIR/libyaza_mobile.so"
  if [ "$BUILD_MODE" = debug ]; then
    "$CLANG" --target="$TRIPLE" "$OPT_FLAGS" -shared -fPIC -pthread -Wl,--no-undefined \
      "$OUT_DIR/mobile_engine.o" "$OUT_DIR/yaza_runtime_v1.o" \
      "$OUT_DIR/yaza_mobile_v1.o" -lm \
      -o "$OUT_DIR/libyaza_mobile_hotswap.so"
  fi
  mkdir -p "$PROJECT_ROOT/mobile/android/app/build/generated/yazaJniLibs/arm64-v8a"
  cp "$OUT_DIR/libyaza_mobile.so" "$PROJECT_ROOT/mobile/android/app/build/generated/yazaJniLibs/arm64-v8a/libyaza_mobile.so"
else
  "$AR" rcs "$OUT_DIR/libyaza_mobile.a" "$OUT_DIR/mobile_engine.o" \
    "$OUT_DIR/yaza_runtime_v1.o" "$OUT_DIR/yaza_mobile_v1.o"
  if [ "$BUILD_MODE" = debug ] && [ "$TARGET_NAME" = ios-simulator-arm64 ]; then
    "$CLANG" --target="$TRIPLE" -isysroot "$SDKROOT" "$OPT_FLAGS" -dynamiclib -fPIC -pthread \
      -install_name "@rpath/libyaza_mobile_hotswap.dylib" \
      "$OUT_DIR/mobile_engine.o" "$OUT_DIR/yaza_runtime_v1.o" \
      "$OUT_DIR/yaza_mobile_v1.o" -lm \
      -o "$OUT_DIR/libyaza_mobile_hotswap.dylib"
    if command -v codesign >/dev/null 2>&1; then
      codesign --force --sign - "$OUT_DIR/libyaza_mobile_hotswap.dylib" >/dev/null 2>&1 || true
    fi
  fi
fi
printf '%s\n' "$BUILD_MODE" > "$MODE_STAMP"

printf 'Built '
if [ "$IS_ANDROID" = 1 ]; then
  printf '%s\n' "$OUT_DIR/libyaza_mobile.so"
else
  printf '%s\n' "$OUT_DIR/libyaza_mobile.a"
fi


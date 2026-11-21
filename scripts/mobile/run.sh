#!/bin/sh
set -eu

if [ "$#" -lt 2 ]; then
  printf 'Usage: %s <compiler> <android-emulator|ios-simulator>\n' "$0" >&2
  exit 2
fi
YAZA_BIN=$1
TARGET=$2
PROJECT_ROOT=$(pwd -P)
if [ ! -x "$YAZA_BIN" ]; then
  printf 'error: YAZA compiler is not executable: %s\n' "$YAZA_BIN" >&2
  exit 1
fi
export YAZA_BIN
export YAZA_MOBILE_BUILD_MODE=debug

case "$TARGET" in
  android-emulator)
    GRADLE_BIN=$(printenv YAZA_GRADLE_BIN || true)
    if [ -n "$GRADLE_BIN" ] && [ ! -x "$GRADLE_BIN" ]; then GRADLE_BIN=; fi
    if [ -z "$GRADLE_BIN" ] && [ -n "${HOME:-}" ]; then
      for candidate in "$HOME"/.gradle/wrapper/dists/gradle-8.13-bin/*/gradle-8.13/bin/gradle; do
        [ -x "$candidate" ] || continue
        GRADLE_BIN=$candidate
        break
      done
    fi
    if [ -z "$GRADLE_BIN" ]; then GRADLE_BIN=$(command -v gradle 2>/dev/null || true); fi
    if [ -z "$GRADLE_BIN" ]; then
      printf 'error: Gradle is required; install it or set YAZA_GRADLE_BIN\n' >&2
      exit 1
    fi
    # Gradle 8.13 + AGP 8.13 require JDK 17-23; pick a compatible JDK if JAVA_HOME is unset or >= 24.
    PICKED_JAVA_HOME=
    if [ -n "${JAVA_HOME:-}" ] && [ -x "$JAVA_HOME/bin/java" ]; then
      JAVA_VER=$("$JAVA_HOME/bin/java" -version 2>&1 | sed -n 's/.*version "\([^"]*\)".*/\1/p' | head -1)
      JAVA_MAJ=$(printf '%s\n' "$JAVA_VER" | cut -d. -f1)
      case "$JAVA_MAJ" in
        1[789]|2[0-3]) PICKED_JAVA_HOME="$JAVA_HOME" ;;
      esac
    fi
    if [ -z "$PICKED_JAVA_HOME" ] && command -v /usr/libexec/java_home >/dev/null 2>&1; then
      for v in 21 17; do
        CAND=$(/usr/libexec/java_home -v "$v" 2>/dev/null || true)
        if [ -n "$CAND" ] && [ -x "$CAND/bin/java" ]; then
          PICKED_JAVA_HOME="$CAND"
          break
        fi
      done
    fi
    if [ -z "$PICKED_JAVA_HOME" ]; then
      for cand in "$HOME/Library/Java/JavaVirtualMachines"/jbr-17*/Contents/Home \
                  "/Applications/Android Studio.app/Contents/jbr/Contents/Home" \
                  /opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home \
                  /opt/homebrew/opt/openjdk@17/libexec/openjdk.jdk/Contents/Home; do
        if [ -x "$cand/bin/java" ]; then
          PICKED_JAVA_HOME="$cand"
          break
        fi
      done
    fi
    if [ -n "$PICKED_JAVA_HOME" ]; then
      export JAVA_HOME="$PICKED_JAVA_HOME"
    fi
    ANDROID_SDK=$(printenv ANDROID_HOME || true)
    if [ -z "$ANDROID_SDK" ]; then ANDROID_SDK=$(printenv ANDROID_SDK_ROOT || true); fi
    if [ -z "$ANDROID_SDK" ]; then
      case "$(uname -s)" in
        Darwin) ANDROID_SDK="${HOME:-}/Library/Android/sdk" ;;
        Linux) ANDROID_SDK="${HOME:-}/Android/Sdk" ;;
      esac
    fi
    if [ -n "$ANDROID_SDK" ] && [ -d "$ANDROID_SDK" ]; then
      export ANDROID_HOME="$ANDROID_SDK"
    fi
    ADB_BIN=$(command -v adb 2>/dev/null || true)
    if [ -z "$ADB_BIN" ] && [ -x "$ANDROID_SDK/platform-tools/adb" ]; then ADB_BIN="$ANDROID_SDK/platform-tools/adb"; fi
    if [ -z "$ADB_BIN" ]; then
      printf 'error: Android platform-tools (adb) are required\n' >&2
      exit 1
    fi
    if ! "$ADB_BIN" devices 2>/dev/null | grep -q '[[:space:]]device$'; then
      printf 'error: no online Android device or emulator is connected\n' >&2
      exit 1
    fi
    (cd "$PROJECT_ROOT/mobile/android" && "$GRADLE_BIN" installDebug)
    "$ADB_BIN" shell monkey -p dev.yaza.mobile 1
    ;;
  ios-simulator)
    if ! command -v xcodebuild >/dev/null 2>&1 || ! command -v xcrun >/dev/null 2>&1; then
      printf 'error: Xcode command-line tools are required to run the iOS starter\n' >&2
      exit 1
    fi
    if ! xcrun simctl list devices booted 2>/dev/null | grep -q 'Booted'; then
      printf 'error: boot an iOS simulator in Xcode or Simulator, then retry\n' >&2
      exit 1
    fi
    DERIVED="$PROJECT_ROOT/build/mobile/ios-derived"
    xcodebuild -project "$PROJECT_ROOT/mobile/ios/YazaMobile.xcodeproj" \
      -scheme YazaMobile -configuration Debug -sdk iphonesimulator \
      -destination 'generic/platform=iOS Simulator' -derivedDataPath "$DERIVED" \
      CODE_SIGNING_ALLOWED=NO build
    APP="$DERIVED/Build/Products/Debug-iphonesimulator/YazaMobile.app"
    xcrun simctl install booted "$APP"
    xcrun simctl launch booted dev.yaza.mobile
    ;;
  *)
    printf 'error: unsupported mobile run target: %s\n' "$TARGET" >&2
    exit 2
    ;;
esac


#!/bin/sh
set -eu

if [ "$#" -lt 2 ]; then
  printf 'Usage: %s <compiler> <android|ios|auto|android-emulator|ios-simulator>\n' "$0" >&2
  exit 2
fi
YAZA_BIN=$1
TARGET=$2

if [ ! -x "$YAZA_BIN" ]; then
  printf 'error: YAZA compiler is not executable: %s\n' "$YAZA_BIN" >&2
  exit 1
fi
if ! command -v python3 >/dev/null 2>&1; then
  printf 'error: python3 is required for yaza mobile run\n' >&2
  exit 1
fi

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
export YAZA_BIN
export YAZA_MOBILE_BUILD_MODE=${YAZA_MOBILE_BUILD_MODE:-debug}

# Discover Gradle (if available)
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
if [ -n "$GRADLE_BIN" ]; then export YAZA_GRADLE_BIN="$GRADLE_BIN"; fi

# Prefer JDK 17-23 for Gradle 8.13 + AGP 8.13 compatibility.
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

# Discover Android SDK / adb / emulator paths when present.
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
ADB_BIN=$(printenv YAZA_ADB_BIN || true)
if [ -z "$ADB_BIN" ]; then ADB_BIN=$(command -v adb 2>/dev/null || true); fi
if [ -z "$ADB_BIN" ] && [ -n "$ANDROID_SDK" ] && [ -x "$ANDROID_SDK/platform-tools/adb" ]; then
  ADB_BIN="$ANDROID_SDK/platform-tools/adb"
fi
if [ -n "$ADB_BIN" ]; then export YAZA_ADB_BIN="$ADB_BIN"; fi

EMULATOR_BIN=$(printenv YAZA_EMULATOR_BIN || true)
if [ -z "$EMULATOR_BIN" ]; then EMULATOR_BIN=$(command -v emulator 2>/dev/null || true); fi
if [ -z "$EMULATOR_BIN" ] && [ -n "$ANDROID_SDK" ] && [ -x "$ANDROID_SDK/emulator/emulator" ]; then
  EMULATOR_BIN="$ANDROID_SDK/emulator/emulator"
fi
if [ -n "$EMULATOR_BIN" ]; then export YAZA_EMULATOR_BIN="$EMULATOR_BIN"; fi

exec python3 "$SCRIPT_DIR/dev_runner.py" "$YAZA_BIN" "$TARGET"

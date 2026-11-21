#!/bin/sh
set -u

YAZA_BIN=yaza
if [ "$#" -ge 1 ] && [ -n "$1" ]; then YAZA_BIN=$1; fi
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
printf 'YAZA Mobile toolchain doctor\n'
printf '  Compiler: '
if command -v "$YAZA_BIN" >/dev/null 2>&1; then
  "$YAZA_BIN" --version 2>/dev/null || printf 'found (%s)\n' "$YAZA_BIN"
else
  printf 'missing (%s)\n' "$YAZA_BIN"
fi
printf '\nAndroid ARM64\n'
if [ -n "$ANDROID_SDK" ] && [ -d "$ANDROID_SDK" ]; then printf '  Android SDK: found at %s\n' "$ANDROID_SDK"; else printf '  Android SDK: missing; set ANDROID_HOME or install the default SDK\n'; fi
if [ -n "$NDK" ] && [ -d "$NDK/toolchains/llvm/prebuilt" ]; then
  printf '  NDK: found at %s\n' "$NDK"
  case "$(uname -s)/$(uname -m)" in
    Darwin/arm64)
      if [ -d "$NDK/toolchains/llvm/prebuilt/darwin-arm64" ]; then HOST_TAG=darwin-arm64; else HOST_TAG=darwin-x86_64; fi
      ;;
    Darwin/x86_64) HOST_TAG=darwin-x86_64 ;;
    Linux/x86_64) HOST_TAG=linux-x86_64 ;;
    Linux/aarch64|Linux/arm64) HOST_TAG=linux-aarch64 ;;
    *) HOST_TAG=unsupported ;;
  esac
  if [ -d "$NDK/toolchains/llvm/prebuilt/$HOST_TAG" ]; then printf '  NDK host toolchain: %s\n' "$HOST_TAG"; else printf '  NDK host toolchain: missing for %s/%s\n' "$(uname -s)" "$(uname -m)"; fi
else
  printf '  NDK: missing; install it with Android Studio or set ANDROID_NDK_HOME\n'
fi
JAVA_BIN=
if [ -n "${JAVA_HOME:-}" ] && [ -x "$JAVA_HOME/bin/java" ]; then
  JAVA_BIN="$JAVA_HOME/bin/java"
elif command -v /usr/libexec/java_home >/dev/null 2>&1; then
  for v in 21 17; do
    CAND=$(/usr/libexec/java_home -v "$v" 2>/dev/null || true)
    if [ -n "$CAND" ] && [ -x "$CAND/bin/java" ]; then
      JAVA_BIN="$CAND/bin/java"
      break
    fi
  done
fi
if [ -z "$JAVA_BIN" ]; then
  for candidate in /opt/homebrew/opt/openjdk@21/bin/java /opt/homebrew/opt/openjdk@17/bin/java /opt/homebrew/opt/openjdk/bin/java /usr/local/opt/openjdk/bin/java; do
    if [ -x "$candidate" ]; then
      JAVA_BIN=$candidate
      break
    fi
  done
  if [ -z "$JAVA_BIN" ] && command -v java >/dev/null 2>&1; then
    JAVA_BIN=$(command -v java)
  fi
fi
if [ -n "$JAVA_BIN" ]; then
  JAVA_VERSION=$("$JAVA_BIN" -version 2>&1 | sed -n 's/.*version "\([^"]*\)".*/\1/p' | head -1)
  case "$JAVA_VERSION" in
    1.*) JAVA_MAJOR=$(printf '%s\n' "$JAVA_VERSION" | cut -d. -f2) ;;
    *) JAVA_MAJOR=$(printf '%s\n' "$JAVA_VERSION" | cut -d. -f1) ;;
  esac
  case "$JAVA_MAJOR" in
    *[!0-9]*|'') printf '  Java: found (%s), version unavailable (Android build requires JDK 17+)\n' "$JAVA_BIN" ;;
    *)
      if [ "$JAVA_MAJOR" -ge 17 ]; then
        printf '  Java: JDK %s found\n' "$JAVA_MAJOR"
      else
        printf '  Java: JDK %s found; Android build requires JDK 17+\n' "$JAVA_MAJOR"
      fi
      ;;
  esac
else
  printf '  Java: missing; Android app packaging requires JDK 17+\n'
fi
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
if [ -n "$GRADLE_BIN" ]; then
  case "$GRADLE_BIN" in
    */gradle-8.13/bin/gradle) printf '  Gradle: 8.13 cached\n' ;;
    *) printf '  Gradle: available at %s (template requires 8.13+)\n' "$GRADLE_BIN" ;;
  esac
else
  printf '  Gradle: missing; install Gradle or set YAZA_GRADLE_BIN\n'
fi
ADB_BIN=$(command -v adb 2>/dev/null || true)
if [ -z "$ADB_BIN" ] && [ -x "$ANDROID_SDK/platform-tools/adb" ]; then ADB_BIN="$ANDROID_SDK/platform-tools/adb"; fi
if [ -n "$ADB_BIN" ]; then
  printf '  adb: found\n'
  if "$ADB_BIN" devices 2>/dev/null | grep -q '[[:space:]]device$'; then printf '  Android device: connected\n'; else printf '  Android device: none connected\n'; fi
else
  printf '  adb: missing\n'
fi
printf '\niOS ARM64\n'
if command -v xcrun >/dev/null 2>&1 && xcrun --sdk iphoneos --show-sdk-path >/dev/null 2>&1; then
  printf '  Xcode iOS SDK: found (%s)\n' "$(xcrun --sdk iphoneos --show-sdk-version)"
else
  printf '  Xcode iOS SDK: missing\n'
fi
if command -v xcodebuild >/dev/null 2>&1; then printf '  Xcode build tools: found\n'; else printf '  Xcode build tools: missing\n'; fi
if command -v xcrun >/dev/null 2>&1; then
  SIMULATORS=$(xcrun simctl list devices available 2>/dev/null || true)
  if printf '%s\n' "$SIMULATORS" | grep -q 'Booted'; then
    printf '  iOS simulator: booted\n'
  elif printf '%s\n' "$SIMULATORS" | grep -q 'iPhone'; then
    printf '  iOS simulator: runtime installed, no booted device\n'
  else
    printf '  iOS simulator: runtime not installed\n'
  fi
fi
printf '\nInstall missing platform tools before building or running a device app.\n'


#!/bin/sh
set -eu

PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
YAZA_BIN=${YAZA_BIN:-"$PROJECT_DIR/build/yaza-llvm"}
if [ ! -x "$YAZA_BIN" ]; then
  printf 'Missing executable compiler: %s\n' "$YAZA_BIN" >&2
  exit 1
fi
WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/yaza-mobile-engine.XXXXXX")
trap 'rm -rf "$WORK_DIR"' EXIT HUP INT TERM
export YAZA_RUNTIME_DIR="$PROJECT_DIR/runtime"
export YAZA_STDLIB_DIR="$PROJECT_DIR/std"

"$YAZA_BIN" emit-llvm-module "$PROJECT_DIR/templates/mobile/src/mobile_engine.yz" --export-request mobile_reduce -o "$WORK_DIR/mobile_engine.ll"

ENGINE_LL="$WORK_DIR/mobile_engine.ll"
RUNTIME_C="$PROJECT_DIR/runtime/src/yaza_runtime_v1.c"
BRIDGE_C="$PROJECT_DIR/runtime/mobile/src/yaza_mobile_v1.c"
BRIDGE_TEST_C="$PROJECT_DIR/tests/mobile/yaza_mobile_v1.c"
JNI_C="$PROJECT_DIR/templates/mobile/mobile/android/app/src/main/cpp/yaza_mobile_jni.c"
JNI_TEST_C="$PROJECT_DIR/tests/mobile/yaza_mobile_jni_v1.c"

clang -std=c11 -O1 -g -Wall -Wextra -Werror -Wno-override-module -pthread \
  -DYAZA_RT_V1_DISABLE_PROCESS_SPAWN=1 \
  -I"$PROJECT_DIR/runtime/include" -I"$PROJECT_DIR/runtime/mobile/include" \
  "$ENGINE_LL" "$RUNTIME_C" "$BRIDGE_C" "$BRIDGE_TEST_C" \
  -lm -o "$WORK_DIR/mobile-bridge-test"
"$WORK_DIR/mobile-bridge-test"

clang -std=c11 -O1 -g -Wall -Wextra -Werror -Wno-override-module -pthread \
  -DYAZA_RT_V1_DISABLE_PROCESS_SPAWN=1 \
  -I"$PROJECT_DIR/runtime/include" -I"$PROJECT_DIR/runtime/mobile/include" -I"$PROJECT_DIR/tests/mobile/mock_jni" \
  "$ENGINE_LL" "$RUNTIME_C" "$BRIDGE_C" "$JNI_C" "$JNI_TEST_C" \
  -lm -ldl -o "$WORK_DIR/mobile-jni-test"
"$WORK_DIR/mobile-jni-test"

if [ "$(uname -s)" = "Linux" ]; then
  MOBILE_ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1}
else
  MOBILE_ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0}
fi

clang -std=c11 -O1 -g -Wall -Wextra -Werror -Wno-override-module -pthread \
  -DYAZA_RT_V1_DISABLE_PROCESS_SPAWN=1 \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"$PROJECT_DIR/runtime/include" -I"$PROJECT_DIR/runtime/mobile/include" \
  "$ENGINE_LL" "$RUNTIME_C" "$BRIDGE_C" "$BRIDGE_TEST_C" \
  -lm -o "$WORK_DIR/mobile-bridge-asan"
ASAN_OPTIONS="$MOBILE_ASAN_OPTIONS" "$WORK_DIR/mobile-bridge-asan"

clang -std=c11 -O1 -g -Wall -Wextra -Werror -Wno-override-module -pthread \
  -DYAZA_RT_V1_DISABLE_PROCESS_SPAWN=1 \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"$PROJECT_DIR/runtime/include" -I"$PROJECT_DIR/runtime/mobile/include" -I"$PROJECT_DIR/tests/mobile/mock_jni" \
  "$ENGINE_LL" "$RUNTIME_C" "$BRIDGE_C" "$JNI_C" "$JNI_TEST_C" \
  -lm -ldl -o "$WORK_DIR/mobile-jni-asan"
ASAN_OPTIONS="$MOBILE_ASAN_OPTIONS" "$WORK_DIR/mobile-jni-asan"

clang -std=c11 -O1 -g -Wall -Wextra -Werror -Wno-override-module -pthread \
  -DYAZA_RT_V1_DISABLE_PROCESS_SPAWN=1 \
  -fsanitize=thread -fno-omit-frame-pointer \
  -I"$PROJECT_DIR/runtime/include" -I"$PROJECT_DIR/runtime/mobile/include" \
  "$ENGINE_LL" "$RUNTIME_C" "$BRIDGE_C" "$BRIDGE_TEST_C" \
  -lm -o "$WORK_DIR/mobile-bridge-tsan"
TSAN_OPTIONS=${TSAN_OPTIONS:-halt_on_error=1} "$WORK_DIR/mobile-bridge-tsan"

if [ "$(uname -s)" = "Darwin" ] && command -v leaks >/dev/null 2>&1; then
  MallocStackLogging=1 leaks --atExit -- "$WORK_DIR/mobile-bridge-test"
  MallocStackLogging=1 leaks --atExit -- "$WORK_DIR/mobile-jni-test"
fi

#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
RUNTIME_DIR="$PROJECT_DIR/runtime"
RUNTIME_CC=${CC:-cc}
TEMP_ROOT=${TMPDIR:-/tmp}
TEMP_ROOT=${TEMP_ROOT%/}
BUILD_DIR=$(mktemp -d "$TEMP_ROOT/yazabase-vfs-v1.XXXXXX")
trap 'rm -rf "$BUILD_DIR"' EXIT HUP INT TERM

"$RUNTIME_CC" -std=c11 -Wall -Wextra -Werror -pthread \
  -DYAZA_VFS_V1_TESTING -DYAZA_VFS_V1_TEST_MAX_HANDLES=8 \
  "$RUNTIME_DIR/tests/yazabase_vfs_v1.c" "$RUNTIME_DIR/src/yaza_runtime_v1.c" \
  -I"$RUNTIME_DIR/include" -lm -o "$BUILD_DIR/yazabase_vfs_v1"
"$BUILD_DIR/yazabase_vfs_v1"

"$RUNTIME_CC" -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined \
  -DYAZA_VFS_V1_TESTING -DYAZA_VFS_V1_TEST_MAX_HANDLES=8 -fno-omit-frame-pointer "$RUNTIME_DIR/tests/yazabase_vfs_v1.c" \
  "$RUNTIME_DIR/src/yaza_runtime_v1.c" -I"$RUNTIME_DIR/include" -lm \
  -o "$BUILD_DIR/yazabase_vfs_v1-asan"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} "$BUILD_DIR/yazabase_vfs_v1-asan"

"$RUNTIME_CC" -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=thread \
  -DYAZA_VFS_V1_TESTING -DYAZA_VFS_V1_TEST_MAX_HANDLES=8 -fno-omit-frame-pointer \
  "$RUNTIME_DIR/tests/yazabase_vfs_v1.c" "$RUNTIME_DIR/src/yaza_runtime_v1.c" \
  -I"$RUNTIME_DIR/include" -lm -o "$BUILD_DIR/yazabase_vfs_v1-tsan"
TSAN_OPTIONS=${TSAN_OPTIONS:-halt_on_error=1} "$BUILD_DIR/yazabase_vfs_v1-tsan"

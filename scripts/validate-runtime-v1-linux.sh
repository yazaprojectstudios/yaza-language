#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
RUNTIME_DIR="$PROJECT_DIR/runtime"
RUNTIME_CC=${CC:-cc}
TEMP_ROOT=${TMPDIR:-/tmp}
TEMP_ROOT=${TEMP_ROOT%/}
BUILD_DIR=$(mktemp -d "$TEMP_ROOT/yaza-runtime-v1-linux.XXXXXX")
trap 'rm -rf "$BUILD_DIR"' EXIT HUP INT TERM

HOST_OS=$(uname -s)
HOST_ARCH=$(uname -m)
QUALIFICATION_LABEL="Linux runtime v1 C qualification ($HOST_ARCH)"
if [ "$HOST_OS" != Linux ]; then
  if [ "${YAZA_ALLOW_NON_LINUX_SMOKE:-0}" != 1 ]; then
    printf 'error: this qualification script must run on Linux\n' >&2
    exit 2
  fi
  QUALIFICATION_LABEL="Local runtime v1 C smoke ($HOST_OS/$HOST_ARCH; not Linux qualification)"
else
  case "$HOST_ARCH" in
    x86_64|aarch64) ;;
    *)
      printf 'error: unsupported Linux runtime qualification architecture: %s\n' "$HOST_ARCH" >&2
      exit 2
      ;;
  esac
fi

ulimit -c 0 || true
cd "$BUILD_DIR"

build_and_run() {
  test_name=$1
  shift
  case "$test_name" in
    yazabase_vfs_v1) set -- -DYAZA_VFS_V1_TESTING -DYAZA_VFS_V1_TEST_MAX_HANDLES=8 "$@" ;;
  esac
  "$RUNTIME_CC" -std=c11 -Wall -Wextra -Werror -pthread "$@" \
    "$RUNTIME_DIR/tests/$test_name.c" "$RUNTIME_DIR/src/yaza_runtime_v1.c" \
    -I"$RUNTIME_DIR/include" -lm -o "$BUILD_DIR/$test_name"
  case "$test_name" in
    scope_cap_v1) YAZA_RT_V1_MAX_LIVE_SCOPES=3 "$BUILD_DIR/$test_name" ;;
    process_quota_v1) YAZA_RT_V1_PROCESS_LIMIT_BYTES=131072 "$BUILD_DIR/$test_name" ;;
    *) "$BUILD_DIR/$test_name" ;;
  esac
}

for runtime_test in \
  text_v1 arena_v1 bytes_v1 typed_list_v1 numeric_text_v1 float64_v1 args_v1 \
  request_scope_v1 request_scope_concurrent_v1 request_cycle_handle_v1 \
  scope_cap_v1 scope_owner_v1 sibling_cycle_copy_v1 process_quota_v1 \
  thread_v1 yazabase_vfs_v1; do
  build_and_run "$runtime_test"
done

build_and_run_release() {
  test_name=$1
  shift
  "$RUNTIME_CC" -std=c11 -O2 -Wall -Wextra -Werror -pthread "$@" \
    "$RUNTIME_DIR/tests/$test_name.c" "$RUNTIME_DIR/src/yaza_runtime_v1.c" \
    -I"$RUNTIME_DIR/include" -lm -o "$BUILD_DIR/$test_name-release"
  "$BUILD_DIR/$test_name-release"
  printf 'runtime-v1: %s optimized -O2 smoke passed\n' "$test_name"
}

build_and_run_release arena_v1
build_and_run_release typed_list_v1
build_and_run_release request_scope_concurrent_v1
build_and_run_release thread_v1
build_and_run_release allocator_failure_v1 -DYAZA_RT_V1_TESTING

build_and_run config_v1
"$RUNTIME_CC" -std=c11 -Wall -Wextra -Werror -pthread \
  "$RUNTIME_DIR/tests/environment_config_v1.c" "$RUNTIME_DIR/src/yaza_runtime_v1.c" \
  -I"$RUNTIME_DIR/include" -lm -o "$BUILD_DIR/environment_config_v1"
env \
  YAZA_RT_V1_PROCESS_LIMIT_BYTES=131072 \
  YAZA_RT_V1_PREFERRED_CHUNK_BYTES=16384 \
  YAZA_RT_V1_MAX_LIVE_SCOPES=2 \
  YAZA_RT_V1_MAX_LIST_ITEMS=16 \
  YAZA_RT_V1_MAX_LIST_VECTOR_BYTES=64 \
  "$BUILD_DIR/environment_config_v1"

build_and_run invalid_environment_config_v1
build_and_run allocator_failure_v1 -DYAZA_RT_V1_TESTING

build_and_run_tsan() {
  test_name=$1
  set --
  case "$test_name" in
    yazabase_vfs_v1) set -- -DYAZA_VFS_V1_TESTING -DYAZA_VFS_V1_TEST_MAX_HANDLES=8 ;;
  esac
  "$RUNTIME_CC" -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=thread \
    "$@" \
    "$RUNTIME_DIR/tests/$test_name.c" "$RUNTIME_DIR/src/yaza_runtime_v1.c" \
    -I"$RUNTIME_DIR/include" -lm -o "$BUILD_DIR/$test_name-tsan"
  TSAN_OPTIONS=${TSAN_OPTIONS:-halt_on_error=1} "$BUILD_DIR/$test_name-tsan"
  printf 'runtime-v1: %s ThreadSanitizer passed\n' "$test_name"
}

build_and_run_tsan arena_v1
build_and_run_tsan typed_list_v1
build_and_run_tsan request_scope_concurrent_v1
build_and_run_tsan thread_v1
build_and_run_tsan yazabase_vfs_v1

"$RUNTIME_CC" -std=c11 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$RUNTIME_DIR/tests/arena_asan_poison_v1.c" \
  "$RUNTIME_DIR/src/yaza_runtime_v1.c" -I"$RUNTIME_DIR/include" -lm \
  -o "$BUILD_DIR/arena-asan-poison"
ASAN_OPTIONS=detect_leaks=0 "$BUILD_DIR/arena-asan-poison"
ASAN_OPTIONS=detect_leaks=0:halt_on_error=0:abort_on_error=0 \
  "$BUILD_DIR/arena-asan-poison" --read-after-reset \
  >"$BUILD_DIR/arena-use-after-reset.log" 2>&1 || :
if ! grep -F 'use-after-poison' "$BUILD_DIR/arena-use-after-reset.log" >/dev/null; then
  cat "$BUILD_DIR/arena-use-after-reset.log" >&2
  printf 'error: Linux ASan did not report a stale arena read after reset\n' >&2
  exit 1
fi

printf '%s passed, including ASan arena-reset poisoning. Generated LLVM module and compiler-target checks remain separate.\n' "$QUALIFICATION_LABEL"

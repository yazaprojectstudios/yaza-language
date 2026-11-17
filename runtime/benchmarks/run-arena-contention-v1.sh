#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)
RUNTIME_CC=${CC:-clang}
TEMP_ROOT=${TMPDIR:-/tmp}
TEMP_ROOT=${TEMP_ROOT%/}
BUILD_DIR=$(mktemp -d "$TEMP_ROOT/yaza-arena-contention.XXXXXX")
trap 'rm -rf "$BUILD_DIR"' EXIT HUP INT TERM

"$RUNTIME_CC" -std=c11 -O2 -Wall -Wextra -Werror -pthread \
  "$SCRIPT_DIR/arena_contention_v1.c" \
  "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$BUILD_DIR/arena-contention"
"$BUILD_DIR/arena-contention"

"$RUNTIME_CC" -std=c11 -O2 -Wall -Wextra -Werror -pthread -DYAZA_RT_V1_BENCH_GLOBAL_PROCESS_LIVE_METRICS \
  "$SCRIPT_DIR/arena_contention_v1.c" \
  "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$BUILD_DIR/arena-contention-global-live-metrics-reference"
"$BUILD_DIR/arena-contention-global-live-metrics-reference"

"$RUNTIME_CC" -std=c11 -O2 -Wall -Wextra -Werror -pthread -DYAZA_RT_V1_BENCH_GLOBAL_PROCESS_ALLOCATION_COUNT \
  "$SCRIPT_DIR/arena_contention_v1.c" \
  "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$BUILD_DIR/arena-contention-global-count-reference"
"$BUILD_DIR/arena-contention-global-count-reference"

"$RUNTIME_CC" -std=c11 -O2 -Wall -Wextra -Werror -pthread -DYAZA_RT_V1_BENCH_SKIP_PROCESS_PEAK_METRICS \
  "$SCRIPT_DIR/arena_contention_v1.c" \
  "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$BUILD_DIR/arena-contention-no-process-peaks"
"$BUILD_DIR/arena-contention-no-process-peaks"

for skipped_metric in PROCESS_USED PROCESS_STORAGE PROCESS_ALLOCATION_COUNT; do
  binary_name=$(printf '%s' "$skipped_metric" | tr '[:upper:]' '[:lower:]')
  "${RUNTIME_CC}" -std=c11 -O2 -Wall -Wextra -Werror -pthread "-DYAZA_RT_V1_BENCH_SKIP_${skipped_metric}" \
    "$SCRIPT_DIR/arena_contention_v1.c" \
    "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
    -I"$PROJECT_DIR/runtime/include" -lm -o "$BUILD_DIR/arena-contention-$binary_name"
  "$BUILD_DIR/arena-contention-$binary_name"
done

"$RUNTIME_CC" -std=c11 -O2 -Wall -Wextra -Werror -pthread \
  -DYAZA_RT_V1_BENCH_SKIP_PROCESS_STORAGE -DYAZA_RT_V1_BENCH_SKIP_PROCESS_ALLOCATION_COUNT \
  "$SCRIPT_DIR/arena_contention_v1.c" \
  "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$BUILD_DIR/arena-contention-process-used-only"
"$BUILD_DIR/arena-contention-process-used-only"

"$RUNTIME_CC" -std=c11 -O2 -Wall -Wextra -Werror -pthread -DYAZA_RT_V1_BENCH_SKIP_PROCESS_METRICS \
  "$SCRIPT_DIR/arena_contention_v1.c" \
  "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" \
  -I"$PROJECT_DIR/runtime/include" -lm -o "$BUILD_DIR/arena-contention-no-process-metrics"
"$BUILD_DIR/arena-contention-no-process-metrics"

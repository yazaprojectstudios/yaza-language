#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
YAZA_BIN=${YAZA_TEST_CLI:-"$PROJECT_DIR/build/yaza"}
TEMP_ROOT=${TMPDIR:-/tmp}
TEMP_ROOT=${TEMP_ROOT%/}
WORK_DIR=$(mktemp -d "$TEMP_ROOT/yaza-data-science.XXXXXX")
trap 'rm -rf "$WORK_DIR"' EXIT HUP INT TERM

export YAZA_STDLIB_DIR="$PROJECT_DIR/std"
export YAZA_RUNTIME_DIR="$PROJECT_DIR/runtime"

printf '==> Data Science Gate 1: Emitting LLVM IR for data-science-smoke...\n'
"$YAZA_BIN" emit-llvm "$PROJECT_DIR/examples/data-science-smoke.yz" -o "$WORK_DIR/ds-smoke.ll"

printf '==> Data Science Gate 2: Compiling & running in debug, -O2, and ASan/UBSan...\n'
clang -Wno-override-module -std=c11 -O0 -g "$WORK_DIR/ds-smoke.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" -I"$PROJECT_DIR/runtime/include" -pthread -lm -o "$WORK_DIR/ds-smoke-debug"
clang -Wno-override-module -std=c11 -O2 "$WORK_DIR/ds-smoke.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" -I"$PROJECT_DIR/runtime/include" -pthread -lm -o "$WORK_DIR/ds-smoke-release"
clang -Wno-override-module -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer "$WORK_DIR/ds-smoke.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" -I"$PROJECT_DIR/runtime/include" -pthread -lm -o "$WORK_DIR/ds-smoke-asan"

for mode in debug release asan; do
  OUT=$("$WORK_DIR/ds-smoke-$mode")
  if [ "$OUT" != "YAZA_DATA_SCIENCE_SMOKE: PASS" ]; then
    printf 'error: unexpected output in %s mode: %s\n' "$mode" "$OUT" >&2
    exit 1
  fi
  printf '  [%s] data-science-smoke: PASS\n' "$mode"
done

printf '==> Data Science Gate 3: SciPy/NumPy 86-Case Golden Cross-Validation (debug, -O2, ASan/UBSan)...\n'
"$YAZA_BIN" emit-llvm "$PROJECT_DIR/tests/stdlib/data_science_golden_check.yz" -o "$WORK_DIR/ds-golden.ll"
clang -Wno-override-module -std=c11 -O0 -g "$WORK_DIR/ds-golden.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" -I"$PROJECT_DIR/runtime/include" -pthread -lm -o "$WORK_DIR/ds-golden-debug"
clang -Wno-override-module -std=c11 -O2 "$WORK_DIR/ds-golden.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" -I"$PROJECT_DIR/runtime/include" -pthread -lm -o "$WORK_DIR/ds-golden-release"
clang -Wno-override-module -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer "$WORK_DIR/ds-golden.ll" "$PROJECT_DIR/runtime/src/yaza_runtime_v1.c" -I"$PROJECT_DIR/runtime/include" -pthread -lm -o "$WORK_DIR/ds-golden-asan"

for mode in debug release asan; do
  GOUT=$("$WORK_DIR/ds-golden-$mode")
  if ! printf '%s\n' "$GOUT" | grep -q "GOLDEN_SCIPY_CROSS_VALIDATION_PASS cases=86 failures=0"; then
    printf 'error: golden cross-validation failed in %s mode:\n%s\n' "$mode" "$GOUT" >&2
    exit 1
  fi
  printf '  [%s] data-science-golden (86 SciPy cases): PASS\n' "$mode"
done
printf '%s\n' "$GOUT"

printf 'data-science-validation-ok\n'

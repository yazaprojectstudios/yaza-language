#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
LLVM_COMPILER=${YAZA_LLVM_BIN:-"$PROJECT_DIR/build/yaza-llvm"}

if [ ! -x "$LLVM_COMPILER" ]; then
  printf 'error: LLVM compiler is not executable: %s\n' "$LLVM_COMPILER" >&2
  exit 127
fi

printf '==> Testing Core 2 Positive Smoke Fixtures...\n'

# 1. Ownership & Move
"$LLVM_COMPILER" run "$PROJECT_DIR/examples/core-ownership-move-smoke.yz" >/dev/null
printf '  [PASS] core-ownership-move-smoke.yz\n'

# 2. Ownership & Borrowing
"$LLVM_COMPILER" run "$PROJECT_DIR/examples/core-ownership-borrow-smoke.yz" >/dev/null
printf '  [PASS] core-ownership-borrow-smoke.yz\n'

# 3. Async / Await / Spawn
"$LLVM_COMPILER" run "$PROJECT_DIR/examples/core-async-await-smoke.yz" >/dev/null
printf '  [PASS] core-async-await-smoke.yz\n'

printf '==> Testing Core 2 Negative Diagnostic Fixtures...\n'

expect_failure() {
  fixture="$1"
  pattern="$2"
  out=$("$LLVM_COMPILER" run "$PROJECT_DIR/examples/$fixture" 2>&1 || true)
  if ! printf '%s' "$out" | grep -F "$pattern" >/dev/null; then
    printf 'error: expected diagnostic "%s" in %s, got:\n%s\n' "$pattern" "$fixture" "$out" >&2
    exit 1
  fi
  printf '  [PASS] %s -> "%s"\n' "$fixture" "$pattern"
}

expect_failure "ownership-use-after-move-errors.yz" "use of moved value 'a'"
expect_failure "ownership-move-while-borrowed-errors.yz" "cannot move 'it' while borrowed"
expect_failure "ownership-double-mut-borrow-errors.yz" "overlapping mutable borrow of 'c'"
expect_failure "ownership-call-overlapping-mut-errors.yz" "overlapping mutable borrow of 'c'"
expect_failure "ownership-move-out-of-borrow-errors.yz" "cannot move out of borrowed reference 'it'"
expect_failure "async-invalid-fn-errors.yz" "expected 'fn' after 'async'"
expect_failure "await-non-task-errors.yz" "await requires a Task or Future expression"

printf '==> All Core 2 features validated successfully!\n'

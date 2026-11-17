#!/bin/sh
set -eu

PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
LLVM_COMPILER=${YAZA_LLVM_COMPILER:-"$PROJECT_DIR/build/yaza-llvm"}
WORK_DIR=${TMPDIR:-/tmp}/yaza-std-collections-$$
DEBUG_PROGRAM="$WORK_DIR/collections-search-debug"
RELEASE_PROGRAM="$WORK_DIR/collections-search-release"
OUTPUT="$WORK_DIR/output.txt"

mkdir -p "$WORK_DIR"
trap 'rm -rf "$WORK_DIR"' EXIT HUP INT TERM

YAZA_STDLIB_DIR="$PROJECT_DIR" "$LLVM_COMPILER" build \
  "$PROJECT_DIR/examples/core-collections-search-smoke.yz" \
  -o "$DEBUG_PROGRAM"
YAZA_STDLIB_DIR="$PROJECT_DIR" "$LLVM_COMPILER" build --release \
  "$PROJECT_DIR/examples/core-collections-search-smoke.yz" \
  -o "$RELEASE_PROGRAM"

for PROGRAM in "$DEBUG_PROGRAM" "$RELEASE_PROGRAM"; do
  "$PROGRAM" >"$OUTPUT"
  if [ "$(cat "$OUTPUT")" != "collections search passed" ]; then
    cat "$OUTPUT"
    echo "collections smoke produced unexpected output: $PROGRAM" >&2
    exit 1
  fi
done

SOURCE="$PROJECT_DIR/examples/core-collections-search-smoke.yz"
YAZA_STDLIB_DIR="$PROJECT_DIR" "$LLVM_COMPILER" emit-llvm \
  "$SOURCE" -o "$WORK_DIR/collections-search.ll"
clang -O2 -S -emit-llvm -x ir "$WORK_DIR/collections-search.ll" \
  -o "$WORK_DIR/collections-search-opt.ll"
clang --target=arm64-apple-macosx26.0.0 -O2 -S -x ir \
  "$WORK_DIR/collections-search.ll" -o "$WORK_DIR/collections-search-arm64.s"
clang --target=x86_64-unknown-linux-gnu -O2 -S -x ir \
  "$WORK_DIR/collections-search.ll" -o "$WORK_DIR/collections-search-x86_64.s"
python3 - "$WORK_DIR/collections-search-opt.ll" \
  "$WORK_DIR/collections-search-arm64.s" \
  "$WORK_DIR/collections-search-x86_64.s" <<'PY'
import re
import sys
from pathlib import Path

ir = Path(sys.argv[1]).read_text()
arm64 = Path(sys.argv[2]).read_text()
x86_64 = Path(sys.argv[3]).read_text()
symbol = "yaza.core.fn.collections_branchless_binary_search_number"

def function_body(text, prefix, suffix):
    start = text.find(prefix)
    if start < 0:
        raise SystemExit(f"missing generated function: {prefix}")
    end = text.find(suffix, start)
    if end < 0:
        raise SystemExit(f"unterminated generated function: {prefix}")
    return text[start:end]

ir_body = function_body(ir, f"define internal fastcc i64 @{symbol}", "\n}")
comparison = re.search(r"(%[A-Za-z0-9_.]+) = icmp slt i64 [^,]+, %arg1", ir_body)
if not comparison:
    raise SystemExit("optimized IR did not retain the Number search comparison")
condition = re.escape(comparison.group(1))
if re.search(rf"br i1 {condition},", ir_body):
    raise SystemExit("optimized IR branches on the search direction")
if not re.search(rf"select i1 {condition},", ir_body):
    raise SystemExit("optimized IR did not lower the search direction to select")

arm_body = function_body(arm64, f"_{symbol}:", "\t.cfi_endproc")
x86_body = function_body(x86_64, f"{symbol}:", "\t.cfi_endproc")
if not re.search(r"\bcsel\b", arm_body):
    raise SystemExit("optimized ARM64 assembly has no conditional select")
if not re.search(r"\bcmov[a-z]*\b", x86_body):
    raise SystemExit("optimized x86_64 assembly has no conditional move")
PY

echo "YAZA std/collections search smoke and branchless codegen checks passed"

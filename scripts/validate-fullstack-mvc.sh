#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
YAZA_BIN=${YAZA_TEST_CLI:-"$PROJECT_DIR/build/yaza"}
YAZABASE_CLI=${YAZABASE_BIN:-"$PROJECT_DIR/build/yazabase-cli"}
TEMP_ROOT=${TMPDIR:-/tmp}
TEMP_ROOT=${TEMP_ROOT%/}
WORK_DIR=$(mktemp -d "$TEMP_ROOT/yaza-fullstack-mvc.XXXXXX")
trap 'rm -rf "$WORK_DIR"' EXIT HUP INT TERM

export YAZA_STDLIB_DIR="$PROJECT_DIR/std"
export YAZA_RUNTIME_DIR="$PROJECT_DIR/runtime"
export YAZABASE_BIN="$YAZABASE_CLI"

printf '==> Fullstack Gate 1: Validating std/web/* (debug, -O2, ASan/UBSan)...\n'
sh "$PROJECT_DIR/scripts/validate-std-web.sh"

printf '==> Fullstack Gate 2: Scaffolding & verifying yaza new --fullstack...\n'
cd "$WORK_DIR"
"$YAZA_BIN" new --fullstack mvc_ci
cd mvc_ci
mkdir -p data
"$YAZABASE_CLI" set-user ./data admin 'Fullstack_CI_Pass_2026' >/dev/null
export YAZABASE_USER=admin
export YAZABASE_PASSWORD='Fullstack_CI_Pass_2026'

"$YAZA_BIN" arch check
"$YAZA_BIN" config >/dev/null
"$YAZA_BIN" check
"$YAZA_BIN" build --release src/main.yz -o mvc_ci_bin
YAZA_SELF_TEST=1 ./mvc_ci_bin

printf 'fullstack-mvc-validation-ok\n'

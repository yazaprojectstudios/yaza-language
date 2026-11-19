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

printf '==> Fullstack Gate 2: Scaffolding via yaza new --mvc & running MVC checks...\n'
cd "$WORK_DIR"
"$YAZA_BIN" new --mvc mvc_ci
cd mvc_ci
mkdir -p data
"$YAZABASE_CLI" set-user ./data admin 'Fullstack_CI_Pass_2026' >/dev/null
export YAZABASE_USER=admin
export YAZABASE_PASSWORD='Fullstack_CI_Pass_2026'

"$YAZA_BIN" arch check
"$YAZA_BIN" audit --security
"$YAZA_BIN" config >/dev/null
"$YAZA_BIN" check
"$YAZA_BIN" build --release src/main.yz -o mvc_ci_bin
YAZA_SELF_TEST=1 ./mvc_ci_bin

printf '==> Fullstack Gate 3: Negative guardrails (arch violation & weak CSP audit)...\n'
printf 'use "std/db/yazabase"\nfn bad() -> Number:\n    return 1\n' > src/views/pages/bad_view.yz
if "$YAZA_BIN" arch check >/dev/null 2>&1; then
  printf 'error: yaza arch check failed to reject view importing std/db/yazabase\n' >&2
  exit 1
fi
rm -f src/views/pages/bad_view.yz
printf '  [PASS] yaza arch check rejected forbidden views -> std/db import\n'

printf 'fn bad_csp() -> Text:\n    return "script-src '\''unsafe-inline'\''"\n' > src/views/pages/bad_csp.yz
if "$YAZA_BIN" audit --security >/dev/null 2>&1; then
  printf 'error: yaza audit --security failed to flag unsafe-inline CSP\n' >&2
  exit 1
fi
rm -f src/views/pages/bad_csp.yz
printf '  [PASS] yaza audit --security flagged weak unsafe-inline CSP (YZA-SEC-005)\n'

printf 'fullstack-mvc-validation-ok\n'

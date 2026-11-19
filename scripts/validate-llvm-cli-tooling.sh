#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
COMPILER=${YAZA_LLVM_COMPILER:-"$PROJECT_DIR/build/yaza-llvm"}

if [ ! -x "$COMPILER" ]; then
  printf 'error: compiler binary not found at %s\n' "$COMPILER" >&2
  exit 1
fi

TMP_DIR=$(mktemp -d /tmp/yaza-cli-validate.XXXXXX)
cleanup() {
  rm -rf "$TMP_DIR"
}
trap cleanup EXIT INT TERM

printf '==> 1. Testing --help and --version\n'
"$COMPILER" --help | grep -q "YAZA LLVM"
"$COMPILER" --version | grep -q "YAZA LLVM 1.0.1"
printf '  [PASS] --help and --version verified\n'

printf '==> 2. Testing check subcommand\n'
"$COMPILER" check "$PROJECT_DIR/examples/hello.yz" | grep -q "Valid"
JSON_CHECK=$("$COMPILER" check --format json "$PROJECT_DIR/examples/hello.yz")
printf '%s\n' "$JSON_CHECK" | grep -q '"diagnostics":'
printf '  [PASS] check verified (human & json)\n'

printf '==> 3. Testing secret token subcommand\n'
TOKEN=$("$COMPILER" secret token)
TOKEN_LEN=$(printf '%s' "$TOKEN" | wc -c | tr -d ' ')
if [ "$TOKEN_LEN" -ne 64 ]; then
  printf 'error: expected 64 hex characters, got %s\n' "$TOKEN_LEN" >&2
  exit 1
fi
printf '  [PASS] secret token generated 64 hex chars: %s\n' "$TOKEN"

printf '==> 4. Testing new project scaffolding\n'
(cd "$TMP_DIR" && "$COMPILER" new --minimal-api min_api)
if [ ! -f "$TMP_DIR/min_api/yaza.toml" ] || [ ! -f "$TMP_DIR/min_api/src/main.yz" ]; then
  printf 'error: minimal-api project files missing\n' >&2
  exit 1
fi
"$COMPILER" check "$TMP_DIR/min_api/src/main.yz" | grep -q "Valid"

(cd "$TMP_DIR" && "$COMPILER" new --api full_api)
if [ ! -f "$TMP_DIR/full_api/yaza.toml" ] || [ ! -f "$TMP_DIR/full_api/src/main.yz" ]; then
  printf 'error: api project files missing\n' >&2
  exit 1
fi
"$COMPILER" check "$TMP_DIR/full_api/src/main.yz" | grep -q "Valid"
printf '  [PASS] new --minimal-api and --api scaffolded and checked\n'

printf '==> 5. Testing build --sanitize\n'
"$COMPILER" build --sanitize "$PROJECT_DIR/examples/hello.yz" -o "$TMP_DIR/hello_san"
SAN_OUT=$("$TMP_DIR/hello_san")
if [ "$SAN_OUT" != "Hello, world!" ]; then
  printf 'error: unexpected sanitized binary output: %s\n' "$SAN_OUT" >&2
  exit 1
fi
printf '  [PASS] build --sanitize compiled and executed cleanly\n'

printf '==> 6. Testing bench subcommand\n'
"$COMPILER" bench "$PROJECT_DIR/examples/hello.yz" 3 | grep -q "average"
printf '  [PASS] bench verified\n'

printf '==> 7. Testing audit --security\n'
AUDIT_HUMAN=$(cd "$TMP_DIR/min_api" && "$COMPILER" audit --security)
printf '%s\n' "$AUDIT_HUMAN" | grep -q "YAZA Security Audit"
AUDIT_JSON=$(cd "$TMP_DIR/min_api" && "$COMPILER" audit --security --format json)
printf '%s\n' "$AUDIT_JSON" | grep -q '"findings":'
printf '  [PASS] audit --security verified (human & json)\n'

printf '==> 8. Testing test subcommand\n'
mkdir -p "$TMP_DIR/test_suite"
cat << 'EOF' > "$TMP_DIR/test_suite/sample.test.yz"
start:
    assert 1 + 1 == 2
EOF
TEST_OUT="$("$COMPILER" test "$TMP_DIR/test_suite")"
printf '%s\n' "$TEST_OUT" | grep -q "PASS"
printf '  [PASS] test runner executed fixture\n'

printf '==> 9. Testing architecture check\n'
"$COMPILER" arch check "$TMP_DIR/full_api" | grep -q "no forbidden module dependencies"
printf '  [PASS] architecture check verified\n'

printf '==> 10. Testing run with arguments and port\n'
cat << 'EOF' > "$TMP_DIR/port_test.yz"
start:
    let port: Text = env_get("YAZA_PORT")
    say "PORT=" + port
EOF
RUN_OUT="$("$COMPILER" run --port 8088 "$TMP_DIR/port_test.yz")"
printf '%s\n' "$RUN_OUT" | grep -q "PORT=8088"
printf '  [PASS] run --port verified\n'

printf '\nAll Phase 6 CLI tooling tests PASSED cleanly!\n'

#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
YAZA_BIN=${YAZA_TEST_CLI:-"$PROJECT_DIR/build/yaza"}
PORT=${YAZA_TEST_HTTPS_PORT:-18443}
WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/yaza-https-client.XXXXXX")
SERVER_PID=
cleanup() {
  if [ -n "$SERVER_PID" ]; then kill "$SERVER_PID" 2>/dev/null || true; fi
  rm -rf "$WORK_DIR"
}
trap cleanup EXIT HUP INT TERM

cat > "$WORK_DIR/cert.conf" <<'EOF'
[req]
distinguished_name = dn
x509_extensions = v3
prompt = no
[dn]
CN = localhost
[v3]
subjectAltName = DNS:localhost,IP:127.0.0.1
basicConstraints = critical,CA:TRUE
keyUsage = critical,digitalSignature,keyCertSign
EOF
openssl req -x509 -newkey rsa:2048 -nodes \
  -keyout "$WORK_DIR/key.pem" -out "$WORK_DIR/cert.pem" \
  -days 1 -config "$WORK_DIR/cert.conf" >/dev/null 2>&1

export YAZA_STDLIB_DIR="$PROJECT_DIR"
export YAZA_RUNTIME_DIR="$PROJECT_DIR/runtime"
"$YAZA_BIN" build --release "$PROJECT_DIR/examples/https-trusted-smoke.yz" \
  -o "$WORK_DIR/trusted"
"$YAZA_BIN" build --release "$PROJECT_DIR/examples/https-selfsigned-smoke.yz" \
  -o "$WORK_DIR/untrusted"

openssl s_server -accept "$PORT" -cert "$WORK_DIR/cert.pem" \
  -key "$WORK_DIR/key.pem" -www > "$WORK_DIR/server.log" 2>&1 &
SERVER_PID=$!
attempt=0
until curl -ksS --max-time 1 "https://localhost:$PORT/" >/dev/null 2>&1; do
  attempt=$((attempt + 1))
  if [ "$attempt" -ge 20 ] || ! kill -0 "$SERVER_PID" 2>/dev/null; then
    cat "$WORK_DIR/server.log" >&2
    exit 1
  fi
  sleep 0.2
done
if ! kill -0 "$SERVER_PID" 2>/dev/null; then
  cat "$WORK_DIR/server.log" >&2
  exit 1
fi

YAZA_TLS_CA_BUNDLE="$WORK_DIR/cert.pem" "$WORK_DIR/trusted" "https://localhost:$PORT/"
YAZA_TLS_CA_BUNDLE= "$WORK_DIR/untrusted" "https://localhost:$PORT/"
printf 'https-client-focused-validation-ok\n'

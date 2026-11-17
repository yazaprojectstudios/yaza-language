#!/bin/sh
set -eu

PROJECT_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TEMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/yaza-api-starter-smoke.XXXXXX")
SERVER="$TEMP_DIR/api-server"
RESPONSE="$TEMP_DIR/response.json"
PORT=${YAZA_TEST_PORT:-$((20000 + $$ % 40000))}
YAZA_TEST_CLI=${YAZA_TEST_CLI:-yaza}
YAZA_TEST_SANITIZE=${YAZA_TEST_SANITIZE:-0}
SERVER_PID=
FALLBACK_PID=

cleanup() {
    if [ -n "$SERVER_PID" ]; then
        kill -TERM "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    if [ -n "$FALLBACK_PID" ]; then
        kill -TERM "$FALLBACK_PID" 2>/dev/null || true
        wait "$FALLBACK_PID" 2>/dev/null || true
    fi
    rm -rf "$TEMP_DIR"
}
trap cleanup EXIT HUP INT TERM

fail() {
    printf 'FAIL %s\n' "$1" >&2
    if [ -f "$RESPONSE" ]; then
        printf 'Last response body:\n' >&2
        cat "$RESPONSE" >&2
        printf '\n' >&2
    fi
    if [ -f "$TEMP_DIR/server.log" ]; then cat "$TEMP_DIR/server.log" >&2; fi
    exit 1
}

cd "$PROJECT_ROOT"
if [ "$YAZA_TEST_SANITIZE" = 1 ]; then
    "$YAZA_TEST_CLI" build --sanitize src/main.yz -o "$SERVER"
else
    "$YAZA_TEST_CLI" build src/main.yz -o "$SERVER"
fi
if YAZA_HOST=127.0.0.1 YAZA_PORT="$PORT" YAZA_API_ALLOW_PROCESS=invalid DATABASE_URL= \
    "$SERVER" >"$TEMP_DIR/invalid-process-policy.log" 2>&1; then
    fail 'API accepted an invalid process-spawn policy'
fi
grep -F "YAZA_API_ALLOW_PROCESS must be exactly 'true' or 'false'" \
    "$TEMP_DIR/invalid-process-policy.log" >/dev/null || fail 'invalid process policy did not fail closed with a clear diagnostic'
if YAZA_HOST=127.0.0.1 YAZA_PORT="$PORT" YAZA_API_ALLOW_PROCESS=true \
    YAZA_API_ALLOWED_PROCESSES= DATABASE_URL= \
    "$SERVER" >"$TEMP_DIR/missing-process-allowlist.log" 2>&1; then
    fail 'API accepted process spawning without an executable allowlist'
fi
grep -F 'YAZA_API_ALLOW_PROCESS=true requires YAZA_API_ALLOWED_PROCESSES' \
    "$TEMP_DIR/missing-process-allowlist.log" >/dev/null || fail 'missing executable allowlist did not fail closed'
if YAZA_HOST=127.0.0.1 YAZA_PORT="$PORT" YAZA_API_ALLOW_PROCESS=true \
    YAZA_API_ALLOWED_PROCESSES=touch DATABASE_URL= \
    "$SERVER" >"$TEMP_DIR/invalid-process-allowlist.log" 2>&1; then
    fail 'API accepted a relative executable allowlist entry'
fi
grep -F 'YAZA_API_ALLOWED_PROCESSES must contain at most 16 canonical absolute executables' \
    "$TEMP_DIR/invalid-process-allowlist.log" >/dev/null || fail 'invalid executable allowlist did not fail closed'
if YAZA_HOST=127.0.0.1 YAZA_PORT="$PORT" YAZA_API_ALLOW_PROCESS=true \
    YAZA_API_ALLOWED_PROCESSES=/usr/bin/touch YAZA_API_ALLOWED_PROCESS_ARGV= DATABASE_URL= \
    "$SERVER" >"$TEMP_DIR/missing-process-argv-policy.log" 2>&1; then
    fail 'API accepted process spawning without an exact-argv policy'
fi
grep -F 'YAZA_API_ALLOW_PROCESS=true requires YAZA_API_ALLOWED_PROCESS_ARGV' \
    "$TEMP_DIR/missing-process-argv-policy.log" >/dev/null || fail 'missing exact-argv policy did not fail closed'

YAZA_HOST=127.0.0.1 YAZA_PORT="$PORT" YAZA_API_ALLOW_PROCESS= DATABASE_URL= \
    "$SERVER" >"$TEMP_DIR/server.log" 2>&1 &
SERVER_PID=$!

attempt=0
until curl -fsS --max-time 1 "http://127.0.0.1:$PORT/healthz" >/dev/null 2>&1; do
    attempt=$((attempt + 1))
    if [ "$attempt" -ge 30 ]; then fail 'API starter did not become ready'; fi
    sleep 0.1
done
grep -F 'Process spawn : disabled (explicitly allow executables and exact argv vectors to enable)' \
    "$TEMP_DIR/server.log" >/dev/null || fail 'API did not report the default-deny process policy'
curl -fsS --max-time 2 "http://127.0.0.1:$PORT/readyz" >"$RESPONSE" \
    || fail 'readiness endpoint did not respond'
grep -F '"status":"ok"' "$RESPONSE" >/dev/null || fail 'readiness endpoint returned the wrong status'
curl -fsS --max-time 2 "http://127.0.0.1:$PORT/metrics" >"$TEMP_DIR/worker-metrics.txt" \
    || fail 'worker metrics endpoint did not respond'
grep -E '^yaza_api_workers [1-9][0-9]*$' "$TEMP_DIR/worker-metrics.txt" >/dev/null \
    || fail 'worker metrics did not expose a positive thread count'
grep -E '^yaza_api_worker_queue_capacity [1-9][0-9]*$' "$TEMP_DIR/worker-metrics.txt" >/dev/null \
    || fail 'worker metrics did not expose a positive bounded queue capacity'

YAZA_HOST=127.0.0.1 YAZA_PORT="$PORT" YAZA_API_ALLOW_PROCESS=true \
    YAZA_API_ALLOWED_PROCESSES=/usr/bin/touch \
    YAZA_API_ALLOWED_PROCESS_ARGV='[["/usr/bin/touch","/tmp/yaza-smoke-marker"]]' DATABASE_URL= \
    "$SERVER" >"$TEMP_DIR/fallback.log" 2>&1 &
FALLBACK_PID=$!
attempt=0
until grep -F 'Listening     : http://127.0.0.1:' "$TEMP_DIR/fallback.log" >/dev/null 2>&1; do
    attempt=$((attempt + 1))
    if [ "$attempt" -ge 30 ]; then fail 'port-fallback API did not report its selected address'; fi
    sleep 0.1
done
grep -F 'Process spawn : enabled for explicit-env APIs with executable and exact-argv policies' \
    "$TEMP_DIR/fallback.log" >/dev/null || fail 'API did not report both process capability policies'
grep -F 'Allowed tools : 1 exact executable paths' \
    "$TEMP_DIR/fallback.log" >/dev/null || fail 'API did not report its allowlist size'
grep -F 'Allowed argv  : 1 exact argument vectors' \
    "$TEMP_DIR/fallback.log" >/dev/null || fail 'API did not report its exact-argv policy size'
fallback_port=$(sed -n 's/^  Listening[[:space:]]*: http:\/\/127\.0\.0\.1:\([0-9][0-9]*\)$/\1/p' "$TEMP_DIR/fallback.log" | sed -n '1p')
[ -n "$fallback_port" ] || fail 'port-fallback API did not print a parseable port'
[ "$fallback_port" != "$PORT" ] || fail 'port-fallback API reused an occupied port'
grep -F "requested port $PORT is occupied; selected the next available port $fallback_port" \
    "$TEMP_DIR/fallback.log" >/dev/null || fail 'port-fallback API did not explain the selected port'
curl -fsS --max-time 3 "http://127.0.0.1:$fallback_port/healthz" >/dev/null || fail 'health endpoint did not respond on the fallback port'
kill -TERM "$FALLBACK_PID"
wait "$FALLBACK_PID"
FALLBACK_PID=

status=$(curl -sS --max-time 5 -o "$RESPONSE" -w '%{http_code}' "http://127.0.0.1:$PORT/") || fail 'root request failed'
[ "$status" = 200 ] || fail "root returned $status; expected 200"
grep -F 'Hello, world!' "$RESPONSE" >/dev/null || fail 'root returned the wrong welcome response'

status=$(curl -sS --max-time 5 -o "$RESPONSE" -w '%{http_code}' "http://127.0.0.1:$PORT/api/products") || fail 'product list request failed'
[ "$status" = 200 ] || fail "product list returned $status; expected 200"
grep -E '^\[[[:space:]]*\{' "$RESPONSE" >/dev/null || fail 'product list did not return a JSON array of records'
grep -E '"id":[[:space:]]*"sku-001"' "$RESPONSE" >/dev/null || fail 'product list omitted the first typed record ID'
grep -E '"name":[[:space:]]*"Canvas Tote Bag"' "$RESPONSE" >/dev/null || fail 'sample products were not returned'

status=$(curl -sS --max-time 5 -o "$RESPONSE" -w '%{http_code}' \
    "http://127.0.0.1:$PORT/api/products/sku-001") || fail 'product detail request failed'
[ "$status" = 200 ] || fail "product detail returned $status; expected 200"
grep -F 'Canvas Tote Bag' "$RESPONSE" >/dev/null || fail 'product detail returned the wrong record'

status=$(curl -sS --max-time 5 -o "$RESPONSE" -w '%{http_code}' -X POST \
    -H 'Content-Type: application/json' \
    --data '{"name":"Smoke item","priceMinor":1000,"currency":"IDR","available":true}' \
    "http://127.0.0.1:$PORT/api/products") || fail 'product create request failed'
[ "$status" = 201 ] || fail "product create returned $status; expected 201"
grep -F 'Smoke item' "$RESPONSE" >/dev/null || fail 'created product was not returned'

status=$(curl -sS --max-time 5 -o "$RESPONSE" -w '%{http_code}' -X PUT \
    -H 'Content-Type: application/json' \
    --data '{"name":"Replacement","priceMinor":2000,"currency":"IDR","available":true}' \
    "http://127.0.0.1:$PORT/api/products/sku-001") || fail 'product replace request failed'
[ "$status" = 200 ] || fail "product replace returned $status; expected 200"
grep -F 'Replacement' "$RESPONSE" >/dev/null || fail 'replaced product was not returned'

status=$(curl -sS --max-time 5 -o "$RESPONSE" -w '%{http_code}' -X PATCH \
    -H 'Content-Type: application/json' --data '{"name":"Patched"}' \
    "http://127.0.0.1:$PORT/api/products/sku-001") || fail 'product patch request failed'
[ "$status" = 200 ] || fail "product patch returned $status; expected 200"
grep -F 'Patched' "$RESPONSE" >/dev/null || fail 'patched product was not returned'

status=$(curl -sS --max-time 5 -o "$RESPONSE" -w '%{http_code}' -X DELETE \
    "http://127.0.0.1:$PORT/api/products/sku-001") || fail 'product delete request failed'
[ "$status" = 204 ] || fail "product delete returned $status; expected 204"

concurrent_pids=
index=0
while [ "$index" -lt 12 ]; do
    curl -sS --max-time 5 -o "$TEMP_DIR/concurrent-$index.json" -w '%{http_code}' -X POST \
        -H 'Content-Type: application/json' \
        --data "{\"name\":\"Concurrent $index\",\"priceMinor\":1000,\"currency\":\"IDR\",\"available\":true}" \
        "http://127.0.0.1:$PORT/api/products" >"$TEMP_DIR/concurrent-$index.status" &
    concurrent_pids="$concurrent_pids $!"
    index=$((index + 1))
done
for pid in $concurrent_pids; do
    wait "$pid" || fail 'parallel product create request failed'
done
index=0
while [ "$index" -lt 12 ]; do
    status=$(cat "$TEMP_DIR/concurrent-$index.status")
    [ "$status" = 201 ] || fail "parallel product create $index returned $status; expected 201"
    grep -F "Concurrent $index" "$TEMP_DIR/concurrent-$index.json" >/dev/null ||
        fail "parallel product create $index returned the wrong record"
    index=$((index + 1))
done
status=$(curl -sS --max-time 5 -o "$RESPONSE" -w '%{http_code}' \
    "http://127.0.0.1:$PORT/api/products?limit=100") || fail 'parallel product list request failed'
[ "$status" = 200 ] || fail "parallel product list returned $status; expected 200"
index=0
while [ "$index" -lt 12 ]; do
    grep -F "Concurrent $index" "$RESPONSE" >/dev/null ||
        fail "parallel create $index was missing from the product store"
    index=$((index + 1))
done

printf '%s\n' 'API starter smoke passed (health/readiness, worker metrics, Product CRUD, and 12 concurrent creates).'

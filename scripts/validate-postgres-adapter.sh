#!/bin/sh
set -eu

cd "$(dirname "$0")/.."
mkdir -p build

echo "=== [1/4] Validating PostgreSQL URI parser ==="
./build/yaza-llvm build examples/postgres-uri-smoke.yz -o build/postgres-uri-smoke
./build/postgres-uri-smoke
echo "  [OK] PostgreSQL URI parsing and configuration pass"

echo "=== [2/4] Validating PostgreSQL wire protocol v3 framing ==="
./build/yaza-llvm build examples/postgres-protocol-smoke.yz -o build/postgres-protocol-smoke
./build/postgres-protocol-smoke
echo "  [OK] PostgreSQL wire protocol v3 frames and messages pass"

echo "=== [3/4] Validating PostgreSQL SCRAM-SHA-256 authentication ==="
./build/yaza-llvm build examples/postgres-scram-smoke.yz -o build/postgres-scram-smoke
./build/postgres-scram-smoke
echo "  [OK] PostgreSQL SCRAM-SHA-256 SASL handshake pass"

echo "=== [4/4] Validating PostgreSQL typed row and DbValue mapping ==="
./build/yaza-llvm build examples/postgres-values-smoke.yz -o build/postgres-values-smoke
./build/postgres-values-smoke
echo "  [OK] PostgreSQL typed row and DbValue mapping pass"

echo "=== PostgreSQL Adapter Verification Passed (exit 0) ==="

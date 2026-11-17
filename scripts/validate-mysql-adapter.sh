#!/bin/sh
set -eu

cd "$(dirname "$0")/.."
mkdir -p build

echo "=== [1/4] Validating MySQL URI parser ==="
./build/yaza-llvm build examples/mysql-uri-smoke.yz -o build/mysql-uri-smoke
./build/mysql-uri-smoke
echo "  [OK] MySQL URI parsing and configuration pass"

echo "=== [2/4] Validating MySQL wire protocol v10 framing ==="
./build/yaza-llvm build examples/mysql-protocol-smoke.yz -o build/mysql-protocol-smoke
./build/mysql-protocol-smoke
echo "  [OK] MySQL wire protocol v10 frames, packets, and commands pass"

echo "=== [3/4] Validating MySQL authentication (caching_sha2_password) ==="
./build/yaza-llvm build examples/mysql-auth-smoke.yz -o build/mysql-auth-smoke
./build/mysql-auth-smoke
echo "  [OK] MySQL caching_sha2_password scramble and XOR pass"

echo "=== [4/4] Validating MySQL typed row and DbValue mapping ==="
./build/yaza-llvm build examples/mysql-values-smoke.yz -o build/mysql-values-smoke
./build/mysql-values-smoke
echo "  [OK] MySQL typed row and DbValue mapping pass"

echo "=== MySQL Adapter Verification Passed (exit 0) ==="

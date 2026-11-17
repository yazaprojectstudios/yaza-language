#!/bin/sh
set -eu

cd "$(dirname "$0")/.."
mkdir -p build

echo "=== [1/3] Validating MSSQL URI parser ==="
./build/yaza-llvm build examples/mssql-uri-smoke.yz -o build/mssql-uri-smoke
./build/mssql-uri-smoke
echo "  [OK] MSSQL URI parsing and configuration pass"

echo "=== [2/3] Validating MSSQL TDS wire protocol framing ==="
./build/yaza-llvm build examples/mssql-protocol-smoke.yz -o build/mssql-protocol-smoke
./build/mssql-protocol-smoke
echo "  [OK] MSSQL TDS wire protocol packets, prelogin, batch, and tokens pass"

echo "=== [3/3] Validating MSSQL typed row and DbValue mapping ==="
./build/yaza-llvm build examples/mssql-values-smoke.yz -o build/mssql-values-smoke
./build/mssql-values-smoke
echo "  [OK] MSSQL typed row and DbValue mapping pass"

echo "=== MSSQL Adapter Verification Passed (exit 0) ==="

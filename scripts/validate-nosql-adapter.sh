#!/bin/sh
set -eu

cd "$(dirname "$0")/.."
mkdir -p build

echo "=== [1/3] Validating NoSQL URI parsers (Redis & MongoDB) ==="
./build/yaza-llvm build examples/nosql-uri-smoke.yz -o build/nosql-uri-smoke
./build/nosql-uri-smoke
echo "  [OK] NoSQL URI parsing and configuration pass"

echo "=== [2/3] Validating Key-Value NoSQL wire protocol (Redis RESP) ==="
./build/yaza-llvm build examples/nosql-kv-smoke.yz -o build/nosql-kv-smoke
./build/nosql-kv-smoke
echo "  [OK] Key-Value RESP protocol serialization, commands, and responses pass"

echo "=== [3/3] Validating Document NoSQL wire protocol (BSON & OP_MSG) ==="
./build/yaza-llvm build examples/nosql-doc-smoke.yz -o build/nosql-doc-smoke
./build/nosql-doc-smoke
echo "  [OK] Document BSON serialization, parsing, and OP_MSG wire packets pass"

echo "=== NoSQL Adapter Verification Passed (exit 0) ==="

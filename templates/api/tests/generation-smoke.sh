#!/bin/sh
set -eu

PROJECT_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TEMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/yaza-api-starter-generation.XXXXXX")
YAZA_TEST_CLI=${YAZA_TEST_CLI:-yaza}
trap 'rm -rf "$TEMP_DIR"' EXIT HUP INT TERM

if ! command -v jq >/dev/null 2>&1; then
    printf '%s\n' 'FAIL generation smoke requires jq' >&2
    exit 2
fi

cd "$PROJECT_ROOT"
"$YAZA_TEST_CLI" generate --postman -o "$TEMP_DIR/postman.json"
"$YAZA_TEST_CLI" generate --swagger -o "$TEMP_DIR/swagger.json"

jq -e '
    (.item | length) == 8
    and ([.item[] | select(.request.method == "HEAD")] | length) == 2
    and ([.item[] | select(.request.method != "HEAD")] | length) == 6
    and any(.item[]; .name == "/api/products" and .request.method == "GET"
        and .request.url.raw == "http://127.0.0.1:8080/api/products?limit={{route1_query_limit}}&offset={{route1_query_offset}}")
    and any(.item[]; .name == "/api/products" and .request.method == "POST"
        and .request.url.raw == "http://127.0.0.1:8080/api/products"
        and (.request.body.raw | fromjson).name == "sample"
        and (.request.body.raw | fromjson).priceMinor == 1
        and (.request.body.raw | fromjson).currency == "sample"
        and (.request.body.raw | fromjson).available == true)
    and any(.item[]; .name == "HEAD /api/products" and .request.method == "HEAD"
        and .request.url.raw == "http://127.0.0.1:8080/api/products?limit={{route1_query_limit}}&offset={{route1_query_offset}}")
    and ([.variable[].key] | length) == ([.variable[].key] | unique | length)
    and all(.item[]; (.request.url.raw | startswith("http://127.0.0.1:8080/")))
' "$TEMP_DIR/postman.json" >/dev/null || {
    printf '%s\n' 'FAIL Postman output must contain all six declared routes, their two implicit HEAD requests, usable absolute URLs, unique variables, and a generated typed request example' >&2
    exit 1
}

jq -e '
    .openapi == "3.1.0"
    and (.paths | length) == 2
    and .servers[0].url == "http://127.0.0.1:8080"
    and (.paths["/api/products"].get.parameters | any(.name == "limit" and .in == "query"))
' "$TEMP_DIR/swagger.json" >/dev/null || {
    printf '%s\n' 'FAIL Swagger/OpenAPI output did not include the product API, server URL, and query parameters' >&2
    exit 1
}

printf '%s\n' 'API starter generation smoke passed (Postman and Swagger/OpenAPI).'

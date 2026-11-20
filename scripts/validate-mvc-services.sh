#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
YAZA_BIN=${YAZA_TEST_CLI:-"$PROJECT_DIR/build/yaza"}
YAZABASE_CLI=${YAZABASE_BIN:-"$PROJECT_DIR/build/yazabase-cli"}
WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/yaza-mvc-services.XXXXXX")
trap 'rm -rf "$WORK_DIR"' EXIT HUP INT TERM
export YAZA_STDLIB_DIR="$PROJECT_DIR"
export YAZA_RUNTIME_DIR="$PROJECT_DIR/runtime"
export YAZABASE_BIN="$YAZABASE_CLI"

"$YAZA_BIN" run "$PROJECT_DIR/examples/login-limiter-smoke.yz"
"$YAZA_BIN" run "$PROJECT_DIR/examples/telemetry-histogram-smoke.yz"
"$YAZA_BIN" run "$PROJECT_DIR/examples/db-migrations-smoke.yz"
"$YAZA_BIN" run "$PROJECT_DIR/examples/outbound-transport-smoke.yz"
"$YAZA_BIN" run "$PROJECT_DIR/examples/api-config-workers-smoke.yz"

ln -s "$PROJECT_DIR/templates" "$WORK_DIR/templates"
cd "$WORK_DIR"
"$YAZA_BIN" new --mvc mvc_ci
cd mvc_ci
mkdir -p data
"$YAZABASE_CLI" set-user ./data db_service DatabaseServicePass_2026
export YAZABASE_USER=db_service
export YAZABASE_PASSWORD=DatabaseServicePass_2026
export YAZABASE_DATA_DIR=./data
export YAZA_ADMIN_EMAIL=admin@example.com
export YAZA_PUBLIC_BASE_URL=http://127.0.0.1:8080
"$YAZA_BIN" build --release src/admin_setup.yz -o admin-setup
YAZA_WEB_ADMIN_USER=admin YAZA_WEB_ADMIN_PASSWORD=WebAdminPass_2026_Secure ./admin-setup
"$YAZA_BIN" build --release src/migrate.yz -o migrate
./migrate up
./migrate up
./migrate down
./migrate up
"$YAZA_BIN" build --release src/mail_worker.yz -o mail-worker
"$YAZA_BIN" build --release src/job_worker.yz -o job-worker
"$YAZA_BIN" build --release src/main.yz -o app
YAZA_SELF_TEST=1 ./app
cp "$PROJECT_DIR/tests/mvc-services-smoke.yz" src/mvc_services_smoke.yz
"$YAZA_BIN" build --release src/mvc_services_smoke.yz -o mvc-services-smoke
./mvc-services-smoke
mkdir -p data/job_files
printf 'abc' > data/job_files/input.txt
chmod 600 data/job_files/input.txt
cp "$PROJECT_DIR/tests/mvc-background-job-smoke.yz" src/mvc_background_job_smoke.yz
"$YAZA_BIN" build --release src/mvc_background_job_smoke.yz -o mvc-background-job-smoke
./mvc-background-job-smoke
cp "$PROJECT_DIR/tests/mvc-trace-smoke.yz" src/mvc_trace_smoke.yz
"$YAZA_BIN" build --release src/mvc_trace_smoke.yz -o mvc-trace-smoke
YAZA_TRACE_REQUESTS=1 ./mvc-trace-smoke > "$WORK_DIR/trace.log"
grep -Fq '"layer":"controller"' "$WORK_DIR/trace.log"
grep -Fq '"layer":"service"' "$WORK_DIR/trace.log"
grep -Fq '"layer":"database"' "$WORK_DIR/trace.log"
grep -Fq '"request_id":"trace-smoke-1"' "$WORK_DIR/trace.log"
grep -Fq 'mvc-trace-smoke passed' "$WORK_DIR/trace.log"
printf 'mvc-trace-smoke passed\n'
printf 'mvc-services-focused-validation-ok\n'

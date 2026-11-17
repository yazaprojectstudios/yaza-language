#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
COMPILER=${YAZA_LLVM_COMPILER:-"$PROJECT_DIR/build/yaza-llvm"}
case "$COMPILER" in
  /*) ;;
  *) COMPILER="$(pwd)/$COMPILER" ;;
esac
BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/yazabase-phase0.XXXXXX")
trap 'rm -rf "$BUILD_DIR"' EXIT HUP INT TERM

if [ ! -x "$COMPILER" ]; then
  printf 'error: executable YAZA LLVM compiler not found: %s\n' "$COMPILER" >&2
  exit 127
fi

# Keep the compiler-host arena generous for the large YazaBase binder and
# catalog fixtures. The generated fixture executables still start with the
# runtime's normal program limit in run_fixture.
COMPILER_EXECUTABLE=$COMPILER
COMPILER_PROCESS_LIMIT_BYTES=${YAZA_COMPILER_PROCESS_LIMIT_BYTES:-34359738368}
COMPILER_LIST_VECTOR_LIMIT_BYTES=${YAZA_COMPILER_LIST_VECTOR_LIMIT_BYTES:-34359738368}
COMPILER_MAX_LIST_ITEMS=${YAZA_COMPILER_MAX_LIST_ITEMS:-32000000}
export YAZA_COMPILER_EXECUTABLE=$COMPILER_EXECUTABLE
export YAZA_COMPILER_PROCESS_LIMIT_BYTES=$COMPILER_PROCESS_LIMIT_BYTES
export YAZA_COMPILER_LIST_VECTOR_LIMIT_BYTES=$COMPILER_LIST_VECTOR_LIMIT_BYTES
export YAZA_COMPILER_MAX_LIST_ITEMS=$COMPILER_MAX_LIST_ITEMS
COMPILER_WRAPPER=$BUILD_DIR/yaza-compiler-wrapper
cat >"$COMPILER_WRAPPER" <<'EOF'
#!/bin/sh
YAZA_RT_V1_PROCESS_LIMIT_BYTES="$YAZA_COMPILER_PROCESS_LIMIT_BYTES" \
YAZA_RT_V1_MAX_LIST_VECTOR_BYTES="$YAZA_COMPILER_LIST_VECTOR_LIMIT_BYTES" \
YAZA_RT_V1_MAX_LIST_ITEMS="$YAZA_COMPILER_MAX_LIST_ITEMS" \
  exec "$YAZA_COMPILER_EXECUTABLE" "$@"
EOF
chmod +x "$COMPILER_WRAPPER"
COMPILER=$COMPILER_WRAPPER

run_fixture() {
  executable=$1
  expected=$2
  shift 2
  process_limit=
  case "${1:-}" in
    --process-limit=*)
      process_limit=${1#--process-limit=}
      shift
      ;;
  esac
  if [ -n "$process_limit" ]; then
    if ! output=$(YAZA_RT_V1_PROCESS_LIMIT_BYTES="$process_limit" "$executable" "$@"); then
      printf 'error: fixture process failed: %s\n' "$executable" >&2
      printf '%s\n' "$output" >&2
      return 1
    fi
  elif ! output=$("$executable" "$@"); then
    printf 'error: fixture process failed: %s\n' "$executable" >&2
    printf '%s\n' "$output" >&2
    return 1
  fi
  printf '%s\n' "$output"
  case "$output" in
    *"$expected"*) ;;
    *)
      printf 'error: fixture did not report its success marker (%s): %s\n' "$expected" "$executable" >&2
      return 1
      ;;
  esac
}

run_fixture_with_disk_quota() {
  quota_limit_bytes=$1
  executable=$2
  expected=$3
  shift 3
  if ! quota_output=$(YAZA_DB_MAX_DISK_BYTES="$quota_limit_bytes" "$executable" "$@"); then
    printf 'error: disk-quota fixture process failed: %s\n' "$executable" >&2
    printf '%s\n' "$quota_output" >&2
    return 1
  fi
  printf '%s\n' "$quota_output"
  case "$quota_output" in
    *"$expected"*) ;;
    *)
      printf 'error: disk-quota fixture did not report its success marker (%s): %s\n' "$expected" "$executable" >&2
      return 1
      ;;
  esac
}

cd "$PROJECT_DIR"
for fixture in \
  core-float64-bits-smoke \
  yazabase-sql-lexer-v1-smoke \
  yazabase-sql-parser-v1-smoke \
  yazabase-sql-parser-v2-smoke \
  yazabase-sql-parser-v3-smoke \
  yazabase-sql-create-index-parser-v1-smoke \
  yazabase-sql-parser-fuzz-v1-smoke \
  yazabase-storage-decoder-fuzz-v1-smoke \
  yazabase-sql-insert-v1-smoke \
  yazabase-sql-create-table-v1-smoke \
  yazabase-sql-insert-bind-v1-smoke \
  yazabase-sql-insert-exec-v1-smoke \
  yazabase-sql-binder-v1-smoke \
  yazabase-sql-explain-v1-smoke \
  yazabase-sql-executor-v1-smoke \
  yazabase-sql-inner-join-memory-v1-smoke \
  yazabase-sql-multi-join-memory-v1-smoke \
  yazabase-key-v1-smoke \
  yazabase-page-v1-smoke \
  yazabase-page-wal-v1-smoke \
  yazabase-node-split-wal-v1-smoke \
  yazabase-meta-free-v1-smoke \
  yazabase-catalog-v1-smoke \
  yazabase-row-v1-smoke \
  yazabase-mvcc-v1-smoke \
  yazabase-overflow-v1-smoke \
  yazabase-fallible-parse-smoke; do
  YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
    "$COMPILER" build "examples/$fixture.yz" -o "$BUILD_DIR/$fixture"
  case "$fixture" in
    core-float64-bits-smoke) expected=core-float64-bits-ok ;;
    *) expected="${fixture%-smoke}-ok" ;;
  esac
  run_fixture "$BUILD_DIR/$fixture" "$expected"
done

YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-wal-v1-smoke.yz \
  -o "$BUILD_DIR/yazabase-wal-v1-smoke"
mkdir "$BUILD_DIR/wal-root"
run_fixture "$BUILD_DIR/yazabase-wal-v1-smoke" yazabase-wal-v1-ok "$BUILD_DIR/wal-root"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-wal-rotation-smoke.yz \
  -o "$BUILD_DIR/yazabase-wal-rotation-smoke"
mkdir "$BUILD_DIR/wal-rotation-root"
run_fixture "$BUILD_DIR/yazabase-wal-rotation-smoke" yazabase-wal-rotation-ok \
  --process-limit=1073741824 "$BUILD_DIR/wal-rotation-root"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-vfs-v1-smoke.yz \
  -o "$BUILD_DIR/yazabase-vfs-v1-smoke"
mkdir "$BUILD_DIR/vfs-root"
run_fixture "$BUILD_DIR/yazabase-vfs-v1-smoke" 'vfs round-trip bytes:' "$BUILD_DIR/vfs-root"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-storage-v1-smoke.yz \
  -o "$BUILD_DIR/yazabase-storage-v1-smoke"
mkdir "$BUILD_DIR/storage-root"
run_fixture "$BUILD_DIR/yazabase-storage-v1-smoke" yazabase-storage-v1-ok "$BUILD_DIR/storage-root"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-page-image-v1-smoke.yz \
  -o "$BUILD_DIR/yazabase-page-image-v1-smoke"
mkdir "$BUILD_DIR/page-image-root"
run_fixture "$BUILD_DIR/yazabase-page-image-v1-smoke" yazabase-page-image-v1-ok "$BUILD_DIR/page-image-root"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-allocator-v1-smoke.yz \
  -o "$BUILD_DIR/yazabase-allocator-v1-smoke"
mkdir "$BUILD_DIR/allocator-root"
run_fixture "$BUILD_DIR/yazabase-allocator-v1-smoke" yazabase-allocator-v1-ok "$BUILD_DIR/allocator-root"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-checkpoint-v1-smoke.yz \
  -o "$BUILD_DIR/yazabase-checkpoint-v1-smoke"
mkdir "$BUILD_DIR/checkpoint-root"
run_fixture "$BUILD_DIR/yazabase-checkpoint-v1-smoke" yazabase-checkpoint-v1-ok "$BUILD_DIR/checkpoint-root"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-capacity-growth-v1-smoke.yz \
  -o "$BUILD_DIR/yazabase-capacity-growth-v1-smoke"
mkdir "$BUILD_DIR/capacity-growth-root"
run_fixture "$BUILD_DIR/yazabase-capacity-growth-v1-smoke" \
  yazabase-capacity-growth-v1-ok "$BUILD_DIR/capacity-growth-root"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-disk-quota-v1-smoke.yz \
  -o "$BUILD_DIR/yazabase-disk-quota-v1-smoke"
mkdir "$BUILD_DIR/disk-quota-create-root" "$BUILD_DIR/disk-quota-growth-root" "$BUILD_DIR/disk-quota-wal-root" "$BUILD_DIR/disk-quota-invalid-root"
run_fixture_with_disk_quota 134217727 "$BUILD_DIR/yazabase-disk-quota-v1-smoke" \
  "yazabase-disk-quota-v1-ok database-create-denied-cleanly" \
  "$BUILD_DIR/disk-quota-create-root" create-denied
run_fixture_with_disk_quota 134217728 "$BUILD_DIR/yazabase-disk-quota-v1-smoke" \
  "yazabase-disk-quota-v1-ok database-growth-denied-with-files-unchanged" \
  "$BUILD_DIR/disk-quota-growth-root" growth-denied
run_fixture_with_disk_quota 134217728 "$BUILD_DIR/yazabase-disk-quota-v1-smoke" \
  "yazabase-disk-quota-v1-ok wal-segment-create-denied-cleanly" \
  "$BUILD_DIR/disk-quota-wal-root" wal-rotation-denied
run_fixture_with_disk_quota invalid "$BUILD_DIR/yazabase-disk-quota-v1-smoke" \
  "yazabase-disk-quota-v1-ok invalid-config-rejected" \
  "$BUILD_DIR/disk-quota-invalid-root" invalid-config

YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-leaf-recovery-v1-smoke.yz \
  -o "$BUILD_DIR/yazabase-leaf-recovery-v1-smoke"
mkdir "$BUILD_DIR/leaf-recovery-root"
run_fixture "$BUILD_DIR/yazabase-leaf-recovery-v1-smoke" \
  yazabase-leaf-recovery-v1-ok "$BUILD_DIR/leaf-recovery-root"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-node-split-recovery-v1-smoke.yz \
  -o "$BUILD_DIR/yazabase-node-split-recovery-v1-smoke"
mkdir "$BUILD_DIR/node-split-recovery-root"
run_fixture "$BUILD_DIR/yazabase-node-split-recovery-v1-smoke" \
  yazabase-node-split-recovery-v1-ok "$BUILD_DIR/node-split-recovery-root"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-transaction-v1-smoke.yz \
  -o "$BUILD_DIR/yazabase-transaction-v1-smoke"
mkdir "$BUILD_DIR/transaction-root"
run_fixture "$BUILD_DIR/yazabase-transaction-v1-smoke" \
  yazabase-transaction-v1-ok "$BUILD_DIR/transaction-root"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-empty-transaction-v1-smoke.yz \
  -o "$BUILD_DIR/yazabase-empty-transaction-v1-smoke"
mkdir "$BUILD_DIR/empty-transaction-root"
run_fixture "$BUILD_DIR/yazabase-empty-transaction-v1-smoke" \
  yazabase-empty-transaction-v1-ok "$BUILD_DIR/empty-transaction-root"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-sql-drop-table-v1-smoke.yz \
  -o "$BUILD_DIR/yazabase-sql-drop-table-v1-smoke"
mkdir "$BUILD_DIR/sql-drop-table-root"
run_fixture "$BUILD_DIR/yazabase-sql-drop-table-v1-smoke" \
  yazabase-sql-drop-table-v1-ok "$BUILD_DIR/sql-drop-table-root"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-sql-create-index-v1-smoke.yz \
  -o "$BUILD_DIR/yazabase-sql-create-index-v1-smoke"
mkdir "$BUILD_DIR/sql-create-index-root"
run_fixture "$BUILD_DIR/yazabase-sql-create-index-v1-smoke" \
  yazabase-sql-create-index-v1-ok "$BUILD_DIR/sql-create-index-root"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-sql-alter-table-v1-smoke.yz \
  -o "$BUILD_DIR/yazabase-sql-alter-table-v1-smoke"
mkdir "$BUILD_DIR/sql-alter-table-root"
run_fixture "$BUILD_DIR/yazabase-sql-alter-table-v1-smoke" \
  yazabase-sql-alter-table-v1-ok "$BUILD_DIR/sql-alter-table-root"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-root-growth-v1-smoke.yz \
  -o "$BUILD_DIR/yazabase-root-growth-v1-smoke"
mkdir "$BUILD_DIR/root-growth-root"
run_fixture "$BUILD_DIR/yazabase-root-growth-v1-smoke" \
  yazabase-root-growth-v1-ok "$BUILD_DIR/root-growth-root"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-catalog-init-v1-smoke.yz \
  -o "$BUILD_DIR/yazabase-catalog-init-v1-smoke"
mkdir "$BUILD_DIR/catalog-init-root"
run_fixture "$BUILD_DIR/yazabase-catalog-init-v1-smoke" yazabase-catalog-init-v1-ok "$BUILD_DIR/catalog-init-root"

mkdir "$BUILD_DIR/database-cli-root"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build --release "$PROJECT_DIR/compiler/yazabase_cli.yz" \
  -o "$PROJECT_DIR/build/yazabase-cli"
run_fixture "$COMPILER" "Created YazaBase database" db init \
  "$BUILD_DIR/database-cli-root" "cli-init-smoke.ydb"
if [ ! -f "$BUILD_DIR/database-cli-root/cli-init-smoke.ydb" ]; then
  printf 'error: yaza db init did not create the requested database file\n' >&2
  exit 1
fi
run_fixture "$COMPILER" "Committed transaction 1 at timestamp 2" db sql \
  "$BUILD_DIR/database-cli-root" "cli-init-smoke.ydb" \
  "CREATE TABLE people (id BIGINT PRIMARY KEY, name TEXT)"
run_fixture "$COMPILER" "YazaBase logical plan v1 (physical access path not shown)" db sql \
  "$BUILD_DIR/database-cli-root" "cli-init-smoke.ydb" \
  "EXPLAIN SELECT id, name FROM people WHERE id = 1"
run_fixture "$COMPILER" "2: Project(columns=2)" db sql \
  "$BUILD_DIR/database-cli-root" "cli-init-smoke.ydb" \
  'EXPLAIN SELECT id, name FROM people WHERE id = ?'
if explain_bind_output=$("$COMPILER" db sql \
  "$BUILD_DIR/database-cli-root" "cli-init-smoke.ydb" \
  'EXPLAIN SELECT id FROM people WHERE id = ?' --param 'bigint:1' 2>&1); then
  printf 'error: EXPLAIN unexpectedly consumed execution bind values\n' >&2
  exit 1
fi
case "$explain_bind_output" in
  *"EXPLAIN does not consume runtime bind values"*) ;;
  *)
    printf 'error: EXPLAIN did not report its bind-value contract\n' >&2
    printf '%s\n' "$explain_bind_output" >&2
    exit 1
    ;;
esac
run_fixture "$COMPILER" "Committed transaction 2 at timestamp 3" db sql \
  "$BUILD_DIR/database-cli-root" "cli-init-smoke.ydb" \
  "INSERT INTO people (id, name) VALUES (1, 'Ada'), (2, 'Grace')"
csv_insert_sql="INSERT INTO people (id, name) VALUES (3, 'Comma, \"quoted\" value'), (4, 'line one
line two'), (5, NULL), (6, '\N')"
run_fixture "$COMPILER" "Committed transaction 3 at timestamp 4" db sql \
  "$BUILD_DIR/database-cli-root" "cli-init-smoke.ydb" "$csv_insert_sql"
run_fixture "$COMPILER" "1 | Ada" db sql \
  "$BUILD_DIR/database-cli-root" "cli-init-smoke.ydb" \
  "SELECT id, name FROM people WHERE id >= 1 ORDER BY id"
run_fixture "$COMPILER" "1 | Ada" db sql \
  "$BUILD_DIR/database-cli-root" "cli-init-smoke.ydb" \
  'SELECT id, name FROM people WHERE id = $1' --param 'bigint:1'
run_fixture "$COMPILER" "1 | Ada" db sql \
  "$BUILD_DIR/database-cli-root" "cli-init-smoke.ydb" \
  'SELECT id, name FROM people WHERE name = ?' --param 'text:Ada'
run_fixture "$COMPILER" "0 row(s)" db sql \
  "$BUILD_DIR/database-cli-root" "cli-init-smoke.ydb" \
  'SELECT id, name FROM people WHERE name = $1' --param 'null:text'
run_fixture "$COMPILER" "Committed transaction 4 at timestamp 5" db sql \
  "$BUILD_DIR/database-cli-root" "cli-init-smoke.ydb" \
  "CREATE TABLE typed_values (id BIGINT PRIMARY KEY, flag BOOLEAN, amount DOUBLE, small SMALLINT, whole INTEGER, label TEXT)"
run_fixture "$COMPILER" "Committed transaction 5 at timestamp 6" db sql \
  "$BUILD_DIR/database-cli-root" "cli-init-smoke.ydb" \
  "INSERT INTO typed_values VALUES (1, TRUE, 1.25, 7, 9, 'value')"
run_fixture "$COMPILER" "1 | TRUE" db sql \
  "$BUILD_DIR/database-cli-root" "cli-init-smoke.ydb" \
  'SELECT id, flag FROM typed_values WHERE flag = $1' --param 'boolean:true'
run_fixture "$COMPILER" "1" db sql \
  "$BUILD_DIR/database-cli-root" "cli-init-smoke.ydb" \
  'SELECT id FROM typed_values WHERE amount = $1' --param 'double:1.25'
run_fixture "$COMPILER" "1" db sql \
  "$BUILD_DIR/database-cli-root" "cli-init-smoke.ydb" \
  'SELECT id FROM typed_values WHERE small = $1' --param 'smallint:7'
run_fixture "$COMPILER" "1" db sql \
  "$BUILD_DIR/database-cli-root" "cli-init-smoke.ydb" \
  'SELECT id FROM typed_values WHERE whole = $1' --param 'integer:9'
run_fixture "$COMPILER" "1" db sql \
  "$BUILD_DIR/database-cli-root" "cli-init-smoke.ydb" \
  'SELECT id FROM typed_values WHERE label = $1' --param 'text:value'
if bind_range_output=$("$COMPILER" db sql \
  "$BUILD_DIR/database-cli-root" "cli-init-smoke.ydb" \
  'SELECT id FROM typed_values WHERE small = $1' --param 'smallint:40000' 2>&1); then
  printf 'error: CLI accepted an out-of-range SMALLINT bind value\n' >&2
  exit 1
fi
case "$bind_range_output" in
  *"SQL SMALLINT value is out of range"*) ;;
  *)
    printf 'error: CLI did not report a SMALLINT range error\n' >&2
    printf '%s\n' "$bind_range_output" >&2
    exit 1
    ;;
esac
run_fixture "$COMPILER" "1 | Ada" db sql \
  "$BUILD_DIR/database-cli-root" "cli-init-smoke.ydb" \
  "SELECT p.id, p.name FROM people AS p WHERE p.id = 1 ORDER BY p.id"
run_fixture "$COMPILER" "1 | Ada" db sql \
  "$BUILD_DIR/database-cli-root" "cli-init-smoke.ydb" \
  "SELECT id, name FROM people LIMIT 1"
run_fixture "$COMPILER" '"Comma, ""quoted"" value"' db sql --csv \
  "$BUILD_DIR/database-cli-root" "cli-init-smoke.ydb" \
  "SELECT name FROM people WHERE id = 3"
run_fixture "$COMPILER" '"line one
line two"' db sql --csv \
  "$BUILD_DIR/database-cli-root" "cli-init-smoke.ydb" \
  "SELECT name FROM people WHERE id = 4"
run_fixture "$COMPILER" '\N' db sql --csv \
  "$BUILD_DIR/database-cli-root" "cli-init-smoke.ydb" \
  "SELECT name FROM people WHERE id = 5"
run_fixture "$COMPILER" '"\N"' db sql --csv \
  "$BUILD_DIR/database-cli-root" "cli-init-smoke.ydb" \
  "SELECT name FROM people WHERE id = 6"
if duplicate_output=$("$COMPILER" db init \
  "$BUILD_DIR/database-cli-root" "cli-init-smoke.ydb" 2>&1); then
  printf 'error: yaza db init replaced an existing database\n' >&2
  exit 1
fi
case "$duplicate_output" in
  *"YDBv1 database name already exists"*) ;;
  *)
    printf 'error: yaza db init did not report the duplicate database name\n' >&2
    printf '%s\n' "$duplicate_output" >&2
    exit 1
    ;;
esac
if [ ! -f "$BUILD_DIR/database-cli-root/cli-init-smoke.ydb" ]; then
  printf 'error: duplicate yaza db init removed the existing database file\n' >&2
  exit 1
fi

mkdir "$BUILD_DIR/database-cli-backup-root"
mkdir "$BUILD_DIR/database-cli-restore-root"
run_fixture "$COMPILER" "Backed up YazaBase database" db backup \
  "$BUILD_DIR/database-cli-root" "cli-init-smoke.ydb" \
  "$BUILD_DIR/database-cli-backup-root"
if [ ! -f "$BUILD_DIR/database-cli-backup-root/cli-init-smoke.ydb" ]; then
  printf 'error: yaza db backup did not publish the database file\n' >&2
  exit 1
fi
run_fixture "$COMPILER" "Restored YazaBase database" db restore \
  "$BUILD_DIR/database-cli-backup-root" "cli-init-smoke.ydb" \
  "$BUILD_DIR/database-cli-restore-root"
run_fixture "$COMPILER" "1 | Ada" db sql \
  "$BUILD_DIR/database-cli-restore-root" "cli-init-smoke.ydb" \
  "SELECT id, name FROM people WHERE id = 1"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-catalog-snapshot-v1-smoke.yz \
  -o "$BUILD_DIR/yazabase-catalog-snapshot-v1-smoke"
mkdir "$BUILD_DIR/catalog-snapshot-root"
run_fixture "$BUILD_DIR/yazabase-catalog-snapshot-v1-smoke" \
  yazabase-catalog-snapshot-v1-ok "$BUILD_DIR/catalog-snapshot-root"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-sql-create-table-persist-v1-smoke.yz \
  -o "$BUILD_DIR/yazabase-sql-create-table-persist-v1-smoke"
mkdir "$BUILD_DIR/create-table-persist-root"
run_fixture "$BUILD_DIR/yazabase-sql-create-table-persist-v1-smoke" \
  yazabase-sql-create-table-persist-v1-ok "$BUILD_DIR/create-table-persist-root"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-sql-decimal-persist-v1-smoke.yz \
  -o "$BUILD_DIR/yazabase-sql-decimal-persist-v1-smoke"
mkdir "$BUILD_DIR/decimal-persist-root"
run_fixture "$BUILD_DIR/yazabase-sql-decimal-persist-v1-smoke" \
  yazabase-sql-decimal-persist-v1-ok "$BUILD_DIR/decimal-persist-root"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-overflow-sql-v1-smoke.yz \
  -o "$BUILD_DIR/yazabase-overflow-sql-v1-smoke"
mkdir "$BUILD_DIR/overflow-sql-root"
run_fixture "$BUILD_DIR/yazabase-overflow-sql-v1-smoke" \
  yazabase-overflow-sql-v1-ok "$BUILD_DIR/overflow-sql-root"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-backup-v1-smoke.yz \
  -o "$BUILD_DIR/yazabase-backup-v1-smoke"
mkdir "$BUILD_DIR/backup-source-root"
mkdir "$BUILD_DIR/backup-destination-root"
run_fixture "$BUILD_DIR/yazabase-backup-v1-smoke" \
  yazabase-backup-v1-ok "$BUILD_DIR/backup-source-root" \
  "$BUILD_DIR/backup-destination-root"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-catalog-tree-growth-v1-smoke.yz \
  -o "$BUILD_DIR/yazabase-catalog-tree-growth-v1-smoke"
mkdir "$BUILD_DIR/catalog-tree-growth-root"
YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-sql-inner-split-v1-smoke.yz \
  -o "$BUILD_DIR/yazabase-sql-inner-split-v1-smoke"
mkdir "$BUILD_DIR/sql-inner-split-root"
run_fixture "$BUILD_DIR/yazabase-sql-inner-split-v1-smoke" \
  yazabase-sql-inner-split-v1-ok "$BUILD_DIR/sql-inner-split-root"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-sql-multi-ancestor-split-v1-smoke.yz \
  -o "$BUILD_DIR/yazabase-sql-multi-ancestor-split-v1-smoke"
mkdir "$BUILD_DIR/sql-multi-ancestor-split-root"
run_fixture "$BUILD_DIR/yazabase-sql-multi-ancestor-split-v1-smoke" \
  yazabase-sql-multi-ancestor-split-v1-ok "$BUILD_DIR/sql-multi-ancestor-split-root"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-btree-scan-v1-smoke.yz \
  -o "$BUILD_DIR/yazabase-btree-scan-v1-smoke"
mkdir "$BUILD_DIR/btree-scan-root"
run_fixture "$BUILD_DIR/yazabase-btree-scan-v1-smoke" \
  yazabase-btree-scan-v1-ok "$BUILD_DIR/btree-scan-root"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-sql-inner-join-persist-v1-smoke.yz \
  -o "$BUILD_DIR/yazabase-sql-inner-join-persist-v1-smoke"
mkdir "$BUILD_DIR/sql-inner-join-persist-root"
run_fixture "$BUILD_DIR/yazabase-sql-inner-join-persist-v1-smoke" \
  yazabase-sql-inner-join-persist-v1-ok "$BUILD_DIR/sql-inner-join-persist-root"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-sql-multi-join-persist-v1-smoke.yz \
  -o "$BUILD_DIR/yazabase-sql-multi-join-persist-v1-smoke"
mkdir "$BUILD_DIR/sql-multi-join-persist-root"
run_fixture "$BUILD_DIR/yazabase-sql-multi-join-persist-v1-smoke" \
  yazabase-sql-multi-join-persist-v1-ok "$BUILD_DIR/sql-multi-join-persist-root"

YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-sql-set-ops-v1-smoke.yz \
  -o "$BUILD_DIR/yazabase-sql-set-ops-v1-smoke"
mkdir "$BUILD_DIR/sql-set-ops-root"
run_fixture "$BUILD_DIR/yazabase-sql-set-ops-v1-smoke" \
  yazabase-sql-set-ops-v1-ok "$BUILD_DIR/sql-set-ops-root"

if YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-sql-arena-escape-errors.yz \
  -o "$BUILD_DIR/yazabase-sql-arena-escape" >"$BUILD_DIR/arena-escape.log" 2>&1; then
  printf 'error: compiler accepted a SQL AST escaping its arena scope\n' >&2
  exit 1
fi
if ! grep -F 'YIR ownership: value owned by shorter-lived arena' \
  "$BUILD_DIR/arena-escape.log" >/dev/null; then
  cat "$BUILD_DIR/arena-escape.log" >&2
  printf 'error: SQL AST escape rejection did not come from the ownership verifier\n' >&2
  exit 1
fi

if YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-sql-v2-arena-escape-errors.yz \
  -o "$BUILD_DIR/yazabase-sql-v2-arena-escape" >"$BUILD_DIR/arena-v2-escape.log" 2>&1; then
  printf 'error: compiler accepted a SQL AST v2 escaping its arena scope\n' >&2
  exit 1
fi
if ! grep -F 'YIR ownership: value owned by shorter-lived arena' \
  "$BUILD_DIR/arena-v2-escape.log" >/dev/null; then
  cat "$BUILD_DIR/arena-v2-escape.log" >&2
  printf 'error: SQL AST v2 escape rejection did not come from the ownership verifier\n' >&2
  exit 1
fi

if YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-sql-v3-arena-escape-errors.yz \
  -o "$BUILD_DIR/yazabase-sql-v3-arena-escape" >"$BUILD_DIR/arena-v3-escape.log" 2>&1; then
  printf 'error: compiler accepted a SQL AST v3 escaping its arena scope\n' >&2
  exit 1
fi
if ! grep -F 'YIR ownership: value owned by shorter-lived arena' \
  "$BUILD_DIR/arena-v3-escape.log" >/dev/null; then
  cat "$BUILD_DIR/arena-v3-escape.log" >&2
  printf 'error: SQL AST v3 escape rejection did not come from the ownership verifier\n' >&2
  exit 1
fi
if YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-vfs-lock-type-errors.yz \
  -o "$BUILD_DIR/yazabase-vfs-lock-type-errors" >"$BUILD_DIR/vfs-lock-type.log" 2>&1; then
  printf 'error: compiler accepted invalid types for vfs_lock_exclusive\n' >&2
  exit 1
fi
if ! grep -F 'vfs_lock_exclusive argument 1 must be Number' \
  "$BUILD_DIR/vfs-lock-type.log" >/dev/null; then
  cat "$BUILD_DIR/vfs-lock-type.log" >&2
  printf 'error: vfs_lock_exclusive signature mismatch had an unexpected diagnostic\n' >&2
  exit 1
fi
printf 'Yaza Base VFS lock signature mismatch was rejected by the YIR type checker\n'
if YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-vfs-list-type-errors.yz \
  -o "$BUILD_DIR/yazabase-vfs-list-type-errors" >"$BUILD_DIR/vfs-list-type.log" 2>&1; then
  printf 'error: compiler accepted invalid types for vfs_list_names\n' >&2
  exit 1
fi
if ! grep -F 'expression type Text does not match expected Number' \
  "$BUILD_DIR/vfs-list-type.log" >/dev/null; then
  cat "$BUILD_DIR/vfs-list-type.log" >&2
  printf 'error: vfs_list_names signature mismatch had an unexpected diagnostic\n' >&2
  exit 1
fi
printf 'Yaza Base VFS directory-list signature mismatch was rejected by the YIR type checker\n'
if YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/yazabase-vfs-preallocate-type-errors.yz \
  -o "$BUILD_DIR/yazabase-vfs-preallocate-type-errors" >"$BUILD_DIR/vfs-preallocate-type.log" 2>&1; then
  printf 'error: compiler accepted invalid types for preallocate\n' >&2
  exit 1
fi
if ! grep -F 'vfs_preallocate argument 2 must be Number' \
  "$BUILD_DIR/vfs-preallocate-type.log" >/dev/null; then
  cat "$BUILD_DIR/vfs-preallocate-type.log" >&2
  printf 'error: vfs_preallocate signature mismatch had an unexpected diagnostic\n' >&2
  exit 1
fi
printf 'Yaza Base VFS preallocation signature mismatch was rejected by the YIR type checker\n'
if YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/core-float64-bits-type-errors.yz \
  -o "$BUILD_DIR/core-float64-bits-type-errors" >"$BUILD_DIR/float64-bits-type.log" 2>&1; then
  printf 'error: compiler accepted a non-Float64 argument to float64_bits\n' >&2
  exit 1
fi
if ! grep -F 'float64_bits expects one Float64 argument' \
  "$BUILD_DIR/float64-bits-type.log" >/dev/null; then
  cat "$BUILD_DIR/float64-bits-type.log" >&2
  printf 'error: float64_bits signature mismatch had an unexpected diagnostic\n' >&2
  exit 1
fi
if YAZA_STDLIB_DIR="$PROJECT_DIR/std" \
  "$COMPILER" build examples/core-float64-from-bits-type-errors.yz \
  -o "$BUILD_DIR/core-float64-from-bits-type-errors" >"$BUILD_DIR/float64-from-bits-type.log" 2>&1; then
  printf 'error: compiler accepted a non-Number argument to float64_from_bits\n' >&2
  exit 1
fi
if ! grep -F 'float64_from_bits expects one Number argument' \
  "$BUILD_DIR/float64-from-bits-type.log" >/dev/null; then
  cat "$BUILD_DIR/float64-from-bits-type.log" >&2
  printf 'error: float64_from_bits signature mismatch had an unexpected diagnostic\n' >&2
  exit 1
fi
printf 'IEEE-754 bitcast signature mismatches were rejected by the YIR type checker\n'
printf 'Yaza Base SQL AST v1, v2, and v3 escapes were rejected by the YIR ownership verifier\n'
YAZA_LLVM_COMPILER="$COMPILER" \
  YAZA_COMPILER_PROCESS_LIMIT_BYTES="$COMPILER_PROCESS_LIMIT_BYTES" \
  sh "$SCRIPT_DIR/validate-yazabase-sync-recovery.sh"

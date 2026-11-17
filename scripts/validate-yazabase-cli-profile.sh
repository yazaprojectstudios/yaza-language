#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
CLI=${YAZABASE_CLI:-"$PROJECT_DIR/build/yazabase-cli"}
TEMP_ROOT=${TMPDIR:-/tmp}
TEMP_ROOT=${TEMP_ROOT%/}
WORK_DIR=$(mktemp -d "$TEMP_ROOT/yazabase-cli-profile.XXXXXX")
trap 'rm -rf "$WORK_DIR"' EXIT HUP INT TERM

if [ ! -x "$CLI" ]; then
  printf 'error: YazaBase CLI not found: %s (build it with scripts/build-yazabase-cli.sh)\n' "$CLI" >&2
  exit 127
fi

DATA_DIR="$WORK_DIR/data"
mkdir -p "$DATA_DIR"
"$CLI" init "$DATA_DIR" profile.ydb >/dev/null
"$CLI" sql "$DATA_DIR" profile.ydb 'CREATE TABLE profile_probe (id BIGINT PRIMARY KEY, amount DECIMAL(10,2))' >/dev/null
"$CLI" sql "$DATA_DIR" profile.ydb \
  'INSERT INTO profile_probe (id, amount) VALUES (1, $1)' \
  --param decimal\(10,2\):1.20 >/dev/null
"$CLI" sql "$DATA_DIR" profile.ydb 'CREATE TABLE uuid_probe (id UUID PRIMARY KEY, label TEXT NOT NULL)' >/dev/null
"$CLI" sql "$DATA_DIR" profile.ydb \
  'INSERT INTO uuid_probe (id, label) VALUES ($1, '\''uuid-cli'\'')' \
  --param uuid:550E8400-E29B-41D4-A716-446655440000 >/dev/null
"$CLI" sql "$DATA_DIR" profile.ydb 'CREATE TABLE blob_probe (id BIGINT PRIMARY KEY, body BLOB)' >/dev/null
"$CLI" sql "$DATA_DIR" profile.ydb \
  'INSERT INTO blob_probe (id, body) VALUES (1, $1)' \
  --param blob:ff008000 >/dev/null
"$CLI" sql "$DATA_DIR" profile.ydb \
  'INSERT INTO blob_probe (id, body) VALUES (2, $1)' \
  --param 'blob:' >/dev/null
"$CLI" sql "$DATA_DIR" profile.ydb \
  'INSERT INTO blob_probe (id, body) VALUES (3, $1)' \
  --param 'null:blob' >/dev/null

OUTPUT=$("$CLI" sql --csv --timings "$DATA_DIR" profile.ydb \
  'SELECT id FROM profile_probe WHERE id = $1' --param bigint:1)
for PROFILE_TAG in YAZA_PROFILE_NS YAZA_QUERY_PROFILE_NS YAZA_LOOKUP_PROFILE_NS YAZA_SCAN_PROFILE_NS; do
  case "$OUTPUT" in
    *"$PROFILE_TAG "*) ;;
    *) printf '%s\n' "$OUTPUT"; printf 'error: CLI timing profile group is missing: %s\n' "$PROFILE_TAG" >&2; exit 1 ;;
  esac
done
case "$OUTPUT" in
  *"id"*"1"*) ;;
  *) printf '%s\n' "$OUTPUT"; printf 'error: profiled SELECT returned no expected row\n' >&2; exit 1 ;;
esac

DECIMAL_OUTPUT=$(
  "$CLI" sql --csv "$DATA_DIR" profile.ydb \
    'SELECT amount FROM profile_probe WHERE amount = $1' \
    --param 'decimal(10,2):1.20' | tr -d '\r'
)
if [ "$DECIMAL_OUTPUT" != "amount
1.20" ]; then
  printf '%s\n' "$DECIMAL_OUTPUT"
  printf 'error: CLI DECIMAL bind or CSV result did not preserve exact scale\n' >&2
  exit 1
fi

NULL_DECIMAL_OUTPUT=$(
  "$CLI" sql --csv "$DATA_DIR" profile.ydb \
    'SELECT amount FROM profile_probe WHERE amount = $1' \
    --param 'null:decimal(10,2)' | tr -d '\r'
)
if [ "$NULL_DECIMAL_OUTPUT" != "amount" ]; then
  printf '%s\n' "$NULL_DECIMAL_OUTPUT"
  printf 'error: typed DECIMAL NULL bind should match no non-NULL row\n' >&2
  exit 1
fi

UUID_OUTPUT=$(
  "$CLI" sql --csv "$DATA_DIR" profile.ydb \
    'SELECT id FROM uuid_probe WHERE id = $1' \
    --param uuid:550e8400-e29b-41d4-a716-446655440000 | tr -d '\r'
)
if [ "$UUID_OUTPUT" != "id
550e8400-e29b-41d4-a716-446655440000" ]; then
  printf '%s\n' "$UUID_OUTPUT"
  printf 'error: CLI UUID bind or CSV result did not canonicalize lowercase UUID text\n' >&2
  exit 1
fi

if "$CLI" sql --csv "$DATA_DIR" profile.ydb \
  'SELECT id FROM uuid_probe WHERE id = $1' \
  --param uuid:550e8400e29b41d4a716446655440000 >/dev/null 2>&1; then
  printf 'error: CLI accepted UUID bind text without canonical hyphen positions\n' >&2
  exit 1
fi

NULL_UUID_OUTPUT=$(
  "$CLI" sql --csv "$DATA_DIR" profile.ydb \
    'SELECT id FROM uuid_probe WHERE id = $1' \
    --param 'null:uuid' | tr -d '\r'
)
if [ "$NULL_UUID_OUTPUT" != "id" ]; then
  printf '%s\n' "$NULL_UUID_OUTPUT"
  printf 'error: typed UUID NULL bind should match no non-NULL row\n' >&2
  exit 1
fi

BLOB_OUTPUT=$(
  "$CLI" sql --csv "$DATA_DIR" profile.ydb \
    'SELECT body FROM blob_probe WHERE body = $1' \
    --param blob:ff008000 | tr -d '\r'
)
if [ "$BLOB_OUTPUT" != "body
X'ff008000'" ]; then
  printf '%s\n' "$BLOB_OUTPUT"
  printf 'error: CLI BLOB bind or CSV output did not preserve arbitrary bytes\n' >&2
  exit 1
fi

EMPTY_BLOB_OUTPUT=$(
  "$CLI" sql --csv "$DATA_DIR" profile.ydb \
    'SELECT body FROM blob_probe WHERE id = 2' | tr -d '\r'
)
if [ "$EMPTY_BLOB_OUTPUT" != "body
X''" ]; then
  printf '%s\n' "$EMPTY_BLOB_OUTPUT"
  printf 'error: CLI did not distinguish an empty BLOB from NULL\n' >&2
  exit 1
fi

NULL_BLOB_OUTPUT=$(
  "$CLI" sql --csv "$DATA_DIR" profile.ydb \
    'SELECT body FROM blob_probe WHERE id = 3' | tr -d '\r'
)
if [ "$NULL_BLOB_OUTPUT" != "body
\\N" ]; then
  printf '%s\n' "$NULL_BLOB_OUTPUT"
  printf 'error: CLI did not preserve NULL BLOB distinctly from an empty BLOB\n' >&2
  exit 1
fi

if "$CLI" sql "$DATA_DIR" profile.ydb \
  'UPDATE profile_probe SET amount = $1 WHERE id = 1' \
  --param 'decimal(10,2):1.201' >/dev/null 2>&1; then
  printf 'error: CLI accepted a DECIMAL bind with non-zero digits beyond its declared scale\n' >&2
  exit 1
fi
if "$CLI" sql "$DATA_DIR" profile.ydb \
  'UPDATE profile_probe SET amount = $1 WHERE id = 1' \
  --param 'decimal(2,3):1.00' >/dev/null 2>&1; then
  printf 'error: CLI accepted a DECIMAL bind whose scale exceeds its precision\n' >&2
  exit 1
fi

PROFILE_LINES=$(printf '%s\n' "$OUTPUT" | sed -n '/^YAZA_.*PROFILE_NS /p')
printf '%s\n' "$PROFILE_LINES" | awk '
  /^YAZA_PROFILE_NS / { expected = 6; seen[1] = 1; check = 1 }
  /^YAZA_QUERY_PROFILE_NS / { expected = 4; seen[2] = 1; check = 1 }
  /^YAZA_LOOKUP_PROFILE_NS / { expected = 3; seen[3] = 1; check = 1 }
  /^YAZA_SCAN_PROFILE_NS / { expected = 7; seen[4] = 1; check = 1 }
  {
    if (!check) next
    count = 0
    for (i = 2; i <= NF; i++) {
      split($i, field, "=")
      if (field[2] !~ /^[0-9]+$/) exit 1
      count++
    }
    if (count != expected) exit 1
    check = 0
  }
  END { if (!seen[1] || !seen[2] || !seen[3] || !seen[4]) exit 1 }
' || { printf 'error: CLI timing groups must contain 20 nonnegative integer nanosecond values\n' >&2; exit 1; }

SCAN_OUTPUT=$("$CLI" sql --csv --timings "$DATA_DIR" profile.ydb 'SELECT id FROM profile_probe')
SCAN_PROFILE_LINE=$(printf '%s\n' "$SCAN_OUTPUT" | sed -n '/^YAZA_SCAN_PROFILE_NS /p')
printf '%s\n' "$SCAN_PROFILE_LINE" | awk '
  {
    for (i = 2; i <= NF; i++) {
      split($i, field, "=")
      if (field[2] !~ /^[0-9]+$/) exit 1
    }
    if (NF != 8) exit 1
  }
' || { printf '%s\n' "$SCAN_OUTPUT"; printf 'error: full-scan profile fields are missing or malformed\n' >&2; exit 1; }

printf '%s\n' "$PROFILE_LINES"
printf '%s\n' "$SCAN_PROFILE_LINE"
printf 'yazabase-cli-profile-scalar-types-focused-validation-ok\n'

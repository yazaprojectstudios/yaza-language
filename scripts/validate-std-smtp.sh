#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
YAZA_BIN=${YAZA_TEST_CLI:-"$PROJECT_DIR/build/yaza"}

if [ ! -x "$YAZA_BIN" ]; then
  printf 'error: YAZA compiler not found: %s\n' "$YAZA_BIN" >&2
  exit 127
fi

export YAZA_STDLIB_DIR="$PROJECT_DIR"
export YAZA_RUNTIME_DIR="$PROJECT_DIR/runtime"

for FIXTURE in smtp-format-smoke smtp-mvc-service-smoke smtp-local-relay-smoke outbound-transport-smoke; do
  OUTPUT=$("$YAZA_BIN" run "$PROJECT_DIR/examples/$FIXTURE.yz")
  if [ "$OUTPUT" != "$FIXTURE passed" ]; then
    printf 'error: %s failed: %s\n' "$FIXTURE" "$OUTPUT" >&2
    exit 1
  fi
  printf '%s\n' "$OUTPUT"
done

printf 'std-smtp-focused-validation-ok\n'

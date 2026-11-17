#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
MODE=${1:-both}

case "$MODE" in
  normal)
    sh "$SCRIPT_DIR/validate-yazabase-sync-recovery.sh" none unique-index
    ;;
  asan)
    sh "$SCRIPT_DIR/validate-yazabase-sync-recovery.sh" asan unique-index
    ;;
  both)
    sh "$SCRIPT_DIR/validate-yazabase-sync-recovery.sh" both unique-index
    ;;
  *)
    printf 'usage: %s [normal|asan|both]\n' "$0" >&2
    exit 2
    ;;
esac

#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
MODE=${1:-both}

case "$MODE" in
  normal)
    sh "$SCRIPT_DIR/validate-yazabase-sync-recovery.sh" none secondary-index
    ;;
  asan)
    sh "$SCRIPT_DIR/validate-yazabase-sync-recovery.sh" asan secondary-index
    ;;
  both)
    sh "$SCRIPT_DIR/validate-yazabase-sync-recovery.sh" both secondary-index
    ;;
  *)
    printf 'usage: %s [normal|asan|both]\n' "$0" >&2
    exit 2
    ;;
esac

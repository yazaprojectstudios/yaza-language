#!/bin/sh
set -eu

INSTALL_DIR=${YAZABASE_INSTALL_DIR:-"$HOME/.yazabase"}
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)

if [ -x "$SCRIPT_DIR/bin/yazabase" ]; then
  CLI_SOURCE="$SCRIPT_DIR/bin/yazabase"
elif [ -x "$REPO_ROOT/build/yazabase-cli" ]; then
  CLI_SOURCE="$REPO_ROOT/build/yazabase-cli"
else
  printf 'error: YazaBase executable not found next to this installer or in build/.\n' >&2
  exit 1
fi

if [ -f "$SCRIPT_DIR/lib/libyazabase.so" ]; then
  LIB_SOURCE="$SCRIPT_DIR/lib/libyazabase.so"
elif [ -f "$SCRIPT_DIR/lib/libyazabase.dylib" ]; then
  LIB_SOURCE="$SCRIPT_DIR/lib/libyazabase.dylib"
elif [ -f "$REPO_ROOT/build/libyazabase.so" ]; then
  LIB_SOURCE="$REPO_ROOT/build/libyazabase.so"
elif [ -f "$REPO_ROOT/build/libyazabase.dylib" ]; then
  LIB_SOURCE="$REPO_ROOT/build/libyazabase.dylib"
else
  LIB_SOURCE=
fi

if [ -f "$SCRIPT_DIR/include/yazabase.h" ]; then
  HEADER_SOURCE="$SCRIPT_DIR/include/yazabase.h"
elif [ -f "$REPO_ROOT/runtime/include/yazabase.h" ]; then
  HEADER_SOURCE="$REPO_ROOT/runtime/include/yazabase.h"
else
  printf 'error: YazaBase C header not found next to this installer or in runtime/include/.\n' >&2
  exit 1
fi

mkdir -p "$INSTALL_DIR/bin" "$INSTALL_DIR/lib" "$INSTALL_DIR/include"
cp "$CLI_SOURCE" "$INSTALL_DIR/bin/yazabase"
chmod 755 "$INSTALL_DIR/bin/yazabase"
cp "$HEADER_SOURCE" "$INSTALL_DIR/include/yazabase.h"
if [ -n "$LIB_SOURCE" ]; then
  cp "$LIB_SOURCE" "$INSTALL_DIR/lib/"
fi

printf 'Installed YazaBase to %s\n' "$INSTALL_DIR"
printf 'Add it to PATH with:\n  export PATH="%s/bin:$PATH"\n' "$INSTALL_DIR"
printf 'Try:\n  yazabase shell :memory:\n'

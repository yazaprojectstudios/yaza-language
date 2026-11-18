#!/bin/sh
set -eu

COPYFILE_DISABLE=1
export COPYFILE_DISABLE

PROJECT_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DIST_DIR="$PROJECT_ROOT/dist"
mkdir -p "$DIST_DIR"

MACHINE=$(uname -m)
VERSION="1.0.0"
PACKAGE="$DIST_DIR/yaza-$VERSION-macos-arm64.pkg"

if [ "$MACHINE" != "arm64" ]; then
    printf '%s\n' "This package targets Apple Silicon arm64; current machine is $MACHINE." >&2
    exit 1
fi

for tool in pkgbuild pkgutil; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        printf '%s\n' "Required macOS packaging tool not found: $tool" >&2
        exit 1
    fi
done

if [ ! -x "$PROJECT_ROOT/build/yaza-llvm" ]; then
    printf '%s\n' "LLVM binary not found at $PROJECT_ROOT/build/yaza-llvm" >&2
    exit 1
fi

ARGON2_PREFIX=${YAZA_ARGON2_ROOT:-$(pkg-config --variable=prefix libargon2 2>/dev/null || brew --prefix argon2 2>/dev/null || echo "")}

STAGE_DIR=$(mktemp -d "${TMPDIR:-/private/tmp}/yaza-pkg.XXXXXX")
trap 'rm -rf "$STAGE_DIR"' EXIT HUP INT TERM

PAYLOAD="$STAGE_DIR/root"
install -d "$PAYLOAD/usr/local/bin" \
    "$PAYLOAD/usr/local/include" \
    "$PAYLOAD/usr/local/lib" \
    "$PAYLOAD/usr/local/share/yaza/runtime/src" \
    "$PAYLOAD/usr/local/share/yaza/runtime/include" \
    "$PAYLOAD/usr/local/share/yaza/std" \
    "$PAYLOAD/usr/local/share/yaza/templates"

install -m 755 "$PROJECT_ROOT/build/yaza-llvm" "$PAYLOAD/usr/local/bin/yaza"

if [ -f "$PROJECT_ROOT/build/yazabase-cli" ]; then
    install -m 755 "$PROJECT_ROOT/build/yazabase-cli" "$PAYLOAD/usr/local/bin/yazabase"
fi

if [ -f "$PROJECT_ROOT/runtime/include/yazabase.h" ]; then
    install -m 644 "$PROJECT_ROOT/runtime/include/yazabase.h" "$PAYLOAD/usr/local/include/yazabase.h"
fi

if [ -f "$PROJECT_ROOT/build/libyazabase.dylib" ]; then
    install -m 755 "$PROJECT_ROOT/build/libyazabase.dylib" "$PAYLOAD/usr/local/lib/libyazabase.dylib"
fi

if [ -f "$PROJECT_ROOT/runtime/include/yaza_runtime_v1.h" ]; then
    install -m 644 "$PROJECT_ROOT/runtime/include/yaza_runtime_v1.h" "$PAYLOAD/usr/local/share/yaza/runtime/include/yaza_runtime_v1.h"
fi

if [ -f "$PROJECT_ROOT/runtime/src/yaza_runtime_v1.c" ]; then
    install -m 644 "$PROJECT_ROOT/runtime/src/yaza_runtime_v1.c" "$PAYLOAD/usr/local/share/yaza/runtime/src/yaza_runtime_v1.c"
fi

if [ -d "$PROJECT_ROOT/std" ]; then
    cp -R -X "$PROJECT_ROOT/std/." "$PAYLOAD/usr/local/share/yaza/std/"
fi

if [ -d "$PROJECT_ROOT/templates" ]; then
    cp -R -X "$PROJECT_ROOT/templates/." "$PAYLOAD/usr/local/share/yaza/templates/"
fi

if [ -n "$ARGON2_PREFIX" ] && [ -f "$ARGON2_PREFIX/include/argon2.h" ]; then
    install -d "$PAYLOAD/usr/local/share/yaza/providers/argon2/include" "$PAYLOAD/usr/local/share/yaza/providers/argon2/lib"
    install -m 644 "$ARGON2_PREFIX/include/argon2.h" "$PAYLOAD/usr/local/share/yaza/providers/argon2/include/argon2.h"
    if [ -f "$ARGON2_PREFIX/lib/libargon2.a" ]; then
        install -m 644 "$ARGON2_PREFIX/lib/libargon2.a" "$PAYLOAD/usr/local/share/yaza/providers/argon2/lib/libargon2.a"
    fi
    if [ -f "$ARGON2_PREFIX/LICENSE" ]; then
        install -m 644 "$ARGON2_PREFIX/LICENSE" "$PAYLOAD/usr/local/share/yaza/providers/argon2/LICENSE"
    fi
fi

dot_clean -m "$PAYLOAD" 2>/dev/null || true
find "$PAYLOAD" -name '._*' -type f -delete 2>/dev/null || true
if command -v xattr >/dev/null 2>&1; then
    xattr -cr "$PAYLOAD" 2>/dev/null || true
fi

PKG_SCRIPTS="$STAGE_DIR/scripts"
install -d "$PKG_SCRIPTS"
cat <<'EOF' > "$PKG_SCRIPTS/postinstall"
#!/bin/sh
printf '\n'
printf '  ███▄      ▄███    Welcome to YAZA Programming Language!\n'
printf '   ▀███▄  ▄███▀     Version 1.0.0 · Zero-GC Arena · Native LLVM · YazaBase\n'
printf '     ▀███▄ ▀█▀\n'
printf '     ▄███▀          Installed CLI tools to: /usr/local/bin/yaza & yazabase\n'
printf '   ▄███▀            Docs: https://yazastudios.id/docs/\n\n'
exit 0
EOF
chmod 755 "$PKG_SCRIPTS/postinstall"

pkgbuild \
    --root "$PAYLOAD" \
    --scripts "$PKG_SCRIPTS" \
    --identifier "org.yaza-lang.cli" \
    --version "$VERSION" \
    --install-location "/" \
    "$PACKAGE"

# Copy to website releases when running in workspace with website/
if [ -d "$PROJECT_ROOT/../website/public" ]; then
    mkdir -p "$PROJECT_ROOT/../website/public/releases"
    cp "$PACKAGE" "$PROJECT_ROOT/../website/public/releases/"
fi

printf 'Successfully built macOS PKG installer: %s\n' "$PACKAGE"

#!/bin/sh
# Development-only source watcher. The compiled application always runs as a child process.
set -u

if [ "$#" -lt 3 ]; then
    printf 'Usage: watch-run.sh <yaza-bin> <source.yz> <-O0|-O2> [app args...]\n' >&2
    exit 2
fi
compiler=$1
source=$2
mode=$3
shift 3

watch_dir=$(mktemp -d "${TMPDIR:-/tmp}/yaza-watch.XXXXXX") || exit 1
child_pid=
generation=0

stop_child() {
    if [ -n "$child_pid" ]; then
        kill -TERM "$child_pid" 2>/dev/null || :
        wait "$child_pid" 2>/dev/null || :
        child_pid=
    fi
}

cleanup() {
    trap - EXIT INT TERM
    stop_child
    rm -rf -- "$watch_dir"
}
trap 'exit 130' INT
trap 'exit 143' TERM
trap cleanup EXIT

snapshot() {
    {
        find . \( -name .git -o -name .yaza -o -name build -o -name dist -o -name data -o -name node_modules \) -prune -o \
            -type f \( -name '*.yz' -o -name 'yaza.toml' \) -print
        printf '%s\n' "$source"
    } | LC_ALL=C sort -u | while IFS= read -r path; do
        if [ -f "$path" ]; then cksum "$path" 2>/dev/null || :; fi
    done | cksum
}

rebuild() {
    generation=$((generation + 1))
    next="$watch_dir/app-$generation"
    printf '\nYAZA Watch  |  compiling change %s...\n' "$generation"
    if [ "$mode" = '-O2' ]; then
        if ! "$compiler" build --release "$source" -o "$next"; then
            printf 'YAZA Watch  |  build failed; previous app is still running\n' >&2
            return 1
        fi
    else
        if ! "$compiler" build "$source" -o "$next"; then
            printf 'YAZA Watch  |  build failed; previous app is still running\n' >&2
            return 1
        fi
    fi
    stop_child
    printf 'YAZA Watch  |  running change %s; watching .yz and yaza.toml\n\n' "$generation"
    "$next" "$@" &
    child_pid=$!
    return 0
}

last_snapshot=$(snapshot)
rebuild "$@" || :
while :; do
    sleep 0.5
    current_snapshot=$(snapshot)
    if [ "$current_snapshot" != "$last_snapshot" ]; then
        # Coalesce consecutive editor writes before compiling.
        sleep 0.3
        last_snapshot=$(snapshot)
        rebuild "$@" || :
    fi
done

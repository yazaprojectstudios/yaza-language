#!/bin/sh
set -eu

usage() {
    printf '%s\n' 'Usage: c-to-llvm-ir.sh [--bitcode] <input.c> [output.ll|output.bc]' >&2
    exit 2
}

format=ir
if [ "${1-}" = --bitcode ]; then
    format=bitcode
    shift
fi
[ "$#" -ge 1 ] && [ "$#" -le 2 ] || usage

input=$1
[ -f "$input" ] || {
    printf 'Input C source does not exist: %s\n' "$input" >&2
    exit 2
}

if [ "$#" -eq 2 ]; then
    output=$2
else
    case "$input" in
        *.c) output=${input%.c} ;;
        *) output=$input ;;
    esac
    if [ "$format" = bitcode ]; then
        output=$output.bc
    else
        output=$output.ll
    fi
fi

compiler=${CLANG:-clang}
if [ "$format" = bitcode ]; then
    "$compiler" -c -emit-llvm "$input" -o "$output"
else
    "$compiler" -S -emit-llvm "$input" -o "$output"
fi
printf 'LLVM %s written to %s\n' "$format" "$output"

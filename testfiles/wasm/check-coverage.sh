#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
#
# The completeness gate: cross-reference the published interface against what the host
# actually implements and what the conformance suite actually exercises.
#
# This exists because the interface is the promise. An IDL that documents more than the host
# provides is worse than no IDL at all -- a plugin author reads an operation, writes against
# it, and finds out at run time. And an operation that is implemented but never exercised is
# only assumed to work.
#
# Three lists, two comparisons:
#   IDL         what is promised
#   host table  what exists
#   .wat suite  what is proven
#
# Usage: check-coverage.sh [path-to-idl] [path-to-wasm-backend.cpp]

here=$(cd "$(dirname "$0")" && pwd)
idl=${1:-$here/inkscape-plugin-1.0.idl}
backend=${2:-$here/../../src/extension/implementation/wasm-backend.cpp}

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# Operation names the IDL declares: attributes and methods, minus constants and dictionaries.
sed -n 's/^[[:space:]]*\(readonly \)\?attribute [A-Za-z:?<> ]* \([a-zA-Z]*\);.*/\2/p' "$idl" > "$work/idl"
sed -n 's/^[[:space:]]*[A-Za-z:?<> ]* \([a-zA-Z]*\)(.*/\1/p' "$idl" >> "$work/idl"
sort -u "$work/idl" -o "$work/idl"

# Names the host registers.
sed -n 's/^[[:space:]]*{"org\.inkscape\.[A-Za-z]*",[[:space:]]*"\([a-zA-Z]*\)".*/\1/p' "$backend" | sort -u > "$work/host"

# Names the suite imports.
cat "$here"/*.wat 2>/dev/null |
    sed -n 's/.*(import "org\.inkscape\.[A-Za-z]*"[[:space:]]*"\([a-zA-Z]*\)".*/\1/p' | sort -u > "$work/suite"

missing_host=$(comm -23 "$work/idl" "$work/host")
missing_suite=$(comm -23 "$work/host" "$work/suite")

printf 'declared in IDL   %3d\n' "$(wc -l < "$work/idl")"
printf 'implemented       %3d\n' "$(wc -l < "$work/host")"
printf 'exercised         %3d\n' "$(wc -l < "$work/suite")"
echo

if [ -n "$missing_host" ]; then
    echo "NOT IMPLEMENTED (declared, but the host has no entry):"
    echo "$missing_host" | sed 's/^/  /'
    echo
fi

if [ -n "$missing_suite" ]; then
    echo "NOT EXERCISED (implemented, but no .wat imports it):"
    echo "$missing_suite" | sed 's/^/  /'
    echo
fi

[ -z "$missing_host" ] && [ -z "$missing_suite" ] && echo "complete: every declared operation is implemented and exercised"
[ -z "$missing_host" ] && [ -z "$missing_suite" ]

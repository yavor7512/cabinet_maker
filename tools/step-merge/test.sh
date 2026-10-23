#!/usr/bin/env bash
set -euo pipefail
bin="$1"
input="$2"
dir=$(mktemp -d)
trap 'rm -rf "$dir"' EXIT
"$bin" --output "$dir/one.step" "$input" > "$dir/log"
"$bin" --output "$dir/repeated.step" "$input" "$input" >> "$dir/log"
"$bin" --output "$dir/nested.step" "$dir/one.step" "$dir/repeated.step" >> "$dir/log"
"$bin" --force --output "$dir/one.step" "$input" >> "$dir/log"
if "$bin" --output "$dir/one.step" "$input" >> "$dir/log" 2>&1; then exit 1; fi
if "$bin" --force --output "$dir/one.step" "$dir/one.step" >> "$dir/log" 2>&1; then exit 1; fi
if "$bin" --output "$dir/bad.step" "$dir/missing.step" >> "$dir/log" 2>&1; then exit 1; fi
if "$bin" --output "$dir/empty.step" >> "$dir/log" 2>&1; then exit 1; fi
printf 'not a STEP file\n' > "$dir/invalid.step"
if "$bin" --output "$dir/bad.step" "$dir/invalid.step" >> "$dir/log" 2>&1; then exit 1; fi
printf 'Passed: single, repeated, nested inputs, replacement, and rejection checks.\n'

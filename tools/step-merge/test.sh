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
mkdir "$dir/one.step.merging.0"
printf 'leave this slot alone\n' > "$dir/one.step.merging.0/keep.txt"
"$bin" --force --output "$dir/one.step" "$input" >> "$dir/log"
[[ -f "$dir/one.step.merging.0/keep.txt" && ! -e "$dir/one.step.merging.1" ]] || exit 1
mkdir "$dir/blocked.step"
printf 'preserve destination\n' > "$dir/blocked.step/keep.txt"
if "$bin" --force --output "$dir/blocked.step" "$input" >> "$dir/log" 2>&1; then exit 1; fi
[[ -f "$dir/blocked.step/keep.txt" && ! -e "$dir/blocked.step.merging.0" ]] || exit 1
if "$bin" --output "$dir/one.step" "$input" >> "$dir/log" 2>&1; then exit 1; fi
if "$bin" --force --output "$dir/one.step" "$dir/one.step" >> "$dir/log" 2>&1; then exit 1; fi
if "$bin" --output "$dir/bad.step" "$dir/missing.step" >> "$dir/log" 2>&1; then exit 1; fi
if "$bin" --output "$dir/empty.step" >> "$dir/log" 2>&1; then exit 1; fi
printf 'not a STEP file\n' > "$dir/invalid.step"
if "$bin" --output "$dir/bad.step" "$dir/invalid.step" >> "$dir/log" 2>&1; then exit 1; fi
if [[ $# -ge 3 ]]; then
  "$3" --write-face "$dir/face.step"
  if "$bin" --output "$dir/face-merged.step" "$dir/face.step" >> "$dir/log" 2>&1; then exit 1; fi
  if [[ -e "$dir/face-merged.step" ]]; then exit 1; fi
  "$3" --write-roles "$dir/roles.step"
  "$bin" --output "$dir/roles-merged.step" "$dir/roles.step" >> "$dir/log"
  "$3" --check-roles "$dir/roles-merged.step"
fi
printf 'Passed: single, repeated, nested inputs, replacement, and rejection checks.\n'

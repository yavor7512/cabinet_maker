#!/usr/bin/env bash
# Exercise the public Make interface with real tools and fresh build trees.
set -euo pipefail
root=${1:?Project folder required}
make_bin=${2:-make}
work=$(mktemp -d "${TMPDIR:-/tmp}/drawer-make-tests.XXXXXX")
trap 'rm -rf "$work"' EXIT
# Do not inherit the calling make's command-line overrides or jobserver.
unset MAKEFLAGS MFLAGS MAKEOVERRIDES MAKELEVEL

invoke() {
  "$make_bin" --no-print-directory -C "$root" \
    "BUILD_DIR=$case_dir/drawer build" \
    "MERGE_BUILD_DIR=$case_dir/merge build" \
    "CUTLIST_BUILD_DIR=$case_dir/cutlist build" \
    "OUTPUT_DIR=$case_dir/generated output" JOBS=2 "$@"
}
fail() {
  printf 'FAIL: %s\n' "$*" >&2
  tail -n 80 "$log" >&2
  exit 1
}
require_file() { [[ -s "$1" ]] || fail "Missing or empty file: $1"; }
expect_failure() {
  if invoke "$@" > "$log" 2>&1; then fail "Unexpected success: $*"; fi
}

for target in help clean all lower upper carcass carcase assemblies assembly models merged autocad packet \
              project build drawer-build merge-build cutlist-build test drawer-test cutlist-test merge-test regression-test; do
  case_dir="$work/$target case"
  mkdir -p "$case_dir"
  log="$case_dir/run.log"
  # These include both previously reported invocations. Parallel all also checks
  # that shared prerequisites run once when packet/autocad need the same models.
  args=("$target")
  case "$target" in
    all) args=(-j3 all FACE_WIDTH_MM=469.9 FACE_HEIGHT_MM=711.2) ;;
    lower) args=(lower FACE_WIDTH_MM=469.9 FACE_HEIGHT_MM=711.2 LOWER_POSITION_Y_MM=12) ;;
    assembly) args=(assembly ASSEMBLY=carcass) ;;
  esac
  invoke "${args[@]}" > "$log" 2>&1 || fail "$target from empty folders"
  out="$case_dir/generated output"
  case "$target" in
    all|lower|upper|carcass|carcase|assemblies|assembly|models|merged|autocad|packet|project|build|drawer-build|test|drawer-test|regression-test)
      require_file "$case_dir/drawer build/drawer" ;;
  esac
  case "$target" in
    all|merged|autocad|packet|build|merge-build|test|merge-test|regression-test)
      require_file "$case_dir/merge build/step-merge" ;;
  esac
  case "$target" in
    all|packet|build|cutlist-build|test|cutlist-test|regression-test)
      require_file "$case_dir/cutlist build/step-cutlist" ;;
  esac
  case "$target" in
    all|packet) require_file "$out/cabinet-shop-packet/shop-packet.pdf" ;;
  esac
  case "$target" in
    all|packet|merged) require_file "$out/complete-cabinet.step" ;;
  esac
  case "$target" in
    all|autocad) require_file "$out/complete-cabinet-autocad.step" ;;
  esac
  case "$target" in
    all|models|merged|autocad|packet)
      require_file "$out/inner-drawer-477x523x102.step"
      require_file "$out/tall-front-bottom-aligned.step"
      require_file "$out/cabinet-carcass.step" ;;
    lower) require_file "$out/tall-front-bottom-aligned.step" ;;
    upper) require_file "$out/inner-drawer-477x523x102.step" ;;
    carcass|carcase|assembly) require_file "$out/cabinet-carcass.step" ;;
    *) [[ ! -e "$out" ]] || fail "$target wrote production outputs" ;;
  esac
  case "$target" in
    lower|upper|carcass|carcase|assemblies|assembly|models|project|drawer-build|drawer-test)
      [[ ! -e "$case_dir/merge build" && ! -e "$case_dir/cutlist build" ]] || fail "$target built unrelated tools" ;;
  esac
  printf 'PASS: %s from empty folders\n' "$target"
done

case_dir="$work/all case"
log="$case_dir/failure.log"
pdf="$case_dir/generated output/cabinet-shop-packet/shop-packet.pdf"
before=$(cksum < "$pdf")
# A project path containing spaces and legacy Make overrides must work without
# another build. Unknown explicit project overrides must preserve existing STEP.
cp "$case_dir/drawer build/default-project.json" "$case_dir/custom design.json"
invoke lower SKIP_BUILD=1 "PROJECT_FILE=$case_dir/custom design.json" \
  FACE_WIDTH_MM=469.9 FACE_HEIGHT_MM=711.2 LOWER_POSITION_Y_MM=12 > "$log" 2>&1 || fail 'Custom project path'
cp -R "$root/designs" "$case_dir/custom designs"
invoke lower SKIP_BUILD=1 "PROJECT_FILE=$case_dir/custom designs/cabinet.jsonnet" > "$log" 2>&1 || fail 'Jsonnet project/import paths with spaces'
step_before=$(cksum < "$case_dir/generated output/tall-front-bottom-aligned.step")
project_before=$(cksum < "$case_dir/drawer build/project.json")
expect_failure lower SKIP_BUILD=1 JSONNET=/usr/bin/false
[[ $(cksum < "$case_dir/drawer build/project.json") == "$project_before" ]] || fail 'Failed Jsonnet replaced expanded project'
[[ $(cksum < "$case_dir/generated output/tall-front-bottom-aligned.step") == "$step_before" ]] || fail 'Failed Jsonnet replaced STEP'
printf '{ invalid jsonnet\n' > "$case_dir/custom designs/broken.jsonnet"
expect_failure lower SKIP_BUILD=1 "PROJECT_FILE=$case_dir/custom designs/broken.jsonnet"
[[ $(cksum < "$case_dir/drawer build/project.json") == "$project_before" ]] || fail 'Invalid Jsonnet replaced expanded project'
[[ $(cksum < "$case_dir/generated output/tall-front-bottom-aligned.step") == "$step_before" ]] || fail 'Invalid Jsonnet replaced STEP'
expect_failure lower SKIP_BUILD=1 'PROJECT_OPTIONS=--set MISSING_PARAMETER=10'
[[ $(cksum < "$case_dir/generated output/tall-front-bottom-aligned.step") == "$step_before" ]] || fail 'Invalid project replaced existing STEP'
printf 'PASS: JSON/Jsonnet paths, imports and overrides; failed expansion preserves configuration and STEP\n'
expect_failure packet SKIP_BUILD=1 CUTLIST_BIN=/usr/bin/false
[[ $(cksum < "$pdf") == "$before" ]] || fail 'Failed planner replaced the existing PDF'
printf 'PASS: failed planning preserves the existing PDF and returns failure\n'

# A new assembly and filename are discovered from configuration alone. Full
# generation/merging must not depend on the production upper/lower/carcass IDs.
standalone="$root/tests/standalone.jsonnet"
invoke all SKIP_BUILD=1 "PROJECT_FILE=$standalone" "OUTPUT_DIR=$case_dir/standalone output" > "$log" 2>&1 || fail 'Generic full project generation'
require_file "$case_dir/standalone output/display shelf.step"
require_file "$case_dir/standalone output/complete-cabinet.step"
require_file "$case_dir/standalone output/complete-cabinet-autocad.step"
require_file "$case_dir/standalone output/cabinet-shop-packet/shop-packet.pdf"
[[ ! -e "$case_dir/standalone output/cabinet-carcass.step" ]] || fail 'Generic generation created a built-in carcass'
invoke assembly SKIP_BUILD=1 "PROJECT_FILE=$standalone" ASSEMBLY=shelf \
  "ASSEMBLY_OUTPUT=$case_dir/single shelf.step" > "$log" 2>&1 || fail 'Generic assembly and explicit filename'
require_file "$case_dir/single shelf.step"
invoke assemblies SKIP_BUILD=1 "PROJECT_FILE=$standalone" > "$log" 2>&1 || fail 'Generic assembly discovery'
[[ $(cat "$log") == *'Étagère shelf'* ]] || fail 'Configured assembly was not listed'
# The failed packet command above regenerated model files before planning
# failed, so snapshot the current STEP immediately before testing preservation.
step_before=$(cksum < "$case_dir/generated output/tall-front-bottom-aligned.step")
expect_failure assembly SKIP_BUILD=1
expect_failure assembly SKIP_BUILD=1 ASSEMBLY=missing "ASSEMBLY_OUTPUT=$case_dir/generated output/tall-front-bottom-aligned.step"
[[ $(cksum < "$case_dir/generated output/tall-front-bottom-aligned.step") == "$step_before" ]] || fail 'Invalid assembly selection replaced STEP'
printf 'PASS: configuration-only discovery, selection, filenames and full-project generation\n'

mkdir -p "$case_dir/unrecognized" "$case_dir/generated output/archive"
printf 'preserve me\n' > "$case_dir/unrecognized/sentinel"
printf 'archive\n' > "$case_dir/generated output/archive/sentinel"
expect_failure clean "CUTLIST_BUILD_DIR=$case_dir/unrecognized"
require_file "$case_dir/unrecognized/sentinel"
require_file "$case_dir/drawer build/drawer"
require_file "$case_dir/merge build/step-merge"
expect_failure clean "BUILD_DIR=$root"
expect_failure clean "BUILD_DIR=$case_dir/generated output"
ln -s "$case_dir/drawer build" "$case_dir/build link"
expect_failure clean "BUILD_DIR=$case_dir/build link"
expect_failure clean lower
printf 'PASS: clean rejects unsafe paths and mixed build/clean goals before deleting anything\n'

invoke clean > "$log" 2>&1 || fail clean
for name in drawer merge cutlist; do
  [[ ! -e "$case_dir/$name build" ]] || fail "clean left $name build"
done
[[ $(cksum < "$pdf") == "$before" ]] || fail 'clean changed the PDF'
require_file "$case_dir/generated output/tall-front-bottom-aligned.step"
require_file "$case_dir/generated output/archive/sentinel"
invoke clean > "$log" 2>&1 || fail 'Repeated clean'
expect_failure lower SKIP_BUILD=1
printf 'PASS: clean removes all build trees, is repeatable and preserves deliverables/archives\n'
printf 'PASS: missing tools fail clearly when SKIP_BUILD=1\n'

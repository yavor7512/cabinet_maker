# Run from this project: make, make models, make test, or make help.
# Override settings with make NAME=value. All dimensions are millimetres.
.DEFAULT_GOAL := all
SHELL := /bin/bash
# macOS ships GNU Make 3.81, which ignores .SHELLFLAGS. Multi-command
# recipes explicitly enable strict Bash error handling instead.
.DELETE_ON_ERROR:
.SUFFIXES:

# Cleaning while another goal builds can delete files that goal is using.
ifneq ($(filter clean,$(MAKECMDGOALS)),)
ifneq ($(words $(MAKECMDGOALS)),1)
$(error Run 'make clean' separately, then run the desired build target)
endif
endif

ROOT := $(CURDIR)
BUILD_DIR ?= $(ROOT)/build
OUTPUT_DIR ?= $(ROOT)/generated
CMAKE ?= cmake
CTEST ?= ctest
JSONNET ?= jsonnet
OPENCASCADE_PREFIX ?= $(firstword $(wildcard /opt/homebrew/opt/opencascade /usr/local/opt/opencascade))
BUILD_TYPE ?= Release
JOBS ?= 4
DRAWER_BIN ?= $(BUILD_DIR)/drawer
MERGE_BUILD_DIR ?= $(ROOT)/tools/step-merge/build
CUTLIST_BUILD_DIR ?= $(ROOT)/tools/step-cutlist/build
MERGE_BIN ?= $(MERGE_BUILD_DIR)/step-merge
CUTLIST_BIN ?= $(CUTLIST_BUILD_DIR)/step-cutlist
SKIP_BUILD ?= 0

# Project geometry and construction live in this file. Only explicitly supplied
# Make/environment values override its parameters; no second set of size defaults.
PROJECT_FILE ?= $(ROOT)/designs/cabinet.jsonnet
EXPANDED_PROJECT := $(BUILD_DIR)/project.json
PROJECT_OPTIONS ?=
ASSEMBLY ?=
ASSEMBLY_OUTPUT ?=
PROJECT_VARIABLES := WALL_THICKNESS_MM BOTTOM_THICKNESS_MM DADO_DEPTH_MM BOTTOM_OFFSET_MM DADO_CLEARANCE_MM \
 BOX_WIDTH_MM BOX_DEPTH_MM UPPER_HEIGHT_MM LOWER_HEIGHT_MM \
 LOWER_POSITION_X_MM LOWER_POSITION_Y_MM LOWER_POSITION_Z_MM \
 UPPER_POSITION_X_MM UPPER_POSITION_Y_MM UPPER_POSITION_Z_MM \
 FACE_WIDTH_MM FACE_HEIGHT_MM FACE_THICKNESS_MM FACE_TOP_Z_MM FACE_OFFSET_X_MM FACE_OFFSET_Y_MM \
 OPENING_WIDTH_MM OPENING_HEIGHT_MM CABINET_DEPTH_MM CARCASS_THICKNESS_MM \
 STILE_WIDTH_MM STILE_THICKNESS_MM FRAME_FRONT_Y_MM CARCASS_POSITION_X_MM CARCASS_POSITION_Z_MM \
 SCREWS_PER_CORNER SCREW_LENGTH_MM SCREW_DIAMETER_MM HEAD_DIAMETER_MM HEAD_HEIGHT_MM \
 TIP_LENGTH_MM POCKET_DIAMETER_MM POCKET_ANGLE_DEG SEAT_DISTANCE_MM \
 SCREW_LOWER_MM SCREW_UPPER_MM SCREW_SPACING_MM POCKET_CLEARANCE_MM SCREW_TIP_CLEARANCE_MM \
 UPPER_BOARD_START LOWER_BOARD_START CARCASS_BOARD_START
supplied = $(filter-out undefined default,$(origin $(1)))
project_override = $(if $(call supplied,$(1)),--set "$(1)=$($(1))")

# Compatibility with previous Make overrides. Explicit primary parameters below
# win over these aliases. Their values remain expressions, resolved by the model.
PROJECT_ALIASES = $(if $(call supplied,INTERIOR_WIDTH_MM),--set "BOX_WIDTH_MM=($(INTERIOR_WIDTH_MM))+2*WALL_THICKNESS_MM") \
 $(if $(call supplied,INTERIOR_DEPTH_MM),--set "BOX_DEPTH_MM=($(INTERIOR_DEPTH_MM))+2*WALL_THICKNESS_MM") \
 $(if $(call supplied,UPPER_USABLE_HEIGHT_MM),--set "UPPER_HEIGHT_MM=($(UPPER_USABLE_HEIGHT_MM))+BOTTOM_THICKNESS_MM+BOTTOM_OFFSET_MM") \
 $(if $(call supplied,LOWER_USABLE_HEIGHT_MM),--set "LOWER_HEIGHT_MM=($(LOWER_USABLE_HEIGHT_MM))+BOTTOM_THICKNESS_MM+BOTTOM_OFFSET_MM") \
 $(if $(call supplied,FACE_BOTTOM_OVERLAY_MM),--set "FACE_TOP_Z_MM=FACE_HEIGHT_MM-($(FACE_BOTTOM_OVERLAY_MM))") \
 $(if $(call supplied,FACE_OFFSET_MM),--set "FACE_TOP_Z_MM=lower_box.center_z+($(FACE_OFFSET_MM))+FACE_HEIGHT_MM/2")
PROJECT_ARGS = --project "$(EXPANDED_PROJECT)" $(PROJECT_ALIASES) $(foreach var,$(PROJECT_VARIABLES),$(call project_override,$(var))) $(PROJECT_OPTIONS)

# Cutting-plan and export settings are independent of cabinet construction.
SHEETS ?= 5x5
KERF_MM ?= 5
WORKFLOW ?= shop
EXTRA_AREA_PERCENT ?= 35
SHOP_ATTEMPTS ?= 64
COLOR ?= \#A8793E
PACKET_OPTIONS ?=

# Export filenames and merge order come from the selected configuration.
ASSEMBLY_DESTINATION = $(if $(ASSEMBLY_OUTPUT),--output "$(ASSEMBLY_OUTPUT)",--output-dir "$(OUTPUT_DIR)")
.PHONY: all build drawer-build merge-build cutlist-build models upper lower carcass carcase merged autocad packet
.PHONY: assemblies assembly
.PHONY: project test drawer-test cutlist-test merge-test regression-test test-make clean help
all: packet autocad
build: drawer-build merge-build cutlist-build

# Each public generation/test target depends directly on the tools it needs.
# Always configure before building: this also supports an empty build directory.
# CMake owns incremental source/header dependency tracking.
ifeq ($(SKIP_BUILD),1)
drawer-build:
	@test -x "$(DRAWER_BIN)" || { printf '%s\n' 'SKIP_BUILD=1 requires an existing DRAWER_BIN; run make drawer-build SKIP_BUILD=0.' >&2; exit 1; }
merge-build:
	@test -x "$(MERGE_BIN)" || { printf '%s\n' 'SKIP_BUILD=1 requires an existing MERGE_BIN; run make merge-build SKIP_BUILD=0.' >&2; exit 1; }
cutlist-build:
	@test -x "$(CUTLIST_BIN)" || { printf '%s\n' 'SKIP_BUILD=1 requires an existing CUTLIST_BIN; run make cutlist-build SKIP_BUILD=0.' >&2; exit 1; }
else
drawer-build:
	"$(CMAKE)" -S "$(ROOT)" -B "$(BUILD_DIR)" -DCMAKE_PREFIX_PATH="$(OPENCASCADE_PREFIX)" -DCMAKE_BUILD_TYPE="$(BUILD_TYPE)" -DBUILD_TESTING=ON -DJSONNET_EXECUTABLE:STRING="$(JSONNET)"
	"$(CMAKE)" --build "$(BUILD_DIR)" --parallel "$(JOBS)"
merge-build:
	"$(CMAKE)" -S "$(ROOT)/tools/step-merge" -B "$(MERGE_BUILD_DIR)" -DCMAKE_PREFIX_PATH="$(OPENCASCADE_PREFIX)" -DCMAKE_BUILD_TYPE="$(BUILD_TYPE)"
	"$(CMAKE)" --build "$(MERGE_BUILD_DIR)" --parallel "$(JOBS)"
cutlist-build:
	"$(CMAKE)" -S "$(ROOT)/tools/step-cutlist" -B "$(CUTLIST_BUILD_DIR)" -DCMAKE_PREFIX_PATH="$(OPENCASCADE_PREFIX)" -DCMAKE_BUILD_TYPE="$(BUILD_TYPE)" -DBUILD_TESTING=ON
	"$(CMAKE)" --build "$(CUTLIST_BUILD_DIR)" --parallel "$(JOBS)"
endif

# Intentional phony generation: changing any setting always refreshes geometry.
# CMake still rebuilds C++ incrementally. Prerequisites make `make -j` safe.
models: project
	"$(DRAWER_BIN)" $(PROJECT_ARGS) --all --output-dir "$(OUTPUT_DIR)"
project: drawer-build
	"$(CMAKE)" -DINPUT="$(PROJECT_FILE)" -DOUTPUT="$(EXPANDED_PROJECT)" \
	 -DJSONNET_EXECUTABLE="$(JSONNET)" -P "$(ROOT)/cmake/ExpandProject.cmake"
assemblies: project
	"$(DRAWER_BIN)" $(PROJECT_ARGS) --list-assemblies
assembly: project
	@test -n "$(ASSEMBLY)" || { printf '%s\n' 'Set ASSEMBLY to a configured ID; use make assemblies to list them.' >&2; exit 1; }
	"$(DRAWER_BIN)" $(PROJECT_ARGS) --assembly "$(ASSEMBLY)" $(ASSEMBLY_DESTINATION)
# Cabinet shortcuts select configured assemblies and preserve CAD import filenames.
upper: project
	@mkdir -p "$(OUTPUT_DIR)"
	"$(DRAWER_BIN)" $(PROJECT_ARGS) --assembly upper --output-dir "$(OUTPUT_DIR)"
lower: project
	@mkdir -p "$(OUTPUT_DIR)"
	"$(DRAWER_BIN)" $(PROJECT_ARGS) --assembly lower --output-dir "$(OUTPUT_DIR)"
carcass: project
	@mkdir -p "$(OUTPUT_DIR)"
	"$(DRAWER_BIN)" $(PROJECT_ARGS) --assembly carcass --output-dir "$(OUTPUT_DIR)"
carcase: carcass
# Capture the list before merging so a failed query cannot produce a partial merge.
define merge_assemblies
	@set -euo pipefail; \
	 outputs=$$("$(DRAWER_BIN)" $(PROJECT_ARGS) --list-outputs --output-dir "$(OUTPUT_DIR)"); \
	 inputs=(); while IFS= read -r file; do inputs+=("$$file"); done <<< "$$outputs"; \
	 "$(MERGE_BIN)" --force $(1) --color '$(COLOR)' --output "$(OUTPUT_DIR)/$(2)" "$${inputs[@]}"
endef
merged: models merge-build
	$(call merge_assemblies,,complete-cabinet.step)
autocad: models merge-build
	$(call merge_assemblies,--autocad,complete-cabinet-autocad.step)
packet: merged cutlist-build
	@set -euo pipefail; \
	 tmp=$$(mktemp -d "$(OUTPUT_DIR)/.packet.XXXXXX"); \
	 trap 'rm -rf "$$tmp"' EXIT; \
	 "$(CUTLIST_BIN)" --out "$$tmp" --sheets "$(SHEETS)" --kerf "$(KERF_MM)" \
	 --workflow "$(WORKFLOW)" --extra-area-percent "$(EXTRA_AREA_PERCENT)" --shop-attempts "$(SHOP_ATTEMPTS)" \
	 $(PACKET_OPTIONS) "$(OUTPUT_DIR)/complete-cabinet.step"; \
	 mkdir -p "$(OUTPUT_DIR)/cabinet-shop-packet"; \
	 mv -f "$$tmp/shop-packet.pdf" "$(OUTPUT_DIR)/cabinet-shop-packet/shop-packet.pdf"; \
	 if [[ -d "$$tmp/details" ]]; then \
	   dest=$$(mktemp -d "$(OUTPUT_DIR)/cabinet-shop-packet/details.XXXXXX"); \
	   mv "$$tmp/details" "$$dest/export"; \
	 fi

# Test suites are independent of generated production models and each other.
test: drawer-test cutlist-test merge-test regression-test
drawer-test: drawer-build
	"$(CTEST)" --test-dir "$(BUILD_DIR)" --output-on-failure --no-tests=error
cutlist-test: cutlist-build
	"$(CTEST)" --test-dir "$(CUTLIST_BUILD_DIR)" --output-on-failure --no-tests=error
merge-test: merge-build
	bash "$(ROOT)/tools/step-merge/test.sh" "$(MERGE_BIN)" "$(ROOT)/tests/fixtures/pre-config/cabinet-carcass.step" "$(MERGE_BUILD_DIR)/check-orientation"

# Fixed artifacts captured before the first configuration refactor, independent
# of current construction recipes and the geometry engine.
regression-test: build
	"$(CMAKE)" -DDRAWER="$(DRAWER_BIN)" -DCOMPARE="$(BUILD_DIR)/project_tests" \
	 -DMERGE="$(MERGE_BIN)" -DCUTLIST="$(CUTLIST_BIN)" -DJSONNET_EXECUTABLE="$(JSONNET)" \
	 -DROOT="$(ROOT)" -DOUTPUT="$(BUILD_DIR)/output-regression" \
	 -P "$(ROOT)/tests/output-regression.cmake"

# Full cold-start regression matrix; deliberately separate from fast make test.
test-make:
	bash "$(ROOT)/tests/make-targets.sh" "$(ROOT)" "$(MAKE)"

# Validate all paths before removing any CMake build tree. Generated deliverables
# and archives are kept; an unknown directory is never recursively removed.
clean:
	"$(CMAKE)" -DPROJECT_ROOT="$(ROOT)" -DDRAWER_BUILD="$(BUILD_DIR)" \
	 -DMERGE_BUILD="$(MERGE_BUILD_DIR)" -DCUTLIST_BUILD="$(CUTLIST_BUILD_DIR)" \
	 -DOUTPUT_DIR="$(OUTPUT_DIR)" -P "$(ROOT)/cmake/Clean.cmake"

help:
	@printf '%s\n' \
	 'make / make all   Build tools, generate configured assemblies, merged STEP, AutoCAD STEP and PDF.' \
	 'make upper        Build the drawer tool and generate only the upper inner drawer.' \
	 'make lower        Build the drawer tool and generate only the tall-front lower drawer.' \
	 'make carcass      Build the drawer tool and generate only the carcass.' \
	 'make carcase      Alias for make carcass.' \
	 'make assemblies   List the assembly IDs and names in PROJECT_FILE.' \
	 'make assembly ASSEMBLY=ID   Generate one assembly using its configured filename.' \
	 'ASSEMBLY_OUTPUT=/path/to/file.step overrides the generic assembly output filename.' \
	 'make models       Build the drawer tool and generate every configured assembly.' \
	 'make merged       Generate the configured assemblies and merge them into complete-cabinet.step.' \
	 'make packet       Generate models, merged STEP and shop-packet.pdf.' \
	 'make autocad      Generate models and the rotated AutoCAD export.' \
	 'make build        Configure/build all three C++ tools; no production outputs.' \
	 'make project      Build the drawer tool and expand PROJECT_FILE to BUILD_DIR/project.json.' \
	 'make drawer-build / merge-build / cutlist-build   Build one tool independently.' \
	 'make test         Build and run geometry, placement, cut-list and merger tests.' \
	 'make regression-test   Compare models, merged exports and PDF to pre-configuration fixtures.' \
	 'make drawer-test / cutlist-test / merge-test      Build and test one tool.' \
	 'make test-make    Test every public target from fresh temporary build/output folders.' \
	 'make clean        Remove the configured CMake build trees; preserve generated files and archives.' \
	 'Run clean separately: make clean && make lower (or any other target).' \
	 'All generation targets work from a clean checkout; no prior make all is needed.' \
	 'Overrides: make lower FACE_WIDTH_MM=469.9 FACE_HEIGHT_MM=711.2 LOWER_POSITION_Y_MM=12' \
	 'Dimensions/placements: designs/cabinet.jsonnet; construction recipes: designs/lib/.' \
	 'PROJECT_FILE accepts .jsonnet or expanded .json. JSONNET selects the Jsonnet executable.' \
	 'Project parameters: Make overrides win over the file; PROJECT_OPTIONS accepts additional --set options.' \
	 'The face top follows the upper box by default; FACE_TOP_Z_MM overrides that relationship.' \
	 'FACE_BOTTOM_OVERLAY_MM and FACE_OFFSET_MM remain supported as placement aliases.' \
	 'Paths with spaces: make OUTPUT_DIR="/path/to/my output"' \
	 'Build controls: JOBS=4 BUILD_TYPE=Release OPENCASCADE_PREFIX=/path/to/opencascade' \
	 'SKIP_BUILD=1 explicitly requires existing executables; missing tools fail immediately.' \
	 'Default PDF has no identification-label pages; PACKET_OPTIONS="--labels" adds them.' \
	 'PACKET_OPTIONS="--export-details" retains CSV/SVG exports alongside the PDF.' \
	 'Generated outputs refresh on every invocation; existing PDFs survive failed planning.'

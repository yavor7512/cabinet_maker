# Cabinet design and reusable construction

Edit `cabinet.jsonnet` for this cabinet's dimensions, placements, stock and
hardware. Both drawers reuse one specification. Edit the recipes in `lib/` to
change how individual boards fit together. Neither requires recompiling C++.

All lengths are millimetres. Vectors are `[X, Y, Z]`: right, rearward, up.
`size` is the exterior size; `at` is the front-left bottom corner of a drawer
box. The carcass uses the left opening edge, front of the frame and opening
bottom as its reference point.

## Generate

Install Jsonnet alongside the existing build dependencies:

```sh
brew install cmake opencascade rapidjson jsonnet
make all
make lower FACE_WIDTH_MM=469.9 FACE_HEIGHT_MM=711.2 LOWER_POSITION_Y_MM=12
make packet PROJECT_FILE="/path/to/another cabinet.jsonnet"
```

Make expands the selected Jsonnet file into `build/project.json`, then invokes
the existing OpenCascade generator. Every generation invocation expands again,
including `SKIP_BUILD=1`, so edits to imported recipes take effect immediately.
The generated JSON is an internal representation; do not maintain or edit it.
There is no checked-in `designs/cabinet.json` to keep synchronized.

`PROJECT_FILE` also accepts existing expanded `.json` projects. Relative Jsonnet
imports resolve next to the importing file. Copy `lib/` with the cabinet file
when moving a design. Paths containing spaces are supported. Set `JSONNET` to
an executable path if the tool is not on `PATH`.

Each Make target builds its own prerequisites from empty build folders. Failed
Jsonnet expansion preserves the previous expanded file and STEP outputs and
stops generation. `make clean` removes the configured build trees, including
expanded configuration, while keeping generated STEP/PDF deliverables.

To inspect resolved board envelopes without generating models:

```sh
make project
build/drawer --project build/project.json --describe
build/drawer --project build/project.json --describe --assembly lower
```

The C++ CLI accepts expanded JSON, not Jsonnet directly. CMake builds a separate
`build/default-project.json` for tests from the production design. It tracks the
cabinet file and every `.libsonnet` recipe as build dependencies.

## Reuse and construction changes

`lib/joinery.libsonnet` exposes these functions:

| Function | Construction |
| --- | --- |
| `drawer(id, settings)` | Full-depth sides, front/back between sides, captured dado bottom and four repeated pocket-screw joints. |
| `panel(id, settings)` | One board with independently specified size and axis alignments; optional projected overlap checks. |
| `carcass(id, settings)` | Full-height sides outside the opening, caps between sides and two full-height stiles. |
| `pocketScrew(id, settings)` | Reusable screw/pocket dimensions and clearances. |
| `project(components, aliases={})` | Combines instances into an expanded project and rejects duplicate names. |

The actual relationships live in `lib/drawer.libsonnet` and
`lib/carcass.libsonnet`. For example, the drawer front spans the inside faces
of its sides, and the bottom extends into each wall by the dado depth. The
screw recipe generates the four mirrored corners from one definition.

To add another drawer, call `wood.drawer` again with another ID, placement and
non-overlapping `board_start`. The same construction recipe is reused. Instance
dimensions can differ. Give a drawer's optional `attachments` list the IDs of
panels to export after its hardware; the lower drawer lists `lower.face`,
preserving the original STEP part order.

Recipes return ordinary Jsonnet objects, so a project can also extend a recipe
with `+` and override individual board relationships. See
`tests/template-cases.jsonnet` for a working external construction change that
puts carcass caps over the sides. New construction methods remain external
templates; C++ supplies the generic geometry and machining primitives.

## Relationships and overrides

References address the original rectangular blank, so machining does not move
its reference surfaces:

| Axis | Size | Minimum | Centre | Maximum |
| --- | --- | --- | --- | --- |
| X | `width` | `left_x` | `center_x` | `right_x` |
| Y | `depth` | `front_y` | `center_y` | `rear_y` |
| Z | `height` | `bottom_z` | `center_z` | `top_z` |

Use quoted expressions for model relationships, such as `'upper_box.top_z'` or
`'lower_box.center_x - carcass_opening_width / 2'`. These are resolved by the
model **after** command-line overrides. Ordinary unquoted Jsonnet arithmetic is
evaluated during configuration expansion. Supported model-expression operations
are `+`, `-`, `*`, `/`, parentheses, unary signs, `min`, `max`, `abs`, `sin_deg`
and `cos_deg`. Unknown references, cycles and invalid dimensions fail explicitly.

The tall face is 469.9 × 711.2 × 12 mm. Its rear touches the lower drawer front
at Y=12 and its top follows the upper drawer top at Z=721. To move only that
face up 25.8 mm, preserving both drawers and the Y contact:

```sh
make lower FACE_TOP_Z_MM='upper_box.top_z + 25.8'
```

Or change its `top_z` alignment to `'upper_box.top_z + 25.8'` in the design.
Its bottom then moves from Z=9.8 to Z=35.6. The face's `offset` vector provides
additional independent X/Y/Z offsets. A positive Y offset that intersects the
drawer front is rejected during geometry generation.

Existing Make variable names are mapped by `lib/make-aliases.libsonnet`. For
example, explicitly setting `BOX_WIDTH_MM` overrides both `upper_width` and
`lower_width`. Without that override, each instance retains its own default.
An alias used in an expression reads its first target. An explicitly named
instance parameter takes precedence over its shared alias:

```sh
make models BOX_WIDTH_MM=500 PROJECT_OPTIONS='--set lower_width=480'
```

Parameters are named `<instance>_<setting>`; the generated JSON lists every
available name. The Makefile forwards the established uppercase names and
`PROJECT_OPTIONS` accepts additional `--set NAME=EXPRESSION` options. Unknown
parameters fail. Repeated overrides of the same name use the last value.

`FACE_BOTTOM_OVERLAY_MM` and `FACE_OFFSET_MM` remain compatibility aliases for
face-top placement. The first puts the face bottom at the negative of its value
in world Z. The second measures from the lower box's vertical centre. An
explicit `FACE_TOP_Z_MM` wins. Interior-width/depth and usable-height aliases
also remain supported.

## Expanded JSON contract

The generic engine still accepts schema version 1, including older files without
the optional alias and ordering fields:

| Section | Contents |
| --- | --- |
| `parameters` | Numeric values and deferred expressions. |
| `parameter_aliases` | Optional alias → array of declared parameter names. Explicit overrides fan out; targets cannot belong to multiple aliases. |
| `assemblies` | Names and optional `part_order` arrays of board IDs and `@hardware`. |
| `frames` | Nonphysical reference boxes. |
| `boards` | Assembly, name, board number, size and placement. |
| `cuts` | Named rectangular cutters, targets and per-axis clearances. |
| `hardware_profiles` | Screw and pocket dimensions/clearances. |
| `hardware` | Joints, axes, spacing and obstacles. |
| `checks` | Expressions with minimum/maximum bounds and diagnostic messages. |

For each board/frame axis, provide either a size and exactly one alignment, or
a `between` pair defining both size and position. All resolved coordinates are
world coordinates; assembly membership does not add an implicit translation.
Boards default to export order by board number. Explicit `part_order` must
include every board exactly once and `@hardware` when screws are present.

The supported primitives remain axis-aligned rectangular boards, rectangular
cuts and horizontal pocket screws. Arbitrarily rotated boards or new machining
operations require extending the geometry engine.

## Regression checks

```sh
make test
make regression-test
make test-make
```

The output regression compares all three assemblies against STEP files saved
before `cabinet.json` existed, including each part's occupied material, names,
order and position. It also compares both merged exports to fixed references
assembled from those old models, and compares the entire PDF byte for byte with
the original packet. Fixtures and provenance are documented in
`tests/fixtures/pre-config/README.md`.

The other tests cover independent template instances, editable construction,
placement links, machining, alias overrides, invalid input, and all public Make
targets from empty build folders. `make test-make` is the slower cold-build suite.

# Cabinet maker — C++ / OpenCascade

Cabinet construction is defined entirely in Jsonnet: dimensions, board layout,
joints, hardware placement, assembly membership, export filenames and merge
order. C++17/OpenCascade resolves those relationships, builds and checks the
solids, and exports STEP. There are no built-in drawer or carcass generators.

The production design is [`designs/cabinet.jsonnet`](designs/cabinet.jsonnet).
Reusable recipes in [`designs/lib`](designs/lib) describe how boards fit together.
Both drawers share one recipe. See the [design guide](designs/README.md) for
construction changes, named surfaces, parameter overrides and new assemblies.

## Build and generate

Requires CMake 3.20+, a C++17 compiler, OpenCascade 7.8+ (tested with 7.9.3),
RapidJSON headers and Jsonnet (tested with 0.22.0). On macOS:

```sh
brew install cmake opencascade rapidjson jsonnet
make all
```

This builds the tools, generates every configured assembly, merges them in the
configured order, exports an AutoCAD orientation and generates the shop packet.
Every public target builds its own prerequisites; no preliminary `make all` is
needed. GNU Make 3.81 shipped with macOS is supported. Paths may contain spaces.

## Select an assembly

```sh
make assemblies                       # List IDs, names, board counts and filenames
make carcass                          # Generate the configured carcass
make carcase                          # Spelling alias for carcass
make upper
make lower
make assembly ASSEMBLY=lower           # General selection, same configured filename
make assembly ASSEMBLY=lower ASSEMBLY_OUTPUT="/tmp/my lower drawer.step"
make models                           # Generate every assembly in the configuration
```

`upper`, `lower` and `carcass` are convenience targets selecting those IDs in the
current design. They contain no construction rules. The general `assembly` target
works with any configured ID. To use another design:

```sh
make assemblies PROJECT_FILE="/path/to/design.jsonnet"
make assembly PROJECT_FILE="/path/to/design.jsonnet" ASSEMBLY=shelf
make all PROJECT_FILE="/path/to/design.jsonnet"
```

Adding a configured assembly requires no C++ or Makefile changes. An assembly's
optional `filename` defaults to `ID.step`. `assembly_order` controls generation
and merge order; if omitted, IDs are sorted alphabetically. Explicit order must
include every assembly once. Filenames must be unique plain `.step` filenames.

Other targets:

| Target | Result |
| --- | --- |
| `make project` | Expand Jsonnet to `build/project.json`. |
| `make merged` | Generate configured assemblies and `generated/complete-cabinet.step`. |
| `make autocad` | Generate assemblies and `generated/complete-cabinet-autocad.step`. |
| `make packet` | Generate assemblies, merged STEP and `generated/cabinet-shop-packet/shop-packet.pdf`. |
| `make build` | Build all three tools without generating production deliverables. |
| `make drawer-build`, `make merge-build`, `make cutlist-build` | Build one tool. |
| `make help` | List targets, paths and options. |

## Dimensions and placement

Lengths are millimetres. X is right, Y rearward and Z up. Default box dimensions
are exterior dimensions; positions refer to each box's front-left bottom corner.

| Assembly | Exterior width × depth × height | Box position X / Y / Z |
| --- | --- | --- |
| Upper drawer | 477 × 523 × 102 | 0 / 12 / 619 |
| Lower drawer box | 477 × 523 × 203 | 0 / 12 / 35 |

The applied face is 469.9 × 711.2 × 12 mm (18.5 × 28 inches wide and high).
Its rear touches the lower drawer front at Y=12, and its top follows the upper
box top at Z=721. It spans Z=9.8..721. The carcass opening is 494 × 737 mm,
overall depth 597 mm, panel thickness 18 mm and face-frame stock 38 × 18 mm.
Overall carcass size including the frame is 570 × 597 × 773 mm. These values
and construction relationships live in the configuration.

Drawer walls, bottoms and applied face use 12 mm stock. Dados and bottom offsets
are 6 mm; bottom blanks are 465 × 511 mm. The simplified screw profile is also
configured externally. Its 19.05 mm screw length and other hardware dimensions
should be checked against the purchased hardware and a test joint.

Established Make overrides remain available:

```sh
make all FACE_WIDTH_MM=469.9 FACE_HEIGHT_MM=711.2
make lower FACE_WIDTH_MM=469.9 FACE_HEIGHT_MM=711.2 LOWER_POSITION_Y_MM=12
make lower FACE_TOP_Z_MM='upper_box.top_z + 25.8'
make models BOX_WIDTH_MM=500 PROJECT_OPTIONS='--set lower_width=480'
```

The face-top example moves only the face up 25.8 mm, keeping the drawer boards
fixed. Make overrides are passed to declared configuration parameters; specific
instance parameters override shared aliases. `PROJECT_OPTIONS` accepts additional
`--set NAME=EXPRESSION` arguments. See the design guide for compatibility aliases.

The production configuration retains the existing CAD import filenames:
`inner-drawer-477x523x102.step`, `tall-front-bottom-aligned.step` and
`cabinet-carcass.step`. The lower filename is historical; it no longer describes
the face alignment. Import all models preserving their original coordinates.

## Configuration-only command line

Make expands Jsonnet; the C++ executable consumes the resulting JSON:

```sh
make project
build/drawer --project build/project.json --list-assemblies
build/drawer --project build/project.json --assembly carcass --output-dir generated
build/drawer --project build/project.json --all --output-dir generated
build/drawer --project build/project.json --describe --assembly lower
build/drawer --project build/project.json --assembly lower \
  --set FACE_TOP_Z_MM='upper_box.top_z + 25.8' --output generated/raised-face.step
```

`--describe` resolves board envelopes without generating solids. `--list-outputs`
with `--output-dir` lists configured export paths in merge order. `--help` prints
the CLI manual. The old `--model`, `--width`, `--face-*` and other standalone
construction flags have been removed; use a project and parameter overrides.

C++ provides rectangular boards/cutters, Boolean operations, horizontal pocket
screws, named assemblies, validation and STEP export/reimport. Numerical
validation tolerances stay in the engine. Cabinet sizes and board-to-board
relationships do not. Rotated boards or new machining primitives require engine
support; new arrangements of supported primitives only require configuration.
Runner/TIP-ON mechanisms, bins and carcass/face mounting hardware are not modeled.

## Shop packet

The cut-list tool reads the generated STEP solids. It uses actual modeled board
sizes and machining, rather than a separate manually maintained parts list.
`make all` uses a 5 mm maximum kerf allowance (`KERF_MM=5`). The cutting guide
identifies the subpanel and dimension to keep on the left of the blade for each
cut. The default plan uses two initial columns and 24 cuts across the two sheets.

The PDF contains material information, parts, machining flags and dimensioned
cutting layouts. Blank identification labels are omitted; every piece, including
the small pieces, has its size shown. `PACKET_OPTIONS="--labels"` adds labels;
`PACKET_OPTIONS="--export-details"` retains CSV/SVG details. Other planning options
are documented in [`tools/step-cutlist/README.md`](tools/step-cutlist/README.md).

The PDF replaces the previous packet only after successful generation. Assembly
filenames and merge order affect part numbering and are therefore configuration
choices covered by regression tests. The AutoCAD export applies one -90° X-axis
rotation to the full assembly; see the [merger guide](tools/step-merge/README.md).

## Tests and local outputs

```sh
make test              # Geometry, CLI, templates, placement, cut planning and merger
make regression-test   # Frozen models and byte-for-byte shop packet comparison
make test-make         # Every public target from fresh build/output directories
make clean             # Remove build trees, preserve deliverables and local archives
```

The [frozen test fixtures](tests/fixtures/pre-config/README.md) are independent
references captured before the configuration migration. Tests compare all 33
parts, machining, names, order, placement and both merged orientations. The PDF
must match byte for byte. No legacy construction implementation is retained as a
test oracle. The standalone shelf test verifies that an unrelated assembly can
be listed, generated and merged without special C++ or Makefile logic.

`make clean` validates all configured build paths before removing them. It rejects
source/output directories, symlinks and unrecognized folders. Run it separately:
`make clean && make lower`, not `make clean lower`.

Production outputs, intermediate files and archives are ignored by Git. Only the
six frozen STEP/PDF test inputs are tracked. Defaults use `generated/` and the
tools' `build/` directories. Keep custom output/build paths outside the checkout
or add them to `.git/info/exclude`. Older drawings and cut plans may be kept
locally; they are not repository inputs. `HELP.txt`, if generated, is ignored.

Use `OUTPUT_DIR`, `BUILD_DIR`, `MERGE_BUILD_DIR` and `CUTLIST_BUILD_DIR` to change
paths. `JOBS`, `BUILD_TYPE`, `OPENCASCADE_PREFIX` and `JSONNET` control the build.
`SKIP_BUILD=1` requires existing executables but still expands the configuration.
For direct CMake builds, set `CMAKE_PREFIX_PATH` or `OpenCASCADE_DIR` to the local
OpenCascade installation.

# Eddie drawer — C++ / OpenCascade

An open-top drawer with a separate bottom seated in dados in all four walls. The STEP file contains fourteen solids (four walls, one bottom, one applied face and eight pocket screws), in their assembled positions. Modeling and verification use C++17 and OpenCascade; no Python is used.

## Production cabinet configuration

Run `make all` to generate the current cabinet and shop packet. Its dimensions,
and placements live in the compact
[`designs/cabinet.jsonnet`](designs/cabinet.jsonnet). Reusable construction recipes
in [`designs/lib`](designs/lib) define how boards fit together, dados and screw
patterns. Both drawers reuse one recipe. No C++ rebuild is required for design changes. The Makefile
forwards explicit parameter overrides, such as `make lower FACE_WIDTH_MM=469.9`.

The tall face's top follows the upper drawer's top, and its rear touches the
lower drawer's front. The initial geometry remains identical to the previous
generator. See the [configuration guide](designs/README.md) for named surfaces,
`between` relationships, joinery, custom projects and command examples. Make
expands Jsonnet into `build/project.json` automatically; that file is generated,
not a second design to maintain. `make project` expands it for inspection.

The standalone CLI below remains available for compatibility. Its built-in
defaults apply only when running without `--project`; the Make targets use the
project configuration instead.

## Standalone CLI dimensions and defaults

| Parameter | Default (mm) | Meaning |
|---|---:|---|
| `--width` | 300 | Usable interior width (X) |
| `--depth` | 505 | Usable interior depth (Y) |
| `--height` | 70 | Usable height above the top of the bottom panel (Z) |
| `--thickness` | 12 | Wall thickness |
| `--bottom-thickness` | 12 | Bottom thickness, also the dado groove width |
| `--dado-depth` | 6 | Cut depth into the inside face of each wall |
| `--bottom-offset` | 6 | Distance from wall bottom edge to bottom panel underside |

The dado width follows the bottom thickness automatically, with exact fit and no machining clearance. The bottom offset leaves a lower lip underneath the groove; it is independent of cut depth. The dado must leave at least 0.001 mm of wall material. Numeric inputs must be finite. Positive lengths default to the range 0.001–1,000,000 mm; face offsets are signed and clearances may be zero. See --help for configurable limits.

Exterior dimensions are `(width + 2 × wall thickness)` × `(depth + 2 × wall thickness)` × `(height + bottom thickness + bottom offset)`. Defaults produce **324 × 529 × 88 mm**. Bottom panel size is `(width + 2 × dado depth)` × `(depth + 2 × dado depth)` × `bottom thickness`: **312 × 517 × 12 mm** by default.

The side walls run the full outside depth; front and rear walls fit between them. The side grooves stop at the panel's ends, while front/rear grooves span their full width. Wall corners use plain butt joints. No runners, handle, face-mounting fasteners or manufacturing clearances are modeled. The exterior bottom corner is the origin.

## Build

Requires CMake 3.20+, a C++17 compiler, OpenCascade 7.8+ (tested with 7.9.3), the header-only RapidJSON library, and Jsonnet (tested with 0.22.0).

```sh
brew install cmake opencascade rapidjson jsonnet
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix opencascade)"
cmake --build build -j
```

On other systems, set `CMAKE_PREFIX_PATH` to the OpenCascade installation prefix or `OpenCASCADE_DIR` to the directory containing `OpenCASCADEConfig.cmake`.

## Generate and test

```sh
./build/drawer --output drawer.step
./build/drawer --width 300 --depth 505 --height 70 --thickness 12 --bottom-thickness 6 --dado-depth 4 --bottom-offset 6 --output custom.step
./build/drawer --help
ctest --test-dir build --output-on-failure
```

Every flag is optional. Edit the `constexpr` values in `src/settings.hpp` and rebuild to change permanent defaults; command-line arguments override them. Output directories are created as needed. Existing output files are overwritten.

Before hardware is added, generation validates five wooden solids, analytic panel volumes and exterior bounds. After adding hardware it checks screw containment in the wall envelope, clearance from the bottom, seating in the pocket and pairwise component overlap. STEP reimport checks validity, the expected solid count (by default fourteen, or thirteen with --no-face), each component volume and assembly bounds. Tests include alternate hardware, invalid screw length, dado/rim collisions and incompatible thin walls. CAD failures return a nonzero exit status.

## Pocket-screw hardware

By default two screws per corner, eight total; --screws-per-corner changes the count. Pocket holes are on the OUTSIDE faces of the front/back boards. The full-depth sides and all drawer dimensions remain unchanged. Default pocket axis heights are 35 and 65 mm above the wall bottom, clear of the 6–18 mm bottom groove.

Recommended jig: **Kreg 720PRO + Micro-Pocket Drill Guide Kit 730 (KPHA730)**. The 720PRO offers automatic thickness setting/clamping, workpiece supports, a repeat stop and dust collection. The 730 is specifically compatible and intended for thin stock, including 1/2-inch material. Use SPS pan-head screws. The 730 manual gives 1-inch screws for the 1/2-inch setting but explicitly allows SPS-F075 3/4-inch screws where a longer screw could exit the workpiece. The CAD uses the shorter 19.05 mm option. Select thread type for the actual wood and verify the actual joint on scrap.

Sources:
- [720PRO specifications](https://www.kregtool.com/shop/pocket-hole-joinery/pocket-hole-jigs/kreg-pocket-hole-jig-720pro/KPHJ720PRO.html)
- [Micro-Pocket 730 compatibility](https://www.kregtool.com/en/products/pocket-hole-joinery/pocket-hole-jig-accessories/micro-pocket-drill-guide-kit-730/KPHA730.html)
- [730 manual and screw-length guidance](https://www.kregtool.com/on/demandware.static/-/Library-Sites-RefArchSharedLibrary/default/dwcdefa88d/manuals/KPHA730_NA.pdf)
- [Kreg pocket-hole angle guidance](https://www.kregtool.com/shop/pocket-hole-joinery/?src=hpcatphj1024)

### Model accuracy and adjustable hardware

This is an **SPS-style assembly visualization, not a certified Kreg screw model or a machining template**. Screws have a cylindrical pan-head envelope, smooth thread-envelope shaft and conical tip; threads and drive recesses are omitted. Nominal head, shaft, pocket and shoulder dimensions are engineering assumptions, not verified manufacturer drawings. Do not transfer the CAD shoulder depth to a jig stop collar: use the manufacturer's setup instructions. Measure a sample drilled joint before using these pockets for CNC manufacture.

To avoid overlapping solids, the receiving side contains the installed screw's envelope. That represents material displaced by the screw, **not a recommendation to predrill a full-diameter receiver hole**.

All hardware defaults are centralized in `src/settings.hpp` and can be overridden:

| CLI option | Default | Meaning |
|---|---:|---|
| `--screw-length` | 19.05 mm | Under-head to tip |
| `--screw-diameter` | 3.5 mm | Smooth thread envelope |
| `--head-diameter` | 5.5 mm | Pan-head envelope |
| `--head-height` | 1.8 mm | Axial head thickness |
| `--tip-length` | 3 mm | Conical tip length |
| `--pocket-diameter` | 7 mm | Nominal counterbore |
| `--pocket-angle` | 15 degrees | Angle to outside front/back face |
| `--seat-distance` | 10.5 mm | Distance along screw axis from head underside to joint |
| `--screw-lower` | 35 mm | Lower screw Z coordinate |
| `--screw-upper` | 65 mm | Upper screw Z coordinate |

The small bore through the front/back uses the screw envelope diameter for assembly clearance; it does not reproduce the proprietary stepped drill profile. Pocket cutters are flat-shouldered cylinders. Heights are independent of drawer height and must be adjusted if you make a shorter drawer or move the bottom up. Hardware that breaks through a wall or collides is rejected rather than silently resized. This includes the default hardware in 10 mm walls.

```sh
./build/drawer --screw-length 19.05 --screw-lower 35 --screw-upper 65 --output drawer.step
./build/drawer --thickness 18 --screw-length 25.4 --seat-distance 16 --output thicker-walls.step
```

## Applied drawer face

A separate rectangular face is included by default: **375 mm wide × 135 mm high × 12 mm thick**. Its back touches the drawer box at Y=0; its front is at Y=-12 mm. It is centered horizontally and vertically on the outside of the box, giving **25.5 mm side overhangs** and **23.5 mm top/bottom overhangs** with the default drawer dimensions. The assembled exterior is **375 mm wide × 541 mm deep × 135 mm high**. Usable interior dimensions are unchanged.

The face covers the front pocket openings. Assemble the pocket-screwed drawer box before attaching the face. Face attachment hardware is not modeled; the eight existing screws connect the box corners only.

```sh
./build/drawer --face-height 135 --face-width 375 --face-thickness 12 --output drawer.step
./build/drawer --no-face --output drawer-box.step
```

All three face dimensions are positive millimetre values using the same numeric validation as other dimensions. Defaults are `constexpr` values in `src/settings.hpp`. Smaller faces are allowed and remain centered. `--no-face` omits the panel; `--face` enables it again. The last of those two flags wins; numeric face options do not change visibility. The box origin remains unchanged, so an oversized centered face extends into negative X and Z coordinates.

## Face alignment and complete command-line reference

`--face-offset` shifts the applied face vertically from its centered position: **positive = up, negative = down, zero = centered**. The box and usable space stay fixed. `--face-offset-x` similarly shifts it right/left. The face must still overlap the box front. For the default box, top overhang is `22.975 + face-offset` mm and bottom overhang is `22.975 - face-offset` mm; negative overhang means that edge of the box is exposed.

```sh
./build/drawer --face-offset 10 --output raised-face.step
./build/drawer --face-offset=-10 --output lowered-face.step
./build/drawer --help
```

The man-style help includes NAME, SYNOPSIS, DESCRIPTION, OPTIONS, CONSTRAINTS, VALIDATION, EXAMPLES, FILES, LIMITATIONS and EXIT STATUS. To save a local copy, run `./build/drawer --help > HELP.txt`; this generated file is ignored by Git. Both `--name value` and `--name=value` are supported. Repeated numeric options use the last value.

For the legacy standalone mode, dimensions, hardware and validation settings have
named defaults in **src/settings.hpp** and command-line overrides. Its construction
formulas remain in C++. Production Make targets use the external Jsonnet recipes;
the C++ project engine evaluates their boards and machining operations.

Additional options:

| Option | Default | Meaning |
|---|---:|---|
| `--face-offset` | 0 mm | Vertical translation from centered |
| `--face-offset-x` | 0 mm | Horizontal translation from centered |
| `--screws-per-corner` | 2 | Integer screw count; evenly spaced from lower to upper; one uses lower |
| `--pocket-clearance` | 2 mm | Minimum clearance from dado, rim and adjacent pocket |
| `--screw-tip-clearance` | 0.5 mm | Minimum clearance from outside side face |
| `--distance-tolerance` | 0.00001 mm | Absolute bounds-check tolerance |
| `--volume-tolerance` | 0.00001 mm³ | Absolute volume/intersection tolerance |
| `--relative-tolerance` | 0.0000001 | Relative volume-check tolerance |
| `--min-feature` | 0.001 mm | Minimum positive length / remaining dado wall |
| `--max-dimension` | 1000000 mm | Maximum length / offset magnitude |

Validation tolerances are numerical comparison settings, not manufacturing clearances. Increasing them relaxes checks. Defaults are recommended. The original model dimensions and centered face remain the defaults.

## Named STEP assemblies and board numbering

STEP export uses OpenCascade XCAF assembly metadata. Each file has **one named root drawer assembly**, with selectable numbered board parts and named pocket screws beneath it. Permanent connections are represented by membership in this assembled drawer; parts are not Boolean-fused or provided with simulated constraints. The importer's handling of assembly selection/rigidity remains application-dependent.

- `--assembly-name NAME` sets the parent object name (default: output filename stem).
- `--board-start N` sets the first board number (positive integer, default 1).
- Board order: left side, right side, front wall, back wall, bottom panel, optional applied face.
- Names include the number: `B01 - Left side`, `B02 - Right side`, etc.
- Screws are `S01 - Front left pocket screw 1`, etc.; screw numbering is local to each drawer assembly.

The project assigns **B01–B05** to `Upper inner drawer`, and **B06–B11** to `Lower pull-out tray with tall front`, so all boards across the two generated drawers have unique numbers. For additional template instances, choose a non-overlapping `board_start`; the legacy CLI accepts `--board-start`.

On each run, the program reimports the STEP file and verifies one root assembly, its name, all component and instance names, uniqueness, component volumes, total solid count and bounds. Import the regenerated STEP files again to see the new hierarchy; existing imported models will not update automatically. The original unnumbered default translator text may still occur in STEP header metadata, but no longer names the drawer or its parts.

## Shared coordinates for importing both drawers

`--position-x`, `--position-y` and `--position-z` translate the complete drawer assembly, including every board, its applied face and all screws. Defaults are zero, values are signed millimetres. X is right, Y is rearward, Z is up. Face offsets remain local to the drawer and are applied before assembly translation. This is translation only; it does not create inter-drawer joints or constraints.

The Makefile uses a shared datum at the lower box's front-left plan position, with Z=0 at the cabinet opening bottom:

| Assembly | X | Y | Z (box bottom) | Box top |
|---|---:|---:|---:|---:|
| Lower tray | 0 | 12 | 35 | 238 |
| Upper inner drawer | 0 | 12 | 619 | 721 |

Thus the upper box is 584 mm above the lower box, with a 381 mm vertical gap between box rim and upper box bottom. Both boxes start at Y=12; the rear surface of the applied face also lies at Y=12. Project parameters control both placements and accept Make overrides. The tall face spans Z=9.8 to 721 mm (equivalent to the old face-offset 228.9 mm). Its top is linked to the upper box's top; its rear is linked to the lower box's front. Final runner installation geometry is not modeled.

Run `make models`, then import both generated STEP files into the **same CAD workspace**, preserving original coordinates. Turn off any import option that centers or auto-arranges each file. Each file remains one named assembly with selectable boards and screws. The files do not reference one another: their common coordinates provide the relative positioning.

Every export verifies each named component's world-space bounds after STEP reimport. Integration tests generate both files from the shell and independently check drawer coordinates and absence of solid overlap between assemblies.

## Cabinet assembly and shared coordinates

`Makefile` now calls the same C++ executable three times, producing
`inner-drawer-477x523x102.step`, `tall-front-bottom-aligned.step` and
`cabinet-carcass.step`. Import all three preserving original coordinates.
X is right, Y rearward and Z up; opening bottom is Z=0.
Upper drawer bottom is Z=619 and lower tray bottom is Z=35 (584 mm difference).
Both drawer boxes start at Y=12.

The project's `carcass` assembly contains numbered boards B12–B17:
two sides, bottom, top, two full-height face-frame
stiles. The two side panels align with the opening edges; there are no side spacers. It reuses the STEP assembly
exporter, named parts, translation flags and geometry validation. No front rails
cross the opening. Carcass local origin is the left opening edge, front of frame,
and opening bottom. The legacy `--model carcass` command also remains available.

Approved construction assumptions (all CLI parameters): opening 494 × 737 mm,
overall depth 597 mm including frame, carcass plywood 18 mm, stiles 38 mm wide
and 18 mm thick. Overall including the face frame is 570 × 597 × 773 mm. The carcass box is 530 mm wide; the stiles overhang each side by 20 mm. Top/bottom fit between full-height sides.

Make accepts command-line and environment overrides `OPENING_WIDTH_MM`, `OPENING_HEIGHT_MM`,
`CABINET_DEPTH_MM`, `CARCASS_THICKNESS_MM`, `STILE_WIDTH_MM`,
`STILE_THICKNESS_MM` and `FRAME_FRONT_Y_MM` (default 3.5). It centers the cabinet
opening on the drawers automatically. Frame front is Y=3.5; the applied face
spans Y=0 to 12. These are explicit assembly-placement settings.

The tall face is 469.9 × 711.2 × 12 mm (18.5 × 28 inches wide and high) and spans world Z=9.8 to 721. Its default top follows `upper_box.top_z`. Set `FACE_TOP_Z_MM` for a fixed top, or use the compatibility Make variable `FACE_BOTTOM_OVERLAY_MM` to position its lower edge at the negative of that value. Either explicit override replaces the default top relationship. The current actual material is 12 mm for drawers/face and 18 mm for the carcass; the original reference PDF uses nominal imperial stock. The legacy filename tall-front-bottom-aligned.step is retained to replace existing imports; it no longer describes the face alignment. Runner/TIP-ON mechanisms, bins, face fixing
screws and carcass fastening hardware are not modeled; their dimensions and
mounting details require a separate specification. The boards form a named
assembly with selectable parts, not a single fused solid.

Run `build/drawer --help` for the complete CLI manual. Validation includes
reimported part names, volumes and bounds, plus pairwise intersection checks
between the three generated assemblies in the CTest integration test.

## Actual metric plywood

Project stock is 12 mm for drawer walls, bottoms and applied front; 18 mm for carcass and frame. Dados and bottom offsets default to 6 mm. The project relationships derive board sizes from 477 x 523 mm box exteriors and 102/203 mm heights: interiors 453 x 499 mm; bottoms 465 x 511 mm; usable heights 84/185 mm. These values and assembly positions are configurable; the initial carcass outer height is 773 mm. Hardware lengths such as 19.05 mm are unchanged purchased screw sizes.

For 12 mm walls, the modeled pocket screw seat distance is 10.5 mm (previously 11.5 mm), keeping the head inside the thinner wall envelope. Screw/head dimensions are unchanged. Run `make packet` to produce `generated/cabinet-shop-packet/shop-packet.pdf` for the current material sizes. Older cutting plans and imported drawings may be kept locally as historical references; they are not included in the repository.

## Build and generation with Make

Run from the project folder. `make` builds the C++ tools and refreshes the three models, normal/AutoCAD merged files, and the shop PDF. Every generation target configures and builds its own required tools, even from empty build folders. No preliminary `make all` is required. `make upper`, `make lower`, and `make carcass` generate individual assemblies; `make models` generates all three. `make packet`, `make merged`, `make autocad`, and `make help` are also available.

Generated deliverables, intermediate files and local archives are excluded from
Git. Default outputs go under `generated/`, and CMake intermediates stay in the
tools' `build/` directories. Keep custom output/build directories outside the
checkout or add them to your local `.git/info/exclude`. The only tracked STEP/PDF
files are the frozen inputs in `tests/fixtures/pre-config/`, which let regression
tests compare against the approved pre-configuration geometry and shop packet.
Those fixtures must not be overwritten by a production build.

Dimensions and hardware settings come from the project file and accept Make overrides; for example:

```bash
make packet WALL_THICKNESS_MM=12 BOTTOM_THICKNESS_MM=12 CARCASS_THICKNESS_MM=18 SHEETS=5x5
make models OUTPUT_DIR="/tmp/drawer models"
make lower FACE_WIDTH_MM=469.9 FACE_HEIGHT_MM=711.2 LOWER_POSITION_Y_MM=12
```

Use `make help` for build paths and PDF options. Generation targets intentionally refresh their outputs so changed parameters cannot leave stale geometry. C++ compilation remains incremental. The PDF is generated in a temporary directory and replaces the current packet only after success. Identification-label pages are omitted by default; every piece in the cutting diagrams includes its dimensions. `PACKET_OPTIONS="--labels"` optionally adds paper labels, and `PACKET_OPTIONS="--export-details"` retains CSV/SVG exports.

`make clean` removes the three configured CMake build trees, preserving generated models, PDFs and archives. Overrides for `BUILD_DIR`, `MERGE_BUILD_DIR`, and `CUTLIST_BUILD_DIR` apply to both building and cleaning. The clean target validates all paths and project caches before deleting anything; it refuses source/output directories, symlinks and unrecognized folders. Run it separately from generation: `make clean && make lower` (not `make clean lower`).

`make build` builds all tools; `make drawer-build`, `make merge-build`, and `make cutlist-build` build one tool. `make test` builds and runs the geometry/placement, Jsonnet template, cut-list, merger and frozen-output regression suites; each suite also has an independent target. `make regression-test` compares all five STEP exports and the PDF to the [pre-configuration fixtures](tests/fixtures/pre-config/README.md), including Boolean geometry comparison and a byte-for-byte PDF check. `make test-make` exercises every public build/generation/test target from fresh temporary folders, including paths with spaces, parallel generation, clean safeguards and failed-output preservation. `SKIP_BUILD=1` requires existing executables but still expands the selected design on every generation. The Makefile supports GNU Make 3.81 shipped with macOS.


## Keep-left shop cutting

`make all` now uses a 5 mm maximum kerf allowance (`KERF_MM=5`). The shop packet
identifies the source subpanel and the dimension to keep on the left of the
blade for every cut. Horizontal diagram cuts are performed with the panel turned
clockwise so its diagram-bottom edge becomes the left reference edge.

The planner includes finishing passes for remainders, so actual kerfs up to the
allowance produce the same blank sizes when each kept-edge measurement is set
independently. A wider kerf requires replanning. See the cutting guide in the PDF
and `tools/step-cutlist/README.md` for the piece labels and measurement method.

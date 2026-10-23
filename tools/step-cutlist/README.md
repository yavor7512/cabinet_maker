# STEP plywood inventory and cut optimizer

Independent C++17/OpenCascade tool. Build and run from this directory:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/opencascade -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/step-cutlist --help
./build/step-cutlist --out my-plan model.step another-model.step
```

From the Drawer project root, for the complete cabinet, including machined board blanks:

```bash
./tools/step-cutlist/build/step-cutlist \
  --out generated/complete-cabinet-cut-list \
  --sheets 5x5 --kerf 5 \
  generated/complete-cabinet.step
```

The output directory must be new or empty. Nothing in the source STEP is changed.

## Scope and geometry

Reads STEP solids using OpenCascade/XCAF, including nested assemblies, duplicate
part occurrences and arbitrary translations/rotations. Determines thickness from
the smallest local dimension. By default, recovers rectangular blanks from the
surviving six exterior datum planes of boards. Dados, pocket drilling, holes and
notches can remove material without changing the stock blank needed.

This is a blank-cutting plan, not a machining plan. `inventory.csv` flags machined
boards and records removed volume; `machining.csv` lists work remaining after the
blank is cut. Refer to the original STEP for the operations and their dimensions.
The tool does not reconstruct material beyond the remaining outer extents, infer
joinery allowances, or supply router/drill instructions. If six exterior datum
planes cannot be established, it rejects the part instead of guessing stock size.
Default sheet-like limits are 50.8 mm thickness and thickness/smaller-face-dimension
ratio <=0.5. Use `--strict-rectangles` to restore the original full-box-only check.

Board recognition uses geometry, independently of part names. The Drawer
configuration distinguishes boards from hardware, and its STEP exporter carries
those roles as `DrawerRole:Board` and `DrawerRole:Hardware` XCAF layers through
merged files. Assembly roles apply to their descendants; a child explicitly
tagged with the opposite role is rejected as a conflict. Explicitly tagged hardware is
excluded from plywood quantities and listed in `excluded.csv`. A rectangular
board named for its purpose, such as a screw tray, remains in the plan. An
untyped solid that fails board recognition is unsupported and blocks a complete
plan, even if its name says "screw". The current complete cabinet contains
17 board blanks and 16 excluded pocket screws.

`--skip-unsupported` still means a partial plan, but is not needed for the current
complete cabinet. Inventory/rejections are saved if unidentifiable geometry exists.
The code does not treat every bounding box as a board. Surface-only models remain
unsupported. Colors do not establish material species.

All inputs in a run must use the same material species and grade. Thickness groups
are kept separate (default grouping tolerance .01 mm). Run separately for different
materials. CSV quantities are rounded to .001 mm for display; layouts use measured
geometry without rounding. All dimensions and results are millimetres.

## Optimization guarantees (material workflow)

Stock dimensions are exact feet converted to mm: 8x4 = 2438.4x1219.2,
4x4 = 1219.2x1219.2, 5x5 = 1524x1524. Unlimited sheets of selected sizes are
available. The objective minimizes total purchased area, then sheet count. It does
not minimize dollars, cut count, or saw handling. It assumes square, usable stock
and includes no edge-trimming allowance or part-sizing allowance.

By default, each part's longest dimension follows the sheet X/grain direction.
STEP usually does not specify grain; this is an assumption, not a detected fact.
`--allow-rotate` permits 90-degree rotations when grain is unimportant.

With `--workflow material`, for <=12 parts per thickness by default, exact subset dynamic programming generates
all binary rectangular guillotine slicing trees with kerf and retains Pareto-optimal
bounding dimensions. A second subset DP selects the sheet partition and stock sizes.
A guillotine cut traverses a whole current subpanel, so the output can be followed
as straight saw cuts. The optimality claim covers this model, not unrestricted
non-guillotine nesting. Floating-point comparisons use a 0.000001 mm numerical
allowance; this is not a machining allowance.

Exact-search limits are configurable: `--exact-limit` (up to 16), `--seconds`
(default 10 per thickness), and `--max-candidates` (default 5000000).
When any limit prevents completion, a validated shelf/guillotine heuristic is used
and the report explicitly says optimum NOT proven. Exact optimization is exponential;
raising these limits may substantially increase time and memory use.

Before output, every part is checked for count, dimensions, sheet containment and
kerf separation. Internal slicing-tree dimensions are checked when emitting cuts.

## Files

- `inventory.csv`: one row per accepted physical part occurrence, with source path.
- `quantities.csv`: aggregate rectangular sizes and quantities.
- `rejected.csv`: every unsupported object and reason.
- `excluded.csv`: hardware with an explicit role intentionally excluded.
- `machining.csv`: boards requiring additional operations after blank cutting.
- `report.txt`: thickness groups, stock usage, utilization and exact/heuristic status.
- `thickness-N/placements.csv`: nominal part coordinates, inventory IDs and finished-piece references.
- `thickness-N/cuts.csv`: ordered cuts, FROM subpanel references, KEEP LEFT dimensions, left/right result labels and orientation.
- `thickness-N/sheet-N.svg`: annotated sheet diagram; hover a part for its source name.

Coordinates are measured from the sheet lower-left. V cuts are parallel to Y;
H cuts are parallel to X. Cut coordinate is the retained edge, with kerf on its
positive side. At an outer edge a trimming cut may consume less than a full kerf
because the remaining saw width is outside the stock. Material workflow uses depth-first order; shop workflow batches stages while
preserving dependencies.
Use FROM and KEEP LEFT to execute the sequence. Diagram coordinates and starting
subpanel sizes assume the maximum planning kerf and are not cumulative saw settings.

## Future angled cuts

`model.hpp` owns STEP geometry classification; `nesting.hpp` owns the rectangular
optimizer; `main.cpp` owns CLI and reports. This separation allows a future polygon
part representation and nesting backend without changing STEP traversal.

30/45/60-degree finished-part cut layouts are not yet enabled. A machined part\nwith six surviving rectangular datum planes may be represented by its stock blank;\nits angled machining is not planned. In-plane angled outlines (trapezoids,
triangles) require polygon nesting and cut-path/kerf geometry. Beveled board edges
are different: the plan must also record blade tilt and face-dependent dimensions.
Neither case should be represented as a rotated rectangular bounding box.

## Shop workflow (new default)

`--workflow shop` favors fewer saw passes, repeatable strip widths and staged
cutting. The implementation follows the practical approach documented by
[MaxCut: workshop optimization](https://maxcutsoftware.com/features/cutting-optimization/tailored-optimization/)
and [HOMAG: cutting parameters](https://docs.homag.cloud/docs/en/intellidivide-processing-parameters-optimization-parameters-cutting):
use edge-to-edge cuts, select a starting direction, and reduce changes of cutting
direction. These sources describe production practices, not public algorithms;
this tool's search is an independent implementation.

The original material-area search supplies the baseline sheet budget. The shop
search considers both rip-first and crosscut-first strip layouts, sorts matching
widths together, and tries 64 deterministic orderings by default. Four cut policies
are evaluated: sizing shared-width columns or shared-height rows before their
crosscuts, shared waste trimming before splitting, trimming along the current
cut direction, and delaying trims to individual blanks. Each includes finishing
passes on dimensions inherited from a right-hand remainder, even when that
remainder is nominally an exact fit. Every actual saw pass,
including trimming waste, counts; stacking multiple pieces is NOT assumed.

Selection minimizes actual cuts, then slicing-tree direction changes, then sheet
area. This is a heuristic, not a proof of globally minimum cut count. Strip width
matching encourages repeated settings; the code does not claim to minimize fence
setting changes or predict total shop time. Initial sheet-size/grain assumptions
and the no-edge-trim allowance apply. Stock defaults to 5x5 (1524 x 1524 mm) for easier handling; other sizes remain available via --sheets.

- `--workflow shop`: default production-friendly search.
- `--workflow material`: retain the original area-optimal guillotine search.
- `--extra-area-percent 35`: default allows up to 35% extra purchased area
  in exchange for fewer cuts. Set 0 to keep the baseline area budget.
- `--shop-attempts 64`: deterministic search orderings, 1–1000. Both initial
  cutting directions are considered for each ordering. Exact-search time/work
  limits control the material baseline, not these bounded heuristic passes.

The SVG shows numbered, stage-colored saw lines and a matching sidebar:
FROM subpanel, KEEP LEFT dimension and nominal subpanel size for each pass. SVG numbers match `cuts.csv`.
V is a vertical line in the drawing; H is horizontal. These denote the diagram
axis, not a blade tilt. V retains diagram-left; H retains diagram-bottom.
For H, turn the panel clockwise so its diagram-bottom edge is physically on
the left of the blade. Use the sheet diagram as the orientation reference
each time, preserving the grain layout. Stage labels group directions along the slicing tree; they are not an
instruction to stack pieces or run detached offcuts through a saw simultaneously.

The cut sequence is simulated before output: every cut must reference an existing
subpanel and every placed blank must exist as a finished rectangle after the last
cut. This supplements checks on kerf separation, geometry, inventory and bounds.

For the current 12/18 mm cabinet with the 469.9 x 711.2 mm face, 5 mm planning
kerf and two 5x5 sheets:
- 12 mm parts: shop plan 16 cuts, starting with two full-height columns.
- 18 mm parts: shop plan 8 cuts: two column-width cuts, then six crosscuts.
These counts include finishing passes and exclude the separate dado/drill operations.

## Keep-left cutting and kerf allowance

The default `--kerf 5` reserves **up to 5 mm** for each blade pass. The Makefile
uses the same default through `KERF_MM`. The actual kerf may be smaller; it must
not be larger than the planned allowance. Stock must still be square and have
the stated usable dimensions.

For each cut, select the **FROM** piece, orient the specified reference edge to
the left, and measure **KEEP LEFT** from that edge to the blade tooth edge nearest
the retained piece. Reset the measurement on the current panel for every cut.
Do not use the blade centre, cumulative sheet coordinates or kerf subtraction.

`S1` denotes sheet 1 within its thickness group. After cut `Cn`, label the left
piece `CnL` and the right remainder `CnR`; retain either if a later FROM references
it. A P ID in the PDF's LEFT PIECE column identifies a completed blank.

The planner tracks whether each dimension was directly sized or inherited from
a remainder. Remainder dimensions get a finishing pass before they can become a
finished blank. Thus a thinner actual blade leaves extra stock for trimming rather
than an oversized final piece. A nominal edge pass can be skipped only after
measuring that the piece is already to size. This can require more cuts than a
plan that assumes the exact blade kerf.

Every generated sequence is simulated at zero, half and full planning kerf.
Tests also exercise 1.2, 3.2 and 4.9 mm blades against a 5 mm allowance, exact-fit
siblings in both directions, all four trim policies, and missing finishing cuts.
The PDF includes an orientation/measurement guide and references for every cut.

## Shop packet (default output)

The output directory contains only `shop-packet.pdf`: a material summary, a
numbered parts list, machining/exception notes, and a layout with saw order for
each sheet. Every diagram piece includes its ID and dimensions, including small
panels and narrow strips. Identification-label pages are omitted by default.
`--labels` appends cut-out part labels. These are plain paper labels,
not a particular adhesive-label stock template. `--export-details` retains the
CSV, SVG and report files listed above inside `details/`. Existing output folders
must still be empty; older exports are not overwritten.

The PDF uses embedded Helvetica Neue. PDF generation is C++ with macOS CoreText
for font extraction; it needs no Python or browser. The current PDF font backend
requires macOS. Diagrams are not to scale; dimensions are authoritative. Non-ASCII
name characters are currently replaced by question marks in the PDF; full source
names remain in the optional CSV exports. Compact part names use the final STEP
path component, with unique inventory IDs to distinguish duplicate names.

Machining notes identify blanks with removed material, but do not extract dado,
hole or pocket-screw setup dimensions. Refer to the source drawings/model before
machining. Unsupported geometry is listed prominently, and stops cutting-plan
creation unless `--skip-unsupported` explicitly requests the accepted subset.

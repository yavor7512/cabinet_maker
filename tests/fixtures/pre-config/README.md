# Independent output regression fixtures

The three component STEP files and `shop-packet.pdf` were copied unchanged from
`/tmp/drawer-project-baseline`, the backup made on 2026-10-22 **before** the first
`cabinet.json` refactor. They are not generated from the current configuration or
current legacy C++ implementation during tests.

The two `complete-cabinet*.step` fixtures were assembled once from those original
component files using the unchanged STEP merger, brown `#A8793E`, the original
carcass/upper/lower input order, and the usual AutoCAD rotation for that variant.
These are frozen derived references; tests never rebuild their expected results.

`make regression-test` generates all five STEP exports and the PDF from the
current Jsonnet design in a separate build subdirectory. It checks:

- All 33 parts: names, assembly hierarchy, order, colors, world-space bounds,
  volume and Boolean intersection with the original occupied material. This
  detects relocated pockets/dados even if bounds and volume remain unchanged.
- Both merged orientations, including the AutoCAD transform.
- The entire PDF byte for byte: expected SHA256
  `dc4d58f66cc6ae72d4bd4af18d76b9dbb580766c7619c057f14775bf165e5d1e`.

Bounds tolerance is 0.00001 mm; volume/Boolean tolerance is the larger of
0.00001 mm³ and 0.0000001 of part volume. STEP timestamps and equivalent entity
serialization are intentionally ignored. The fixed PDF covers dimensions,
piece IDs, the 5 mm kerf allowance, two-column layouts and all 24 cuts.

These fixtures should only be replaced following a deliberate design/output
change, with the reason recorded here. Passing a test is not a reason to update
its expected artifacts.

# STEP merger

Independent C++17/OpenCascade command-line utility. No Python.

Build:
```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/opencascade
cmake --build build -j
```

Merge any number of files (quote paths containing spaces):
```bash
./build/step-merge --output combined.step first.step second.step third.step
```

Use `--force` to replace an output. Inputs cannot be overwritten. `--help`
prints the CLI manual. Each input becomes a numbered, filename-named group
under one root assembly. Existing nested assemblies, parts, names and positions
are copied using XCAF. Bodies are not fused, repositioned or deduplicated.
Repeated inputs create separate occurrences. Overlapping parts remain separate.
Source units are imported by OpenCascade; output uses millimetres. Standard
XCAF metadata is copied, but unsupported annotations are not guaranteed to survive.

The output is reimported and its hierarchy, names, per-part world bounds, volumes
and solid counts are checked before replacing the destination. Each run reserves
a private numbered sibling directory for its temporary STEP. Stale directories
from interrupted runs are left untouched.

Run smoke tests with `bash test.sh ./build/step-merge /path/to/input.step`.

## AutoCAD viewer orientation

Add `--autocad` to rotate the complete model -90 degrees about world X through
the origin. Mapping is `(X,Y,Z) -> (X,Z,-Y)`: original Z-up becomes Y-up,
and original positive depth becomes negative Z. This is a viewer-oriented
export preset, not a claim that all Autodesk applications require Y-up.
Default export remains unchanged. All parts share the same rotation; dimensions,
relative placement, names and hierarchy are retained. Apply this option only
once to original Z-up inputs, and use a separate output filename.

```bash
./build/step-merge --autocad -o cabinet-autocad.step cabinet.step
```

`check-orientation normal/model.step rotated/model.step` independently verifies
the axis mapping, volume and hierarchy after export (use matching output basenames
and input lists for both files).

## Material appearance

`--material dark-wood` overrides all objects (including hardware) with the solid
brown preset `#80552F`, an approximate dark wood tone based on the supplied image.
`--color '#80552F'` accepts any six-digit sRGB hex color. The last material/color
option wins. Omit both options to retain imported colors. These options combine
with `--autocad` and are checked after STEP reimport for every leaf part.

This is STEP surface color, not a physical wood material or a grain texture.
Bitmap grain and rendering materials are not portably conveyed by this exporter;
apply those in the viewing/rendering application. Lighting can affect appearance.

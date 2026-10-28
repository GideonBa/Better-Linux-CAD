# Pixelarticons subset

Source: [halfmage/pixelarticons](https://github.com/halfmage/pixelarticons), version **2.4.2**,
commit `c9ecc2da61afde85dbe539025e3d931c6152ad75` (2026-10-06).
Copyright Gerrit Halfmann; the original MIT notice is retained verbatim in `LICENSE`.
Only icons available in that public repository are used.

`catalog.json` is the explicit presentation mapping for stable command IDs, English command/button
labels, typed browser nodes and Sketch record families. Unmapped commands keep their text.
Examples of intentional fallbacks include Extrude, Revolve, Sweep, Loft, Fillet, Chamfer and joints:
the selected pack does not offer an unambiguous symbol for those CAD operations. Authored names and
opaque object identifiers never determine a browser icon.

The 65 original files in `svg/` are unmodified. `masks/` contains lossless 24×24 XPM masks rendered
from those paths, with only transparent or opaque white pixels. `source-manifest.json` records the
source SVG, mask and license SHA-256 values. `resources.cmake` explicitly lists the bundled files.

The application recolors those masks through a QtGui `QIconEngine`: off-white normally, lavender
when active/checked and muted lavender when disabled. Nearest-neighbor scaling preserves hard
pixel edges, including fractional display scales. Ordinary ribbon buttons, compact labeled buttons,
tabs, tree rows and task buttons request 24 logical pixels. Qt retains its menu layout metrics.
Mapped context shortcuts use icon buttons with the original label as tooltip and accessible name;
unmapped shortcuts retain text. The dark-violet palette, charcoal canvas and classic splash remain.

No npm, React, QtSvg, system icon theme, network access or generation tool is needed to build or run
BLCAD. The masks, license and catalog are bundled Qt resources.

## Regeneration

Regeneration alone requires Python 3, Pillow and `rsvg-convert` (librsvg). Extract the exact pinned
upstream archive, then run from the repository root:

```bash
python3 scripts/vendor_pixelarticons.py /path/to/pixelarticons-c9ecc2da61afde85dbe539025e3d931c6152ad75
```

The script validates the package version and rejects masks with fractional alpha or a non-24px
canvas. Review each proposed symbol visually, edit the catalog, regenerate, then run the existing
GUI appearance/workflow checks and `[gui][icons]`. Missing or ambiguous mappings are valid fallbacks.

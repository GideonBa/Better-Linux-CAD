# GUI Spatial Path, Sweep, Loft, and Surface Workbench MVP-7

Status: implemented in Block 102.

`GuiSpatialSurfaceWorkbench` exposes complete authored model-space `Sketch3D` records (points,
lines, polylines, arcs, splines, helices, and guide roles), connected ordered `PathCurve` inputs from
planar sketches or 3D sketches, Sweep/SweepCut/SweepSurface, path-following Extrude/Cut,
Loft/LoftCut/LoftSurface, and every current Surface feature variant: Boundary, Fill, Trim, Extend,
Stitch/Knit/Sew, and Closed-shell-to-solid.

Capability-specific prompts make profiles, paths, guides, section order, alignment, continuity,
orientation, twist, boundary/surface inputs, and result Body kind explicit. Preview validates a
copied `PartDocument`, derives the candidate recompute plan, and publishes neither intent nor cached
Geometry. Apply uses one GUI document transaction.

The existing viewport scene authority already emits model-space 3D curves and PathCurves and
classifies cached Body results as `SolidBody` or `SurfaceBody`. The Block-102 acceptance scenario
proves these three presentation families together after GUI-authored recompute. Missing profile,
guide, path, section, topology, or output-Body references fail closed through normal Core errors.

## Verification

```bash
./build/dev-gui/blcad_gui_tests "[gui][path-workbench]"
./build/dev-gui/blcad_gui_tests "[gui][sweep-loft]"
./build/dev-gui/blcad_gui_tests "[gui][surface-workbench]"
```

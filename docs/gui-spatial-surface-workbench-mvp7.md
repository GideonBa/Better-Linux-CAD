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


## Advanced input catalog foundation

`FinishingSelectionBuilder` supplies compatible `SurfaceSourceFace`, `LoftBoundarySupport` and
`PathSegment` inputs from the actual recomputed Solid or Surface Body. Existing finishing roles
still require a Solid. Face identities retain authored contributors and explicit Body ownership;
curved edges use complete, unique native result witnesses. Hidden input access is explicit and
does not change persistent visibility. Stale or ambiguous sources reject normally.

Viewport paths selected on a placed imported Body bind the current result witness. Existing
imported-topology Path bindings retain their immutable-base meaning; the catalog does not relabel
those references to make a transformed edge resolve. Input edits require a recomputed cache for
the new source intent before subsequent read-only queries.

[Focused selectors](../tests/geometry/advanced_surface_selection_tests.cpp) prove three-face
curved/tangent quilts, typed support ownership, native boundary paths, explicit hidden scope and
a placed imported cylindrical rim. Combined with existing finishing and semantic-path regressions:
**11 cases / 515 assertions**, all passed; all five build targets passed. This is the catalog
foundation only; complete native advanced authoring remains in progress in `mvp-plan.md`.

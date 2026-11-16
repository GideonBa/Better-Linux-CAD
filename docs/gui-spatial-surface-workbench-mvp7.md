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

## Native material-region and current-edge selectors — 173

Sweep creation exposes the explicit outer-boundary/material-region policy and lists the exact
immediate holes resolved by the execution kernel. Touching/crossing contours fail before Apply;
open profiles ignore inactive region controls. Preview and Apply retain the authored policy.

Create Path also accepts current, visible native Solid/Surface Body edges, with typed semantic
provenance, current history scope and placed-import freshness checks. Model clicks update both the
ordered segment list and source chooser. Curve-preserving execution retains the native arc. Cancel
restores entry selection, filters and semantic scope and removes temporary edge presentations.
Hidden owned Bodies cannot reappear through the legacy terminal-shape viewport fallback.

Frozen runtime: 1,206 files, SHA-256
`975bbfe3a94fa0e937a02058e02d490fde257d663e43ea3b839bb214c81ccfad`.
All five targets passed. Focused Geometry **14 / 903**, Qt **12 passed / 5 native skips / 1,709
assertions**, native combined Sweep/Path/curved-Surface workflows **5 / 363** at both 100% and
compact 150% passed; final captures were reviewed. The native hollow Sweep checks independently
integrated volume `30π`, an empty axial point, complete Undo/Redo, reopening and physical STEP
reimport. The selected current Body arc checks true path length `5π` after Apply.

Shared Sweep/Loft editing and constrained Loft controls remain required for complete Block 173.

## Shared Sweep and Loft editing controllers — 173

Typed editing retains the complete Body result operation, Sweep profile region policy, path,
orientation/up vector, guide and twist binding. Unchanged parameter values are not rewritten.
An explicit independent twist binding preserves the original Angle parameter; Angle expression
parameters remain outside the existing Core capability. Loft editing retains exact section
alignment/rotation, ordered rails, constraints and global continuity, with automatic realignment
opt-in during editing. Invalid geometry/global G2 remains an atomic failure.

All five targets and focused Qt **8 / 241** passed, including independent four-operation Sweep
volumes, exact no-op serialization and the verified four-boundary G2 airfoil Loft. Frozen runtime
1,207 files: `7e10578b87e54fcc66a6c778b0c916f3cab26554decca2b753e6ad6e9cac44fe`.
Native shared panels and constrained Loft authoring remain the next Block 173 gate.

## Native constrained Loft and shared curve panels — 173

The owned Loft task exposes ordered/reversible rails, explicit original-boundary entity anchors,
true boundary/guide length fractions, whole start/end or adjacent-rail patch supports, G1/G2,
normal reversal and authored tolerance units (mm, rad, 1/mm). Non-unit tangent scale and unsupported
legacy longitudinal G2 fail explicitly. Missing/intermediate stations are never guessed.

Support selectors use current owned faces and a matching pre-feature cache when editing. Hidden
inputs require the visible scope option. Model clicks and chooser selection cross-highlight the
same typed source. Repair opens the existing reference workflow. Unavailable saved sections/faces
remain repairable inputs. Cancel restores selection/filter/scope and removes transient faces.

Editing retains exact section alignment/rotation, all constraints, rail order/reversal, full Body
context, orientation/up vector and numeric precision. Existing tolerance bindings remain read-only;
choosing a new independent value retains shared formulas. Sweep offers an explicit independent
twist override. Result ownership is shown during editing. Source controls are disabled during drags.
Invalid constraint collection cancels the incomplete candidate, preventing Apply from publishing
an unconstrained substitute. Editing previews replace the actual result Body presentation.

Feedback reports measured native curve/position/normal errors, G1/G2 join counts, sampled/refined
stations and G2 curvature error where verified, qualified before the Body operation.

Native 100% and compact 150% each passed **8 / 701**, including actual model-clicked support selection,
shared editing, exact history/reopening and physical STEP reimport. Independent Bernstein profile
points and inside/outside material checks validate native and reimported geometry. Reviewed captures include anchor and support fields and numerical feedback.
Focused Qt passed **23 / 3,612**, with six native-only skips. Full Core **785 / 35,068** and
Geometry **789 / 81,422** passed; their binaries remain unchanged by the GUI refinements.
Frozen runtime: 1,213 files, `8346c17e863d56591e9c0f95e812976d7d2c0cbaff3de870d98cd2d051909daa`.
Final full Qt regression remains the whole-block acceptance gate.

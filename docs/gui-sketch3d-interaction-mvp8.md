# Interactive Sketch3D MVP-8

Status: implemented in Block 120.

Block 120 adds a headless direct-manipulation authority around the existing persistent `Sketch3D` model. It does not introduce a second spatial-curve representation and does not claim a full variational 3D constraint solver.

## Authority split

Persistent authority remains in `Sketch3D`, `SketchPoint3D`, `SketchLine3D`, `SketchPolyline3D`, `SketchArc3D`, `SketchSpline3D`, `SketchHelix3D`, and `SketchGuideCurve3D`. Screen rays, triad hover, temporary lock highlights, rubber-band curves, typed HUD text, sampled geometry, and handle placement remain transient GUI products.

`Sketch3DInteractionService` produces complete disposable `Sketch3D` candidates. A caller may preview the candidate and commit it through its document/session transaction boundary only after source freshness is rechecked.

## Orthogonal locks

`Sketch3DLock` supports:

- free model-space placement;
- X, Y, and Z axis locks through an anchor point;
- XY, YZ, and ZX plane locks through an anchor point.

Pointer-derived coordinates are resolved first. Explicit typed X/Y/Z values then override the corresponding components. Distance-direction input requires distance, azimuth, and elevation together and rejects non-finite or negative distance values.

Example: an anchor `(0,0,0)`, distance `10`, azimuth `0`, and elevation `30 degrees` resolves to approximately `(8.6603,0,5)`.

## Point and line placement

Point placement creates explicit linear-displacement coordinates in millimetres. Line placement atomically creates two points and one `SketchLine3D`; an optional guide role creates a linked `SketchGuideCurve3D` in the same candidate. Duplicate IDs, degenerate lines, or invalid guide references fail before publication.

Guide roles reuse the existing persistent values `General`, `SweepPath`, `LoftGuide`, and `Centerline`.

## Direct handles

Moving a point handle rebuilds the complete candidate with the same point ID and preserves every line, polyline, arc, spline, helix, and guide reference that targets that point. The source `Sketch3D` is never mutated. Parameter-driven coordinates are replaced only by an explicit accepted handle edit; the UI must present that semantic change before commit.

## Planar-point projection

`project_planar_point(...)` receives the resolved model-space point plus the source `SketchId` and stable `SketchReferenceTarget`. It creates a local `SketchPoint3D` and publishes `Sketch3DProjectedPointIntent` alongside the candidate so the document/session layer can retain or present the source association explicitly. Raw screen coordinates or OCCT vertex identity never become the source reference.

## Failure policy

The service fails closed for missing handles, duplicate IDs, invalid source Sketch IDs, non-finite coordinates, incomplete distance-angle tuples, negative distances, degenerate line endpoints, and invalid existing Sketch3D references. No partial candidate is returned.

## Focused proof

```bash
./build/dev/blcad_core_tests "[gui][sketch-3d-edit]"
./build/dev/blcad_core_tests "[integration][sketch-3d-direct-manipulation]"
```

Both tags are registered on headless Core tests in `tests/core/sketch_3d_tests.cpp`; the interaction
service is proven without a Qt viewport, matching the headless-first authority split above.

## Next boundary

Block 121 implements and accepts deterministic tutorial documents, machine-checked tool coverage,
GUI/headless equivalence, exact undo/redo, atomicity, high-DPI mapping, and measured interaction
performance in `docs/interactive-sketcher-mvp8-acceptance.md`. Direct handles for arc/spline/helix
parameters and any variational 3D constraint solving remain bounded later extensions and are not
implied by Block 120. Block 122 begins Interactive Modeling MVP-9.


## Replacement shell: typed point/line editor — Block 136, 2026-10-23

The model ribbon's **3D-Skizze** action (`blcad.action.sketch_3d`) creates a spatial sketch.
Double-clicking its browser node reopens the same sketch. The owned `ShellSketch3DTools` task
(`sketch3d.edit`, dock `blcad.shell.sketch3d_task`) offers model-space XYZ values in millimetres,
a new/existing point selector and start/end selectors for new lines between shared local points.
IDs are allocated across the complete entity namespace. Moving a selected point edits all curves
that reference it; existing arc, spline, polyline, helix and guide intent remains in the candidate.
This surface does not offer curve deletion or endpoint-reference rebinding.

**Punkt übernehmen** and **Linie ergänzen** stage changes locally; they do not dirty the document,
create history or save intent. Several local changes form one Apply transaction. Pending numeric
point edits must be staged or discarded by reselecting a point before Preview/Apply. Numeric inputs
accept signed values within ±1e9 mm with nine decimal places. Unedited points and curves retain
original precision and bindings. Editing a parameter-driven point requires the explicit checkbox
**Parameterbindungen durch XYZ-Werte ersetzen**; that accepted edit replaces all three coordinates
with explicit values. No parameter is deleted.

Preview commits into a disposable copy of the entire value session, using normal catalog validation
and complete Geometry recompute. It presents the candidate Sketch3D, paths and solid/surface outputs.
Apply repeats validation and publishes once. `PartDocument::update_sketch_3d` preserves identity,
collection position and downstream dependency edges, refreshes incoming dependencies, rejects cycles
and revalidates every consuming PathCurve's source identities, connectivity, closure and junctions.
The existing JSON schema remains unchanged. Degenerate geometry or invalid downstream results reject
publication; the local candidate remains correctable. Undo/Redo restores the complete transaction.

The task requires a fresh Part, no planar Sketch editor, no repair mode and no foreign active task.
Escape (including numeric children), dock close, history, saving/revision changes and document/context
replacement discard it. Saving during preview writes committed intent only. Selection masks/scopes
are not changed by this typed editor.

Proofs: `[core][sketch-3d-update]`, `[block136-sketch3d]` and `[shell-combined-sketch3d]`.
The combined scenario uses real Qt ribbon/popup/tree/numeric/button/file input to create four points,
three connected lines, PathCurve and Sweep, reject an endpoint collapse, extend the swept solid,
exercise four-step exact Undo/Redo, cancel another edit, reopen canonically and export STEP. It runs
on both offscreen and native backends; native OCCT presentation and reachable controls at 1280×800
remain required evidence.

This initial typed slice left native collection, direct handles, distance/direction entry and
planar-point projection open. The collection/grip extension below supplies the bounded point/line
interactions; pointer placement, distance/direction and projection remain unfinished.
New arc/spline/helix parameter tools and variational 3D constraints remain later extensions of the
historical bounded Core contract, rather than implied capabilities of this editor.

## Replacement shell: point/line collection and direct grips — Block 136, 2026-10-23

The existing task adds **Modellklick wählt** roles for editing a point, collecting either new-line
endpoint and collecting both endpoints from an existing line. Collection uses the active candidate's
stable local IDs and the viewport's authoritative model-to-screen transform, including newly staged
points. A hit must be within 10 device-independent pixels of a projected point or straight segment;
nearest hits win, with authored order breaking ties. This is explicit active-sketch collection through
the view, including points occluded by a body; it does not use OCCT vertex identity or select foreign
sketches/geometry. Arc/spline/helix picking is outside this bounded point/line surface. Global selection
masks and scopes remain unchanged; the task's explicit collection roles consume their own clicks.

**Punktgriffe anzeigen** enables translation grips for an existing or locally staged selected point.
The **Griffrichtungen** selector exposes XYZ, one X/Y/Z axis or the two independent axes of XY/YZ/ZX.
These restrictions reuse `Sketch3DInteractionService::resolve_point`; a plane restriction does not
provide a free plane-surface drag. Grips use the existing manipulator layer and the actual viewport
ray/projection mapping. Mouse movement and the final release build complete local candidates, with
full downstream previews. Numeric refinement supplies an absolute XYZ coordinate in mm; later motion
or release cannot replace that override. Full-precision untouched components remain intact.

Point grips require staged numeric point edits and explicit parameter-detachment consent when any
coordinate is bound. Each gesture retains a complete pre-drag candidate. Escape first rolls back the
gesture; another Escape closes the task. Invalid numeric release, camera/viewport size changes,
window deactivation and changing point/lock/pick/grip controls roll back the gesture. Editing an
XYZ field also rolls back the gesture and retains the newly typed component as separate pending
input; the other fields return to their pre-gesture values. Revision,
history, Save, document replacement and task cancellation release capture and clear handles. Apply
is blocked during a gesture and requires a valid candidate afterward. A rejected downstream candidate
remains locally correctable; no drag creates live intent or separate history entries.

The extended fully UI-authored Sweep workflow now picks the source endpoint in the viewport, locks
the Z grip, rejects a collapsed segment and refines it to 40 mm before one Apply, preserving the
480→640 mm³ volume proof and four-step history/reopen/STEP sequence. Focused input tests also cover
candidate-only picking, all seven lock modes, precise explicit detachment, raw final-release mapping,
Escape rollback, invalid text and lifecycle/camera/focus boundaries. Both backends execute the same
cases: `[block136-sketch3d-grips]` and `[shell-sketch3d-grips-workflow]`.

Pointer placement of new points, explicit distance/azimuth/elevation entry and planar-point projection
remain the next Sketch3D work. The family and Block 136 remain partial.

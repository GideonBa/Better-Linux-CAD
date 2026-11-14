# Interactive Sketch3D MVP-8

Current extension: [Blocks 163–165](inventor-parity-sequence.md) own persistent spatial
constraints, solver diagnostics and arc/spline/helix interaction. Reuse this existing Sketch3D
authority; the accepted point/line boundary below is not a second implementation queue.

Historical foundation: implemented in Block 120. Numbered extension status belongs to
[`mvp-plan.md`](mvp-plan.md); the Block 165 work below is not complete family acceptance.

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
The **Zeiger-/Griffsperre** selector exposes XYZ, one X/Y/Z axis or the two independent axes of XY/YZ/ZX.
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

This collection/grip slice left pointer placement, explicit distance/azimuth/elevation entry and
planar-point projection open. The anchored placement extension below supplies pointer input;
distance/direction and projection remain unfinished. The family and Block 136 remain partial.

## Replacement shell: anchored pointer placement — Block 136, 2026-10-23

**Modellklick wählt → Neuen Punkt platzieren** stages new explicit points with actual viewport
input. **Platzierungsanker** selects the model origin or a stable local point of the active candidate,
including previously staged points. This selector remains explicit after each placement; selecting
the newly created point for editing does not silently change the anchor.

**Zeiger-/Griffsperre** shares the existing seven restrictions. XYZ intersects the view ray with the
camera-facing plane through the anchor, fixing depth explicitly rather than inferring a surface.
X/Y/Z use the closest point on the corresponding model axis through that anchor. XY/YZ/ZX intersect
the corresponding model plane through it. Nearly view-parallel axes and edge-on planes fail closed;
the task asks for a different view. Both orthographic and perspective mappings use the viewport's
authoritative ray/projection. Perspective points behind the camera and coordinates outside ±1e9 mm
are rejected. There is no automatic topology/surface snapping or planar-reference association.

The transient amber marker and anchor line show the resolved hover point; XYZ values appear in the
task summary. Hover and press do not change the local sketch, document, history or dirty state. A
matching left-button release inside the viewport recomputes from its final sample and stages exactly
one new point through `Sketch3DInteractionService::add_point`. The anchor's full-precision resolved
coordinates are frozen during the gesture. Bound anchors keep their parameter bindings; new points
copy explicit values without a live association or detachment requirement. Stable IDs reuse the
existing complete-entity allocator and unchanged persistence schema.

Pending XYZ text must be staged or discarded before placement, including spin text not yet committed
by focus loss. Apply and Preview are blocked during a captured placement. Existing local edits and
their valid preview survive rejected release, camera/size/focus changes and anchor/lock/mode/control
changes. Editing XYZ cancels capture and keeps that typed edit pending. Mouse navigation remains
available through non-left buttons. Escape first exits placement and retains earlier local edits;
another Escape closes the task. Save/revision/task/document boundaries clear the marker and capture
without publishing an unfinished point. Accepted local points still require the existing complete
downstream Preview and one Apply transaction.

Proofs: `[block136-sketch3d-placement]` covers all seven modes in both projections, bound and local
anchors, final release, prior-preview preservation, pending text, unobservable directions and stale
tasks. `[shell-sketch3d-placement-workflow]` extends the existing fully UI-authored chain/PathCurve/
Sweep scenario: viewport point placement, typed refinement, downstream rejection/recovery, exact
history, canonical reopen and STEP. Native capture/reachability checks remain required at 1280×800.
The distance/direction extension below supplies explicit model-direction input. Planar-point
projection remains next; the family and Block 136 stay partial.

## Replacement shell: distance/direction input — Block 136, 2026-10-23

**Distanz (mm)**, **Azimut (°)** and **Elevation (°)** plus **Punkt aus Richtung ergänzen** create
one new local explicit point relative to **Platzierungsanker**. Origin, existing candidate points
and newly staged points share the same precise anchor resolver as pointer placement. Azimuth starts
at model +X and increases toward +Y; elevation starts at XY and increases toward +Z. The resolved
offset is `(d cos(e) cos(a), d cos(e) sin(a), d sin(e))`. This explicit tuple supersedes pointer/grip
locks and does not need a visible ray or camera gesture. Selecting an existing point for editing
does not turn the command into a move. Bound anchors retain their parameters; new points copy values.

All three fields are required. They accept finite bare numbers with decimal points, optional signs
and scientific notation; units come from the labels. Distance is bounded to 0…1e9 mm, azimuth to
−360…360° and elevation to −90…90°. Resolved XYZ must remain within ±1e9 mm. Invalid or incomplete
text is retained with an error, without clamping, consuming IDs or changing the local candidate.
Zero distance is valid point input; a later degenerate line remains subject to ordinary validation.

Any nonempty tuple field invalidates the previous complete preview and blocks typed point/line
staging, pointer placement, grips and Preview/Apply until the tuple is staged or **Richtungseingabe
leeren** discards its text. Earlier staged points/curves remain intact. Editing a tuple rolls back
an active grip or cancels pointer capture before retaining the text. Conversely, pending XYZ text
must be staged or discarded through point selection before the tuple can create a point. Successful
staging clears the tuple fields, selects the allocated point and preserves the chosen anchor.
The existing complete downstream Preview and single Apply transaction remain required. Escape,
Save/revision/task/document boundaries discard unfinished state and reset tuple text on reopening.
Persistence and Core resolver semantics are unchanged.

Proofs: five `[block136-sketch3d-direction]` cases plus `[shell-sketch3d-direction-workflow]`.
The combined workflow mixes typed XYZ, pointer placement and a direction tuple; it rejects an
incomplete tuple, recovers the endpoint, authors PathCurve/Sweep, rejects/corrects a downstream grip,
checks exact four-step history, canonical reopen and STEP. Native controls/capture are checked at
1280×800. The planar-point extension below supplies the remaining bounded copy surface; Block 136
stays open for the other recorded families and whole-shell acceptance.

## Replacement shell: explicit planar-point copies — Block 136, 2026-10-23

**Planarer Quellpunkt** lists authored line starts/ends and existing projected points from the
current Part's planar Sketches. Each entry retains the stable Sketch ID, entity ID and typed
`SketchReferenceTarget` kind, including equal entity IDs in different Sketches. Standalone Sketch
points, arc/spline points and raw OCCT vertices are outside the accepted target vocabulary.

Geometry resolves each selected point through the current workplane and reference projector,
including principal/arbitrary construction planes, generated-face workplanes and projected semantic
vertices. Both resolvers receive the current shape cache when available. A projected source is first
resolved in its planar Sketch; the resulting planar coordinates are then mapped into model XYZ.
This retains the source Sketch's projection semantics. Non-finite or resolved coordinates outside
±1e9 mm reject locally without consuming a point ID or changing previous local intent.

**Unabhängige XYZ-Kopie bestätigen** is required before each **Quellpunkt nach 3D kopieren**.
Changing the source resets consent. This surface deliberately chooses the historical Core contract's
explicit-copy option: `project_planar_point` returns a new local point plus a typed source receipt.
The task shows the selected source/XYZ values and retains every local point's source receipt until
completion. Apply persists independent explicit XYZ values. These copies retain their values after
source edits; no dependency or source receipt is persisted. Reopening shows no stale provenance or
consent. Live associative projection would require a separate persistent contract.

Selection and consent alone preserve earlier local previews. Changing either rolls back a live grip
or cancels pointer capture. Copying requires staged/discarded XYZ text, empty direction fields and a
finished gesture; it re-resolves the current source before staging through the Core service. Several
copies and other local edits still require complete downstream Preview and one Apply transaction.
Copying resets consent, selects the new local point and keeps the explicit placement anchor unchanged.
Save/revision/task/document boundaries and Escape clear the source controls and local receipts.

Six `[block136-sketch3d-projection]` cases cover four oriented planes, projected construction points,
cached generated faces/semantic vertices, precision, source identity, per-copy consent, pending input,
gesture rollback, coordinate bounds, source-edit independence, exact history and persistence/expiry.
`[shell-combined-projected-sweep]` adds a fully UI-authored workflow: XZ source and XY profile Sketches,
two explicit endpoint copies, a direction point, shared lines, PathCurve/Sweep, failed/corrected
endpoint edits, four-step history, canonical reopening and STEP. Inspect native controls and receipts
at 1280×800. This restores the bounded Sketch3D/PathCurve family; Block 136 remains in progress.

## Solver-backed task foundation — 165

The existing task owns one accepted local Sketch3D. Editing a constrained owner starts from its
current derived geometry without publishing seed normalization. Typed point edits and point grips
pin all three requested coordinates exactly; the solver may move other free components or reject
the edit. Explicit parameter-detachment consent remains required. Rejected typed values remain
correctable and block Preview; rejected final grip release restores the complete pre-gesture draft,
including earlier local edits. Persistent constraints, shared parameter values and formulas remain
unchanged. DOF reports distinguish total, admissible rigid and internal freedom without transient
interaction pins.

An immutable editing context evaluates current source intent before the first executable consumer,
limited further by any active End-of-Part prefix. Graph/source/cycle validation precedes numerical
work; external constrained owners resolve from the same source scope. Imported authorities retain
the saved document root and byte-freshness checks. Pending owned resources are local until the same
final transaction adds them with the Sketch3D; they never mutate existing parameters.

Preview uses a disposable complete session and validates the entire downstream feature suffix,
including when the real session is in a prefix context. Presentation filtering respects hidden
inputs. Removed source presentations are included in the replacement set; Cancel restores the
committed scene. Apply repeats validation and publishes one operation. Failed publication preserves
the document and undo history, while its diagnostic invalidates borrowed session contexts.

[Task/session proofs](../tests/gui/gui_spatial_solver_interaction_tests.cpp) verify exact typed
positions, independent wire length, conflict correction preserving earlier edits, graph-cycle
rejection, Undo/Redo and canonical reopening. Native source collection and complete native S03–S04
acceptance remain the remaining work within 165. Typed authoring and curve grips extend this task.

## Typed spatial curve forms — 165

The fixed Points/Curves register bar and fixed DOF/Preview/Apply/Cancel area surround independently
scrollable forms. Curves create, edit, delete and explicitly rebind lines, polylines, three-point
arcs, fit/control splines, helices and parametric 3D equations. Existing identities and curve types
remain fixed during edits. Ordered selectors retain typed local, construction-point or planar-point
references. Linked references keep their source dependencies; the historical Points-tab planar copy
still creates independent XYZ values. Local defining points reuse the existing point/triad editor.

Helix radius/pitch may be owned Length values or existing Length parameters. Bound values display
their current resolved values and remain read-only until an explicit source change. Choosing a
literal creates a curve-owned value; it never edits or removes the former shared parameter/formula.
Turns select an existing integer Count or a new owned Count. Principal axes and new Counts remain
in a local resource ledger until the same final Apply transaction. Cancel discards them. Windings
remain discrete sources, outside numerical drag variables.

Equation forms retain variable name/unit, typed domain, XYZ expressions, modeling tolerance,
closure and construction intent. Derived poles are not editable. Internal spline C0/C1/C2 remains
distinct from an endpoint Smooth/G2 relation. Guide aliases can be created, removed or rebound to
an existing curve with their stable identity; referenced curves cannot be deleted implicitly.
Incomplete/rejected forms remain correctable, block competing point edits and Preview, and change
neither the accepted local draft nor the document. Accept stages through the shared solver/context;
Preview revalidates all downstream geometry and Apply creates one history operation.

[Curve form tests](../tests/gui/gui_spatial_curve_authoring_tests.cpp) cover every family, linked
reference rebinding, formula preservation, point-free Helix resource ownership, metadata guards,
correction, exact history and reopening. The native case checks the visible footer, viewport,
vertical-only scrolling, preview and complete Cancel at ordinary and compact/scaled sizes.

## Spatial constraints and dimensions — 165

The Constraints register creates, edits, deletes and explicitly rebinds all sixteen relations in
[the solver contract](part-sketch-3d-core-mvp6.md#spatial-solver-boundary--164). Ordered target
selectors distinguish points, curve endpoints/whole curves, straight directions, axes, planes and
faces. The source catalog checks actual supported Geometry at the task's pre-consumer scope and
excludes cyclic descendants. Native/imported/result faces retain typed identities. Existing
references are retained as typed current-target choices until explicitly rebound.

Dimensions select an owned literal in mm/degrees or an existing compatible shared/formula parameter;
changing the binding never writes that parameter. Oriented angles add an explicit third axis.
Fixed anchors may copy the current target position exactly or accept explicit XYZ components.
Point-on-curve retains its authored normalized branch seed. Unedited literals, anchor components,
branch seeds, Helix scalars and equation modeling tolerance retain their full stored precision,
independently of the displayed decimal count. Incomplete numeric text survives focus changes;
Accept validates it before solving and preserves the accepted local draft on failure.

DOF distinguishes total, admissible rigid and internal freedom; bounded per-ID conflict/redundancy
feedback accompanies the report. Endpoint inspection displays actual native unit tangents and normal
curvature vectors in inverse mm. A fixed G1-only join rejects Smooth/G2 and can be corrected to
Tangent without publishing the failed relation. These curve diagnostics do not claim surface-boundary
G2 Loft or upgrade a saved path policy.

Curve and constraint forms share the existing task's local authority, resources, complete downstream
preview and single Apply operation. Unaccepted forms block competing edits and publication;
Discard clears only their form input. Closed/stale document contexts invalidate the task rather than
reusing former target choices. [GUI physical/continuity/selector proofs](../tests/gui/gui_spatial_constraint_authoring_tests.cpp)
check a dimension-driven native Sweep, exact Fixed anchors, G1-only rejection from actual derivatives,
all relation modes, numeric correction, dependency guards, Undo/Redo and reopening. Native runs also
check visible actions and vertical scrolling at ordinary and compact/scaled sizes. Direct native
source picking and complete combined S03–S04 remain the next interaction work.

## Radius, pitch and defining-point grips — 165

Curves offers radius/pitch handles for the selected existing curve. The same fixed task area hosts
the absolute handle-value input for both Points and Curves. Selecting a curve or toggling handles
retains an unchanged valid preview; switching registers cancels capture before changing its mode.
Native camera/ray mapping remains authoritative at ordinary and scaled resolutions.

Arc radius derives its center, plane, direction and radius from the actual circular edge. It scales
all three local, unbound defining points about the frozen center and pins their nine coordinates
while the solver adjusts other free variables. Referenced/parameter-bound defining points require
explicit rebinding/detachment first. Fit/control spline and arc defining points reuse the existing
local point selector and translation triad, preserving their stable reference identities.

Helix handles expose only owned radius/pitch values. Shared/formula bindings remain read-only until
explicitly rebound. Radius uses the actual starting radial frame; pitch uses the native endpoint
and resolved axis, with world movement scaled by the immutable integer turn count. Changing one
scalar preserves the other binding, handedness, axis and Count source. No continuous turns grip or
implicit shared-parameter write exists. Numeric scalar input accepts finite **1e-9…1e9 mm** values;
invalid text rejects before the generic manipulator's clamping behavior can change the request.

Every move and final release rebuilds from the complete pre-gesture source. Transient hard pins keep
requested values exact; failed solve/downstream preview rejects the whole gesture. Escape, lost
mouse capture, camera/size/focus changes and curve selection restore earlier local edits and their
valid preview. Apply remains one final operation for the accepted draft. Cancel or a stale task
context publishes nothing. Geometry products, glyph frames and pins are transient.

[Curve-grip proofs](../tests/gui/gui_spatial_curve_grip_tests.cpp) check actual endpoint radius/height
and curvature-derived circle radius, exact owned values and point coordinates, discrete/shared
source preservation, successful arc/spline editing, incompatible-radius rollback, six interruption
paths and exact Undo/Redo/reopening. Existing point-grip regression runs with these cases. Historical
path sampling and the separate verified Surface G2 work remain unchanged requirements of 166/169;
direct native source collection and combined S03–S04 remain required before whole-165 acceptance.

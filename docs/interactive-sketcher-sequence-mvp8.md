# Interactive Sketcher Sequence MVP-8

Status: in progress. Blocks 106–114 are implemented; Block 115 is the current next technical step.
Blocks 106–121 precede Interactive Modeling MVP-9 (Blocks 122–131) and STEP Import MVP-10
(Blocks 132–138).

This phase turns the Block-99 validation surface into a productive, Inventor-familiar Sketch
workbench. The target is direct manipulation, visible constraints, in-canvas dimensions, predictable
commands, and immediate feedback without moving model, solver, Geometry, or persistence authority
into Qt.

## Product outcome

A user can enter a planar Sketch, create common mechanical profiles, edit curves directly, add manual
or inferred geometric constraints, inspect semantic glyphs and conflicts, create parameter-backed
annotations, repair conflicts, and leave the Sketch with atomic undoable document results. Persistent
intent remains available to headless Core/Geometry consumers.

## Frozen authority rules

1. Screen coordinates, hover state, snaps, rubber bands, control polygons, selected handles, sampled
   curves, glyph placement, and font rasterization are transient.
2. Shared Sketch points, entities, accepted constraints, dimensions, annotations, and
   construction/reference state are stable semantic intent.
3. Numeric coordinate equality is not topology connectivity.
4. The planar solver is a deterministic headless Core service.
5. Dragging, editing, and constraint authoring operate on disposable complete candidates; successful
   acceptance commits one document transaction.
6. Fixed, reference, stale, ambiguous, conflicting, redundant, invalid, or non-convergent candidates
   fail closed.
7. Automatic constraints remain transient inference until Block-114 disposable solve accepts them.
8. Projected geometry remains associative/read-only until explicit conversion.
9. Historical persistence is never silently reinterpreted; new schemas are explicit and versioned.
10. Derived solve, Geometry, font, layout, glyph, and interaction products are never opaque persistent
    caches.

## Frozen phase order

```text
106 Sketch workspace, interaction state, command HUD, usability contract — implemented
107 plane mapping, hit testing, box selection, grid, snapping, inference preview — implemented
108 shared planar point/entity topology, mutation commands, JSON migration, undo — implemented
109 deterministic planar constraint solver, DOF accounting, conflicts, diagnostics — implemented
110 solver-backed mouse dragging, handles, live preview, atomic commit — implemented
111 point, line, polyline, rectangle, polygon, construction-geometry creation — implemented
112 circle, arc, ellipse, slot creation/editing — implemented
113 spline editing, continuity handles, Sketch text — implemented
114 manual and automatic geometric constraints with glyph interaction — implemented
115 driving/reference dimensions, in-canvas editing, parameter/expression binding — next
116 trim, extend, split, corner fillet, corner chamfer
117 offset, project/include, construction axes, associative references
118 move, rotate, scale, copy, mirror, rectangular/circular Sketch patterns
119 region recognition, profile selection, diagnostics, repair, Finish Sketch
120 interactive Sketch3D creation and direct point/curve manipulation
121 integrated usability, persistence, solver, performance, GUI/headless acceptance
122–131 Interactive Part & Assembly Modeling MVP-9
132–138 STEP Import MVP-10
```

## Block 106 — Sketch workspace and interaction contract — Implemented

`GuiWorkspace::Sketch` and `GuiSketchWorkspace` project Sketch-specific stages onto generic task and
transaction authority. Enter/Finish Sketch preserve workspace, selection, filters, and camera state.
The contextual surface exposes Create, Constrain, Dimension, Modify, and Project groups plus browser,
status, and numeric-HUD products.

Canonical contract: `docs/gui-interactive-sketch-workspace-mvp8.md`.

Focused tags: `[gui][sketch-workspace]`, `[gui][sketch-command-lifecycle]`.

## Block 107 — Plane interaction, hit testing, snapping, and inference — Implemented

`GuiSketchPlaneMapping` provides device-independent Screen-DIP/view-ray/active-plane mapping.
Deterministic hit priority is `Point -> Curve -> Dimension -> Glyph`. Window/Crossing selection,
bounded grid display, geometric snaps, and horizontal/vertical/X/Y alignment inference remain
transient GUI products.

Canonical contract: `docs/gui-sketch-plane-interaction-mvp8.md`.

Focused tags: `[gui][sketch-hit-test]`, `[gui][sketch-snap]`, `[gui][sketch-box-selection]`.

## Block 108 — Shared planar point topology — Implemented

`SketchPointId`, `SketchTopologyPoint`, `SketchTopologyEntity`, and `SketchTopology` provide stable
shared identity. Historical embedded `Point2` records migrate deterministically from explicit ordered
profile connectivity only; unrelated equal coordinates remain distinct point ids.

`SketchEditCommandExecutor` implements dependency-safe Add/Move/Replace/Remove with complete before
and after snapshots. Canonical topology persistence is `blcad.sketch_topology.mvp8`, version 1.

Canonical contract: `docs/sketch-shared-topology-mvp8.md`.

Focused tags: `[core][sketch-topology]`, `[core][sketch-edit-command]`,
`[core][sketch-json-migration]`.

## Block 109 — General planar constraint solver — Implemented

`SketchConstraintSolver` deterministically solves Coincident, Fixed, Horizontal, Vertical, Parallel,
Perpendicular, Collinear, Equal, Tangent, Concentric, Midpoint, Symmetric, PointOnObject, horizontal/
vertical/aligned distances, Radial, Diameter, and Angular families over Block-108 topology. Canonical
variable/constraint ordering, normalized residuals, damped Gauss-Newton execution, Jacobian-rank DOF,
redundancy attribution, and stable conflict diagnostics are headless derived authority.

Canonical contract: `docs/sketch-planar-constraint-solver-mvp8.md`.

Focused tags: `[core][sketch-solver]`, `[core][sketch-dof]`,
`[core][sketch-conflict-diagnostics]`.

## Block 110 — Solver-backed mouse dragging — Implemented

Semantic endpoint, midpoint, center, radius, arc, spline-control, and dimension-target handles resolve
to persistent topology roles. Pointer samples coalesce into disposable solve candidates; release
flushes the exact final position and commits one `Drag sketch handle` transaction. Cancel, lost
capture, stale source, reference geometry, or failed solve restore the original document.

Canonical contract: `docs/gui-sketch-solver-drag-mvp8.md`.

Focused tags: `[gui][sketch-drag]`, `[integration][sketch-live-solve]`.

## Block 111 — Basic creation tools — Implemented

Point, two-point line, continuous polyline, rectangle families, parallelogram, regular polygon, and
centerline creation reuse the frozen command lifecycle and Block-107 snapping. Composite tools expand
into ordinary lines and closed profiles; Point and Centerline create construction geometry. Numeric
entry supports absolute, relative, and polar picks.

Canonical contract: `docs/gui-sketch-basic-creation-mvp8.md`.

Focused tags: `[gui][sketch-create-basic]`, `[integration][sketch-basic-profile]`.

## Block 112 — Circles, arcs, ellipses, and slots — Implemented

Center-radius/diameter, two-point, three-point, and tangent-circle families; center/start/end,
three-point, and tangent arcs; ellipse/elliptical-arc families; and center/overall slots are implemented.
Full circles persist `CircleProfile` plus diameter `Parameter`. Arcs use `ArcSegment`; ellipses use
deterministic cubic `SplineSegment` spans; slots use ordered line/arc `ArcClosedProfile` contours.
Preview sampling and tangent inference remain transient until an accepted Block-114 candidate exists.

Canonical contract: `docs/gui-sketch-conic-slot-creation-mvp8.md`.

Focused tags: `[core][sketch-conics]`, `[geometry][sketch-conics]`,
`[gui][sketch-create-conics]`.

## Block 113 — Spline editing and Sketch text — Implemented

Block 113 keeps cubic `SplineSegment` as the sole persistent planar spline representation and adds
connected-chain control/fit-point editing, deterministic conversion, insertion/removal, control
polygons, C0/G1 continuity handles, curvature diagnostics, immutable GUI preview, source freshness,
and one `Edit sketch spline` transaction.

It also adds parameter/expression-backed `SketchText` intent with canonical
`blcad.sketch_text.mvp8`, version-1 sidecar persistence and deterministic requested/system/built-in
vector-font fallback diagnostics.

Canonical contract: `docs/gui-sketch-spline-text-mvp8.md`.

Focused tags: `[core][sketch-spline-edit]`, `[geometry][sketch-spline-edit]`,
`[gui][sketch-spline]`, `[core][sketch-text]`, `[geometry][sketch-text]`.

## Block 114 — Constraint authoring and inference UX — Implemented

Block 114 exposes every non-dimensional Block-109 family through stable topology targets:
Coincident, Fixed, Horizontal, Vertical, Parallel, Perpendicular, Collinear, Equal, Tangent,
Concentric, Midpoint, Symmetric, and PointOnObject.

`SketchConstraintIntent` freezes stable id, family, ordered point/entity targets, and
`manual|automatic` provenance. `SketchConstraintCatalog` provides deterministic per-Sketch ordering and
canonical `blcad.sketch_constraints.mvp8`, version-1 sidecar persistence. Historical
`blcad.part_document.mvp1` remains unchanged.

`GuiSketchConstraintController::compatible_kinds(...)` derives only selection-compatible commands.
`automatic_candidate(...)` maps Horizontal/Vertical inference, Endpoint/Intersection, Midpoint, Center,
and Nearest snaps into candidates with explicit semantic targets. No snap commits implicitly.

`SketchConstraintAuthoringService` combines historical constraints, accepted sidecar intent, and the
candidate into one Block-109 solve. Only `under_constrained` and `fully_constrained` materialize a
complete candidate Sketch. Redundant, conflicting, invalid-reference, and non-convergent results remain
preview-only and publish stable ids without source mutation.

`SketchConstraintGlyphLayoutResolver` and the GUI controller publish deterministic accepted, preview,
conflict, and redundancy glyphs as `GuiSketchHitKind::Glyph` annotations. Commit rechecks Part,
topology, and catalog freshness and performs one `Add sketch constraint` Part transaction. Coordinated
undo/redo restores both complete Sketch and sidecar catalog snapshots.

Canonical contract: `docs/gui-sketch-constraint-authoring-mvp8.md`.

Focused tags: `[core][sketch-constraints]`, `[geometry][sketch-constraints]`,
`[gui][sketch-constraints]`, `[integration][sketch-auto-constraint]`.

## Block 115 — Dimensions and expressions — Next

Add horizontal, vertical, aligned, point-to-point, length, radius, diameter, angle, and arc-length
dimensions plus driving/reference mode, in-canvas editing, and parameter/expression binding. Dimension
values remain typed Core `Parameter`/`Quantity` intent rather than widget text.

Focused tags: `[core][sketch-dimensions]`, `[gui][sketch-dimensions]`,
`[integration][sketch-expression-edit]`.

## Block 116 — Trim, extend, split, fillet, and chamfer

Implement trim, extend, split, two-entity Sketch fillet, and Sketch chamfer over complete candidate
topology. Ambiguous intersections or reference geometry never partially mutate the Sketch.

Focused tags: `[core][sketch-modify]`, `[geometry][sketch-modify]`,
`[gui][sketch-trim-extend]`.

## Block 117 — Offset, projection, and construction references

Add chain/loop offset, supported associative projection, explicit break-link conversion, and matching
lost/ambiguous reference workflow.

Focused tags: `[core][sketch-offset-project]`, `[geometry][sketch-offset-project]`,
`[gui][sketch-project]`.

## Block 118 — Transform, mirror, and Sketch patterns

Add move, rotate, scale, copy, mirror, rectangular pattern, and circular pattern for selected Sketch
geometry. Associative pattern intent is explicit; exploded copies are ordinary entities.

Focused tags: `[core][sketch-transform-pattern]`, `[gui][sketch-transform-pattern]`.

## Block 119 — Regions, profiles, diagnostics, and Finish Sketch

Continuously recognize bounded regions from solved visible non-construction geometry, expose profile
selection and open/self-crossing diagnostics, and complete the solver-aware Finish Sketch workflow.

Focused tags: `[geometry][sketch-regions]`, `[gui][sketch-profile-selection]`,
`[integration][sketch-finish]`.

## Block 120 — Interactive Sketch3D

Add orthogonal triad/plane locks, model-space point/line placement, typed XYZ/distance/angle input, 3D
curve handles, guide roles, and projection of planar Sketch points. This does not imply a full
variational 3D constraint solver.

Focused tags: `[gui][sketch-3d-edit]`, `[integration][sketch-3d-direct-manipulation]`.

## Block 121 — Interactive Sketcher acceptance

Add deterministic tutorial documents and a coverage manifest for every planned Sketch tool. Prove
mouse/script equivalence, persistence/recompute, exact undo/redo, conflict atomicity, reference repair,
keyboard Apply/Cancel, high-DPI mapping, and no stale preview publication. Measure representative hover,
drag, solve, and region-recognition performance.

Focused tags: `[integration][interactive-sketcher]`, `[integration][sketch-gui-headless]`,
`[performance][sketch-interaction]`.

## Coverage matrix through Block 114

| Sketch capability | Canonical contract | Interactive owner |
|---|---|---:|
| Sketch workspace and command lifecycle | `gui-interactive-sketch-workspace-mvp8.md` | 106 |
| Plane mapping, hit testing, snapping | `gui-sketch-plane-interaction-mvp8.md` | 107 |
| Shared point/entity topology | `sketch-shared-topology-mvp8.md` | 108–121 |
| Planar solver and DOF | `sketch-planar-constraint-solver-mvp8.md` | 109–121 |
| Solver-backed handles and drag | `gui-sketch-solver-drag-mvp8.md` | 110 |
| Basic line/profile creation | `gui-sketch-basic-creation-mvp8.md` | 111 |
| Circles, arcs, ellipses, slots | `gui-sketch-conic-slot-creation-mvp8.md` | 112 |
| Spline editing and continuity | `gui-sketch-spline-text-mvp8.md` | 113 |
| Parameter-backed Sketch text and font fallback | `gui-sketch-spline-text-mvp8.md` | 113 |
| Constraint intent, inference, glyphs, conflict preview | `gui-sketch-constraint-authoring-mvp8.md` | 114 |
| Dimensions and expressions | dimension/parameter contracts | 115 |
| Trim/extend and corner modification | arc/trim contracts | 116 |
| Offset/project/reference recovery | projection/reference contracts | 117 |
| Sketch transforms/patterns | pattern contracts | 118 |
| Automatic regions and Finish Sketch | region/repair contracts | 119 |
| Sketch3D interaction | Sketch3D contracts | 120 |

After Block 121, Interactive Part & Assembly Modeling MVP-9 begins at Block 122. STEP Import MVP-10
follows in Blocks 132–138.

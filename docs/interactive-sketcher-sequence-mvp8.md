# Interactive Sketcher Sequence MVP-8

Status: in progress. Blocks 106–116 are implemented; Block 117 is the current next technical step.
Blocks 106–121 precede Interactive Modeling MVP-9 (Blocks 122–131) and STEP Import MVP-10
(Blocks 132–138).

This phase turns the Block-99 validation surface into a productive, Inventor-familiar Sketch
workbench. The target is direct manipulation, visible constraints, in-canvas dimensions, predictable
commands, and immediate feedback without moving model, solver, Geometry, or persistence authority
into Qt.

## Product outcome

A user can enter a planar Sketch, create common mechanical profiles, edit curves directly, add manual
or inferred geometric constraints, author typed driving/reference dimensions, edit bound values or
expressions through semantic annotations, inspect glyphs and conflicts, repair topology, and leave the
Sketch with atomic undoable document results. Persistent intent remains available to headless
Core/Geometry consumers.

## Frozen authority rules

1. Screen coordinates, hover state, snaps, rubber bands, control polygons, selected handles, sampled
   curves, glyph placement, formatted dimension text, and font rasterization are transient.
2. Shared Sketch points, entities, accepted constraints, dimension targets/modes/bindings, annotations,
   and construction/reference state are stable semantic intent.
3. Numeric coordinate equality is not topology connectivity.
4. The planar solver is a deterministic headless Core service.
5. Dragging, editing, constraint authoring, and dimension authoring operate on disposable complete
   candidates; successful acceptance commits one document transaction.
6. Fixed, reference, stale, ambiguous, conflicting, redundant, invalid, measurement-mismatched, or
   non-convergent candidates fail closed.
7. Automatic constraints remain transient inference until Block-114 disposable solve accepts them.
8. Projected geometry remains associative/read-only until explicit conversion.
9. Historical persistence is never silently reinterpreted; new schemas are explicit and versioned.
10. Derived solve, Geometry, font, layout, glyph, measurement, and interaction products are never opaque
    persistent caches.

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
115 driving/reference dimensions, in-canvas editing, parameter/expression binding — implemented
116 trim, extend, split, corner fillet, corner chamfer — implemented
117 offset, project/include, construction axes, associative references — next
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
transient GUI products. Sketch selections preserve the exact hit `candidate_id` in addition to the
persistent semantic id, allowing separate endpoint roles on one entity.

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

Block 115 maps all driving dimensions onto these existing families. Arc length uses an iterative
Block-115 equivalent-radius calibration followed by final measured-value validation rather than a
parallel Qt or Geometry solver.

Canonical contract: `docs/sketch-planar-constraint-solver-mvp8.md`.

Focused tags: `[core][sketch-solver]`, `[core][sketch-dof]`,
`[core][sketch-conflict-diagnostics]`.

## Block 110 — Solver-backed mouse dragging — Implemented

Semantic endpoint, midpoint, center, radius, arc, spline-control, and dimension-target handles resolve
to persistent topology roles. Pointer samples coalesce into disposable solve candidates; release
flushes the exact final position and commits one `Drag sketch handle` transaction. Cancel, lost
capture, stale source, reference geometry, or failed solve restore the original document.

Session-backed drag now composes accepted Block-114 constraints and Block-115 driving dimensions into
the baseline. Release rechecks both catalogs and rejects any final topology that violates a driving
measurement, including arc-length dimensions.

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
conflict, and redundancy glyphs as `GuiSketchHitKind::Glyph` annotations. Selecting one accepted
glyph enables atomic `Remove sketch constraint` deletion. `GuiDocumentSession` owns constraint
catalogs through Save/Open, dirty state, global history, and later drag solves.

Canonical contract: `docs/gui-sketch-constraint-authoring-mvp8.md`.

Focused tags: `[core][sketch-constraints]`, `[geometry][sketch-constraints]`,
`[gui][sketch-constraints]`, `[integration][sketch-auto-constraint]`.

## Block 115 — Dimensions and expressions — Implemented

Block 115 adds horizontal, vertical, aligned, point-to-point, line-length, arc-radius, arc-diameter,
angle, and calibrated arc-length dimensions over stable Block-108 point/entity targets.

`SketchDimensionIntent` freezes stable id, family, ordered targets, `driving|reference` mode, and an
optional typed `ParameterId`. Driving dimensions require an existing compatible Length/Angle parameter
or expression and contribute to the combined Block-109 solve. Reference dimensions contribute no
residual and measure current solved topology only.

`SketchDimensionCatalogSystemBuilder` composes historical constraints, Block-114 constraints, and all
driving dimensions. Arc length uses iterative equivalent-radius calibration and final actual-length
validation. Only under-/fully-constrained, measurement-valid, exactly materializable candidates publish.

`GuiSketchDimensionController` provides semantic preview annotations, compatibility/parameter lists,
atomic add/edit/rebind/mode transactions, exact undo/redo, and later-drag enforcement. Literal editing
updates direct parameters; formula editing uses the existing expression graph. The Qt binder exposes all
nine families plus reference creation and annotation editing without owning values or formulas.

Canonical persistence is `blcad.sketch_dimensions.mvp8`, version 1, stored by `GuiDocumentSession` as
`<part>.sketch-dimensions.json`. Dimension catalogs participate in dirty state, Save/Open, and every
history snapshot; measured values and glyph layout remain derived.

Canonical contract: `docs/gui-sketch-dimension-authoring-mvp8.md`.

Focused tags: `[core][sketch-dimensions]`, `[geometry][sketch-dimensions]`,
`[gui][sketch-dimensions]`, `[integration][sketch-expression-edit]`,
`[integration][sketch-live-solve]`.

## Block 116 — Trim, extend, split, fillet, and chamfer — Implemented

`SketchModifyService` (Core) rewrites a disposable candidate Sketch for trim, extend, split, two-line
fillet, and two-line chamfer using analytic line/arc intersection and De Casteljau spline splitting.
Trim shortens to the picked bounding intersection, removes a bounded middle as a split, or removes an
entity with no bounding intersection; extend moves the picked end to the nearest intersection beyond
it; fillet/chamfer trim both lines to the setback and insert a tangent arc or connector line.

In-place edits keep entity ids so referencing constraints, dimensions, and tangent continuity are
preserved; removed/split source ids that are referenced by embedded intent, and any modification of a
profile-contour entity, fail closed with an explicit diagnostic before any candidate is built.
`GuiSketchModifyController` previews without mutation and commits one atomic transaction that
re-derives the operation from the current Sketch and re-validates the Block-114 and Block-115
catalogs, so a modification dropping a catalog-referenced entity fails closed.

Canonical contract: `docs/gui-sketch-modify-mvp8.md`.

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

## Coverage matrix through Block 116

| Sketch capability | Canonical contract | Interactive owner |
|---|---|---:|
| Sketch workspace and command lifecycle | `gui-interactive-sketch-workspace-mvp8.md` | 106 |
| Plane mapping, hit testing, snapping | `gui-sketch-plane-interaction-mvp8.md` | 107 |
| Shared point/entity topology | `sketch-shared-topology-mvp8.md` | 108–121 |
| Planar solver and DOF | `sketch-planar-constraint-solver-mvp8.md` | 109–121 |
| Solver-backed handles and drag | `gui-sketch-solver-drag-mvp8.md` | 110, 114–115 consumers |
| Basic line/profile creation | `gui-sketch-basic-creation-mvp8.md` | 111 |
| Circles, arcs, ellipses, slots | `gui-sketch-conic-slot-creation-mvp8.md` | 112 |
| Spline editing and continuity | `gui-sketch-spline-text-mvp8.md` | 113 |
| Parameter-backed Sketch text and font fallback | `gui-sketch-spline-text-mvp8.md` | 113 |
| Constraint intent, inference, glyphs, conflict preview | `gui-sketch-constraint-authoring-mvp8.md` | 114 |
| Dimensions, typed parameters, expressions, annotation editing | `gui-sketch-dimension-authoring-mvp8.md` | 115 |
| Trim/extend/split and corner fillet/chamfer | `gui-sketch-modify-mvp8.md` | 116 |
| Offset/project/reference recovery | projection/reference contracts | 117 |
| Sketch transforms/patterns | pattern contracts | 118 |
| Automatic regions and Finish Sketch | region/repair contracts | 119 |
| Sketch3D interaction | Sketch3D contracts | 120 |

## Explicit deferrals

Deliberately outside Blocks 106–121; none of these may be introduced implicitly by a later block:

- automatic tracing from raster images, unconstrained freehand drawing, and handwriting recognition;
- proprietary Inventor command names, icons, shortcuts, or exact layout cloning;
- a full variational 3D Sketch constraint solver;
- collaboration cursors, multi-user concurrent editing, and manufacturing-specific annotations.

Scope narrowed against the original per-block planning; each item needs its own explicit later
boundary instead of silent inclusion:

- a curved/three-point **arc slot** family (Block 112 implements the two straight slot families);
- an analytic persistent **ellipse entity** — ellipses persist as deterministic cubic
  `SplineSegment` spans per `docs/gui-sketch-conic-slot-creation-mvp8.md`;
- geometrically solved tangent circle/arc **construction**; tangency is authored as an explicit
  accepted Block-114 constraint, never inferred from coordinates;
- spline **open/closed-state toggling** and explicit **self-intersection diagnostics** (Block 113
  edits open chains; degenerate/stale candidates fail closed);
- Sketch text **alignment, multiline layout, letter spacing, and baseline/path placement**, and
  conversion of glyph outlines into **closed selectable profile loops** — Block 113 text uses one
  anchor, height/rotation parameters, and stroke-vector layout;
- constraint **suppression** (no Core suppression intent exists for planar constraints) —
  Block 114 offers add, inspect, locate, and delete;
- dimension **extension lines, arrows, text-collision avoidance, and draggable annotation
  positions**; Block 115 publishes one semantic anchor annotation per dimension;
- dimensions targeting full **CircleProfile** records; radius/diameter/arc-length target
  three-point arcs until shared topology covers circle-profile entities.

After Block 121, Interactive Part & Assembly Modeling MVP-9 begins at Block 122. STEP Import MVP-10
follows in Blocks 132–138.

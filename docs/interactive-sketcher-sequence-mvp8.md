# Interactive Sketcher Sequence MVP-8

Status: in progress. Blocks 106–110 are implemented; Block 111 is the current next technical step.
Blocks 106–121 precede Interactive Modeling MVP-9 (Blocks 122–131) and STEP Import MVP-10
(Blocks 132–138).

This phase turns the Block-99 validation surface into a productive, Inventor-familiar Sketch
workbench. The target is direct manipulation, visible constraints, in-canvas dimensions, predictable
commands, and immediate feedback without moving model or solver authority into Qt.

## Product outcome

A user can enter a planar Sketch, create common mechanical profiles without authoring JSON, drag
unconstrained points, watch constrained geometry solve continuously, add/edit dimensions in the
canvas, repair conflicts, and leave the Sketch with one undoable document result. Persistent intent
remains available to headless Core/Geometry consumers.

The phase also gives implemented Sketch3D points and curves direct manipulation. Unsupported tools
remain visibly unavailable; the sequence does not claim commercial-CAD parity before acceptance.

## Core reason for the phase order

The historical planar model embeds `Point2` coordinates directly in curve/profile records. Equal
coordinates are not shared identity and the historical diagnostic layer was not a general constraint
solver. A GUI-only drag implementation would therefore tear connected profiles apart or produce
results that headless recompute cannot reproduce.

Blocks 108–110 deliberately establish:

```text
shared point/entity topology
  -> deterministic general planar solver
  -> transactional solver-backed drag
```

The viewport is a client of those authorities.

## Frozen Sketch workspace and mouse contract

```text
+--------------------------------------------------------------------------------+
| Finish Sketch | Undo/Redo | Create | Constrain | Dimension | Modify | Project  |
+----------------------+---------------------------------------------------------+
| Sketch browser       | constraint glyphs, dimensions, origin and axes          |
| - entities           |                                                         |
| - constraints        |                  planar Sketch canvas                    |
| - dimensions         |                                                         |
| - diagnostics        |                                                         |
|----------------------|                                                         |
| contextual task      |                                                         |
| panel / numeric HUD  |                                                         |
+----------------------+---------------------------------------------------------+
| tool hint | cursor coordinates | snap/inference | remaining DOF | solve status  |
+--------------------------------------------------------------------------------+
```

Mouse conventions:

- left click selects or places; drag moves a handle or box-selects from empty canvas;
- `Esc` backs out one command stage and never silently commits;
- right click opens contextual actions and may repeat the last compatible command;
- `Ctrl` toggles selection and `Shift` adds selection;
- `Delete` invokes validated remove intent rather than erasing presentation objects;
- numeric typing during creation or drag opens the transient numeric HUD;
- middle drag pans, wheel zooms, and normal-to-plane Sketch editing keeps orbit disabled until the
  user deliberately leaves normal view;
- double click edits a dimension or supported entity property; `Enter` accepts numeric input.

## Non-negotiable architecture rules

1. Screen coordinates, hover state, snap candidates, rubber-band previews, interaction samples, and
   glyph placement are transient GUI state.
2. Shared Sketch points, entities, constraints, dimensions, and construction/reference state are Core
   intent with stable ids and canonical persistence.
3. Numeric coordinate equality is not topology connectivity. Distinct point ids may be coincident.
4. The solver is a deterministic headless Core service; widgets do not own constraint mathematics.
5. Dragging solves a disposable candidate. Release commits one document transaction; `Esc`, lost
   capture, or failed solve restores the pre-drag snapshot.
6. Fixed or fully constrained geometry refuses incompatible drag instead of deleting constraints.
7. Automatic constraints are transient inference candidates until accepted.
8. Projected geometry remains associative/read-only until explicit conversion.
9. Every creation/edit workflow has keyboard-accessible Apply/Cancel and a headless command path.
10. Solver failure, non-convergence, ambiguity, or over-constraint publishes diagnostics and no partial
    persistent mutation.
11. Historical PartDocument Sketch JSON migrates deterministically through an explicit Core boundary;
    save/load never depends on GUI session state.
12. Solver results, Jacobians, residuals, rank, DOF, and conflict/redundancy diagnostics are derived and
    never become an opaque persistent solve cache.

## Canonical authority rule

Every block below names the existing authorities it may consume. A capability not present in those
contracts or explicitly introduced by the block does not exist for that block. New persistent intent
must be declared at the numbered boundary and proven headlessly before a GUI consumer depends on it.

## Frozen phase order

```text
106 Sketch workspace, interaction state, command HUD, usability contract — implemented
107 plane mapping, hit testing, box selection, grid, snapping, inference preview — implemented
108 shared planar point/entity topology, mutation commands, JSON migration, undo — implemented
109 deterministic planar constraint solver, DOF accounting, conflicts, diagnostics — implemented
110 solver-backed mouse dragging, handles, live preview, atomic commit — implemented
111 point, line, polyline, rectangle, polygon, construction-geometry creation — next — next — next — next
112 circle, arc, ellipse, slot creation/editing
113 spline editing, continuity handles, Sketch text
114 manual and automatic geometric constraints with glyph interaction
115 driving/reference dimensions, in-canvas editing, parameter/expression binding
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

Planar Sketch is a real contextual `GuiWorkspace::Sketch`. `GuiSketchWorkspace` projects its
Sketch-specific stages onto the existing generic `GuiTaskState` authority:

```text
Idle
Hover
CollectingPicks
NumericInput
Preview
SelectedHandle
DragCandidate
```

`Esc` backs out one stage. Enter/Finish Sketch capture and restore workspace, semantic selection,
viewport filter, and complete camera bookmark. The Sketch surface exposes `Create`, `Constrain`,
`Dimension`, `Modify`, and `Project` plus `Entities`, `Constraints`, `Dimensions`, and `Diagnostics`.
The numeric HUD validates a disposable line candidate before Preview and commits through the existing
atomic Part transaction path.

Canonical contract: `docs/gui-interactive-sketch-workspace-mvp8.md`.

Focused tags: `[gui][sketch-workspace]`, `[gui][sketch-command-lifecycle]`.

## Block 107 — Plane interaction, hit testing, snapping, and inference — Implemented

`GuiSketchPlaneMapping` is the device-independent Screen-DIP -> view-ray -> active-plane -> model-space
mapping and reverse projection. `GuiSketchInteractionSceneBuilder` derives transient curves,
landmarks, annotations, and intersections.

The frozen hit priority is:

```text
Point -> Curve -> Dimension -> Glyph
```

Repeated click cycles deterministic overlapping hits. Empty-space drag uses left-to-right Window and
right-to-left Crossing semantics. Grid, grid snap, origin/axis/endpoint/midpoint/center/quadrant/
intersection/nearest snaps, and horizontal/vertical/X/Y alignment inference are implemented.

The current scene builder projects historical Sketch compatibility intent. It never promotes a screen
hit, sampled endpoint, or equal coordinate to persistent `SketchPointId` identity.

Canonical contract: `docs/gui-sketch-plane-interaction-mvp8.md`.

Focused tags: `[gui][sketch-hit-test]`, `[gui][sketch-snap]`,
`[gui][sketch-box-selection]`.

## Block 108 — Shared point topology and editable Core commands — Implemented

Block 108 adds `SketchPointId`, `SketchTopologyPoint`, `SketchTopologyEntity`, and canonical
`SketchTopology`. Existing Line/Arc/Spline and profile intent is projected into stable topology
identities for solver/direct-manipulation consumers.

Global point/entity collections are lexicographic id order. Ordered entity point roles and ordered
profile curve dependencies remain semantic order and are never sorted.

Historical embedded `Point2` usages do not contain point identity. `SketchTopology::migrate_legacy(...)`
derives point sharing only from explicit ordered connectivity already present in `ClosedProfile`,
`ArcClosedProfile`, and each CompositeClosedProfile contour. A current curve end and next curve start
are paired, including loop closure. Connected usage groups choose the lexicographically smallest
proposed usage id as canonical `SketchPointId` and publish every collapse through
`SketchTopologyMigrationReport`.

Two unrelated point usages at the same coordinate remain distinct point ids. This is required for
explicit `Coincident` constraints between logically separate solver variables.

`SketchEditCommandExecutor` implements dependency-safe Add/Move/Replace/Remove. Reference topology is
read-only. Every successful command publishes exact complete `before`/`after` topology snapshots and
`SketchTopologyUndoStack` uses them for exact undo/redo.

Canonical persistence is `blcad.sketch_topology.mvp8`, version 1. Historical
`blcad.part_document.mvp1` remains load-compatible through explicit migration and a lossless checked
materialization bridge.

Canonical contract: `docs/sketch-shared-topology-mvp8.md`.

Focused tags: `[core][sketch-topology]`, `[core][sketch-edit-command]`,
`[core][sketch-json-migration]`.

## Block 109 — General planar constraint solver — Implemented

Block 109 adds `SketchConstraintSolver`, a deterministic headless nonlinear solver over Block-108
`SketchTopology`.

### Canonical solve request

`SketchConstraintSystem` stores `SketchSolverConstraint` records and sorts them lexicographically by
stable constraint id. A target is explicitly a persistent topology point or entity. Direct solver
values use millimeters for linear values and degrees for angular values.

The headless request supports all initial families:

```text
coincident
fixed
horizontal
vertical
parallel
perpendicular
collinear
equal
tangent
concentric
midpoint
symmetric
point-on-object
horizontal distance
vertical distance
aligned distance
radial
diameter
angular
```

Tangent requires a shared persistent topology endpoint. Point-on-object supports Line/Arc; equal
supports Line/Arc measures; concentric supports Arc/CircleProfile/CircularHolePattern centers;
radial/diameter target Arc; angular targets two lines. Invalid type/reference combinations report
`invalid_reference`; no geometry or endpoint is guessed from coordinate equality.

### Variable, residual, and numeric ordering

Every non-reference point contributes canonical variables in lexicographic `SketchPointId` order and
family-local `X -> Y` order. Reference points remain constant.

The characteristic length is:

```text
max(1.0 mm, planar topology point bounding-box diagonal)
```

Length residuals are normalized by that scale. Constraint residual blocks follow canonical constraint
id order and documented family-local row order. The Jacobian uses central differences with default
coordinate step `1e-7 * characteristic_length`.

The numeric engine solves damped Gauss-Newton normal equations with deterministic damping and
power-of-two line-search sequences. Frozen defaults are:

```text
convergence RMS          1e-9
rank abs tolerance       1e-10
rank relative tolerance  1e-8
initial damping          1e-6
maximum iterations       80
maximum damping attempts 8
maximum line-search steps 12
```

### DOF, redundancy, conflict, and status

After convergence, deterministic Gaussian elimination computes final Jacobian rank. Remaining planar
DOF is `variable_count - rank`.

Redundancy is attributed in canonical constraint order: append one complete constraint Jacobian block;
if a non-empty block does not increase cumulative rank, that constraint id is redundant.

A stalled inconsistent solve uses deterministic remove-one conflict attribution. Each constraint is
removed in canonical id order and the reduced system is re-solved from the original variable snapshot.
The omitted id is attributed when the reduced system converges. This is stable single-removal
attribution, not a minimum-unsatisfiable-subset claim.

Result states are exactly:

```text
fully_constrained
under_constrained
redundant
conflicting
non_convergent
invalid_reference
```

Status precedence is invalid reference, deterministic iteration limit, stalled conflict/non-convergence,
then converged redundancy, zero-DOF fully constrained, or under constrained with exact remaining DOF.

`SketchSolveResult` contains a solved disposable topology, canonical variable order, rank, remaining
DOF, redundant/conflicting ids, diagnostics, iteration count, and normalized residual summary. It is
derived and never persisted as an opaque solve cache. A failed/diagnostic result never mutates source
or persistent Sketch intent.

### Persisted Sketch adapter

`SketchConstraintSystemBuilder::from_legacy(...)` projects currently persisted Sketch intent onto the
same solver boundary. Existing Fixed/Horizontal/Vertical/Parallel/Perpendicular/EqualLength,
TangentContinuity, and current parameter-backed distance dimensions are mapped to canonical solver
families. Signed horizontal/vertical dimension direction is preserved from current topology and length
parameter values are read in millimeters.

Historical projected-reference constraints are translated semantically, but Block-108 projected
point/line entities intentionally contain no invented resolved coordinates. Those solves therefore
report `invalid_reference` until the later associative-reference owner supplies coordinate-capable
solver targets.

Block 109 does not add persistence/UI authoring records for every direct solver family. Blocks 114 and
115 own user-facing constraint/dimension authoring; they consume this one headless mathematical
authority.

Canonical contract: `docs/sketch-planar-constraint-solver-mvp8.md`.

Focused tags: `[core][sketch-solver]`, `[core][sketch-dof]`,
`[core][sketch-conflict-diagnostics]`.

## Block 110 — Solver-backed mouse dragging — Implemented

`GuiSketchDragController` builds lexicographically ordered semantic Endpoint, Midpoint, Center, Radius,
Arc, Spline-control, and current Dimension-target handles from Block-108 topology. Shared junctions are
deduplicated by `SketchPointId`; handle screen positions are transient overlay state.

Point, line-midpoint, Arc-center, and Arc-radius drag targets translate to transient Block-109
Coincident, Midpoint, Concentric, and Radial constraints. Temporary pointer/center ids and
`zz.gui.drag.target` are stripped from solve output before preview. The source-only solved topology must
materialize and re-migrate exactly before it can be shown or committed.

Move samples coalesce into one latest pending pointer and one zero-delay solve callback. Release calls
`flush(...)` synchronously with the exact final snapped pointer before commit. Preview updates the
interaction scene, handles, remaining DOF, and solve status without `PartDocument` mutation.

Conflicting/non-convergent/invalid-reference candidates, reference handles, or incompatible fully
constrained geometry are refused without weakening constraints. `Esc`, lost mouse capture, and window
deactivation restore the pre-drag scene and create no history entry.

Successful release revalidates source topology and constraint-system freshness, then commits one
`Drag sketch handle` document transaction through the existing session recompute/undo authority.

Canonical contract: `docs/gui-sketch-solver-drag-mvp8.md`.

Focused tags: `[gui][sketch-drag]`, `[integration][sketch-live-solve]`.

## Block 111 — Basic creation tools — Current next technical step

Implement point, two-point line, continuous polyline, center/corner rectangle, three-point rectangle,
parallelogram, regular polygon, centerline, and construction geometry. Multi-click commands reuse
Block-107 snap/inference and Block-109 solve authority. Composite tools expand into ordinary points,
lines, and constraints rather than GUI-only primitives.

Focused tags: `[gui][sketch-create-basic]`, `[integration][sketch-basic-profile]`.

## Block 112 — Circles, arcs, ellipses, and slots

Add persistent Core/Geometry intent for supported circle, arc, ellipse/elliptical-arc, and slot
construction families. Full circles become real curve entities where required. Degenerate radii and
ambiguous collinear picks fail closed.

Focused tags: `[core][sketch-conics]`, `[geometry][sketch-conics]`,
`[gui][sketch-create-conics]`.

## Block 113 — Spline editing and Sketch text

Add fit/control-point spline editing, insertion/removal, control polygon, continuity handles,
supported deterministic representation conversion, and parameter/expression-backed Sketch text with
explicit font fallback diagnostics.

Focused tags: `[core][sketch-spline-edit]`, `[geometry][sketch-spline-edit]`,
`[gui][sketch-spline]`, `[core][sketch-text]`, `[geometry][sketch-text]`.

## Block 114 — Constraint authoring and inference UX

Expose every Block-109 constraint through selection-driven commands and glyphs. Creation tools preview
automatic constraint candidates. Conflicting additions preview the stable conflict set and do not
mutate persistent intent.

Focused tags: `[gui][sketch-constraints]`, `[integration][sketch-auto-constraint]`.

## Block 115 — Dimensions and expressions

Add horizontal, vertical, aligned, point-to-point, length, radius, diameter, angle, and arc-length
dimensions plus driven/reference mode, in-canvas editing, and parameter/expression binding.

Focused tags: `[core][sketch-dimensions]`, `[gui][sketch-dimensions]`,
`[integration][sketch-expression-edit]`.

## Block 116 — Trim, extend, split, fillet, and chamfer

Implement trim, extend, split, two-entity Sketch fillet, and Sketch chamfer over candidate topology.
Ambiguous intersections or reference geometry never partially mutate the Sketch.

Focused tags: `[core][sketch-modify]`, `[geometry][sketch-modify]`,
`[gui][sketch-trim-extend]`.

## Block 117 — Offset, projection, and construction references

Add chain/loop offset, supported associative projection, explicit break-link conversion, and matching
lost/ambiguous reference workflow. Projected geometry remains read-only until explicitly converted.

Focused tags: `[core][sketch-offset-project]`, `[geometry][sketch-offset-project]`,
`[gui][sketch-project]`.

## Block 118 — Transform, mirror, and Sketch patterns

Add move, rotate, scale, copy, mirror, rectangular pattern, and circular pattern for selected Sketch
geometry. Associative pattern intent is explicit; exploded copies are ordinary entities.

Focused tags: `[core][sketch-transform-pattern]`, `[gui][sketch-transform-pattern]`.

## Block 119 — Regions, profiles, diagnostics, and Finish Sketch

Continuously recognize bounded regions from solved visible non-construction geometry, expose profile
selection and open/self-crossing diagnostics, and complete solver-aware Finish Sketch workflow. The
profile handoff is consumed by Interactive Modeling Blocks 122/124.

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
keyboard Apply/Cancel, high-DPI mapping, and no stale preview publication. Measure hover, drag, solve,
and region-recognition performance on representative Sketch sizes.

Focused tags: `[integration][interactive-sketcher]`, `[integration][sketch-gui-headless]`,
`[performance][sketch-interaction]`.

After Block 121, Interactive Part & Assembly Modeling MVP-9 begins at Block 122. STEP Import MVP-10
follows in Blocks 132–138.

## Sketch-domain coverage through Block 94

| Sketch capability | Canonical contracts | Interactive owner |
|---|---|---:|
| Datum/derived workplanes | workplane and bounded-workplane contracts | 106, 107 |
| Planar lines and closed line profiles | `sketch-mvp1-data-model.md`, `general-closed-sketch-profile-mvp.md` | 108, 111 |
| Shared planar point/entity identity | `sketch-shared-topology-mvp8.md` | 108–121 |
| General planar constraint solve and DOF | `sketch-planar-constraint-solver-mvp8.md` | 109–121 |
| Construction geometry | `construction-geometry-mvp.md` | 111 |
| Circles and circle profiles | `sketch-mvp1-data-model.md` | 112 |
| Arcs and arc/spline profiles | `arc-and-trim-extend-sketch-profile-mvp.md` | 112 |
| Splines and tangent continuity | `spline-and-tangent-continuity-mvp.md` | 113 |
| Constraint authoring/glyph UX | constraint and solver contracts | 114 |
| Dimensions and parameter expressions | constraint/dimension and parameter contracts | 115 |
| Trim/extend | `arc-and-trim-extend-sketch-profile-mvp.md` | 116 |
| Projected/reference geometry and recovery | projection/reference/recovery contracts | 117 |
| Circular hole pattern | `bolt-circle-pattern-mvp3.md` | 118 and MVP-9 Block 124 |
| Composite/automatic regions | profile-region contracts | 119 |
| Repair transactions and undo | Sketch repair contracts | 119 |
| Sketch3D points and curves | Part Sketch3D MVP-6 contracts | 120 |

This matrix prevents later blocks from silently inventing currently absent Sketch capability.

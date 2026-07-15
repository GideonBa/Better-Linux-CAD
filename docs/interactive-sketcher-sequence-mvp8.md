# Interactive Sketcher Sequence MVP-8

Status: in progress. Blocks 106–108 are implemented; Block 109 is the current next technical step.
Blocks 106–121 precede Interactive Modeling MVP-9 (Blocks 122–131,
`docs/interactive-modeling-sequence-mvp9.md`) and STEP Import MVP-10 (Blocks 132–138).

This phase turns the Block-99 validation surface into a productive, Inventor-familiar Sketch
workbench. The target is direct manipulation, visible constraints, in-canvas dimensions, predictable
commands, and immediate feedback without copying branded assets or product-specific UI layouts.

## Product outcome

A user can enter a planar Sketch, create common mechanical profiles without authoring JSON, drag
unconstrained points, watch constrained geometry solve continuously, add/edit dimensions in the
canvas, repair conflicts, and leave the Sketch with one undoable document result. Persistent intent
remains available to headless Core/Geometry consumers.

The phase also gives implemented Sketch3D points and curves direct manipulation. Unsupported tools
remain visibly unavailable; the sequence does not claim commercial-CAD parity before acceptance.

## Core reason for the phase order

The historical planar model embeds `Point2` coordinates directly in line, arc, spline, and profile
records. Equal coordinates are not shared identity and the historical diagnostic layer is not a
general constraint solver. A GUI-only drag implementation would therefore tear connected profiles
apart or create results that headless recompute cannot reproduce.

Blocks 108–110 deliberately establish:

```text
shared point/entity topology
  -> deterministic general solver
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
2. Shared Sketch points, entities, constraints, dimensions, and construction/reference state are
   Core intent with stable ids and canonical persistence.
3. Numeric coordinate equality is not topology connectivity. Distinct point ids may be coincident.
4. The solver is a deterministic headless Core service; widgets do not own constraint mathematics.
5. Dragging solves a disposable candidate. Release commits one document transaction; `Esc`, lost
   capture, or failed solve restores the pre-drag snapshot.
6. Fixed or fully constrained geometry refuses incompatible drag instead of deleting constraints.
7. Automatic constraints are transient inference candidates until accepted.
8. Projected geometry remains associative/read-only until explicit conversion.
9. Every creation/edit workflow has keyboard-accessible Apply/Cancel and a headless command path.
10. Solver failure, non-convergence, ambiguity, or over-constraint publishes diagnostics and no
    partial persistent mutation.
11. Historical PartDocument Sketch JSON migrates deterministically through an explicit Core boundary;
    save/load never depends on GUI session state.

## Canonical authority rule

Every block below names the existing authorities it may consume. A capability not present in those
contracts or explicitly introduced by the block does not exist for that block. New persistent intent
must be declared at the numbered boundary and proven headlessly before a GUI consumer depends on it.

## Frozen phase order

```text
106 Sketch workspace, interaction state, command HUD, usability contract — implemented
107 plane mapping, hit testing, box selection, grid, snapping, inference preview — implemented
108 shared planar point/entity topology, mutation commands, JSON migration, undo — implemented
109 deterministic planar constraint solver, DOF accounting, conflicts, diagnostics — next
110 solver-backed mouse dragging, handles, live preview, atomic commit
111 point, line, polyline, rectangle, polygon, construction-geometry creation
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

Existing authority: `docs/gui-feature-validation-sequence-mvp7.md`,
`docs/gui-sketch-workbench-mvp7.md`, `docs/workplane-resolver-mvp2.md`,
`docs/sketch-mvp1-data-model.md`.

Focused tags: `[gui][sketch-workspace]`, `[gui][sketch-command-lifecycle]`.

## Block 107 — Plane interaction, hit testing, snapping, and inference — Implemented

`GuiSketchPlaneMapping` is the device-independent Screen-DIP -> view-ray -> active-plane ->
model-space mapping and reverse projection. The native OCCT bridge converts Qt DIP to physical view
pixels explicitly; the deterministic offscreen provider uses an explicit plane center and
model-units-per-DIP scale.

`GuiSketchInteractionSceneBuilder` derives transient curves, landmarks, annotations, and intersections.
The frozen hit priority is:

```text
Point -> Curve -> Dimension -> Glyph
```

Repeated click cycles deterministic overlapping hits. Empty-space drag uses left-to-right Window and
right-to-left Crossing semantics. Grid, grid snap, origin/axis/endpoint/midpoint/center/quadrant/
intersection/nearest snaps, and horizontal/vertical/X/Y alignment inference are implemented.

The current scene builder projects historical `Sketch` compatibility intent. It never promotes a
screen hit, sampled endpoint, or equal coordinate to persistent `SketchPointId` identity.

Canonical contract: `docs/gui-sketch-plane-interaction-mvp8.md`.

Focused tags: `[gui][sketch-hit-test]`, `[gui][sketch-snap]`,
`[gui][sketch-box-selection]`.

## Block 108 — Shared point topology and editable Core commands — Implemented

Block 108 adds `SketchPointId`, `SketchTopologyPoint`, `SketchTopologyEntity`, and canonical
`SketchTopology`. Existing Line/Arc/Spline and profile intent is projected into stable topology
identities for future solver/direct-manipulation consumers.

Global point/entity collections are lexicographic id order. Ordered entity point roles and ordered
profile curve dependencies remain semantic order and are never sorted.

### Explicit connectivity-derived legacy migration

Historical embedded `Point2` usages do not contain point identity. Migration therefore does not
pretend that coordinate equality proves connectivity.

`SketchTopology::migrate_legacy(...)` derives point sharing only from explicit ordered connectivity
already present in:

```text
ClosedProfile
ArcClosedProfile
CompositeClosedProfile outer contour
each CompositeClosedProfile inner contour
```

For a supported editable curve in one ordered contour, the current curve's end usage and next curve's
start usage are paired, including last-to-first closure. Their historical coordinates must agree
within `1e-9` and flags must be compatible. Connected usage groups choose the lexicographically
smallest proposed usage id as canonical `SketchPointId`.

`SketchTopologyMigrationReport` records each endpoint usage collapsed into the canonical shared id.
A connected triangle therefore migrates from six historical endpoint usages to three shared points
and three explicit identity-change records independent of line insertion order.

Two unrelated point usages at the same coordinate remain distinct point ids. This is required for
Block 109 to represent explicit `Coincident` constraints between logically separate solver variables.

Explicit topology flags are:

```text
construction
reference
```

Reference and construction cannot both be true. Projected point/line and reference-generated line
intent migrates as read-only reference entities.

### Editable topology commands

`SketchEditCommandExecutor` implements:

```text
AddPoint / AddEntity
MovePoint
ReplaceEntity
RemovePoint / RemoveEntity
```

Commands execute on disposable collections and republish only through complete topology validation.
Deletion is dependency-safe and reference intent is read-only. Add and Move allow distinct point ids
to occupy the same numeric coordinate; no implicit id merge occurs.

Moving one shared point changes one point record and every connected entity continues to reference the
same id.

Every successful command produces a `SketchEditTransaction` containing exact canonical `before` and
`after` snapshots. `SketchTopologyUndoStack` uses those full snapshots for exact undo/redo and clears
redo after a new command.

### Topology persistence and compatibility migration

Canonical topology persistence is:

```text
schema  = blcad.sketch_topology.mvp8
version = 1
```

It persists point ids/coordinates/flags, entity ids/kinds/ordered point references/ordered entity
dependencies/flags, and canonical dependency records. Equal-coordinate distinct point ids round-trip
exactly.

Historical `blcad.part_document.mvp1` remains load-compatible.
`migrate_legacy_part_document_sketch_json(...)` loads the old PartDocument, selects one Sketch,
migrates it, and returns topology plus migration report. No GUI state participates.

`SketchTopologyPartDocumentEditor` supports lossless compatibility application for representable
edits. It migrates, applies one topology command, materializes a historical Sketch candidate,
re-migrates, requires exact topology equality, and only then invokes atomic
`PartDocument::update_sketch(...)`. Lossy point identity, flags, explicit orphan records, or ordered
connectivity cause fail-closed rejection.

Canonical contract: `docs/sketch-shared-topology-mvp8.md`.

Existing authority: `docs/sketch-mvp1-data-model.md`,
`docs/general-closed-sketch-profile-mvp.md`,
`docs/arc-and-trim-extend-sketch-profile-mvp.md`,
`docs/spline-and-tangent-continuity-mvp.md`, `docs/construction-geometry-mvp.md`,
`docs/semantic-references.md`, `docs/file-format.md`,
`docs/gui-sketch-plane-interaction-mvp8.md`.

Focused tags: `[core][sketch-topology]`, `[core][sketch-edit-command]`,
`[core][sketch-json-migration]`.

## Block 109 — General planar constraint solver — Current next technical step

Add a deterministic headless nonlinear solver over Block-108 planar point/entity topology. Freeze
variable ordering, scale normalization, tolerances, iteration limits, convergence reporting, and
stable conflict attribution.

Initial solve families:

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
radial / diameter
angular
```

Report exactly:

```text
fully constrained
under constrained (remaining DOF)
redundant
conflicting
non-convergent
invalid reference
```

No solver result is persisted as opaque cache. Canonical point/entity/constraint/dimension intent plus
deterministic solving remains authority.

Existing authority: `docs/sketch-shared-topology-mvp8.md`,
`docs/sketch-constraints-and-dimensions-mvp.md`, `docs/sketch-solver-diagnostics-mvp.md`.

Focused tags: `[core][sketch-solver]`, `[core][sketch-dof]`,
`[core][sketch-conflict-diagnostics]`.

## Block 110 — Solver-backed mouse dragging

Expose endpoint, midpoint, center, radius, arc, spline, and dimension handles. Pointer movement maps
to a target and asks Block 109 for a disposable candidate. Preview never mutates the document.
Release commits one transaction; `Esc`, lost capture, or failed solve restores pre-drag state. The
final pointer position is never dropped by throttling.

Existing authority: Blocks 108–109, `docs/gui-sketch-workbench-mvp7.md`, and the Block-96 GUI
transaction/undo contract.

Focused tags: `[gui][sketch-drag]`, `[integration][sketch-live-solve]`.

## Block 111 — Basic creation tools

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

Add orthogonal triad/plane locks, model-space point/line placement, typed XYZ/distance/angle input,
3D curve handles, guide roles, and projection of planar Sketch points. This does not imply a full
variational 3D constraint solver.

Focused tags: `[gui][sketch-3d-edit]`, `[integration][sketch-3d-direct-manipulation]`.

## Block 121 — Interactive Sketcher acceptance

Add deterministic tutorial documents and a coverage manifest for every planned Sketch tool. Prove
mouse/script equivalence, persistence/recompute, exact undo/redo, conflict atomicity, reference
repair, keyboard Apply/Cancel, high-DPI mapping, and no stale preview publication. Measure hover,
drag, solve, and region-recognition performance on representative Sketch sizes.

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
| Construction geometry | `construction-geometry-mvp.md` | 111 |
| Circles and circle profiles | `sketch-mvp1-data-model.md` | 112 |
| Arcs and arc/spline profiles | `arc-and-trim-extend-sketch-profile-mvp.md` | 112 |
| Splines and tangent continuity | `spline-and-tangent-continuity-mvp.md` | 113 |
| Constraints and DOF diagnostics | constraint/dimension and solver-diagnostic contracts | 109, 114, 119 |
| Dimensions and parameter expressions | constraint/dimension and parameter contracts | 115 |
| Trim/extend | `arc-and-trim-extend-sketch-profile-mvp.md` | 116 |
| Projected/reference geometry and recovery | projection/reference/recovery contracts | 117 |
| Circular hole pattern | `bolt-circle-pattern-mvp3.md` | 118 and MVP-9 Block 124 |
| Composite/automatic regions | profile-region contracts | 119 |
| Repair transactions and undo | Sketch repair contracts | 119 |
| Sketch3D points and curves | Part Sketch3D MVP-6 contracts | 120 |

This matrix prevents later blocks from silently inventing currently absent Sketch capability.

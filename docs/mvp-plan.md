---
doc: MVP Plan
role: >-
  Implementation-sequence source of truth. Feature-specific documents remain
  canonical for exact contracts, formulas, persistence details, failure
  policies, ordering, and focused proofs.
implemented_through: Block 108
current_block: 109
current_boundary: Deterministic general planar constraint solver, DOF accounting, conflicts, and diagnostics
current_tag: "[core][sketch-solver]"
phase_status:
  mvp_1: "Single-part modeling — implemented"
  mvp_2: "Semantic references and richer sketch workflows — implemented"
  mvp_3: "Parametric bolt circle pattern — implemented"
  mvp_4: "Assembly parameters and Project container — implemented"
  mvp_5: "Assembly relationships, motion, hierarchy, analysis, exchange — Blocks 1–47 implemented"
  mvp_6: "Part Construction — Blocks 48–94 implemented; MVP complete"
  mvp_7: "GUI Feature Validation — Blocks 95–105 implemented; MVP complete"
  mvp_8: "Interactive Sketcher — Blocks 106–108 implemented; Blocks 109–121 planned; Block 109 next"
  mvp_9: "Interactive Part & Assembly Modeling — Blocks 122–131 planned after Interactive Sketcher acceptance"
  mvp_10: "STEP Import — Blocks 132–138 planned after Interactive Modeling acceptance"
---

# MVP Plan

This document is the numbered implementation-sequence source of truth for BLCAD.

Exact feature semantics are intentionally not duplicated here. Before implementing one block, read
the block's canonical sequence and the feature-specific authorities named there. If current code and
a planning document disagree, verify current implementation and update the affected canonical
contract explicitly; do not invent capability inside a later block.

## Current status

```text
implemented through  Block 108
current block        Block 109
current phase        Interactive Sketcher MVP-8
current boundary     deterministic general planar constraint solver
```

Block 108 is implemented. Block 109 is the current next technical step.

## Phase map

```text
MVP-1   single-Part parametric foundation                         implemented
MVP-2   semantic references and richer Sketch workflows           implemented
MVP-3   parametric bolt-circle pattern                            implemented
MVP-4   assembly parameters and Project container                 implemented
MVP-5   Assembly system                                 Blocks 1–47 implemented
MVP-6   Part Construction                              Blocks 48–94 implemented
MVP-7   GUI Feature Validation                        Blocks 95–105 implemented
MVP-8   Interactive Sketcher                          Blocks 106–121 in progress
MVP-9   Interactive Part & Assembly Modeling          Blocks 122–131 planned
MVP-10  STEP Import                                    Blocks 132–138 planned
```

Canonical phase sequences:

- `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`
- `docs/part-construction-sequence-mvp6.md`
- `docs/gui-feature-validation-sequence-mvp7.md`
- `docs/interactive-sketcher-sequence-mvp8.md`
- `docs/interactive-modeling-sequence-mvp9.md`
- `docs/step-import-sequence-mvp10.md`

## Implemented foundation through Block 47 — Assembly MVP-5

Blocks 1–47 establish the single-Part foundation, semantic workplane/reference workflows, Project
container, local and cross-hierarchy Assembly authority, generic typed geometric targets, stable
generated-topology producer identity, deterministic compatibility selection, eight geometric
relationship families, the shared numeric solve path, freshness-gated application, rooted hierarchy
semantics, Revolute/Prismatic/Cylindrical/Planar/Spherical joints, posed geometry, contact/sweep
analysis, and flattened/structured STEP exchange.

Assembly model identity remains semantic. Raw OCCT topology, XDE labels, STEP entity ids, and memory
addresses are never persistent target identity.

## Implemented Blocks 48–94 — Part Construction MVP-6

The Part Construction sequence implements:

```text
Body identity, visibility, scoped recompute, inspection
NewBody / Join / Cut / Intersect result intent
Body Boolean, BodyTransform, SketchOwnership
reusable typed Part-feature input references
rich Extrude / ExtrudedCut extent, taper, thin, path control
Revolve / RevolveCut
Linear / Circular Pattern
MirrorFeature
Fillet / Chamfer
Shell / Draft
Sketch3D and model-space curves
PathCurve
Sweep / SweepCut / SweepSurface
spatial/twist/guide-controlled Sweep
path-following Extrude / ExtrudedCut
Loft / LoftCut / LoftSurface
multi-section and guided continuity-controlled Loft
Boundary / Fill Surface
Surface Trim / Extend
Stitch / Knit / Sew shell
closed-shell-to-solid conversion
visible Solid/Surface multi-body STEP export
integrated Core/Geometry acceptance
```

Canonical sequence: `docs/part-construction-sequence-mvp6.md`.

The phase is accepted through Block 94. Later UI phases consume these Core/Geometry authorities rather
than reimplementing feature mathematics in widgets.

## Implemented Blocks 95–105 — GUI Feature Validation MVP-7

The optional Qt 6 application layer implements:

```text
CAD application shell and workspaces
GuiDocumentSession
GuiTaskState and command enablement
open / save / save-as / dirty handling
document transactions and exact undo/redo
recompute and structured diagnostics
OCCT viewport and navigation
stable semantic viewport picking
deterministic model / assembly browser
property/task editing through Core mutation paths
bidirectional semantic selection synchronization
planar Sketch validation/edit/repair workbench
parameter, Body, Extrude/Cut, Revolve, hole workflows
Pattern, Mirror, finishing, Shell, Draft workflows
Body Boolean / Transform workflows
Sketch3D / PathCurve / Sweep / Loft / Surface workflows
Assembly authoring, solve, motion, analysis, STEP export
machine-checked feature coverage and GUI/headless equivalence
```

Canonical sequence: `docs/gui-feature-validation-sequence-mvp7.md`.

Block 105 closes the validation phase. Qt remains a client; persistent model, solver, Geometry, and
exchange authority do not move into widgets.

## Interactive Sketcher MVP-8 — Blocks 106–121

Canonical sequence: `docs/interactive-sketcher-sequence-mvp8.md`.

The product target is a productive planar and 3D Sketch environment with direct manipulation,
visible constraints, in-canvas dimensions, deterministic snapping, solver-backed dragging, complete
creation/modify workflows, profile recognition, repair, and GUI/headless equivalence.

Frozen order:

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
```

### Block 106 — Contextual Sketch workspace — Implemented

`GuiWorkspace::Sketch` and `GuiSketchWorkspace` implement the transient interaction stages:

```text
Idle
Hover
CollectingPicks
NumericInput
Preview
SelectedHandle
DragCandidate
```

Enter/Finish Sketch capture and restore workspace, semantic selection, viewport filter, and camera
state. `Esc` backs out one stage. Numeric candidates validate on disposable Sketch state and commit
through the existing atomic Part transaction authority.

Canonical contract: `docs/gui-interactive-sketch-workspace-mvp8.md`.

Focused tags: `[gui][sketch-workspace]`, `[gui][sketch-command-lifecycle]`.

### Block 107 — Plane interaction — Implemented

Block 107 implements the device-independent Screen-DIP/view-ray/workplane/model mapping, transient
interaction scene, zoom-stable `Point -> Curve -> Dimension -> Glyph` hit priority, deterministic
stacked-hit cycling, Window/Crossing selection, bounded grid display, grid snap, geometric snap
families, and horizontal/vertical/X/Y alignment inference.

No pixel, interaction sample, hover product, hit stack, grid line, or snap candidate becomes model
identity.

Canonical contract: `docs/gui-sketch-plane-interaction-mvp8.md`.

Focused tags: `[gui][sketch-hit-test]`, `[gui][sketch-snap]`,
`[gui][sketch-box-selection]`.

### Block 108 — Shared planar Sketch topology — Implemented

Block 108 introduces typed `SketchPointId` and canonical `SketchTopology` authority for future solver
and direct-manipulation consumers.

Implemented records:

```text
SketchTopologyPoint
  id
  position
  construction
  reference

SketchTopologyEntity
  id
  kind
  ordered point references
  ordered entity dependencies
  construction
  reference

SketchTopologyDependency
  consumer_id
  source_entity_id
  role
```

Supported topology families cover existing lines, arcs, splines, rectangle/circle/pattern centers,
closed/arc/composite profile relationships, projected points/lines, and reference-generated lines.
Global point/entity collections are canonical lexicographic id order. Defining point roles and
profile dependencies keep semantic order.

`SketchTopology::migrate_legacy(...)` converts the historical embedded-`Point2` Sketch model.
Same-flag point usages within `1e-9` share one canonical point id. Candidate processing is topology-id
ordered, so source insertion order does not change the result. `SketchTopologyMigrationReport`
records each legacy identity collapsed into a canonical shared point.

`SketchEditCommandExecutor` implements dependency-safe `Add`, `Move`, `Replace`, and `Remove`
payloads. Reference topology is read-only. Every successful edit produces exact complete `before`
and `after` canonical snapshots; `SketchTopologyUndoStack` uses these snapshots for exact undo/redo.

Canonical topology persistence is:

```text
schema  = blcad.sketch_topology.mvp8
version = 1
```

`migrate_legacy_part_document_sketch_json(...)` explicitly loads historical
`blcad.part_document.mvp1` JSON and migrates one selected Sketch without GUI state.

`SketchTopologyPartDocumentEditor` supports topology edits that can be projected back to the current
legacy Sketch records without any point-identity or flag loss. It materializes, re-migrates, requires
exact topology equality, and only then calls `PartDocument::update_sketch(...)`. Non-representable
states fail closed and remain persistable through the canonical topology JSON.

Canonical contract: `docs/sketch-shared-topology-mvp8.md`.

Focused tags:

```text
[core][sketch-topology]
[core][sketch-edit-command]
[core][sketch-json-migration]
```

## Current next technical step — Block 109

Block 109 owns the deterministic general planar constraint solver over Block-108 point/entity
topology.

Freeze:

```text
solver variable ordering
coordinate / angle / residual scale normalization
absolute and relative tolerances
iteration and damping limits
convergence classification
remaining-DOF accounting
stable redundant/conflict attribution
invalid-reference diagnostics
```

Initial constraint/residual families:

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

Required solve states:

```text
fully constrained
under constrained (remaining DOF)
redundant
conflicting
non-convergent
invalid reference
```

Solver results remain derived. No opaque solved coordinate cache becomes persistent model authority.

Existing authority:

- `docs/sketch-shared-topology-mvp8.md`
- `docs/sketch-constraints-and-dimensions-mvp.md`
- `docs/sketch-solver-diagnostics-mvp.md`

Focused tags:

```text
[core][sketch-solver]
[core][sketch-dof]
[core][sketch-conflict-diagnostics]
```

## Blocks 110–121 — Remaining Interactive Sketcher sequence

Block 110 adds solver-backed handle dragging and one-transaction release commits. Block 111 adds basic
point/line/polyline/rectangle/polygon/construction creation. Block 112 adds circles, arcs, ellipses,
and slots. Block 113 adds spline editing and Sketch text. Block 114 exposes constraint authoring and
automatic-constraint UX. Block 115 completes dimensions and expression binding. Block 116 adds trim,
extend, split, Sketch fillet, and Sketch chamfer. Block 117 adds offset and associative projection.
Block 118 adds transforms, copy, mirror, and Sketch patterns. Block 119 closes region/profile/repair
and Finish Sketch behavior. Block 120 adds Interactive Sketch3D. Block 121 is integrated acceptance
and measured performance.

Exact per-block boundaries and focused tags are in
`docs/interactive-sketcher-sequence-mvp8.md`.

## Interactive Part & Assembly Modeling MVP-9 — Blocks 122–131

Canonical sequence: `docs/interactive-modeling-sequence-mvp9.md`.

```text
122 modeling workspace, preselection command start, mini-toolbar, navigation aids
123 transient viewport manipulator infrastructure and numeric coupling
124 interactive Extrude, path Extrude, Revolve
125 interactive Fillet, Chamfer, Shell, Draft
126 interactive Pattern, Mirror, Body Boolean, Body Transform
127 interactive PathCurve, Sweep, Loft
128 interactive Surface authoring and surface-to-solid
129 feature edit lifecycle and Core feature-update boundary
130 interactive Assembly placement, relationships, joints, motion
131 measure tool, coverage manifest v2, integrated acceptance
```

## STEP Import MVP-10 — Blocks 132–138

Canonical sequence: `docs/step-import-sequence-mvp10.md`.

```text
132 STEP source identity, import modes, JSON, freshness
133 OCCT STEP/XDE reader and deterministic imported body definitions
134 stable imported topology identity, recovery, target resolution
135 Reference Part integration with assemblies
136 EditableBody ImportedBodyFeature and downstream modeling
137 structured STEP assembly import
138 integrated import, refresh, edit, assembly, re-export acceptance
```

The explicit modes remain `Reference` and `EditableBody`; imported foreign history is never
heuristically reconstructed.

## Sequencing rules

1. Implement exactly the current block before advancing the status header.
2. Read the current block's Existing authority set before coding.
3. Do not merge adjacent blocks because they touch one subsystem.
4. New persistent intent requires explicit Core contract, persistence policy, and focused headless
   proof before GUI consumers depend on it.
5. Transient GUI state never becomes model identity to avoid a Core boundary.
6. Failed validation, solve, recompute, import, or Geometry execution publishes no partial persistent
   mutation.
7. Update this file, current phase sequence, feature-specific contract, relevant architecture/UI/setup
   summaries, file-format authority where persistence changes, and README status after a block.
8. Advance `implemented_through`, `current_block`, `current_boundary`, and `current_tag` only after the
   implementation and proof are checked in.

## Current handoff

Block 108 is implemented. Block 109 is next.

Read `docs/sketch-shared-topology-mvp8.md`, `docs/sketch-constraints-and-dimensions-mvp.md`, and
`docs/sketch-solver-diagnostics-mvp.md`, then implement the deterministic general planar constraint
solver before beginning mouse dragging in Block 110.

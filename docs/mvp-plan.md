---
doc: MVP Plan
role: >-
  Implementation-sequence source of truth. Feature-specific documents remain
  canonical for exact contracts, formulas, persistence details, failure
  policies, ordering, and focused proofs.
implemented_through: Block 109
current_block: 110
current_boundary: Solver-backed Sketch mouse dragging, semantic handles, live preview, and atomic release commit
current_tag: "[gui][sketch-drag]"
phase_status:
  mvp_1: "Single-part modeling — implemented"
  mvp_2: "Semantic references and richer sketch workflows — implemented"
  mvp_3: "Parametric bolt-circle pattern — implemented"
  mvp_4: "Assembly parameters and Project container — implemented"
  mvp_5: "Assembly relationships, motion, hierarchy, analysis, exchange — Blocks 1–47 implemented"
  mvp_6: "Part Construction — Blocks 48–94 implemented; MVP complete"
  mvp_7: "GUI Feature Validation — Blocks 95–105 implemented; MVP complete"
  mvp_8: "Interactive Sketcher — Blocks 106–109 implemented; Blocks 110–121 planned; Block 110 next"
  mvp_9: "Interactive Part & Assembly Modeling — Blocks 122–131 planned after Interactive Sketcher acceptance"
  mvp_10: "STEP Import — Blocks 132–138 planned after Interactive Modeling acceptance"
---

# MVP Plan

This document is the numbered implementation-sequence source of truth for BLCAD. Exact feature
semantics are intentionally not duplicated here; feature-specific contracts remain authoritative for
mathematics, persistence spellings, migration rules, and failure policy.

## Current status

```text
implemented through  Block 109
current block        Block 110
current phase        Interactive Sketcher MVP-8
current boundary     solver-backed Sketch mouse dragging
```

Block 109 is implemented. Block 110 is the current next technical step.

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
container, local/cross-hierarchy Assembly authority, generic typed geometric targets, stable generated
topology identity, deterministic compatibility selection, geometric relationship families, shared
numeric solving, freshness-gated application, rooted hierarchy, five joint families, posed geometry,
analysis, and flattened/structured STEP exchange.

Assembly identity remains semantic. Raw OCCT topology, XDE labels, STEP entity ids, and memory
addresses are never persistent target identity.

## Implemented Blocks 48–94 — Part Construction MVP-6

Part Construction implements stable Body identity, Body-scoped recompute/inspection, NewBody/Join/
Cut/Intersect result intent, Body Boolean/transform workflows, typed feature inputs, richer Extrude/Cut,
Revolve, Patterns, Mirror, Fillet, Chamfer, Shell, Draft, Sketch3D, PathCurve, Sweep, path Extrude,
Loft, Surface features, closed-shell-to-solid conversion, multi-body STEP exchange, and integrated
Core/Geometry acceptance.

Canonical sequence: `docs/part-construction-sequence-mvp6.md`.

## Implemented Blocks 95–105 — GUI Feature Validation MVP-7

The optional Qt 6 layer implements the CAD shell, `GuiDocumentSession`, generic task state, document
lifecycle, atomic candidate transactions, exact undo/redo, recompute/diagnostics, OCCT viewport,
stable semantic picking, deterministic browser/property projection, synchronized selection, and
Sketch/Part/Surface/Assembly/analysis/exchange validation workbenches.

Qt remains a client; persistent model, solver, Geometry, and exchange authority do not move into
widgets.

Canonical sequence: `docs/gui-feature-validation-sequence-mvp7.md`.

## Interactive Sketcher MVP-8 — Blocks 106–121

Canonical sequence: `docs/interactive-sketcher-sequence-mvp8.md`.

Frozen order:

```text
106 Sketch workspace, interaction state, command HUD, usability contract — implemented
107 plane mapping, hit testing, box selection, grid, snapping, inference preview — implemented
108 shared planar point/entity topology, mutation commands, JSON migration, undo — implemented
109 deterministic planar constraint solver, DOF accounting, conflicts, diagnostics — implemented
110 solver-backed mouse dragging, handles, live preview, atomic commit — next
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

`GuiWorkspace::Sketch` and `GuiSketchWorkspace` implement Sketch staging over generic task/transaction
authority. Enter/Finish Sketch capture and restore workspace, selection, filters, and camera state.
Numeric HUD candidates validate on disposable Sketch state before atomic commit.

Canonical contract: `docs/gui-interactive-sketch-workspace-mvp8.md`.

### Block 107 — Plane interaction — Implemented

Block 107 implements Screen-DIP/view-ray/workplane/model mapping, transient interaction scene
projection, zoom-stable `Point -> Curve -> Dimension -> Glyph` hit priority, stacked-hit cycling,
Window/Crossing selection, bounded grid display, geometric snaps, and horizontal/vertical/X/Y
inference.

No screen or interaction product becomes model identity.

Canonical contract: `docs/gui-sketch-plane-interaction-mvp8.md`.

### Block 108 — Shared planar Sketch topology — Implemented

Block 108 introduces typed `SketchPointId` and canonical `SketchTopology`. Global point/entity
collections are lexicographic id order; defining point roles and profile dependencies preserve semantic
order. Historical embedded `Point2` intent migrates explicitly. Shared point identity is derived only
from existing ordered closed-profile connectivity, never from global coordinate equality.

`SketchEditCommandExecutor` implements dependency-safe Add/Move/Replace/Remove with exact complete
before/after topology snapshots. `SketchTopologyUndoStack` uses those snapshots for exact undo/redo.
Canonical persistence is `blcad.sketch_topology.mvp8`, version 1. Historical PartDocument Sketch JSON
remains compatible through explicit migration and a lossless materialization bridge.

Canonical contract: `docs/sketch-shared-topology-mvp8.md`.

Focused tags:

```text
[core][sketch-topology]
[core][sketch-edit-command]
[core][sketch-json-migration]
```

### Block 109 — Deterministic planar constraint solver — Implemented

Block 109 adds `SketchConstraintSolver` over Block-108 topology. `SketchConstraintSystem` canonicalizes
constraint order by stable id. Every non-reference point contributes solver variables in canonical
`SketchPointId` order and `X -> Y` order; reference points remain constant.

The direct headless solver supports:

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

Linear values are millimeters and angular values are degrees. Length residuals use deterministic
characteristic scale `max(1 mm, topology point bounding-box diagonal)`. The Jacobian uses central
differences; the numeric engine uses deterministic damped Gauss-Newton normal equations and fixed
iteration/damping/line-search limits.

After convergence, deterministic Jacobian rank gives exact local remaining DOF:

```text
remaining_dof = variable_count - jacobian_rank
```

Canonical incremental rank contribution attributes redundant constraints. A stalled inconsistent
system uses canonical remove-one/re-solve attribution for stable conflict ids. This is deterministic
single-removal attribution rather than a globally minimal unsatisfiable subset claim.

Solve states are exactly:

```text
fully_constrained
under_constrained
redundant
conflicting
non_convergent
invalid_reference
```

`SketchSolveResult` is derived and contains solved disposable topology, variable order, Jacobian rank,
remaining DOF, redundant/conflicting ids, stable diagnostics, iteration count, and normalized residual
summary. Solver caches/results are not persistent model authority and solving never mutates the source
topology.

`SketchConstraintSystemBuilder::from_legacy(...)` maps the currently persisted geometric constraints,
TangentContinuity, and parameter-backed distance dimensions to the same solver boundary. The direct
solver already supports the complete Block-109 family set; Blocks 114/115 own persistence and GUI
authoring for families not represented by current historical Sketch records.

Canonical contract: `docs/sketch-planar-constraint-solver-mvp8.md`.

Focused tags:

```text
[core][sketch-solver]
[core][sketch-dof]
[core][sketch-conflict-diagnostics]
```

## Current next technical step — Block 110

Block 110 owns solver-backed direct manipulation over Blocks 106–109.

Required boundary:

```text
semantic endpoint / midpoint / center / radius / arc / spline / dimension handles
screen pointer
  -> Block-107 plane mapping and snap/inference
  -> semantic SketchPointId/entity drag target
  -> disposable Block-108 topology candidate
  -> transient drag target equation
  -> Block-109 solve
  -> live viewport preview
  -> release: one validated document transaction
```

Freeze:

- handle identity and hit priority relative to normal Sketch hits;
- which topology points/entities each handle controls;
- temporary drag target semantics;
- solver throttling/coalescing without dropping the final pointer position;
- fixed/fully-constrained refusal behavior;
- `Esc` and lost-capture rollback;
- preview publication without PartDocument mutation;
- one undo entry on successful release;
- exact pre-drag snapshot restoration on failed solve or cancellation.

Block 110 does not implement the broad creation surface from Block 111.

Existing authority:

- `docs/gui-interactive-sketch-workspace-mvp8.md`
- `docs/gui-sketch-plane-interaction-mvp8.md`
- `docs/sketch-shared-topology-mvp8.md`
- `docs/sketch-planar-constraint-solver-mvp8.md`
- Block-96 GUI transaction/undo contract

Focused tags:

```text
[gui][sketch-drag]
[integration][sketch-live-solve]
```

## Remaining Interactive Sketcher sequence

Block 111 adds basic point/line/polyline/rectangle/polygon/construction creation. Block 112 adds
circles, arcs, ellipses, and slots. Block 113 adds spline editing and Sketch text. Block 114 exposes
constraint authoring and automatic-constraint UX. Block 115 completes dimensions and expression
binding. Block 116 adds trim/extend/split/Sketch fillet/chamfer. Block 117 adds offset and associative
projection. Block 118 adds transforms/copy/mirror/Sketch patterns. Block 119 closes region/profile/
repair and Finish Sketch behavior. Block 120 adds Interactive Sketch3D. Block 121 is integrated
acceptance and measured performance.

Exact per-block boundaries and focused tags are in `docs/interactive-sketcher-sequence-mvp8.md`.

## Later phases

Interactive Part & Assembly Modeling MVP-9 is Blocks 122–131 and is canonical in
`docs/interactive-modeling-sequence-mvp9.md`.

STEP Import MVP-10 is Blocks 132–138 and is canonical in `docs/step-import-sequence-mvp10.md`.

## Sequencing rules

1. Implement exactly the current block before advancing the status header.
2. Read the current block's Existing authority set before coding.
3. Do not merge adjacent blocks because they touch one subsystem.
4. New persistent intent requires explicit Core contract, persistence policy, and focused headless proof
   before GUI consumers depend on it.
5. Transient GUI state never becomes model identity to avoid a Core boundary.
6. Failed validation, solve, recompute, import, or Geometry execution publishes no partial persistent
   mutation.
7. Update this file, current phase sequence, feature-specific contract, relevant architecture/UI/setup
   summaries, and README status after a block.
8. Advance `implemented_through`, `current_block`, `current_boundary`, and `current_tag` only after the
   implementation and proof are checked in.

## Current handoff

Block 109 is implemented. Block 110 is next.

Read the Block-106/107 GUI interaction contracts, `docs/sketch-shared-topology-mvp8.md`, and
`docs/sketch-planar-constraint-solver-mvp8.md`, then implement solver-backed semantic-handle dragging
before beginning creation tools in Block 111.

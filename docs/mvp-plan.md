---
doc: MVP Plan
role: >-
  Implementation-sequence source of truth. Feature-specific documents remain
  canonical for exact contracts, formulas, persistence details, failure
  policies, ordering, and focused proofs.
implemented_through: Block 113
current_block: 114
current_boundary: Manual and automatic geometric constraints with glyph interaction
current_tag: "[gui][sketch-constraints]"
phase_status:
  mvp_1: "Single-part modeling — implemented"
  mvp_2: "Semantic references and richer sketch workflows — implemented"
  mvp_3: "Parametric bolt-circle pattern — implemented"
  mvp_4: "Assembly parameters and Project container — implemented"
  mvp_5: "Assembly relationships, motion, hierarchy, analysis, exchange — Blocks 1–47 implemented"
  mvp_6: "Part Construction — Blocks 48–94 implemented; MVP complete"
  mvp_7: "GUI Feature Validation — Blocks 95–105 implemented; MVP complete"
  mvp_8: "Interactive Sketcher — Blocks 106–113 implemented; Blocks 114–121 planned; Block 114 next"
  mvp_9: "Interactive Part & Assembly Modeling — Blocks 122–131 planned after Interactive Sketcher acceptance"
  mvp_10: "STEP Import — Blocks 132–138 planned after Interactive Modeling acceptance"
---

# MVP Plan

This document is the numbered implementation-sequence source of truth for BLCAD. Exact feature
semantics are intentionally not duplicated here; feature-specific contracts remain authoritative for
mathematics, persistence spellings, migration rules, ordering, and failure policy.

## Current status

```text
implemented through  Block 113
current block        Block 114
current phase        Interactive Sketcher MVP-8
current boundary     manual/automatic geometric constraints and glyph interaction
```

Block 113 is implemented. Block 114 is the current next technical step.

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
container, local/cross-hierarchy Assembly authority, typed geometric targets, stable generated
topology identity, deterministic compatibility selection, relationship families, shared numeric
solving, rooted hierarchy, five joint families, posed geometry, analysis, and flattened/structured
STEP exchange.

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
110 solver-backed mouse dragging, handles, live preview, atomic commit — implemented
111 point, line, polyline, rectangle, polygon, construction-geometry creation — implemented
112 circle, arc, ellipse, slot creation/editing — implemented
113 spline editing, continuity handles, Sketch text — implemented
114 manual and automatic geometric constraints with glyph interaction — next
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

Canonical contract: `docs/gui-interactive-sketch-workspace-mvp8.md`.

### Block 107 — Plane interaction — Implemented

Block 107 implements device-independent plane mapping, transient interaction scene projection,
zoom-stable hit priority, stacked-hit cycling, Window/Crossing selection, grid, snaps, and inference.

Canonical contract: `docs/gui-sketch-plane-interaction-mvp8.md`.

### Block 108 — Shared planar Sketch topology — Implemented

Block 108 introduces typed `SketchPointId` and canonical `SketchTopology`, dependency-safe edit
commands, deterministic historical migration, exact topology undo/redo, and canonical
`blcad.sketch_topology.mvp8` persistence.

Canonical contract: `docs/sketch-shared-topology-mvp8.md`.

### Block 109 — Deterministic planar constraint solver — Implemented

Block 109 adds the headless `SketchConstraintSolver`, complete initial residual families, canonical
ordering, deterministic damped solving, Jacobian-rank DOF, redundancy/conflict attribution, and
persistent-Sketch adaptation.

Canonical contract: `docs/sketch-planar-constraint-solver-mvp8.md`.

### Block 110 — Solver-backed Sketch mouse dragging — Implemented

Block 110 adds semantic handles, coalesced live solve, exact release flush, complete preview rollback,
source freshness checks, and one `Drag sketch handle` document transaction.

Canonical contract: `docs/gui-sketch-solver-drag-mvp8.md`.

### Block 111 — Basic Sketch creation tools — Implemented

Point, line, polyline, rectangle families, parallelogram, polygon, and centerline creation reuse the
workspace/plane/transaction authorities and commit ordinary Sketch or construction intent.

Canonical contract: `docs/gui-sketch-basic-creation-mvp8.md`.

### Block 112 — Circles, arcs, ellipses, and slots — Implemented

Block 112 adds full-circle `CircleProfile`/diameter-Parameter intent, circular arcs, deterministic
cubic ellipse spans, and ordered line/arc slot profiles through the existing creation lifecycle.

Canonical contract: `docs/gui-sketch-conic-slot-creation-mvp8.md`.

### Block 113 — Spline editing and Sketch text — Implemented

Block 113 implements connected-chain control-point editing, fit-point authoring, insertion/removal,
control polygons, deterministic Catmull-Rom-to-Bezier conversion, C0/G1 continuity handles, tangent
alignment, spline sampling/curvature diagnostics, immutable GUI preview, source freshness checks, and
one `Edit sketch spline` transaction with exact undo/redo.

It also adds stable parameter/expression-backed `SketchText` intent, canonical
`blcad.sketch_text.mvp8` version-1 sidecar persistence, deterministic unit-bearing token substitution,
and requested/system/built-in vector-font fallback with explicit diagnostics. Historical
`blcad.part_document.mvp1` remains unchanged.

Canonical contract: `docs/gui-sketch-spline-text-mvp8.md`.

Focused tags:

```text
[core][sketch-spline-edit]
[geometry][sketch-spline-edit]
[gui][sketch-spline]
[core][sketch-text]
[geometry][sketch-text]
```

## Current next technical step — Block 114

Expose compatible Block-109 constraints through selection-driven GUI commands and semantic glyphs.
Creation/edit workflows publish automatic constraint candidates; acceptance first solves a disposable
candidate and presents conflicts/redundancy. Failed or conflicting additions publish diagnostics and
leave persistent Sketch intent unchanged.

Focused tags:

```text
[gui][sketch-constraints]
[integration][sketch-auto-constraint]
```

## Remaining Interactive Sketcher sequence

Block 115 completes dimensions and expression binding. Block 116 adds trim/extend/split/Sketch
fillet/chamfer. Block 117 adds offset and associative projection. Block 118 adds transforms, copy,
mirror, and Sketch patterns. Block 119 closes region/profile/repair and Finish Sketch behavior. Block
120 adds Interactive Sketch3D. Block 121 is integrated acceptance and measured performance.

## Later phases

Interactive Part & Assembly Modeling MVP-9 is Blocks 122–131 and is canonical in
`docs/interactive-modeling-sequence-mvp9.md`.

STEP Import MVP-10 is Blocks 132–138 and is canonical in `docs/step-import-sequence-mvp10.md`.

## Sequencing rules

1. Implement exactly the current block before advancing status.
2. Read and reuse existing authority before introducing persistent intent.
3. New persistent intent requires an explicit Core contract, persistence policy, and focused headless
   proof before GUI consumers depend on it.
4. Transient GUI or Geometry state never becomes model identity to avoid a Core boundary.
5. Failed validation, solve, recompute, import, or Geometry execution publishes no partial mutation.
6. Update this file, the current phase sequence, feature-specific contract, setup/status documentation,
   and README after each block.
7. Advance `implemented_through`, `current_block`, `current_boundary`, and `current_tag` only after the
   implementation and proof are checked in.

## Current handoff

Block 113 is implemented. Block 114 is next. Read the Block-106/107 interaction contracts,
`docs/sketch-shared-topology-mvp8.md`, `docs/sketch-planar-constraint-solver-mvp8.md`, and all current
creation/edit contracts, then expose constraint authoring without introducing a second solver,
constraint identity, glyph state, or transaction authority.

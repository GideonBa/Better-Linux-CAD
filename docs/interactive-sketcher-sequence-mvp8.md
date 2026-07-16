# Interactive Sketcher Sequence MVP-8

Status: implemented and accepted through Block 121.

Interactive Sketcher MVP-8 spans Blocks 106–121. Core and Geometry remain the persistent and computational authorities; GUI code owns interaction state, presentation, and transaction orchestration only.

## Block 106 — Sketch workspace and command lifecycle — Implemented

Adds the contextual Sketch workspace, command lifecycle, HUD, Apply/Cancel behavior, selection ownership, and one accepted transaction boundary.

Canonical contract: `docs/gui-interactive-sketch-workspace-mvp8.md`.

## Block 107 — Plane interaction, hit testing, grid, and snapping — Implemented

Adds device-independent plane/model mapping, deterministic hit testing, box selection, grid display, snapping, and inference preview without implicit persistent constraints.

Canonical contract: `docs/gui-sketch-plane-interaction-mvp8.md`.

## Block 108 — Shared planar topology — Implemented

Adds stable shared point/entity topology, mutation commands, migration, and exact history semantics.

Canonical contract: `docs/sketch-shared-topology-mvp8.md`.

## Block 109 — Planar constraint solver — Implemented

Adds deterministic planar solving, DOF accounting, redundancy/conflict attribution, diagnostics, and fail-closed materialization.

Canonical contract: `docs/sketch-planar-constraint-solver-mvp8.md`.

## Block 110 — Solver-backed drag — Implemented

Adds semantic handles, disposable live solve, source freshness, stale-preview rejection, and one atomic release transaction.

Canonical contract: `docs/gui-sketch-solver-drag-mvp8.md`.

## Block 111 — Basic creation tools — Implemented

Adds point, line, polyline, rectangle families, parallelogram, regular polygon, centerline, construction geometry, and numeric absolute/relative/polar input.

Canonical contract: `docs/gui-sketch-basic-creation-mvp8.md`.

## Block 112 — Circles, arcs, ellipses, and slots — Implemented

Adds circle and arc families, deterministic cubic ellipse representation, and straight slot families through the shared command and transaction boundary.

Canonical contract: `docs/gui-sketch-conic-slot-creation-mvp8.md`.

## Block 113 — Splines and Sketch text — Implemented

Adds connected-chain spline editing, fit/control conversion, insertion/removal, continuity handles and diagnostics, plus parameter/expression-backed Sketch text with deterministic font fallback.

Canonical contract: `docs/gui-sketch-spline-text-mvp8.md`.

## Block 114 — Constraint authoring and inference — Implemented

Adds all accepted non-dimensional constraint families, stable intent, manual/automatic provenance, compatibility selection, conflict/redundancy preview, glyph interaction, persistence, and history integration.

Canonical contract: `docs/gui-sketch-constraint-authoring-mvp8.md`.

## Block 115 — Dimensions and expressions — Implemented

Adds driving/reference distance, length, radius, diameter, angle, and arc-length dimensions with parameter/expression binding, semantic annotations, persistence, and drag enforcement.

Canonical contract: `docs/gui-sketch-dimension-authoring-mvp8.md`.

## Block 116 — Modify tools — Implemented

Adds trim, extend, split, two-line fillet, and two-line chamfer as disposable Core candidates with reference-, profile-, constraint-, and dimension-aware fail-closed validation.

Canonical contract: `docs/gui-sketch-modify-mvp8.md`.

## Block 117 — Offset and projection — Implemented

Adds ordered line-chain/loop offset with deterministic miter joints, associative projected point/line records, construction-axis projection, and explicit projected-line break-link conversion.

Canonical contract: `docs/gui-sketch-offset-project-mvp8.md`.

## Block 118 — Transform, mirror, and patterns — Implemented

Adds stable-id move/rotate/scale, detached copy/mirror, rectangular/circular pattern generation, and explicit associative-versus-exploded pattern intent.

Canonical contract: `docs/gui-sketch-transform-pattern-mvp8.md`.

## Block 119 — Regions and Finish Sketch — Implemented

Adds connected-component line-region recognition, stable open/self-crossing/ambiguous diagnostics, point-based profile selection, and fail-closed Finish Sketch materialization.

Canonical contract: `docs/gui-sketch-regions-finish-mvp8.md`.

## Block 120 — Interactive Sketch3D — Implemented

Adds free, axis, and plane locks; typed XYZ and distance/azimuth/elevation input; atomic point/line creation; guide roles; shared-point handle movement; and explicit planar-point projection intent. No full variational 3D solver is implied.

Canonical contract: `docs/gui-sketch3d-interaction-mvp8.md`.

## Block 121 — Interactive Sketcher acceptance — Implemented

Adds `InteractiveSketcherAcceptance`, a machine-readable catalog covering all fifteen delivered capability families. Every record requires mouse/script equivalence, persistence/recompute, exact undo/redo, conflict atomicity, reference repair, keyboard Apply/Cancel, high-DPI mapping, and stale-preview rejection evidence.

Two deterministic tutorials freeze planar and Sketch3D command sequences and expected stable ids. Representative hover, drag, solve, and region-recognition budgets are cataloged for dedicated release-build benchmarks; ordinary CI validates the bounded budget metadata without nondeterministic wall-clock assertions.

Canonical contract: `docs/interactive-sketcher-acceptance-mvp8.md`.

Focused tags: `[integration][interactive-sketcher]`, `[integration][sketch-gui-headless]`, `[performance][sketch-interaction]`.

## Coverage matrix

| Capability | Canonical contract | Block |
|---|---|---:|
| Workspace and command lifecycle | `gui-interactive-sketch-workspace-mvp8.md` | 106 |
| Plane mapping, hit testing, snapping | `gui-sketch-plane-interaction-mvp8.md` | 107 |
| Shared planar topology | `sketch-shared-topology-mvp8.md` | 108 |
| Planar solver and diagnostics | `sketch-planar-constraint-solver-mvp8.md` | 109 |
| Solver-backed drag | `gui-sketch-solver-drag-mvp8.md` | 110 |
| Basic creation | `gui-sketch-basic-creation-mvp8.md` | 111 |
| Conics and slots | `gui-sketch-conic-slot-creation-mvp8.md` | 112 |
| Splines and text | `gui-sketch-spline-text-mvp8.md` | 113 |
| Constraint authoring | `gui-sketch-constraint-authoring-mvp8.md` | 114 |
| Dimension authoring | `gui-sketch-dimension-authoring-mvp8.md` | 115 |
| Modify tools | `gui-sketch-modify-mvp8.md` | 116 |
| Offset/project/references | `gui-sketch-offset-project-mvp8.md` | 117 |
| Transform/mirror/pattern | `gui-sketch-transform-pattern-mvp8.md` | 118 |
| Regions and Finish Sketch | `gui-sketch-regions-finish-mvp8.md` | 119 |
| Interactive Sketch3D | `gui-sketch3d-interaction-mvp8.md` | 120 |
| Integrated acceptance | `interactive-sketcher-acceptance-mvp8.md` | 121 |

## Explicit deferrals

The following remain outside MVP-8 and require explicit later boundaries:

- raster tracing, freehand drawing, handwriting recognition, proprietary UI cloning, collaboration cursors, and manufacturing-specific annotations;
- a full variational 3D Sketch solver;
- curved/three-point arc slots and a separate persistent analytic ellipse entity;
- spline open/closed toggling and dedicated spline self-intersection diagnostics;
- advanced Sketch-text layout and conversion to selectable profile loops;
- constraint suppression and advanced dimension annotation layout;
- Qt command/preview wiring for the narrowed Core-only Block-117 and Block-118 authorities;
- arc/spline offset, automatic chain discovery, alternative joins, and variable-distance offset;
- region recognition over arc and spline boundaries;
- interactive curved Sketch3D creation and arc/spline/helix parameter handles.

Interactive Part & Assembly Modeling MVP-9 begins at Block 122. STEP Import MVP-10 follows in Blocks 132–138.

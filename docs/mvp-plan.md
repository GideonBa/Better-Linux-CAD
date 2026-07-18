---
doc: MVP Plan
role: >-
  Implementation-sequence source of truth. Feature-specific documents remain
  canonical for exact contracts, formulas, persistence details, failure
  policies, ordering, and focused proofs.
implemented_through: Block 128
current_block: 129
current_boundary: Feature edit lifecycle and the Core feature-update command boundary
current_tag: "[core][feature-update-command]"
phase_status:
  mvp_1: "Single-part modeling — implemented"
  mvp_2: "Semantic references and richer sketch workflows — implemented"
  mvp_3: "Parametric bolt-circle pattern — implemented"
  mvp_4: "Assembly parameters and Project container — implemented"
  mvp_5: "Assembly relationships, motion, hierarchy, analysis, exchange — Blocks 1–47 implemented"
  mvp_6: "Part Construction — Blocks 48–94 implemented; MVP complete"
  mvp_7: "GUI Feature Validation — Blocks 95–105 implemented; MVP complete"
  mvp_8: "Interactive Sketcher — Blocks 106–121 implemented; MVP complete"
  mvp_9: "Interactive Part & Assembly Modeling — Blocks 122–128 implemented; Block 129 next"
  mvp_10: "STEP Import — Blocks 132–138 planned after Interactive Modeling acceptance"
---

# MVP Plan

This document is the numbered implementation-sequence source of truth for BLCAD. Exact feature
semantics are intentionally not duplicated here; feature-specific contracts remain authoritative for
mathematics, persistence spellings, migration rules, ordering, and failure policy.

## Current status

```text
implemented through  Block 128
current block        Block 129
current phase        Interactive Part & Assembly Modeling MVP-9
current boundary     feature edit lifecycle and the Core feature-update command boundary
```

Interactive Sketcher MVP-8 is complete and accepted. Blocks 122–123 establish the selection-first
modeling workspace and reusable candidate-only viewport manipulators; Block 124 adds interactive
Extrude, path Extrude, and Revolve authoring, Block 125 adds interactive Fillet, Chamfer, Shell, and
Draft authoring, Block 126 adds interactive Pattern, Mirror, Body Boolean, and Body Transform
authoring, Block 127 adds interactive PathCurve, Sweep, and Loft authoring, and Block 128 adds
interactive Surface authoring and surface-to-solid conversion over that infrastructure. Block 129 is
the current next technical step.

## Phase map

```text
MVP-1   single-Part parametric foundation                         implemented
MVP-2   semantic references and richer Sketch workflows           implemented
MVP-3   parametric bolt-circle pattern                            implemented
MVP-4   assembly parameters and Project container                 implemented
MVP-5   Assembly system                                 Blocks 1–47 implemented
MVP-6   Part Construction                              Blocks 48–94 implemented
MVP-7   GUI Feature Validation                        Blocks 95–105 implemented
MVP-8   Interactive Sketcher                          Blocks 106–121 implemented
MVP-9   Interactive Part & Assembly Modeling          Blocks 122–128 implemented; Block 129 next
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
Revolve, Patterns, Mirror, Fillet, Chamfer, Shell, Draft, Sketch3D/PathCurve, Sweep, path Extrude,
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
114 manual and automatic geometric constraints with glyph interaction — implemented
115 driving/reference dimensions, in-canvas editing, parameter/expression binding — implemented
116 trim, extend, split, corner fillet, corner chamfer — implemented
117 offset, project/include, construction axes, associative references — implemented
118 move, rotate, scale, copy, mirror, rectangular/circular Sketch patterns — implemented
119 region recognition, profile selection, diagnostics, repair, Finish Sketch — implemented
120 interactive Sketch3D creation and direct point/curve manipulation — implemented
121 integrated usability, persistence, solver, performance, GUI/headless acceptance — implemented
```

### Blocks 106–110 — Workspace, topology, solver, and drag — Implemented

Blocks 106–107 implement the contextual Sketch workspace and device-independent interaction. Block 108
introduces stable shared `SketchPointId`/entity topology and versioned topology persistence. Block 109
implements the deterministic general planar solver with DOF, redundancy, and conflict attribution.
Block 110 provides semantic handles, disposable live solve, source freshness, and one atomic release
transaction.

Canonical contracts:

- `docs/gui-interactive-sketch-workspace-mvp8.md`
- `docs/gui-sketch-plane-interaction-mvp8.md`
- `docs/sketch-shared-topology-mvp8.md`
- `docs/sketch-planar-constraint-solver-mvp8.md`
- `docs/gui-sketch-solver-drag-mvp8.md`

### Blocks 111–112 — Sketch creation — Implemented

Block 111 creates points, lines, polylines, rectangle families, polygons, centerlines, and construction
geometry. Block 112 adds exact full circles, circular arcs, deterministic cubic ellipses/elliptical arcs,
and line/arc slot profiles through the same interaction and transaction boundary.

Canonical contracts:

- `docs/gui-sketch-basic-creation-mvp8.md`
- `docs/gui-sketch-conic-slot-creation-mvp8.md`

### Block 113 — Spline editing and Sketch text — Implemented

Block 113 adds connected-chain control/fit-point spline editing, deterministic conversion,
insertion/removal, control polygons, C0/G1 diagnostics, immutable GUI preview, source freshness, and one
`Edit sketch spline` transaction. It also adds parameter/expression-backed Sketch text, versioned
`blcad.sketch_text.mvp8` sidecar persistence, and deterministic system/built-in vector-font fallback.

Canonical contract: `docs/gui-sketch-spline-text-mvp8.md`.

### Block 114 — Geometric constraint authoring and glyph interaction — Implemented

Block 114 adds stable point/entity `SketchConstraintIntent` targets for every non-dimensional Block-109
family, deterministic selection compatibility, manual/automatic provenance, snap-to-constraint
candidates, disposable solve/conflict/redundancy preview, and semantic accepted/preview/conflict glyphs.

Accepted candidates materialize a complete solved Sketch and commit one `Add sketch constraint` Part
transaction. The versioned `blcad.sketch_constraints.mvp8` sidecar stores only persistent intent;
solver results and glyph layout remain derived. `GuiDocumentSession` owns the catalog through dirty
state, Save/Open, global history, and later solver-backed drag.

Canonical contract: `docs/gui-sketch-constraint-authoring-mvp8.md`.

### Block 115 — Driving/reference dimensions and expressions — Implemented

Block 115 adds horizontal, vertical, aligned, point-to-point, line-length, arc-radius, arc-diameter,
angle, and calibrated arc-length dimensions over stable topology targets. Driving dimensions bind an
existing typed Length/Angle parameter or expression; reference dimensions only measure current solved
topology.

The versioned `blcad.sketch_dimensions.mvp8` sidecar is part of `GuiDocumentSession` dirty state,
Save/Open, and every global history snapshot. Parameter edits, complete solved Sketch geometry, and the
dimension catalog publish together. Semantic in-canvas annotations remain derived and clickable.
Block-110 drag now consumes both accepted Block-114 constraints and Block-115 dimensions.

Canonical contract: `docs/gui-sketch-dimension-authoring-mvp8.md`.

Focused tags:

```text
[core][sketch-dimensions]
[geometry][sketch-dimensions]
[gui][sketch-dimensions]
[integration][sketch-expression-edit]
[integration][sketch-live-solve]
```

### Block 116 — Trim, extend, split, fillet, and chamfer — Implemented

Block 116 adds `SketchModifyService` (Core) trim, extend, split, two-line fillet, and two-line
chamfer. It rewrites a disposable candidate Sketch with analytic line/arc intersection and De
Casteljau spline splitting: trim shortens to the picked bounding intersection, removes a bounded
middle as a split, or removes an unbounded entity; extend moves the picked end to the nearest
intersection beyond it; fillet/chamfer trim both lines to the setback and insert a tangent arc or
connector line.

In-place edits keep entity ids so referencing constraints, dimensions, and tangent continuity are
preserved. Removed/split ids referenced by embedded intent, and any modification of a profile-contour
entity, fail closed with an explicit diagnostic before a candidate is built.
`GuiSketchModifyController` previews without mutation and commits one atomic transaction that
re-derives the operation from the current Sketch and re-validates the Block-114 and Block-115 catalogs.

Canonical contract: `docs/gui-sketch-modify-mvp8.md`.

Focused tags:

```text
[core][sketch-modify]
[geometry][sketch-modify]
[gui][sketch-trim-extend]
```

### Block 117 — Offset, projection, and construction references — Implemented

Block 117 adds `SketchOffsetProjectService` (Core): ordered line-chain and closed-loop offset along
left-hand normals with deterministic miter joints, associative Project/Include of existing
`ProjectedSketchPoint`/`ProjectedSketchLine` records (construction axes project through their
`ConstructionLineId`), and explicit break-link conversion of a projected line into an ordinary
`LineSegment` with the same stable id. Break-link fails closed while embedded reference constraints
still depend on the projected entity. No new JSON schema; no parallel recovery semantics.

Canonical contract: `docs/gui-sketch-offset-project-mvp8.md`.

Focused tags:

```text
[core][sketch-offset-project]
```

### Block 118 — Transform, mirror, and Sketch patterns — Implemented

Block 118 adds `SketchTransformPatternService` (Core): id-preserving move/rotate/scale that weave
transformed replacement curves into the candidate before embedded intent is copied, so referencing
constraints, dimensions, and continuity are preserved in place; ordinary copy/mirror with
deterministic `.copy.N`/`.mirror.N` ids; and rectangular/circular patterns with explicit
`SketchPatternMode` (`Associative` returns a `SketchPatternIntent`, `Exploded` returns ordinary
entities). Copy, mirror, and patterns report non-duplicated referencing intent through
`uncopied_references` instead of silently discarding it.

Canonical contract: `docs/gui-sketch-transform-pattern-mvp8.md`.

Focused tags:

```text
[core][sketch-transform-pattern]
```

### Block 119 — Regions, profiles, diagnostics, and Finish Sketch — Implemented

Block 119 adds `SketchFinishService` (Geometry): endpoint-connected component partition of explicit
line entities, per-component validation through the established `SketchRegionFinder` failure policy,
typed open-contour/self-intersection/ambiguous-topology diagnostics, point-in-region profile
selection, and a fail-closed Finish Sketch that adds one ordinary `ClosedProfile` through
`PartDocument::update_sketch(...)`. Region ids are deterministic
`generated.region.<sketch>.<index>` values.

Canonical contract: `docs/gui-sketch-regions-finish-mvp8.md`.

Focused tags:

```text
[geometry][sketch-regions]
[gui][sketch-profile-selection]
[integration][sketch-finish]
```

### Block 120 — Interactive Sketch3D — Implemented

Block 120 adds `Sketch3DInteractionService` (Core) over the persistent `Sketch3D` model: orthogonal
axis/plane locks with typed XYZ and distance/azimuth/elevation input, atomic point/line placement
with optional guide roles, reference-preserving point-handle moves, and planar-point projection that
publishes an explicit `Sketch3DProjectedPointIntent`. No second spatial-curve representation and no
variational 3D constraint solver are introduced.

Canonical contract: `docs/gui-sketch3d-interaction-mvp8.md`.

Focused tags:

```text
[gui][sketch-3d-edit]
[integration][sketch-3d-direct-manipulation]
```

Both Block-120 tags are registered on headless Core tests in `tests/core/sketch_3d_tests.cpp`.

## Block 121 — Integrated Interactive Sketcher acceptance — Implemented

The machine-checked coverage manifest and integrated acceptance prove deterministic tutorial
documents, GUI/headless recompute equivalence, pointer/model-target equivalence, exact drag
Undo/Redo, Cancel/stale-preview atomicity, high-DPI mapping, and measured interaction paths. See
`docs/interactive-sketcher-mvp8-acceptance.md`.

## Block 122 — Selection-first modeling workspace and navigation — Implemented

`GuiModelingWorkspace` gives the application shell one shared transient Part/Surface/Assembly
interaction authority. Verified semantic capabilities drive exact first-input command enablement and
deterministic mini-toolbar ordering. Command start consumes preselection; Cancel restores the complete
semantic selection and capability context; accepted commands become repeatable. Finish Sketch can
handoff an authoritative profile directly to the profile-first command family. Session and viewport
selection filters stay synchronized, and ViewCube targets, home view, and named camera bookmarks are
transient.

Canonical contract: `docs/gui-modeling-workspace-mvp9.md`.

Focused tags:

```text
[gui][modeling-workspace]
[gui][in-context-command]
[gui][view-navigation-aids]
```

## Block 123 — Transient viewport manipulators and numeric coupling — Implemented

`GuiViewportManipulatorLayer` provides reusable linear, angular, radial, translate/rotate-triad,
PatternCount, and PatternSpacing handles. Presentation and hit tolerances are fixed in DIP, while
candidate values derive from deterministic camera rays, model axes, and explicit handle planes.
Overlapping hits resolve by screen distance and stable handle id. Dragging emits candidate values only;
release processes the exact final sample. Valid numeric HUD input overrides later pointer movement
until explicitly cleared. `GuiViewportManipulatorShellBinder` owns the transparent overlay, mouse
capture, cancellation, and HUD synchronization without receiving document mutation authority.

Canonical contract: `docs/gui-viewport-manipulators-mvp9.md`.

Focused tags:

```text
[gui][viewport-manipulators]
[gui][manipulator-numeric-coupling]
```

## Block 124 — Interactive Extrude, path Extrude, and Revolve — Implemented

`GuiInteractiveExtrudeController` and `GuiInteractiveRevolveController` (headless) upgrade the MVP-7
form-driven commands into direct manipulation: a materialized profile region, driving `Length`
parameter or revolve axis, and result `Body` seed a disposable candidate feature. Block-123
extent/taper/thin/angle handles drive the parameters; preview validates a PartDocument clone through
the recompute plan; Apply commits one transaction that seeds the driving parameters and adds the
feature. All seven extent modes, taper, thin intent, path direction, and the three revolve extents
remain frozen Core contracts. `GuiInteractiveFeatureCoordinator` (owned by `MainWindow`) connects the
active controller to the shared Block-123 manipulator layer, folding viewport candidates into the
candidate without moving authority into a widget.

Canonical contract: `docs/gui-interactive-extrude-revolve-mvp9.md`.

Focused tags:

```text
[gui][interactive-extrude]
[gui][interactive-revolve]
[integration][extrude-revolve-manipulator]
```

## Block 125 — Interactive Fillet, Chamfer, Shell, and Draft — Implemented

`GuiInteractiveFinishingController` (Fillet/Chamfer) and `GuiInteractiveShellDraftController`
(Shell/Draft) are headless controllers that collect an ordered, duplicate-free chain of semantic
edges/faces, drive Block-123 radius/distance/angle/thickness handles into existing `Length`/`Angle`
driving parameters, preview a disposable PartDocument clone through the recompute plan, and commit one
transaction that seeds the parameters and adds the feature. The three chamfer modes, shell direction,
and the draft pull/neutral/angle inputs stay frozen Core contracts; edges/faces without stable
semantic identity are unpickable. `GuiInteractiveFeatureCoordinator` now also owns these controllers.

Canonical contract: `docs/gui-interactive-finishing-mvp9.md`.

Focused tags:

```text
[gui][interactive-finishing]
[gui][interactive-shell-draft]
[integration][edge-chain-picking]
```

## Block 126 — Interactive Pattern, Mirror, Body Boolean, and Body Transform — Implemented

`GuiInteractivePatternMirrorController` (Linear/Circular Pattern, Mirror) and
`GuiInteractiveBodyOperationController` (Body Boolean, Body Transform) are headless controllers that
collect ordered Feature/Body sources or tool Bodies, drive Block-123 spacing/count/angle handles or
translate-triad/rotate/scale handles into existing `Count`/`Length` parameters and literals, preview a
disposable PartDocument clone, and commit one transaction. Body Transform appends exactly one
Translate/Rotate/UniformScale record to the persistent stack and never collapses it; the target Body
can never also be a boolean tool. `GuiInteractiveFeatureCoordinator` now also owns these controllers.

Canonical contract: `docs/gui-interactive-pattern-body-mvp9.md`.

Focused tags:

```text
[gui][interactive-pattern-mirror]
[gui][interactive-body-operation]
[integration][pattern-ghost-preview]
```

## Block 127 — Interactive PathCurve, Sweep, and Loft — Implemented

`GuiInteractivePathSweepController` (PathCurve, Sweep) and `GuiInteractiveLoftController` (Loft) are
headless controllers that collect an ordered, duplicate-free chain of connected segments, a
profile+trajectory with an optional twist `Angle` parameter or guide (never both), or ordered loft
sections with `reorder_section` drag-reorder and C0/G1/G2 continuity. Each previews a disposable
PartDocument clone and commits one transaction; the sweep twist is the only Block-123 handle. Combined
guide-plus-twist and unsupported G2 continuity surface as fail-closed diagnostics.
`GuiInteractiveFeatureCoordinator` now also owns these controllers.

Canonical contract: `docs/gui-interactive-path-sweep-loft-mvp9.md`.

Focused tags:

```text
[gui][interactive-path-sweep]
[gui][interactive-loft]
[integration][ordered-section-picking]
```

## Block 128 — Interactive Surface authoring and surface-to-solid — Implemented

`GuiInteractiveSurfaceController` (headless) authors all six `SurfaceFeature` kinds — Boundary/Fill,
Trim, Extend, Stitch, and ClosedShellToSolid — collecting boundary curves or surfaces, driving the
`surface.extend` distance handle and a typed stitch tolerance, and committing one transaction. Because
stitch free-edge/gap and closed-shell closedness diagnostics are geometric, preview runs the real
`GeometryRecomputeExecutor` on a throwaway clone so the authoritative diagnostic surfaces before
commit. `GuiInteractiveFeatureCoordinator` now also owns this controller.

Canonical contract: `docs/gui-interactive-surface-mvp9.md`.

Focused tags:

```text
[gui][interactive-surface]
[integration][stitch-diagnostic-overlay]
```

## Current next technical step — Block 129

Produce the checked-in feature update-capability inventory and add per-family Core feature-update/remove
commands where the inventory proves them missing, then open the double-click feature edit lifecycle
over the Block-124..128 controllers, as defined in `docs/interactive-modeling-sequence-mvp9.md`.

## Later phases

Interactive Part & Assembly Modeling MVP-9 continues through Block 131. STEP Import MVP-10 follows
in Blocks 132–138.

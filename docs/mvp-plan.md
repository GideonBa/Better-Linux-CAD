---
doc: MVP Plan
role: >-
  Implementation-sequence source of truth. Feature-specific documents remain
  canonical for exact contracts, formulas, persistence details, failure
  policies, ordering, and focused proofs.
implemented_through: Block 135
current_block: 136
current_boundary: Feature full coverage in the replacement shell
current_tag: "[gui][shell-coverage]"
phase_status:
  mvp_1: "Single-part modeling — implemented"
  mvp_2: "Semantic references and richer sketch workflows — implemented"
  mvp_3: "Parametric bolt-circle pattern — implemented"
  mvp_4: "Assembly parameters and Project container — implemented"
  mvp_5: "Assembly relationships, motion, hierarchy, analysis, exchange — Blocks 1–47 implemented"
  mvp_6: "Part Construction — Blocks 48–94 implemented; MVP complete"
  mvp_7: "GUI Feature Validation — Blocks 95–105 implemented; MVP complete"
  mvp_8: "Interactive Sketcher — Blocks 106–121 implemented; MVP complete"
  mvp_9: "Interactive Part & Assembly Modeling — Blocks 122–131 implemented; MVP complete"
  mvp_9r: "GUI Shell Reset — Blocks 132–135 implemented; Block 136 in progress"
  mvp_10: "STEP Import — Blocks 137–143 planned"
---

# MVP Plan

This document is the numbered implementation-sequence source of truth for BLCAD. Exact feature
semantics are intentionally not duplicated here; feature-specific contracts remain authoritative for
mathematics, persistence spellings, migration rules, ordering, and failure policy.

## Current status

```text
implemented through  Block 135
current block        Block 136
current phase        GUI Shell Reset MVP-9R
current boundary     Feature full coverage in the replacement shell
```

Interactive Sketcher MVP-8 is complete and accepted. Blocks 122–123 establish the selection-first
modeling workspace and reusable candidate-only viewport manipulators; Block 124 adds interactive
Extrude, path Extrude, and Revolve authoring, Block 125 adds interactive Fillet, Chamfer, Shell, and
Draft authoring, Block 126 adds interactive Pattern, Mirror, Body Boolean, and Body Transform
authoring, Block 127 adds interactive PathCurve, Sweep, and Loft authoring, Block 128 adds
interactive Surface authoring and surface-to-solid conversion, and Block 129 adds the feature edit
lifecycle and Core feature-update commands over that infrastructure, and Block 130 adds interactive
Assembly placement, compatibility-filtered relationships, joint frames, and coordinate motion.
Block 131 closes MVP-9 with read-only Measure, coverage manifest v2, and integrated acceptance.

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
MVP-9   Interactive Part & Assembly Modeling          Blocks 122–131 implemented
MVP-9R  GUI Shell Reset                       Blocks 132–135 implemented; Block 136 in progress
MVP-10  STEP Import                                            Blocks 137–143 planned
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
geometry. Block 112 adds exact full circles, circular arcs and exact rational ellipses,
and line/arc slot profiles through the same interaction and transaction boundary. Shell hardening
adds reference-based tangent construction, persistent dimension placement and inline editing, open
Sketch completion, profile-aware splitting/corners and spline continuation. These changes and their
focused proofs are recorded in the respective contracts; Block 136 remains planned for the remaining
feature command surface.

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

## Block 129 — Feature edit lifecycle and Core feature-update commands — Implemented

`PartDocument` gained `update_<family>`/`remove_<family>` for all thirteen feature families; update
replaces a record in place with a stable id, ordered position, and JSON shape while preserving
downstream and result-body co-producer edges, and remove is dependency-protected. The checked-in
inventory `docs/gui-feature-update-inventory-mvp9.json` records the before/after authorities. The
headless `GuiFeatureEditController` identifies a double-clicked feature's family, previews a disposable
edited candidate, and commits one update transaction that fails closed when the edit or any downstream
recompute fails. New Core intent is confined to `[core][feature-update-command]`; no JSON shape
changes; BodyTransform stack edits and feature reorder are explicit non-goals.

Canonical contract: `docs/gui-feature-edit-mvp9.md`.

Focused tags:

```text
[core][feature-update-command]
[gui][feature-edit]
[integration][feature-edit-recompute]
```

## Block 130 — Interactive Assembly placement, relationships, joints, and motion — Implemented

The headless Block-130 controllers insert/edit component and rigid-subassembly occurrences through
translate/rotate triads, filter second relationship/joint targets through the existing capability
matrices, preview relationships with the real solver and joints with root-space oriented frames, and
drive exactly one typed joint coordinate with limits plus transient DOF/solve HUD status. Relationship
intent and solved pose, and motion coordinate plus posed transforms, each commit in one Project
transaction. Spherical is rejected as a selected drive; no Core/Geometry/persistence intent changes.

Canonical contract: `docs/gui-interactive-assembly-mvp9.md`.

Focused tags:

```text
[gui][interactive-assembly-placement]
[gui][interactive-relationships]
[gui][interactive-joint-motion]
```

## Block 131 — Measure, coverage manifest v2, and integrated acceptance — Implemented

Read-only Measure queries fresh derived Geometry for point/edge/face distances, angles,
radius/diameter, and Body volume/solid counts without document mutation. Coverage manifest v2
machine-checks every MVP-7 family disposition, and integrated Part/Assembly, equivalence,
fail-closed, persistence/export, and measured interaction proofs complete MVP-9.

Canonical contracts: `docs/gui-measure-mvp9.md` and
`docs/interactive-modeling-mvp9-acceptance.md`.

## GUI Shell Reset MVP-9R — Blocks 132–136

The Qt shell that accumulated through MVP-7..9 (MainWindow plus seven self-installing binders that
discover widgets via `findChild` object names, hijack foreign QActions, and poll on timers, with two
competing workspace tab bars) proved unusable as a foundation and is replaced outright. Everything
below the shell is kept unchanged: Core, Geometry, `GuiDocumentSession`, `GuiCommandRegistry`,
selection/browser models, all workbenches, and the Qt-free interactive controllers and coordinators
of Blocks 106–131. The replacement shell is Inventor-inspired: one ribbon, one browser, one
viewport, direct ownership wiring (the shell constructs its components and passes references —
no `findChild` discovery, no self-installation, no action rewiring, no polling).

Sequence contract: `docs/gui-shell-reset-sequence-mvp9r.md`.

- Block 132 — QOpenGLWidget viewport foundation — Implemented: `OcctViewport` rendering internals
  rebuilt on a mouse-transparent `QOpenGLWidget` render-surface child (OCCT renders into Qt's GL
  context via `Aspect_NeutralWindow` + `OpenGl_Context`/FBO wrapping, driver `buffersNoSwap`; the
  official OCCT Qt pattern). `paintGL` is the only redraw site; every state change funnels through
  a single scheduled repaint. Public viewport API unchanged; Block-122/123 layers and headless
  tests untouched. Eliminates the native-window flicker class at the root.
- Block 133 — Shell reset — Implemented: MainWindow, all seven binders, and the shell-bound tests
  deleted; new `ShellWindow` (src/gui/shell/) with Inventor-style `ShellRibbon`
  (3D-Modell/Skizze/Ansicht), model browser with "Ursprung" folder, diagnostics, status bar,
  document lifecycle, selection and revision-gated refresh flow, view controls, origin seeding
  (`DatumPlane::xz/yz` Core extension + `seed_origin_geometry`), and the sketch entry skeleton
  ("2D-Skizze erstellen" on a selected origin plane with the adaptive white grid).
- Block 134 — Sketch in the new shell — Implemented: `ShellSketchTools` replaces the five deleted
  binders as one shell-owned component with explicit references. Ribbon groups (German UI labels):
  `Zeichnen` (21 tools as family menus), `Abhängigkeiten` (13 families plus delete, verified
  end-to-end), `Bemaßung` (9 kinds, reference/driving, edit), `Raster` toggles, and solver-backed drag
  as the default tool. The `Ändern` group (Modify) was initially deferred to Block 136 but is wired in
  Block 134: Trim/Extend/Split as modal pick tools, two-curve Fillet/Chamfer with auto-constraints and
  a driving size dimension (`docs/gui-shell-reset-sequence-mvp9r.md`, "Ändern group").
- Block 135 — Interactive features minimal — Implemented: `ShellFeatureTools` makes Extrude and
  Revolve reachable from the ribbon through `GuiInteractiveFeatureCoordinator`, owns the
  notification-driven manipulator overlay and right-side numeric task panel, and provides validated
  preview/apply/cancel. Finish Sketch hands off one materialized profile. Fresh Parts provision the
  driving Length parameter and result Body only inside the final feature transaction, so Cancel is
  mutation-free and one Undo removes the complete operation. This is the first fully usable
  end-to-end replacement shell (sketch → profile → extrude/revolve). The follow-up hardening audit
  adds persistent viewport selection for topology-authored profiles, stale-selection invalidation,
  strict unit parsing, numeric drag override and rollback, collision-safe generated inputs,
  immediate camera remapping, and expanded negative/atomic proofs.
- Block 136 — Feature full coverage: remaining feature commands (Fillet, Chamfer, Shell, Draft,
  Pattern, Mirror, Boolean, Transform, Sweep, Loft, Surface, Measure, Assembly), feature edit via
  browser double-click, acceptance coverage.

### Sketch workflow follow-up — 21 October 2026

The replacement shell now supports nonmodal transforms with multiple curve picks, box selection,
snapped dragging and numeric refinement; separate axis/element picking for Mirror and Symmetric;
persistent dashed construction and dash-dot center lines; orthographic projection with visible
committed references; and fitted spline creation until right-click, including closure and extrusion.
Closed spline and mixed spline/line contours remain usable alongside helper geometry. The
elliptical-arc GUI command is removed; exact conic spans remain available internally. These are
implemented usability improvements, not acceptance of the remaining Block-136 feature surface.

Contracts and repeatable proof: `docs/gui-sketch-transform-pattern-mvp8.md`,
`docs/gui-sketch-offset-project-mvp8.md`, `docs/gui-sketch-spline-text-mvp8.md`, and
`docs/development-setup.md`.

### Block 136 — Finishing command slice

The replacement shell restores Fillet, all three Chamfer modes, Shell and Draft through the shared
Feature task panel, real Body previews, numeric/drag input and atomic parameter-plus-feature Apply.
The target picker supports current, unambiguous semantic box edges/faces, including rectangles drawn
as four Sketch lines. Cancel, Undo/Redo and Save/Open use the existing authorities. Contract and proof:
`docs/gui-interactive-finishing-mvp9.md`, tag `[block136-finishing]`.

The remaining Block-136 work is tracked by the follow-up slices and current next step below.

### Block 136 — Body pattern and Mirror slice

The replacement shell restores linear and circular Body patterns and Body Mirror through the shared
Feature task panel. Multiple source Bodies can be toggled in the model or list; datum/construction
axes and planes can be chosen from the editor or picked in the model. Spacing/total extent, direction,
angle and integer count share live Geometry previews with the numeric editors and separate drag
handles. Candidate-only parameters and the result Body commit atomically with the feature. Cancel,
Undo/Redo and Save/Open preserve the existing authorities. Contract and proof:
`docs/gui-interactive-pattern-body-mvp9.md`, tag `[block136-patterns]`.

The remaining Block-136 work is tracked by the follow-up slices and current next step below.

### Block 136 — Body Boolean and Transform slice

The replacement shell restores Body Add/Subtract/Intersect with explicit target/tool roles, both
result modes, tool retention, and candidate-only NewBody provisioning. Translate/Rotate/UniformScale
share multiple Body sources, World-coordinate numeric controls and live drag handles, including a
dimensionless scale factor and explicit center. Actual multi-Body previews hide replaced/consumed
sources and remain disposable. Apply appends one transform record per selected Body in one atomic
transaction; Cancel, Undo/Redo, Save/Open and explicit ownership flags preserve existing authorities.
Contract and proof: `docs/gui-interactive-pattern-body-mvp9.md`, tag `[block136-body]`.

The remaining Block-136 work is tracked by the follow-up slices and current next step below.

### Block 136 — PathCurve, Sweep and Loft slice

The replacement shell restores ordered PathCurve creation, Sweep and Loft through the shared task
panel, explicit source roles and actual Geometry previews. Path segments and Loft sections can be
reordered by dragging or task buttons, with explicit reversal/normal flips. Sweep exposes twist
handles and numeric refinement; guides, center paths, orientation and continuity retain their existing
contracts. Solid, cut and surface modes use correctly typed candidate Bodies or an existing cut target.
New result Bodies and twist parameters commit atomically with the feature. Closed fitted-spline
profiles remain usable, and planar rational spline trajectories reuse deterministic 32-span resolution.
Cancel, Undo/Redo and Save/Open preserve the existing authorities. Contract and proof:
`docs/gui-interactive-path-sweep-loft-mvp9.md`, tag `[block136-spatial]`.

The replacement shell restores Boundary/Fill, Trim/Extend, Stitch (Knit/Sew) and closed-shell-to-solid
through the shared task panel and real Geometry previews. Ordered input lists, separate target/contour
pick roles, an outward extension drag with numeric refinement and a typed stitch tolerance consume
the existing Surface contracts. New result Bodies and parameters remain candidate-only until one
Apply; Cancel, Undo/Redo and Save/Open preserve the existing authorities. Planar Fill stays an exact
plane for reliable downstream Trim. Contract/proof: `docs/gui-interactive-surface-mvp9.md`,
`[block136-surface]`.

The replacement shell restores read-only Measure for distance, angle, radius/diameter and Body
volume/solid count. Explicit A/B roles and native topology filters select fresh derived Bodies or
posed Project components. Disposable target identities and readouts are cleared after model or
freshness changes; witness overlays follow camera changes. Measurement preserves document, history,
dirty state and save format. Contract/proof: `docs/gui-measure-mvp9.md`, `[block136-measure]`.

The replacement shell restores Assembly Project creation/definition loading, component and rigid
subassembly placement, compatibility-filtered root relationships/joints, coordinate motion and group
solve. Actual posed previews, native handles and numeric inputs remain candidate-only until one
Project transaction. Passive Spherical and bounded root-local authoring retain their existing limits.
Contract/proof: `docs/gui-interactive-assembly-mvp9.md`, `[block136-assembly]`.

The replacement shell restores browser-double-click editing for all thirteen persistent feature
families. Existing inputs, parameter bindings and ordered references preload into an edit task;
scalar drag/numeric changes recompute the complete candidate before one atomic update. Stable feature
and result identities, expression bindings, downstream failure handling, history and persistence retain
their authorities. Contract/proof: `docs/gui-feature-edit-mvp9.md`, `[block136-feature-edit]`.

The replacement shell restores Part and flattened/structured Assembly STEP export plus read-only
interference, clearance, contact and local DOF inspection. Failed exports preserve existing targets;
analysis results expire after model changes. Source intent, history and authored poses remain unchanged.
Sampled sweep controls and occurrence-linked result highlighting remain open. Contract/proof:
`docs/gui-analysis-step-export-workbench-mvp7.md`, `[block136-inspect-export]`.

Whole-block acceptance remains open.

### Block 136 — Construction and workplane creation slice — 2026-10-23

The replacement shell restores explicit construction points, lines, planes and datum axes, lines
through two points, axes from construction lines, three-point planes, parameter-bound offset planes
and supported semantic-face workplanes. The nonmodal task provides disposable resolved Geometry
previews, input rejection and one atomic Apply, including a newly provisioned offset parameter.
Resolved helper geometry is visible and selectable, and construction/derived planes now support
browser/viewport selection followed by normal-to-plane Sketch creation and downstream extrusion.
Undo/Redo and canonical Save/Open retain existing authorities. Contract/proof:
`docs/gui-sketch-workbench-mvp7.md`, `[block136-construction]`, `[geometry][construction-presentation]`.

In-place construction/workplane editing, generated-reference and parallel-through-point authoring,
and native reference collection/manipulators remain explicit gaps. This slice does not close the
datum family or Block 136.

### Block 136 — Combined UI workflow regression — 2026-10-23

Five longer ShellWindow regressions combine actual ribbon/menu, browser, viewport Sketch, task and
dialog input paths across machining, multi-Body operations, multi-workplane Loft, Assembly consumers
and cancellation/recovery. They check independent Geometry results, multi-step and branching history,
complete canonical save/reopen and read-only export/analysis. Proof: `[shell-combined-workflow]`;
scope and commands: `docs/gui-shell-acceptance-mvp9r.md`, `docs/development-setup.md`.

This extends integrated shell evidence without closing the remaining feature-family/input variants
or advancing Block 136. Construction/workplane editing was the next implementation step; its
restored boundary is recorded below.

### Block 136 — Construction and workplane editing slice — 2026-10-23

Browser double-click reopens the nine restored construction/workplane definitions for same-ID name,
placement and reference edits. Core typed updates preserve ordered intent and downstream graph edges,
reject cycles atomically and invalidate consumers. Disposable previews recompute downstream Geometry;
Apply retains existing session history and persistence authorities. Offset rebinding can provision a
fresh parameter without changing shared bindings. Proof: `[core][construction-update]`,
`[block136-construction-edit]`, including the sixth `[shell-combined-workflow]` scenario.
Contract: `docs/gui-sketch-workbench-mvp7.md`; validation: `docs/gui-shell-acceptance-mvp9r.md`.

Generated-reference and parallel-through-point creation/editing, native reference collection and
manipulators remain open. This slice does not close the datum family or Block 136.

### Block 136 — Generated construction references and parallel relations — 2026-10-23

The construction task adds creation/editing for semantic edge-midpoint and vertex points, parallel
lines through points using construction-line or generated-edge directions, and parallel workplanes
through points. All fourteen restored definitions retain existing identity, preview, transaction,
history and persistence contracts. Generated points now resolve correctly in three-point/parallel
workplanes; Core/JSON validate and load their dependencies while preserving authored order.
Contract: `docs/gui-sketch-workbench-mvp7.md`. Proof: `[construction-generated]`,
`[block136-construction-relations]`, including the seventh `[shell-combined-workflow]` scenario.

Native reference collection and construction manipulators remain open. This slice does not close
the datum family or Block 136. Verification is recorded in `docs/gui-shell-acceptance-mvp9r.md`.

### Block 136 — Mixed-path oriented Loft verification — 2026-10-23

Deeper Loft/Sweep verification adds three/five unequal rectangle/trapezoid sections on
independently oriented and rolled planes along connected line, cubic spline and reversed arc sources.
It fixes the Loft control sampler's arc-to-chord reduction: binding stations and intermediate samples
now retain true circular length/positions. Existing uniform normalized section stations and 32-span
spline resolution remain explicit limits. Independent OCCT boundary checks cover solid/surface,
open/guide controls, recompute, persistence and transactional rejection. A separate shell workflow
opens prepared Sketch/plane inputs, then authors the path/Loft, recovers invalid direction/order/G2,
edits a middle parameter and exercises history, canonical reopen and STEP through Qt input.

Contracts/proof: `docs/part-guided-loft-geometry-mvp6.md`,
`docs/gui-interactive-path-sweep-loft-mvp9.md`, `[mixed-path-loft]`, `[shell-mixed-path-loft]`.
Verification is recorded in `docs/gui-shell-acceptance-mvp9r.md`. This slice does not close Block 136;
native construction reference collection/manipulators remain the next implementation step.

### Block 136 — Native construction reference collection — 2026-10-23

The construction task now collects existing points/lines/planes and supported generated faces,
edges and vertices through native model clicks, with an explicit input role and ordered point
collection. One typed Geometry catalogue drives generated selectors and disposable presentations;
role-scoped selection rejects unrelated/overlapping objects. Create/Edit preserve the existing
candidate, transaction, history and persistence contracts. Custom masks/scopes are restored on exit;
hidden and unavailable source geometry is omitted. Explicit coordinates/directions remain numeric.

Contract/proof: `docs/gui-sketch-workbench-mvp7.md`, `[block136-construction-picking]`,
`[geometry][construction-picking]`. Native mouse regressions and full-suite evidence are recorded in
`docs/gui-shell-acceptance-mvp9r.md`. Construction position/direction/offset manipulators remain open;
this slice does not close the datum family or Block 136.

### Block 136 — Construction position, direction/frame and offset grips — 2026-10-23

The construction task consumes the shared manipulator layer for explicit XYZ positions, World-axis
line/axis directions and complete plane frames, plus positive offsets for new candidate parameters.
Typed input overrides further pointer motion; final release, two-level Escape, focus/camera changes
and task expiry preserve candidate/transaction authority. Full-precision frames survive rounded
field presentation and later edits. Reference-driven definitions and shared offset bindings retain
their reference/read-only controls. The headless camera bookmark path now retains restored values.

Contract/proof: `docs/gui-sketch-workbench-mvp7.md`, `docs/gui-viewport-manipulators-mvp9.md`,
`[shell-construction-manipulators]`, `[shell-combined-manipulators]` (the eighth combined UI workflow).
The datum family's bounded surface is restored; remaining family gaps and integrated acceptance keep
Block 136 open. Verification is recorded in `docs/gui-shell-acceptance-mvp9r.md`.

### Block 136 — Explicit reference repair — 2026-10-23

The replacement shell now diagnoses and explicitly rebinds projected Sketch points/lines and
semantic-face workplanes. Preview recomputes a disposable Part; Apply revalidates one atomic history
transaction while retaining consumer IDs, constraints and dependency edges. Selector changes revoke
preview; cancellation, save, history and document/revision expiry preserve committed intent. Core and
shell regression proofs cover fallbacks, cyclic rejection, placement, persistence and actual input.
Contract: `docs/gui-sketch-workbench-mvp7.md`; proof: `[core][sketch-reference-repair]` and
`[block136-reference-repair]`. Test results are recorded in `docs/gui-shell-acceptance-mvp9r.md`.
The family remains partial because initial-load failures, interdependent repair batches and explicit
constraint-diagnostic suggestion controls remain open. Block 136 has not advanced to complete.

### Block 136 — Atomic reference-repair lists — 2026-10-23

The existing repair task now stages multiple replacements, updates repeated choices per scoped
consumer identity, and supports remove/reset before one complete Geometry preview and atomic Apply.
Staging can repair an upstream plane before choosing dependent projected inputs while another broken
reference still prevents full presentation. Invalid stages preserve the previous queue; all exit paths
discard it. Preview includes the union of affected consumers and downstream outputs. One Undo/Redo
restores the complete repair, with canonical persistence and no new document schema fields.

Contract/proof: `docs/gui-sketch-workbench-mvp7.md`, `[block136-reference-repair-batches]`, including
actual ribbon/popup/list/button input. Verification lives in `docs/gui-shell-acceptance-mvp9r.md`.
The batch prerequisite is implemented before repair-time loading; the family and Block 136 remain
partial for the remaining loading and constraint-diagnostic controls.

### Block 136 — Repair-time opening and failed-state history — 2026-10-23

An explicit Part repair-open path now retains Core-valid intent when initial Geometry recompute
fails. Failed states publish no ShapeCache, keep the original saved/dirty boundary, and allow only
validated repair publication. The repair task builds provisional successful producer results locally
for typed target choices; a complete candidate recompute remains mandatory. Undo can restore the
original failed snapshot without reusing repaired geometry; Redo restores the successful model.
Normal Open, Core validation, sidecar validation and Project admission retain their existing bounds.

Contract/proof: `docs/reference-recovery-mvp.md`, `docs/gui-sketch-workbench-mvp7.md`,
`[repair-open-session]` and actual file/ribbon/popup/list/button input in `[block136-repair-open]`.
Verification lives in `docs/gui-shell-acceptance-mvp9r.md`. Core-invalid missing-source documents and
constraint-diagnostic suggestion controls still keep the family and Block 136 partial.

### Block 136 — Missing-source reference recovery — 2026-10-23

Explicit Part repair loading now retains absent sources of existing face-derived workplanes and
projected semantic/construction points and lines. The loader preserves typed intent and authored
order without synthetic source records; ordinary loading and unrelated Core validation stay strict.
Intermediate Sketch repairs retain only unchanged missing inputs. Complete Geometry validates all
reference sources before cache mutation, so partial repairs cannot publish even when a projected
input has no executable consumer. Failed-state saving/history reuse the bounded repair-load mode.

Contract/proof: `docs/reference-recovery-mvp.md`, `[missing-reference-recovery]`,
`[missing-reference-session]` and actual six-input shell workflow `[block136-missing-reference]`.
Verification lives in `docs/gui-shell-acceptance-mvp9r.md`. This closes the recorded bounded
missing-source loading gap; constraint-diagnostic suggestion controls keep the family and Block 136
partial. Missing sources in other feature/construction kinds remain outside this repair contract.

### Block 136 — Explicit Sketch diagnostic suggestions — 2026-10-23

The shell now exposes existing Sketch diagnostics and safe explicit repair suggestions through an
owned task in the model and Sketch ribbons. Supported fixes require a fully validated disposable
session Preview before one Apply transaction; unsupported/manual suggestions remain visible and
blocked. Active Sketch interactions resume from committed intent on exit. Catalogs, persistence,
history and task/revision boundaries retain their authority. The bounded planar/reference family
is restored, with no expansion of Core repair kinds or live-solver diagnostics.

Contract/proof: `docs/gui-sketch-workbench-mvp7.md` and `[block136-sketch-diagnostics]`, including
actual ribbon/list/button/camera/history/file input. Verification lives in
`docs/gui-shell-acceptance-mvp9r.md`. Ten families have restored bounded surfaces and eight retain
explicit gaps. The entire Block 136 remains in progress.

### Block 136 — Typed Sketch3D point/line editor — 2026-10-23

The shell now creates spatial points and shared-point lines and reopens existing Sketch3D records
for typed XYZ point editing. Local changes require a complete disposable downstream Preview before
one Apply transaction; parameter detachment is explicit. Same-ID Core updates preserve order and
references while revalidating consuming paths. Focused tests retain curved/guide intent and reject
invalid candidates; the combined UI scenario creates PathCurve/Sweep, edits a spatial endpoint,
checks four-step history, canonical reopening and STEP. Contract:
`docs/gui-sketch3d-interaction-mvp8.md`; verification: `docs/gui-shell-acceptance-mvp9r.md`.
Native collection/locks/direct handles, distance/direction input and planar projection remain open.
Ten families remain restored and eight partial; neither this family nor Block 136 is closed.

### Block 136 — Sketch3D viewport collection and point grips — 2026-10-23

The Sketch3D task adds projected active-candidate point/line collection and opt-in XYZ point grips
with axis/plane restrictions and absolute numeric refinement. Gesture candidates retain full
precision and complete downstream previews; Escape, invalid numeric release, camera/focus/size
changes and task/document boundaries roll back or discard them without live history. Parameter
bindings require explicit consent. Focused tests and the extended actual-input Sweep workflow
cover these contracts. See `docs/gui-sketch3d-interaction-mvp8.md` for exact bounds and
`docs/gui-shell-acceptance-mvp9r.md` for verification. Ten families remain restored and eight partial;
Block 136 remains in progress.

### Block 136 — Extended cross-family GUI workflows — 2026-10-23

Three additional actual-input shell workflows combine authored solid faces with Stitch/solid
conversion/Boolean, Revolve with circular pattern/rotation/scale and upstream edits, and the imported
seated-Part example with bounded Planar joint coordinates/motion/analysis/exports. They check rejected
previews, corrected candidates, exact multi-step and branching history, canonical reopening and
read-only consumers. Coverage proofs and verification are tracked in
`docs/gui-shell-acceptance-mvp9r.md`; repeatable commands live in `docs/development-setup.md`.
This strengthens the integrated evidence without expanding command contracts or closing remaining
family gaps. The Surface/Boolean chain also guards the corrected shared-task layout after a transform,
keeping NewBody and retention inputs reachable. Block 136 remains in progress.

### Block 136 — Anchored Sketch3D pointer placement — 2026-10-23

The existing Sketch3D task now stages new points through viewport hover/press/final release, with
an explicit origin/local-point anchor and all seven direction restrictions. XYZ uses a camera-facing
plane through the anchor; model-axis/plane modes fail closed when the view cannot resolve them.
Transient markers and capture never publish intent. Pending numeric text, interrupted gestures and
task/document boundaries retain or discard candidates through the existing preview/Apply authority.
New points copy anchor values explicitly while bound anchors retain their bindings and precision.
Six focused input cases and the extended UI-authored chain/PathCurve/Sweep scenario cover both
projections, cleanup, downstream recompute, exact history and persistence. Contract:
`docs/gui-sketch3d-interaction-mvp8.md`; verification: `docs/gui-shell-acceptance-mvp9r.md`.
Ten families remain restored and eight partial; Block 136 stays in progress.

## Current next technical step — Block 136

The current v3 coverage inventory records all eighteen accepted families, current shell proof sources,
reachable entry points and explicit remaining gaps. The machine check distinguishes inventory validity
from closure readiness; historical controller proofs cannot close a shell gap. Acceptance contract:
`docs/gui-shell-acceptance-mvp9r.md`, `[integration][gui-feature-coverage]` / `[gui][shell-coverage]`.

Continue Sketch3D with explicit distance/azimuth/elevation input relative to the placement anchor,
then planar-point projection with explicit source-association semantics. Anchored pointer placement,
active-candidate point/line collection and direct point grips with axis/plane restrictions now extend
typed XYZ point editing and shared-point line authoring.
Explicit constraint-diagnostic suggestion controls cover the accepted bounded Core diagnostic/repair
subset.
The bounded repair-open path now supports failed-recompute Parts and missing face/point/line sources,
with typed intent, complete atomic publication and failed-state history. Other structural dependency
failures remain strict loading errors.
The construction task now has fourteen bounded create/edit modes, native reference collection and
position/direction/frame/new-offset grips. Continue with parameter/Hole/extent editors, workspace
navigation/context, advanced Assembly controls and integrated native Part/Assembly acceptance in the
order recorded in
`docs/gui-shell-acceptance-mvp9r.md`. Block 136 remains in progress until all recorded gaps and pending
workflows are resolved and the closure gate plus full/native regressions pass.

## Later phases

Interactive Part & Assembly Modeling MVP-9 is complete through Block 131. GUI Shell Reset MVP-9R
runs through Blocks 132–136. STEP Import MVP-10 shifts to Blocks 137–143 (contract unchanged in
`docs/step-import-sequence-mvp10.md`; only the numbering moves).

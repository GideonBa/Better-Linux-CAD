---
doc: MVP Plan
role: >-
  Implementation-sequence source of truth. Feature-specific documents remain
  canonical for exact contracts, formulas, persistence details, failure
  policies, ordering, and focused proofs.
implemented_through: Block 137
current_block: 138
current_boundary: OCCT STEP/XDE reader and deterministic imported body definitions
current_tag: "[geometry][step-import-reader]"
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
  mvp_9r: "GUI Shell Reset — Blocks 132–136 implemented; MVP complete"
  mvp_10: "STEP Import — Block 137 implemented; Blocks 138–143 planned"
---

# MVP Plan

This document is the numbered implementation-sequence source of truth for BLCAD. Exact feature
semantics are intentionally not duplicated here; feature-specific contracts remain authoritative for
mathematics, persistence spellings, migration rules, ordering, and failure policy.

## Current status

```text
implemented through  Block 137
current block        Block 138 (planned)
current phase        STEP Import MVP-10
current boundary     OCCT STEP/XDE reader and deterministic imported body definitions
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
Blocks 132–136 replace the Qt shell and restore all eighteen bounded feature families with current
ShellWindow proofs. Block 136 is accepted on 2026-10-25; its full regression, native interaction,
visual and closure evidence is recorded in `docs/gui-shell-acceptance-mvp9r.md`. Dated implementation
entries below retain the acceptance state at the time of each slice.

Block 137 is complete on 2026-10-26: persistent STEP source identity, explicit modes, canonical JSON,
freshness and transactional source management are implemented and verified. Its Core/GUI contracts,
complex workflows, full regression and native evidence live in `docs/step-import-sequence-mvp10.md`.
The actual STEP/XDE geometry reader remains the next planned boundary, Block 138.

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
MVP-9R  GUI Shell Reset                                 Blocks 132–136 implemented
MVP-10  STEP Import                         Block 137 implemented; Blocks 138–143 planned
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
focused proofs are recorded in the respective contracts. Block 136 completes replacement-shell
feature coverage, with current evidence in `docs/gui-shell-acceptance-mvp9r.md`.

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
- Block 136 — Feature full coverage — Implemented and accepted: restored feature commands (Fillet,
  Chamfer, Shell, Draft, Pattern, Mirror, Boolean, Transform, Sweep, Loft, Surface, Measure, Assembly), feature edit via
  browser double-click, construction/reference repair, Sketch3D, parameters, navigation/filter/repeat
  and selection contexts, advanced Assembly and analysis, and integrated acceptance coverage.

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

### Block 136 — Sketch3D distance/direction input — 2026-10-23

The spatial task now stages new explicit points from a complete distance/azimuth/elevation tuple
relative to the origin or a stable local anchor. It reuses the Core resolver, copies precise bound
anchor values without detaching them, and keeps explicit model-direction input independent of
pointer locks. Incomplete, non-finite and out-of-range inputs remain correctable; pending tuples
block competing authoring and whole-model Preview/Apply until staged or cleared. Editing a tuple
rolls back live gestures. Five focused input cases and the combined UI-authored chain/PathCurve/Sweep
workflow cover rejection/recovery, precise anchors, history, session expiry, reopening and STEP.
Contract and verification: `docs/gui-sketch3d-interaction-mvp8.md`,
`docs/gui-shell-acceptance-mvp9r.md`. Ten families remain restored and eight partial; Block 136 stays open.

### Block 136 — Explicit planar-point copying into Sketch3D — 2026-10-23

The spatial task now collects supported planar endpoints/projected points by stable Sketch/entity/
target identity, resolves them through Geometry's current workplane/reference cache and stages
explicit XYZ copies after per-point consent. Local source receipts make the independent-copy
semantics reviewable; committed values retain no live source dependency. Pending numeric input,
gesture rollback and session expiry preserve complete Preview/Apply authority. Six focused cases
cover oriented/cached/projected sources and independence/history; a thirteenth combined UI workflow
authors the planar sources, spatial chain and Sweep through rejection/recovery, history, reopen and
STEP. Contracts/evidence: `docs/gui-sketch3d-interaction-mvp8.md`,
`docs/gui-shell-acceptance-mvp9r.md`. Eleven bounded families are restored and seven remain partial.
Block 136 stays open, with the numbered completion boundary unchanged at Block 135.

### Block 136 — Standalone Part parameter management — 2026-10-23

The Model ribbon now opens a standalone parameter catalog with stable identities, types, evaluated
values/formulas and direct consumer IDs. Users create Length/Count/Angle literals and Length/Count
expressions and stage existing value/formula edits locally. Complete downstream Preview precedes
one atomic Apply; invalid text, cycles and geometry remain correctable without publishing intent.
Task/history/save/document boundaries discard local drafts. Formula loading now resolves later
authored inputs and restores catalog order, retaining canonical history and persistence after rebinds.
The fourteenth combined UI workflow creates a Sketch/Extrude and typed/formula parameters, rebinds
the feature, corrects rejected formula/numeric edits, and checks exact history, reopening and STEP.
Contracts: `docs/parameter-model.md`, `docs/parameter-expression-mvp.md`; evidence:
`docs/gui-shell-acceptance-mvp9r.md`. Twelve bounded families are restored and six remain partial.
Block 136 stays open and numbered completion remains at Block 135.

### Block 136 — Dedicated through-all hole authoring — 2026-10-24

The replacement shell now creates single cylindrical holes and complete bolt circles through a
dedicated task. Explicit Body/workplane/placement inputs, new typed values and existing literal/
formula bindings stay local until complete downstream Preview and one atomic Apply. Existing
modifiers remain in the Body chain. Invalid numeric, bounded-face, no-op and erased-solid candidates
remain correctable; history, sidecars, persistence and session boundaries retain their authority.
Seven focused shell cases and a fifteenth combined actual-input workflow cover oriented planes,
bindings, a finished/parameter-driven plate, rejection/recovery, exact history, reopening and STEP.
Contract: `docs/gui-part-foundation-workbench-mvp7.md`; evidence:
`docs/gui-shell-acceptance-mvp9r.md`. Advanced extent editors remain a gap in the same family.
Twelve bounded families remain restored and six partial; Block 136 and the Block-135 numbered
completion boundary are unchanged. The semantic Hole Wizard remains separate target architecture.

### Block 136 — Advanced Extrude extent authoring — 2026-10-24

The existing Extrusion task now exposes all seven termination modes, four Body operations, explicit
semantic source-plane limits, reversed direction, a candidate-only second distance and optional
taper. Used inputs and the feature publish in one transaction; switching modes leaves no unused
parameters or Bodies. Focused GUI cases cover oriented profiles, Body results, rejected bounds/text,
taper handle rollback, canonical persistence and global identity collisions. A sixteenth combined
actual-input workflow machines an offset profile with two-sided, to-face and between cuts, corrects
invalid bounds, checks exact history, cancels a tapered candidate, reopens and exports STEP.
The workflow also exposed and fixed target resolution that could select an older face-source Body
instead of the latest modifier. A focused Geometry regression proves both face modes preserve
previous cuts through full and isolated repeated recomputes.
Strong inward taper now rejects collapsed/reversed offset edges before OCCT can loft a large
inverted contour; a second Geometry regression and the GUI taper rollback case cover this boundary.
Contract: `docs/gui-interactive-extrude-revolve-mvp9.md`; evidence:
`docs/gui-shell-acceptance-mvp9r.md`. Thin/open-profile and path Extrude, expanded Revolve controls
and native limit collection remain in the same partial family. Twelve bounded families remain
restored and six partial. Block 136 and the Block-135 numbered completion boundary are unchanged.

### Block 136 — Expanded Revolve controls — 2026-10-24

The existing Drehung task now authors Full, directed Angle and total-angle Symmetric extents, four
Body operations and compatible typed axes. Explicit/derived DatumAxis, ConstructionLine, eligible
circular-cut primary axes and generated linear edges use the same transformed Geometry frame as
execution. Initial invalid axis/profile combinations stay correctable; only NewBody provisions a
Body and angles remain literals. Focused shell regressions and a seventeenth combined actual-input
workflow cover signed bounds, axis identities, mixed-feature allocation, directed/symmetric/full
grooves, downstream edits, exact history, canonical reopening and STEP. Contracts and evidence:
`docs/gui-interactive-extrude-revolve-mvp9.md`, `docs/gui-shell-acceptance-mvp9r.md`.
Shared ordered target resolution preserves earlier cuts when a generated axis references an older
producer. Core updates preserve the original predecessor and terminal producer when editing an
in-place chain. Extrude/Revolve ID allocation also reserves existing mixed-family Body identities.
Thin/open-profile and path Extrude and viewport limit-face collection remain in the same partial
family. Before a future shell Delete action, separately audit terminal-modifier removal and
restoration of the preceding Body producer. Twelve bounded families remain restored and six partial.
Block 136 and the Block-135 numbered completion boundary are unchanged.

### Block 136 — Path-directed Extrude authoring — 2026-10-24

The existing Extrusion task now selects an open PathCurve and all four Body operations, retaining
Extrude identity and the path's orientation. Mode changes preserve local straight-field text,
restore straight intent on return and publish no unused distance parameters or scalar handles in
path mode. Candidate geometry and modifying-Body replacement use the complete disposable recompute;
Apply remains one history transaction. Focused tests and an eighteenth combined actual-input
workflow cover oriented operations, a line/spline/reversed-arc route, prior-cut preservation,
upstream pocket/path edits, rejection/recovery, exact history, reopening and STEP. Contracts and
verification evidence: `docs/gui-interactive-extrude-revolve-mvp9.md`,
`docs/gui-shell-acceptance-mvp9r.md`. Thin/open-profile Extrude and native limit-face collection remain
in the same partial family. Twelve bounded families remain restored and six partial; Block 136 and
the Block-135 numbered completion boundary are unchanged.

### Block 136 — Open-line Thin Extrude authoring — 2026-10-24

The existing Extrusion task now consumes the retained single-open-line thin adapter with OneSided,
MidPlane and TwoSided walls, candidate-only thickness parameters and a Sketch-plane thickness
handle. Finish Sketch and browser/viewport semantic selection share the bounded input predicate;
no closed profile is invented. The seven straight terminations, taper and four Body operations
retain atomic whole-model Preview/Apply, sidecars, history and persistence. Focused shell cases and
a nineteenth combined actual-input workflow cover oriented wall bounds, operations/extents,
invalid text, handle rollback, unused-input suppression, downstream thickness edits, exact history,
reopening and STEP. Contracts/evidence: `docs/gui-interactive-extrude-revolve-mvp9.md`,
`docs/gui-shell-acceptance-mvp9r.md`. Native viewport limit-face collection remains in this partial
family. General curved/open chains remain outside the retained Geometry boundary. Twelve bounded
families remain restored and six partial; Block 136 and numbered completion through 135 are unchanged.
Native verification also exposed integer projection rounding that could merge distinct handle
directions. The viewport now preserves OCCT's subpixel camera projection; a separate red/green
regression covers orthographic/perspective precision alongside the native Thin pointer workflow.

### Block 136 — Native Extrude limit-face collection — 2026-10-24

The Extrude task now collects first/second semantic source-plane roles directly in the native
viewport, with an explicit role selector, synchronized face combos/highlights, translucent source
presentations and exact Face-only semantic scope. Invalid candidates remain correctable without
stale result preview or Core/history writes. Ending collection, mode changes and command lifecycle
restore the previous selection context and remove temporary source presentations. Both closed-profile
and single-line Thin face extents use the same bounded source API; arbitrary final-Body topology is
not added.

Five focused GUI scenarios and the twentieth combined workflow cover origin-plane orientations,
invalid reference recovery, scope/highlight restoration, lifecycle, native clicks and a fully
UI-authored pocket/ToFace-cut/Between-Thin-wall model. Its changed source depth, earlier-feature edit,
five-step exact history, canonical reopening and STEP export exercise downstream references.
Verification evidence lives in `docs/gui-shell-acceptance-mvp9r.md`. The bounded Extrude/Revolve/Hole
family is restored; thirteen families are restored and five remain partial. Block 136 remains open
and the numbered completion boundary stays at Block 135.

### Block 136 — ViewCube, Home and named camera bookmarks — 2026-10-24

The replacement shell now owns seven standard views, a ViewCube menu, explicit Home capture/restore
and named camera save/replace/restore/remove controls through the retained transient workspace API.
Names are trimmed; snapshots preserve complete camera values and remain outside model history and
persistence. Sketch focus and viewport grip guards prevent competing camera commands; previews keep
their task/reference/text state across navigation. Successful document replacement clears the
catalog, while Save/history and failed or cancelled replacement preserve it.

The offscreen viewport now applies OCCT's standard orientation frames instead of discarding the
request, and projection changes notify consumers on both backends. Five focused cases and the
twenty-first combined Qt-input workflow exercise camera/projection/plane restoration, preview and
Sketch guards, real extent drag/Escape, source-depth propagation, three-step history, canonical
reopening and STEP. Verification is recorded in `docs/gui-shell-acceptance-mvp9r.md`.

Contextual commands, visible filters and repeat presentation remain in the partial viewport family;
thirteen families remain restored and five partial. Block 136 stays open and completion remains
through Block 135.

### Block 136 — Visible model selection filters — 2026-10-24

The replacement shell now exposes all eight frozen filters in an always-visible status-bar combo.
The session and viewport share the selected model mask; changing it removes incompatible selection
and browser-only profile fallbacks. Hidden datums/profiles cannot enable Create Sketch/Extrude.
Active tasks, Sketch focus and interaction capture guard the control. Task-owned reference masks
remain authoritative, including Edge collection for finishing and Datum collection for holes under
a Body model filter. Apply/Cancel/history restore the chosen model filter. Save/history preserve
this transient choice; successful document replacement resets it to All.

Six focused scenarios and the twenty-second combined Qt-input workflow cover filter semantics,
reference collection, invalid-input recovery, upstream edits, history, canonical reopening and STEP.
Verification is recorded in `docs/gui-shell-acceptance-mvp9r.md`. Contextual commands and repeat
presentation remain open in the viewport family. Thirteen families remain restored and five partial;
Block 136 stays open and numbered completion remains through Block 135.

### Block 136 — Repeat the last accepted modeling command — 2026-10-24

The shell now owns a visible **Erneut: …** action and Ctrl+R window shortcut. Feature, Hole
and Assembly task owners report the original command action only after successful Apply. Repeat
starts that owner's ordinary fresh task with current compatible inputs and normal defaults; no old
selection, parameters, path options or drafts are replayed. Failed/cancelled tasks do not replace the
record. Task/Sketch/repair/capture guards and owner enablement prevent competing or invalid starts.
Save/history and failed/cancelled replacement retain the transient record; successful document
replacement clears it. Redo is separately labelled Wiederherstellen.

Six focused scenarios and the twenty-third combined Qt-input workflow cover fresh defaults, changed
profiles, invalid input, Assembly insertion, Ctrl+R/Escape, upstream edits, exact history, canonical
reopening and STEP. Verification lives in `docs/gui-shell-acceptance-mvp9r.md`. Contextual commands
remain open in the viewport family; thirteen families remain restored and five partial. Block 136
stays open and numbered completion remains through Block 135.

### Block 136 — Bounded profile context toolbar — 2026-10-24

The shell now owns a viewport mini-toolbar for verified materialized profiles and single open lines.
Extrude/Revolve use the normal owner's enabled commands; open lines offer only Thin-capable Extrude.
Context start consumes selection after the real task begins. Context/task Cancel and Escape restore
complete preselection plus its verified typed source. Apply uses the existing atomic commit and repeat
record; no context state is persisted. Sketch/repair/capture/unrelated-task guards protect interaction,
and the toolbar clamps near the last viewport click without polling or command discovery.

Six focused cases and the twenty-fourth combined Qt-input workflow cover Core/filter rejection,
consumption/restoration, invalid previews, single-line boundaries, history and document replacement.
The combined Part authors profile-based Cut and Thin features, changes upstream depth, edits an
earlier feature, checks five-step history, canonical reopening and STEP. Verification is recorded in
`docs/gui-shell-acceptance-mvp9r.md`. Remaining profile, Body, Surface and Assembly context commands
stay open. Thirteen families remain restored and five partial; Block 136 stays open and numbered
completion remains through Block 135.

### Block 136 — Sweep and Loft profile contexts — 2026-10-24

The profile mini-toolbar now includes Sweep and Loft after Extrude/Revolve. Original owner actions
start the tasks; a typed Sketch/Profile handoff selects the exact Sweep source or seeds Loft's first
section, independently of chooser order. Normal remaining-input eligibility and Geometry validation
still apply. A single open line retains only Thin-capable Extrude. All cancellation routes restore
complete preselection after the owner restores its filter/scope; Apply uses the existing transaction.
Dynamic task bounds and a shared ordering/hint group keep the Loft controls complete and separated
after switching from Sweep.

Five focused cases and the twenty-fifth combined Qt-input workflow cover nondefault source identity,
missing inputs, raw-region exclusion, duplicate rejection, three-section ordering and G2 recovery, independent Sweep,
parameter/earlier-feature edits, four-step exact history, canonical reopening and STEP. Native proof
and full regression evidence are recorded in `docs/gui-shell-acceptance-mvp9r.md`. Continue with
finishing, pattern, Body, Surface and Assembly context commands. Thirteen families remain restored and
five partial; Block 136 stays open and numbered completion remains through Block 135.

### Block 136 — Stable Edge/Face finishing contexts — 2026-10-24

The shared `ShellContextTools` toolbar now offers Fillet/Chamfer and Shell/Draft for a single verified
stable Edge/Face under the explicit model filter. The current Geometry builder resolves typed Body
and semantic target identity. The original command starts and seeds that exact Body/target without
adding an authoring authority. Its task owns the current Body's scope; Apply/Cancel restore the prior
filter and scope, and all cancellation paths restore complete still-valid preselection. Idle target
presentation responds to filter/task changes without a Core revision. Unsupported or lost roles,
multiple selections, stale/hidden Bodies and unrelated interaction owners fail closed. The native
combined test also exposed clipped mode controls after command switches; preferred form row heights
now remain scrollable, including the secondary Chamfer value.

Five focused cases and the twenty-sixth combined Qt workflow cover all four commands, a nondefault
Body, consumption/restoration, atomic Apply, exact history, Save and document replacement. The native
workflow authors two Bodies, cancels Draft and a two-distance Chamfer, applies Shell/Fillet/angular
Chamfer, batches two upstream edits, renames an earlier feature, checks five-step exact Undo/Redo,
reopens canonically and exports STEP. It explicitly skips offscreen because idle subshape source
selection requires native OCCT hits. Contracts and verification live in
`docs/gui-modeling-workspace-mvp9.md`, `docs/gui-interactive-finishing-mvp9.md` and
`docs/gui-shell-acceptance-mvp9r.md`. Continue with pattern, Body, Surface and Assembly contexts.
Thirteen families remain restored and five partial; Block 136 stays open and numbered completion
remains through Block 135.

### Block 136 — Body pattern and Mirror contexts — 2026-10-24

A single verified current solid Body now offers Linear pattern, Circular pattern and Mirror through
the shared toolbar. Typed source handoff seeds the exact Body through the original task, including
curved solids, independently of chooser order or finishing topology roles. Normal preview and atomic
Apply authority remain unchanged. The task owns its offered Body/reference scope and restores the
prior scope/filter on Apply and Cancel; all context cancellation routes restore complete still-valid
preselection. Browser Body/reference picks reach the same owner as viewport picks; the combined
bounds checks exposed and now cover this previously missing browser handoff. Wrapped guidance stays
outside dynamic form rows to preserve controls across tool switches. Multiple, hidden, stale, missing
and surface sources fail closed.

Five focused cases and the twenty-seventh combined Qt workflow cover source identity, scope/reference
transitions, cancellation, atomic history, Save and replacement. The combined Part chains Mirror,
Circular pattern and Linear pattern, changes source depth/count/extent together, edits the earlier
Mirror, checks five-step exact history, reopens canonically and exports STEP. Native picking verifies
the actual OCCT Body; offscreen uses the browser. Contracts and validation live in
`docs/gui-modeling-workspace-mvp9.md`, `docs/gui-interactive-pattern-body-mvp9.md` and
`docs/gui-shell-acceptance-mvp9r.md`. Continue with Body Boolean/Transform, Surface and Assembly
contexts, then advanced Assembly and integrated acceptance. Thirteen families remain restored and
five partial; Block 136 stays open and numbered completion remains through Block 135.

### Block 136 — Body Boolean and Transform contexts — 2026-10-25

The shared context toolbar now offers Boolean, Translate, Rotate and Scale for one verified current
Body. Typed handoff assigns the exact Boolean target with no arbitrary tools, or the sole initial
Transform source. Surface Bodies support transforms; Boolean retains its solid-only boundary and
second-solid prerequisite. The original owner keeps preview, Apply and repeat authority. Body tasks
own their offered source/tool/target/axis scope, restore the previous scope/filter on completion and
restore complete valid context selection on cancellation. Browser collection reaches the same owner.
The new combined test also exposed family-grouped loading that moved an earlier translation after a
Boolean cut during the next transaction. Optional validated `feature_order` preserves interleaved
executable history without storing derived shapes; Core/Geometry tests cover graph identity, corrupt
metadata, legacy loading, bounds/volumes and incremental replay. Contract: `docs/file-format.md`.

Five focused cases and the twenty-eighth combined Qt workflow cover exact roles, surface sources,
reference transitions, invalid input, cancellation, atomic history, Save and replacement. The combined
Part cancels multi-Body translation, moves the nondefault target into overlap, switches Boolean
targets, cuts with retained tools, rotates and scales with explicit references/center, checks exact
four-step Undo/Redo, reopens canonically and exports STEP. Verification is recorded in
`docs/gui-shell-acceptance-mvp9r.md`. Continue with Surface and Assembly contexts, advanced Assembly
and integrated acceptance. Thirteen families remain restored and five partial; Block 136 stays open
and numbered completion remains through Block 135.

### Block 136 — Surface contexts and task selection cleanup — 2026-10-25

The shared toolbar now offers all six Surface commands through their original task owners.
Verified PathCurves/stable edges seed Boundary/Fill; Surface Bodies/stable faces seed Trim/Extend
and Stitch; shell-to-solid accepts a Surface Body and keeps Geometry closure diagnostics. Typed
reference keys, exact kind/filter/scope checks and idempotent first-input handoff avoid arbitrary
chooser defaults. Trim/Extend collect the remaining boundary explicitly. Browser picks reach the
same owner, cancellation restores valid preselection and removes temporary subshape presentations.
Wrapped guidance stays outside dynamic rows to preserve controls across command switches.

Six focused cases and the twenty-ninth combined Qt workflow cover all six contexts, stable solid
roles, source/key guards, cancellation, scope/scene cleanup, atomic history, Save and replacement.
The combined Part authors its box, closed paths and trim profile through Qt, chains Fill/Trim/Extend/
Stitch and solid conversion, rejects an open shell, changes upstream depth, checks six-step history,
reopens canonically and exports STEP. Verification and native evidence live in
`docs/gui-shell-acceptance-mvp9r.md`; contracts live in `docs/gui-interactive-surface-mvp9.md` and
`docs/gui-modeling-workspace-mvp9.md`. Continue with Assembly contexts, then advanced Assembly and
integrated acceptance. Thirteen families remain restored and five partial; Block 136 stays open
and numbered completion remains through Block 135.

### Block 136 — Assembly completion implementation — 2026-10-25

The final implementation slice adds root Assembly selection contexts, hierarchy validation,
occurrence-qualified relationship/joint create/edit tasks, connected cross-hierarchy solve/application,
role-addressed joint motion, sampled Revolute analysis and exact occurrence result highlighting.
New focused regressions accompany two realistic integrated Qt workflows covering repeated child
Assemblies, history, rejected candidates, persistence and both Assembly STEP modes.

Nineteen new GUI cases include two integrated Qt workflows and native proofs for rotated hierarchy
target picking and visible occurrence highlighting. A diagnostic-only revision no longer prevents
correcting an invalid analysis interval. Core/Geometry authority and persistence formats are unchanged.
The exact contracts remain in the existing Assembly, modeling-workspace and analysis documents.

### Block 136 — Final replacement-shell acceptance — 2026-10-25

All eighteen accepted feature families have restored bounded surfaces and current shell proofs;
the v3 manifest has no family gaps or pending acceptance workflows. Full Core (530 cases), Geometry
(441), offscreen GUI (516 passed, 19 native-only skips) and canonical native shell (351 passed,
no skips) pass. Native captures, visible framebuffer highlighting and separate application startup
were verified. The explicit `BLCAD_REQUIRE_SHELL_ACCEPTANCE=1` closure gate passes with the completed
plan and manifest. Full counts and commands live in `docs/gui-shell-acceptance-mvp9r.md`.

GUI Shell Reset MVP-9R is complete through Block 136. Existing feature-specific geometry boundaries
remain explicit, including discrete Revolute sampling and shared child-definition pose authority.
Historical Block-131 controller acceptance remains separate from the completed shell evidence.

## Block 137 — STEP source identity and GUI management — Complete (2026-10-26)

Part and Project source registries persist explicit Reference/EditableBody intent, canonical relative
paths and SHA-256 fingerprints with strict, legacy-compatible JSON. Read-only inspection distinguishes
available, missing, changed and unreadable files. Explicit refresh rechecks source intent, document
location and current bytes before an atomic digest update. Save As, definition loading and export
protect source ownership and files.

The shell source task and browser expose that authority. Twenty-one new cases comprise nine Core,
seven headless GUI and five complex shell cases. They combine existing modeling, nested Projects,
exact history, stale-preview recovery, Save/Open, relocation, owner-ID collisions, lifecycle controls
and native dialogs/layout. Full Core (539), Geometry (441), offscreen GUI (527 passed, 20 native-only
skips) and native shell (356 passed, no skips) pass; all four targets, native captures and separate
application startup are verified. The historical Block-136 shell manifest remains unchanged; its
completion gate accepts later numbered progress. Exact evidence is in
`docs/step-import-sequence-mvp10.md`.

## Loft automatic segment compatibility follow-up — 2026-10-26

Completed automatic derived subdivision for unequal Loft edge counts while retaining original
Sketch intent and explicit order/seam/normal authority. Seven new Geometry cases and one complex GUI
case cover the 8/9/6 spline pattern, exact rational/circular contours, original boundary points,
center/guide binding, bounded failure, preview/cancellation, downstream modeling, history and
Save/Open/STEP. A local reproduction of the supplied document also recomputes and exports without
manually subdividing its Sketches.

All five build targets pass. Full Core (539 cases), Geometry (448) and offscreen GUI (528 passed,
20 native-only skips) pass. The affected native Loft/path suite passes 12 cases with no skips;
its focused offscreen counterpart passes 11 cases with one native-only skip. Native captures and
separate application startup are verified. Exact evidence and remaining geometric bounds live in
`docs/gui-interactive-path-sweep-loft-mvp9.md` and the Part Loft Geometry documents. This improves
the existing Loft boundary; Block 138 and later STEP import work remain planned.

## English desktop and modeling usability follow-up — 2026-10-26

User-reported desktop and modeling issues are tracked as a follow-up to the accepted Sketcher and
Modeling surfaces. Numbered progress remains through Block 137; this work does not implement the
Block-138 STEP reader.

Implemented and verified:

- English desktop labels and defaults, square monochrome controls, black viewport background;
- exact nested curved profile holes for selection and extrusion;
- current-result curved edge Fillet/Chamfer references, preserved cross-family execution order and
  simplified, validated Union boundaries with isolated immutable input caches;
- fixed, selectable Sketch origin/axes, connected fitted-spline drag including raw closed seams,
  conservative older-sidecar point migration and per-element visibility;
- Rapid Dimension hover/placement, projected/aligned point distances, arc/full-circle size annotations,
  exact placement history and full-circle diameter parameter authority.

Verification covers focused Core/Geometry checks, complex Qt workflows, full Core/Geometry/
offscreen GUI regression, native xcb captures and separate startup. Current-result edge witnesses
support unchanged recompute and persistence; geometrically changed edges require explicit reselection.
The detailed contracts live in the existing region, finishing, Boolean, topology, browser, modeling
workspace and dimension authoring documents.

| Verification | Result |
| --- | --- |
| GUI preset: application, Core, Geometry, GUI tests and STEP exporter | All five targets build |
| Full Core | 545 cases, 17,037 assertions passed |
| Full Geometry | 452 cases, 31,557 assertions passed |
| Full offscreen GUI | 545 passed, 20 native-only skips; 72,769 assertions passed |
| Shell coverage closure gate | 3 cases, 299 assertions passed |
| Focused native appearance, Rapid Dimension and closed-spline seam | 8 cases, 350 assertions passed |
| Native combined hierarchy after popup synchronization correction | 1 case, 792 assertions passed; offscreen counterpart: 747 assertions |
| Complete native acceptance plus follow-up workflows | 374 cases, 69,278 assertions passed; no skips |
| Separate native application startup | Starts and remains alive until the six-second smoke timeout |

The follow-up adds 17 GUI test cases, including combined authoring, editing, visibility, invalid-input
recovery, history and Save/Open workflows. Appearance checks cover white square frames, solid black
fills, enabled/disabled arrows and opaque white placeholder text. Native popup helpers retain exact
cursor/row/click assertions while allowing a bounded retry after delayed desktop pointer restoration.
Screen-space placement tolerances account for native pixel rounding; committed dimensions must retain
their preview anchor exactly.

Native acceptance combines the canonical shell filter in `docs/gui-shell-acceptance-mvp9r.md`
with Block-137, automatic Loft subdivision and the follow-up tags in `docs/development-setup.md`.
The final production/test source freeze is unchanged throughout the successful native run. Captures
under `/tmp/blcad-ui-modeling-native/` verify the English monochrome ribbon, visible dropdown arrows,
white placeholders, Rapid Dimension labels, connected spline seams, nested material selection and
the curved-body Union preview. The available X11 visual emits OCCT's known Medium depth/stencil
warning; no High-severity, BadDrawable or BadWindow errors occurred. No required check is unavailable.

## Hermes-inspired desktop design follow-up — 2026-10-26

The user requested a documented plan followed by implementation of the Hermes website's aesthetic,
explicitly releasing the previous monochrome/white-frame requirement. The implementation sequence,
visual roles, acceptance criteria and evidence live in `docs/gui-hermes-design.md`.

Implemented the shared electric-blue/paper palette, offline licensed Barlow/Plex typography,
navigation/ribbon hierarchy, task action roles, coordinated canvas readouts and startup screen.
Compact workspace arrows remain visible and clickable at 1024×640. STEP checksum readouts include
styled padding in their height, and native focus/popup checks retain exact input assertions after
bounded event-loop readiness.

Final validation: Core **545 / 17,037**, Geometry **452 / 31,557**, offscreen GUI **566 cases,
546 passed / 20 native-only skips / 73,433 assertions**, canonical native GUI **378 / 70,304**
(no skips), 150% native design/workflow **5 / 771**, and required shell closure **3 / 299**.
Separate native startup stayed running until the intentional timeout (124). The source freeze
remained unchanged across final GUI/native checks. Native images and full evidence live in the
design document. Delivery follows the normal protected-branch PR merge workflow; this follow-up
does not advance numbered STEP import progress.

## Loft profile correspondence follow-up — 2026-10-26

The screenshot's new unsaved document is unavailable, so its exact settings cannot be reproduced.
A deterministic regression nevertheless proves valid but distorted solids from independently drawn
identical profiles: a 2,240 mm³ prism became 746.6667 or 1,642.6667 mm³ after reversing the middle
contour or changing its starting corner.

Implemented default, optional alignment for new closed-section GUI Lofts. Geometry suggests existing
seam/flip references from a bounded, normalized contour comparison; manual reversals stay local and
guide-bound or explicitly aligned intent remains authoritative. Existing saved features and source
Sketches are preserved. Analytic prism/circle/ellipse tests and a complex actual-input GUI workflow
cover intermediate geometry, cancellation, opt-out/recovery, history, Save/Open and STEP.

Final validation: Core **545 / 17,037**, Geometry **453 / 33,119**, full offscreen GUI **570 cases,
550 passed / 20 native-only skips / 74,758 assertions**, focused native Loft/path UI **17 / 4,830**
(no skips), and required shell closure **3 / 299**. All five targets build; separate native startup
stayed running until the intentional timeout (124), and the 737-file source freeze remained
unchanged. Native captures, focused results, heuristic limits and unrelated native-only omissions
are recorded in `docs/gui-interactive-path-sweep-loft-mvp9.md`. Numbered status remains through
Block 137, with Block 138 planned.

## Modeling usability issue triage — 2026-10-26

The user requested investigation and GitHub issues for later implementation. Review at main
`5e37af328d47ab7f2b5254114dc381fd98b70e35` produced the following independent backlog items.
Each issue includes pinned source evidence, reproduction or explicit evidence limits, proposed
acceptance criteria and realistic native GUI workflows. Existing features are distinguished from
missing shell/persistence integration; these tickets do not claim that every reported path is absent.

| Area | Issue | Evidence / current status | Suggested priority |
| --- | --- | --- | --- |
| Sketch endpoint snaps | [#104](https://github.com/GideonBa/Better-Linux-CAD/issues/104) | Confirmed persistence gap for loop closure / restarted Line | P2 |
| Coincident → closed region | [#105](https://github.com/GideonBa/Better-Linux-CAD/issues/105) | Exact Coincident equality groups implemented; cross-layer/native regression in `gui-sketch-regions-finish-mvp8.md` | P1 |
| Dimension line layout | [#106](https://github.com/GideonBa/Better-Linux-CAD/issues/106) | Confirmed family-specific rendering gaps; DIN/ISO target recorded | P2 |
| Dimension double-click editing | [#107](https://github.com/GideonBa/Better-Linux-CAD/issues/107) | Confirmed paint/pick mismatch; exact reported gesture pending | P1 |
| Dimension deletion | [#108](https://github.com/GideonBa/Better-Linux-CAD/issues/108) | Existing backend; visible-selection/keyboard route needs reproduction | P1 |
| Construction conversion | [#109](https://github.com/GideonBa/Better-Linux-CAD/issues/109) | Missing shell toggle and curved-flag compatibility persistence | P2 |
| Sketch-line rotation axes | [#110](https://github.com/GideonBa/Better-Linux-CAD/issues/110) | Missing persistent sketch/entity axis source | P2 |
| Body Rotate axis parity | [#111](https://github.com/GideonBa/Better-Linux-CAD/issues/111) | Shell omits existing derived-datum/semantic-edge support | P2 |
| Sketch on planar result face | [#112](https://github.com/GideonBa/Better-Linux-CAD/issues/112) | Direct workflow and general associative support identity missing | P2 |
| Cut feedback and workflow | [#113](https://github.com/GideonBa/Better-Linux-CAD/issues/113) | Implemented: distinct Cut outcomes, correctable diagnostics and exact curved extents; native regression verified | P1 |
| Complex-Part responsiveness | [#114](https://github.com/GideonBa/Better-Linux-CAD/issues/114) | Implemented for review: cached previews/Surface validation, single-sampled witnesses and deep-rotor native profiling; residual costs and local target misses documented | P1 |

The independent Coincident probe commits the real GUI constraint intent, then checks Geometry:
a 100×50 mm outline with a 0.001 mm initial gap retains a 3.85787585e-8 mm gap after an accepted
solve and has no closed region. The strict Geometry Finish API rejects it; the shell may leave the
editor without creating a usable profile. The zero-gap control yields one region. A separate Cut
probe accepts a disjoint tool with unchanged 1,000 mm³ target volume; the overlapping control
correctly removes 40 mm³.

The isolated Debug backend preview probe uses 1/25/100/500 existing rectangular extrusion features
and three samples per size. Median new-Extrude preview costs are 0.475/5.606/30.984/408.725 ms.
Each candidate starts with an empty ShapeCache and recomputes the complete document; committed edits
also perform whole-document JSON/history work, while changed scene publication recreates AIS
presentations. These are measured execution costs and investigation leads, not a profile of the
user's exact model or an end-to-end native event latency guarantee.

Existing focused checks passed: Sketch Core **17 / 392**, region Geometry **4 / 101**,
axis/workplane Core **20 / 931**, axis/workplane Geometry **35 / 1,450**, Cut/recompute Geometry
**32 / 2,144**, and targeted native Sketch/dimension/Revolve/Body/Cut/performance GUI
**38 / 5,213** (no skips). These passing tests cover the supported paths and expose the need for the
new end-to-end regressions specified in the issues. No implementation source changed, so full
regression suites were not repeated for this documentation-only delivery.

Local evidence: `/tmp/blcad-issue-triage-probes.cpp`, `/tmp/blcad-issue-triage-probes.log`,
`/tmp/blcad-issue-triage-probes-large.log`, `/tmp/blcad-issue-triage-cut-geometry.log` and
`/tmp/blcad-issue-triage-native.log`; supporting scope logs use
`/tmp/blcad-issue-triage-sketch-` and `/tmp/blcad-triage-axes-planes-`.
The suggested P1 queue first restores profile closure and dimension edit/delete usability, then
clarifies ineffective Cuts and profiles complex-Part interaction. Numbered status remains through
Block 137; Block 138 and subsequent STEP topology/import work remain planned.

The 2026-10-28 issue follow-up implements exact Coincident equality groups for #105. Its cross-layer,
mixed-curve, native dimension/drag/Extrude and history/persistence proofs are recorded in
`gui-sketch-regions-finish-mvp8.md`; the numeric contract is in
`sketch-planar-constraint-solver-mvp8.md`. This maintenance fix advances no numbered block.

## Current next technical step — Block 138 (planned)

Implement the OCCT STEP/XDE reader and deterministic product/body definitions over the verified
external-source contract. Normalize declared units to millimeters, preserve supported names and
hierarchy diagnostics, and reject empty/invalid transfer or ambiguous units before mutation. Add real
STEP fixtures and GUI inspection previews with product/body/unit diagnostics. Measure inspection cost
on realistic assets while keeping explicit source revalidation. Imported topology, Reference Parts,
EditableBody features and structured Assembly import remain Blocks 139–143.

## Later phases

Interactive Part & Assembly Modeling MVP-9 is complete through Block 131. GUI Shell Reset MVP-9R
is complete through Block 136. STEP Import MVP-10 has completed source management in Block 137;
Blocks 138–143 remain planned in `docs/step-import-sequence-mvp10.md`.


Issue #113 follow-up (2026-10-28) rejects ineffective/full-target Cuts with distinct correction
messages, guards aggregate and legacy paths without mutating cached sources, and supports exact
circle/arc/spline contours through resolved extrusion extents. Native workflows prove wrong-target
and outward-direction recovery after Fillet, independent material/volume outcomes, transactional
failure, associative tool edits, history, reopening and STEP. Full Core **545 / 17,037**, Geometry
**459 / 35,243**, offscreen GUI **552 passed / 20 native-only skips / 76,150 assertions**; expanded
native **20 / 7,596**, native 150% **3 / 1,918**, all five targets built and separate startup reached
intentional timeout 124. Exact contracts/evidence live in `docs/part-body-boolean-geometry-mvp6.md`
and `docs/gui-interactive-extrude-revolve-mvp9.md`. Numbered implementation status is unchanged.


Issue #114 preview/selection follow-up (2026-10-28) uses the user's clarified workload: one
revolved rotor with repeated curved blades. Fresh, complete candidate caches retain clean producers
and execute explicit changed/new roots plus all dependent Body consumers. Result-edge choices sample
each edge once while retaining the existing full witness/ambiguity checks. Native Debug warm
20-blade preview p50/p95 changes **1,531.2 / 1,582.9 ms → 61.9 / 69.0 ms**; 40 blades changes
**5,017.4 / 5,211.7 ms → 101.9 / 117.2 ms**. Raw before/after samples, environment, diagnostic
limits and exact contracts live in `docs/gui-modeling-workspace-mvp9.md` and `docs/benchmarks/`.

Validation: Core **546 / 17,061**, Geometry **459 / 35,243**, offscreen GUI **554 passed /
20 native-only skips / 76,545 assertions**, expanded native **25 / 9,772**, 150% rotor and
supplementary controller checks **9 / 828**, final native profile **1 / 870**. All five targets build;
separate startup reaches intentional timeout 124. At this initial stage, full history/edit costs and
broader input/solver/drag/picking profiling remained outside the proof. No numbered implementation
block was advanced by this bounded follow-up.


Issue #114 interaction acceptance (2026-10-29) adds 5/20/40-blade connected rotors with 4/12/24
serial transforms (7/15/27 producers), real driving-dimension edits and curved Sketch drag sequences.
The isolated native profile now separates solver, dimension authoring, coalesced drag, actual mouse
picking, complete preview publication/events/capture, and Apply/Undo/Redo including history costs.
Surface metadata validation reuses the same affected-root cache path as shape preview.

Deep 20/40-blade preview p95 is **66.1 / 131.7 ms**; publication/events/capture included is
**119.2 / 199.3 ms**. Full Apply/Undo/Redo still costs about **5–6 s** at 40 blades. Local targets
and two recorded misses (20-blade drag **17.8 ms** versus 16 ms; 40-blade JSON **24.0 ms** versus
20 ms) are explicit review evidence, not user-approved release guarantees. Measurements prioritize
commit/history next; renderer changes and workers are not introduced by this fix.

Focused offscreen proof passes **3 cases / 620 assertions**, the isolated five-sample native profile
**1 / 1,349**, and final native proof including both large rotor workflows, Surface/Stitch,
viewport lifecycle and required shell coverage **8 / 1,858**, without skips. All five targets build;
a separate startup exits at intentional timeout **124**. Source hub/blade volume checks and remote
Fillet-removal invariance replace an unsupported exact aggregate-scaling oracle, with the final
volume tolerance tightened and production tolerances unchanged. The final **741** source/test/
asset/build-definition fingerprints remain unchanged after native verification. Scope, raw data,
budget review and residual performance limits are documented in the existing workspace/development
docs. The implementation is ready for PR review; the issue remains open until review/merge.
No numbered implementation block is advanced.

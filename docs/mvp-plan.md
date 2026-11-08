---
doc: MVP Plan
role: >-
  Implementation-sequence source of truth. Feature-specific documents remain
  canonical for exact contracts, formulas, persistence details, failure
  policies, ordering, and focused proofs.
implemented_through: Block 150
current_block: 151
current_boundary: Block 151C Physical Properties task verified; combined acceptance remains
current_tag: "[parity-b151]"
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
  mvp_10: "STEP Import — Blocks 137–143 implemented; MVP complete"
  gui_workflow_revision: "Inventor-inspired desktop workflows — Block 144 implemented and accepted"
  advanced_modeling_workflows: "Blocks 145–150 accepted; Block 151 in progress; Blocks 152–204 planned"
---

# MVP Plan

This document is the numbered implementation-sequence source of truth for BLCAD. Exact feature
semantics are intentionally not duplicated here; feature-specific contracts remain authoritative for
mathematics, persistence spellings, migration rules, ordering, and failure policy.

## Current status

```text
implemented through  Block 150
current block        Block 151 (in progress; slice C)
current phase        Materials and physical properties UI
current boundary     Inventor-like assignment and physical-property tasks
```

The 2026-11-04 GUI request is complete through Block 144. The 2026-11-06 request assigns a
detailed implementation and combined-test plan for the identified remaining capabilities inside
existing workspaces. [The consolidated sequence](inventor-parity-sequence.md) allocates Blocks
145–204; [the test plan](inventor-parity-test-plan.md) and the planned workflow section of
`user-interface.md` define evidence and Inventor-like interaction. All new blocks are planned and
not started at that planning delivery. The subsequent implementation request begins Block 145
under the serial slice plan below. Previously implemented contracts remain in force until extended.

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
Block 138 is complete on 2026-10-29: verified immutable STEP bytes feed the deterministic XDE
reader and read-only product/body/unit inspection. Its full regression, native proofs and realistic
rotor measurements are recorded in the same sequence and `docs/development-setup.md`.
Block 139 is complete on 2026-10-29: persisted imported topology catalogs, strict recovery, real
Part/Assembly target consumers and native semantic inspection are accepted. Block 140 is complete
on 2026-10-29: immutable Reference Parts support shared/nested Assembly occurrences, verified
constraints and motion, analysis/export, explicit shared-source refresh and conversion to a new
Editable Part. Block 141 is complete on 2026-10-29: direct EditableBody creation, immutable ordered
base outputs, incremental downstream history, imported face/edge authoring and strict full modeled
refresh are accepted. Block 142 is complete on 2026-10-29: structured STEP import preserves shared
definitions, exact direct occurrences and supported names beneath an explicit synthetic root, using
the existing Reference/EditableBody and Project JSON contracts. PR #134 is merged and main verified.
Block 143 is complete on 2026-10-29: continuous shared-command and native GUI workflows prove
import, modeling, relationships, motion, analysis, save/open, explicit refresh and physical STEP
re-export, including atomic failures, source immutability and portable semantic JSON. STEP Import
MVP-10 is complete through Block 143.

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
MVP-10  STEP Import            Blocks 137–143 implemented; MVP complete
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
| Sketch endpoint snaps | [#104](https://github.com/GideonBa/Better-Linux-CAD/issues/104) | Implemented: stable picked endpoint identity; automatic Coincident geometry/history transaction; native loop/restart/overlap/12-segment and persistence/drag proof | P2 |
| Coincident → closed region | [#105](https://github.com/GideonBa/Better-Linux-CAD/issues/105) | Exact Coincident equality groups implemented; cross-layer/native regression in `gui-sketch-regions-finish-mvp8.md` | P1 |
| Dimension line layout | [#106](https://github.com/GideonBa/Better-Linux-CAD/issues/106) | Implemented: explicit family geometry and projected strokes/terminators, shared paint/picking/editor bounds, native normal/oblique neighboring-dimension and history/persistence proof; delivered in PR #129 | P2 |
| Dimension double-click editing | [#107](https://github.com/GideonBa/Better-Linux-CAD/issues/107) | Implemented: shared badge rectangle and independent double-click resolution; native proof | P1 |
| Dimension deletion | [#108](https://github.com/GideonBa/Better-Linux-CAD/issues/108) | Implemented: visible selection, multi-selection enablement and real Delete/Backspace proof | P1 |
| Construction conversion | [#109](https://github.com/GideonBa/Better-Linux-CAD/issues/109) | Implemented: reversible typed curve/group conversion, lossless curved roles, region exclusion and native modeling/history/persistence proof | P2 |
| Sketch-line rotation axes | [#110](https://github.com/GideonBa/Better-Linux-CAD/issues/110) | Implemented: scoped Sketch-line persistence, current transformed-plane resolution, associative source edits and native modeling/history/persistence proof | P2 |
| Body Rotate axis parity | [#111](https://github.com/GideonBa/Better-Linux-CAD/issues/111) | Implemented: typed derived datum and supported semantic-edge choices, shared resolver and native upstream/history/persistence proof | P2 |
| Sketch on planar result face | [#112](https://github.com/GideonBa/Better-Linux-CAD/issues/112) | Implemented: direct planar result-face Sketch creation, associative producer-owned support, strict recovery/invalidation/repair and native modeling/history/persistence proof; delivered in PR #128 | P2 |
| Cut feedback and workflow | [#113](https://github.com/GideonBa/Better-Linux-CAD/issues/113) | Implemented: distinct Cut outcomes, correctable diagnostics and exact curved extents; native regression verified | P1 |
| Complex-Part responsiveness | [#114](https://github.com/GideonBa/Better-Linux-CAD/issues/114) | Completed: affected-root previews/transactions, bounded validated history geometry and native deep-rotor acceptance; conservative fallbacks and local JSON target miss documented | P1 |

Completion audit on 2026-10-29 reconciles the two previously stale #106/#112 status cells with their
accepted implementations and delivery records below. The original issue queue is closed; PRs
#123–#135 were created and merged serially, with each branch based on the preceding merge. Numbered
completion remains through Block 143. The documented performance measurement limits and later
STEP-import deferrals retain their existing scope.

The 2026-10-28 dimension-input follow-up implements #107/#108 with shared painted/picked badge
bounds, independent double-click resolution, visible selection and multi-dimension Delete/Backspace
enablement. The unchanged editor/deletion transactions retain geometry, relations and backing
parameters. Full GUI **557 passed / 20 native-only skips / 76,327 assertions**, native affected
workflows **49 / 3,115**, and native 150% **7 / 1,606** passed. Exact contracts and evidence live in
`docs/gui-sketch-dimension-authoring-mvp8.md`. This is an unnumbered usability fix; Block 138 remains
planned and no entire implementation block is advanced.

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

## Serial issue and implementation queue — 2026-10-29

The authorized delivery order is one active implementation branch and at most one open PR. Finish
the complete issue, inspect the staged diff, run appropriate headless/native checks, update existing
contracts and evidence, commit and normally push, then merge its PR through main's rules. Fetch and
verify the delivered tree before starting the next issue from main. Do not stack unfinished PRs.

The adopted issue queue is **#114 → #104 → #109 → #111 → #110 → #112 → #106**. Existing
merged #105/#107/#108/#113 fixes remain regression coverage. Start with #114's remaining full-model
Apply/history hotspot, then complete the endpoint, construction, axis, face-support and dimension
presentation acceptance criteria in their own issues. Reading dependency plans does not start their
implementation. Close an issue only when its acceptance criteria and evidence are complete.

**Implementation and acceptance complete: #114**, branch `fix/complex-part-history-cache`, based
on main `8e72a8e`; deliver through its single PR before starting #104 from verified merged main.
Delivered by [PR #123](https://github.com/GideonBa/Better-Linux-CAD/pull/123), merge `736f100`;
issue #114 is closed and the delivered tree matches the tested commit `8003646` exactly.
The new isolated five-sample 40-blade baseline measures warm Apply p95 **5,080.7 ms**, Undo
**5,256.0 ms** and Redo **5,071.9 ms**. Conservative affected-root transactions and a four-state
runtime history LRU reduce these to **126.3 / 15.9 / 18.4 ms**. Atomic publication, strict validation,
exact history/persistence and full fallback remain. Focused proof passes **7 / 969**; the isolated
5/20/40-blade native profile passes **1 / 1,379**. Full Core **549 / 17,217**, Geometry **459 /
35,243** and final offscreen GUI **574 passed / 20 native-only skips / 80,765 assertions** pass.
Native affected-family and required shell coverage pass **39 / 12,547** without skips. The scaled
rotor/input gate passes **8 / 2,435**; separate startup reaches intentional timeout **124**. All
five targets build. Five new regression cases include a reproduced legacy mixed-model final-result
defect; it retains full execution. The native popup fixture keeps its original two-second/four-warp
bounds and strict row/pointer checks after loop unwinding. All **886** final source fingerprints
remain unchanged through validation. Exact raw measurements, provenance and checks are in
`docs/gui-modeling-workspace-mvp9.md` and `docs/development-setup.md`. The 40-blade JSON p95
**22.949 ms** remains above the unchanged local **20 ms** target; cache-miss history and conservative
fallbacks still perform real full geometry work. No release SLA or universal latency bound is claimed.

**Implementation and acceptance complete: #104**, branch `fix/persistent-line-endpoint-snaps`,
based on verified main `736f100`. Delivered by [PR #124](https://github.com/GideonBa/Better-Linux-CAD/pull/124),
merge `4f655c1`; issue #104 is closed and the delivered tree equals tested commit `a5c8a8a`.
Stable canonical endpoint evidence replaces coordinate-based post-commit chain coupling.
Geometry and automatic Coincident recipes share one history transaction. Closure, restart, overlap,
twelve segments, persistence/drag, stale-target rejection and queued input are covered by **6 / 313**
offscreen and **6 / 331** natively at both 100% and 150%. The full GUI suite passes **578 / 20
expected native-only skips / 81,018 assertions**; native affected-sketch and required shell acceptance
pass **82 / 3,609**, without skips. Five targets build; Core/Geometry remain unchanged from verified
main. The input manifest and exact checks live in `docs/development-setup.md`, with behavior in
`docs/gui-sketch-basic-creation-mvp8.md`. Remaining order: **#109 → #111 → #110 → #112 → #106**;
Blocks 138–143 remain planned.

**Implementation and acceptance complete: #109**, branch `fix/reversible-sketch-construction`,
based on verified main `4f655c1`. Delivered by [PR #125](https://github.com/GideonBa/Better-Linux-CAD/pull/125),
merge `d32edff`; issue #109 is closed and the delivered tree equals tested commit `ab4da06`. Role-only conversion retains geometry, canonical ids, exact rational weights, parameters,
constraints, dimensions and references; nested/generated groups expand without duplicating members.
PartDocument persistence now carries curved roles. Geometry excludes auxiliary boundaries/holes;
whole-selection read-only checks and downstream rejection stay atomic. Native proof covers a
rectangle with curved inner geometry, conversion/reconversion, diameter authoring, drag, Extrude/Cut,
exact history, reopen, empty-cache volume equivalence and non-mutating STEP export. Unlinked editable
projection snapshots inherit the actual source type's role; associative references remain read-only.
Full Core **555 / 17,463**, Geometry **461 / 35,315** and final GUI **582 passed / 21 expected
native-only skips / 81,234 assertions** pass. Native affected-family and required shell coverage
pass **101 / 7,086**; final 150% construction/projection proof passes **5 / 614**, without skips.
All five targets build; final source/inventory fingerprints and exact evidence are in
`docs/development-setup.md`. Remaining issue order is **#111 → #110 → #112 → #106**. This fix
advances no numbered implementation block; Blocks 138–143 remain planned.

**Implementation and acceptance complete: #111**, branch `fix/body-rotate-axis-parity`, based
on verified main `d32edff`. Body Rotate exposes typed explicit/derived datum, construction-line and
supported semantic-edge sources with clear labels. Scoped native edge collection shares cache-aware
Geometry resolution with the chooser, manipulator frame, preview and Apply; explicit numeric axes
retain the consuming coordinate-frame contract. Five off-origin 90-degree axis representations,
invalid references and posed edge presentations pass Geometry proof. The native 100%/150% workflow
covers derived selection, cancellation, viewport edge selection, source depth 8 → 12 mm, exact local
Undo/Redo, canonical reopen, full recompute and STEP. Full Geometry **463 / 35,442** and full GUI
**606 cases: 584 passed, 22 expected native-only skips / 81,444** pass. Native workflow **1 / 320**
at each scale, affected native/required shell gate **34 / 7,497**, and separate startup **124** pass;
all 890 final runtime fingerprints remain unchanged. Exact contracts and logs are recorded in
`docs/development-setup.md`. Delivered by [PR #126](https://github.com/GideonBa/Better-Linux-CAD/pull/126),
merge `c73226b`; issue #111 is closed and main exactly matches tested commit `bbc69c4`.
Sketch-line persistence is tracked in #110; this fix advances no numbered block. Remaining issue
order at delivery: **#110 → #112 → #106**.

**Implementation and acceptance complete: #110**, branch `feat/persistent-sketch-line-axes`,
from verified main `c73226b`. Revolve and Body Rotate persist exact `(SketchId, SketchEntityId)`
references for editable normal/construction/centerline segments. Current endpoints map through the
cache-aware workplane exactly once, including owned poses. Sketch dependencies propagate source
edits; strict new JSON and membership checks reject malformed/missing/renamed/duplicate sources
without coordinate retargeting. Accessible existing selectors retain typed identity; Revolve keeps
its plane/non-crossing guards. Full Core **557 / 17,711**, Geometry **466 / 35,655** and full GUI
**609 cases: 586 passed, 23 expected native-only skips / 81,564** pass. Native normal/construction/
centerline generator paths pass **1 / 851** at each of 100% and 150%, authoring a 180° oblique
Revolve, 90° Body Rotate, retained endpoint drag, downstream two-instance pattern, exact history,
full recompute, canonical reopen and STEP. Final affected native/required shell gate **45 / 7,735**
and separate startup **124** pass, with all 892 runtime fingerprints unchanged. Existing contract
and evidence files are synchronized; see `docs/development-setup.md`. Delivered by
[PR #127](https://github.com/GideonBa/Better-Linux-CAD/pull/127), merge `fd7d17f`; issue #110 is
closed and main exactly matches tested commit `873cbe6`. Remaining issue order: **#112 → #106**.
Blocks 138–143 remain planned.

**Completed: #112**, branch
`feat/associative-result-face-sketches`, from main `fd7d17f`. Eligible current planar faces now
create a producer-bound support and Sketch atomically. Authored face origins survive OCCT Boolean,
finishing and transform history; copy instances retain separate identities. Missing or split
supports fail closed and have an explicit previewed repair route. The cache-aware frame follows
thickness/profile/datum/pose edits without rewriting local Sketch geometry or applying poses twice.
Old six-role workplanes and old canonical files remain unchanged. New-file loading interleaves
support metadata with the authored executable order. Cancel removes the pair and unfinished
geometry, preserves Save As/dirty markers and advances model/presentation revisions; initial
Undo/Redo removes/restores both records. Finish hands the profile to modeling through a Faces filter.

All five targets build. Complete Core **557 / 17,711**, Geometry **478 / 36,075** and GUI
**612 cases — 587 passed, 25 expected native-only skips / 81,685 assertions** pass. Focused
Geometry is **12 / 420**, UI/coverage **4 / 412**. Native creation/pocket 8 → 20 mm, oblique
polygon/Loft/Boolean/Fillet support workflows pass **2 / 1,021** at 100% and 150%, including exact
history, full recompute, Save/Open and STEP. The affected required native gate passes
**39 / 6,975**, without skips or unmatched filters; separate startup reaches intentional **124**.
All **896** final runtime fingerprints remain unchanged. Existing contracts, coverage and raw
verification commands/logs are synchronized in `docs/development-setup.md`. Delivered by
[PR #128](https://github.com/GideonBa/Better-Linux-CAD/pull/128), with verified implementation
commit `76b68ed`. The remaining issue queue is **#106**; start it only after this PR is merged.
Imported supports remain outside this issue and no numbered implementation block is advanced.
PR #128 merged as `c9d203b`; the delivered main tree exactly matches head `de270a9`, all runtime
fingerprints match, issue #112 is closed with its seven acceptance checks complete and no PR remains.

**Completed: #106**, branch `fix/sketch-dimension-presentation`, from main
`c9d203b`. Dimensions now separate true measured references, placed measure/extension lines,
angle/arc-length arcs, leaders, origin marks and terminators. A shared projected layout supplies
painted strokes, triangles, caption bounds, picking and inline-editor placement. Horizontal/vertical
spans show the correct projection; radius stays on the actual partial arc. View-sized metrics and
transient reflow preserve attachment during label movement, zoom and oblique-plane editing.
Dimension meaning, parameter bindings and persisted offsets remain unchanged.

All five targets build. Complete Core **557 / 17,711**, Geometry **479 / 36,084** and GUI
**620 cases — 594 passed, 26 expected native-only skips / 81,956 assertions** pass. Deterministic
layout/required inventory passes **10 / 572**. Native layout and existing dimension input pass
**8 / 2,074** at both 100% and 150%, including pairwise-disjoint initial caption rectangles,
actual mouse/key input, exact history and reopen. Affected native Sketch/desktop/lifecycle/coverage
passes **52 / 1,840** without skips; separate startup reaches intentional **124**. The final
**900** runtime fingerprints remain unchanged. The native scale proof precedes only a mechanical
Shell-test filename/coverage-registration correction; its test content and production sources
are unchanged, and the rebuilt final broad/full gates pass. Exact commands and evidence live in
`docs/development-setup.md`.

Selected editor conventions and the official DIN EN ISO 129-1:2022-02 metadata/public ISO preview
review are documented in `docs/gui-sketch-dimension-authoring-mvp8.md`. Full DIN/ISO conformity
requires a separate review of the complete applicable text and is not claimed here. Manual label
placement retains existing overlap cycling; no automatic caption-collision solver is introduced.
Delivery: [PR #129](https://github.com/GideonBa/Better-Linux-CAD/pull/129), verified implementation
commit `8dd28dc`. PR #129 merged as `61e60b6`; main exactly matches tested head `9b929e7`,
all 900 runtime fingerprints match, all six issue acceptance checks are complete, and a fresh
GitHub search finds no open issues or PRs. No numbered implementation block is advanced by this fix.

With the issue queue complete and merged, execute **Blocks 138 → 139 → 140 → 141 → 142 →
143**, each with the architecture boundaries, GUI access and acceptance gates in
`docs/step-import-sequence-mvp10.md`. This queue is a plan, not evidence that any later block is
complete; Blocks 137–143 are now complete, each delivered after the preceding protected merge.

## Implemented Block 138 — STEP/XDE reader and geometry inspection (2026-10-29)

Branch `feat/step-xde-inspection` starts from verified main `61e60b6`, after all open issues and
PRs are closed. Secure byte snapshots, the Geometry reader and the existing headless/Shell
inspection clients are delivered together on this branch; no additional PR is opened in parallel.

The reader normalizes declared units to millimeters, preserves supported names and shared product
definitions, separates product-local bodies from direct occurrences, and rejects invalid/empty
transfer, ambiguous units, external references and invalid composed placements before publication.
Inspection is read-only and disposable, with explicit source revalidation and reliable invalidation
after atomic file replacement. Core JSON and authored history contracts remain unchanged.

All five targets build. Full Core **560 / 17,987**, Geometry **491 / 36,718** and GUI
**625 cases — 597 passed, 28 expected native-only skips / 82,197 assertions** pass. Native source,
inspection, viewer and required coverage pass **21 / 3,267** at 100%; the two new inspection cases
also pass at 150% **2 / 673**. Separate startup reaches intentional **124**. The isolated 20-blade
rotor profile passes **1 / 64**, with five warm reader samples at **169.8 ms median / 173.2 ms
observed maximum**, without a portable budget claim. All **904** runtime fingerprints remain
unchanged. Exact commands, red-before-fix regressions, captures and raw timing observations live in
`docs/development-setup.md` and `docs/benchmarks/block138-step-inspection.json`.
Delivery: [PR #130](https://github.com/GideonBa/Better-Linux-CAD/pull/130), verified implementation
commit `4a7fdd6`. PR #130 merged as `ca2db6e`; main exactly matches tested head `a0aaa7c`,
all 904 runtime fingerprints match and fresh GitHub searches find no open issues or PRs.

## Implemented Block 139 — Imported topology identity and recovery (2026-10-29)

Branch `feat/step-import-topology` starts from verified main `ca2db6e`, after PR #130 is merged.
The implementation adds persisted imported body/face/edge/vertex identities with normalized
recovery descriptors and explicit Resolved/Lost/Ambiguous outcomes. Supported analytic capabilities
are frozen; imported semantic references resolve for downstream Part inputs and local/cross-hierarchy
Assembly targets.
Exact boundaries and focused tags live in `docs/step-import-sequence-mvp10.md`. Core catalog intent,
Geometry recovery/capability resolution and the GUI display/filter/picking client are implemented.
All five targets build; focused Core **6 / 247**, Geometry **15 / 1,878** and GUI
**24 cases — 22 passed, 2 expected native-only skips / 3,348 assertions** pass. Full Core
**566 / 18,234**, Geometry **506 / 38,596**, native 100% **26 / 5,137** and native 150%
**1 / 934** pass. Full GUI passes **631 cases — 602 passed, 29 expected native-only skips /
83,143 assertions**; separate native startup reaches intentional **124**. All **914** runtime
fingerprints remain unchanged. Reviewed regression cases protect duplicate capture, independent
products, fixed asset roots, exact analytic limits and prior cache retention after source failure.
Contracts, local build-artifact repair and complete evidence live in `docs/development-setup.md`
and the STEP import sequence. Delivery: [PR #131](https://github.com/GideonBa/Better-Linux-CAD/pull/131),
verified implementation commit `95f15a6`. PR #131 merged as `d403460`; main exactly matches tested head `945cccd`, with all 914 runtime fingerprints unchanged. Fresh GitHub searches find no open issues or PRs before Block 140 begins.

## Implemented Block 140 — Reference Parts in assemblies (2026-10-29)

Branch `feat/step-reference-parts` starts from verified main `d403460` after PR #131 is merged.
Immutable imported Part definitions own explicit source/catalog and ordered Body mappings and
share their separate Reference geometry authority across repeated and nested Assembly occurrences.
Existing exact topology and explicit datum Frame targets participate in constraint/joint solving,
motion, analysis and protected flat/structured STEP export through a verified saved-root context.
Part edits fail in Reference mode. Shared-source refresh validates and reports affected owners;
conversion creates a new Editable Part without replacing existing Assembly instances.

The Sources task uses the same prepared headless commands and atomic transaction path. Local
product/child-definition previews isolate existing scene presentations; root-Assembly placement
retains context. Native visibility and camera checks prove restoration after Cancel and Apply.
Open/history retain valid persisted imported intent and Redo with a diagnostic and cleared derived
geometry when external recovery fails; authoring Apply remains strict.

All five targets build. Full Core **577 / 19,304** and Geometry **533 / 41,638** pass. Focused
GUI passes **18 cases — 15 passed, 3 expected native-only skips / 2,138 assertions**. Full GUI
passes **638 cases — 607 passed, 31 expected native-only skips / 83,543 assertions**. Native 100%
passes **37 / 5,771** and native 150% passes **2 / 1,110**; reviewed captures show isolated complete
conversion geometry and reachable controls. Separate native startup reaches intentional **124**.
The historical bodyless Part CLI export passes and writes **19,170 bytes**. The final **961**-file
runtime inventory is frozen under
`b62553c647c81ea3d1df8038987d7aefc9c5dc7448c70a83bce602734e3de6a1`; only the corrected historical
GUI Assembly fixture differs from the native-gate inventory. Exact commands, source-freshness and
failure-atomicity regressions, inventory comparison and acceptance evidence live in
`docs/development-setup.md` and `docs/step-import-sequence-mvp10.md`.
The final serialized documentation/GUI inventory passes **3 / 313** after advancing this boundary.
Delivery: [PR #132](https://github.com/GideonBa/Better-Linux-CAD/pull/132), verified implementation
commit `16fd35e`.
PR #132 merged as `f426480`; main exactly matches tested head `ba79e27`, including all **961**
runtime fingerprints. Fresh GitHub searches find no open issues or PRs before Block 141 begins.

## Implemented Block 141 — EditableBody downstream modeling (2026-10-29)

Branch `feat/step-editable-body-modeling` starts from verified main `f426480` after PR #132 is
merged. Direct EditableBody import creates normal editable Parts with real immutable base features,
ordered Solid/Surface output Bodies, and source → catalog → base → downstream dependencies.
Per-output base evidence is distinct from latest modeled Body geometry. Incremental parameter
changes retain that base; actual imported face Sketches and placed imported-edge finishing preserve
owned semantic provenance. Full modeled shared-source refresh validates all bindings before commit.

Core/Geometry reviews and regressions protect source graph identities, JSON replay, selected Body
ownership, guarded deletion, terminal predecessor restoration, stale-source rejection and old-cache
reconciliation after base/modifier deletion or reused identities. GUI previews, parameter edits,
feature editing, repair and export share the saved verified source context. Open/history retain
valid imported intent with failed derived state for genuinely lost subentities; authoring stays strict.

All five targets build, with **502 nonempty** dependency records. Focused Core **12 / 1,544**,
Geometry **17 / 921**, and final focused GUI **14 cases — 11 passed, 3 native-only skips / 849
assertions** pass. Full Core **589 / 20,850**, Geometry **550 / 42,559**, and GUI **643 cases —
611 passed, 32 native-only skips / 83,923 assertions** pass. Native 100% **29 / 17,979** and 150%
**1 / 554** pass without skips. Actual Qt input proves direct import, face-supported boss, Cut,
Hole, imported-edge Fillet, physical parameter change with unchanged base, history, reopening,
protected export, complete modeled refresh and incompatible-face rollback. Reviewed captures show
reachable controls at both scales. Separate native startup reaches intentional **124**; historical
CLI export writes **19,170 bytes**. Runtime freeze: **970 files**,
`b4e445ff1a40202e7b623460eed89263def50f04457b5d17b31d29961e92d288`.
Core/Geometry gates precede the final GUI-only correction; their sources/binaries remain unchanged.
Exact commands and final inventory evidence live in `docs/development-setup.md`.

Delivery: [PR #133](https://github.com/GideonBa/Better-Linux-CAD/pull/133), verified implementation
commit `c4fc431`. Runtime fingerprints remain unchanged; this PR must merge before Block 142 starts.

PR #133 merged as `525c98f`; main exactly matches tested head `2db9891` and all **970** runtime
fingerprints. Fresh GitHub searches find no open issues or PRs before Block 142 begins.

## Implemented Block 142 — Structured STEP assembly import (2026-10-29)

Branch `feat/structured-step-assembly-import` starts from verified main `525c98f` after PR #133.
Creates one shared imported Part/Assembly per unique product definition, exact direct occurrences
beneath an explicit synthetic BLCAD root, deterministic generated identities and names, and strict
candidate validation. The existing Sources task creates a distinct protected Project copy using
explicit Reference/EditableBody source mode and the shared headless command. No new JSON payload
or inferred assembly constraints/joints are introduced. Malformed usage endpoints/cycles/depth,
ambiguous Part/Body recovery and unresolved Assembly ordering fail closed. Equal-geometry Assembly
products remain distinct and receive deterministic IDs from supported metadata and rooted context.
Source geometry refresh retains ordinary authored structure and placements.

All five targets build; all **507** Ninja dependency records are valid and nonempty. Focused Core
**7 / 1,651**, Geometry **10 / 1,313**, and GUI **7 cases — 6 passed, 1 native-only skip / 1,590
assertions** pass. Full Core **596 / 22,501**, Geometry **560 / 43,872**, and GUI **647 cases —
614 passed, 33 native-only skips / 85,200 assertions** pass. Native 100% **11 / 5,238** and 150%
**1 / 1,414** pass without skips. Native input proves both modes, all five posed leaves, protected
copy/open, unchanged original session, modal-source races and stale consuming geometry clearing.
Reviewed captures show the hierarchy and posed preview at both scales. Legacy CLI writes
**19,170 bytes**. Final runtime freeze: **978 files**,
`a39896f9dac3f7bb6ecbcdfcb8bf61a7d8007bea99ac042e4ed5b36c70c980c0`.
Separate native startup reaches intentional **124**; final Shell inventory passes **3 / 313**.
Exact commands and final inventory/startup evidence live in `docs/development-setup.md`.

Delivery: [PR #134](https://github.com/GideonBa/Better-Linux-CAD/pull/134), verified implementation
commit `dbe6560`. Runtime fingerprints remain unchanged; this PR must merge before Block 143 starts.

## Implemented Block 143 — Integrated STEP import acceptance (2026-10-29)

PR #134 is merged as `c61f5fe`. Main exactly matches tested head `d930a9a`, tree
`5a2aad73ad0a76398fd131c0ee12253329acde4f`, and all **978** runtime fingerprints. Fresh GitHub
searches find no open issues or PRs before branch `test/integrated-step-import-acceptance` starts.
Block 143 closes continuous import/relationship/motion/analysis/refresh/edit/re-export acceptance
and exact GUI/headless equivalence using the shared commands already delivered. Reference command
creation/repetition, imported relationships, 25-degree joint motion, analysis, JSON, grouped refresh,
conversion and both physical STEP read-backs share one continuous story. Direct and converted
Editable entries use the same ordered additive/subtractive/finishing history, full/incremental/
reopened producer equivalence and actual export geometry. Structured imports retain both modes;
the Editable continuation adds ordinary authored Point relationships, a Body transform and actual
supplier geometry refresh without replacing hierarchy/names/placements. Missing/Changed/Lost/
Ambiguous/stale-candidate failures preserve input authority and existing outputs; malformed initial
structures reject candidate creation. Source bytes and portable semantic JSON are verified.

Acceptance: all five targets build; full Core **596 / 22,501** and Geometry **564 / 49,787**,
focused Geometry **5 / 5,978**, focused GUI **8 cases — 5 passed, 3 native-only skips / 1,765
assertions**, native 100% **13 / 7,391** and 150% **4 / 3,875** pass. Native captures are reviewed;
all **507** dependency records are valid and nonempty. Full GUI passes **648 cases — 615 passed,
33 expected native-only skips / 85,528 assertions**. Separate startup reaches intentional **124**;
legacy CLI writes **19,170 bytes**. Runtime freeze: **978 files**,
`e0942e2c06a4aa15f79ce60597a12a764a9fa36f24e8b3a2fc7f0789615d8722`.
Application/library implementation is unchanged by this acceptance slice. Exact commands, proof
locations and final numbered-documentation inventory live in the STEP sequence and development setup.
Final Shell inventory passes **3 / 313** after completion advances through Block 143.

Delivery: [PR #135](https://github.com/GideonBa/Better-Linux-CAD/pull/135), verified implementation
commit `220d030`. All runtime fingerprints remain unchanged. This delivery completes the requested
work through Block 143; later implementation remains unassigned.

## Implemented Block 144 — Inventor-inspired workflow and layout (2026-11-05)

User-authorized GUI revision begins on `feat/inventor-workflow-layout` from clean main
`c9e4560`, after PR #136 merged and a fresh GitHub search confirmed no open PRs. Each delivery
must merge normally through branch protection before another implementation branch starts.
Typography (Barlow/IBM Plex Mono), Pixelarticons, dark-violet palette and CAD authorities remain
unchanged. This block concerns presentation and access to existing functions, not new geometry.

| Work | Acceptance | Status |
| --- | --- | --- |
| Full-width workspace | Ribbon spans above browser, viewport and task panels; compact/scaled native controls remain reachable | Verified |
| Command organization | File/quick access across contexts; Model, Sketch, Surface, Assembly, Inspect, Manage, View route existing actions | Verified |
| Command discovery | Ctrl+K searches registered actions/groups with shortcuts, live availability and original command execution | Verified |
| Recoverable panels | View restores hidden properties/diagnostics and resets the supporting layout without cancelling tasks | Verified |
| Regression acceptance | Full GUI suite plus focused native modeling, Sketch, Surface, Assembly, exchange, appearance and 150% checks | Passed |
| Delivery | Reviewed diff, documented results, professional commit, normal push and protected PR merge | Delivered via PR #137 |

Final GUI: **620 passed, 33 native-only skips / 85,483 assertions** across 653 cases. Broad native
Qt/OCCT: **96 / 33,276**; initial 150% modeling/appearance: **13 / 1,565**; after the chooser-icon
polish, final native and 150% appearance/discovery both pass **12 / 1,288**, without skips. Separate
startup reaches intentional timeout **124**. All five targets build and all **508** dependency records
are nonempty/valid. Final **948** runtime fingerprints stay unchanged. Existing CAD/solver and file
contracts, fonts, icon assets and color definitions are preserved. Runtime gates and actual captures
are recorded in `development-setup.md` and `user-interface.md`.

Delivery: [PR #137](https://github.com/GideonBa/Better-Linux-CAD/pull/137), implementation
`e538be8`, following the separate planning/serial-delivery commit `76705a1`. This PR must merge
normally and updated main must match the tested tree before any successor branch is created.
Runtime fingerprints remain unchanged through this documentation-only delivery update.
Previously deferred modeling capabilities remain scoped by their feature contracts.

## Blocks 145–204 — Advanced modeling workflows

Status authority: Blocks 145–150 are **Implemented and accepted**;
Block 151 is **In progress**; Blocks 152–204 are **Planned; not started**.
The 2026-11-06 implementation request starts the serial slices below. Detailed block dependencies,
contract owners and completion criteria live in `inventor-parity-sequence.md`. Scenario definitions,
negative cases, combination matrix, native proof and performance protocol live in
`inventor-parity-test-plan.md`. Shared UI/layout behavior is owned by the planned section of
`user-interface.md`. Existing catalog lists are mapped there rather than assigned competing sequences.

| Phase | Blocks | Assigned scope | Status |
| --- | --- | --- | --- |
| A | 145–151 | Shared workflow/evidence, feature history/suppression/reorder, material resources and physical properties | 145–150 accepted; 151 in progress |
| B | 152–158 | Semantic holes, extents/seats, cosmetic/modeled threads, placement patterns, Wizard UI and everyday acceptance | Planned; not started |
| C | 159–165 | Planar G2/equation curves, spatial constraint model/solver and interactive 3D curves | Planned; not started |
| D | 166–174 | Curve-preserving paths, multi-wire/semantic-edge Sweep, verified G2 Loft, Offset/Thicken, curved Trim/Extend and Surface acceptance | Planned; not started |
| E | 175–184 | Variable/face/full-round Fillet and corners, Rib/Web, Emboss/Engrave and their property workflows | Planned; not started |
| F | 185–188 | Direct face edits, checked healing, immutable imported bases and integrated advanced Part acceptance | Planned; not started |
| G | 189–198 | Model states/variants, occurrence-local flexibility, adaptive cross-document design, coordinated Assembly holes and UI | Planned; not started |
| H | 199–204 | Motion/contact extensions, finite measurement, quality/thickness analysis, measured performance and final combined acceptance | Planned; not started |

Advance individual completed blocks or named slices only after their full criteria and relevant
native checks pass. A phase range remains incomplete while any constituent block is open. Serial
PR/merge discipline applies to implementation and planning deliveries; no successor branch starts
before the preceding PR is merged and main refreshed. Deferred drawings/BOMs, sheet metal,
CAM/FEM, automatic fastener libraries and general physical dynamics are not assigned here.

### Planning delivery — 2026-11-06

Starts from clean main `e08280c` after GitHub confirms PR #137 merged and no open PRs. Main is
fetched and fast-forward verified before `docs/inventor-feature-parity-plan` is created. This change
reconciles existing catalogs, adds the serial block sequence and combined test obligations, and
extends the existing UI authority with a clearly labelled future workflow contract. Runtime code,
test implementations, file schemas and accepted capability inventories are unchanged. Validation
passes: Markdown/contract links, all 60 consecutive blocks and backward dependencies, all 37
scenario IDs, unchanged implementation boundary, documentation-only path scope and `git diff --check`.
Existing GUI shell coverage passes **3 / 313**, without skips. Builds/full CAD/native runs are not
repeated for documentation changes; future test gates are not claimed as executed. Exact commands
are recorded in `development-setup.md`. Source/staged diffs are reviewed. Delivery:
[PR #138](https://github.com/GideonBa/Better-Linux-CAD/pull/138), plan commit `86fc04f`, on the
normally pushed `docs/inventor-feature-parity-plan` branch. Merge through the required PR path,
refresh main and compare its tree with the reviewed branch before starting Block 145. This
planning delivery leaves implemented status through Block 144.

### Block 145 serial implementation slices — 2026-11-06

The implementation request authorizes the full sequence. Deliver the following coherent slices
serially; none alone closes Block 145. A merged preceding PR and refreshed main are prerequisites
for the next slice/branch.

| Slice | Deliverable | Status |
| --- | --- | --- |
| 145A | Extend existing capability inventory with explicit planned/implemented API/command/proof mappings, reject invalid evidence, capture reproducible unchanged-intent/geometry and timing baseline | Merged via PR #139 |
| 145B | Reusable property groups, active semantic selector and diagnostics controls; adopt in existing Extrude with keyboard/compact/native proof | Merged via PR #140 |
| 145C1 | Extend existing GuiDocumentSession with a complete transient Part-workflow checkpoint, committed-only Save and one-history publication; headless failure/history/sidecar tests | Merged via PR #141 |
| 145C2 | Existing-owner Extrude/source Sketch breadcrumbs using C1, revalidation, complete Cancel, committed-only Save and guarded consumers; native integration | Merged via PR #142 |
| 145C3 | Explicit profile-selector command-first/Create Another lifecycle, retained compatible settings without hidden targets; whole-Block-145 native/compact/scaled closure | Merged via PR #143 |

145A starts on `feat/advanced-workflow-evidence` from clean `dbff649`, after verified merged
PR #138 and refreshed main. `user-interface.md` owns the slice design and baseline boundaries.
No new CAD command or feature schema is introduced by the evidence slice. Keep the implementation
boundary at 144 until all implementation slices and the complete Block-145 criteria pass.

145A adds 24 explicit capability entries (existing Extrude plus 23 planned extensions), validates
actual actions, declared/compiled headless APIs and registered nonhidden headless/native proof
sources, and rejects fabricated disposition, ownership beyond the completed boundary, wrong
sources, unresolved/OR filters and missing capabilities. Historical family acceptance records
are unchanged. A reproducible Extrude baseline checks 1600 mm³ geometry, JSON invariance,
Apply/Undo/Redo, three fresh-session and ten reused-controller samples with source/runtime hashes.
The raw artifact is `benchmarks/block145a-workflow-baseline.json`.

GUI test target builds. Focused coverage/baseline passes **6 / 1,523**, without skips. Native existing
modeling/layout baseline passes **6 / 390** at both 100% and 150%; ordinary/compact captures are
reviewed. Exact commands, measured limits, sandbox file-publication retry and startup evidence
live in `development-setup.md`. No new property controls, CAD operation or document schema are
claimed by this slice. 145B/C remain required before completing Block 145.

Delivery: [PR #139](https://github.com/GideonBa/Better-Linux-CAD/pull/139), implementation `40bcd79`.
The documentation-only delivery record preserves the 943 runtime fingerprints. Merge normally,
refresh main and verify the reviewed tree before creating the 145B branch.

145B starts on `feat/shared-property-controls` from verified main `ff39198` after PR #139 merged;
the reviewed 145A tree matches main. The design in `user-interface.md` scopes the reusable widgets,
existing Extrude adoption, candidate-only selection, diagnostics and Ctrl+Enter validation.

145B implements those widgets and controls without a Core or persistence change. Three new widget/
workflow cases and the extended native face-pick case verify collapsed diagnostics, command-owner
lifetime, cleared/reselected inputs, blocked-signal synchronization, invalid/corrected typed values,
compact completion controls and the physical 1600 mm³ Ctrl+Enter outcome. GUI/application build and
format/diff checks pass. Focused offscreen: **27 passed, 1 native-only skip / 3,682 assertions**;
full GUI: **626 passed, 33 native-only skips / 85,853**. Native 100%: **57 / 23,719**; native 150%:
**12 / 1,862**, no skips. The first scaled run's existing search-navigation focus failure and full
repeat are documented in `development-setup.md`. Captures are reviewed, startup reaches timeout
124, and runtime fingerprints remain unchanged. Numbered completion remains at 144.

Delivery: [PR #140](https://github.com/GideonBa/Better-Linux-CAD/pull/140), implementation `84fd6e1`.
PR #140 is merged as `6c15fe1`; its reviewed tree matches refreshed main. Slice C is split before
implementation because nested Sketch commands need an explicit session transaction boundary.
`feat/compound-part-workflow` starts from verified `6c15fe1`. C1 extends the existing session's
history/publication authority; it adds no document schema or second model/solver. `user-interface.md`
owns the bounded API and Save/Cancel/failure design. C2 alone wires visible breadcrumb transitions;
no UI adoption or Block-145 completion is claimed by C1.

C1 implements the complete checkpoint in the existing session authority, with committed-only
main/sidecar Save and Save As, isolated local history, one final validated publication, full Cancel
and immutable entry state when sessions are copied. Eleven new transaction cases pass **655
assertions**. GUI/application build, formatting and diff review pass; broader focus passes **35
cases, 1 native-only skip / 2,223 assertions**. Full GUI passes **637 cases, 33 native-only skips /
86,508 assertions**. Native existing-workflow compatibility passes **5 / 462** at both 100% and
150%, without skips. Exact commands, source freeze and scope limits live in `development-setup.md`.
C2 remains required, and implemented-through remains 144.

Delivery: [PR #141](https://github.com/GideonBa/Better-Linux-CAD/pull/141), implementation `861a5c6`.
PR #141 is merged as `dc0a235`; refreshed main matches the tested tree.
`feat/feature-source-workflow` starts from that verified state. C2 delivers the source-edit
transition and consumer guards; C3 separately delivers fresh profile collection/Create Another.
The split keeps the root transaction and selection lifecycle reviewable without advancing Block 145.

C2 adopts complete workflows in the current Extrude owner and existing Sketch workspace. It adds
fixed Part/Draft and source breadcrumbs, explicit pending-source revalidation, refreshed frames and
semantic limits, local Sketch history, complete root cancellation and one validated root Apply.
STEP/full-model Measure guard draft geometry. A stale borrowed pointer in an old test is replaced
with stable-ID lookup after cancellation; a genuine selection regression is fixed by rebuilding the
restored scene before restoring selection. Contracts, actual Qt proof inventory and native images
are updated. No Core/schema change and no C3 completion is claimed.

GUI/application build, scoped formatting and diff checks pass. Focused affected families: **48
passed, 2 native-only skips / 5,537 assertions**; feature/finish focus: **60 passed, 2 native-only
skips / 2,392**. Full GUI: **641 passed, 34 native-only skips / 86,827**. Broad native: **57/59**
pass; two existing cursor-settling failures in Surface/rotor dropdowns pass a focused complete repeat
(**2 / 1,486**), without product or harness changes. Native 150%: **22 / 3,920**, no skips. Captures
are reviewed and startup reaches intentional timeout 124. `development-setup.md` retains exact
commands, failed initial attempts, freeze and scope limits. Implemented-through remains 144.

Delivery: [PR #142](https://github.com/GideonBa/Better-Linux-CAD/pull/142), implementation `11fd8a8`.
PR #142 is merged as `23cf279`; refreshed main matches the tested tree.
`feat/extrude-create-another` starts from that verified state. C3 extends the same owner, session
workflow and property selector, with no new persistence or geometry authority.


C3 completes explicit Profile/Open Line selection, command-first collection and Create Another in
that owner. Fresh candidates retain only literal/mode settings and require new Profile/Body/Path/
limit references. Required input, source correction, complete cancellation, committed-only Save,
separate feature history and reopening are covered by actual Qt workflows. New tasks scroll to their
primary input; empty summaries and compact/scaled completion controls are verified visually.
Construction/Centerline thin sources are rejected. No new CAD operation or document schema is added.

Final runtime acceptance, completed on 2026-11-07 after the requested pause: full GUI **648 passed,
34 native-only skips / 87784 assertions**; native 100% **80 / 11609**; native 150% **36 / 6931**,
without skips or retries. Separate native startup reaches expected timeout 124. Three isolated native
processes supply three first-workflow and thirty reused-window timing samples; physical/canonical
oracles pass each trial. The checked C3 artifact retains host/build metadata, all raw samples and
runtime fingerprints; historical 145A evidence remains intact. Detailed commands, earlier failed
attempts, explicit scope limits and the unchanged final freeze live in `development-setup.md`.

Together A/B/C1/C2/C3 meet Block 145 UI01 and bounded P01 baseline criteria. Only the shared workflow
capability advances to implemented coverage; 22 future extensions remain planned. Implemented-through
advances to **145**, and Blocks 146–204 remain unstarted. The next implementation branch may begin
only after C3's normal protected PR is merged and refreshed main matches the reviewed tree.

Delivery: [PR #143](https://github.com/GideonBa/Better-Linux-CAD/pull/143), implementation `7f50220`.
The delivery-record change is documentation only. Merge normally, refresh main and compare its
tree with the reviewed branch before starting Block 146.

### Block 146 serial implementation — 2026-11-07

PR #143 is merged normally as `27256d7`. Refreshed main matches the reviewed branch tree;
`feat/authored-feature-history` starts from this verified state. Implemented-through stays 145.
Existing `feature-system.md` owns Core history behavior and `file-format.md` owns persistence.
Block 147 owns effective suppressed execution/cache semantics; Block 148 owns visible browser edits.

| Slice | Bounded deliverable | Status |
| --- | --- | --- |
| 146A | Read-only typed inventory of every executable family; shared atomic replay for existing `feature_order`; unchanged historical JSON and mixed-family/consumer regression proof | Merged via PR #144 |
| 146B | Persistent authored order and sparse active/suppressed intent, validated candidates, typed dependency diagnostics and complete strict/legacy Core/JSON proof | Merged via PR #145 |

A supplies the existing-authority foundation; it introduces no suppression state, new JSON field,
reorder command or UI completion claim. Its public query follows the current dependency order and
includes BodyTransform/imported bases while excluding parameters, Sketches, paths and Body aliases.
The existing generic JSON replay moves into PartDocument and continues using typed adders, preserving
collection order and non-feature consumers. Every future history mutation must reuse that authority.
A alone did not complete 146; B supplies the remaining intent/Core/JSON proof below. Effective
H01 execution remains the first gate in 147, followed by native browser acceptance in 148.

146A is verified against the frozen runtime inventory in `development-setup.md`: focused Core
**12 / 627**, full Core **601 / 22,700**, full Geometry **564 / 49,787**, full GUI **682 cases:
648 passed, 34 native-only skips / 87,895 assertions**, and native persistence/history/Extrude
combinations **36 / 2,708**, without skips. Separate native startup reaches expected timeout 124.
Four checked-in legacy fixtures come from accepted Core `27256d7`, before rebuilding the library.
Invalid metadata retains historical JSON diagnostics; no existing validation is relaxed. The first
sandboxed GUI attempt is interrupted after protected atomic-publication failures; the identical
final run with required filesystem access passes. Exact logs/provenance live in the setup guide.
Implemented-through remains 145. Source/staged review and runtime-fingerprint verification pass.
Delivery: [PR #144](https://github.com/GideonBa/Better-Linux-CAD/pull/144), implementation
`af40935`. The delivery-record commit is documentation only. Merge normally, refresh main and
compare the reviewed tree before starting 146B.

PR #144 is confirmed merged as `7367faa`. Refreshed main matches its reviewed tree;
`feat/part-history-intent` begins from that clean state. B first freezes the versioned persistence
contract in `file-format.md`, extends the existing family inventory/replay authority and keeps
all new history UI/effective-evaluation claims reserved for 147–148.

B completes the assigned 146 intent/Core/JSON contract: every current executable family retains
identity/parameters with sparse states, explicit independent order survives copies/loading, typed
candidates rebuild modifier predecessors and reject unavailable creators/resource dependencies.
All families append under existing explicit history; terminal removals clear both order and state
before ID reuse. Imported bases cannot be suppressed; immutable Reference Parts cannot author
history. Version 2 protects new intent from older readers; four historical byte fixtures stay exact.

Whole Block 146 is accepted with focused Core **27 / 1,116**, full Core **616 / 23,189**, full
Geometry **566 / 50,073**, full GUI **684 cases: 650 passed, 34 native-only skips / 87,935 assertions**,
native combinations **38 / 2,748** without skips, and native startup timeout **124**. All **968**
frozen runtime files remain unchanged. Exact logs, development failures and correction evidence live
in `development-setup.md`. Existing assertions are not weakened; new geometric/GUI guards prevent
unsupported explicit history from consuming old full-model caches or being published as fresh.
Effective suppression/prefix evaluation is **not implemented** by this proof: 147 replaces that
conservative boundary with physical fresh/incremental proof; history browser/End of Part remains
148. Implemented-through advances to 146 only. The post-promotion inventory passes **5 / 495**;
all runtime fingerprints remain unchanged. Source/staged review passes.
Delivery: [PR #145](https://github.com/GideonBa/Better-Linux-CAD/pull/145), implementation
`9329de2`. The delivery record changes documentation only. Merge normally and verify/refetch main
with tree comparison before starting 147.

### Block 147 serial implementation — 2026-11-07

PR #145 is confirmed merged as `a659016`. Refreshed main matches the reviewed branch;
`feat/effective-part-history` starts from that verified state. Implemented-through remains 146.

| Slice | Bounded deliverable | Status |
| --- | --- | --- |
| 147A | Read-only effective availability/prefix plan from existing typed records: active/suppressed/blocked/outside-prefix entries, semantic/resource versus Body input reasons, creator/modifier and consumed-Body rules; complete Core combination proof | Merged via PR #146 |
| 147B | Actual disposable full/prefix Geometry evaluation and complete-source cache freshness; independent fresh/incremental checks, old-cache/picking/analysis/export rejection and native integration | Verified; delivery PR #147 |

A owns no second feature tree, new persistent field or Geometry capability claim. B replaces the
conservative 146B guard with verified execution. Existing `feature-system.md` owns these contracts;
`file-format.md` continues to prohibit persisting derived scope, availability and cache state.
Whole 147 acceptance requires both slices and the assigned H01/H02 physical evidence. Native
browser/End of Part editing remains 148.

147A passes **11 new cases / 538 assertions**, combined history focus **38 / 1,654** and full Core
**627 / 23,727**. The read-only method leaves source JSON/graph/invalidation unchanged and preserves
authored flags while deriving blocked chains. Prefix counts, all families, modifier bypass versus
semantic output loss, explicit closures, imported bases, consumed tools and actual Body ownership
are covered. Resource IDs resembling `body:` aliases remain resources unless a Body owns the node.
The 969-file runtime freeze is unchanged; formatting/source/diff review passes. Geometry/GUI/native
suites are not rerun for this unused Core-only query. Their 146B execution guard is unchanged; full
physical and native acceptance remains mandatory in B. No schema or executable capability advances.
Implemented-through remains 146; source/staged review passes.
Delivery: [PR #146](https://github.com/GideonBa/Better-Linux-CAD/pull/146), implementation `90a836d`.
The delivery record is documentation only. Verify merged state, refresh main and compare the
reviewed tree before starting 147B.

PR #146 is confirmed merged as `f1b8c61`; refreshed main matches its reviewed tree.
`feat/history-geometry-evaluation` starts from that verified state. B uses the existing Core plan
and per-row Body input predecessors. Geometry records source-bound complete/prefix scope in its
transient cache; no persistent field or alternate Part is introduced. Whole 147 remains open.

147B acceptance: all fifteen registered families execute from the existing effective plan on
fresh disposable caches. Suppressed modifiers restore their per-row Body predecessor; suppressed
creators expose no Body; blocked inputs and failed kernel/source checks never publish a partial
replacement. Complete/prefix scopes, canonical source and transient executable lifetime bind derived
results. Identical ID/JSON reuse and public cache mutation/clear cannot reauthorize prior outputs.
Full consumers validate scope; Undo/Redo validates retained cache ownership, and STEP preflight
requires an available visible result. Explicit-history incremental calls use a documented full
fresh fallback. Unscoped legacy execution and both JSON migration contracts stay intact.

Final proof: focused Geometry **9 / 1,923**, GUI **3 / 142**; full Core **627 / 23,727**, Geometry
**573 / 51,710**, GUI **685 cases: 651 passed, 34 native-only skips / 88,037 assertions**. Native
100% combinations pass **39 / 2,851**, without skips; separate startup reaches expected timeout
**124**. Independent legacy volume/bounds comparisons cover every native family, every prefix and
suppression/restore. Mixed proof covers reorder/closures, unaffected Bodies, failed suffixes,
imported boss/cut/Hole/Fillet/transform plus STEP readback, immutable Reference scopes, assembly
analysis/posed export, changed supplier bytes, scene ownership/Measure, Save/reopen and repair.
All **969** runtime fingerprints remain unchanged through final tests. Formatting/diff/source
review passes. Exact commands/logs/freeze digest live in `development-setup.md`.

Whole Block 147 is **Implemented and accepted**. Block 148 remains **Planned; not started** and owns
native suppression/reorder/End of Part browser interaction, prefix editing/insertion, Save during
marker preview and camera restoration. No planned UI capability is promoted by evaluator proof.
Complete normal PR delivery, confirm merge, refresh main and compare the reviewed tree before 148.

Delivery: [PR #147](https://github.com/GideonBa/Better-Linux-CAD/pull/147), implementation `9de9dd9`.
The delivery-record commit changes documentation only and preserves all 969 runtime fingerprints.
Normal merge, confirmed merged state and refreshed-main/reviewed-tree comparison are required before
starting Block 148.

### Block 148 serial implementation — 2026-11-07

PR #147 is confirmed merged as `6d8179d`. Refreshed main matches the exact reviewed branch tree;
`feat/history-browser-commands` starts from that verified state. Implemented-through remains 147.
The shared UI contract stays in `user-interface.md`; effective state/cache ownership stays in
`feature-system.md`/`shape-cache-mvp1-data-model.md`, with no second persistence plan.

| Slice | Bounded deliverable | Status |
| --- | --- | --- |
| 148A | Existing browser follows executable order and shows authored/effective status; staged suppression/restore with explicit dependent closure; validated reorder/drag/drop; context Edit uses the existing editor; full-source preview/Apply/Cancel, persistence and native interaction tests | Merged via PR #148 |
| 148B | Visibly labelled transient End of Part prefix, edit/insert at marker, complete suffix validation before one Apply, committed-only Save, disabled incomplete export/analysis and Cancel/camera/selection restoration | Verified; native integration delivered via PR #150 |
| 148C | H01–H03/UI01 combined native acceptance at 100%/150%, compact layout/captures, failure recovery and complete capability evidence | Delivered via PR #151 |

Only the whole completed/verified block advances numbered status or planned history UI capabilities.
A preview leaves source intent/history/files intact. Dependents are listed and included only by an
explicit choice; restore does not silently restore other flags. Illegal order or failed Geometry
leaves the original browser/model unchanged and the candidate correctable. Visibility controls
remain distinct from suppression. End of Part behavior is not supplied by A.

148A acceptance: full Geometry **574 / 51,745**; full offscreen GUI **697 cases: 663 passed,
34 expected native-only skips / 88,526 assertions**; native 100% combined commands **52 / 4,401**;
compact native 150% **7 / 299**, no native skips or failures. Separate startup reaches expected
timeout 124. A retained-Sketch selection regression is corrected and the existing combined Revolve
test passes unchanged. Core sources/tests remain unchanged from accepted 147B. All 975 runtime
fingerprints remain unchanged through final checks; exact evidence/commands and native captures
are recorded in `development-setup.md`/`user-interface.md`. Whole 148 and its capability promotion
remain open for B/C. Normal PR merge and refreshed-main/tree comparison precede B's branch.

Delivery: [PR #148](https://github.com/GideonBa/Better-Linux-CAD/pull/148), implementation `8d802d3`.
The delivery-record commit changes documentation only and preserves all 975 runtime fingerprints.

148B starts on `feat/end-of-part-context` after PR #148 is confirmed merged as `1179001` and
refreshed main matches the reviewed tree. Its session foundation extends the existing Part workflow
with transient prefix ownership, insertion at the marker, complete suffix publication, committed
Save and local draft Undo/Redo. It first has headless/API acceptance; native marker placement,
child-command coordination and shared existing-editor/creation integration follow serially before
B is accepted. This subdivision retains B's existing scope and does not promote a partial UI.

The 148B session foundation is implemented and verified: focused GUI/session regression **55 /
3,694**; full offscreen GUI **707 cases: 673 passed, 34 expected native-only skips / 89,135
assertions**; native session/existing-browser/source-Sketch combinations **36 / 2,241**, no native
skips or failures; separate startup expected timeout 124. All 976 runtime fingerprints remain
unchanged. Core/Geometry sources/tests are unchanged and their prior full results are retained,
not newly claimed. Exact scope/commands are in `development-setup.md`; native marker/child-command
work remains specified in `user-interface.md`. This accepts the session foundation only, not B or
whole 148. Protected PR delivery and verified refreshed main precede its native integration branch.

Session-foundation delivery: [PR #149](https://github.com/GideonBa/Better-Linux-CAD/pull/149),
implementation `9ec2bc2`. The delivery-record update changes documentation only and preserves all
976 runtime fingerprints. Normal merge/confirmed state and reviewed-tree comparison remain
required before native marker integration starts.

Native integration starts on `feat/end-of-part-browser` from verified merged main `ec1e42e`
(PR #149), after exact reviewed-tree comparison. It adds explicitly qualified prefix scene/input
consumers, browser marker/context controls, a fixed visible root context and existing child editors/
authoring tools. The existing outer Part checkpoint remains the complete Save/publication authority;
child checkpoints bound local Undo and cannot publish the parent. Full suffix/native H02/H03/UI01
acceptance is still required before B/C or the whole block is marked complete.

148B native integration is implemented and verified: full Geometry **575 / 51,871**, full offscreen
GUI **716 cases: 680 passed, 36 expected native-only skips / 89,560 assertions**, native 100%
combinations **67 / 5,441**, compact native 150% history/prefix cases **12 / 673**, no native skips
or failures. Focused checks and retained raw UI captures are recorded in the existing setup/UI docs.
Core code/tests are unchanged; its previous full result is retained. Whole 148 remains in progress
and implemented-through remains 147; combined H01–H03/UI01 acceptance and capability promotion
remain 148C. Normal PR delivery, confirmed merge and refreshed-main/tree comparison precede C.


148B native integration delivery: [PR #150](https://github.com/GideonBa/Better-Linux-CAD/pull/150),
implementation `92b8c38`. The delivery-record update changes documentation only and preserves all
976 runtime fingerprints. Confirm normal merge and compare refreshed main with the reviewed tree
before starting the 148C acceptance branch.


148C starts on `feat/history-workflow-acceptance` after PR #150 is confirmed merged as `266e064`
and refreshed main matches the exact reviewed tree. It adds combined suppress/reorder/prefix/edit/
restore/Save/reopen/exchange input evidence and parent cancellation while an existing child owns
input, including long names, folded browser state and selection/camera restoration. Native history
acceptance runs at requested 1280×800 ordinary / 1024×640 compact windows, bounded only by actual
available screen geometry, at 100%/150%. Core/Geometry regression evidence is retained where
runtime code is unchanged. Compact layout/folding corrections require renewed full GUI verification.
The capability manifest and numbered boundary advance only after the
combined acceptance and final inventory pass; no new history authority or feature editor is added.


148C final acceptance passes full offscreen GUI **719 cases: 682 passed, 37 expected native-only
skips / 89,823 assertions**, native 100% combinations **79 / 6,434**, native compact 150%
**15 / 1,047**, without native skips or failures. Separate startup reaches expected timeout 124.
The final correction gives completed history commands explicit Apply/Cancel status and retains
long-name/folded/compact behavior. All 976 runtime fingerprints remain unchanged through final
checks. Core/Geometry code/tests are unchanged in C; their accepted **627 / 23,727** and
**575 / 51,871** full results are retained without claiming new runs. Raw captures, commands and
source digest remain in the existing setup/UI docs.

Whole Block 148 is **Implemented and accepted** after H01–H03/UI01: staged browser history,
explicit dependent suppression, validated reorder, transient prefix edit/insert, correctable suffix
validation, committed Save, semantic scene/camera/history restoration and complete-only consumers.
The existing capability manifest now maps `feature-history` to actual commands/APIs and separate
Qt/native proofs; the final inventory validates that mapping and numbered boundary before delivery.
Implemented-through advances to 148 only. Block 149 is **Planned; not started**. Complete normal PR
delivery, confirm merge, refresh main and compare the reviewed tree before starting its branch.


Post-promotion inventory passes **5 / 568**, without skips or failures, and all 976 runtime
fingerprints remain unchanged after final contract/capture updates. Exact validation is recorded in
`development-setup.md`. Whole-148 acceptance is complete; delivery is pending the normal protected PR.


148C delivery: [PR #151](https://github.com/GideonBa/Better-Linux-CAD/pull/151), implementation
`59eeab1`. The delivery-record update changes documentation only and preserves all 976 runtime
fingerprints. Confirm normal merge and compare refreshed main with the reviewed tree before 149.


### Block 149 serial implementation — 2026-11-07

PR #151 is confirmed merged as `d9f229b`. Refreshed main matches the reviewed tree;
`feat/engineering-resource-foundation` starts from that verified state. Implemented-through remains
148. `engineering-modules.md` owns the shared material/standards resource contract; `file-format.md`
owns its subsequent persistence, and `parameter-model.md` records dimensional boundaries. This is
one common resource store for later Hole/Thread tables, not independent per-module catalogs.

| Slice | Bounded deliverable | Status |
| --- | --- | --- |
| 149A | Typed density/engineering pressure, per-value provenance, immutable Material and typed standards-table revisions, duplicate/conflict rules and atomic catalog merge; synthetic Core tests | Delivered via PR #152 |
| 149B | Standalone Part snapshots, Part default/Body overrides, Project resource deduplication, strict versioned JSON and offline/legacy compatibility | Delivered via PR #153/#154 |
| 149B1 | Shared strict resource JSON and standalone Part version 3 with material pins; preserve history/result-face loading; conservative Project-v1 serialization boundary | Delivered via PR #153 |
| 149B2 | Project resource ownership/deduplication, versioned Project persistence and offline member portability; replace B1's temporary project boundary | Delivered via PR #154 |
| 149C | Explicit coordinated revision updates, referenced-resource removal guards and atomic multi-document rollback/freshness evidence through existing mutation authorities | Implemented and verified; PR #155 |

149A adds no document assignment, writer/schema, material UI, mass query or capability promotion.
B specifies exact migration/JSON fields before its writer; C verifies whole-149 integration. The
existing materials/physical-properties UI stays assigned to 151. Whole 149 advances only after
all slices and R01 acceptance; native UI verification belongs to its visible integration.


149A is implemented and verified: focused Core **8 / 1,107**, full Core **635 / 24,834**, with no
failures or skips. All 981 runtime fingerprints remain unchanged through final checks; formatting
and source/diff review pass. Quantities remain typed, tables reject incompatible values before
integer conversion, and immutable revision/catalog merge failures preserve existing snapshots.
No Part/Project JSON, assignment, mass/UI or capability promotion is added. Geometry/GUI runtime
is unchanged and its preceding accepted results are retained without claiming reruns. Exact
commands/source digest are recorded in `development-setup.md`. Whole 149 remains in progress;
normal delivery/confirmed merge and refreshed-main/tree comparison precede 149B.


149A delivery: [PR #152](https://github.com/GideonBa/Better-Linux-CAD/pull/152), implementation
`830e449`. The delivery-record update changes documentation only and preserves all 981 runtime
fingerprints. Confirm normal merge and compare refreshed main with the reviewed tree before 149B.


149B1 starts on `feat/part-engineering-resources` from verified merged main `21bb116` (PR #152),
after exact reviewed-tree comparison. B is split serially because standalone Part and Project
resource ownership introduce separate persistence contracts. B1 adds Part pins and the shared
strict catalog codec, retaining resources through typed history replay and advanced support loading.
Project-v1 read/write explicitly rejects resource-bearing Parts until B2 supplies deduplicated
Project ownership/versioning; legacy resource-free Projects remain supported. Whole B/149 remain
open. `file-format.md` specifies version 3 before the writer; GUI/mass capability remains pending.

149B1 is implemented and verified: full Core **643 / 25,070**, Geometry **576 / 51,923**,
offscreen GUI **684 passed / 37 native-only skips / 90,001 assertions**, native 100%
**81 / 6,615** and compact native 150% **16 / 1,131**, without failures. Both native runs have no
skips; raw resource/history captures are visually reviewed. Exact commands, final runtime freeze
and startup/inventory evidence remain in `development-setup.md`. Shared typed resource JSON,
Part-v3 pins and history retention are accepted for this slice only. Project-v1 explicitly
rejects these members until B2 supplies shared ownership; C still owns coordinated updates.
Implemented-through stays 148, whole 149 remains in progress and material UI stays planned for 151.
Complete normal PR delivery/confirmed merge and refreshed-main/tree comparison before B2.

149B1 delivery: [PR #153](https://github.com/GideonBa/Better-Linux-CAD/pull/153), implementation
`4ff157f`. The delivery-record update changes documentation only and preserves all 990 runtime
fingerprints. Confirm normal merge and compare refreshed main with the reviewed tree before B2.

149B2 starts on `feat/project-engineering-resources` from verified merged main `49a2ed1`
(PR #153), after exact reviewed-tree comparison. `file-format.md` specifies Project V2 and scoped
member references before the writer change. The shared catalog is the single value authority;
member subsets remain portable through the standalone codec. Core conflict/legacy/malformed-input
tests and existing Project session history/save/reopen authorities supply this slice's proof.
Whole 149 remains in progress; revision updates/removal guards and material UI remain C/151.

149B2 is implemented and verified: full Core **649 / 25,309**, Geometry **576 / 51,923**,
offscreen GUI **686 passed / 37 native-only skips / 90,075 assertions**, native 100%
**51 / 4,514** and compact native 150% **15 / 858**, without failures. Native runs have no skips;
raw captures are visually reviewed. Shared ownership, strict scoped references and portable
standalone extraction are accepted for 149B. Exact commands/freeze/startup/inventory evidence
remain in `development-setup.md`. No material/mass UI or coordinated revision/removal operation
is inferred; whole 149 stays open with implemented-through 148. Complete normal PR delivery,
confirm merge and compare refreshed main with the reviewed tree before C.

149B2 delivery: [PR #154](https://github.com/GideonBa/Better-Linux-CAD/pull/154), implementation
`15650e6`. The delivery-record update changes documentation only and preserves all 994 runtime
fingerprints. Confirm normal merge and compare refreshed main with the reviewed tree before C.

149C starts on `feat/engineering-resource-updates` from verified merged main `5e17135`
(PR #154), after exact reviewed-tree comparison. The existing resource contract specifies explicit
candidate adoption/removal before implementation. This slice closes R01 Core/persistence/session
integration for whole 149 only after verification; material/mass UI remains 150/151. Implemented-
through stays 148 while C is in progress.

149C is implemented and verified: full Core **655 / 25,599**, Geometry **577 / 51,991**,
offscreen GUI **688 passed / 37 native-only skips / 90,150 assertions**, native 100%
**80 / 6,481** and compact native 150% **20 / 1,280**, without failures. Native runs have no skips;
raw captures are visually reviewed and separate startup reaches intentional timeout 124. Exact
commands/freeze/inventory/source review remain in `development-setup.md`.

Whole Block 149 is **Implemented and accepted** at the R01 backend gate: typed provenance-bearing
immutable revisions, default/Body pins, strict standalone/Project persistence, deduplicated insertion,
offline extraction, explicit selected-member adoption, guarded removal, atomic failure and existing
history/save/freshness integration. Implemented-through advances to 149 only. Physical mass and
material-only BRep reuse remain 150; visible Material/Physical Properties commands and R01's native
assignment UI remain 151. Their GUI capability stays planned. Phase A remains open while 150/151
are incomplete. Complete normal PR delivery/confirmed merge and refreshed-main/tree comparison
before starting 150; it remains **Planned; not started** here.

149C/whole-149 delivery: [PR #155](https://github.com/GideonBa/Better-Linux-CAD/pull/155),
implementation `a3321bc`. The delivery-record update changes documentation only and preserves
all 994 runtime fingerprints. Confirm normal merge and compare refreshed main with the reviewed
tree before 150.

### Block 150 serial implementation — 2026-11-07

PR #155 is confirmed merged as `3cbca99`; refreshed main matches the reviewed tree before
`feat/part-physical-properties` starts. Implemented-through stays 149. Existing physical/result
contracts are extended in `engineering-modules.md` and `gui-measure-mvp9.md`, not a parallel plan.

| Slice | Bounded deliverable | Status |
| --- | --- | --- |
| 150A | Analytic shape measures, stable central/principal tensors, density-aware complete Part query with immutable freshness, surface/empty/missing-density semantics and focused analytic/combination tests | Delivered via PR #156 |
| 150B | Explicit physical assembly participation, hidden active/repeated nested occurrences, posed centroid/tensor aggregation and independent analytic oracles | Delivered via PR #157 |
| 150C | Authoritative material-only reuse through existing cache/session owners, result invalidation and complete R02/combined acceptance | Implemented and verified; PR #158 |

No borrowed legacy cache is accepted as an A freshness certificate; its complete private evaluation
precedes later reuse. Whole 150 advances only after all slices and R02 acceptance. Native Material/
Physical Properties controls remain 151. No persistence version/field or new parameter kind is added.

150A is implemented and verified: new analytic/combination Geometry proof **10 / 634**, full Geometry
**587 / 52,625**, focused offscreen GUI **20 passed / 2 native-only skips / 1,503 assertions**,
native 100% regression **22 / 1,816** and native 150% Measure **7 / 477**, without failures in final
runs. Native runs have no skips, captures are reviewed and separate startup reaches timeout 124.
The initial large-coordinate cancellation, fixture-ID mistakes and existing native Inspect-navigation
defect, their corrections and source freezes are documented in `development-setup.md`; no assertion
is silently dropped. Core/full-GUI results are explicitly retained or not rerun, as recorded there.
Only A is accepted: complete Part snapshots, density-aware tensors/availability and actual source
freshness. Whole 150 remains in progress; assembly poses, material-only reuse and new UI remain B/C/151.
Implemented-through stays 149. Complete normal PR delivery/confirmed merge and refreshed-main/tree
comparison before B.

150A delivery: [PR #156](https://github.com/GideonBa/Better-Linux-CAD/pull/156), implementation
`ab57d37`. The delivery-record update changes documentation only and preserves all 1,000 final
runtime fingerprints. Confirm normal merge and compare refreshed main with the reviewed tree
before B.

150B starts on `feat/assembly-physical-properties` from verified merged main `25e2163`
(PR #156), after exact reviewed-tree comparison. Existing physical contracts specify explicit
participation, per-definition evaluation, root-frame pose math and immutable qualified results
before implementation. Whole 150 remains in progress, implemented-through 149; material-only
reuse and new UI remain C/151.

150B verifies the explicit physical leaf policy and immutable Project query with one complete
integration per participating Part definition. Hidden active paths participate; suppression and
unused definitions contribute nothing. Root-frame R·I·Rᵀ and parallel-axis totals match independent
repeated/nested box oracles. Pose/material/executable-lifetime changes and actual supplier-byte
changes invalidate results; broken active geometry, ambiguous ownership and unsynchronized
resources fail without partial publication. Partial mass retains its known-subset centroid.
Focused Core **4 / 201**, focused Geometry **16 / 843**, full Core **657 / 25,706** and full Geometry
**593 / 52,834** pass. Focused offscreen GUI **31 passed / 4 native-only skips / 2,510 assertions**,
native 100% **27 / 2,445**, native 150% **7 / 477**, final inventory **5 / 568** and intentional
startup timeout **124** pass. Runtime freeze **1,002** files, SHA-256
`ebdd82493f4a156d9b4ba4e0a46bbc475a1195d664aec67e2255147d9394fdc6` stays unchanged.
Commands, raw captures and validation scope are recorded in `development-setup.md`. Whole 150
and new Physical Properties UI remain unaccepted pending C/151.

Delivery: [PR #157](https://github.com/GideonBa/Better-Linux-CAD/pull/157), implementation `fd88b4e`.
The delivery-record commit changes documentation only; all 1,002 runtime fingerprints remain
unchanged. Confirm normal merge and exact reviewed-tree comparison on refreshed main before C.

150C starts on `feat/material-only-physical-reuse` from confirmed merged main `8a492df`
(PR #157), after exact reviewed-tree comparison. It adds immutable physical-result reweighting,
executor-owned complete geometry provenance and material-only cache refresh through the existing
session transaction owners. Geometry inputs, source lifetimes, prefix scope and actual supplier
bytes remain authoritative; unknown/changed geometry uses the existing recomputation path. Tables
remain geometry inputs for their assigned later consumers. Whole 150 stays in progress until
combined numerical/session/history/source acceptance completes; new controls remain 151.

150C completes strict material-only reweighting from immutable integrated snapshots and refreshes
executor-owned complete ShapeCache metadata without feature execution/BRep reconstruction. Actual
parameter, standard-table, lifetime, prefix, imported-byte and untrusted-cache changes cannot take
this path. Legacy unscoped execution remains compatible. Normal transaction copies preserve source
lifetimes only when canonical intent, evaluated values and Part graphs match strict reconstruction;
otherwise existing normalization remains in force. The expression-editor and injected-transform
regressions, corrections, negative proofs and source freezes are recorded in `development-setup.md`.

Whole Block 150 is **Implemented and accepted** at the R02 numerical/cache/session gate: final
focused Geometry **22 / 1,067**, focused GUI **29 passed / 1 native-only skip / 2,411 assertions**,
full Geometry **599 / 53,058**, full offscreen GUI **691 passed / 37 native-only skips / 90,279
assertions** across 728 cases, native 100% **61 / 4,483** and compact 150% **26 / 1,737** pass.
Native checks have no failures/skips, captures are reviewed, and separate startup reaches intentional
timeout **124**. Core's unchanged **657 / 25,706** result is retained, not rerun. Final runtime freeze
**1,005** files, SHA-256 `a82faa248dcab2180f1af4e33a116132a68a763f2a779fe6665a4705a59be29a`,
remains unchanged. Final inventory/source review and normal PR delivery precede the next branch.
Implemented-through advances to 150 only; 151 is **Planned; not started**. R01/R02 native Material/
Physical Properties controls, display units, centroid presentation and clipboard remain 151;
their GUI capability remains planned. Phase A remains open until 151 is accepted.

Final inventory passes **5 / 568**, without failures/skips. Working/staged source review, formatting
and whitespace checks complete delivery verification; all 1,005 runtime fingerprints remain unchanged.

150C/whole-150 delivery: [PR #158](https://github.com/GideonBa/Better-Linux-CAD/pull/158),
implementation `6fd158e`. The delivery-record update changes documentation only and preserves
all 1,005 runtime fingerprints. Confirm normal merge and exact reviewed-tree comparison on
refreshed main before starting 151.

### Block 151 serial implementation — 2026-11-07

PR #158 is confirmed merged as `7f27a87`; refreshed main matches reviewed tree
`066680b24c63ad29f238e1e289a8f4b43df987aa` before
`feat/material-physical-properties-controllers` starts. Implemented-through stays 150. Existing
resource/physical contracts remain in `engineering-modules.md`; `gui-measure-mvp9.md` owns read-only
physical presentation and `user-interface.md` owns command placement/layout. No competing resource,
occurrence, history or geometry authority is introduced.

| Slice | Bounded deliverable | Status |
| --- | --- | --- |
| 151A | Qt-free material candidate/transaction and physical-query controllers; exact source/scope guards, display units/clipboard data, qualified Body/occurrence totals and focused combined tests | Implemented and verified; PR #159 |
| 151B | Manage Material task, Part/Body assignment/context starts, immutable revision/provenance controls and explicit selected-member updates through existing session owners | Implemented and verified; PR #160 |
| 151C | Inspect Physical Properties task, scope/units/refresh/status/precision, clipboard and transient centroid presentation through existing viewport/task owners | Implemented and verified; delivery pending |
| 151D | R01–R02/UI01 native combined assignment/revision/history/Save/Open, nested/multi-Body/missing-density scopes, ordinary/compact/scaled layout and full acceptance | Planned; not started |

Material editing stages typed changes outside committed intent; identical intent creates no history
entry. Ready End-of-Part metadata edits use the existing child workflow. Physical Properties requires
complete fresh geometry and remains read-only: display units, scope, refresh, centroid visibility and
clipboard never author a model edit. Actual source/supplier changes invalidate results before Copy.
Existing Body Summary/supporting-geometry Measure behavior is preserved. The planned Materials GUI
capability stays planned until its actual commands/native proof exist; whole 151 and Phase A remain
open until all slices are accepted. Standard-table/Hole controls remain in their assigned later blocks.

151A implements disposable typed material proposals and metadata-only replay through the session,
including explicit-member revision/removal, no-op history protection and ready-prefix metadata.
The physical controller owns complete immutable queries, exact Body/repeated nested occurrence
scopes, read-only display-unit/density/tensor conversions, explicit refresh and guarded clipboard data.
Occurrence totals reuse Geometry aggregation. All 14 new controller cases are included in final
focused GUI **55 passed / 2 expected native-only skips / 3,840 assertions** across 57 cases;
full Geometry **599 / 53,058**, native 100% **67 / 4,795**, compact 150% **45 / 3,341** and
intentional native startup timeout **124** pass. Core **657 / 25,706** and the preceding 728-case
full-GUI result are explicitly retained, not rerun or claimed as new-UI acceptance. Exact commands,
the automatic-review timeout/retry and reviewed raw captures are recorded in `development-setup.md`.
Runtime freeze **1,011** files, SHA-256 `b2638b10a397fd1370ad58c7cd8f494db68783e229a1f9d0745fa1b9fa0acd2b`,
remains unchanged. Final inventory/source review and normal PR delivery precede B. Whole 151 and
Phase A stay in progress; Material/Physical Properties GUI capability stays planned until B–D.

Final inventory passes **5 / 568**, without failures/skips. Working/staged source review, full
new-file and modified-line formatting, whitespace checks and the unchanged runtime freeze complete
A delivery verification. No visible command/centroid/clipboard-OS acceptance is inferred from A.

151A delivery: [PR #159](https://github.com/GideonBa/Better-Linux-CAD/pull/159), implementation
`65708dc`. The delivery-record update changes documentation only and preserves all 1,011 verified
runtime fingerprints. Confirm normal merge and exact reviewed-tree comparison on refreshed main
before starting B.

151B starts on `feat/material-management-task` after PR #159 is confirmed merged as `956fdc2`.
Refreshed main matches reviewed tree `489592d1ecdcba75a239acfb59fe0c5c32650004`.
The Manage ribbon and Part/Body browser context share one Material task. Its scrollable property
groups stage catalogue snapshots, assignment and explicit-member adoption/removal through 151A;
Apply/Cancel stay visible outside the scroll area. Existing snapshots are read-only, changed values
require a new revision, and property provenance is disclosed/preserved when unchanged. Project
assignment targets a Part definition and therefore all its occurrences. Ready-prefix changes use
the existing child checkpoint. Native ordinary/compact/scaled interaction and combined persistence,
history, revision, provenance and Reference Part checks are required before B delivery. Whole 151
and the Materials capability remain open pending the remaining slices.

151B verification completes on the corrected label/layout source. Full GUI passes **750 cases:
713 passed / 37 expected native-only skips / 91,012 assertions**. Full Geometry passes **601 /
53,123**; later edits affect only Material label sizing and its GUI test. Final native 100% passes
**57 / 5,515**, compact 150% **33 / 3,276**, inventory **5 / 568** and intentional startup timeout
**124**. Core **657 / 25,706** is retained, not rerun. Eight Material cases cover pending Save,
one Undo/Redo, immutable/provenance validation, explicit Project members, duplicate local Body IDs,
Reference read-only/reuse, prefix child/parent publication and vertically reachable compact layout.
The Project viewport consumes complete native/member or separately validated Reference results,
including multiple visible Bodies; metadata-only Project library edits retain unchanged Reference
topology after actual supplier checks. Earlier fixture/Reference/layout failures, corrected scrolling
diagnostics, approval timeouts/retries and unmodified reviewed captures are recorded in the existing
setup/UI documents. Runtime freeze **1,014** files, SHA-256
`8313f8f2f66206da1303258bfa0d088d451fbcd583883e0da3163ae6270356bf`.
Working/staged formatting, whitespace/source review and normal PR delivery complete B only.
Implemented-through stays 150; whole 151 and Phase A remain open for C/D.

151B delivery: [PR #160](https://github.com/GideonBa/Better-Linux-CAD/pull/160), implementation
`f6ec440`. This delivery-record update changes documentation only and preserves all 1,014 verified
runtime fingerprints. Confirm normal merge and exact reviewed-tree comparison on refreshed main
before starting C.

151C starts on `feat/physical-properties-task` after PR #160 is confirmed merged as `8a4d9f5`.
Refreshed main matches reviewed tree `23bfd037ad6a2c4151c43c6fb2987e88f8828f4e`. Inspect exposes
one read-only Physical Properties task through the existing 151A controller. Scope and unit changes
are presentation only; Refresh is explicit. Stale model/Save/supplier state clears usable readout,
Copy and centroid presentation until refreshed. Complete, known-subset, unavailable and not-applicable
mass stay distinct, with separate volume/mass centroids, material density/revision, precision and
central/principal tensors. A transparent viewport child reprojects a labelled centroid triad from
canonical mm coordinates through the existing camera notification; no scene/selection authority,
model object or polling is added. Content scrolls vertically with bounded text and fixed Refresh/
Copy/Close actions. Native ordinary/compact/scaled clipboard, source-staleness, camera/centroid,
material/history/Save/Open and qualified nested/multi-Body scopes are required before C delivery.
Whole 151/Phase A remain open for D.

151C verification: full GUI **759 cases: 721 passed / 38 expected native-only skips / 91,503
assertions**; final native 100% **46 / 2,555**, compact 150% **46 / 2,563**; inventory **5 / 568**;
intentional startup timeout **124**. Initial focused combinations pass **33 / 1 expected native
skip / 1,337 assertions** across 34 cases. Core **657 / 25,706** and Geometry **601 / 53,123** are
retained without a rerun: C changes GUI code/tests only. Nine new cases cover actual ribbon and
clipboard, units/tensors/source provenance, qualified repeated scopes, Save/supplier staleness,
Material→known-subset mass→Undo, lost scopes, empty/Surface/unknown density, prefix/busy refusal,
independent native centroid pixels across units/camera and fully reachable compact widgets/footer.
Reviewed unmodified captures retain the fixed current mass, availability/participation and actions.
The exact-zero string failures and numeric-oracle correction, approval retry and compact summary
refinement are recorded in existing setup/UI docs. Final runtime freeze **1,017** files, SHA-256
`a6b40e029dd213d063930ada3585284496f16486a449b02fcd825150dcdd4e88`.
Source/new-file/working/staged formatting and review complete C delivery only. Implemented-through
stays 150; whole 151/Phase A and combined Materials GUI capability remain gated by D.

## Later phases

Interactive Part & Assembly Modeling MVP-9 is complete through Block 131. GUI Shell Reset MVP-9R
is complete through Block 136. STEP Import MVP-10 is complete through Block 143, including source
management, geometry inspection, semantic topology/recovery, immutable Reference Parts in
assemblies, EditableBody downstream modeling, structured STEP Projects and integrated end-to-end
acceptance in `docs/step-import-sequence-mvp10.md`. The next assigned program is the planned
advanced workflow sequence above; capabilities outside Blocks 145–204 remain unassigned.


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
docs. This acceptance follow-up is included in the combined PR integration recorded below;
the documented full-history costs and local target misses remain follow-up work.
No numbered implementation block is advanced.

PR integration (2026-10-29) resolves #116/#118 against current main and recovers the #121
acceptance follow-up that had been merged into an already-merged performance branch. The native
badge renderer retains both dimension interaction and dark theme colors. Full combined verification:
Core **549 / 17,217**, Geometry **459 / 35,243**, offscreen GUI **569 passed / 20 native-only
skips / 80,332 assertions**, native affected workflows and required shell coverage **136 / 29,127**.
After a test-only focus/screen-fit adjustment, dimension input passes offscreen **7 / 1,618** and
natively **7 / 1,739**; final 150% UI/dimension proof passes **16 / 3,209**, without skips.
All five targets build and separate native startup reaches timeout 124. Existing contracts and
`development-setup.md` record the exact boundaries; no timing profile was rerun and the known
40-blade full Apply/Undo/Redo cost remains about **5–6 s**. Block 137 remains complete and
Block 138 remains planned.
This last paragraph records the initial integration state; the #114 transaction/history completion
above supersedes its Apply/Undo/Redo measurements without advancing any numbered block.

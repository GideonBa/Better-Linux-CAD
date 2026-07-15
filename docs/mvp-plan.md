---
doc: MVP Plan
role: >-
  Implementation-sequence source of truth. Feature-specific documents remain
  canonical for exact contracts, formulas, persistence details, failure
  policies, ordering, and focused proofs.
implemented_through: Block 107
current_block: 108
current_boundary: Shared planar point/entity topology, mutation commands, JSON migration, and undo semantics
current_tag: "[core][sketch-topology]"
phase_status:
  mvp_1: "Single-part modeling — implemented"
  mvp_2: "Semantic references and richer sketch workflows — implemented"
  mvp_3: "Parametric bolt circle pattern — implemented"
  mvp_4: "Assembly parameters and Project container — implemented"
  mvp_5: "Assembly relationships, motion, hierarchy, analysis, exchange — Blocks 1–47 implemented"
  mvp_6: "Part Construction — Blocks 48–94 implemented; MVP complete"
  mvp_7: "GUI Feature Validation — Blocks 95–105 implemented; MVP complete"
  mvp_8: "Interactive Sketcher — Blocks 106–107 implemented; Blocks 108–121 planned; Block 108 next"
  mvp_9: "Interactive Part & Assembly Modeling — Blocks 122–131 planned after Interactive Sketcher acceptance"
  mvp_10: "STEP Import — Blocks 132–138 planned after Interactive Modeling acceptance"
---

# MVP Plan

This document is the numbered implementation-sequence source of truth for BLCAD.

Exact feature semantics are intentionally not duplicated here. Before implementing one block, read
the block's canonical sequence and the feature-specific authorities named there. If current code and
a planning document disagree, verify the current implementation and update the affected canonical
document explicitly; do not silently invent a capability inside a later block.

## Current status

```text
implemented through  Block 107
current block        Block 108
current phase        Interactive Sketcher MVP-8
current boundary     shared planar point/entity topology and editable Core commands
```

Block 107 is implemented. Block 108 is the current next technical step.

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

Canonical phase sequences are:

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

The exact Assembly sequence and per-block contracts remain in:

- `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`
- `docs/assembly-general-geometric-target-roadmap.md`
- `docs/assembly-geometric-target-taxonomy-mvp5.md`
- `docs/assembly-reference-geometry-intent-mvp5.md`
- `docs/assembly-generated-topology-reference-mvp5.md`
- `docs/assembly-generic-relationship-intent-mvp5.md`
- `docs/assembly-generic-relationship-equations-mvp5.md`
- `docs/assembly-joint-target-compatibility-mvp5.md`
- `docs/assembly-joint-coordinate-model-mvp5.md`
- `docs/assembly-joint-coordinate-json-mvp5.md`
- `docs/assembly-vector-joint-drive-mvp5.md`

Assembly model identity remains semantic. Raw OCCT topology, XDE labels, STEP entity ids, and memory
addresses are never persistent target identity.

## Implemented Blocks 48–94 — Part Construction MVP-6

Blocks 48–94 implement broad multi-body Part construction. The sequence establishes:

```text
Body identity and visibility
Body-scoped recompute and inspection
NewBody / Join / Cut / Intersect result intent
Body Booleans
BodyTransform and SketchOwnership
reusable typed Part-feature input references
rich Extrude / ExtrudedCut extent, taper, thin, and path control
Revolve / RevolveCut
Linear / Circular Pattern
MirrorFeature
Fillet / Chamfer
Shell / Draft
Sketch3D points and richer curves
PathCurve
Sweep / SweepCut / SweepSurface
guided and twist-controlled Sweep
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

The phase is accepted through Block 94. Later GUI phases must consume these authorities rather than
reimplementing feature mathematics in widgets.

## Implemented Blocks 95–105 — GUI Feature Validation MVP-7

Blocks 95–105 implement and accept the optional Qt 6 application layer over the existing Core and
Geometry authorities.

Implemented GUI boundaries include:

```text
Qt application shell and CAD regions
GuiDocumentSession
GuiTaskState and command enablement
open / save / save-as / dirty handling
document transactions and exact undo/redo
recompute and structured diagnostics
OCCT viewport and navigation
stable semantic viewport picking
deterministic model / assembly browser
property/task editing through existing Core mutation paths
bidirectional semantic selection synchronization
planar Sketch validation/edit/repair workbench
parameter, Body, Extrude/Cut, Revolve, hole workflows
Pattern, Mirror, finishing, Shell, Draft workflows
Body Boolean / Transform workflows
Sketch3D / PathCurve / Sweep / Loft / Surface workflows
complete Assembly authoring, solve, motion, analysis, and STEP export
machine-checked feature coverage and GUI/headless equivalence
```

Canonical sequence: `docs/gui-feature-validation-sequence-mvp7.md`.

Block 105 closes this phase. Qt remains a client; persistent model, solver, Geometry, and exchange
authority do not move into widgets.

## Interactive Sketcher MVP-8 — Blocks 106–121

Canonical sequence: `docs/interactive-sketcher-sequence-mvp8.md`.

The product target is a productive planar and 3D Sketch environment with direct manipulation,
visible constraints, in-canvas dimensions, deterministic snapping, solver-backed dragging, complete
creation/modify workflows, profile recognition, repair, and GUI/headless equivalence.

Frozen order:

```text
106 Sketch workspace, interaction state, command HUD, and usability contract — implemented
107 plane mapping, hit testing, box selection, grid, snapping, inference preview — implemented
108 shared planar point/entity topology, mutation commands, JSON migration, undo semantics — next
109 deterministic planar constraint solver, DOF accounting, conflicts, diagnostics
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

### Block 106 — Sketch workspace and interaction contract — Implemented

Block 106 promotes planar Sketch editing to a real transient `GuiWorkspace::Sketch` and freezes the
Sketch-specific interaction lifecycle over the existing generic task authority.

Implemented interaction stages are:

```text
Idle
Hover
CollectingPicks
NumericInput
Preview
SelectedHandle
DragCandidate
```

`Esc` backs out one stage and never silently commits. Enter/Finish Sketch capture and restore
workspace, semantic selection, viewport filter, and full camera state. The Sketch surface exposes:

```text
Create | Constrain | Dimension | Modify | Project
Entities | Constraints | Dimensions | Diagnostics
```

The first numeric-HUD client validates a line on a disposable Sketch copy before Preview and commits
through the existing atomic Part transaction path.

Canonical contract: `docs/gui-interactive-sketch-workspace-mvp8.md`.

Focused tags:

```text
[gui][sketch-workspace]
[gui][sketch-command-lifecycle]
```

### Block 107 — Plane interaction, hit testing, snapping, and inference — Implemented

Block 107 adds one device-independent mapping between:

```text
Qt screen position in DIP
view ray in model space
active workplane coordinates
model-space point
```

`GuiSketchInteractionSceneBuilder` derives a transient read-only interaction projection from the
current persistent Sketch. Current scene products cover authored lines, arcs, splines, supported
profile circles/rectangles/hole patterns, projected/reference-driven geometry where existing
Geometry resolution succeeds, dimension anchors, constraint anchors, and deterministic sampled
curve intersections.

`GuiSketchInteractionController` implements zoom-stable DIP tolerances and the frozen hit priority:

```text
Point -> Curve -> Dimension -> Glyph
```

Repeated click cycles the stable overlapping-hit stack. Empty-space drag uses:

```text
left -> right   Window
right -> left   Crossing
```

Implemented snap/inference families are:

```text
Origin
Axis
Endpoint
Midpoint
Center
Quadrant
Intersection
Nearest
Grid
HorizontalInference
VerticalInference
AlignmentX
AlignmentY
```

Eligible snap candidates are ordered by model-space distance, screen-space distance, stable family
priority, and candidate id. The OCCT viewport renders transient grid, hover, snap-marker, and
Window/Crossing rubber-band products through one transparent overlay. A shell binder publishes raw
plane cursor coordinates, snap/inference text, and stable semantic Sketch selection into the
existing Block-106 status/session surfaces.

Block 107 adds no JSON field and no persistent sampled topology.

Canonical contract: `docs/gui-sketch-plane-interaction-mvp8.md`.

Focused tags:

```text
[gui][sketch-hit-test]
[gui][sketch-snap]
[gui][sketch-box-selection]
```

## Current next technical step — Block 108

Block 108 owns the persistent planar Sketch topology migration required before a real general solver
or mouse drag can be correct.

Implement stable shared planar point identity so connected entities may reference the same point
rather than relying on equal floating-point coordinates. Lines, arcs, splines, and profile intent
must use deterministic point/entity references where the new contract requires them.

Add headless editable Core commands for:

```text
add
move
replace
remove
```

Deletion must be dependency-safe. Construction/reference state must be explicit. Canonical ordering
must be deterministic. Existing Block-99/107 Sketch JSON must load through an explicit migration;
save/load must not depend on GUI session state. Migration should preserve current generated geometry
and semantic entity ids where possible and report unavoidable identity changes.

Undo/redo must remain exact through the existing document transaction/history authority.

Existing authority to read before implementation:

- `docs/sketch-mvp1-data-model.md`
- `docs/general-closed-sketch-profile-mvp.md`
- `docs/arc-and-trim-extend-sketch-profile-mvp.md`
- `docs/spline-and-tangent-continuity-mvp.md`
- `docs/construction-geometry-mvp.md`
- `docs/semantic-references.md`
- `docs/file-format.md`
- `docs/gui-sketch-plane-interaction-mvp8.md`

Focused tags:

```text
[core][sketch-topology]
[core][sketch-edit-command]
[core][sketch-json-migration]
```

## Block 109 — General planar constraint solver

Add a deterministic headless nonlinear solver over planar point and curve parameters. Freeze variable
ordering, scale normalization, tolerances, iteration limits, convergence reporting, and stable
conflict attribution.

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
horizontal / vertical / aligned distance
radial / diameter / angular dimensions
```

Report:

```text
fully constrained
under constrained (remaining DOF)
redundant
conflicting
non-convergent
invalid reference
```

Focused tags: `[core][sketch-solver]`, `[core][sketch-dof]`,
`[core][sketch-conflict-diagnostics]`.

## Block 110 — Solver-backed mouse dragging

Expose endpoint, midpoint, center, radius, arc, spline, and dimension handles. Every pointer move asks
the Core solver for a disposable candidate. Canvas preview never changes the document. Release
commits one transaction; `Esc`, lost capture, or failed solve restores the pre-drag state.

Focused tags: `[gui][sketch-drag]`, `[integration][sketch-live-solve]`.

## Block 111 — Basic creation tools

Implement point, two-point line, continuous polyline, center/corner rectangle, three-point rectangle,
parallelogram, regular polygon, centerline, and construction geometry. Multi-click commands use the
Block-107 snap/inference layer and Block-109 solver authority.

Focused tags: `[gui][sketch-create-basic]`, `[integration][sketch-basic-profile]`.

## Block 112 — Circles, arcs, ellipses, and slots

Add the required persistent Core/Geometry intent and interactive creation/editing for supported
circle, arc, ellipse/elliptical-arc, and slot construction families. Full circles become real curve
entities where required rather than nearly closed arcs.

Focused tags: `[core][sketch-conics]`, `[geometry][sketch-conics]`,
`[gui][sketch-create-conics]`.

## Block 113 — Spline editing and Sketch text

Add fit/control-point spline editing, control polygons, continuity handles, supported representation
conversion, and parameter/expression-backed Sketch text with deterministic font fallback diagnostics.

Focused tags: `[core][sketch-spline-edit]`, `[geometry][sketch-spline-edit]`,
`[gui][sketch-spline]`, `[core][sketch-text]`, `[geometry][sketch-text]`.

## Block 114 — Constraint authoring and inference UX

Expose every Block-109 constraint through selection-driven commands and constraint glyphs. Creation
tools preview automatic constraint candidates; conflicting additions preview the conflict set and do
not mutate the document.

Focused tags: `[gui][sketch-constraints]`, `[integration][sketch-auto-constraint]`.

## Block 115 — Dimensions and expressions

Add horizontal, vertical, aligned, point-to-point, length, radius, diameter, angle, and arc-length
dimensions plus driven/reference mode, in-canvas editing, and parameter/expression binding.

Focused tags: `[core][sketch-dimensions]`, `[gui][sketch-dimensions]`,
`[integration][sketch-expression-edit]`.

## Block 116 — Sketch modify tools

Implement trim, extend, split, two-entity Sketch fillet, and Sketch chamfer over candidate topology.
Ambiguous intersections or reference geometry never partially mutate the Sketch.

Focused tags: `[core][sketch-modify]`, `[geometry][sketch-modify]`,
`[gui][sketch-trim-extend]`.

## Block 117 — Offset, projection, and construction references

Add chain/loop offset, supported associative projection, explicit break-link conversion, and the
matching lost/ambiguous reference workflow. Projected geometry remains read-only until explicitly
converted.

Focused tags: `[core][sketch-offset-project]`, `[geometry][sketch-offset-project]`,
`[gui][sketch-project]`.

## Block 118 — Transform, mirror, and Sketch patterns

Add move, rotate, scale, copy, mirror, rectangular pattern, and circular pattern for selected Sketch
geometry. Associative pattern intent must be explicit; exploded copies are ordinary entities.

Focused tags: `[core][sketch-transform-pattern]`, `[gui][sketch-transform-pattern]`.

## Block 119 — Regions, profiles, diagnostics, and Finish Sketch

Continuously recognize bounded regions from solved visible non-construction geometry, expose profile
selection and open/self-crossing diagnostics, and complete the solver-aware Finish Sketch workflow.

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

## Interactive Part & Assembly Modeling MVP-9 — Blocks 122–131

Canonical sequence: `docs/interactive-modeling-sequence-mvp9.md`.

Frozen order:

```text
122 modeling workspace, preselection command start, mini-toolbar, navigation aids
123 transient viewport manipulator infrastructure and numeric coupling
124 interactive Extrude, path Extrude, and Revolve authoring
125 interactive Fillet, Chamfer, Shell, and Draft authoring
126 interactive Pattern, Mirror, Body Boolean, and Body Transform authoring
127 interactive PathCurve, Sweep, and Loft authoring
128 interactive Surface authoring and surface-to-solid conversion
129 feature edit lifecycle and Core feature-update command boundary
130 interactive Assembly placement, relationships, joints, and motion
131 measure tool, coverage manifest v2, and integrated acceptance
```

This phase is selection-first and manipulator-driven over the Block-94 feature authorities. Transient
manipulators and previews never become persistent model identity. New persistent intent is allowed
only where the block's canonical plan explicitly declares it.

## STEP Import MVP-10 — Blocks 132–138

Canonical sequence: `docs/step-import-sequence-mvp10.md`.

Frozen order:

```text
132 STEP source identity, import modes, JSON, and freshness
133 OCCT STEP/XDE reader and deterministic imported body definitions
134 stable imported topology identity, recovery, and target resolution
135 Reference Part integration with assemblies
136 EditableBody ImportedBodyFeature and downstream modeling
137 structured STEP assembly import
138 integrated import, refresh, edit, assembly, and re-export acceptance
```

The two explicit import modes are:

```text
Reference
EditableBody
```

Reference keeps the external STEP source as immutable geometry authority while allowing Assembly
placement/relationships/joints through stable semantic imported topology. EditableBody starts normal
BLCAD Part history at an immutable `ImportedBodyFeature`; subsequent supported BLCAD features may
consume its Body outputs. The phase never reconstructs foreign CAD history heuristically.

## Sequencing rules

1. Implement exactly the current block before advancing the status header.
2. Read the current block's Existing authority list in its canonical sequence before coding.
3. Do not merge adjacent blocks merely because they touch the same subsystem.
4. New persistent intent requires an explicit Core contract, JSON policy where applicable, and focused
   headless proof before GUI consumers depend on it.
5. Transient GUI state never becomes model identity to avoid a Core boundary.
6. A failed validation, solve, recompute, import, or Geometry operation must not publish partial
   persistent mutation.
7. Update this file, the current phase sequence, the feature-specific canonical document, relevant
   architecture/UI/setup summaries, and the README status after completing a numbered block.
8. Advance `implemented_through`, `current_block`, `current_boundary`, and `current_tag` only after the
   completed block implementation and proof are checked in.

## Current handoff

Block 107 is implemented. Block 108 is next.

Read `docs/interactive-sketcher-sequence-mvp8.md` and the Block-108 Existing authority set, then
implement shared persistent planar point/entity topology and editable Core commands before beginning
the general planar solver in Block 109.

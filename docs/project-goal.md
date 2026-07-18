# Project Goal

BLCAD is intended to become an independent parametric CAD system for Linux.

The long-term goal is not a thin STEP exporter and not a wrapper around final BRep geometry. BLCAD
owns model intent and uses OCCT/Open CASCADE as the computed geometry and exchange implementation
layer.

Persistent or explicitly authored model intent now includes substantial parts of:

- typed parameters and unit-aware expressions;
- planar Sketch/profile intent and canonical shared planar point/entity topology;
- explicit Sketch construction/reference state in the Block-108 topology schema;
- construction geometry, constraints, dimensions, and repair intent;
- parametric Part features, Body identity, and feature history;
- semantic generated geometry references and canonical generated-topology producer-role identities;
- dependency, invalidation, and recompute intent;
- assembly parameters and bindings;
- Part component occurrences and direct rigid placement;
- local geometric Assembly constraints and typed joint coordinates;
- Project-owned child assemblies and rigid subassembly occurrences;
- Project-level occurrence-qualified geometric constraints and joints.

Derived data includes OCCT shapes, generated-topology recovery results, hierarchy traversal,
transform-authority mappings, target source classifications/descriptors/capabilities, Assembly and
Sketch residuals/Jacobians, solve/motion results, planar Sketch variable order, Jacobian rank,
remaining DOF, conflict/redundancy diagnostics, freshness snapshots, exchange records, posed shapes,
contact classifications, motion samples, XDE labels, STEP entities, Sketch interaction samples, screen
mappings, hit stacks, grid lines, snap/inference candidates, legacy Sketch-topology migration reports,
PartDocument compatibility materialization candidates, modeling preselection capabilities, contextual
command recommendations, selection filters, repeat state, ViewCube/Home state, camera bookmarks,
manipulator handle descriptors, camera-ray mappings, fixed-DIP hit products, pointer measurements,
numeric-HUD text, and candidate parameter values. These are regenerable query/execution/presentation
products rather than primary model authority.

## Direction

The project grows through controlled headless vertical slices:

1. single-Part parametric model;
2. stable recompute graph;
3. generated-face workplanes and semantic references;
4. broader Sketch/profile/feature support;
5. Sketch constraints and diagnostics;
6. Assembly relationships, motion, hierarchy, posed geometry, analysis, structured exchange, and
   generic target architecture;
7. broad multi-body Part Construction;
8. GUI/application workflows and complete implemented-feature validation;
9. productive deterministic solver-backed Interactive Sketcher with direct manipulation;
10. selection-first Interactive Part/Surface/Assembly Modeling;
11. STEP Part/Assembly import as Reference or EditableBody;
12. engineering modules.

Phases 1–9 are implemented through accepted Interactive Sketcher Block 121. Interactive Part &
Assembly Modeling MVP-9 is active through Block 128:

```text
122 selection-first Part/Surface/Assembly workspace
    capability-exact contextual command start
    visible contextual mini-toolbar and repeat
    persistent-profile-only Finish Sketch handoff
    synchronized session/viewport selection filters
    ViewCube, captured Home, and session-local camera bookmarks
123 reusable transient viewport manipulators
    linear / angular / radial handles
    translate / rotate triads
    pattern count / spacing handles
    model-space mapping + fixed-DIP hit testing
    exact final release + numeric-HUD override
    candidate values only; no document mutation
```

Block 124 adds interactive Extrude, path Extrude, and Revolve authoring
(`docs/gui-interactive-extrude-revolve-mvp9.md`), Block 125 adds interactive Fillet, Chamfer,
Shell, and Draft authoring (`docs/gui-interactive-finishing-mvp9.md`), and Block 126 adds interactive
Pattern, Mirror, Body Boolean, and Body Transform authoring
(`docs/gui-interactive-pattern-body-mvp9.md`), and Block 127 adds interactive PathCurve, Sweep, and
Loft authoring (`docs/gui-interactive-path-sweep-loft-mvp9.md`), and Block 128 adds interactive Surface
authoring and surface-to-solid conversion (`docs/gui-interactive-surface-mvp9.md`). Block 129, the
feature edit lifecycle and Core feature-update commands, is the current next technical step. STEP
Import starts at Block 132 after Interactive Modeling acceptance.

Development rule:

```text
model intent
  -> serialization compatibility / explicit migration
  -> stable semantic source and topology identity
  -> deterministic derived connectivity/query semantics
  -> typed geometry and capability projection
  -> deterministic geometry / numeric execution
  -> complete validation / freshness / explicit application when mutation is required
  -> diagnostics, analysis, interaction, or exchange consumers
```

A feature should be narrow enough to prove through focused headless tests before UI or broad topology
support is added. When one feature crosses several authority boundaries, it is split into sequenced
implementation blocks instead of becoming one large solver, hierarchy, format, target, motion, or
interaction rewrite.

## Current technical phase

The completed Assembly sequence implements local and cross-hierarchy solving, five motion-joint
families through passive Spherical, rigid nested assembly hierarchy, posed geometry, static contact
classification, bounded Revolute sampling, flattened/structured STEP exchange, generic typed geometric
target capability projection, assembly-selectable reference geometry, stable generated Part-topology
identity, and persistent geometric relationship intent.

Blocks 48–94 implement broad multi-body Part Construction: stable Body identity and Body-scoped
recompute, Body Booleans/transforms, reusable typed Part-feature inputs, richer Extrude/Cut,
Revolve/RevolveCut, general Patterns, Mirror, Fillet, Chamfer, Shell, Draft, persistent Sketch3D curves,
PathCurve, Sweep, path Extrude, multi-section/guided Loft, Boundary/Fill Surface, Trim/Extend Surface,
Stitch/Knit/Sew shell, closed-shell-to-solid conversion, and deterministic visible Solid/Surface Body
STEP export.

Blocks 95–105 implement and accept the optional Qt application layer over those authorities. The GUI
owns session/command/task/selection and transient presentation state; Core and Geometry remain model,
solver, geometry, recompute, analysis, and exchange authorities.

Blocks 106–121 implement and accept Interactive Sketcher MVP-8:

```text
contextual Sketch workspace and command lifecycle
  -> device-independent plane interaction / hit / selection / grid / snap / inference
  -> stable shared SketchPointId and canonical SketchTopology
  -> deterministic migration from explicit historical profile connectivity
  -> distinct equal-coordinate point ids remain distinct without a topology relation
  -> dependency-safe Add / Move / Replace / Remove Core commands
  -> exact full-topology snapshot undo / redo
  -> blcad.sketch_topology.mvp8 persistence
  -> lossless compatibility application through PartDocument::update_sketch where representable
  -> canonical SketchConstraintSystem and stable constraint ordering
  -> canonical non-reference SketchPointId variable ordering, X then Y
  -> normalized residual families and central-difference Jacobian
  -> deterministic damped Gauss-Newton execution
  -> final Jacobian rank and exact local remaining DOF
  -> stable canonical redundancy attribution
  -> stable remove-one conflict attribution
  -> fully constrained / under constrained / redundant / conflicting / non-convergent / invalid reference
  -> stable semantic drag handles over persistent point/entity roles
  -> transient Coincident / Midpoint / Concentric / Radial drag equations
  -> latest-pointer coalescing and synchronous exact release flush
  -> live solved preview without PartDocument mutation
  -> rollback on Esc / lost capture / solve refusal
  -> one freshness-checked Drag sketch handle document transaction on release
  -> creation, constraints, dimensions, modify/project, transforms, patterns, regions, and Sketch3D
  -> machine-checked coverage, GUI/headless equivalence, atomicity, and measured performance
```

The canonical Block-108 topology is solver/direct-manipulation identity authority. The historical
`blcad.part_document.mvp1` Sketch records remain a compatibility carrier for current Geometry
consumers; they are not silently reinterpreted as if shared point ids had always existed.

Block 109 adds no opaque solved-coordinate cache. `SketchSolveResult`, solver variables, residuals,
Jacobian, rank, remaining DOF, iteration state, and conflict/redundancy diagnostics are derived. The
source topology is never mutated by `SketchConstraintSolver::solve(...)`.

Block 122 begins Interactive Part/Surface/Assembly Modeling without extending persistent model intent.
`GuiModelingWorkspace` combines one semantic selection with verified capabilities, starts the existing
`GuiTaskState`, and restores complete preselection on cancellation. `GuiModelingWorkspaceShellBinder`
renders the visible context tabs, filters, mini-toolbar, Repeat, ViewCube, Home, and camera-bookmark
controls.

Block 123 adds `GuiViewportManipulatorLayer` and `GuiViewportManipulatorShellBinder`. Stable
model-space handle frames map pointer rays to signed distances, radii, angles, triad coordinates, and
pattern values. Presentation and hit tolerances remain fixed in DIP; ties use stable handle ids. Typed
HUD values override dragging, and release evaluates the exact final pointer sample. The layer emits
candidate values only and cannot modify Core, Geometry, Part JSON, Project JSON, or sidecar state.

## Identity and authority distinctions

### Sketch point identity

```text
SketchPointId
  -> one canonical planar point record
  -> referenced by one or more topology entities
```

Equal floating-point coordinates are not connectivity identity. Legacy migration shares point ids only
where the historical Sketch already contains explicit ordered profile connectivity between supported
curve endpoints. It reports every endpoint-usage identity collapsed into the canonical shared point.

Two unrelated point ids may have identical coordinates. Block 109 can therefore model an explicit
`Coincident` relationship between logically distinct points instead of erasing one variable before
solving.

Block-107 screen coordinates, hover candidates, sampled polylines, hit-cycle positions, and snap
markers are never promoted to `SketchPointId`.

### Sketch solver identity

```text
constraint identity  = stable SketchSolverConstraint id
point target         = SketchPointId
entity target        = canonical SketchTopologyEntity id
variable identity    = (SketchPointId, X|Y)
```

Constraint insertion order is not solver order. `SketchConstraintSystem` sorts stable ids
lexicographically. Non-reference points are already canonical and each contributes X then Y.

A solver target is never an AIS owner, OCCT subshape, screen pixel, or interaction sample. Missing or
capability-incompatible targets report `invalid_reference`.

### Assembly semantic endpoint identity

```text
local
  (local ComponentInstanceId,
   semantic_reference)

cross-hierarchy
  (occurrence_path,
   local ComponentInstanceId,
   semantic_reference)
```

A semantic-reference string may represent a legacy feature role, a `ref:` reference-geometry source,
or a canonical `topo:` generated-topology producer-role identity.

### Persisted transform authority

```text
(assembly_document: DocumentId,
 local ComponentInstanceId)
```

### Generated Part-topology semantic source identity

```text
source feature producer
+ generated topology family
+ named semantic producer role
+ exact source profile identity where profile-derived
```

Pattern result-vector position is not persistent semantic identity. Raw OCCT hash/traversal/map
identity, XDE label tags, STEP entity ids, memory addresses, screen coordinates, and sampled Sketch
polylines are never promoted to semantic target identity.

### Structured exchange and static contact identity

```text
exchange component occurrence
  = (containing rooted assembly path,
     local ComponentInstanceId)

static contact pair
  = canonical ordered pair of exact rooted component exchange identities
```

Repeated rooted occurrences of one shared child assembly can be exchange/contact distinct while still
sharing one child-document component transform authority.

## Long-term scope

The system should eventually cover parametric Part modeling, robust constrained Sketches, datum and
generated-geometry Sketch support, stable semantic references/recovery, broad feature history,
Assembly constraints/joints, typed geometric target selection, nested motion, top-down design,
engineering assistants for standard mechanical domains, standard-parts/material libraries, STEP
import/export and STL export, richer analysis, technical drawings/BOMs, and later CAM/FEM coupling.

The aim is a modern engineering-oriented CAD system for Linux combining classic parametric CAD
concepts with explicit semantic model intent, deterministic headless behavior, and strong technical
assistants.

## Non-goals for the current phase

Interactive Modeling MVP-9 does not yet claim full production-grade GUI parity. Blocks 122–123
establish selection-first command start, navigation, reusable candidate-only manipulators, and numeric
coupling; Blocks 124–131 add family-specific Part/Surface/Assembly authoring, feature editing, motion
interaction, measure, and integrated acceptance in sequence.

The current phase does not introduce arbitrary raw OCCT topology as persistent identity, a second
Assembly transform authority, whole-subassembly solve variables, feature-history reorder, persistent
appearance, direct modeling, section views, general constrained assembly free-drag, a general physics
engine, full contact dynamics, or continuous collision detection without a proved continuous
algorithm. These are sequencing boundaries rather than permanent product exclusions.

## Documentation authority

`docs/mvp-plan.md` is the implementation-sequence source of truth.

`docs/interactive-sketcher-sequence-mvp8.md` is canonical for productive Sketch interaction.

`docs/interactive-modeling-sequence-mvp9.md` is canonical for Blocks 122–131.

`docs/gui-modeling-workspace-mvp9.md` is canonical for the Block-122 selection-first workspace,
contextual command, filter, Finish-Sketch handoff, and navigation contract.

`docs/gui-viewport-manipulators-mvp9.md` is canonical for Block-123 handle families, model-space
mapping, fixed-DIP presentation/hit testing, exact release, numeric coupling, and candidate-only
authority.

`docs/sketch-shared-topology-mvp8.md` is canonical for Block-108 planar point/entity identity,
migration, edit commands, topology JSON, and compatibility materialization.

`docs/sketch-planar-constraint-solver-mvp8.md` is canonical for Block-109 solver variable ordering,
residual families, normalization, numeric policy, DOF, conflict/redundancy attribution, and solve
states.

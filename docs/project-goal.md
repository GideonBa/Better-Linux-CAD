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
and PartDocument compatibility materialization candidates. These are regenerable query/execution
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
10. STEP Part/Assembly import as Reference or EditableBody;
11. engineering modules.

Phases 1–8 are implemented through GUI Feature Validation Block 105. Interactive Sketcher MVP-8 is in
progress with Blocks 106–110 implemented:

```text
106 contextual planar Sketch workspace and command lifecycle
107 device-independent plane interaction / hit / box selection / grid / snap / inference
108 stable shared SketchPointId / SketchTopology / migration / edit commands / topology persistence
109 deterministic general planar constraint solver / exact local DOF / conflict and redundancy output
110 semantic Sketch handles / solver-backed live drag / rollback / exact final sample / atomic release
```

Block 111 basic creation — point, line, polyline, rectangle families, parallelogram, regular
polygon, centerline, and construction geometry with rubber-band previews, numeric entry, and one
transaction per completed tool — is implemented (`docs/gui-sketch-basic-creation-mvp8.md`).
Blocks 112–115 are implemented: circle/arc/ellipse/slot creation
(`docs/gui-sketch-conic-slot-creation-mvp8.md`), spline editing plus parameter-backed Sketch text
(`docs/gui-sketch-spline-text-mvp8.md`), constraint authoring with glyphs and conflict preview
(`docs/gui-sketch-constraint-authoring-mvp8.md`), and driving/reference dimensions with
parameter/expression binding (`docs/gui-sketch-dimension-authoring-mvp8.md`). Block 116 is the
current next technical step and adds trim, extend, split, Sketch fillet, and Sketch chamfer.

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

Blocks 106–110 establish the implemented Interactive Sketcher foundation:

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
```

The canonical Block-108 topology is solver/direct-manipulation identity authority. The historical
`blcad.part_document.mvp1` Sketch records remain a compatibility carrier for current Geometry
consumers; they are not silently reinterpreted as if shared point ids had always existed.

Block 109 adds no opaque solved-coordinate cache. `SketchSolveResult`, solver variables, residuals,
Jacobian, rank, remaining DOF, iteration state, and conflict/redundancy diagnostics are derived. The
source topology is never mutated by `SketchConstraintSolver::solve(...)`.

The current next boundary is Block 111: basic creation commands over the implemented interaction,
topology, solver, and drag authorities. Creation must use explicit Core topology/edit commands and
ordinary points/lines/constraints rather than GUI-only composite primitives. Interactive
Sketcher continues through Block 121. Interactive Part/Surface/Assembly Modeling follows in Blocks
122–131, and STEP Part plus structured Assembly import follows in Blocks 132–138.

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

The current phase does not yet claim production-grade GUI parity. Blocks 106–110 establish workspace,
plane interaction, persistent shared Sketch topology, and deterministic solver foundations; Blocks
110–121 deliberately add direct manipulation, creation, constraint/dimension authoring, modify/project
workflows, regions, Sketch3D interaction, and acceptance in sequence.

The current phase also does not introduce arbitrary raw OCCT topology as persistent identity, a second
Assembly transform authority, whole-subassembly solve variables, a general physics engine, full
contact dynamics, or continuous collision detection without a proved continuous algorithm. These are
sequencing boundaries rather than permanent product exclusions.

## Documentation authority

`docs/mvp-plan.md` is the implementation-sequence source of truth.

`docs/interactive-sketcher-sequence-mvp8.md` is canonical for productive Sketch interaction.

`docs/sketch-shared-topology-mvp8.md` is canonical for Block-108 planar point/entity identity,
migration, edit commands, topology JSON, and compatibility materialization.

`docs/sketch-planar-constraint-solver-mvp8.md` is canonical for Block-109 solver variable ordering,
residual families, normalization, numeric policy, DOF, conflict/redundancy attribution, and solve
states.

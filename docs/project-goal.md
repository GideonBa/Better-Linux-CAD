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

Derived data includes OCCT shapes, generated-topology producer classification/recovery query results,
hierarchy traversal, transform-authority mappings, target source classifications/descriptors/
capabilities, residuals/Jacobians, solve/motion results, freshness snapshots, exchange records, posed
shapes, contact classifications, motion samples, XDE labels, STEP entities, Sketch interaction
samples, screen mappings, hit stacks, grid lines, snap/inference candidates, legacy Sketch-topology
migration reports, and PartDocument compatibility materialization candidates. These are regenerable
query/execution products rather than primary model authority.

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
9. productive solver-backed Interactive Sketcher with direct manipulation;
10. STEP Part/Assembly import as Reference or EditableBody;
11. engineering modules.

Phases 1–8 are implemented through GUI Feature Validation Block 105. Interactive Sketcher MVP-8 is
in progress with Blocks 106–108 implemented. Block 106 establishes the contextual planar Sketch
workspace and command lifecycle. Block 107 adds device-independent plane interaction, zoom-stable
hit testing, stacked-hit cycling, Window/Crossing selection, grid, snapping, and inference. Block 108
adds stable shared planar point/entity topology, deterministic migration from embedded `Point2`
Sketch intent, dependency-safe Core edit commands, exact topology undo/redo, and canonical topology
JSON. Block 109 is the current next technical step and owns the deterministic general planar
constraint solver.

Development rule:

```text
model intent
  -> serialization compatibility / explicit migration
  -> stable semantic source and topology identity
  -> deterministic derived connectivity/query semantics
  -> typed geometry and capability projection
  -> geometry / numeric execution
  -> complete freshness / explicit application when mutation is required
  -> diagnostics, analysis, or exchange consumers
```

A feature should be narrow enough to prove through focused headless tests before UI or broad topology
support is added. When one feature crosses several authority boundaries, it is split into sequenced
implementation blocks instead of becoming one large solver, hierarchy, format, target, motion, or
interaction rewrite.

## Current technical phase

The completed Assembly sequence implements local and cross-hierarchy solving, five motion-joint
families through passive Spherical, rigid nested assembly hierarchy, posed geometry, static contact
classification, bounded Revolute sampling, flattened/structured STEP exchange, generic typed
geometric target capability projection, assembly-selectable reference geometry, stable generated
Part-topology identity, and persistent Coincident/Parallel/Perpendicular relationship intent.

Blocks 48–94 implement broad multi-body Part Construction: stable Body identity and Body-scoped
recompute, Body Booleans and transforms, reusable typed Part-feature inputs, richer Extrude/Cut,
Revolve/RevolveCut, general Linear/Circular Pattern, Mirror, Fillet, Chamfer, Shell, Draft, persistent
Sketch3D curves, reusable PathCurve, spatial/twist/guide-controlled Sweep, path Extrude, multi-section
and guided Loft, Boundary/Fill Surface, Trim/Extend Surface, Stitch/Knit/Sew shell,
closed-shell-to-solid conversion, and deterministic visible Solid/Surface Body STEP export.

Blocks 95–105 implement and accept the optional Qt application layer over those authorities. The GUI
owns session/command/task/selection and transient presentation state; Core and Geometry remain the
model, solver, geometry, recompute, analysis, and exchange authorities.

Blocks 106–108 now establish the first Interactive Sketcher foundation:

```text
contextual Sketch workspace and command lifecycle
  -> device-independent plane interaction / hit / box selection / grid / snap / inference
  -> stable shared SketchPointId and canonical SketchTopology
  -> deterministic legacy Sketch migration with explicit identity-change reporting
  -> dependency-safe Add / Move / Replace / Remove Core commands
  -> exact full-topology snapshot undo / redo
  -> blcad.sketch_topology.mvp8 persistence
  -> lossless compatibility application through PartDocument::update_sketch where representable
```

The canonical Block-108 topology is the future solver/direct-manipulation identity authority. The
historical `blcad.part_document.mvp1` Sketch records remain a compatibility carrier for current
Geometry consumers; they are not silently reinterpreted as if shared point ids had always existed.

The current next boundary is Block 109: deterministic planar constraint solving, solver-variable
ordering, scale normalization, convergence policy, remaining-DOF accounting, and stable conflict/
redundancy/invalid-reference diagnostics. Interactive Sketcher continues through Block 121.
Interactive Part/Surface/Assembly Modeling follows in Blocks 122–131, and STEP Part plus structured
Assembly import follows in Blocks 132–138.

## Identity and authority distinctions

### Sketch point identity

```text
SketchPointId
  -> one canonical planar point record
  -> referenced by one or more topology entities
```

Equal floating-point coordinates are not the future solver identity model. Legacy migration may
collapse coincident same-flag point usages into one canonical point id and reports every collapsed
usage explicitly.

Block-107 screen coordinates, hover candidates, sampled polylines, hit-cycle positions, and snap
markers are never promoted to `SketchPointId`.

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

Repeated rooted occurrences of one shared child assembly can be exchange/contact distinct while
still sharing one child-document component transform authority.

## Long-term scope

The system should eventually cover parametric Part modeling, robust constrained Sketches, datum and
generated-geometry Sketch support, stable semantic references and recovery, broad feature history,
Assembly constraints and joints, typed geometric target selection, nested motion, top-down design,
engineering assistants for standard mechanical domains, standard-parts/material libraries, STEP
import/export and STL export, richer analysis, technical drawings/BOMs, and later CAM/FEM coupling.

The aim is a modern engineering-oriented CAD system for Linux combining classic parametric CAD
concepts with explicit semantic model intent, deterministic headless behavior, and strong technical
assistants.

## Non-goals for the current phase

The current phase does not yet claim production-grade GUI parity. Blocks 106–108 establish the
workspace, interaction, and persistent shared Sketch-topology foundations; Blocks 109–121 deliberately
add solver, direct manipulation, creation, constraints, dimensions, modify/project workflows,
regions, Sketch3D interaction, and acceptance in sequence.

The current phase also does not introduce arbitrary raw OCCT topology as persistent identity, a
second Assembly transform authority, whole-subassembly solve variables, a general physics engine,
full contact dynamics, or continuous collision detection without a proved continuous algorithm.
These are sequencing boundaries rather than permanent product exclusions.

## Documentation authority

`docs/mvp-plan.md` is the implementation-sequence source of truth.

`docs/interactive-sketcher-sequence-mvp8.md` is canonical for productive Sketch interaction.

`docs/gui-interactive-sketch-workspace-mvp8.md` is canonical for Block 106.

`docs/gui-sketch-plane-interaction-mvp8.md` is canonical for Block 107.

`docs/sketch-shared-topology-mvp8.md` is canonical for Block-108 shared point/entity topology,
migration, edit commands, topology persistence, and the lossless PartDocument compatibility bridge.

`docs/file-format.md` remains authority for the historical PartDocument/Project save schemas;
Block-108 topology persistence is defined in its feature-specific canonical document rather than by
silently changing the historical PartDocument schema.

`docs/interactive-modeling-sequence-mvp9.md` is canonical for interactive Part, Surface, and Assembly
modeling over the Block-94 feature families.

`docs/step-import-sequence-mvp10.md` is canonical for STEP Part and structured Assembly import.

`docs/architecture-summary.md` summarizes implemented architecture.

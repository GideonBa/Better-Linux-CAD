# Architecture Summary

This document condenses the implemented architecture and current target direction. Feature-specific documents remain canonical for exact contracts.

## Goal

BLCAD is an independent parametric CAD system for Linux. The model stores design intent rather than only final BRep geometry: parameters, sketches, features, dependencies, semantic references, construction geometry, assembly structure, joint/limit intent, rigid hierarchy intent, and later multi-body, path, surfacing, flexible motion, and engineering intent.

The long-term goal is recorded in `docs/project-goal.md`.

## Fundamental decisions

- do not fork FreeCAD;
- use OCCT as the geometry kernel;
- keep a custom CAD core above OCCT;
- treat OCCT shapes as computed cache data, not primary model intent;
- persist semantic model references, not raw OCCT topology ids;
- keep geometric assembly constraints separate from motion-joint intent;
- keep rigid hierarchy intent separate from derived traversal, composed placement, and geometry flattening;
- keep GUI code above the CAD core; UI state must not become geometry or solver authority.

## Layers

```text
user interface
command/application layer
parametric core
sketch/constraint layer
construction geometry and datum relations
multi-body part layer
3D sketch and surfacing layer
assembly relationship, motion, and hierarchy layer
engineering analysis modules
OCCT geometry kernel
file and exchange formats
```

## Part model

The implemented part path includes typed quantities/parameters, datum planes, multiple sketch/profile families, additive/subtractive extrude intent, dependency/invalidation/recompute planning, semantic references, derived workplanes, construction geometry, projected/reference-driven sketch geometry, reference recovery, sketch constraints/diagnostics/repair helpers, optional OCCT execution through `ShapeCache`, STEP export, and JSON model-intent serialization.

`CircularHolePattern` is parametric model intent and expands into per-hole cuts during geometry recompute.

## Project and assembly ownership

`Project` owns:

```text
one explicit root AssemblyDocument
project-owned child AssemblyDocument records
project-owned PartDocument records
```

`Project::assembly()` remains the root assembly API.

Child assembly documents are separately owned through `child_assembly_documents()`. Assembly document ids are unique across the root and child collection.

A child assembly document does not automatically occur in the rooted assembly tree. It becomes an occurrence only when a containing assembly stores a `SubassemblyInstance` referencing it.

The root assembly may not be referenced as a child. This preserves one explicit root identity.

## Part component occurrence intent

A `ComponentInstance` is a part occurrence with identity independent from part-document identity. Repeated component occurrences may reference one `PartDocument` while retaining independent placement/state.

Persisted component state is:

```text
visibility
suppression
grounding
RigidTransform
  translation_mm
  rotation_deg
```

Storage-level placement edits are explicit and solver-independent.

## Rigid subassembly occurrence intent

Canonical document: `docs/assembly-rigid-subassembly-nested-export-mvp5.md`.

Stable occurrence identity:

```text
SubassemblyInstanceId
```

A persistent `SubassemblyInstance` stores:

```text
id
name
referenced_assembly_document
visibility
suppression_state
RigidTransform
  translation_mm
  rotation_deg
```

A rigid subassembly occurrence has no grounding field and contributes no solver variables in the current seed. Its child assembly's current internal component transforms are treated as authored rigid pose at the parent boundary.

`AssemblyDocument` keeps part components and child assembly occurrences in separate collections:

```text
component_instances[]
subassembly_instances[]
assembly_constraints[]
assembly_joints[]
```

Adding or loading a child occurrence never changes child component transforms.

## Hierarchy validity

`Project` owns cross-assembly hierarchy validation.

Every `SubassemblyInstance` must reference a project-owned child assembly. Validation rejects:

- duplicate child assembly document ids;
- duplicate subassembly occurrence ids within one containing assembly;
- direct self-reference;
- references to the project root assembly;
- missing child assembly documents;
- direct or indirect cycles.

Cycle validation is deterministic depth-first validation over the root and all owned child assembly documents. Owned but currently unreferenced child graphs are still validated, preventing latent cycles from entering project state.

The document-reference graph, DFS active/completed sets, and parent links are derived and unpersisted.

## Deterministic hierarchy traversal

`AssemblyHierarchyTraversal` derives the rooted occurrence tree after full structure validation.

Traversal order is:

```text
root first
pre-order DFS
child SubassemblyInstance records lexicographically by id
```

Repeated occurrences of one child assembly document remain separate traversal entries.

Each `AssemblyHierarchyOccurrenceDescriptor` contains:

```text
assembly_document
parent_assembly_document
via_subassembly_instance
occurrence_path
parent_transforms_inner_to_outer
visible_path
active_path
```

For:

```text
root --outer--> child --inner--> grandchild
```

the grandchild descriptor has:

```text
occurrence_path = [outer, inner]
parent_transforms_inner_to_outer = [T_inner, T_outer]
```

These descriptors are regenerated for every consumer that needs them.

## Flattened visible-active leaf occurrences

`AssemblyLeafOccurrenceResolver` is the canonical hierarchy-to-leaf boundary for geometry consumers.

It consumes the deterministic hierarchy traversal and returns only visible active part-component leaves.

One `AssemblyLeafOccurrenceDescriptor` stores derived identity and placement context:

```text
assembly_document
subassembly_path
component_instance
referenced_part_document
transforms_inner_to_outer
```

Leaf order is deterministic:

```text
hierarchy pre-order
then local ComponentInstance id order
```

For leaf component transform `Tc`, inner parent `Ti`, and outer parent `To`:

```text
transforms_inner_to_outer = [Tc, Ti, To]
```

Hidden or suppressed subassembly occurrences remove their complete descendant subtree. Inside an active visible assembly occurrence, local component visibility and suppression still apply.

Flattened leaves, occurrence paths, and transform chains are derived and unpersisted.

## Current assembly object families

Implemented assembly-relevant types include:

- `PartDocument`, `AssemblyDocument`, `Project`;
- `ComponentInstance`, `SubassemblyInstance`, `RigidTransform`;
- `AssemblyHierarchyTraversal`, `AssemblyHierarchyOccurrenceDescriptor`;
- `AssemblyLeafOccurrenceResolver`, `AssemblyLeafOccurrenceDescriptor`;
- `AssemblyConstraintTarget`, `AssemblyConstraint`, `AssemblyConstraintGraph`;
- `AssemblyJoint`, `AssemblyJointLimits`, `AssemblyJointId`, `AssemblyJointGraph`;
- semantic face/axis/seating references and target resolvers;
- `AssemblyTransformEvaluator`, `AssemblyHierarchyTransformEvaluator`;
- Mate/Distance/Angle, Concentric, Insert, and Revolute residual descriptors/builders;
- private shared numeric relationship/Jacobian system and solve engine;
- `AssemblyRigidBodySolver`, `AssemblySolveResult`, `AssemblySolveResultApplier`;
- `AssemblyJointMotionSolver`, `AssemblyJointMotionResult`, `AssemblyJointMotionResultApplier`;
- `AssemblySolveDiagnosticsAnalyzer` and rank/DOF descriptors;
- `AssemblyStepExporter`, `AssemblyStepExportSummary`.

Planned assembly/analysis families include interference records, richer joints, flexible subassemblies, cross-hierarchy relationship semantics, motion studies, contact analysis, and component geometry instancing.

## Persistent geometric constraint intent

Canonical document: `docs/assembly-constraint-model-intent-mvp5.md`.

Persistent geometric relationship families are:

```text
Mate
Concentric
Distance
Insert
Angle
```

Every record preserves target A/B semantic strings and active/inactive state. Distance carries a positive length quantity. Angle carries a degree quantity. Mate, Concentric, and Insert are value-free relationship records.

Constraints remain local to the containing `AssemblyDocument`. Cross-hierarchy geometric target semantics are not implemented.

`AssemblyConstraintGraph` contains every local component as a node and every active local geometric constraint as one distinct edge. It excludes inactive constraints, preserves multi-edges, sorts lexicographically, and exposes deterministic connected groups for ordinary geometric solving.

## Persistent joint and limit intent

Canonical document: `docs/assembly-revolute-joint-motion-mvp5.md`.

Joint records are separate from geometric constraints.

The first family is:

```text
AssemblyJointType::Revolute
```

One persistent Revolute record stores identity, name, semantic target A/B, active/inactive state, lower/upper degree limits, and authored current coordinate.

The current seed uses the signed principal range `[-180°, 180°]`, requires `lower < upper`, and requires the coordinate to remain inside the limits. Motion requests outside the limits are rejected rather than clamped.

`AssemblyJointGraph` contains every local component as a node and every active local joint as one distinct lexicographically sorted edge.

Joints also remain local to one `AssemblyDocument`; cross-hierarchy joint semantics are deferred.

## Semantic target families

Generated planar faces:

```text
feature.<feature-id>.top|bottom|right|left|front|back
```

Primary circular-feature axis and seating target:

```text
feature.<feature-id>.axis
feature.<feature-id>.seat
```

The first `.axis`/`.seat` producer is one `SubtractiveExtrude` whose source sketch contains exactly one `CircleProfile` and one total profile.

The resolver preserves component, referenced part, source feature, and source profile identity. Raw OCCT topology ids are never persistent target identity.

Insert and Revolute reuse the `.seat` producer to derive the same primary axis and oriented seating frame.

## Rigid-transform evaluation

Persisted `RigidTransform` semantics are:

```text
translation: millimeters
rotation: degrees
positive rotation: right-hand rule
rotation type: active fixed-axis
application order: X, then Y, then Z
column-vector composition: Rz * Ry * Rx
```

Points/origins rotate then translate. Vectors, normals, and axis directions rotate only.

`AssemblyTransformEvaluator` evaluates one authored level.

`AssemblyHierarchyTransformEvaluator` evaluates a vector of authored transforms exactly inner-to-outer:

```text
[Tc, Ti, To]
point result = To(Ti(Tc(p)))
```

The hierarchy is not collapsed to a composed rotation and converted back to Euler angles. This avoids adding branch/gimbal representation semantics not present in the authored records.

The same level-by-level transform convention is used by nested posed shape export.

## Geometric residual families

### Mate

```text
normal_opposition    = nA + nB
signed_separation_mm = dot(oB - oA, nA)
```

### Distance

```text
normal_parallelism   = cross(nA, nB)
signed_separation_mm = dot(oB - oA, nA)
distance_residual_mm = signed_separation_mm - target_distance_mm
```

### Concentric

```text
direction_parallelism = cross(dA, dB)
axis_offset_mm         = cross(oB - oA, dA)
```

Equal and opposed axis directions are accepted.

### Insert

```text
direction_parallelism       = cross(dA, dB)
axis_offset_mm               = cross(oB - oA, dA)
signed_seating_separation_mm = dot(sB - sA, nA)
```

Target A defines signed seating direction.

### Angle

```text
angle_alignment = dot(nA, nB) - cos(target_angle_deg)
```

The current cosine seed is direction-symmetric and loses local rank at satisfied `0°` and `180°` targets.

## Revolute motion-drive residual

A signed Revolute coordinate requires a directed axis because target A defines positive rotation:

```text
direction_alignment = dA - dB
axis_offset_mm = cross(oB - oA, dA)
signed_seating_separation_mm = dot(sB - sA, nA)
```

Signed twist uses target A's directed axis and oriented seating-plane x axes:

```text
reference_cosine = dot(xA, xB)
reference_sine   = dot(dA, cross(xA, xB))

twist_alignment_sine   = sin(phi - target)
twist_alignment_cosine = cos(phi - target) - 1
```

The sine/cosine pair avoids an `atan2` branch cut.

One transient Revolute drive contributes nine scalar residual components. A regular satisfied one-free-body drive is covered as a `9 x 6` central finite-difference Jacobian of rank `6` because the requested coordinate temporarily fixes the persistent joint's motion DOF.

## Shared numeric relationship and solve path

The private numeric system consumes:

```text
constraint_ids[]
revolute_drives[]
```

Residual order is geometric constraint graph order followed by Revolute joint graph order.

Every free active component contributes:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

The private `detail::solve_numeric_relationships` engine implements central finite differences, damped Gauss-Newton normal equations, partial-pivot Gaussian elimination, damping escalation, backtracking line search, solve states, complete component snapshots, and free-active-component transform proposals.

`AssemblyRigidBodySolver` supplies geometric constraint ids only.

`AssemblyJointMotionSolver` supplies geometric constraint ids plus transient Revolute drives. There is no separate motion optimizer.

The solver and motion systems currently operate on the explicit root `project.assembly()` relationship graph. Rigid child assembly occurrences are not solver variables.

## Suppression and combined motion groups

Ordinary geometric solving removes suppressed components and constraints touching them from the active numeric subgroup while retaining complete solve snapshots for stale-result detection.

Motion derives a transitive local component closure over active geometric constraint and joint edges. The selected joint is driven to the requested coordinate; other active Revolute joints in the same active group are held at their persisted coordinates.

The selected joint itself requires active, non-suppressed endpoints.

## Solve and motion application boundaries

`AssemblyRigidBodySolver` mutates only private `Project` copies and returns `AssemblySolveResult`.

`AssemblySolveResultApplier` accepts only converged fresh results and applies component transform proposals atomically through another project copy.

`AssemblyJointMotionSolver::move` is read-only and returns the selected joint/request, complete driven-joint input snapshots, and an embedded `AssemblySolveResult`.

`AssemblyJointMotionResultApplier` validates all joint snapshots, applies the embedded component proposals, replaces the selected authored joint coordinate, and commits only after all operations succeed.

## Local Jacobian rank and remaining DOF

`AssemblySolveDiagnosticsAnalyzer` evaluates the same geometric finite-difference Jacobian used by ordinary solving.

```text
variable_count  = 6 * free_active_component_count
constrained_dof = rank(J)
remaining_dof   = variable_count - rank(J)
```

Proven regular one-free-body geometric results:

```text
Concentric: rank 4/6, remaining DOF 2
Insert:     rank 5/6, remaining DOF 1
Angle:      rank 1/6, remaining DOF 5 away from extremal targets
```

Persistent-joint null-space presentation without an explicit motion coordinate remains deferred.

## Nested posed assembly STEP export

Canonical documents:

- `docs/assembly-posed-step-export-mvp5.md`
- `docs/assembly-rigid-subassembly-nested-export-mvp5.md`

`AssemblyStepExporter` is a derived geometry consumer:

```text
const Project
  -> validate complete root/child assembly structure and cycles
  -> AssemblyHierarchyTraversal
  -> AssemblyLeafOccurrenceResolver
  -> visible active flattened leaf occurrences
  -> collect unique referenced leaf part ids
  -> sort part ids lexicographically
  -> recompute each referenced PartDocument once into one per-export ShapeCache
  -> reuse each cache across repeated component and child-assembly occurrences
  -> copy each leaf final shape
  -> apply leaf transforms_inner_to_outer level by level
  -> compose one OCCT compound
  -> existing StepExporter
  -> STEP file
```

Repeated occurrences of one child assembly remain distinct leaves with distinct subassembly paths and transform chains while sharing referenced part recompute caches.

Hidden or suppressed subassembly occurrences omit their complete subtree. Hidden/suppressed local components are also omitted.

The root-only export path is the degenerate one-root hierarchy and retains the original component export semantics.

Missing children, cycles, unresolved member/leaf parts, failed recomputes, missing final shapes, transform failures, empty visible-active leaf output, and STEP writer failures stop export.

The current exchange seed remains one geometric compound. Structured STEP product hierarchy, named occurrence products, and geometry instancing are deferred.

`blcad_export_posed_assembly` proves JSON -> optional root geometric solve/apply -> hierarchical leaf flatten -> posed compound export. `examples/nested_subassembly.blcad.project.json` covers repeated two-level child occurrences.

## Persistence boundary

Persisted model intent includes:

- project identity and one explicit root assembly;
- project-owned child assembly documents;
- project-owned part documents;
- parameters and bindings;
- component occurrence identity/state/grounding/transform;
- rigid subassembly occurrence identity, child assembly reference, visibility, suppression, and transform;
- geometric constraint identity, semantic targets, state, and explicit values;
- joint identity, family, semantic targets, state, limits, and authored coordinate.

Derived and unpersisted data includes:

- constraint graph connectivity;
- joint graph connectivity;
- combined motion groups;
- assembly document reference graph;
- hierarchy cycle traversal state;
- hierarchy parent links and traversal order;
- `AssemblyHierarchyOccurrenceDescriptor` values;
- occurrence paths and parent transform chains;
- `AssemblyLeafOccurrenceDescriptor` values and flattened leaf ordering;
- composed/evaluated hierarchy placements;
- resolved semantic geometry;
- residual descriptors and transient drive sets;
- numeric residuals, Jacobians, and normal equations;
- solve/motion results and snapshots;
- rank/DOF diagnostics;
- per-consumer `ShapeCache` values;
- posed leaf shape copies;
- OCCT compounds and STEP entity identity.

Only explicit application changes persisted component solver placement or a selected authored joint coordinate. Rigid subassembly placement/state changes only through explicit occurrence edits.

## Next assembly block

The next block is the first posed assembly interference-analysis seed.

It should consume `AssemblyLeafOccurrenceResolver` rather than reimplement hierarchy traversal. Unique referenced leaf parts should be recomputed once, leaves posed with the same transform-chain semantics as nested STEP export, and unordered leaf pairs derived once in deterministic occurrence-key order.

OCCT common/intersection geometry should classify only finite positive common solid volume above an explicit tolerance as interference. Face, edge, or point touching is not positive-volume interference in the seed.

The output should be read-only occurrence-pair interference records plus leaf/pair analysis counts. Pair candidates, posed shapes, common shapes, volume properties, and results remain derived and unpersisted.

Broad-phase acceleration, contact classification, minimum distance, penetration direction/depth, collision response, swept motion, and physics remain deferred.

## Broader future direction

Multi-body transforms/booleans and path features are documented in `docs/multi-body-transform-and-path-features-roadmap.md`.

Inventor-like sketch/feature parity is documented in `docs/inventor-like-sketcher-and-feature-roadmap.md`.

3D sketching and surfacing are documented in `docs/advanced-surfacing-and-3d-sketch-mvp.md`.

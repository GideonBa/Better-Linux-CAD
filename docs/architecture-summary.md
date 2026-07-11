# Architecture Summary

This document condenses the implemented architecture and current target direction. Feature-specific documents remain canonical for exact contracts.

## Goal

BLCAD is an independent parametric CAD system for Linux. The model stores design intent rather than only final BRep geometry: parameters, sketches, features, dependencies, semantic references, construction geometry, assembly structure, geometric relationships, joint/limit intent, rigid hierarchy placement, and explicit component transforms.

The long-term goal is recorded in `docs/project-goal.md`.

## Fundamental decisions

- do not fork FreeCAD;
- use OCCT as the geometry kernel;
- keep a custom CAD core above OCCT;
- treat OCCT shapes as computed cache data, not primary model intent;
- persist semantic model references, not raw OCCT topology ids;
- keep geometric assembly constraints separate from motion-joint intent;
- keep authored hierarchy transforms separate from derived traversal and composed evaluation;
- keep local assembly relationships separate from occurrence-qualified cross-hierarchy query semantics until a dedicated project-level persistent graph exists;
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

The implemented part path includes typed quantities/parameters, unit-aware parameter expressions, datum planes, multiple sketch/profile families, additive/subtractive extrude intent, dependency/invalidation/recompute planning, semantic references, derived workplanes, construction geometry, projected/reference-driven sketch geometry, reference recovery, sketch constraints/diagnostics/repair helpers, optional OCCT execution through `ShapeCache`, STEP export, and JSON model-intent serialization.

`CircularHolePattern` is persistent parametric model intent and expands into per-hole cuts during geometry recompute.

Expression parameters remain part-document local. Their formulas, dependency edges, topological re-evaluation, cycle rejection, and formula-edit edge replacement are documented in `docs/parameter-expression-mvp.md`.

## Project and assembly ownership

`Project` owns:

```text
one explicit root AssemblyDocument
project-owned child AssemblyDocument records
project-owned PartDocument records
```

`Project::assembly()` remains the explicit root assembly API.

Child assembly documents are separately owned through `child_assembly_documents()`. Assembly document ids are unique across the root and child collection.

A child assembly document does not automatically occur in the rooted assembly tree. It enters the rooted tree only through a `SubassemblyInstance` in a containing assembly.

The root assembly may not be referenced as a child. This preserves one explicit root identity.

## Part component occurrence intent

A `ComponentInstance` is a part occurrence with identity scoped to one `AssemblyDocument`.

Persisted state is:

```text
id
name
referenced_part_document
visibility
suppression_state
grounding_state
RigidTransform
  translation_mm
  rotation_deg
```

Repeated component occurrences may reference one part document while retaining independent local placement/state.

Storage-level placement edits are explicit and solver-independent.

## Rigid subassembly occurrence intent

Canonical document: `docs/assembly-rigid-subassembly-nested-export-mvp5.md`.

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

The occurrence transform is the authored rigid boundary between a containing assembly occurrence and the referenced child assembly document.

A `SubassemblyInstance` currently has no grounding field and is not itself a six-variable rigid-body solve node.

Adding or loading a child occurrence never changes child component transforms.

## Hierarchy validity

`Project` owns cross-document hierarchy validation.

Validation rejects:

- duplicate assembly document ids;
- duplicate subassembly occurrence ids within one containing assembly;
- direct self-reference;
- references to the project root assembly as a child;
- missing child assembly documents;
- direct or indirect assembly-reference cycles.

Owned but currently unreferenced child graphs are also validated so invalid latent cycles cannot enter project state.

The document-reference graph and DFS validation state are derived and unpersisted.

## Deterministic hierarchy traversal

`AssemblyHierarchyTraversal` derives the rooted occurrence tree after complete project structure validation.

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

the grandchild occurrence has:

```text
occurrence_path = [outer, inner]
parent_transforms_inner_to_outer = [T_inner, T_outer]
```

Traversal descriptors, occurrence paths, parent links, and transform chains are derived and unpersisted.

## Hierarchical rigid-transform semantics

Canonical convention for every authored `RigidTransform` level:

```text
translation: millimeters
rotation: degrees
positive rotation: right-hand rule
rotation type: active fixed-axis
application order: X, then Y, then Z
column-vector composition: Rz * Ry * Rx
```

Points rotate then translate. Vectors, normals, and directions rotate only.

`AssemblyTransformEvaluator` evaluates one authored level.

`AssemblyHierarchyTransformEvaluator` evaluates authored transform chains exactly in inner-to-outer order.

For component transform `Tc`, inner parent `Ti`, and outer parent `To`:

```text
chain = [Tc, Ti, To]
point result = To(Ti(Tc(p)))
```

The hierarchy is not collapsed into a persisted composed transform or converted back to Euler angles.

## Flattened visible-active leaf occurrences

`AssemblyLeafOccurrenceResolver` is the canonical hierarchy-to-leaf boundary for posed geometry consumers.

One `AssemblyLeafOccurrenceDescriptor` stores:

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

For leaf component transform `Tc` and parent chain `[Ti, To]`:

```text
transforms_inner_to_outer = [Tc, Ti, To]
```

Hidden or suppressed subassembly occurrences remove their complete descendant subtree. Local hidden/suppressed components are also absent from flattened leaves.

Flattened leaf descriptors remain derived and unpersisted.

## Local geometric constraint intent

Canonical document: `docs/assembly-constraint-model-intent-mvp5.md`.

Persistent local relationship families are:

```text
Mate
Concentric
Distance
Insert
Angle
```

Each `AssemblyConstraint` belongs to one `AssemblyDocument` and stores local component endpoints through `AssemblyConstraintTarget`:

```text
local ComponentInstanceId
semantic_reference
```

Distance stores a length quantity. Angle stores a degree quantity. Mate, Concentric, and Insert store no explicit value.

Local constraints remain local to their containing assembly document. Project-level cross-hierarchy constraint persistence is not yet implemented.

`AssemblyConstraintGraph` contains every local component as a node and every active local constraint as one distinct edge. It preserves multi-edges, sorts lexicographically, and exposes deterministic connected groups.

## Local joint and limit intent

Canonical document: `docs/assembly-revolute-joint-motion-mvp5.md`.

Joint records remain separate from geometric constraints.

The first family is:

```text
AssemblyJointType::Revolute
```

A persistent Revolute record stores identity, name, local semantic endpoints, active/inactive state, lower/upper degree limits, and authored current coordinate.

The current seed uses the signed principal range `[-180°, 180°]`, requires `lower < upper`, and rejects requested coordinates outside limits rather than clamping.

`AssemblyJointGraph` contains every local component as a node and every active local joint as one distinct lexicographically sorted edge.

Cross-hierarchy joint persistence and nested motion propagation remain deferred.

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

`AssemblyConstraintTargetResolver` is the local semantic target authority. It preserves component, referenced part, source feature, and source profile identity. Raw OCCT topology ids are never persistent target identity.

Insert and Revolute reuse the `.seat` producer to derive the same primary axis and oriented seating frame.

## Local geometric residual families

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

A signed Revolute coordinate requires a directed axis because target A defines positive rotation.

The drive reuses axis/seating semantics and adds periodic signed twist alignment:

```text
reference_cosine = dot(xA, xB)
reference_sine   = dot(dA, cross(xA, xB))

twist_alignment_sine   = sin(phi - target)
twist_alignment_cosine = cos(phi - target) - 1
```

The sine/cosine pair avoids an `atan2` branch cut.

One transient drive contributes nine scalar residual components. A regular satisfied one-free-body drive is covered as a `9 x 6` central finite-difference Jacobian of rank `6` because the requested coordinate temporarily fixes the joint motion DOF.

## Shared local numeric solve path

The private numeric system consumes:

```text
constraint_ids[]
revolute_drives[]
```

Every free active local component contributes:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

The private `detail::solve_numeric_relationships` engine implements central finite differences, damped Gauss-Newton normal equations, partial-pivot Gaussian elimination, damping escalation, backtracking line search, explicit solve states, complete component snapshots, and free-active-component transform proposals.

`AssemblyRigidBodySolver` supplies local geometric constraint ids.

`AssemblyJointMotionSolver` supplies local geometric constraint ids plus transient Revolute drives. There is no separate motion optimizer.

The ordinary numeric engine still uses the temporary solve view's `project.assembly()` as its local relationship graph.

## Suppression and combined motion groups

Ordinary local geometric solving removes suppressed components and constraints touching them from the active numeric subgroup while retaining complete solve snapshots for stale-result detection.

Motion derives a transitive local component closure over active geometric constraint and joint edges. The selected joint is driven to the requested coordinate; other active Revolute joints in the same active group are held at persisted coordinates.

The selected joint requires active, non-suppressed endpoints.

## Solve and motion application boundaries

`AssemblyRigidBodySolver` mutates only private `Project` copies and returns `AssemblySolveResult`.

`AssemblySolveResultApplier` accepts only converged fresh results and applies component transform proposals atomically through another project copy.

`AssemblyJointMotionSolver::move` is read-only and returns selected joint/request identity, complete driven-joint input snapshots, and an embedded `AssemblySolveResult`.

`AssemblyJointMotionResultApplier` validates all joint snapshots, applies embedded component proposals, replaces the selected authored coordinate, and commits only after all operations succeed.

## Local Jacobian rank and remaining DOF

`AssemblySolveDiagnosticsAnalyzer` evaluates the same geometric finite-difference Jacobian used by ordinary local solving.

```text
variable_count  = 6 * free_active_component_count
constrained_dof = rank(J)
remaining_dof   = variable_count - rank(J)
```

Proven regular one-free-body results:

```text
Concentric: rank 4/6, remaining DOF 2
Insert:     rank 5/6, remaining DOF 1
Angle:      rank 1/6, remaining DOF 5 away from extremal targets
```

Persistent-joint null-space presentation without an explicit motion coordinate remains deferred.

## Document-scoped flexible child solving

Canonical document: `docs/assembly-flexible-subassembly-solving-mvp5.md`.

`AssemblyFlexibleSubassemblySolver` selects one exact active non-root occurrence path.

The referenced child `AssemblyDocument` is copied into a temporary `Project` as the local root assembly together with project-owned parts. The existing local constraint graph, target resolver, numeric system, rigid-body solver, snapshots, and proposal contract are reused unchanged.

`AssemblyFlexibleSubassemblySolveResult` wraps:

```text
occurrence_path
assembly_document
local_result: AssemblySolveResult
```

Application rebuilds the hierarchy and child-local solve view, reuses `AssemblySolveResultApplier` stale validation, then atomically writes only successfully applied direct child component transforms back to the selected project-owned child document.

The selected `SubassemblyInstance::transform()` remains rigid and unchanged.

Repeated occurrences of one child document share the same solved internal component pose while retaining independent rigid boundary transforms.

This is document-scoped flexible solving, not occurrence-local internal pose overriding.

## Cross-hierarchy relationship target identity

Canonical document: `docs/assembly-cross-hierarchy-relationship-semantics-mvp5.md`.

The implemented read-only occurrence-qualified endpoint identity is:

```text
(occurrence_path, local ComponentInstanceId, semantic_reference)
```

`AssemblyHierarchyConstraintTarget` stores that identity.

The empty occurrence path addresses the explicit root assembly occurrence. Non-empty paths are exact root-to-current `SubassemblyInstanceId` sequences from `AssemblyHierarchyTraversal`.

This identity is required because a local `ComponentInstanceId` is scoped only to one assembly document. Repeated occurrences of one child document may expose the same local component id at different rooted paths.

## Cross-hierarchy target resolution

`AssemblyHierarchyConstraintTargetResolver` resolves a target as follows:

```text
Project
  -> AssemblyHierarchyTraversal::build
  -> exact occurrence_path match
  -> containing AssemblyDocument
  -> temporary local target-resolution view
  -> existing AssemblyConstraintTargetResolver
  -> local semantic geometry + component transform
  -> [component, inner parent, ..., outer parent] transform chain
  -> AssemblyHierarchyTransformEvaluator
  -> root-assembly-space target geometry
```

The existing local resolver remains the only authority for planar face, `.axis`, and `.seat` target semantics.

Resolved typed hierarchy target descriptors preserve occurrence path, containing assembly document, local component id, referenced part id, source feature/profile identity where applicable, semantic family identity, semantic-reference string, and root-space geometry.

Repeated child assembly occurrences therefore resolve through the same local model definition but different parent transform chains.

Visibility and suppression do not alter mathematical target resolution. Future occurrence-qualified graph/solver integration owns participation filtering.

## Cross-hierarchy read-only residual semantics

`AssemblyHierarchyConstraintQuery` is a derived query object. It reuses `AssemblyConstraintType` and the existing Distance/Angle value-family validation rules but is not inserted into any assembly constraint collection.

`AssemblyHierarchyConstraintEquationBuilder` resolves target A and target B into root-assembly space and reuses the canonical residual descriptor types:

```text
PlanarMateResidualDescriptor
PlanarDistanceResidualDescriptor
PlanarAngleResidualDescriptor
ConcentricResidualDescriptor
InsertResidualDescriptor
```

The formulas and target-order semantics are exactly the same as the local builders. Only endpoint identity and transform depth differ.

No persistent cross-hierarchy constraint, graph edge, solve variable, Jacobian row, snapshot, proposal, or result-application path is implemented yet.

## Nested posed assembly geometry

Canonical documents:

- `docs/assembly-posed-step-export-mvp5.md`
- `docs/assembly-rigid-subassembly-nested-export-mvp5.md`
- `docs/assembly-interference-analysis-mvp5.md`

`AssemblyPosedLeafShapeBuilder` and `AssemblyStepExporter` consume the canonical flattened leaf boundary.

The derived pipeline is:

```text
const Project
  -> validate complete root/child assembly structure and cycles
  -> AssemblyHierarchyTraversal
  -> AssemblyLeafOccurrenceResolver
  -> visible active flattened leaves
  -> collect unique referenced leaf part ids
  -> recompute each referenced PartDocument once into one per-consumer ShapeCache
  -> reuse each cache across repeated component and child-assembly occurrences
  -> copy and pose each leaf through exact authored transform-chain semantics
```

`AssemblyStepExporter` composes one OCCT compound and delegates to the existing STEP writer.

The current exchange seed is geometric, not a structured STEP product hierarchy. Named occurrence products and geometry instancing are deferred.

## Posed interference and clearance analysis

`AssemblyInterferenceAnalyzer` evaluates deterministic unordered posed-leaf pairs and classifies only finite positive common solid volume above explicit tolerance as interference.

Face, edge, or point touching without positive common volume is not interference.

`AssemblyClearanceAnalyzer` preserves interference records and evaluates OCCT minimum distance for remaining pairs. Distances below a finite positive threshold become clearance violations. Exact touching without positive volume is a zero-distance clearance violation.

Analysis products are read-only and unpersisted.

`blcad_analyze_assembly` exposes the combined report path headlessly.

## Persistence boundary

Persisted model intent includes:

- project identity and one explicit root assembly;
- project-owned child assembly documents;
- project-owned part documents;
- parameters, formulas, and bindings;
- component occurrence identity/state/grounding/transform;
- rigid subassembly occurrence identity, child assembly reference, visibility, suppression, and transform;
- local geometric constraint identity, semantic targets, state, and explicit values;
- local joint identity, family, semantic targets, state, limits, and authored coordinate.

Derived and unpersisted data includes:

- local constraint and joint graph connectivity;
- combined motion groups;
- assembly document reference graph and hierarchy cycle state;
- hierarchy parent links and traversal order;
- `AssemblyHierarchyOccurrenceDescriptor` values;
- occurrence paths and parent transform chains;
- `AssemblyLeafOccurrenceDescriptor` values and flattened leaf ordering;
- child-local temporary solve views;
- `AssemblyFlexibleSubassemblySolveResult` wrappers;
- `AssemblyHierarchyConstraintTarget` and `AssemblyHierarchyConstraintQuery` values;
- occurrence-qualified hierarchy target descriptors;
- composed/evaluated hierarchy placements;
- resolved semantic geometry;
- local and cross-hierarchy read-only residual descriptors;
- transient drive sets;
- numeric residuals, Jacobians, and normal equations;
- solve/motion results and snapshots;
- rank/DOF diagnostics;
- per-consumer `ShapeCache` values;
- posed leaf shape copies;
- interference/clearance pair products;
- OCCT compounds and STEP entity identity.

Only explicit successful application changes persisted component solver placement or a selected authored joint coordinate. Rigid subassembly placement/state changes only through explicit occurrence edits until a later whole-subassembly solve-variable contract exists.

## Next assembly block

The next block is persistent project-level cross-hierarchy geometric constraint intent plus occurrence-qualified active-constraint graph and shared numeric solve/application integration.

The endpoint identity is now frozen:

```text
(occurrence_path, local ComponentInstanceId, semantic_reference)
```

The next block must not use local `ComponentInstanceId` alone as graph, numeric-variable, snapshot, or proposal identity because repeated occurrences of one child document may share that local id.

It must define occurrence-qualified component variable identity, active/suppressed path filtering, deterministic graph connectivity, shared residual flattening for all five existing geometric families, finite-difference Jacobian evaluation, fresh-result snapshots, and atomic application to the correct project-owned assembly/component targets.

Cross-hierarchy joints and nested motion propagation remain deferred to a later block.

## Broader future direction

Multi-body transforms/booleans and path features are documented in `docs/multi-body-transform-and-path-features-roadmap.md`.

Inventor-like sketch/feature parity is documented in `docs/inventor-like-sketcher-and-feature-roadmap.md`.

3D sketching and surfacing are documented in `docs/advanced-surfacing-and-3d-sketch-mvp.md`.

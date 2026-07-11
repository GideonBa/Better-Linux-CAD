# Architecture Summary

This document summarizes implemented BLCAD architecture and the current direction. Feature-specific MVP documents remain canonical for exact contracts, formulas, failure policies, and focused proofs.

## Goal and authority model

BLCAD is an independent parametric CAD system for Linux. BLCAD stores semantic model intent and uses OCCT/Open CASCADE as a computed geometry kernel.

Fundamental decisions:

- do not fork FreeCAD;
- keep a custom CAD core above OCCT;
- persist model intent rather than final BRep authority;
- persist semantic references rather than raw OCCT topology ids;
- keep geometric assembly constraints separate from motion-joint intent;
- keep authored hierarchy transforms separate from derived traversal and composed evaluation;
- keep Core model intent below Geometry execution/query types;
- keep GUI code above the CAD core;
- require explicit application before solver results change persistent model state.

## Layer direction

```text
user interface
command/application layer
parametric core model intent
sketch / construction / semantic-reference layers
part feature history
assembly relationship / motion / hierarchy intent
geometry and numeric execution
engineering analysis consumers
OCCT geometry kernel
file and exchange formats
```

A lower authority layer must not depend on a higher execution layer for persistent model identity. In particular, save-format authority must not be a `blcad::geometry` query type.

## Part model

The implemented part path includes:

- typed quantities and parameters;
- unit-aware part-local parameter expressions;
- datum planes and derived workplanes;
- construction points, lines, and planes;
- sketches with line, arc, spline, circle, rectangle, composite, and detected profile families;
- projected/reference-driven sketch geometry;
- semantic generated face/edge/vertex references and recovery helpers;
- sketch constraints, dimensions, diagnostics, and repair helper layers;
- additive/subtractive extrude intent;
- `CircularHolePattern` parametric intent;
- dependency graph, invalidation, recompute planning, and incremental expression re-evaluation;
- optional OCCT recompute through `ShapeCache`;
- STEP export and JSON model-intent serialization.

OCCT shapes are computed cache products.

## Project ownership

`Project` owns:

```text
one explicit root AssemblyDocument
project-owned child AssemblyDocument records
project-owned PartDocument records
```

`Project::assembly()` is the explicit root assembly API.

Child assembly documents are owned independently of rooted occurrence placement. A child enters the rooted assembly tree only when a containing assembly stores a `SubassemblyInstance` referencing it.

Assembly document ids are unique across the explicit root and project-owned child collection. The root may not be referenced as a child.

## Component transform authority

A `ComponentInstance` is a part occurrence record scoped to one `AssemblyDocument`.

Persistent component state includes:

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

The direct component transform is persistent local placement authority.

For identity-sensitive mutation and solving under the current document model, one component transform authority is:

```text
ComponentTransformAuthority =
  (AssemblyDocumentId, local ComponentInstanceId)
```

This identity is important for repeated child assembly occurrences: multiple rooted geometric occurrences may read the same child-document component transform authority.

## Rigid subassembly occurrence intent

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

Its transform is the authored rigid boundary from one containing assembly occurrence to the referenced child assembly document.

A `SubassemblyInstance` currently has no grounding field and is not itself a six-variable rigid-body solve node.

Adding or loading a child occurrence never changes child component transforms.

## Hierarchy validity and traversal

`Project` owns cross-document hierarchy validation.

Validation rejects:

- duplicate assembly document ids;
- duplicate subassembly occurrence ids within one containing assembly;
- direct self-reference;
- root-as-child references;
- missing child assembly documents;
- direct or indirect assembly-reference cycles.

Owned but unreferenced child graphs are also validated so latent cycles cannot enter project state.

`AssemblyHierarchyTraversal` derives the rooted occurrence tree after complete project validation.

Deterministic traversal order is:

```text
root first
pre-order DFS
child SubassemblyInstance records lexicographically by id
```

Repeated occurrences of one child assembly document remain separate descriptors.

`AssemblyHierarchyOccurrenceDescriptor` carries derived context:

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

the grandchild context is:

```text
occurrence_path = [outer, inner]
parent_transforms_inner_to_outer = [T_inner, T_outer]
```

Hierarchy descriptors, paths, parent links, and transform chains are derived and unpersisted.

## Rigid-transform semantics

Every authored `RigidTransform` uses:

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

`AssemblyHierarchyTransformEvaluator` evaluates a transform chain exactly in inner-to-outer order.

For:

```text
chain = [T_component, T_inner, T_outer]
```

point evaluation is:

```text
T_outer(T_inner(T_component(p)))
```

The hierarchy is not persisted as a composed matrix or converted back into a recomputed Euler triple.

## Visible-active leaf flattening

`AssemblyLeafOccurrenceResolver` is the canonical hierarchy-to-leaf boundary for posed geometry consumers.

One `AssemblyLeafOccurrenceDescriptor` stores:

```text
assembly_document
subassembly_path
component_instance
referenced_part_document
transforms_inner_to_outer
```

Leaf order is hierarchy pre-order followed by local `ComponentInstanceId` order.

Hidden or suppressed subassembly occurrences remove their complete descendant subtree. Hidden or suppressed local components are also absent from flattened leaves.

Flattened leaves are derived and unpersisted.

## Local geometric constraint intent

Persistent local relationship families are:

```text
Mate
Concentric
Distance
Insert
Angle
```

Local `AssemblyConstraintTarget` identity is:

```text
(local ComponentInstanceId, semantic_reference)
```

Local constraints belong to one containing `AssemblyDocument`. Target A/B order and active/inactive state are persistent intent. Distance carries a length quantity; Angle carries a degree quantity.

`AssemblyConstraintGraph` uses local component nodes and active local relationship multi-edges. It derives deterministic ordering and connected groups without persisting graph caches.

## Semantic target families

Implemented generated planar target strings:

```text
feature.<feature-id>.top|bottom|right|left|front|back
```

Implemented circular feature target strings:

```text
feature.<feature-id>.axis
feature.<feature-id>.seat
```

The first `.axis`/`.seat` producer is a single-circle `SubtractiveExtrude` source sketch.

`AssemblyConstraintTargetResolver` remains the authority for mapping supported local semantic target strings to component-local geometry plus direct component transform context.

Raw OCCT topology ids are never persistent assembly target identity.

## Local residual families

Canonical local residual semantics are:

```text
Mate:
  normal_opposition    = nA + nB
  signed_separation_mm = dot(oB - oA, nA)

Distance:
  normal_parallelism   = cross(nA, nB)
  signed_separation_mm = dot(oB - oA, nA)
  distance_residual_mm = signed_separation_mm - target_distance_mm

Concentric:
  direction_parallelism = cross(dA, dB)
  axis_offset_mm         = cross(oB - oA, dA)

Insert:
  direction_parallelism       = cross(dA, dB)
  axis_offset_mm               = cross(oB - oA, dA)
  signed_seating_separation_mm = dot(sB - sA, nA)

Angle:
  angle_alignment = dot(nA, nB) - cos(target_angle_deg)
```

Target A owns signed Distance and Insert direction semantics.

## Shared local numeric solve path

The private numeric relationship path consumes local geometric constraint ids plus optional transient Revolute drives.

Each free active local component in one ordinary local solve contributes six direct persisted-transform variables:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

The shared numeric engine implements:

- scaled residual flattening;
- central finite differences;
- damped Gauss-Newton normal equations;
- partial-pivot Gaussian elimination;
- deterministic damping escalation;
- backtracking line search;
- explicit solve states and iteration counts;
- complete component snapshots;
- free-active-component transform proposals.

`AssemblyRigidBodySolver` supplies local geometric relationships.

`AssemblyJointMotionSolver` supplies local geometric relationships plus transient Revolute drives. There is no separate motion optimizer.

## Suppression, snapshots, and application

Ordinary local solving removes suppressed components and local constraints touching them from the active numeric subgroup while retaining complete snapshots for stale-result validation.

`AssemblyRigidBodySolver` mutates only private `Project` copies and returns `AssemblySolveResult`.

`AssemblySolveResultApplier` accepts only converged fresh results and applies component transform proposals atomically through another project copy.

`AssemblyJointMotionResultApplier` additionally validates driven-joint snapshots and changes the selected authored joint coordinate only after the embedded component proposals are valid.

Only explicit successful application changes persistent solver placement or selected joint coordinate state.

## Local rank and remaining DOF

`AssemblySolveDiagnosticsAnalyzer` evaluates the same local geometric finite-difference Jacobian used by ordinary solving.

```text
variable_count  = 6 * free_active_component_count
constrained_dof = rank(J)
remaining_dof   = variable_count - rank(J)
```

Covered regular one-free-body results include:

```text
Concentric: rank 4/6, remaining DOF 2
Insert:     rank 5/6, remaining DOF 1
Angle:      rank 1/6, remaining DOF 5 away from extremal targets
```

## Revolute joint motion seed

Persistent local joint intent is separate from geometric constraints.

The first family is `AssemblyJointType::Revolute` with persistent target A/B semantic references, active/inactive state, principal-angle limits, and authored current coordinate.

Motion reuses `.seat` axis/oriented-frame target semantics. Directed axis alignment, seating, and periodic signed sine/cosine twist residuals drive the selected joint coordinate through the shared numeric engine.

The selected joint is driven to the requested coordinate. Other active Revolute joints in the same local combined relationship group are held at their persisted coordinates.

Cross-hierarchy joints remain deferred.

## Document-scoped flexible child solving

Canonical document: `docs/assembly-flexible-subassembly-solving-mvp5.md`.

`AssemblyFlexibleSubassemblySolver` selects one exact active non-root occurrence path and copies the referenced child `AssemblyDocument` into a temporary `Project` as the local solve root together with project-owned parts.

The existing local graph, target resolver, numeric system, solver, snapshots, and proposal contract are reused.

Application rebuilds the current hierarchy and child-local solve view, reuses ordinary stale-result validation, then atomically writes successfully applied direct component transforms back to the selected project-owned child document.

The selected `SubassemblyInstance::transform()` remains rigid and unchanged.

Repeated occurrences of one child document share the same internal component pose because they share one `ComponentTransformAuthority` per child-document component record.

This is document-scoped flexible solving, not occurrence-local internal pose overriding.

## Cross-hierarchy read-only geometric semantics

Canonical document: `docs/assembly-cross-hierarchy-relationship-semantics-mvp5.md`.

The implemented geometric endpoint identity is:

```text
HierarchyEndpoint =
  (occurrence_path, local ComponentInstanceId, semantic_reference)
```

The empty path addresses the root assembly occurrence. Non-empty paths are exact root-to-current `SubassemblyInstanceId` sequences.

`AssemblyHierarchyConstraintTargetResolver` resolves the exact rooted occurrence, selects the containing assembly document, delegates local feature semantics to `AssemblyConstraintTargetResolver`, and evaluates:

```text
[component, inner parent, ..., outer parent]
```

into root-assembly space through `AssemblyHierarchyTransformEvaluator`.

`AssemblyHierarchyConstraintEquationBuilder` then reuses the canonical Mate, Distance, Angle, Concentric, and Insert residual descriptor families.

The query layer is derived and unpersisted. Visibility and suppression do not change mathematical target resolution.

## Cross-hierarchy identity split

Future cross-hierarchy solving must preserve three separate identities:

```text
geometric endpoint
  = (occurrence_path, ComponentInstanceId, semantic_reference)

relationship node
  = (occurrence_path, ComponentInstanceId)

persisted transform authority
  = (AssemblyDocumentId, ComponentInstanceId)
```

This distinction is required by repeated child assembly occurrences.

Example:

```text
([left],  component.shaft) -> (assembly.gearbox, component.shaft)
([right], component.shaft) -> (assembly.gearbox, component.shaft)
```

The occurrence nodes are geometrically distinct because their parent transforms differ. The current persistent model still stores one shared child-document component transform.

Therefore future numeric variables, stale snapshots, proposals, and direct transform application must be keyed by transform authority, not blindly by occurrence path.

A candidate transform authority may affect residual rows from multiple occurrence paths. Each endpoint evaluates the same candidate direct local transform through its own parent transform chain.

## Relationship connectivity versus solve connectivity

A future cross-hierarchy relationship graph must use occurrence-qualified component nodes and include:

```text
local active AssemblyDocument constraints lifted once per rooted assembly occurrence
project-level cross-hierarchy constraints
```

Local relationships must be lifted per occurrence because repeated child occurrences have distinct root-space geometry.

A pure occurrence relationship graph is not sufficient for numeric solve partitioning. Two different occurrence nodes may map to one shared transform authority.

Therefore future solve connectivity is the transitive closure over:

```text
relationship adjacency
shared ComponentTransformAuthority identity
```

Shared authority coupling is not a geometric constraint and contributes no residual row.

Path-sensitive suppression is evaluated before authority deduplication. Visibility remains a presentation/export property.

The complete planned sequence is canonical in `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

## Nested posed geometry and analysis

`AssemblyPosedLeafShapeBuilder` and `AssemblyStepExporter` consume the canonical flattened leaf boundary.

The derived posed pipeline is:

```text
const Project
  -> validate complete assembly structure and cycles
  -> AssemblyHierarchyTraversal
  -> AssemblyLeafOccurrenceResolver
  -> visible-active flattened leaves
  -> unique referenced leaf part ids
  -> one per-consumer ShapeCache per referenced part
  -> copy and pose each leaf through its authored transform chain
```

`AssemblyStepExporter` composes one OCCT compound and delegates to the existing STEP writer.

The current exchange seed is geometric, not a structured STEP product hierarchy.

`AssemblyInterferenceAnalyzer` evaluates deterministic unordered posed-leaf pairs and classifies finite positive common solid volume above tolerance as interference.

`AssemblyClearanceAnalyzer` preserves interference records and computes minimum distance for remaining pairs. Distances below a positive threshold are clearance violations; exact touching without positive common volume is a zero-distance clearance violation.

All analysis products remain derived and unpersisted.

## Persistence boundary

Current persisted model intent includes:

- project identity and one explicit root assembly;
- project-owned child assembly documents;
- project-owned part documents;
- parameters, formulas, and bindings;
- component occurrence identity/state/grounding/transform;
- rigid subassembly occurrence identity/reference/state/transform;
- local geometric constraint identity, semantic targets, state, and values;
- local joint identity, target semantics, state, limits, and authored coordinate.

Derived and unpersisted data includes:

- local constraint and joint graph connectivity;
- combined motion groups;
- assembly document reference graph and cycle state;
- hierarchy descriptors, paths, parent links, and transform chains;
- flattened leaf descriptors;
- child-local temporary solve views;
- flexible-subassembly result wrappers;
- cross-hierarchy read-only endpoint/query objects;
- resolved local and root-space semantic geometry;
- residual descriptors and transient drives;
- numeric residuals, Jacobians, and normal equations;
- solve/motion results, snapshots, and proposals;
- rank/DOF diagnostics;
- per-consumer shape caches and posed shape copies;
- interference/clearance products;
- OCCT compounds and STEP entity identity.

The current Geometry-layer cross-hierarchy endpoint/query types are not persistent save-format authority.

## Current next implementation sequence

Canonical planning document: `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

The next work is split into:

```text
23. Core endpoint contract + persistent Project-owned cross-hierarchy constraint intent
24. additive JSON + project structure validation
25. occurrence relationship graph + transform-authority solve connectivity
26. shared numeric residual/Jacobian + solve-result integration
27. fresh-result application + cross-hierarchy diagnostics
```

Block 23 is next.

It must extract the frozen endpoint value contract into the Core layer before persistence. It must not serialize or otherwise make a `blcad::geometry` query type model authority.

Cross-hierarchy joints and nested motion propagation remain deferred until geometric solve connectivity and transform-authority application semantics are stable.

## Broader future direction

- Multi-body transforms/booleans and path features: `docs/multi-body-transform-and-path-features-roadmap.md`
- Inventor-like sketch/feature parity: `docs/inventor-like-sketcher-and-feature-roadmap.md`
- 3D sketching and surfacing: `docs/advanced-surfacing-and-3d-sketch-mvp.md`

# Cross-Hierarchy Geometric Constraint Solver Sequence MVP-5

Status: planning contract for the blocks after `docs/assembly-cross-hierarchy-relationship-semantics-mvp5.md`. This document freezes the identity and authority boundaries required before persistent cross-hierarchy geometric constraints enter the numeric solver.

## Why the next work is split

The next assembly work crosses separate authority boundaries:

```text
persistent Core intent
  -> additive project JSON / compatibility
  -> derived relationship-to-variable connectivity
  -> numeric residual/Jacobian and solve results
  -> freshness validation / atomic application
  -> diagnostics
```

These boundaries must not be introduced as one implementation block. Persistent intent, file compatibility, connectivity, numeric execution, and mutation have different failure contracts.

Every block below requires its own canonical implementation document and focused test tag before the following block starts.

## Notation

Assembly documents currently use the general `DocumentId` C++ type. In this planning document:

```text
assembly_document: DocumentId
```

means a `DocumentId` that resolves to one `AssemblyDocument` in the project.

`ComponentTransformAuthority` is a conceptual derived identity for planning. It is not an implemented persistent C++ record yet.

## Frozen identities

Cross-hierarchy solving needs three distinct identities. They are related but are not interchangeable.

### 1. Geometric endpoint identity

Already implemented and frozen:

```text
HierarchyEndpoint =
  (occurrence_path, local ComponentInstanceId, semantic_reference)
```

Example:

```text
([subassembly.left],  component.shaft, feature.bore.axis)
([subassembly.right], component.shaft, feature.bore.axis)
```

These are distinct geometric endpoints because their parent transform chains may place the same local model definition at different root-space positions.

The empty occurrence path addresses the explicit root assembly occurrence.

### 2. Geometric component-occurrence identity

Remove only the semantic feature reference:

```text
HierarchyComponentOccurrence =
  (occurrence_path, local ComponentInstanceId)
```

The left and right shaft occurrences above remain distinct geometric occurrences.

A local `ComponentInstanceId` alone is never sufficient rooted geometric identity.

### 3. Persisted transform-authority identity

Under the current document-scoped flexible-subassembly contract, one component record in one assembly document owns direct placement, grounding, and component suppression:

```text
ComponentTransformAuthority =
  (assembly_document: DocumentId, local ComponentInstanceId)
```

This is the authority for:

```text
ComponentInstance::transform()
ComponentInstance::grounding_state()
ComponentInstance::suppression_state()
```

Repeated rooted occurrences of one child assembly document may therefore have different `HierarchyComponentOccurrence` identities while mapping to the same `ComponentTransformAuthority`.

Example:

```text
([left],  component.shaft)
  -> (assembly.gearbox, component.shaft)

([right], component.shaft)
  -> (assembly.gearbox, component.shaft)
```

The occurrence geometry differs; the direct local component transform is stored once.

This distinction remains mandatory until occurrence-local internal pose overrides are implemented.

## Critical variable rule

Do not create independent transform variables per rooted occurrence when those occurrences share one transform authority.

Incorrect under the current persistence contract:

```text
variable([left],  component.shaft) = 6 independent values
variable([right], component.shaft) = 6 independent values
```

There is no persistent model location capable of storing two independent results.

Correct:

```text
variable(assembly.gearbox, component.shaft) = 6 values
```

Every endpoint occurrence that maps to that authority reads the same candidate direct local component transform and evaluates its own parent transform chain:

```text
T_left(T_candidate(p))
T_right(T_candidate(p))
```

The root-space target geometry differs even though the variable is shared.

## Local relationships must not be duplicated per occurrence

A local `AssemblyConstraint` belongs to one `AssemblyDocument` model definition.

If `assembly.gearbox` occurs twice, its local shaft/bearing constraint still describes one shared internal gearbox pose. It must not be flattened twice merely because the child document has `[left]` and `[right]` rooted occurrences.

Duplicating the local relationship per occurrence would add repeated residual blocks and artificial weighting/redundancy.

For local relationship identity, use containing assembly document plus local relationship id:

```text
LocalRelationship =
  (assembly_document: DocumentId, AssemblyConstraintId)
```

A local relationship is evaluated once in its containing assembly document's local assembly space through the existing local target/residual builders.

Rigidly placing the entire local relationship through a parent transform does not create new model intent. Its geometric satisfaction is the same shared internal constraint.

## Cross-hierarchy relationship identity

A future persistent project-level cross-hierarchy constraint owns its exact occurrence-qualified endpoint pair.

Its relationship record appears once in the project-level collection:

```text
CrossHierarchyRelationship = project-level constraint record
```

The record retains:

```text
target_a: HierarchyEndpoint
target_b: HierarchyEndpoint
```

A cross-hierarchy relationship is evaluated in root-assembly space because its endpoints may use different parent transform chains.

## Relationship-to-authority incidence is the solve-connectivity model

A pure occurrence-node graph is not sufficient for numeric partitioning, and a pure authority graph loses occurrence-specific geometric endpoint context.

The future solve-connectivity layer should therefore derive a deterministic relationship-to-authority incidence graph.

Conceptually:

```text
relationship nodes
  - local relationship: (assembly_document, local constraint id)
  - project-level cross-hierarchy relationship id

variable-authority nodes
  - (assembly_document, local component id)

incidence edge
  - relationship depends on transform authority
```

Local relationships retain their local component targets and appear once.

Cross-hierarchy relationships retain their full occurrence-qualified endpoint identities. Each endpoint is resolved to the transform authority of the addressed component record.

The relationship node is connected to every unique transform authority whose candidate transform can affect its residual.

Example:

```text
cross constraint:
  target A = ([left],  component.shaft, feature.bore.axis)
  target B = ([right], component.shaft, feature.bore.axis)
```

Both geometric endpoints may map to:

```text
(assembly.gearbox, component.shaft)
```

The relationship still has two distinct geometric endpoints, but its residual depends on one six-variable transform authority. In the incidence graph the relationship has one unique authority incidence.

This is not a self-constraint simplification: the two endpoint transform chains remain different and the residual may be nonzero.

## Solve groups

Numeric solve groups are connected components of the active relationship-to-authority incidence graph.

This naturally couples:

- local relationships that share component transform authorities;
- cross-hierarchy relationships that share transform authorities;
- local and cross-hierarchy relationships connected through one component authority;
- different rooted occurrence endpoints that map to one shared child-document transform authority.

No synthetic geometric constraint or fake residual edge is required for shared-authority coupling.

Pure-local groups that contain no project-level cross-hierarchy relationship remain ordinary local solver work. The cross-hierarchy solver path should select solve groups containing at least one active project-level cross-hierarchy relationship.

## Active and suppressed participation

Participation differs by relationship source.

### Local relationship participation

A local relationship is active when:

```text
constraint.state == active
and
both local component records are active
```

It is not duplicated or filtered by a particular rooted `SubassemblyInstance` path because it belongs to the shared assembly-document model definition.

### Cross-hierarchy relationship participation

A project-level cross-hierarchy relationship endpoint occurrence participates only when:

```text
all SubassemblyInstance records on occurrence_path are active
and
addressed ComponentInstance suppression_state == active
```

The relationship participates only when both endpoint occurrences participate.

Visibility remains a presentation/export property and must not remove geometric relationships from solving.

### Grounding

Grounding is transform-authority-owned because `grounding_state` belongs to the shared component record.

A grounded authority contributes no numeric variables regardless of how many active geometric occurrences expose it.

## Block 23: Core endpoint contract and persistent project-level intent

Goal: introduce persistent cross-hierarchy geometric relationship intent in the Core layer without JSON, graph, or solving.

Required work:

1. Move or extract the frozen endpoint value contract into the Core layer. The current `blcad::geometry::AssemblyHierarchyConstraintTarget` is a read-only query seed and must not become save-format authority by serializing a Geometry-layer type.
2. Preserve the frozen endpoint shape exactly:

```text
(occurrence_path, local ComponentInstanceId, semantic_reference)
```

3. Add a persistent project-level cross-hierarchy geometric constraint record with:

```text
id
name
type: Mate | Concentric | Distance | Insert | Angle
target_a
target_b
state: active | inactive
optional distance
optional angle
```

4. Keep `AssemblyDocument::constraints()` and all local constraint APIs unchanged.
5. Make `Project` own the new cross-hierarchy constraint collection because endpoints may address different rooted assembly occurrences.
6. Validate record-local value-family rules exactly like local `AssemblyConstraint` intent.
7. Adding a record must not resolve geometry and must not mutate component or subassembly transforms.
8. Keep hierarchy descriptors, graph connectivity, transform-authority mapping, and solver state derived.

Acceptance examples:

```text
root component -> child occurrence component
left occurrence -> right occurrence of the same child document
nested child occurrence -> root component
```

All three must be representable as persistent intent without resolving semantic geometry.

Focused test tag planned:

```text
[core][assembly-cross-hierarchy-intent]
```

## Block 24: Additive JSON and project-structure validation

Goal: make block-23 intent roundtrip safely without introducing graph or numeric behavior.

Planned additive project field:

```text
cross_hierarchy_constraints[]
```

Representative target JSON:

```json
{
  "occurrence_path": ["subassembly.outer", "subassembly.inner"],
  "component_instance": "component.shaft",
  "semantic_reference": "feature.bore.axis"
}
```

The root occurrence uses:

```json
"occurrence_path": []
```

Representative relationship JSON:

```json
{
  "id": "constraint.cross.main",
  "name": "Root to nested shaft",
  "type": "concentric",
  "target_a": {
    "occurrence_path": [],
    "component_instance": "component.root_bore",
    "semantic_reference": "feature.bore.axis"
  },
  "target_b": {
    "occurrence_path": ["subassembly.gearbox"],
    "component_instance": "component.shaft",
    "semantic_reference": "feature.bore.axis"
  },
  "state": "active"
}
```

Required validation:

1. files without `cross_hierarchy_constraints` load with an empty collection;
2. roundtrip preserves occurrence-path order and target A/B order exactly;
3. endpoint paths resolve in the complete rooted hierarchy;
4. the local component exists in the assembly document reached by the path;
5. project-level cross-hierarchy relationship ids are unique in their collection;
6. local constraint ids remain scoped by containing assembly document;
7. unsupported value-family combinations fail closed;
8. loading never resolves semantic target geometry and never mutates transforms.

Focused test tag planned:

```text
[core][assembly-cross-hierarchy-json]
```

`docs/file-format.md` becomes canonical for the exact JSON spelling when this block is implemented. Until then the shape above is a planning contract, not current save-format semantics.

## Block 25: Relationship-to-authority incidence and active solve groups

Goal: derive deterministic connectivity without numeric residual/Jacobian evaluation.

Required relationship nodes:

```text
LocalRelationship
  = (assembly_document: DocumentId, AssemblyConstraintId)

CrossHierarchyRelationship
  = project-level cross-hierarchy constraint id
```

Required variable-authority nodes:

```text
ComponentTransformAuthority
  = (assembly_document: DocumentId, ComponentInstanceId)
```

Required work:

1. collect active local constraints once per owned assembly document, not once per rooted occurrence;
2. collect active project-level cross-hierarchy constraints;
3. apply local component suppression to local relationship participation;
4. apply endpoint path suppression plus addressed component suppression to cross-hierarchy relationship participation;
5. ignore visibility for solve participation;
6. map each participating local target or hierarchy endpoint to its component transform authority;
7. add one incidence edge from a relationship to each unique authority its residual depends on;
8. derive deterministic connected solve groups from the bipartite incidence graph;
9. expose only groups containing at least one active cross-hierarchy relationship to the cross-hierarchy solver API;
10. persist no graph or incidence cache.

Required deterministic ordering:

```text
authority: assembly document id, then ComponentInstanceId
local relationship: assembly document id, then AssemblyConstraintId
cross relationship: project-level relationship id
endpoint occurrence paths: lexicographic SubassemblyInstanceId sequence
```

Focused test tag planned:

```text
[core][assembly-cross-hierarchy-graph]
```

Required proofs include:

- insertion-order-independent incidence and group output;
- root/child and nested cross-hierarchy endpoints;
- repeated child occurrences remain distinct geometric endpoint contexts;
- repeated occurrence endpoints map to one shared transform authority;
- one child local constraint appears once even when the child document occurs repeatedly;
- local and cross-hierarchy relationships join one group through shared authority incidence;
- two cross-hierarchy relationships on different occurrence paths join one group when they depend on the same authority;
- one cross relationship whose two endpoints map to the same authority has one unique authority incidence but retains both endpoint identities;
- suppressing one cross-hierarchy occurrence path removes only relationships using that endpoint path;
- local relationship participation remains document/component-state based;
- visibility does not remove relationships;
- pure-local groups remain ordinary local solver work.

## Block 26: Shared numeric residual/Jacobian and solve-result integration

Goal: solve one exact block-25 cross-hierarchy solve group while keeping result application separate.

Variable identity is:

```text
ComponentTransformAuthority =
  (assembly_document: DocumentId, local ComponentInstanceId)
```

Each unique free active authority contributes exactly six direct local component-transform variables:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

A repeated rooted occurrence adds no second six-variable block when it maps to an authority already present in the group.

### Local relationship residual evaluation

A local relationship is evaluated once in its containing assembly document's local assembly space through the existing local target/equation builders.

The implementation should create or reuse a temporary local target/evaluation view for that assembly document rather than choosing an arbitrary rooted occurrence path.

### Cross-hierarchy relationship residual evaluation

For each hierarchy endpoint:

1. read the candidate direct local transform from the endpoint component's transform authority;
2. resolve the exact endpoint occurrence path;
3. evaluate the endpoint's own parent transform chain;
4. resolve target geometry in root-assembly space;
5. reuse the existing Mate, Distance, Angle, Concentric, or Insert formula.

### Shared numeric engine

Residuals from local and cross-hierarchy relationships are concatenated in deterministic relationship order with the existing family-specific scalar order and length scaling.

The finite-difference Jacobian perturbs one transform-authority variable at a time. Every local or cross-hierarchy residual that depends on that authority must observe the perturbation.

The block should reuse the existing dense linear solve, damping escalation, backtracking, convergence, and iteration-state machinery. If the current private relationship evaluator is too root-local, refactor the numeric engine boundary rather than copy the optimizer.

Result context must expose:

```text
selected solve-group relationship identities
complete ComponentTransformAuthority input snapshots
one transform proposal per free ComponentTransformAuthority
solve state
iteration count
residual summary
```

The source `Project` remains immutable.

Focused test tag planned:

```text
[geometry][assembly-cross-hierarchy-solver]
```

Required numeric proofs include:

- one root-to-child Mate solve;
- one nested cross-hierarchy solve;
- mixed local plus cross-hierarchy relationships in one incidence group;
- all five existing geometric residual families entering the shared numeric path;
- repeated child occurrences sharing one six-variable transform-authority block;
- one local child constraint contributing one residual block despite repeated child occurrences;
- perturbing one shared authority changes every cross-hierarchy residual that depends on it through different occurrence paths;
- a cross relationship between two occurrences of the same authority remains numerically meaningful;
- deterministic repeated solving;
- unchanged rigid `SubassemblyInstance` boundary transforms.

## Block 27: Fresh-result application and cross-hierarchy diagnostics

Goal: atomically apply block-26 proposals and expose derived rank/DOF diagnostics over exactly the same transform-authority variable ordering.

### Component authority snapshots

Every participating transform authority snapshot must identify and preserve:

```text
assembly_document: DocumentId
ComponentInstanceId
source transform
grounding state
component suppression state
```

There must be at most one snapshot and at most one proposal per transform authority.

### Relationship snapshots

The result must snapshot the persistent relationship intent used by the solve group.

For local relationships, identity includes:

```text
assembly_document: DocumentId
AssemblyConstraintId
```

and the snapshot must protect type, state, targets, and explicit Distance/Angle value.

For project-level cross-hierarchy relationships, the snapshot must protect the complete record including both occurrence-qualified endpoints.

### Hierarchy boundary snapshots

Every `SubassemblyInstance` boundary on a participating cross-hierarchy endpoint path can affect root-space residual geometry or path participation.

The result must therefore protect the relevant boundary input context, including at least:

```text
containing assembly document identity
SubassemblyInstanceId
referenced child assembly document identity
suppression state
boundary RigidTransform
```

Visibility is not a solve input and need not make a result stale.

A changed boundary transform, child reference, suppression state, missing path, or changed path resolution makes the result stale.

### Semantic geometry freshness boundary

The existing local solver snapshots component solve inputs but does not currently provide a general `PartDocument`/feature revision token for target-producing model changes.

Block 27 must not claim geometry-complete freshness unless such a revision authority is implemented.

The block must explicitly choose and document one of these contracts:

```text
A. add a stable model/target-input revision snapshot used by local and cross-hierarchy solving
or
B. preserve the current local-solver limitation and state clearly that semantic target-producing part edits are outside stale-result detection
```

Option A is preferable before GUI/background solve-result lifetimes become long, but it is not silently assumed by this roadmap.

### Atomic application

Application rules:

1. only converged results may be applied;
2. duplicate authority, relationship, or hierarchy-boundary snapshots are invalid;
3. all snapshotted current inputs must match;
4. every proposal must match one free active transform-authority snapshot;
5. every proposed direct transform is validated through the existing component transform contract;
6. apply on a `Project` copy;
7. write at most one transform per `ComponentTransformAuthority`;
8. replace the source project only after all writes succeed;
9. never write a composed hierarchy transform;
10. never mutate `SubassemblyInstance::transform()` in this block.

### Diagnostics

Diagnostics use the exact block-26 variable ordering:

```text
variable_count = 6 * unique_free_active_transform_authority_count
constrained_dof = rank(J)
remaining_dof = variable_count - rank(J)
```

This is intentionally not:

```text
6 * free_active_geometric_occurrence_count
```

when repeated occurrence endpoints share a transform authority.

Focused test tags planned:

```text
[geometry][assembly-cross-hierarchy-apply]
[geometry][assembly-cross-hierarchy-diagnostics]
```

Required proofs include stale transform, grounding, component suppression, relationship, and hierarchy-boundary rejection; atomic no-partial-mutation behavior; one proposal per shared authority; correct Jacobian dimensions/rank counting; and posed-leaf/export consumers observing applied child-document transforms through normal regeneration.

## Deferred after block 27

- cross-hierarchy joints and nested motion propagation;
- occurrence-local internal component pose overrides;
- a persistent rigid/flexible occurrence mode;
- whole-`SubassemblyInstance` rigid solve variables and grounding;
- multi-turn and time-based motion studies;
- component geometry instancing;
- structured STEP assembly products;
- richer contact classification, penetration metrics, and swept-motion analysis.

## Next technical step

Implement **block 23 only**:

```text
Core-owned frozen endpoint value contract
  -> persistent Project-owned cross-hierarchy geometric constraint intent
```

Do not add JSON, connectivity graphs, numeric variables, solving, snapshots, diagnostics, or application in the same block.

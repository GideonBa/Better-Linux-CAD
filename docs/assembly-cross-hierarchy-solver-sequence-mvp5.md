# Cross-Hierarchy Geometric Constraint Solver Sequence MVP-5

Status: Blocks 23 and 24 are implemented. Block 25 is the current next technical step.

This document is the canonical implementation sequence for persistent cross-hierarchy geometric constraints after the read-only target/residual seed in `docs/assembly-cross-hierarchy-relationship-semantics-mvp5.md`.

Implemented persistence contracts are canonical in:

- `docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`
- `docs/assembly-cross-hierarchy-constraint-json-mvp5.md`
- `docs/file-format.md`

## Sequencing rule

Cross-hierarchy solving crosses separate authority boundaries:

```text
Core model intent
  -> serialization and structure compatibility
  -> derived relationship-to-transform-authority connectivity
  -> numeric residual/Jacobian execution
  -> freshness validation and atomic application
  -> diagnostics
```

These remain separate implementation blocks.

Do not:

- persist a Geometry-layer query type;
- duplicate one persistent child-component transform into independent variables per rooted occurrence;
- duplicate one local child constraint for every rooted occurrence of its containing `AssemblyDocument`;
- add numeric residual rows merely to express shared transform authority;
- persist derived graph or solve-group caches.

## Frozen identities

### Geometric endpoint

Implemented Core contract:

```text
AssemblyHierarchyConstraintEndpoint =
  (occurrence_path,
   local ComponentInstanceId,
   semantic_reference)
```

The empty path addresses the explicit root assembly occurrence.

Example:

```text
([subassembly.left],  component.shaft, feature.bore.axis)
([subassembly.right], component.shaft, feature.bore.axis)
```

These are distinct geometric endpoints because their hierarchy transform chains may differ.

### Geometric component occurrence

Remove only the semantic reference:

```text
HierarchyComponentOccurrence =
  (occurrence_path,
   local ComponentInstanceId)
```

This is rooted geometric occurrence identity.

### Persisted component transform authority

The current document-scoped flexible-child contract stores direct component placement/state once per assembly document:

```text
ComponentTransformAuthority =
  (assembly_document: DocumentId,
   local ComponentInstanceId)
```

This authority owns the current persistent component inputs:

```text
ComponentInstance::transform()
ComponentInstance::grounding_state()
ComponentInstance::suppression_state()
```

Repeated occurrences of one child assembly may map different geometric occurrences to one authority:

```text
([left],  component.shaft)
  -> (assembly.gearbox, component.shaft)

([right], component.shaft)
  -> (assembly.gearbox, component.shaft)
```

Until occurrence-local internal pose overrides exist, these occurrences must not become independent six-variable blocks.

## Local relationships are model-definition intent

A local `AssemblyConstraint` belongs to one containing `AssemblyDocument`.

Future cross-hierarchy solve collection identity is:

```text
LocalRelationship =
  (assembly_document: DocumentId,
   AssemblyConstraintId)
```

If `assembly.gearbox` is instantiated twice, one local shaft/bearing constraint still exists once in the child document. It is evaluated once in that document's local assembly space.

Duplicating the relationship per rooted occurrence would create artificial residual weighting and redundancy.

## Project-level cross-hierarchy relationship identity

Blocks 23 and 24 implement project-owned:

```text
AssemblyHierarchyConstraint
```

The record preserves exact Core-owned occurrence-qualified endpoints and is now serialized in:

```text
cross_hierarchy_constraints[]
```

A project-level cross-hierarchy constraint id is unique within that Project collection. Local constraint ids remain scoped by containing assembly document.

Cross-hierarchy relationships are evaluated in root-assembly space because target A and target B may follow different parent transform chains.

## Block 23: Core endpoint and persistent project-level intent — Implemented

Canonical document: `docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`.

Implemented:

- Core-owned `AssemblyHierarchyConstraintEndpoint`;
- exact empty-root and ordered non-empty occurrence-path identity;
- persistent `AssemblyHierarchyConstraint` records for Mate, Concentric, Distance, Insert, and Angle;
- reuse of the established Distance/Angle value-family validation contract;
- Project-owned cross-hierarchy constraint collection APIs;
- project-level id uniqueness independent from local document-scoped ids;
- no hierarchy or semantic geometry resolution during record creation/addition;
- no transform mutation;
- Core-record to read-only Geometry-query bridge.

Focused tag:

```text
[core][assembly-cross-hierarchy-intent]
```

## Block 24: Additive JSON and project-structure validation — Implemented

Canonical document: `docs/assembly-cross-hierarchy-constraint-json-mvp5.md`.

Implemented additive Project field:

```text
cross_hierarchy_constraints[]
```

Implemented behavior:

1. files without the field load with an empty collection;
2. serialization emits the collection additively;
3. relationship id/name/type/state roundtrip;
4. target A/B order is preserved exactly;
5. endpoint occurrence-path element order is preserved exactly;
6. Distance uses `mm` and Angle uses `deg`;
7. duplicate project-level cross-hierarchy ids fail closed;
8. endpoint paths resolve structurally from the explicit root through exact authored `SubassemblyInstanceId` sequences;
9. the reached assembly document must contain the addressed local component;
10. loading does not resolve semantic feature geometry;
11. loading does not mutate component or subassembly transforms;
12. `Project::validate_assembly_structure()` performs cross-hierarchy endpoint structure validation only after the ordinary assembly hierarchy has validated.

The structural validator follows authored Core links directly. It does not call OCCT or Geometry target resolvers.

Focused tag:

```text
[core][assembly-cross-hierarchy-json]
```

## Relationship-to-transform-authority incidence

A pure occurrence graph loses persistent mutation authority. A pure authority graph loses occurrence-specific endpoint geometry.

Solve partitioning therefore uses a deterministic bipartite incidence model:

```text
relationship node
    |
    | residual depends on
    v
component transform authority node
```

Relationship nodes are:

```text
LocalRelationship
  = (assembly_document, AssemblyConstraintId)

CrossHierarchyRelationship
  = project-level AssemblyConstraintId
```

Authority nodes are:

```text
ComponentTransformAuthority
  = (assembly_document, ComponentInstanceId)
```

Each relationship is incident to every unique transform authority whose candidate direct component transform can change its residual.

A cross-hierarchy relationship may contain two distinct geometric endpoints that map to the same transform authority.

Example:

```text
target A = ([left],  component.shaft, feature.bore.axis)
target B = ([right], component.shaft, feature.bore.axis)
```

Both may map to:

```text
(assembly.gearbox, component.shaft)
```

The relationship has one unique authority incidence but retains two endpoint identities and two hierarchy transform chains. Its residual can therefore remain nonzero and numerically meaningful.

No synthetic constraint or fake residual row is introduced for shared-authority coupling.

## Block 25: Relationship-to-authority incidence and active solve groups — Next

Goal: derive deterministic cross-hierarchy solve connectivity without numeric residual/Jacobian evaluation.

Required relationship nodes:

```text
LocalRelationship
  = (assembly_document: DocumentId, AssemblyConstraintId)

CrossHierarchyRelationship
  = project-level AssemblyConstraintId
```

Required authority nodes:

```text
ComponentTransformAuthority
  = (assembly_document: DocumentId, ComponentInstanceId)
```

Required behavior:

1. validate complete project structure first;
2. collect active local constraints once per owned `AssemblyDocument`, not once per rooted occurrence;
3. collect active project-level cross-hierarchy constraints;
4. local relationship participation requires active relationship state and active local endpoint components;
5. cross-hierarchy relationship participation requires active relationship state, every `SubassemblyInstance` on both exact endpoint paths to be active, and both addressed endpoint components to be active;
6. visibility does not filter geometric solve participation;
7. map every participating local target to `(containing assembly document, local ComponentInstanceId)`;
8. map every participating hierarchy endpoint to `(assembly document reached by exact occurrence path, local ComponentInstanceId)`;
9. add one incidence edge from a relationship to each unique authority its residual depends on;
10. retain both geometric endpoint identities even when both map to one authority;
11. derive deterministic connected components of the bipartite incidence graph;
12. expose only groups containing at least one active project-level cross-hierarchy relationship to the cross-hierarchy solve API;
13. pure-local groups remain ordinary local solver work;
14. persist no graph, authority mapping, or solve-group cache.

Deterministic ordering:

```text
authority:
  assembly document id
  ComponentInstanceId

local relationship:
  assembly document id
  AssemblyConstraintId

cross-hierarchy relationship:
  project-level AssemblyConstraintId
```

Group ordering must be independent of insertion order. A canonical implementation may order each group by its first ordered relationship/authority key and must document the exact rule.

Required proofs:

- root/child and nested cross-hierarchy relationships;
- repeated child occurrences remain distinct endpoint contexts;
- repeated endpoint occurrences map to one shared transform authority;
- one child-local constraint appears once despite repeated child occurrences;
- local and cross-hierarchy relationships join one group through shared authority incidence;
- two cross-hierarchy relationships on different occurrence paths join one group when they depend on the same authority;
- one cross relationship whose two endpoints map to the same authority has one unique authority incidence but retains both endpoint identities;
- suppressing one endpoint path removes only cross-hierarchy relationships that use that path;
- suppressing a local component removes local relationships touching that component and cross relationships using the addressed occurrence;
- visibility does not remove relationships;
- inactive relationships do not participate;
- pure-local groups are not exposed as cross-hierarchy solve groups;
- graph and group output is insertion-order independent.

Focused tag:

```text
[core][assembly-cross-hierarchy-graph]
```

Block 25 must not evaluate semantic geometry, residual values, Jacobians, or solver iterations.

## Block 26: Shared numeric residual/Jacobian and solve-result integration

Goal: solve one exact Block-25 cross-hierarchy solve group without applying the result.

Variable identity is:

```text
ComponentTransformAuthority =
  (assembly_document: DocumentId,
   local ComponentInstanceId)
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

Residuals from local and cross-hierarchy relationships are concatenated in deterministic relationship order with existing family-specific scalar order and length scaling.

The finite-difference Jacobian perturbs one transform-authority variable at a time. Every local or cross-hierarchy residual that depends on that authority must observe the perturbation.

Reuse the existing dense linear solve, damping escalation, backtracking, convergence, and iteration-state machinery. If the current private relationship evaluator is too root-local, refactor the numeric engine boundary rather than copying the optimizer.

Result context must expose:

```text
selected solve-group relationship identities
complete ComponentTransformAuthority input snapshots
one transform proposal per free ComponentTransformAuthority
solve state
iteration count
residual summary
```

Do not apply proposals in Block 26.

Focused tag:

```text
[geometry][assembly-cross-hierarchy-solver]
```

## Block 27: Fresh-result application and cross-hierarchy diagnostics

Goal: atomically apply Block-26 proposals and expose derived rank/DOF diagnostics over exactly the same transform-authority variable ordering.

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

Local relationship identity includes:

```text
assembly_document: DocumentId
AssemblyConstraintId
```

The snapshot protects type, state, targets, and explicit Distance/Angle value.

A project-level cross-hierarchy relationship snapshot protects the complete persistent record including both occurrence-qualified endpoints.

### Hierarchy boundary snapshots

Every `SubassemblyInstance` boundary on a participating cross-hierarchy endpoint path can affect root-space residual geometry or path participation.

Protect at least:

```text
containing assembly document identity
SubassemblyInstanceId
referenced child assembly document identity
suppression state
boundary RigidTransform
```

Visibility is not a solve input and need not make a result stale.

### Semantic geometry freshness boundary

The existing local solver snapshots component solve inputs but does not currently expose a general `PartDocument`/feature revision token for target-producing model changes.

Block 27 must explicitly choose and document one contract:

```text
A. add a stable model/target-input revision snapshot used by local and cross-hierarchy solving
or
B. preserve the current local-solver limitation and state clearly that semantic target-producing part edits are outside stale-result detection
```

Option A is preferable before GUI/background result lifetimes become long, but it is not silently assumed.

### Atomic application

Rules:

1. only converged results may be applied;
2. duplicate authority, relationship, or hierarchy-boundary snapshots are invalid;
3. all snapshotted current inputs must match;
4. every proposal must match one free active transform-authority snapshot;
5. validate each proposed direct transform through the existing component transform contract;
6. apply on a `Project` copy;
7. write at most one transform per `ComponentTransformAuthority`;
8. replace the source project only after all writes succeed;
9. never write a composed hierarchy transform;
10. never mutate `SubassemblyInstance::transform()` in this block.

### Diagnostics

Use the exact Block-26 variable ordering:

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

Focused tags:

```text
[geometry][assembly-cross-hierarchy-apply]
[geometry][assembly-cross-hierarchy-diagnostics]
```

## Deferred after Block 27

- cross-hierarchy joints and nested motion propagation;
- occurrence-local internal component pose overrides;
- a persistent rigid/flexible occurrence mode;
- whole-`SubassemblyInstance` rigid solve variables and grounding;
- multi-turn and time-based motion studies;
- component geometry instancing;
- structured STEP assembly products;
- richer contact classification, penetration metrics, and swept-motion analysis.

## Next technical step

Implement **Block 25 only**:

```text
active local and project-level cross-hierarchy relationships
  -> exact relationship-to-ComponentTransformAuthority incidence
  -> deterministic connected cross-hierarchy solve groups
```

Do not add residual/Jacobian evaluation, numeric solve variables, solver iterations, snapshots, proposals, diagnostics, or application in the same block.

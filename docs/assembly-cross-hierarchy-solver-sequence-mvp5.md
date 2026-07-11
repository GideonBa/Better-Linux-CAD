# Cross-Hierarchy Geometric Constraint Solver Sequence MVP-5

Status: Blocks 23-26 are implemented. Block 27 is the current next technical step.

This document is the canonical implementation sequence for persistent cross-hierarchy geometric constraints after the read-only target/residual seed in `docs/assembly-cross-hierarchy-relationship-semantics-mvp5.md`.

Implemented contracts are canonical in:

- `docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`
- `docs/assembly-cross-hierarchy-constraint-json-mvp5.md`
- `docs/assembly-cross-hierarchy-incidence-groups-mvp5.md`
- `docs/assembly-cross-hierarchy-numeric-solver-mvp5.md`
- `docs/file-format.md`

## Sequencing rule

Cross-hierarchy solving crosses separate authority boundaries:

```text
Core model intent
  -> serialization and structure compatibility
  -> derived relationship-to-transform-authority connectivity
  -> numeric residual/Jacobian execution and unapplied proposals
  -> freshness validation and atomic application
  -> diagnostics
```

These boundaries must remain explicit.

Do not:

- persist a Geometry-layer query type;
- duplicate one shared child-component transform into independent variables per rooted occurrence;
- duplicate one local child constraint for every rooted occurrence of its containing `AssemblyDocument`;
- add residual rows merely to represent shared transform authority;
- persist graph, solve-group, residual, Jacobian, snapshot, proposal, or diagnostic caches;
- mutate a `SubassemblyInstance::transform()` through component solve variables;
- introduce a second numeric optimizer for cross-hierarchy constraints.

## Frozen identities

### Geometric endpoint

Persistent Core identity:

```text
AssemblyHierarchyConstraintEndpoint =
  (occurrence_path,
   local ComponentInstanceId,
   semantic_reference)
```

The empty occurrence path addresses the explicit root assembly occurrence.

Example:

```text
([subassembly.left],  component.shaft, feature.bore.axis)
([subassembly.right], component.shaft, feature.bore.axis)
```

These endpoints are distinct because their exact hierarchy transform chains may differ.

### Geometric component occurrence

Removing only the semantic reference gives rooted geometric occurrence identity:

```text
HierarchyComponentOccurrence =
  (occurrence_path,
   local ComponentInstanceId)
```

### Direct component transform authority

Persistent direct component placement remains owned once per assembly document:

```text
ComponentTransformAuthority =
  (assembly_document: DocumentId,
   local ComponentInstanceId)
```

The authority identity is derived. It addresses persistent component solve inputs:

```text
ComponentInstance::transform()
ComponentInstance::grounding_state()
ComponentInstance::suppression_state()
```

Repeated occurrences may map to one authority:

```text
([left],  component.shaft)
  -> (assembly.gearbox, component.shaft)

([right], component.shaft)
  -> (assembly.gearbox, component.shaft)
```

Until occurrence-local internal pose overrides exist, both occurrences read and propose one shared direct component transform.

## Relationship identity

A local `AssemblyConstraint` belongs to one containing `AssemblyDocument`:

```text
LocalRelationship =
  (assembly_document: DocumentId,
   AssemblyConstraintId)
```

It exists once as model-definition intent and is evaluated once in that document's local assembly space.

A project-level cross-hierarchy relationship is identified by its Project-scoped id:

```text
CrossHierarchyRelationship =
  project-level AssemblyConstraintId
```

It preserves both exact occurrence-qualified endpoints and is evaluated in root-assembly space.

## Block 23: Core endpoint and project-level constraint intent — Implemented

Canonical document: `docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`.

Implemented:

- Core-owned `AssemblyHierarchyConstraintEndpoint`;
- empty-root and exact ordered non-empty occurrence-path identity;
- persistent `AssemblyHierarchyConstraint` records;
- Mate, Concentric, Distance, Insert, and Angle families;
- established Distance/Angle quantity validation;
- Project-owned cross-hierarchy constraint collection;
- Project-level id uniqueness separate from local document-scoped ids;
- Core-record to read-only Geometry-query bridge;
- no hierarchy or semantic geometry resolution during record construction;
- no transform mutation.

Focused tag:

```text
[core][assembly-cross-hierarchy-intent]
```

## Block 24: Additive JSON and Core structure validation — Implemented

Canonical document: `docs/assembly-cross-hierarchy-constraint-json-mvp5.md`.

Implemented Project JSON field:

```text
cross_hierarchy_constraints[]
```

Implemented behavior:

1. absent field loads as an empty collection;
2. target A/B order roundtrips exactly;
3. occurrence-path element order roundtrips exactly;
4. id/name/type/state roundtrip;
5. Distance uses `mm` and Angle uses `deg`;
6. duplicate Project-level cross-hierarchy ids fail closed;
7. complete ordinary assembly hierarchy validation runs first;
8. each endpoint path is then followed exactly from the explicit root through authored `SubassemblyInstanceId` values;
9. the reached assembly document must contain the addressed local component;
10. semantic feature geometry remains unresolved during loading;
11. loading does not mutate component or subassembly transforms.

Focused tag:

```text
[core][assembly-cross-hierarchy-json]
```

## Block 25: Relationship-to-authority incidence and active solve groups — Implemented

Canonical document: `docs/assembly-cross-hierarchy-incidence-groups-mvp5.md`.

The derived bipartite model is:

```text
relationship node
    |
    | residual depends on
    v
ComponentTransformAuthority node
```

Implemented behavior:

1. complete Project structure validates first;
2. active local constraints are collected once per owned `AssemblyDocument`;
3. active Project-level cross-hierarchy constraints retain exact endpoint identity;
4. local relationship participation requires active relationship state and active local endpoint components;
5. cross-hierarchy participation requires active relationship state, active suppression state on every boundary of both exact endpoint paths, and active addressed components;
6. visibility does not filter solve participation;
7. local targets map to `(containing assembly document, local ComponentInstanceId)`;
8. hierarchy endpoints map to `(assembly document reached by exact occurrence path, local ComponentInstanceId)`;
9. incidence is unique by `(relationship, authority)`;
10. target A/B endpoint mappings remain separate even when both endpoints map to one authority;
11. deterministic connected components are derived over the bipartite graph;
12. only components containing at least one active Project-level cross-hierarchy relationship are exposed as cross-hierarchy solve groups;
13. pure-local components remain ordinary local solver work;
14. graph, mappings, and groups remain derived and unpersisted.

Canonical ordering:

```text
relationship kind:
  local before cross-hierarchy

local relationship:
  assembly document id
  AssemblyConstraintId

cross-hierarchy relationship:
  Project-level AssemblyConstraintId

authority:
  assembly document id
  ComponentInstanceId

incidence:
  relationship key
  authority key

endpoint mapping:
  cross-hierarchy constraint id
  TargetA before TargetB

solve group:
  first ordered relationship key
  first ordered authority key as deterministic fallback
```

All string keys are lexicographic ascending.

Focused tag:

```text
[core][assembly-cross-hierarchy-graph]
```

## Block 26: Shared numeric residual/Jacobian and solve-result integration — Implemented

Canonical document: `docs/assembly-cross-hierarchy-numeric-solver-mvp5.md`.

Goal achieved: solve one exact current `AssemblyCrossHierarchySolveGroup` without applying the result.

### Exact current group validation

`AssemblyCrossHierarchyConstraintSolver::solve` rebuilds the Block-25 graph and requires the supplied group to equal one current deterministic `solve_groups()` entry.

A participation change such as path suppression invalidates an old selected group before numeric work begins.

### Authority-scoped variables

Each unique free active authority contributes six absolute direct component-transform variables:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

Variable blocks follow Block-25 authority order after filtering to free authorities.

Grounded authorities remain residual dependencies and snapshots but contribute no variable block.

Repeated rooted occurrences add no second variable block when they map to an authority already present in the group.

### Local relationship residual evaluation

For each local relationship identity:

```text
(assembly_document, AssemblyConstraintId)
```

Block 26 resolves the containing document, creates a temporary document-as-local-root Project view with the owned part documents, and reuses the existing local numeric relationship path.

The local relationship is evaluated once in containing-document local assembly space.

No arbitrary rooted occurrence of the document is selected.

### Cross-hierarchy relationship residual evaluation

For each Project-level cross-hierarchy relationship:

1. resolve the persistent `AssemblyHierarchyConstraint`;
2. create the derived hierarchy query;
3. preserve target A/B and each exact occurrence path;
4. read current candidate direct component transforms from the candidate Project copy;
5. evaluate each endpoint through its own component-plus-parent chain into root-assembly space;
6. reuse the existing hierarchy equation builder.

Two endpoints may read one candidate authority transform while following different parent transform chains.

### Shared scalar residual flattening

The local numeric system owns one flattening implementation reused by local and hierarchy residual descriptors.

Established component order and counts remain:

```text
Mate        4
Distance    4
Angle       1
Concentric  6
Insert      7
```

Established length scaling remains unchanged.

Complete relationship blocks are concatenated in exact Block-25 solve-group relationship order.

### Shared central finite differences

The numeric system now has one internal evaluator boundary:

```text
absolute candidate variable vector
  -> deterministic scaled residual vector
```

Central finite differences perturb one authority-scoped variable at a time with the existing translation/rotation step semantics.

Every local and cross-hierarchy residual depending on one authority observes the same candidate direct-transform perturbation.

No occurrence-local composed transform or `SubassemblyInstance::transform()` is perturbed.

### Shared numeric engine

The existing numeric solve loop is generalized as `solve_numeric_variables` and remains the only optimizer.

It owns:

```text
option validation
scaled residual evaluation
central finite differences
damped Gauss-Newton normal equations
partial-pivot dense solve
damping escalation
backtracking
convergence / iteration-limit / numerical-failure state
```

Ordinary local solving and joint motion continue through the local `solve_numeric_relationships` adapter.

Cross-hierarchy solving adapts `ComponentTransformAuthority` values to the same engine.

### Block-26 result contract

`AssemblyCrossHierarchySolveResult` exposes:

```text
selected solve-group relationship identities
fixed ComponentTransformAuthority values
complete participating authority snapshots
at most one proposal per free ComponentTransformAuthority
solve state
iteration count
residual summary
```

An authority snapshot preserves:

```text
assembly document id
ComponentInstanceId
source direct transform
grounding state
component suppression state
```

A proposal preserves:

```text
ComponentTransformAuthority
source direct transform
proposed direct transform
```

No result is applied in Block 26.

Focused tag:

```text
[geometry][assembly-cross-hierarchy-solver]
```

Required proofs implemented:

- all five cross-hierarchy residual families flatten with established counts and satisfied semantics;
- a root/child Distance relationship converges through authority-scoped variables;
- a nested endpoint uses its exact parent chain;
- repeated child occurrences share one variable block and one proposal;
- two endpoints of one authority observe the same candidate transform through different parent chains;
- a child-local relationship and a cross-hierarchy relationship contribute to one mixed residual vector;
- relationship and variable ordering are deterministic across persistent insertion order;
- grounded authorities contribute residual context but no variable block;
- no-grounded-reference groups fail closed under the existing solve contract;
- suppressed participation invalidates a previously selected group;
- the source Project is immutable;
- no `SubassemblyInstance::transform()` proposal exists.

## Block 27: Fresh-result application and cross-hierarchy diagnostics — Next

Goal: validate a Block-26 result against all modeled solve inputs, atomically apply fresh converged authority proposals, and expose rank/remaining-DOF diagnostics over exactly the same free-authority variable ordering.

### 27A. Authority freshness

Protect every participating authority snapshot:

```text
assembly_document: DocumentId
ComponentInstanceId
source direct transform
grounding state
component suppression state
```

Requirements:

1. duplicate authority snapshots are invalid;
2. every snapshotted assembly document and component must still exist;
3. transform, grounding, and suppression must match the snapshot;
4. every proposal must map to exactly one free active authority snapshot;
5. at most one proposal may address one authority.

### 27B. Relationship freshness

Protect complete relationship inputs selected by Block 26.

Local relationship identity:

```text
(assembly_document, AssemblyConstraintId)
```

Protect the complete local `AssemblyConstraint` record:

```text
type
state
target A
target B
Distance value
Angle value
```

For Project-level cross-hierarchy relationships, protect the complete persistent `AssemblyHierarchyConstraint`, including both exact occurrence-qualified endpoints.

Relationship removal, replacement, state change, target change, type change, or explicit value change must make the result stale.

### 27C. Hierarchy-boundary freshness

Every `SubassemblyInstance` boundary on a participating cross-hierarchy endpoint path can affect root-space geometry or solve participation.

Protect at least:

```text
containing assembly document identity
SubassemblyInstanceId
referenced child assembly document identity
suppression state
boundary RigidTransform
```

Visibility is not a solve input and need not invalidate a result.

Boundary removal, retargeting, suppression change, or transform change must make the result stale.

### 27D. Semantic target-producing geometry freshness

The existing local solver has no general `PartDocument`/feature revision token for semantic target-producing model edits.

Block 27 must deliberately choose and document one contract:

```text
A. add a stable model/target-input revision snapshot used by local and cross-hierarchy solve results

or

B. preserve the current local-solver limitation and explicitly state that semantic target-producing part edits are outside stale-result detection
```

Option A remains preferable before long-lived GUI or background solve-result lifetimes exist.

The implementation must not silently claim geometry-complete freshness if it chooses B.

### 27E. Atomic application

Rules:

1. only converged results may be applied;
2. validate complete authority, relationship, and hierarchy-boundary freshness first;
3. validate every proposed direct transform through the existing component transform contract;
4. create a Project copy;
5. write at most one direct local component transform per `ComponentTransformAuthority`;
6. replace the source Project only after every write succeeds;
7. never persist a composed hierarchy transform;
8. never mutate `SubassemblyInstance::transform()` through this applier.

### 27F. Diagnostics

Diagnostics must use the exact Block-26 free-authority variable order and the same mixed residual evaluator/Jacobian semantics.

```text
variable_count = 6 * unique_free_active_transform_authority_count
constrained_dof = rank(J)
remaining_dof = variable_count - rank(J)
```

The count is not based on geometric occurrence count.

Recommended focused tags:

```text
[geometry][assembly-cross-hierarchy-application]
[geometry][assembly-cross-hierarchy-diagnostics]
```

Application/freshness and diagnostics may use separate test translation units, but both remain Block 27 unless the canonical plan is deliberately split before implementation.

## Blocks 28+

### 28. Cross-hierarchy joints and nested motion propagation

Deferred until Blocks 23-27 freeze geometric relationship persistence, solve connectivity, authority mapping, numeric execution, freshness, application, and diagnostics.

### 29. Component geometry instancing and structured STEP assembly products

Deferred.

### 30. Richer collision/contact and swept-motion analysis

Deferred.

## Persistence rule

Persist model intent.

Current persistent assembly intent includes component placement/state, local geometric constraints, local joint/limit/coordinate records, project-owned child assemblies, rigid subassembly occurrence placement/state, and Project-owned cross-hierarchy geometric constraints with exact occurrence-qualified endpoints.

Regenerate:

```text
local graph connectivity
combined motion groups
hierarchy traversal and parent transform chains
flattened leaves
cross-hierarchy target-resolution values
root-space resolved geometry
residual descriptors
ComponentTransformAuthority identities
relationship-to-authority incidence
endpoint-authority mappings
cross-hierarchy solve groups
mixed numeric residuals and Jacobians
cross-hierarchy solve state and proposals
authority snapshots
future application freshness snapshots and diagnostics
shape caches and posed shapes
analysis products
STEP compounds
```

## Next technical step

Implement Block 27 only: complete Block-26 result freshness validation, an explicit semantic target-geometry freshness contract, atomic authority-qualified direct-transform application, and rank/remaining-DOF diagnostics over exactly the Block-26 free-authority variable ordering.

Do not introduce cross-hierarchy joints, nested motion propagation, occurrence-local internal pose overrides, or whole-subassembly transform variables in the same block.

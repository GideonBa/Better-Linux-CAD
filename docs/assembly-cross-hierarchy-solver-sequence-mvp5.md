# Cross-Hierarchy Geometric Constraint Solver Sequence MVP-5

Status: Blocks 23, 24, and 25 are implemented. Block 26 is the current next technical step.

This document is the canonical implementation sequence for persistent cross-hierarchy geometric constraints after the read-only target/residual seed in `docs/assembly-cross-hierarchy-relationship-semantics-mvp5.md`.

Implemented contracts are canonical in:

- `docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`
- `docs/assembly-cross-hierarchy-constraint-json-mvp5.md`
- `docs/assembly-cross-hierarchy-incidence-groups-mvp5.md`
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
- persist derived graph, endpoint-mapping, or solve-group caches;
- apply solve proposals in Block 26.

## Frozen identities

### Geometric endpoint

```text
AssemblyHierarchyConstraintEndpoint =
  (occurrence_path,
   local ComponentInstanceId,
   semantic_reference)
```

The empty path addresses the explicit root assembly occurrence. Different paths remain different geometric endpoint contexts even when they reach one shared child document.

### Geometric component occurrence

```text
(occurrence_path,
 local ComponentInstanceId)
```

This is rooted geometric occurrence identity.

### Component transform authority

```text
ComponentTransformAuthority =
  (assembly_document: DocumentId,
   local ComponentInstanceId)
```

The persistent direct transform/state owner is the reached `ComponentInstance` record in that assembly document.

Repeated occurrences may map to one authority:

```text
([left],  component.shaft)
([right], component.shaft)

        ->

(assembly.gearbox, component.shaft)
```

Until occurrence-local internal pose overrides exist, these occurrences must not become independent six-variable blocks.

## Local relationship identity

A local `AssemblyConstraint` belongs to one containing `AssemblyDocument`.

```text
AssemblyLocalRelationshipIdentity =
  (assembly_document: DocumentId,
   AssemblyConstraintId)
```

One local child constraint is collected once from its child document even when that document is instantiated repeatedly.

## Project-level cross-hierarchy relationship identity

Blocks 23 and 24 implement project-owned:

```text
AssemblyHierarchyConstraint
```

Its id is unique within `Project::cross_hierarchy_constraints()` and its exact endpoint values roundtrip through:

```text
cross_hierarchy_constraints[]
```

Local constraint ids remain scoped by containing `AssemblyDocument`.

## Block 23: Core endpoint and persistent project-level intent — Implemented

Canonical document: `docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`.

Implemented:

- Core-owned `AssemblyHierarchyConstraintEndpoint`;
- exact empty-root and ordered non-empty occurrence-path identity;
- persistent project-level Mate, Concentric, Distance, Insert, and Angle records;
- established Distance/Angle value-family validation reuse;
- Project-owned collection and independent id scope;
- no path or semantic geometry resolution during record creation;
- transform immutability;
- Core-record bridge into the read-only Geometry query path.

Focused tag:

```text
[core][assembly-cross-hierarchy-intent]
```

## Block 24: Additive JSON and project-structure validation — Implemented

Canonical document: `docs/assembly-cross-hierarchy-constraint-json-mvp5.md`.

Implemented:

- additive `cross_hierarchy_constraints[]` Project JSON;
- backward-compatible absent-field loading;
- id/name/type/state roundtrip;
- exact target A/B order;
- exact occurrence-path element order;
- Distance `mm` and Angle `deg` roundtrip;
- duplicate project-level id rejection;
- exact Core-only occurrence-path structure validation from the root;
- reached local component validation;
- hierarchy validation before endpoint validation;
- no semantic feature geometry execution;
- transform immutability.

Focused tag:

```text
[core][assembly-cross-hierarchy-json]
```

## Block 25: Relationship-to-authority incidence and active solve groups — Implemented

Canonical document: `docs/assembly-cross-hierarchy-incidence-groups-mvp5.md`.

Implemented public derived identities:

```text
ComponentTransformAuthority
AssemblyLocalRelationshipIdentity
AssemblyProjectCrossHierarchyRelationshipIdentity
AssemblyRelationshipIdentity
```

Implemented graph:

```text
AssemblyCrossHierarchyConstraintGraph
```

The graph validates complete Project structure first.

Local relationship participation requires:

```text
relationship Active
both addressed local components Active
```

Cross-hierarchy relationship participation requires:

```text
relationship Active
all SubassemblyInstance boundaries on target A path Active
target A component Active
all SubassemblyInstance boundaries on target B path Active
target B component Active
```

Visibility is not a solve-participation filter.

Every participating relationship is incident to each unique transform authority whose candidate direct component transform can affect its residual.

Unique edge type:

```text
AssemblyRelationshipAuthorityIncidence
```

Occurrence-specific endpoint context is retained separately in:

```text
AssemblyCrossHierarchyEndpointAuthorityMapping
```

A cross relationship whose target A and target B map to one authority therefore has one unique incidence edge and two endpoint mappings.

Connected components are derived over the bipartite relationship/authority graph. Only components containing at least one active project-level cross-hierarchy relationship are exposed as:

```text
AssemblyCrossHierarchySolveGroup
```

Pure-local components remain ordinary local solver work.

Deterministic ordering is:

```text
relationship kind:
  local before cross-hierarchy

local relationship:
  assembly document id
  constraint id

cross-hierarchy relationship:
  constraint id

authority:
  assembly document id
  component id

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

Block 25 performs no semantic target geometry evaluation, residual flattening, Jacobian construction, or solver iteration.

## Block 26: Shared numeric residual/Jacobian and solve-result integration — Next

Goal: solve one exact `AssemblyCrossHierarchySolveGroup` without applying the result.

### Variable identity

Use:

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

Grounded authorities remain relationship dependencies but contribute no numeric variables.

A repeated rooted occurrence adds no second variable block when its endpoint maps to an authority already present in the group.

Variable order must follow the Block-25 authority order after filtering to free active authorities.

### Local relationship residual evaluation

A local relationship is evaluated once in its containing assembly document's local assembly space.

For each local relationship identity:

```text
(assembly_document, AssemblyConstraintId)
```

Block 26 must resolve the exact containing document and reuse the existing local target/equation semantics.

Do not choose an arbitrary rooted occurrence of that document.

A temporary child-as-local-root evaluation view may be reused or generalized from the existing flexible-child solve adapter.

### Cross-hierarchy relationship residual evaluation

For each `AssemblyProjectCrossHierarchyRelationshipIdentity`:

1. read target A and target B from the persistent `AssemblyHierarchyConstraint` record;
2. read candidate direct local component transforms from their mapped authorities;
3. preserve each endpoint's exact occurrence path;
4. evaluate each endpoint through its own parent transform chain into root-assembly space;
5. reuse the existing Mate, Distance, Angle, Concentric, or Insert equation semantics.

Two endpoints may read one candidate authority transform while using different parent chains.

### Shared residual vector

Relationships are evaluated in the exact Block-25 group relationship order.

Within each relationship, preserve the established family-specific scalar flattening order and existing length scaling.

The complete mixed local/cross residual vector is concatenated deterministically.

### Finite-difference Jacobian

Use central finite differences with the existing numeric perturbation semantics.

Perturb one free transform-authority variable at a time.

Every local or cross-hierarchy residual that depends on that authority must observe the same perturbed candidate direct transform.

Do not perturb occurrence-local composed transforms or `SubassemblyInstance::transform()`.

### Numeric solve engine

Reuse the existing dense numeric solve machinery:

```text
scaled residual evaluation
central finite differences
damped Gauss-Newton normal equations
partial-pivot dense solve
damping escalation
backtracking
convergence / iteration-limit / numerical-failure state
```

If the current private evaluator is too root-local, refactor its evaluator boundary. Do not copy the optimizer into a second cross-hierarchy solver implementation.

### Block-26 result contract

The solve result must expose enough derived context for Block 27:

```text
selected solve-group relationship identities
complete participating ComponentTransformAuthority input snapshots
at most one proposal per free ComponentTransformAuthority
solve state
iteration count
residual summary
```

A ComponentTransformAuthority snapshot must at least preserve:

```text
assembly_document
ComponentInstanceId
source direct transform
grounding state
component suppression state
```

Block 26 may include relationship and hierarchy-boundary input snapshots needed for future freshness validation, but it must not apply proposals.

Focused tag:

```text
[geometry][assembly-cross-hierarchy-solver]
```

Required proofs include:

- a root/child cross-hierarchy Mate or Distance converging through authority-scoped variables;
- a nested endpoint using the exact parent chain;
- repeated child occurrences sharing one variable block;
- two endpoint occurrences of one authority observing the same perturbation but different parent chains;
- a local child relationship and cross relationship contributing to one mixed residual vector;
- deterministic relationship and variable ordering;
- grounded authorities contributing residual context but no variable block;
- suppressed/inactive relationships already absent through the Block-25 group contract;
- source Project immutability before explicit application;
- no `SubassemblyInstance::transform()` proposal.

## Block 27: Fresh-result application and cross-hierarchy diagnostics

Goal: atomically apply Block-26 proposals and expose rank/remaining-DOF diagnostics over exactly the same authority variable ordering.

### Freshness inputs

Protect complete participating transform-authority inputs.

Protect local relationship records by:

```text
(assembly_document, AssemblyConstraintId)
```

Protect complete project-level `AssemblyHierarchyConstraint` records.

Protect every participating hierarchy boundary on cross-hierarchy endpoint paths with at least:

```text
containing assembly document identity
SubassemblyInstanceId
referenced child assembly document identity
suppression state
boundary RigidTransform
```

Visibility is not a solve input.

### Semantic geometry freshness boundary

The existing local solver has no general PartDocument/feature revision token for target-producing model changes.

Block 27 must explicitly choose one contract:

```text
A. add stable model/target-input revision snapshots used by local and cross-hierarchy solving

or

B. preserve the current local-solver limitation and document that semantic target-producing part edits are outside stale-result detection
```

Option A is preferable before long-lived GUI/background solve results.

### Atomic application

Rules:

1. only converged results may be applied;
2. duplicate authority, relationship, or hierarchy-boundary snapshots are invalid;
3. all protected current inputs must match;
4. every proposal must match one free active transform-authority snapshot;
5. validate every direct proposed transform through the existing component transform contract;
6. apply on a `Project` copy;
7. write at most one transform per `ComponentTransformAuthority`;
8. replace the source Project only after every write succeeds;
9. never persist a composed hierarchy transform;
10. never mutate `SubassemblyInstance::transform()` in this block.

### Diagnostics

Use the exact Block-26 free-authority variable order:

```text
variable_count = 6 * unique_free_active_transform_authority_count
constrained_dof = rank(J)
remaining_dof = variable_count - rank(J)
```

The count is not based on geometric occurrence count.

Focused tags should separate application/freshness from diagnostics if implementation size requires two narrowly scoped test files, but both remain Block 27 unless the canonical plan is deliberately split first.

## Blocks 28+

### 28. Cross-hierarchy joints and nested motion propagation

Deferred until Blocks 23-27 freeze geometric relationship persistence, solve connectivity, authority mapping, and application semantics.

### 29. Component geometry instancing and structured STEP assembly products

Deferred.

### 30. Richer collision/contact and swept-motion analysis

Deferred.

## Persistence rule

Persist model intent.

Current persistent assembly intent includes component placement/state, local geometric constraints, local joint/limit/coordinate records, project-owned child assemblies, rigid subassembly occurrence placement/state, and project-owned cross-hierarchy geometric constraints with exact occurrence-qualified endpoints.

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
endpoint-to-authority mappings
cross-hierarchy solve groups
numeric residuals and Jacobians
solve results and proposals
snapshots and diagnostics
shape caches and posed shapes
analysis products
STEP compounds
```

Only explicit application of a fresh converged result changes persistent component transform intent.

## Next technical step

Implement Block 26 only: shared authority-scoped numeric residual/Jacobian evaluation and solve-result integration for one `AssemblyCrossHierarchySolveGroup`.

Do not add result application or cross-hierarchy diagnostics in the same block.

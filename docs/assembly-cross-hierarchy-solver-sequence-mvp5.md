# Cross-Hierarchy Geometric Constraint Solver Sequence MVP-5

Status: Block 23 is implemented in `docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`. Block 24 is the current next technical step.

This document is the canonical implementation sequence for persistent cross-hierarchy geometric constraints after the read-only target/residual seed in `docs/assembly-cross-hierarchy-relationship-semantics-mvp5.md`.

## Sequencing rule

Cross-hierarchy solving crosses separate authority boundaries:

```text
Core model intent
  -> serialization and structure compatibility
  -> derived relationship-to-variable connectivity
  -> numeric residual/Jacobian execution
  -> freshness validation and atomic application
  -> diagnostics
```

These boundaries remain separate implementation blocks.

The current phase must not introduce a Geometry-owned persistence type, duplicate one persistent transform authority into multiple occurrence variables, or duplicate one local child constraint for every rooted occurrence of its containing child document.

## Frozen identities

Three identities remain distinct.

### Geometric endpoint

Implemented Core contract:

```text
AssemblyHierarchyConstraintEndpoint =
  (occurrence_path,
   local ComponentInstanceId,
   semantic_reference)
```

The empty occurrence path addresses the explicit root assembly occurrence.

Two endpoints may refer to the same child-document component and semantic feature while remaining geometrically distinct because their parent transform chains differ.

Example:

```text
([subassembly.left],  component.shaft, feature.bore.axis)
([subassembly.right], component.shaft, feature.bore.axis)
```

### Geometric component occurrence

Remove only the semantic reference:

```text
HierarchyComponentOccurrence =
  (occurrence_path,
   local ComponentInstanceId)
```

This is rooted geometric occurrence identity.

A local `ComponentInstanceId` alone is insufficient rooted identity.

### Persisted component transform authority

The current document-scoped flexible-child contract stores direct component placement/state once in one assembly document:

```text
ComponentTransformAuthority =
  (assembly_document: DocumentId,
   local ComponentInstanceId)
```

This authority owns:

```text
ComponentInstance::transform()
ComponentInstance::grounding_state()
ComponentInstance::suppression_state()
```

Repeated occurrences of one child assembly document may map different geometric occurrences to one transform authority:

```text
([left],  component.shaft)
  -> (assembly.gearbox, component.shaft)

([right], component.shaft)
  -> (assembly.gearbox, component.shaft)
```

Until occurrence-local internal pose overrides exist, these two occurrences must not become independent six-variable blocks.

## Local relationships are model-definition intent

A local `AssemblyConstraint` belongs to one containing `AssemblyDocument`.

Identity for future cross-hierarchy solve collection is:

```text
LocalRelationship =
  (assembly_document: DocumentId,
   AssemblyConstraintId)
```

If `assembly.gearbox` is instantiated twice, one local shaft/bearing constraint still exists once in the shared child document.

It must be evaluated once in the containing assembly document's local assembly space.

Duplicating it once per rooted child occurrence would create artificial residual weighting and redundancy.

## Project-level cross-hierarchy relationship identity

Block 23 implemented one persistent project-owned record:

```text
AssemblyHierarchyConstraint
```

Its endpoints are exact Core-owned occurrence-qualified endpoint values.

Its id is unique inside the project-level cross-hierarchy constraint collection.

Local constraint ids remain scoped by their containing assembly document.

A cross-hierarchy relationship is evaluated in root-assembly space because target A and target B may use different parent transform chains.

## Relationship-to-transform-authority incidence

A pure occurrence graph loses persistent mutation authority. A pure authority graph loses occurrence-specific endpoint geometry.

Future solve partitioning therefore uses a deterministic bipartite incidence model:

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
  = project-level AssemblyHierarchyConstraint id
```

Variable-authority nodes are:

```text
ComponentTransformAuthority
  = (assembly_document, ComponentInstanceId)
```

Each relationship is incident to every unique transform authority whose candidate local component transform can change its residual.

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

The relationship has one unique authority incidence but still two different endpoint transform chains. Its residual can therefore be nonzero and numerically meaningful.

No synthetic constraint or fake residual row is introduced for shared-authority coupling.

## Block 23: Core endpoint and persistent project-level intent — Implemented

Canonical document: `docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`.

Implemented:

- Core-owned `AssemblyHierarchyConstraintEndpoint`;
- exact empty-root and ordered non-empty occurrence-path identity;
- persistent `AssemblyHierarchyConstraint` records for Mate, Concentric, Distance, Insert, and Angle;
- reuse of the established Distance/Angle value-family validation contract;
- Project-owned `cross_hierarchy_constraints` collection APIs;
- project-level cross-hierarchy id uniqueness independent from local document-scoped ids;
- no hierarchy or semantic geometry resolution during record creation/addition;
- no transform mutation;
- Core-record to read-only Geometry-query bridge.

Focused tag:

```text
[core][assembly-cross-hierarchy-intent]
```

No JSON or structure validation is implemented by Block 23.

## Block 24: Additive JSON and project-structure validation — Next

Goal: make Block-23 intent roundtrip safely and validate endpoint structure without adding graph or solver behavior.

Planned additive project field:

```text
cross_hierarchy_constraints[]
```

Representative endpoint JSON:

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

Required behavior:

1. files without `cross_hierarchy_constraints` load with an empty project collection;
2. serialization emits the collection additively;
3. roundtrip preserves relationship id/name/type/state;
4. target A/B order is preserved exactly;
5. every endpoint preserves occurrence-path element order exactly;
6. Distance/Angle quantities roundtrip through the existing unit spelling rules;
7. duplicate project-level cross-hierarchy ids fail closed;
8. endpoint paths resolve in the complete rooted hierarchy;
9. the reached assembly document contains the addressed local component;
10. loading does not resolve semantic target geometry;
11. loading does not mutate component or subassembly transforms;
12. `Project::validate_assembly_structure()` includes cross-hierarchy endpoint structure after member/component/local relationship/joint/hierarchy validation has established the underlying assembly tree.

Validation order must avoid asking target paths to resolve against an invalid or cyclic hierarchy.

The structural validator may use `AssemblyHierarchyTraversal`; it must not call OCCT or any Geometry-layer target resolver.

Focused tag:

```text
[core][assembly-cross-hierarchy-json]
```

`docs/file-format.md` becomes canonical for exact JSON spelling when this block is implemented.

Block 24 must not add incidence graphs, numeric variables, solving, snapshots, diagnostics, or application.

## Block 25: Relationship-to-authority incidence and active solve groups

Goal: derive deterministic cross-hierarchy solve connectivity without numeric residual/Jacobian evaluation.

Required relationship nodes:

```text
LocalRelationship
  = (assembly_document: DocumentId, AssemblyConstraintId)

CrossHierarchyRelationship
  = project-level AssemblyHierarchyConstraint id
```

Required authority nodes:

```text
ComponentTransformAuthority
  = (assembly_document: DocumentId, ComponentInstanceId)
```

Required behavior:

1. collect active local constraints once per owned `AssemblyDocument`, not once per rooted occurrence;
2. collect active project-level cross-hierarchy constraints;
3. local relationship participation uses local relationship state and local component suppression;
4. cross-hierarchy endpoint participation additionally requires every `SubassemblyInstance` on the exact endpoint path to be active;
5. visibility does not filter geometric solve participation;
6. map every participating local target or hierarchy endpoint to its transform authority;
7. add one incidence edge from a relationship to each unique authority its residual depends on;
8. derive deterministic connected components of the bipartite incidence graph;
9. expose only groups containing at least one active project-level cross-hierarchy relationship to the cross-hierarchy solve API;
10. pure-local groups remain ordinary local solver work;
11. persist no graph or incidence cache.

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

Required proofs include repeated child occurrences mapping to one shared authority, one local child constraint appearing once despite repeated occurrences, local/cross relationships joining through authority incidence, one cross relationship with two endpoint occurrences of the same authority retaining both endpoints, path suppression filtering only affected cross relationships, and insertion-order-independent groups.

Focused tag:

```text
[core][assembly-cross-hierarchy-graph]
```

## Block 26: Shared numeric residual/Jacobian and solve-result integration

Goal: solve one exact Block-25 incidence group without applying the result.

Variable identity is:

```text
ComponentTransformAuthority =
  (assembly_document: DocumentId,
   local ComponentInstanceId)
```

Each unique free active authority contributes exactly six variables:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

A repeated occurrence adds no second variable block when it maps to an authority already in the group.

Local relationships are evaluated once in their containing assembly document's local assembly space through the existing local target/equation builders.

Cross-hierarchy relationships are evaluated through their exact Core endpoints in root-assembly space:

```text
candidate local component transform
  -> endpoint occurrence path
  -> exact parent transform chain
  -> existing semantic target resolver
  -> existing family residual formula
```

The shared residual vector preserves deterministic relationship order, family-specific scalar order, and established length scaling.

The finite-difference Jacobian perturbs one transform-authority variable at a time. Every residual depending on that authority must observe the same candidate perturbation, including residuals using different rooted occurrences of one shared child component.

Reuse the existing dense linear solve, damping escalation, backtracking, convergence, and iteration-state machinery. Refactor the relationship evaluator boundary if necessary; do not copy the optimizer.

The result must expose:

```text
selected relationship identities
complete transform-authority input snapshots
at most one proposal per free transform authority
solve state
iteration count
residual summary
```

Source `Project` remains immutable.

Focused tag:

```text
[geometry][assembly-cross-hierarchy-solver]
```

Required proofs include root-to-child, nested, mixed local/cross groups, all five existing geometric families, one shared variable block for repeated occurrences, one local child residual block despite repeated occurrences, shared-authority perturbation propagation across multiple endpoint paths, deterministic solving, and unchanged rigid `SubassemblyInstance` boundary transforms.

## Block 27: Fresh-result application and cross-hierarchy diagnostics

Goal: validate and atomically apply Block-26 proposals and expose rank/DOF diagnostics over exactly the same transform-authority ordering.

Every transform-authority snapshot must preserve:

```text
assembly document id
ComponentInstanceId
source transform
grounding state
component suppression state
```

There must be at most one snapshot and one proposal per transform authority.

The result must also snapshot persistent relationship intent used by the selected solve group.

Local relationship identity includes:

```text
(assembly_document, AssemblyConstraintId)
```

Cross-hierarchy relationship snapshots preserve the complete project-level record including both occurrence-qualified endpoints.

Every `SubassemblyInstance` boundary used by a participating cross-hierarchy endpoint path can affect root-space residual geometry or active participation. Relevant boundary snapshots must therefore protect at least:

```text
containing assembly document id
SubassemblyInstanceId
referenced child assembly document id
suppression state
boundary RigidTransform
```

Visibility is not a solve input and need not make a result stale.

### Semantic geometry freshness boundary

The current local solver does not expose a general `PartDocument`/feature revision token for semantic target-producing model edits.

Block 27 must explicitly choose one contract:

```text
A. introduce stable model/target-input revision snapshots
   used by local and cross-hierarchy solve application

or

B. preserve and document the current limitation that
   target-producing part edits are outside stale-result detection
```

Do not claim geometry-complete stale detection without implementing the required authority.

### Atomic application

Rules:

1. only converged results may be applied;
2. duplicate authority, relationship, or hierarchy-boundary snapshots are invalid;
3. all snapshotted current inputs must match;
4. every proposal must match one free active authority snapshot;
5. proposed direct transforms pass the existing component transform validation contract;
6. application occurs on a `Project` copy;
7. write at most one direct local component transform per authority;
8. commit the source project only after every write succeeds;
9. never persist a composed hierarchy transform;
10. never mutate `SubassemblyInstance::transform()` in this block.

Diagnostics use:

```text
variable_count = 6 * unique_free_active_transform_authority_count
constrained_dof = rank(J)
remaining_dof = variable_count - rank(J)
```

This is intentionally not based on the number of free geometric occurrences.

Focused tags:

```text
[geometry][assembly-cross-hierarchy-apply]
[geometry][assembly-cross-hierarchy-diagnostics]
```

## Deferred after Block 27

- cross-hierarchy joints and nested motion propagation;
- occurrence-local internal child component pose overrides;
- persistent rigid/flexible occurrence mode;
- whole-`SubassemblyInstance` rigid solve variables and grounding;
- multi-turn and time-based motion studies;
- component geometry instancing;
- structured STEP assembly products;
- richer contact classification and swept-motion analysis.

## Next technical step

Implement **Block 24 only**:

```text
AssemblyHierarchyConstraint project JSON
  -> additive cross_hierarchy_constraints[]
  -> exact endpoint and target-order roundtrip
  -> backward-compatible absent-field loading
  -> hierarchy path and reached-component structure validation
```

Do not add incidence graphs, numeric variables, solving, snapshots, diagnostics, or application in the same block.

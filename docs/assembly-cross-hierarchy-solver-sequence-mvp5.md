# Cross-Hierarchy Geometric Constraint Solver Sequence MVP-5

Status: planning contract for the blocks after `docs/assembly-cross-hierarchy-relationship-semantics-mvp5.md`. This document corrects and freezes the identity boundaries required before persistent cross-hierarchy geometric constraints enter the numeric solver.

## Why the next work is split

The next assembly work crosses four separate authority boundaries:

```text
persistent core intent
  -> additive project JSON
  -> derived relationship/participation connectivity
  -> numeric variables and residual/Jacobian evaluation
  -> stale-result validation and atomic application
```

These boundaries must not be introduced as one implementation block. The local assembly path was built incrementally for the same reason: persistent intent, graph semantics, target/residual semantics, solving, and application each have different failure and compatibility contracts.

Every block below requires its own canonical implementation document and focused test tag before the following block starts.

## Frozen identities

Cross-hierarchy solving needs three different identities. They are related but are not interchangeable.

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

### 2. Relationship-node identity

For graph connectivity, remove only the semantic feature reference:

```text
HierarchyComponentOccurrence =
  (occurrence_path, local ComponentInstanceId)
```

The left and right shaft occurrences above are distinct graph nodes.

A local `ComponentInstanceId` alone is never sufficient graph-node identity across the rooted hierarchy.

### 3. Transform-authority identity

Under the current document-scoped flexible-subassembly contract, persisted component placement is owned by one component record in one assembly document:

```text
ComponentTransformAuthority =
  (AssemblyDocumentId, local ComponentInstanceId)
```

This is the identity of:

```text
ComponentInstance::transform()
ComponentInstance::grounding_state()
ComponentInstance::suppression_state()
```

Repeated occurrences of one child `AssemblyDocument` may therefore have different `HierarchyComponentOccurrence` identities while mapping to the same `ComponentTransformAuthority`.

Example:

```text
([left],  component.shaft) -> (assembly.gearbox, component.shaft)
([right], component.shaft) -> (assembly.gearbox, component.shaft)
```

The two occurrence nodes have different root-space geometry but one persistent local transform authority.

This distinction is mandatory until occurrence-local internal pose overrides are implemented.

## Critical non-aliasing rule

Do not solve repeated occurrences as independent transform variables merely because their occurrence paths differ.

Incorrect:

```text
variable([left],  component.shaft)  = 6 independent values
variable([right], component.shaft)  = 6 independent values
```

There is currently no persistent model location capable of storing two independent results for those variables.

Correct under the current persistence contract:

```text
variable(assembly.gearbox, component.shaft) = 6 values
```

Every endpoint occurrence that maps to that authority reads the same candidate local component transform and then evaluates its own parent transform chain.

For example, one candidate shaft translation may be evaluated through both:

```text
T_left(T_candidate(p))
T_right(T_candidate(p))
```

The resulting root-space target geometry differs even though the direct local component variable is shared.

## Solve connectivity versus relationship connectivity

A pure occurrence-node relationship graph is not sufficient to partition numeric solves.

Consider two relationship sets that touch different repeated occurrences of the same child component:

```text
constraint A -> ([left],  component.shaft)
constraint B -> ([right], component.shaft)
```

The occurrence relationship graph may place A and B in different connected components. Numerically they still share one transform authority and cannot be solved independently.

Therefore the future pipeline must expose two derived concepts:

```text
relationship connectivity
  = occurrence-qualified nodes + relationship edges

solve connectivity
  = relationship connectivity
    + equality of ComponentTransformAuthority identity
```

Authority equality is not a geometric constraint and contributes no residual row. It is only a solve-partition coupling rule.

The implementation may derive solve connectivity through an incidence graph, union-find, or another deterministic mechanism, but the result must be equivalent to transitive closure over:

```text
relationship endpoint adjacency
shared transform-authority adjacency
```

## Active and suppressed participation

Participation is path-sensitive before authority deduplication.

A relationship endpoint occurrence is active only when:

```text
all SubassemblyInstance occurrences on occurrence_path are active
and
local ComponentInstance suppression_state == active
```

Visibility remains a presentation/export property and must not remove a geometric relationship from solving.

If one occurrence path is suppressed but another active occurrence maps to the same transform authority, the active occurrence may still make that authority participate.

Only after endpoint/path participation is determined may active occurrence nodes be mapped and deduplicated by transform authority for numeric variables.

Grounding is authority-owned because `grounding_state` belongs to the shared `ComponentInstance` record. A grounded transform authority contributes no numeric variables regardless of how many active rooted occurrences expose it.

## Block 23: Core endpoint contract and persistent project-level intent

Goal: introduce persistent cross-hierarchy geometric relationship intent in the core without JSON, graph, or solving.

Required work:

1. Move or extract the frozen endpoint value contract into the core layer. The current `blcad::geometry::AssemblyHierarchyConstraintTarget` is a read-only geometry-query seed and must not become save-format authority by serializing a Geometry-layer type.
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
5. Make `Project` own the new cross-hierarchy constraint collection because endpoints may address different assembly-document occurrences.
6. Validate record-local value-family rules exactly like local `AssemblyConstraint` intent.
7. Adding a record must not mutate component or subassembly transforms.
8. Keep target geometry, hierarchy descriptors, graph connectivity, and solver state derived.

Acceptance examples:

```text
root component -> child occurrence component
left occurrence -> right occurrence of the same child document
nested child occurrence -> root component
```

All three must be representable as persistent intent without resolving geometry.

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
2. roundtrip preserves endpoint path order and target A/B order exactly;
3. endpoint occurrence paths must resolve in the complete rooted project hierarchy;
4. the local component must exist in the assembly document reached by the path;
5. relationship ids must be unique in the project-level collection;
6. local and project-level constraint ids may remain separately scoped unless a later public command requires project-global relationship ids;
7. unsupported value-family combinations fail closed;
8. loading never resolves semantic geometry and never mutates transforms.

Focused test tag planned:

```text
[core][assembly-cross-hierarchy-json]
```

`docs/file-format.md` becomes canonical for the exact JSON spelling when this block is implemented. Until then the shape above is a planning contract, not a current save format.

## Block 25: Occurrence relationship graph and solve-participation mapping

Goal: derive deterministic connectivity without numeric residual/Jacobian evaluation.

The graph must use:

```text
HierarchyComponentOccurrence =
  (occurrence_path, local ComponentInstanceId)
```

It must derive rooted occurrences deterministically from `AssemblyHierarchyTraversal` and materialize two relationship sources:

```text
1. local active AssemblyDocument constraints lifted once per rooted assembly occurrence
2. active project-level cross-hierarchy constraints
```

This is necessary so a local relationship and a cross-hierarchy relationship can belong to one transitive solve group.

Repeated occurrences of one child document receive separate lifted local relationship edges because they have separate root-space geometry.

The graph must then derive active participation using path suppression and local component suppression. Inactive relationships are excluded.

Separately, the block must map every active occurrence node to:

```text
ComponentTransformAuthority =
  (AssemblyDocumentId, local ComponentInstanceId)
```

Finally it must expose deterministic solve groups coupled by both:

```text
relationship adjacency
shared transform-authority identity
```

Shared authority coupling contributes no residual edge.

Required deterministic ordering:

```text
occurrence paths lexicographically by SubassemblyInstanceId sequence
then local ComponentInstanceId
then relationship id
```

Focused test tag planned:

```text
[core][assembly-cross-hierarchy-graph]
```

Required proofs include:

- insertion-order-independent graph output;
- root/child and nested endpoints;
- repeated child occurrences remain distinct occurrence nodes;
- repeated occurrence nodes map to one shared transform authority;
- two otherwise disconnected relationship sets become one solve group when they share a transform authority;
- suppression of one path does not suppress another active path sharing the same authority;
- visibility does not remove relationships;
- no persisted graph cache.

## Block 26: Shared numeric residual/Jacobian and solve-result integration

Goal: solve one exact block-25 solve group while keeping result application separate.

Variable identity is:

```text
ComponentTransformAuthority =
  (AssemblyDocumentId, local ComponentInstanceId)
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

A repeated occurrence does not add a second six-variable block when it maps to an already-present authority.

Residual evaluation remains occurrence-sensitive:

1. take the candidate direct local transform for the endpoint's transform authority;
2. combine it with that endpoint occurrence's own parent transform chain;
3. resolve/evaluate target geometry in root-assembly space;
4. reuse the existing Mate, Distance, Angle, Concentric, or Insert residual descriptor/formula;
5. flatten with the existing length scaling rules.

The finite-difference Jacobian perturbs one transform authority variable at a time. Every residual row from every endpoint occurrence that maps to that authority must observe the perturbation.

The block should reuse the existing linear solve, damping escalation, backtracking, convergence, and iteration-state machinery rather than introduce a second optimizer.

Result identity must distinguish geometry context from mutation authority. A result needs enough derived context to expose:

```text
selected solve-group relationship identity
occurrence-sensitive residual context
complete ComponentTransformAuthority snapshots
one transform proposal per free ComponentTransformAuthority
solve state / iteration count / residual summary
```

The source `Project` remains immutable.

Focused test tag planned:

```text
[geometry][assembly-cross-hierarchy-solver]
```

Required numeric proofs include:

- one root-to-child Mate solve;
- one nested cross-hierarchy solve;
- mixed local plus cross-hierarchy constraints in one solve group;
- all five existing geometric residual families entering the shared numeric path;
- repeated child occurrences sharing one six-variable transform-authority block;
- a perturbation of one shared authority affecting residual rows from multiple occurrence paths;
- deterministic repeated solving;
- unchanged rigid `SubassemblyInstance` boundary transforms.

## Block 27: Fresh-result application and cross-hierarchy diagnostics

Goal: atomically apply block-26 proposals and expose derived rank/DOF diagnostics over the same variable authority ordering.

Application must validate complete authority snapshots against the current project:

```text
AssemblyDocumentId
ComponentInstanceId
source transform
grounding state
suppression state
```

It must also validate any hierarchy/relationship context required to prove that the result still addresses the same solve inputs. At minimum, changed or missing relationship intent, changed occurrence-path resolution, or changed endpoint target identity must make the result stale.

Application rules:

1. only converged results may be applied;
2. duplicate authority snapshots or proposals are invalid;
3. every proposal must match one free active authority snapshot;
4. each proposed transform is validated through the existing component transform contract;
5. apply on a `Project` copy;
6. write at most one transform per `ComponentTransformAuthority`;
7. replace the source project only after all proposals succeed;
8. never write a composed hierarchy transform;
9. never mutate `SubassemblyInstance::transform()` in this block.

Diagnostics use exactly the block-26 variable ordering:

```text
variable_count = 6 * unique_free_active_transform_authority_count
constrained_dof = rank(J)
remaining_dof = variable_count - rank(J)
```

This is intentionally not:

```text
6 * free_active_occurrence_node_count
```

when repeated occurrence nodes share a transform authority.

Focused test tags planned:

```text
[geometry][assembly-cross-hierarchy-apply]
[geometry][assembly-cross-hierarchy-diagnostics]
```

Required proofs include stale transform, grounding, suppression, relationship, and occurrence-path rejection; atomic no-partial-mutation behavior; repeated-occurrence authority deduplication; correct rank dimensions; and posed-leaf/export consumers observing applied child-document transforms normally.

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

Implement **block 23 only**: extract the frozen occurrence-qualified endpoint value contract into the core layer and add persistent project-owned cross-hierarchy geometric constraint intent. Do not add JSON, graph connectivity, numeric variables, solving, snapshots, or application in the same block.

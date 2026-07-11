# Cross-Hierarchy Relationship-to-Authority Incidence and Solve Groups MVP-5

Status: implemented as Block 25 of `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

This document is canonical for the derived Core connectivity layer between persistent local/project-level geometric relationship intent and future cross-hierarchy numeric solving.

## Scope

Block 25 derives deterministic active relationship connectivity without resolving semantic target geometry and without executing residuals, Jacobians, or solver iterations.

The public entry point is:

```text
AssemblyCrossHierarchyConstraintGraph::build(const Project&)
```

The graph is rebuilt from current Project intent. It is never serialized or cached as model authority.

## Identity split

Three existing identities remain distinct.

### Geometric endpoint

```text
AssemblyHierarchyConstraintEndpoint =
  (occurrence_path,
   local ComponentInstanceId,
   semantic_reference)
```

Endpoint identity preserves exact rooted geometry context.

### Geometric component occurrence

```text
(occurrence_path,
 local ComponentInstanceId)
```

Two repeated child occurrences may be distinct geometric occurrences.

### Component transform authority

Block 25 introduces the derived value identity:

```text
ComponentTransformAuthority =
  (assembly_document: DocumentId,
   local ComponentInstanceId)
```

This identifies the persistent `ComponentInstance` record whose direct local transform, grounding state, and suppression state are authoritative.

The identity itself is derived and unpersisted.

Repeated rooted occurrences may map to one authority:

```text
([subassembly.left],  component.shaft)
([subassembly.right], component.shaft)

        both map to

(assembly.gearbox, component.shaft)
```

Block 25 never creates one authority per rooted occurrence.

## Relationship identity

The graph uses a variant of two relationship identities.

Local relationship:

```text
AssemblyLocalRelationshipIdentity =
  (assembly_document: DocumentId,
   AssemblyConstraintId)
```

Project-level cross-hierarchy relationship:

```text
AssemblyProjectCrossHierarchyRelationshipIdentity =
  (AssemblyConstraintId)
```

The public variant is:

```text
AssemblyRelationshipIdentity
```

A local `AssemblyConstraint` is collected once from its containing `AssemblyDocument`. A shared child document instantiated multiple times does not duplicate its local relationship nodes.

## Participation rules

`build()` first requires complete `Project::validate_assembly_structure()` success.

### Local relationships

A local constraint participates only when:

```text
constraint.state == Active
and target A component suppression == Active
and target B component suppression == Active
```

Visibility is ignored.

Grounding is not a graph-participation filter. Grounding controls future numeric variable creation, not whether a geometric relationship exists in the active incidence component.

### Project-level cross-hierarchy relationships

A project-level relationship participates only when:

```text
constraint.state == Active
and every SubassemblyInstance on target A path is Active
and target A addressed component is Active
and every SubassemblyInstance on target B path is Active
and target B addressed component is Active
```

Visibility is ignored at every path and component level.

If one endpoint path or addressed component is suppressed, the complete cross-hierarchy relationship is absent from the active graph.

Suppression of one rooted path does not suppress another occurrence path that reaches the same child document.

## Exact endpoint-to-authority resolution

For one `AssemblyHierarchyConstraintEndpoint`, Block 25 starts at the explicit root assembly and follows each authored `SubassemblyInstanceId` in `occurrence_path` in order.

The assembly document reached after the complete path supplies the authority document identity:

```text
ComponentTransformAuthority{
  reached_assembly_document.id(),
  endpoint.component_instance()
}
```

The structural existence of the path and component was already established by Block 24. Block 25 still fails closed if the derived resolution unexpectedly cannot reproduce that structure.

No semantic reference is parsed and no PartDocument feature geometry is inspected.

## Unique relationship-to-authority incidence

The bipartite edge type is:

```text
AssemblyRelationshipAuthorityIncidence
```

One edge means:

```text
this relationship residual can depend on
this direct component transform authority
```

There is at most one edge for one `(relationship, authority)` pair.

Example:

```text
target A = ([left],  component.shaft, feature.left.axis)
target B = ([right], component.shaft, feature.right.axis)
```

Both endpoints may map to:

```text
(assembly.gearbox, component.shaft)
```

The graph therefore contains exactly one unique relationship-to-authority incidence for that relationship and authority.

Block 25 does not add a synthetic second authority edge and does not add a fake residual relationship.

## Endpoint mappings preserve occurrence context

Unique incidence cannot replace endpoint geometry identity.

The separate derived mapping type is:

```text
AssemblyCrossHierarchyEndpointAuthorityMapping
```

It stores:

```text
cross-hierarchy constraint id
TargetA | TargetB
complete AssemblyHierarchyConstraintEndpoint
derived ComponentTransformAuthority
```

The previous same-authority example therefore retains two mappings:

```text
TargetA
  ([left], component.shaft, feature.left.axis)
  -> (assembly.gearbox, component.shaft)

TargetB
  ([right], component.shaft, feature.right.axis)
  -> (assembly.gearbox, component.shaft)
```

while the bipartite graph still has one unique authority incidence for the relationship.

This split is required by Block 26: candidate transform mutation is authority-scoped, while root-space target evaluation remains endpoint-occurrence-scoped.

## Connected cross-hierarchy solve groups

`AssemblyCrossHierarchyConstraintGraph` derives connected components over both bipartite node sets:

```text
relationship nodes <-> ComponentTransformAuthority nodes
```

A connected component is exposed as `AssemblyCrossHierarchySolveGroup` only when it contains at least one `AssemblyProjectCrossHierarchyRelationshipIdentity`.

Pure-local connected components are not returned by `solve_groups()`.

They remain ordinary local solver work.

Example:

```text
local gearbox shaft/bearing relationship
        |
        v
(assembly.gearbox, component.shaft)
        ^
        |
cross left/root relationship
        |
cross right/root relationship
```

All three relationships belong to one cross-hierarchy solve group because their residual dependencies meet at the shared shaft transform authority.

No additional constraint is invented to join the group.

## Deterministic ordering

All public graph vectors and group contents are independent of Project insertion order.

### Relationship order

Relationship kind order:

```text
1. local
2. project-level cross-hierarchy
```

Local relationship key:

```text
assembly_document.value()
AssemblyConstraintId.value()
```

Cross-hierarchy relationship key:

```text
AssemblyConstraintId.value()
```

All string keys use lexicographic ascending order.

### Authority order

```text
assembly_document.value()
ComponentInstanceId.value()
```

lexicographic ascending.

### Incidence order

```text
relationship key
then authority key
```

### Endpoint mapping order

```text
cross-hierarchy AssemblyConstraintId
then TargetA before TargetB
```

### Solve-group order

Relationships and authorities are sorted inside every group by the rules above.

Cross-hierarchy solve groups are sorted by the first relationship in each already sorted group. The first authority is the deterministic fallback if relationship keys compare equal.

Connected components are disjoint, so two valid groups cannot share the same first relationship identity.

## Public derived result

`AssemblyCrossHierarchyConstraintGraph` exposes:

```text
relationships()
authorities()
incidences()
endpoint_mappings()
solve_groups()
```

and corresponding counts.

The object owns only derived value copies. Building it never mutates the source `Project`.

## Persistence boundary

Persistent model intent remains:

```text
AssemblyDocument local AssemblyConstraint records
Project-owned AssemblyHierarchyConstraint records
ComponentInstance transform/state
SubassemblyInstance path boundary placement/state
```

Derived and unpersisted Block-25 data is:

```text
ComponentTransformAuthority values
AssemblyRelationshipIdentity values
relationship-to-authority incidences
cross-hierarchy endpoint-to-authority mappings
connected cross-hierarchy solve groups
```

None is emitted to Project JSON.

## Failure policy

Graph construction fails closed when complete Project structure is invalid.

After successful structure validation, it also fails if an expected local target component, cross-hierarchy occurrence path, reached assembly document, or addressed endpoint component cannot be reproduced during graph derivation.

Block 25 does not fail on unsupported semantic feature strings because it does not evaluate semantic geometry.

## Focused coverage

Test file:

```text
tests/core/assembly_cross_hierarchy_constraint_graph_tests.cpp
```

Focused tag:

```text
[core][assembly-cross-hierarchy-graph]
```

The suite proves:

- root/child and nested endpoint authority mapping;
- repeated child occurrences preserving separate endpoint paths;
- repeated endpoint occurrences mapping to one shared transform authority;
- exactly one unique incidence when target A and target B share one authority;
- two endpoint mappings remaining for that same-authority relationship;
- one child-local constraint appearing once despite repeated child occurrences;
- local and cross-hierarchy relationships joining through shared authority incidence;
- two cross-hierarchy relationships on different occurrence paths joining through one shared authority;
- endpoint-path suppression removing only relationships that use the suppressed path;
- local component suppression removing local relationships touching it and cross relationships addressing it;
- visibility not changing graph participation;
- inactive local and project-level relationships not participating;
- pure-local connected components not appearing in `solve_groups()`;
- insertion-order-independent relationships, authorities, incidences, mappings, and groups;
- source Project transform immutability;
- fail-closed invalid Project structure.

Focused command:

```bash
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-graph]"
```

## Explicitly deferred

Block 25 does not implement:

- semantic face/axis/seat geometry execution for a solve group;
- residual flattening for mixed local and cross-hierarchy relationships;
- numeric transform-authority variable vectors;
- finite-difference Jacobians over transform authorities;
- Gauss-Newton solver execution;
- component authority snapshots;
- relationship or hierarchy-boundary snapshots;
- transform proposals;
- result application;
- cross-hierarchy rank/remaining-DOF diagnostics;
- cross-hierarchy joints or nested motion propagation.

## Next technical step

Implement Block 26 only:

```text
one AssemblyCrossHierarchySolveGroup
  -> unique free active ComponentTransformAuthority variables
  -> local relationship residuals in containing-document local space
  -> cross-hierarchy relationship residuals in root-assembly space
  -> shared scaled residual vector
  -> central finite-difference Jacobian by authority variable
  -> existing numeric solve engine
  -> unapplied authority-scoped transform proposals
```

Reuse the existing five geometric residual families and numeric optimizer. Do not apply proposals, add diagnostics, or silently claim geometry-complete stale-result freshness in Block 26.

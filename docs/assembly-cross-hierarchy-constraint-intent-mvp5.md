# Cross-Hierarchy Geometric Constraint Intent MVP-5

Status: implemented Block 23 of `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

## Goal

This block introduces persistent Core-owned geometric relationship intent between exact rooted assembly occurrences without introducing JSON, hierarchy-structure validation, graph connectivity, numeric solving, snapshots, diagnostics, or result application.

The boundary is:

```text
Core endpoint identity
  -> persistent Project-owned relationship record
  -> read-only Geometry query bridge
```

No component or subassembly transform changes when intent is created or added.

## Core endpoint identity

`AssemblyHierarchyConstraintEndpoint` is the Core-owned value contract for one semantic geometric endpoint anywhere in the rooted assembly occurrence tree:

```text
(occurrence_path, local ComponentInstanceId, semantic_reference)
```

The empty occurrence path addresses the explicit root assembly occurrence.

A non-empty path is stored in exact root-to-current `SubassemblyInstanceId` order.

Example:

```text
[
  subassembly.outer,
  subassembly.inner
]
component.shaft
feature.bore.axis
```

The constructor validates only value-local identity rules:

- every `SubassemblyInstanceId` in the path is non-empty;
- the local `ComponentInstanceId` is non-empty;
- the semantic reference string is non-empty.

It deliberately does not build `AssemblyHierarchyTraversal`, resolve the occurrence path, find a component, or resolve semantic geometry.

That structure-validation boundary belongs to Block 24.

## Persistent project-level relationship intent

`AssemblyHierarchyConstraint` stores:

```text
AssemblyConstraintId id
name
AssemblyConstraintType type
AssemblyHierarchyConstraintEndpoint target_a
AssemblyHierarchyConstraintEndpoint target_b
AssemblyConstraintState state
optional distance
optional angle
```

Supported families are exactly the existing local geometric families:

```text
Mate
Concentric
Distance
Insert
Angle
```

The record reuses the established local `AssemblyConstraint` value-family contract:

```text
Mate        -> no distance, no angle
Concentric  -> no distance, no angle
Insert      -> no distance, no angle
Distance    -> required LengthMm distance, no angle
Angle       -> required AngleDeg angle, no distance
```

Target A/B order is preserved exactly.

The record is solver-independent model intent. Construction never resolves geometry and never changes authored transforms.

## Project ownership and id scope

`Project` owns the collection because one relationship may address endpoints in different assembly-document occurrences.

Public Core APIs are:

```text
add_cross_hierarchy_constraint
cross_hierarchy_constraints
cross_hierarchy_constraint_count
find_cross_hierarchy_constraint
```

Cross-hierarchy relationship ids are unique within the project-level cross-hierarchy collection.

Local `AssemblyConstraintId` values remain scoped by their containing `AssemblyDocument`.

Therefore this is legal:

```text
assembly.root / constraint.shared
project cross-hierarchy collection / constraint.shared
```

The two records live in different identity scopes.

Adding a project-level relationship currently validates only record-local intent and project-level cross-hierarchy id uniqueness.

It does not yet validate:

```text
occurrence path exists
reached assembly document exists
local component exists in reached assembly
semantic feature target resolves
```

Those checks are intentionally Block 24 structure/loading work.

## Relationship to the existing read-only Geometry layer

`AssemblyHierarchyConstraintTarget` remains the existing derived Geometry query seed used by `AssemblyHierarchyConstraintTargetResolver`.

It is not persistence authority.

The Core-owned persistence value is:

```text
AssemblyHierarchyConstraintEndpoint
```

`AssemblyHierarchyConstraintQuery::create` now accepts either Core endpoints directly or one complete `AssemblyHierarchyConstraint` record.

The bridge converts Core identity into the existing read-only query seed and then reuses the previously implemented root-space target and residual semantics.

Conceptually:

```text
AssemblyHierarchyConstraint
  -> AssemblyHierarchyConstraintQuery
  -> exact occurrence resolution
  -> existing local semantic target resolver
  -> root-space hierarchy transform evaluation
  -> Mate / Distance / Angle / Concentric / Insert residual descriptor
```

The bridge adds no graph participation policy and no solver execution.

Inactive persistent relationships may still be converted into a pure mathematical query; active/inactive participation belongs to the future incidence layer.

## Persistence boundary

Persistent in-memory model intent now includes:

```text
Project
  cross_hierarchy_constraints[]
    id
    name
    type
    target_a occurrence-qualified endpoint
    target_b occurrence-qualified endpoint
    state
    optional distance / angle
```

The current project JSON format does not serialize this collection yet.

This is deliberate sequencing, not a claim that the format already roundtrips the new intent.

Current save/load behavior remains unchanged until Block 24 introduces an additive field and fail-closed structure validation.

Derived and unpersisted data still includes:

```text
AssemblyHierarchyConstraintQuery
Geometry query target seeds
hierarchy traversal descriptors
resolved containing assembly documents
resolved semantic target geometry
root-space point/vector/plane/axis descriptors
residual descriptors
relationship-to-authority incidence
numeric variables
Jacobian rows
solve results
snapshots
proposals
application state
```

## Failure policy

Core endpoint creation fails on empty path ids, an empty component id, or an empty semantic reference.

Relationship creation fails on the same identity/name/value-family violations as local `AssemblyConstraint` intent.

`Project::add_cross_hierarchy_constraint` fails when the project-level cross-hierarchy collection already contains the same `AssemblyConstraintId`.

No failure in this block depends on OCCT, hierarchy traversal, part geometry, or target resolution.

## Focused coverage

`tests/core/assembly_cross_hierarchy_constraint_tests.cpp` uses:

```text
[core][assembly-cross-hierarchy-intent]
```

The focused suite proves:

- root empty-path endpoint identity;
- exact nested occurrence-path ordering;
- empty occurrence/component/semantic identity rejection;
- shared Mate/Concentric/Distance/Insert/Angle value-family rules;
- inactive state preservation;
- Distance and Angle quantity preservation;
- target A/B endpoint preservation;
- project-owned collection access and lookup;
- project-level cross-hierarchy id uniqueness;
- local and project-level relationship ids using separate scopes;
- root-to-child, repeated-left-to-right, and nested-to-root intent being representable without path/component/geometry resolution;
- component and `SubassemblyInstance` transforms remaining unchanged when intent is added.

Focused command:

```bash
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-intent]"
```

## Explicitly deferred

- project JSON serialization of cross-hierarchy constraints;
- backward-compatible loading without the new collection;
- occurrence-path and reached-component structure validation;
- active/suppressed relationship participation;
- relationship-to-transform-authority incidence;
- numeric variable ordering;
- shared residual/Jacobian flattening for persistent cross-hierarchy records;
- solve results and freshness snapshots;
- atomic result application;
- cross-hierarchy rank/DOF diagnostics;
- cross-hierarchy joints and nested motion.

## Next technical step

Implement Block 24 only:

```text
additive cross_hierarchy_constraints[] project JSON
  -> exact endpoint path and target-order roundtrip
  -> backward-compatible absent-field loading
  -> fail-closed occurrence-path and reached-component structure validation
```

Do not add incidence graphs, numeric variables, solving, snapshots, diagnostics, or application in the same block.

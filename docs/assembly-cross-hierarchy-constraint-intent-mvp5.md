# Cross-Hierarchy Geometric Constraint Intent MVP-5

Status: implemented as Block 23. Blocks 24-26 are implemented follow-ups; Block 27 is next.

This document is canonical for the Core-owned persistent identity and Project-level model intent of geometric relationships whose endpoints may belong to different rooted assembly occurrences.

## Persistent endpoint identity

`AssemblyHierarchyConstraintEndpoint` stores:

```text
occurrence_path
local ComponentInstanceId
semantic_reference
```

Exact identity is:

```text
(occurrence_path,
 local ComponentInstanceId,
 semantic_reference)
```

The empty `occurrence_path` addresses the explicit root assembly occurrence.

A non-empty path is an exact root-to-current ordered sequence of `SubassemblyInstanceId` values. Path order is identity and is not sorted or normalized.

Endpoint construction rejects:

- an empty `SubassemblyInstanceId` inside the path;
- an empty local `ComponentInstanceId`;
- an empty semantic reference.

Construction stores identity only. It does not walk the hierarchy or resolve geometry.

## Persistent Project-level relationship intent

`AssemblyHierarchyConstraint` stores:

```text
AssemblyConstraintId
name
AssemblyConstraintType
target A: AssemblyHierarchyConstraintEndpoint
target B: AssemblyHierarchyConstraintEndpoint
AssemblyConstraintState
optional Distance quantity
optional Angle quantity
```

Supported relationship families are:

```text
Mate
Concentric
Distance
Insert
Angle
```

The established value-family rules are reused from local `AssemblyConstraint` intent:

```text
Mate         -> no Distance, no Angle
Concentric   -> no Distance, no Angle
Insert       -> no Distance, no Angle
Distance     -> LengthMm required, no Angle
Angle        -> AngleDeg required, no Distance
```

Target A/B order is persistent and must not be normalized.

## Ownership and id scope

`Project` owns:

```text
std::vector<AssemblyHierarchyConstraint> cross_hierarchy_constraints
```

Public collection APIs are:

```text
add_cross_hierarchy_constraint
cross_hierarchy_constraints
cross_hierarchy_constraint_count
find_cross_hierarchy_constraint
```

Cross-hierarchy constraint ids are unique within the Project-level cross-hierarchy collection.

Local `AssemblyDocument::constraints()` retain their document-scoped id space. Therefore a local constraint and one Project-level cross-hierarchy constraint may use the same textual `AssemblyConstraintId` without aliasing.

## Intent construction does not resolve structure

`add_cross_hierarchy_constraint` validates Project-level id uniqueness only.

It does not:

- walk the rooted hierarchy;
- require endpoint paths to resolve immediately;
- require addressed local components immediately;
- resolve referenced PartDocument geometry;
- inspect semantic target families;
- call OCCT.

Complete endpoint structure validation belongs to the Project structure boundary implemented by Block 24.

## Core-to-Geometry bridge

`AssemblyHierarchyConstraintQuery::create` accepts either Core endpoint values or a complete persistent `AssemblyHierarchyConstraint`.

The Geometry query layer converts Core endpoint identity into the derived `AssemblyHierarchyConstraintTarget` seed and reuses the existing root-space target/equation semantics.

The Geometry query type is not persistent authority.

## Transform immutability

Creating or adding Project-level cross-hierarchy intent never changes:

```text
ComponentInstance::transform()
SubassemblyInstance::transform()
grounding state
suppression state
visibility
joint coordinates
```

Relationship creation is not solving.

## Focused coverage

Test file:

```text
tests/core/assembly_cross_hierarchy_constraint_tests.cpp
```

Focused tag:

```text
[core][assembly-cross-hierarchy-intent]
```

The suite proves root empty-path identity, exact nested path ordering, invalid identity rejection, all five value-family rules, inactive state preservation, Distance/Angle quantity preservation, exact target A/B preservation, Project collection access, Project-level id uniqueness, separate local/Project id scopes, intent construction before structure or geometry resolution, and transform immutability.

Focused command:

```bash
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-intent]"
```

## Implemented follow-ups

Block 24 is canonical in `docs/assembly-cross-hierarchy-constraint-json-mvp5.md`:

```text
cross_hierarchy_constraints[] Project JSON
exact endpoint and target-order roundtrip
backward-compatible absent-field loading
exact occurrence-path structure validation
reached local component validation
```

Block 25 is canonical in `docs/assembly-cross-hierarchy-incidence-groups-mvp5.md`:

```text
active relationship participation
ComponentTransformAuthority identity
relationship-to-authority incidence
endpoint-to-authority mappings
deterministic connected cross-hierarchy solve groups
```

Block 26 is canonical in `docs/assembly-cross-hierarchy-numeric-solver-mvp5.md`:

```text
unique free authority variable blocks
mixed document-local/root-space residual evaluation
shared five-family residual flattening
shared central finite-difference Jacobian
existing Gauss-Newton engine
complete authority snapshots
unapplied authority-scoped transform proposals
```

All Block-24/25/26 execution products remain derived except the original Project-owned relationship intent and direct component transforms already owned by the model.

## Explicitly deferred from this intent layer

This Core intent contract does not itself implement:

- result freshness validation;
- atomic cross-hierarchy solve-result application;
- cross-hierarchy rank/remaining-DOF diagnostics;
- semantic target-producing PartDocument revision tracking;
- cross-hierarchy joints and nested motion;
- occurrence-local internal child pose overrides;
- whole-subassembly solve variables.

## Current handoff

The current sequence source of truth is `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

Next is Block 27 only:

```text
Block-26 solve result
  -> authority + relationship + hierarchy-boundary freshness validation
  -> explicit semantic target-geometry freshness contract
  -> atomic authority-qualified direct-transform application
  -> rank / constrained-DOF / remaining-DOF diagnostics
     over the exact Block-26 free-authority variable order
```

Do not add cross-hierarchy joint motion, occurrence-local internal pose overrides, or whole-subassembly transform variables in Block 27.

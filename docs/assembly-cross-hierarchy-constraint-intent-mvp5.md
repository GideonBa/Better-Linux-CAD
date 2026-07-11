# Cross-Hierarchy Geometric Constraint Intent MVP-5

Status: implemented as Block 23 of `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

This document is canonical for the Core-owned occurrence-qualified endpoint value contract and persistent Project-owned cross-hierarchy geometric relationship intent.

## Scope

Block 23 introduces persistent model intent only.

It does not resolve hierarchy paths, semantic feature geometry, graph connectivity, numeric variables, or solve results during record creation or insertion.

## Core-owned endpoint identity

Persistent endpoint identity is:

```text
AssemblyHierarchyConstraintEndpoint =
  (occurrence_path,
   local ComponentInstanceId,
   semantic_reference)
```

The endpoint type lives in Core.

It is the persistence authority used by `AssemblyHierarchyConstraint` and Project JSON.

The Geometry-layer `AssemblyHierarchyConstraintTarget` remains a derived query seed and is not save-format authority.

## Root and child paths

The explicit root assembly occurrence uses:

```text
occurrence_path = []
```

A child or nested occurrence uses the exact root-to-current authored `SubassemblyInstanceId` sequence:

```text
[subassembly.outer, subassembly.inner]
```

Path order is preserved exactly.

Endpoint construction validates identity shape only:

- every path id is non-empty;
- `ComponentInstanceId` is non-empty;
- `semantic_reference` is non-empty.

Block 23 does not ask whether a path or addressed component exists in the current Project.

That structure boundary is implemented by Block 24.

## Project-level relationship record

Persistent cross-hierarchy geometric relationship intent is:

```text
AssemblyHierarchyConstraint
```

The record stores:

```text
AssemblyConstraintId id
name
AssemblyConstraintType type
target A: AssemblyHierarchyConstraintEndpoint
target B: AssemblyHierarchyConstraintEndpoint
AssemblyConstraintState state
optional Distance quantity
optional Angle quantity
```

Target A/B order is persistent intent.

Supported families reuse the existing local relationship enum:

```text
Mate
Concentric
Distance
Insert
Angle
```

Supported states reuse:

```text
Active
Inactive
```

## Shared value-family rules

`AssemblyHierarchyConstraint::create` delegates the established local `AssemblyConstraint` value-family contract.

```text
Mate
  no distance
  no angle

Concentric
  no distance
  no angle

Insert
  no distance
  no angle

Distance
  requires LengthMm
  no angle

Angle
  requires AngleDeg
  no distance
```

The cross-hierarchy model does not define a second interpretation of Distance or Angle values.

## Project ownership and id scope

`Project` owns:

```text
cross_hierarchy_constraints_
```

Public APIs include:

```text
add_cross_hierarchy_constraint
cross_hierarchy_constraints
cross_hierarchy_constraint_count
find_cross_hierarchy_constraint
```

Cross-hierarchy ids are unique inside the Project-level cross-hierarchy collection.

Local `AssemblyConstraintId` values remain scoped by their containing `AssemblyDocument`.

Therefore this is legal:

```text
assembly.root / constraint.shared
project cross-hierarchy collection / constraint.shared
```

but two project-level cross-hierarchy records with `constraint.shared` are invalid.

## Intent construction does not resolve structure

A Project may temporarily contain intent such as:

```text
([subassembly.outer, subassembly.inner], component.shaft, feature.bore.axis)
```

before a caller requests complete structure validation.

`add_cross_hierarchy_constraint` checks project-level id uniqueness only.

It does not:

- walk the rooted hierarchy;
- require reached components;
- resolve PartDocument ids;
- inspect semantic target families;
- call OCCT.

Complete Project structure validation is a separate boundary.

## Core-to-Geometry query bridge

`AssemblyHierarchyConstraintQuery::create` accepts either Core endpoint values or a complete `AssemblyHierarchyConstraint`.

The bridge converts Core endpoint identity into the derived Geometry query target and reuses the implemented root-space target/residual semantics.

This does not make the Geometry target persistent.

## Transform immutability

Creating or adding cross-hierarchy relationship intent never changes:

```text
ComponentInstance::transform()
SubassemblyInstance::transform()
```

Relationship creation is not solving.

Grounding, suppression, visibility, and joint coordinates are also unchanged.

## Focused coverage

Test file:

```text
tests/core/assembly_cross_hierarchy_constraint_tests.cpp
```

Focused tag:

```text
[core][assembly-cross-hierarchy-intent]
```

The suite proves:

- root empty-path endpoint identity;
- exact nested path order;
- empty path-element/component/semantic identity rejection;
- shared five-family value rules;
- inactive state preservation;
- Distance and Angle quantity preservation;
- exact target A/B endpoint preservation;
- Project-owned collection access and lookup;
- Project-level id uniqueness;
- separate local and project-level id scopes;
- root-to-child, repeated-left-to-right, and nested-to-root intent construction without structure or geometry execution;
- component and subassembly transform immutability.

Focused command:

```bash
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-intent]"
```

## Implemented follow-ups

Block 24 is implemented in `docs/assembly-cross-hierarchy-constraint-json-mvp5.md`:

```text
cross_hierarchy_constraints[] Project JSON
exact endpoint and target-order roundtrip
backward-compatible absent-field loading
exact occurrence-path structure validation
reached local component validation
```

Block 25 is implemented in `docs/assembly-cross-hierarchy-incidence-groups-mvp5.md`:

```text
active relationship participation
ComponentTransformAuthority identity
relationship-to-authority incidence
endpoint-to-authority mappings
connected cross-hierarchy solve groups
```

The Block-25 graph remains derived and unpersisted.

## Explicitly deferred

- numeric authority-variable ordering and candidate transforms;
- mixed local/root-space residual execution;
- cross-hierarchy finite-difference Jacobians;
- numeric solve results and proposals;
- freshness snapshots and atomic application;
- cross-hierarchy rank/DOF diagnostics;
- cross-hierarchy joints and nested motion.

## Next technical step

Implement Block 26 only:

```text
AssemblyCrossHierarchySolveGroup
  -> unique free active ComponentTransformAuthority variables
  -> mixed local/root-space residual evaluation
  -> shared scaled residual vector
  -> authority-scoped central finite-difference Jacobian
  -> existing numeric solve engine
  -> unapplied transform proposals
```

Do not add result application or cross-hierarchy diagnostics in the same block.

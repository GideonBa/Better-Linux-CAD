# Cross-Hierarchy Geometric Constraint Intent MVP-5

Status: implemented as Block 23. Blocks 24-28 are implemented follow-ups; Block 29 is next.

This document is canonical for the Core-owned persistent endpoint identity and Project-level geometric relationship intent whose endpoints may belong to different rooted assembly occurrences.

## Persistent endpoint identity

`AssemblyHierarchyConstraintEndpoint` stores:

```text
occurrence_path
local ComponentInstanceId
semantic_reference
```

Exact identity:

```text
(occurrence_path,
 local ComponentInstanceId,
 semantic_reference)
```

The empty occurrence path addresses the explicit root assembly occurrence.

A non-empty path is the exact ordered root-to-current sequence of `SubassemblyInstanceId` values. Path order is persistent identity and is never sorted or normalized.

Construction rejects an empty path element, empty component id, or empty semantic reference.

Construction stores identity only. It does not traverse hierarchy, resolve part features, or execute geometry.

Block 28 reuses this exact Core endpoint value for persistent Project-level `AssemblyHierarchyJoint` motion intent. There is no second occurrence-qualified endpoint type.

## Persistent Project-level geometric intent

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

Supported families:

```text
Mate
Concentric
Distance
Insert
Angle
```

Value-family rules reuse local `AssemblyConstraint` semantics:

```text
Mate         -> no Distance, no Angle
Concentric   -> no Distance, no Angle
Insert       -> no Distance, no Angle
Distance     -> LengthMm required, no Angle
Angle        -> AngleDeg required, no Distance
```

Target A/B order is persistent and is not normalized.

## Ownership and id scope

`Project` owns:

```text
std::vector<AssemblyHierarchyConstraint> cross_hierarchy_constraints
```

Public collection APIs include:

```text
add_cross_hierarchy_constraint
cross_hierarchy_constraints
cross_hierarchy_constraint_count
find_cross_hierarchy_constraint
```

Ids are unique inside the Project-level cross-geometric collection.

Local `AssemblyDocument::constraints()` retain document-scoped id spaces. A local constraint and a Project-level cross constraint may use the same textual `AssemblyConstraintId` without aliasing.

Block 28 adds a separate Project-level cross-joint collection with `AssemblyJointId` scope. Geometric and motion records remain separate persistent intent families.

## Intent construction does not resolve structure or geometry

`add_cross_hierarchy_constraint` validates Project-level id uniqueness only.

It does not:

```text
walk the rooted hierarchy
require occurrence paths immediately
require addressed local components immediately
resolve referenced PartDocument geometry
parse semantic face/axis/seat families
call OCCT
mutate component transforms
mutate SubassemblyInstance transforms
```

Complete endpoint path/component structure validation belongs to the Project structure boundary implemented by Block 24.

Semantic target geometry belongs to the Geometry layer.

## Core-to-Geometry bridge

`AssemblyHierarchyConstraintQuery::create` accepts a persistent `AssemblyHierarchyConstraint` and converts its Core endpoints into derived hierarchy target query values.

Geometry then reuses the existing root-space hierarchy target and equation builders.

The Geometry query type is not persistent authority.

## Transform immutability

Creating or adding Project-level cross-hierarchy geometric intent never changes:

```text
ComponentInstance::transform()
SubassemblyInstance::transform()
grounding
suppression
visibility
joint coordinates
```

Relationship creation is not solving.

## Implemented follow-up chain

Block 24:

```text
cross_hierarchy_constraints[] Project JSON
exact endpoint/path/target order roundtrip
exact rooted path and reached-component validation
```

Block 25:

```text
ComponentTransformAuthority
active relationship-to-authority incidence
separate endpoint mappings
deterministic connected geometric solve groups
```

Block 26:

```text
one six-variable block per unique free authority
mixed document-local/root-space residual evaluation
shared finite differences and Gauss-Newton engine
unapplied authority proposals
```

Block 27:

```text
complete authority/relationship/path-boundary freshness
exact semantic PartDocument model-intent freshness
atomic authority application
rank / remaining-DOF diagnostics
```

Block 28:

```text
same occurrence-qualified endpoint value reused by AssemblyHierarchyJoint
Project-level cross_hierarchy_joints[]
combined local/cross geometric/joint motion connectivity
root-space Revolute motion
shared Block-27 freshness and atomic authority application
```

All graph, solve, motion, snapshot, proposal, and diagnostic products remain derived.

## Focused coverage

```bash
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-intent]"
```

The intent suite proves root empty-path identity, exact nested path order, invalid endpoint rejection, all five family/value rules, state/quantity preservation, exact target A/B preservation, Project collection/id scope, construction before structure/geometry resolution, and transform immutability.

Block-28 endpoint reuse is covered by:

```bash
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-joint]"
```

## Persistence boundary

Persistent authority in this contract is only:

```text
AssemblyHierarchyConstraintEndpoint
AssemblyHierarchyConstraint
Project::cross_hierarchy_constraints
```

Resolved occurrence descriptors, reached assembly documents, semantic geometry, root-space targets, transform authorities, incidence, groups, residuals, Jacobians, results, freshness snapshots, proposals, and diagnostics are regenerated.

## Current handoff

Blocks 23-28 are implemented. The sequence source of truth is `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

Next is Block 29 only: define stable derived assembly/component/product exchange identities and emit structured STEP assembly/product relationships while reusing canonical posed-leaf hierarchy transform chains and shared part recompute/cache authority.

Occurrence-local child pose overrides, whole-subassembly solve variables, and swept-motion contact simulation remain deferred.

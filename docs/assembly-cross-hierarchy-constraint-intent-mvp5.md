# Cross-Hierarchy Geometric Constraint Intent MVP-5

Status: implemented as Block 23. Blocks 24-27 are implemented follow-ups; Block 28 is next.

This document is canonical for the Core-owned persistent endpoint identity and Project-level model intent of geometric relationships whose endpoints may belong to different rooted assembly occurrences.

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

The empty occurrence path addresses the explicit root assembly occurrence.

A non-empty path is an exact root-to-current ordered sequence of `SubassemblyInstanceId` values. Path order is identity and is not sorted or normalized.

Endpoint construction rejects an empty path element, empty component id, or empty semantic reference.

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

Supported families are:

```text
Mate
Concentric
Distance
Insert
Angle
```

The established local family/value rules are reused:

```text
Mate         -> no Distance, no Angle
Concentric   -> no Distance, no Angle
Insert       -> no Distance, no Angle
Distance     -> LengthMm required, no Angle
Angle        -> AngleDeg required, no Distance
```

Target A/B order is persistent and is never normalized.

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

Cross-hierarchy ids are unique in the Project-level collection.

Local `AssemblyDocument::constraints()` retain document-scoped id spaces. A local constraint and one Project-level cross-hierarchy constraint may therefore use the same textual `AssemblyConstraintId` without aliasing.

## Intent construction does not resolve structure

`add_cross_hierarchy_constraint` validates Project-level id uniqueness only.

It does not walk the rooted hierarchy, require paths/components immediately, resolve PartDocuments, inspect semantic target families, or call OCCT.

Complete endpoint structure validation belongs to the Project structure boundary implemented by Block 24.

## Core-to-Geometry bridge

`AssemblyHierarchyConstraintQuery::create` accepts Core endpoint values or a complete persistent `AssemblyHierarchyConstraint`.

The Geometry query layer converts Core endpoint identity into the derived hierarchy target/query contract and reuses root-space target/equation semantics.

The Geometry query type is not persistent authority.

## Transform immutability

Creating or adding Project-level cross-hierarchy intent never changes:

```text
ComponentInstance::transform()
SubassemblyInstance::transform()
grounding
suppression
visibility
joint coordinates
```

Relationship creation is not solving.

## Focused coverage

```bash
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-intent]"
```

Coverage proves root empty-path identity, exact nested path ordering, invalid identity rejection, all five family/value rules, state and quantity preservation, exact target order, Project collection access/id scope, construction before structure/geometry resolution, and transform immutability.

## Implemented follow-ups

Block 24 (`docs/assembly-cross-hierarchy-constraint-json-mvp5.md`) adds Project JSON and exact endpoint structure validation.

Block 25 (`docs/assembly-cross-hierarchy-incidence-groups-mvp5.md`) derives active relationship participation, `ComponentTransformAuthority`, relationship-to-authority incidence, endpoint mappings, and deterministic cross-hierarchy solve groups.

Block 26 (`docs/assembly-cross-hierarchy-numeric-solver-mvp5.md`) executes one exact current solve group through authority-scoped variables, mixed document-local/root-space residual evaluation, shared finite differences, and the existing Gauss-Newton engine.

Block 27 (`docs/assembly-cross-hierarchy-application-diagnostics-mvp5.md`) adds complete modeled-input freshness, exact canonical PartDocument model-intent snapshots, atomic authority-qualified application, and cross-hierarchy rank/remaining-DOF diagnostics.

All graph, solve, snapshot, proposal, freshness, and diagnostic products remain derived. Persistent authority remains the original relationship intent and existing component/subassembly/PartDocument model state.

## Explicitly deferred from this intent layer

This Core geometric relationship intent does not itself implement:

- Project-level cross-hierarchy joint intent;
- nested motion propagation;
- occurrence-local child pose overrides;
- whole-subassembly solve variables;
- richer joint families or motion studies.

## Current handoff

The sequence source of truth is `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

Next is Block 28 only: persistent occurrence-qualified Project-level cross-hierarchy Revolute joint intent, additive JSON, joint-to-`ComponentTransformAuthority` incidence, combined geometric/joint motion connectivity, root-space nested Revolute drive evaluation, shared authority-scoped numeric solving, and Block-27-style fresh atomic transform-plus-coordinate application.

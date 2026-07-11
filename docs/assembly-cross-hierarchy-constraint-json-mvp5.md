# Cross-Hierarchy Constraint Project JSON and Structure Validation MVP-5

Status: implemented as Block 24 of `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

Exact Project JSON spelling is canonical in `docs/file-format.md`.

This document records the persistence and Core structure-validation boundary for project-level `AssemblyHierarchyConstraint` intent.

## Scope

Block 24 makes the Block-23 Core model intent roundtrip through Project JSON and validates exact rooted endpoint structure.

It does not resolve semantic feature geometry, derive solve connectivity, or run numeric solving.

## Project-level additive field

Project JSON stores:

```text
cross_hierarchy_constraints[]
```

The field is additive under the existing project schema/version marker.

Files without the field load with:

```text
Project::cross_hierarchy_constraints().empty() == true
```

No schema marker bump is required for this additive optional collection.

## Endpoint JSON

Representative endpoint:

```json
{
  "occurrence_path": [
    "subassembly.outer",
    "subassembly.inner"
  ],
  "component_instance": "component.shaft",
  "semantic_reference": "feature.bore.axis"
}
```

The explicit root occurrence uses:

```json
{
  "occurrence_path": [],
  "component_instance": "component.root",
  "semantic_reference": "feature.base.top"
}
```

`occurrence_path` element order is persistent endpoint identity.

The serializer does not sort or normalize path ids.

## Relationship JSON

Representative Concentric relationship:

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

Supported type spellings are:

```text
mate
concentric
distance
insert
angle
```

Supported state spellings are:

```text
active
inactive
```

Target A/B order is preserved exactly.

## Distance and Angle quantities

Distance uses:

```json
"distance": {
  "unit": "mm",
  "value": 12.5
}
```

Angle uses:

```json
"angle": {
  "unit": "deg",
  "value": 35.0
}
```

Unsupported unit spellings fail closed.

The existing `AssemblyHierarchyConstraint::create` value-family contract remains authoritative:

```text
Distance requires LengthMm and excludes Angle
Angle requires AngleDeg and excludes Distance
Mate/Concentric/Insert carry neither value
```

## Load order

Project deserialization performs:

```text
root AssemblyDocument
  -> child AssemblyDocument records
  -> PartDocument records
  -> cross_hierarchy_constraints[] records
  -> Project::validate_assembly_structure()
```

Project-level duplicate cross-hierarchy constraint ids are rejected during additive insertion.

Complete structure validation also rejects duplicate ids if a caller has corrupted the mutable collection directly.

## Exact Core endpoint path validation

`Project::validate_cross_hierarchy_constraints()` follows each endpoint from the explicit root.

For:

```text
occurrence_path = [outer, inner]
```

the validator performs:

```text
root assembly
  -> find SubassemblyInstance outer in root
  -> resolve outer.referenced_assembly_document
  -> find SubassemblyInstance inner in reached child document
  -> resolve inner.referenced_assembly_document
  -> require endpoint.component_instance in final reached assembly document
```

The validator never performs a global occurrence-id or component-id search.

The exact ordered authored path must resolve.

## Validation order

`Project::validate_assembly_structure()` performs:

```text
member parts
  -> component instances
  -> local assembly constraints
  -> local assembly joints
  -> complete assembly hierarchy
  -> cross-hierarchy constraint endpoint structure
```

The hierarchy is therefore known to have valid child references and no direct or indirect cycles before endpoint paths are followed.

This ordering is intentional. An invalid hierarchy fails before an endpoint error that depends on that hierarchy.

## No Geometry dependency

Structure validation follows authored Core `SubassemblyInstance` references directly.

It does not call:

```text
AssemblyHierarchyTraversal::build
AssemblyConstraintTargetResolver
AssemblyHierarchyConstraintTargetResolver
OCCT
```

`AssemblyHierarchyTraversal::build` itself requires complete `Project::validate_assembly_structure()` success. Calling it from endpoint structure validation would recursively re-enter the full validation path.

## Semantic geometry remains unresolved

The JSON/Core structure layer treats semantic-reference text as persistent opaque intent.

For example:

```text
semantic.no_geometry.distance.a
```

may roundtrip when path and component structure are valid.

A later Geometry target resolver may reject the token as unsupported.

Block 24 does not inspect PartDocument feature history or target family compatibility.

## Transform immutability

Loading or serializing project-level cross-hierarchy constraints does not change:

```text
ComponentInstance::transform()
SubassemblyInstance::transform()
```

It also does not solve, move, ground, suppress, hide, or alter joint coordinates.

## Focused coverage

Test file:

```text
tests/core/assembly_cross_hierarchy_constraint_tests.cpp
```

Focused tag:

```text
[core][assembly-cross-hierarchy-json]
```

The suite proves:

- full five-family Project JSON roundtrip;
- id/name/type/state preservation;
- exact target A/B preservation;
- exact nested occurrence-path element order;
- root empty-path roundtrip;
- Distance millimeter roundtrip;
- Angle degree roundtrip;
- absent-field backward compatibility;
- duplicate id rejection during loading;
- duplicate id rejection after direct mutable collection corruption;
- missing exact path rejection;
- missing reached-component rejection;
- hierarchy validation preceding endpoint validation;
- arbitrary semantic-reference text remaining unresolved during loading;
- component and subassembly transform immutability.

Focused command:

```bash
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-json]"
```

## Implemented follow-up

Block 25 is implemented in `docs/assembly-cross-hierarchy-incidence-groups-mvp5.md`.

It derives:

```text
ComponentTransformAuthority identity
active local/project-level relationship participation
unique relationship-to-authority incidence
TargetA/TargetB endpoint-to-authority mappings
deterministic connected cross-hierarchy solve groups
```

The derived graph is unpersisted.

## Explicitly deferred

The persistence/structure layer does not implement:

- numeric transform-authority variables;
- mixed local/root-space residual evaluation;
- authority-scoped finite-difference Jacobians;
- Gauss-Newton solve execution for cross-hierarchy groups;
- solve-result snapshots or proposals;
- result application;
- cross-hierarchy rank/remaining-DOF diagnostics;
- cross-hierarchy joints.

## Next technical step

Implement Block 26 from `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`:

```text
AssemblyCrossHierarchySolveGroup
  -> unique free active authority variables
  -> local relationship residuals in document-local space
  -> cross-hierarchy relationship residuals in root space
  -> shared scaled residual/Jacobian path
  -> existing numeric solve engine
  -> unapplied authority-scoped proposals
```

Do not apply solve results or add cross-hierarchy diagnostics in Block 26.

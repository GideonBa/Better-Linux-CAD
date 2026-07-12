# Cross-Hierarchy Geometric Constraint Project JSON MVP-5

Status: implemented as Block 24. Blocks 25 and 26 are implemented follow-ups; Block 27 is next.

This document is canonical for the Project JSON spelling and Core structure-validation boundary of persistent `AssemblyHierarchyConstraint` records.

## Additive Project field

Project JSON stores:

```text
cross_hierarchy_constraints[]
```

The field is additive. A Project file without the field loads with an empty cross-hierarchy constraint collection.

Serialization emits the collection without changing local `AssemblyDocument::constraints()` JSON.

## Relationship JSON

One Project-level cross-hierarchy relationship stores:

```json
{
  "id": "constraint.cross.shaft",
  "name": "Shaft alignment",
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

Supported `type` spellings are:

```text
mate
concentric
distance
insert
angle
```

Supported `state` spellings are:

```text
active
inactive
```

Target A/B order is preserved exactly.

## Endpoint JSON

An endpoint stores:

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

The empty path addresses the explicit root assembly occurrence:

```json
{
  "occurrence_path": [],
  "component_instance": "component.root",
  "semantic_reference": "feature.base.top"
}
```

`occurrence_path` element order is persistent endpoint identity and is never sorted or normalized.

## Distance and Angle quantities

Distance uses:

```json
{
  "unit": "mm",
  "value": 12.5
}
```

Angle uses:

```json
{
  "unit": "deg",
  "value": 35.0
}
```

Unsupported unit spellings fail closed.

The established family/value contract remains authoritative:

```text
Distance -> one LengthMm quantity
Angle    -> one AngleDeg quantity
Mate / Concentric / Insert -> no explicit value
```

## Loading and collection uniqueness

During loading:

1. relationship type and state are parsed fail-closed;
2. target A and target B endpoints are reconstructed in stored order;
3. Distance/Angle quantities are reconstructed with existing `Quantity` validation;
4. `AssemblyHierarchyConstraint::create` applies the shared family/value contract;
5. `Project::add_cross_hierarchy_constraint` enforces Project-level cross-hierarchy id uniqueness;
6. complete Project structure validation runs after all records are loaded.

Duplicate Project-level cross-hierarchy ids are rejected.

Local document-scoped constraint ids remain independent from the Project-level cross-hierarchy collection.

## Core structure-validation order

`Project::validate_assembly_structure()` validates:

```text
member parts
  -> component instances
  -> local assembly constraints
  -> local assembly joints
  -> complete assembly hierarchy
  -> cross-hierarchy endpoint structure
```

The complete hierarchy validates before endpoint path resolution.

This prevents endpoint resolution from treating an invalid or cyclic assembly graph as a rooted path authority.

## Exact endpoint path validation

After hierarchy validation, every endpoint path is followed directly from the explicit root through each authored `SubassemblyInstanceId` in stored order.

For:

```text
occurrence_path = [outer, inner]
```

validation requires:

```text
root contains outer
  -> outer references an owned child AssemblyDocument
  -> reached child contains inner
  -> inner references an owned child AssemblyDocument
  -> final reached document contains the endpoint ComponentInstanceId
```

No global occurrence-id or component-id search is used.

The addressed local component must exist in the exact assembly document reached by the path.

## No semantic geometry execution during load

Block-24 structure validation does not:

- parse semantic face/axis/seat families;
- inspect target-producing features;
- execute OCCT;
- build root-space geometry;
- evaluate residuals.

Arbitrary semantic-reference text can therefore roundtrip when endpoint identity and Project structure are valid.

A later Geometry target resolver may reject unsupported semantic geometry.

## Transform immutability

Project serialization/deserialization and cross-hierarchy structure validation do not change:

```text
ComponentInstance::transform()
SubassemblyInstance::transform()
```

Stored direct component and rigid boundary transforms roundtrip as existing model intent.

## Failure policy

Loading/structure validation fails closed on:

- malformed JSON members;
- unsupported relationship type/state spellings;
- unsupported Distance/Angle unit spellings;
- invalid endpoint identity;
- invalid relationship family/value combinations;
- duplicate Project-level cross-hierarchy ids;
- invalid ordinary member/component/local relationship/joint structure;
- missing child assembly references or hierarchy cycles;
- endpoint occurrence paths absent from the exact rooted authored hierarchy;
- addressed local components absent from the exact reached assembly document.

Semantic feature existence and target-family geometry compatibility are not structure checks.

## Focused coverage

Test file:

```text
tests/core/assembly_cross_hierarchy_constraint_tests.cpp
```

Focused tag:

```text
[core][assembly-cross-hierarchy-json]
```

The suite proves all five relationship families roundtrip, id/name/type/state preservation, exact target A/B and nested path order, root empty-path roundtrip, `mm` and `deg` quantities, absent-field backward compatibility, duplicate-id rejection, missing-path and reached-component rejection, hierarchy-error precedence, unresolved semantic text during load, and transform immutability.

Focused command:

```bash
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-json]"
```

## Implemented follow-ups

Block 25 is canonical in `docs/assembly-cross-hierarchy-incidence-groups-mvp5.md` and derives active relationship participation, transform-authority mapping, unique incidence, endpoint mappings, and deterministic cross-hierarchy solve groups.

Block 26 is canonical in `docs/assembly-cross-hierarchy-numeric-solver-mvp5.md` and executes one exact current solve group through authority-scoped variables, mixed local/root-space residual evaluation, shared finite differences, and the existing numeric solve engine. Results remain unapplied.

Neither follow-up changes the Project JSON spelling defined here.

## Explicitly deferred from the persistence/structure layer

This JSON/structure contract does not implement:

- result freshness validation;
- atomic cross-hierarchy solve-result application;
- cross-hierarchy rank/remaining-DOF diagnostics;
- semantic target-producing PartDocument revision tracking;
- cross-hierarchy joints or nested motion;
- occurrence-local internal pose overrides;
- whole-subassembly solve variables.

## Current handoff

The sequence source of truth is `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

Next is Block 27 only: complete Block-26 result freshness validation, an explicit semantic target-geometry freshness contract, atomic authority-qualified direct-transform application, and rank/remaining-DOF diagnostics over the exact Block-26 free-authority variable order.

No new Project JSON field is planned for derived solve results, residuals, Jacobians, snapshots, proposals, or diagnostics.

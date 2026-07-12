# Cross-Hierarchy Geometric Constraint Project JSON MVP-5

Status: implemented as Block 24. Blocks 25-27 are implemented follow-ups; Block 28 is next.

This document is canonical for the Project JSON spelling and Core structure-validation boundary of persistent `AssemblyHierarchyConstraint` records.

## Additive Project field

Project JSON stores:

```text
cross_hierarchy_constraints[]
```

The field is additive. A Project file without it loads with an empty cross-hierarchy constraint collection.

Serialization emits the collection without changing local `AssemblyDocument::constraints()` JSON.

## Relationship JSON

Representative record:

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

Supported type spellings:

```text
mate
concentric
distance
insert
angle
```

Supported states:

```text
active
inactive
```

Target A/B order is preserved exactly.

## Endpoint JSON

An endpoint stores:

```json
{
  "occurrence_path": ["subassembly.outer", "subassembly.inner"],
  "component_instance": "component.shaft",
  "semantic_reference": "feature.bore.axis"
}
```

The empty path addresses the explicit root assembly occurrence.

Occurrence-path element order is persistent endpoint identity and is never sorted or normalized.

## Distance and Angle quantities

Distance uses `mm`; Angle uses `deg`.

The family/value contract remains:

```text
Distance -> one LengthMm quantity
Angle    -> one AngleDeg quantity
Mate / Concentric / Insert -> no explicit value
```

Unsupported unit spellings fail closed.

## Loading and uniqueness

During loading:

1. type/state are parsed fail-closed;
2. target A/B endpoints are reconstructed in stored order;
3. Distance/Angle quantities use existing `Quantity` validation;
4. `AssemblyHierarchyConstraint::create` applies family/value rules;
5. `Project::add_cross_hierarchy_constraint` enforces Project-level id uniqueness;
6. complete Project structure validation runs after all records load.

Local constraint ids remain independent from the Project-level cross-hierarchy collection.

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

## Exact endpoint path validation

Every endpoint path is followed directly from the explicit root through each authored `SubassemblyInstanceId` in stored order.

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

No global occurrence-id/component-id search is used.

## No semantic geometry execution during load

Structure validation does not parse semantic face/axis/seat families, inspect target-producing features, execute OCCT, build root-space target geometry, or evaluate residuals.

Arbitrary semantic-reference text can roundtrip when endpoint identity and Project structure are valid. A later Geometry target resolver may reject unsupported semantic geometry.

## Transform immutability

Project serialization/deserialization and endpoint structure validation do not change component or subassembly transforms.

## Failure policy

Loading/structure validation fails closed on malformed JSON, unsupported type/state/unit spellings, invalid endpoint identity, invalid family/value combinations, duplicate Project-level cross ids, invalid ordinary structure, missing child references/cycles, missing exact endpoint paths, and missing reached components.

Semantic feature existence and target-family compatibility are not Core structure checks.

## Focused coverage

```bash
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-json]"
```

Coverage proves five-family roundtrip, identity/state/name preservation, exact target/path order, root path, `mm`/`deg` quantities, absent-field compatibility, duplicate rejection, missing path/reached component rejection, hierarchy-error precedence, unresolved semantic text during load, and transform immutability.

## Implemented follow-ups

Block 25 derives transform-authority incidence and deterministic solve groups.

Block 26 performs authority-scoped mixed local/root-space numeric solving and returns derived proposals.

Block 27 provides complete modeled-input freshness, exact canonical PartDocument model-intent snapshots, atomic authority application, and cross-hierarchy diagnostics.

None of these follow-ups changes `cross_hierarchy_constraints[]`.

Block-27 solve/freshness products are not serialized.

## Explicitly deferred from this JSON layer

- Project-level cross-hierarchy joint JSON;
- nested motion propagation;
- occurrence-local pose overrides;
- whole-subassembly variables.

## Current handoff

Next is Block 28 from `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

Block 28 may add a separate additive Project-level cross-hierarchy joint collection for persistent Revolute intent. It must not serialize derived motion connectivity, residuals, Jacobians, freshness snapshots, proposals, or diagnostics.

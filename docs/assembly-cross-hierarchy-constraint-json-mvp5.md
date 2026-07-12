# Cross-Hierarchy Geometric Constraint Project JSON MVP-5

Status: implemented as Block 24. Blocks 25-28 are implemented follow-ups; Block 29 is next.

This document is canonical for Project JSON spelling and Core structure validation of persistent `AssemblyHierarchyConstraint` records.

## Additive Project field

Project JSON stores:

```text
cross_hierarchy_constraints[]
```

The field is additive. Files without it load with an empty Project-level cross-geometric collection.

Block 28 separately adds:

```text
cross_hierarchy_joints[]
```

The two collections are independent persistent geometry and motion intent.

## Geometric relationship JSON

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

Supported state spellings:

```text
active
inactive
```

Target A/B order is preserved exactly.

## Occurrence-qualified endpoint JSON

```json
{
  "occurrence_path": ["subassembly.outer", "subassembly.inner"],
  "component_instance": "component.shaft",
  "semantic_reference": "feature.bore.axis"
}
```

Root endpoint:

```json
{
  "occurrence_path": [],
  "component_instance": "component.root",
  "semantic_reference": "feature.base.top"
}
```

Occurrence-path element order is persistent identity and is never sorted or normalized.

Block 28 reuses exactly this endpoint JSON shape for Project-level cross-hierarchy joints.

## Distance and Angle quantities

Distance:

```json
{"unit": "mm", "value": 12.5}
```

Angle:

```json
{"unit": "deg", "value": 35.0}
```

Value-family rules:

```text
Distance -> one LengthMm quantity
Angle    -> one AngleDeg quantity
Mate / Concentric / Insert -> no explicit quantity
```

Unsupported unit spellings fail closed.

## Loading and id uniqueness

During loading:

1. type/state parse fail-closed;
2. target A/B endpoints reconstruct in stored order;
3. Distance/Angle quantities use existing `Quantity` validation;
4. `AssemblyHierarchyConstraint::create` applies family/value rules;
5. `Project::add_cross_hierarchy_constraint` enforces Project-level geometric id uniqueness;
6. complete Project structure validation runs after all Project records load.

Local document-scoped constraint ids remain independent from the Project-level collection.

## Structure-validation order

The Project structure boundary now validates:

```text
member parts
  -> component instances
  -> local geometric constraints
  -> local joints
  -> complete cycle-free hierarchy
  -> Project-level cross geometric endpoints
  -> Project-level cross joint endpoints
```

Block-24 geometric endpoint structure validation occurs only after hierarchy validation.

For one stored path:

```text
[outer, inner]
```

validation requires:

```text
root contains outer
  -> outer references owned child assembly
  -> reached child contains inner
  -> inner references owned child assembly
  -> final reached document contains local endpoint component
```

No global occurrence-id or component-id search is used.

## No semantic geometry execution during load

Core loading/structure validation does not:

```text
parse face/axis/seat target families
inspect target-producing features
execute OCCT
build root-space geometry
evaluate residuals
```

Arbitrary semantic-reference text can roundtrip when endpoint identity and Project structure are valid. A Geometry resolver may reject unsupported target meaning later.

## Transform immutability

Project serialization/deserialization and cross-hierarchy structure validation do not change:

```text
ComponentInstance::transform()
SubassemblyInstance::transform()
```

Stored direct component and rigid boundary transforms roundtrip as existing model intent.

## Failure policy

Loading or structure validation fails closed on malformed JSON, unsupported type/state/unit spellings, invalid endpoint identity, invalid family/value combinations, duplicate Project-level cross-geometric ids, invalid ordinary Project structure, invalid hierarchy/cycles, missing exact occurrence paths, or missing local components in reached assembly documents.

Semantic feature existence and target-family geometry compatibility are not Core structure checks.

## Implemented follow-ups

Block 25 derives active transform-authority incidence and deterministic geometric solve groups.

Block 26 performs authority-scoped mixed local/root-space numeric solving through the shared numeric engine.

Block 27 adds complete modeled-input freshness, exact canonical PartDocument model-intent snapshots, atomic authority application, and diagnostics.

Block 28 adds a separate additive `cross_hierarchy_joints[]` collection using the same endpoint JSON identity, plus combined cross-document Revolute motion. It does not change the geometric record spelling defined here.

No solve/motion result, residual, Jacobian, graph, freshness snapshot, proposal, or diagnostic is serialized.

## Focused coverage

```bash
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-json]"
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-joint-json]"
```

The first tag proves geometric five-family roundtrip, exact target/path order, root path, `mm`/`deg` quantities, absent-field compatibility, duplicate rejection, exact path/reached-component validation, unresolved semantic text, and transform immutability.

The second proves Block-28 additive cross-joint JSON with the same endpoint identity contract.

## Persistence boundary

Persistent model authority here is:

```text
cross_hierarchy_constraints[]
AssemblyHierarchyConstraint records
exact occurrence-qualified endpoints
```

Block 28 separately persists `cross_hierarchy_joints[]`.

Graph connectivity, authority mapping, root-space geometry, residuals, numeric results, snapshots, proposals, diagnostics, and future structured STEP product identities remain derived.

## Current handoff

Blocks 24-28 are implemented. Next is Block 29 from `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`: derived structured assembly exchange identities and STEP product/occurrence relationships.

No new Project JSON field is planned for Block-29 exchange products; exchange identity must remain derived from persistent Project hierarchy and component/part intent.

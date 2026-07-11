# Cross-Hierarchy Constraint JSON and Structure Validation MVP-5

Status: implemented Block 24 of the persistent cross-hierarchy geometric constraint sequence.

This document is canonical for project JSON spelling and Core-only structural validation of `AssemblyHierarchyConstraint` records. `docs/file-format.md` remains the repository-wide save-format reference. The follow-up solve-connectivity sequence is canonical in `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

## Implemented scope

```text
Project-owned AssemblyHierarchyConstraint intent
  -> additive cross_hierarchy_constraints[] project JSON
  -> exact endpoint path/target-order roundtrip
  -> exact Mate/Concentric/Distance/Insert/Angle value-family roundtrip
  -> backward-compatible absent-field loading
  -> complete ordinary project/hierarchy validation
  -> exact authored root-to-endpoint path traversal
  -> reached AssemblyDocument
  -> addressed local ComponentInstance existence validation
```

This block adds no incidence graph, numeric variable, residual evaluation, solve result, stale snapshot, diagnostics, or application path.

## Project JSON field

The additive project-level field is:

```text
cross_hierarchy_constraints[]
```

The historical project schema marker remains:

```text
blcad.project.mvp4
version 1
```

The field is additive. Files that omit it load with an empty Project cross-hierarchy constraint collection.

Serialization emits the field as an array, including an empty array when the project currently owns no cross-hierarchy relationship intent.

## Endpoint JSON

`AssemblyHierarchyConstraintEndpoint` is serialized exactly as:

```json
{
  "occurrence_path": ["subassembly.outer", "subassembly.inner"],
  "component_instance": "component.shaft",
  "semantic_reference": "feature.bore.axis"
}
```

The explicit root occurrence is represented by:

```json
{
  "occurrence_path": [],
  "component_instance": "component.root",
  "semantic_reference": "feature.base.top"
}
```

Persistence rules:

1. `occurrence_path` is always an ordered JSON array;
2. path elements are exact `SubassemblyInstanceId` values;
3. the empty path addresses the explicit root assembly occurrence;
4. path element order is identity and is preserved verbatim;
5. `component_instance` is the local component id in the assembly document reached by the path;
6. `semantic_reference` is stored as opaque model-intent text;
7. no OCCT topology id or resolved geometric descriptor is serialized.

For example:

```text
[subassembly.outer, subassembly.inner]
```

is not equivalent to:

```text
[subassembly.inner, subassembly.outer]
```

and the loader never sorts or normalizes endpoint paths.

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

The exact supported type spellings are:

```text
mate
concentric
distance
insert
angle
```

The exact state spellings are:

```text
active
inactive
```

Target A and target B order is persistent relationship intent and is preserved exactly.

## Distance and Angle quantities

Distance uses the established millimeter spelling:

```json
"distance": {
  "unit": "mm",
  "value": 12.5
}
```

Angle uses the established degree spelling:

```json
"angle": {
  "unit": "deg",
  "value": 35.0
}
```

The loader rejects other unit strings fail-closed.

The existing `AssemblyHierarchyConstraint::create` value-family contract remains authoritative after JSON parsing:

```text
Mate        -> no distance, no angle
Concentric  -> no distance, no angle
Insert      -> no distance, no angle
Distance    -> LengthMm distance required, no angle
Angle       -> AngleDeg angle required, no distance
```

The JSON layer reconstructs typed `Quantity` values and delegates final family validation to the Core relationship record.

## Load order

Project loading performs these model-construction steps:

```text
root AssemblyDocument
  -> child AssemblyDocument records
  -> owned PartDocument records
  -> cross_hierarchy_constraints[] Core records
  -> Project::validate_assembly_structure()
```

Adding a parsed cross-hierarchy record still performs no path or geometry resolution. Structural validation occurs after the complete project model has been reconstructed.

Duplicate project-level cross-hierarchy relationship ids fail during additive collection insertion. `validate_cross_hierarchy_constraints()` also rejects duplicate ids so direct mutable collection access cannot silently bypass the uniqueness invariant.

## Project structure validation order

`Project::validate_assembly_structure()` now validates in this order:

```text
member parts
  -> component instances
  -> local assembly constraints
  -> local assembly joints
  -> complete assembly hierarchy
  -> cross-hierarchy constraint endpoint structure
```

This ordering is required.

Example: if a `SubassemblyInstance` references a missing child document and a cross-hierarchy endpoint also contains a missing path, the hierarchy-reference error is returned first. BLCAD does not attempt endpoint-path resolution against an invalid authored hierarchy.

## Exact Core path resolution

After normal hierarchy validation succeeds, each endpoint is resolved structurally from the explicit root assembly.

Algorithm:

```text
current = Project::assembly()

for occurrence_id in endpoint.occurrence_path:
    instance = current.find_subassembly_instance(occurrence_id)
    require instance exists

    current = Project::find_assembly_document(
        instance.referenced_assembly_document)
    require current exists

require current.find_component_instance(endpoint.component_instance) exists
```

This is exact authored path traversal. It does not search globally by the final `SubassemblyInstanceId`, by child `DocumentId`, or by local `ComponentInstanceId` alone.

For:

```text
root
  subassembly.left  -> assembly.gearbox
  subassembly.right -> assembly.gearbox
```

these remain distinct structural paths:

```text
[subassembly.left]
[subassembly.right]
```

even though both reach the same project-owned child `AssemblyDocument` model definition.

## Why validation does not call the Geometry resolver

The structural validator must answer only:

```text
does this exact occurrence path exist?
and
does the reached AssemblyDocument contain this local ComponentInstanceId?
```

It therefore does not call:

```text
AssemblyHierarchyConstraintTargetResolver
AssemblyConstraintTargetResolver
AssemblyHierarchyConstraintEquationBuilder
OCCT
```

A semantic string such as:

```text
semantic.no_geometry.distance.a
```

can roundtrip successfully when path and component structure are valid. A later Geometry consumer may reject it when actual target geometry is requested.

## Why validation follows authored links directly

`AssemblyHierarchyTraversal::build` currently validates the complete project structure before deriving occurrence descriptors.

Cross-hierarchy structure validation is itself part of `Project::validate_assembly_structure()` after hierarchy validation. Calling `AssemblyHierarchyTraversal::build` from the cross-hierarchy validator would therefore recursively re-enter complete structure validation.

Block 24 instead follows the already validated authored `SubassemblyInstance` chain directly. This preserves the same exact root-to-current path identity without introducing recursive validation or a second hierarchy cache.

## No transform mutation

Serialization and loading preserve authored transforms exactly.

Cross-hierarchy JSON processing does not change:

```text
ComponentInstance::transform()
SubassemblyInstance::transform()
ComponentGroundingState
ComponentSuppressionState
ComponentVisibility
```

No relationship is solved during load.

## Failure policy

Loading or structure validation fails closed on:

- unsupported relationship type spelling;
- unsupported relationship state spelling;
- empty endpoint path ids;
- empty local component ids;
- empty semantic-reference strings;
- invalid Distance/Angle value-family combinations;
- Distance units other than `mm`;
- Angle units other than `deg`;
- duplicate project-level cross-hierarchy constraint ids;
- invalid ordinary member/component/local relationship/joint structure;
- missing child assembly references or hierarchy cycles;
- endpoint occurrence paths absent from the exact rooted authored hierarchy;
- addressed local components absent from the assembly document reached by the exact path.

Semantic feature existence and target-family geometry compatibility are not Block-24 structure checks.

## Focused coverage

`tests/core/assembly_cross_hierarchy_constraint_tests.cpp` under:

```text
[core][assembly-cross-hierarchy-json]
```

proves:

- full five-family project JSON roundtrip;
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

## Explicitly deferred

Block 24 does not implement:

- active relationship-to-transform-authority incidence;
- solve-group connectivity;
- suppression-based relationship participation;
- numeric transform-authority variables;
- cross-hierarchy residual/Jacobian execution;
- solve-result snapshots or proposals;
- result application;
- cross-hierarchy rank/remaining-DOF diagnostics;
- cross-hierarchy joints.

## Next technical step

Implement Block 25 from `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`:

```text
active local + project-level cross-hierarchy relationships
  -> relationship-to-ComponentTransformAuthority incidence
  -> deterministic connected solve groups
```

The graph must keep local relationships once per containing `AssemblyDocument`, preserve occurrence-qualified cross-hierarchy endpoints, map repeated child occurrences to shared document/component transform authorities, and remain entirely derived and unpersisted.

# Assembly Generic Geometric Relationship Intent and JSON MVP-5

Status: implemented as Block 38 of `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`. Block 39 equation/solve integration is implemented in `docs/assembly-generic-relationship-equations-mvp5.md`.

This document is canonical for persistent local and Project-level `Coincident`, `Parallel`, and `Perpendicular` relationship intent, their JSON spellings, value-family validation, graph-participation boundary, and Block-39 handoff.

## Scope

Implemented:

- `AssemblyConstraintType::Coincident`;
- `AssemblyConstraintType::Parallel`;
- `AssemblyConstraintType::Perpendicular`;
- shared local `AssemblyConstraint` intent for all three families;
- shared occurrence-qualified `AssemblyHierarchyConstraint` intent for all three families;
- existing `AssemblyConstraintTarget` and `AssemblyHierarchyConstraintEndpoint` identity shapes without modification;
- active/inactive state reuse;
- exact target A/B order preservation;
- local and Project-level id-scope independence;
- canonical lowercase JSON spellings `coincident`, `parallel`, and `perpendicular`;
- fail-closed scalar-value validation;
- historical five-family JSON compatibility;
- explicit exclusion from current solve and motion graphs until Block 39.

Not implemented:

- Coincident, Parallel, or Perpendicular target compatibility entries;
- residual or equation descriptors for the new families;
- local or cross-hierarchy numeric execution for the new families;
- graph incidence or solve-group participation for the new families;
- freshness/application/diagnostic integration for the new families;
- any new JSON field or schema/version marker.

## Persistent relationship family expansion

The shared Core relationship type is now:

```text
Mate
Concentric
Distance
Insert
Angle
Coincident
Parallel
Perpendicular
```

The three Block-38 families reuse the same relationship records already used by the historical five families.

Local:

```text
AssemblyConstraint
  id
  name
  type
  target_a
  target_b
  state
  optional distance
  optional angle
```

Project-level:

```text
AssemblyHierarchyConstraint
  id
  name
  type
  target_a
  target_b
  state
  optional distance
  optional angle
```

No parallel relationship hierarchy, generic relationship base class, or second endpoint model is introduced.

## Endpoint identity remains unchanged

Local endpoint identity remains:

```text
(local ComponentInstanceId,
 semantic_reference)
```

Project-level endpoint identity remains:

```text
(occurrence_path,
 local ComponentInstanceId,
 semantic_reference)
```

`occurrence_path` remains the exact ordered root-to-current `SubassemblyInstanceId` sequence. The empty path addresses the explicit root occurrence.

The semantic-reference string remains opaque Core model intent. Constructing, adding, serializing, or loading a Block-38 relationship does not resolve Plane/Axis/Line/Point/Circle/Cylinder/Frame geometry.

## Target order and state

Target A/B order is persistent for every relationship family. Block 38 does not canonicalize, sort, or swap endpoints.

The existing state values are reused unchanged:

```text
active
inactive
```

An inactive generic relationship is still persistent model intent. State does not authorize Geometry execution in Block 38.

## Value-family rules

The existing scalar-value authority remains:

```text
Distance
  distance = required LengthMm
  angle    = absent

Angle
  distance = absent
  angle    = required AngleDeg

Mate
Concentric
Insert
Coincident
Parallel
Perpendicular
  distance = absent
  angle    = absent
```

Therefore `Coincident`, `Parallel`, and `Perpendicular` fail closed if either `distance` or `angle` is supplied.

The typed `AssemblyConstraint::create` validation remains the single value-family authority and is reused by `AssemblyHierarchyConstraint::create`. JSON parsing does not bypass it.

## Local and Project-level id scopes

Local relationship ids remain unique inside one `AssemblyDocument`.

Project-level cross-hierarchy relationship ids remain unique inside the Project-owned `cross_hierarchy_constraints[]` collection.

These scopes remain independent. One local relationship and one Project-level relationship may intentionally share the same `AssemblyConstraintId` text.

## JSON

Block 38 adds accepted type spellings only:

```text
coincident
parallel
perpendicular
```

Local JSON shape remains:

```json
{
  "id": "constraint.coincident",
  "name": "Coincident",
  "type": "coincident",
  "target_a": {
    "component_instance": "component.a",
    "semantic_reference": "topo:vertex:feature%2Ebox:top_front_right"
  },
  "target_b": {
    "component_instance": "component.b",
    "semantic_reference": "ref:construction_point:point_b"
  },
  "state": "active"
}
```

Project-level JSON shape remains:

```json
{
  "id": "constraint.cross.parallel",
  "name": "Parallel",
  "type": "parallel",
  "target_a": {
    "occurrence_path": [],
    "component_instance": "component.root",
    "semantic_reference": "ref:datum_axis:axis_root"
  },
  "target_b": {
    "occurrence_path": ["subassembly.child"],
    "component_instance": "component.child",
    "semantic_reference": "topo:linear_edge:feature%2Ebox:top_front"
  },
  "state": "inactive"
}
```

Neither shape gains a new field. Existing schema markers remain:

```text
blcad.assembly_document.mvp4 / version 1
blcad.project.mvp4 / version 1
```

Historical Mate/Concentric/Distance/Insert/Angle files retain the same accepted spellings and serialized shapes.

## Persistent-only execution boundary

Block 38 intentionally does not activate the three new families in existing solve or motion connectivity.

The private Core graph-participation boundary classifies:

```text
Mate / Concentric / Distance / Insert / Angle
  -> current graph participation = yes

Coincident / Parallel / Perpendicular
  -> current graph participation = no
```

This guard is required because the pre-Block-38 graph builders iterate generic `AssemblyConstraint` collections. Without an explicit boundary, merely extending the enum would accidentally place unsupported relationships into solve and motion groups before equations exist.

`AssemblyConstraintGraph`, `AssemblyCrossHierarchyConstraintGraph`, and `AssemblyCrossHierarchyMotionGraph` therefore preserve their historical equation-enabled participation set. Existing graph behavior is unchanged for the original five families.

The Block-37 `AssemblyTargetCompatibilityResolver` likewise has no compatibility entries for the Block-38 types. Direct Geometry compatibility/equation requests fail closed until Block 39.

## Mutation boundary

Adding Block-38 relationship intent may mutate only the owning relationship collection.

It does not mutate:

```text
ComponentInstance::transform()
SubassemblyInstance::transform()
PartDocument intent
semantic-reference strings
hierarchy structure
joint coordinates
```

Loading performs Core structure/value validation only and resolves no Geometry.

## Focused acceptance coverage

```bash
./build/dev/blcad_core_tests "[core][assembly-generic-relationship-intent]"
./build/dev/blcad_core_tests "[core][assembly-generic-relationship-json]"
```

`tests/core/assembly_generic_relationship_tests.cpp` proves:

- all three enum values and canonical lowercase spellings;
- local intent creation and active/inactive state retention;
- target A/B order retention;
- no distance/angle values on generic families;
- scalar injection fails closed through Core/JSON validation;
- Project-level occurrence-qualified intent;
- independent local and Project-level id scopes;
- local and cross-hierarchy transform immutability;
- exact occurrence-path JSON roundtrip;
- all three new type spellings roundtrip locally and Project-level;
- historical five-family spellings remain unchanged;
- generic relationships remain absent from current local solve, cross-hierarchy solve, and cross-hierarchy motion graphs.

## Implemented Block-39 continuation

Block 39 is implemented in `docs/assembly-generic-relationship-equations-mvp5.md`.

The three persistent families now have capability-driven equations and enter existing local/cross-hierarchy solve and motion graphs. Shared residual scaling, central finite differences, Gauss-Newton solving, freshness, atomic application, and Jacobian-rank diagnostics are reused without parallel infrastructure.

Block 40 joint target compatibility and the oriented `Frame` contract are implemented in `docs/assembly-joint-target-compatibility-mvp5.md`. Blocks 41–43 typed coordinate slots, additive compatible JSON, and shared vector drives are implemented in their canonical joint documents. The next technical step is Block 44: the Prismatic joint family.

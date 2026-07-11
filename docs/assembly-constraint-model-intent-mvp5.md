# Assembly Constraint Model Intent MVP-5

Status: implemented solver-independent assembly relationship records for Mate, Concentric, and Distance. Generated planar-face and generated-axis target resolution, planar Mate/Distance residuals, and read-only Concentric residual construction exist as separate downstream geometry layers.

## Goal

This block makes assembly relationships persistent model intent independently from geometry caches and solver output.

It stores what two component occurrences are intended to relate. It does not compute where either component must move.

## Typed identity

Assembly constraints use:

```text
AssemblyConstraintId
```

Constraint ids are independent from component, document, feature, and profile ids. They must be non-empty and unique within one `AssemblyDocument`.

## Semantic component targets

`AssemblyConstraintTarget` combines:

```text
component_instance   ComponentInstanceId
semantic_reference  persistent semantic token
```

Implemented examples:

```text
component.plate.1 : feature.base_extrude.top
component.plate.2 : feature.hole.axis
```

Validation guarantees:

- component instance id is non-empty
- semantic reference string is non-empty
- when a constraint enters an `AssemblyDocument`, both target component ids already exist in that assembly

The semantic token is intentionally opaque at this record layer. Raw OCCT names such as `Face17` or `Edge4` are not intended persistent representation.

Geometry layers currently interpret selected token families:

```text
feature.<feature-id>.top|bottom|right|left|front|back
feature.<feature-id>.axis
```

## Constraint types

The persistent seed supports:

```text
AssemblyConstraintType::Mate
AssemblyConstraintType::Concentric
AssemblyConstraintType::Distance
```

Flush, Coincident, Insert, Angle, Tangent, Lock, and joints remain future record families.

## Constraint record

`AssemblyConstraint` stores:

```text
id
name
type
target_a
target_b
state = active | inactive
distance = optional length quantity
```

Target A/B order is persistent intent and must be preserved.

Order is already observable in current residual conventions:

```text
Distance signed separation = dot(oB - oA, nA)
Concentric axis offset     = cross(oB - oA, dA)
```

`AssemblyConstraintState::Inactive` persists disabled relationship intent without deleting the record.

## Type-specific distance validation

Distance is stored as a BLCAD `Quantity`.

Rules:

- Distance requires one length quantity
- current length rules require a finite positive value
- Mate must not carry a distance value
- Concentric must not carry a distance value
- count quantities are rejected for Distance

Direct constraint-distance binding to an assembly parameter remains deferred until an explicit constraint dependency model exists.

## Assembly ownership

`AssemblyDocument` owns constraints by value:

```text
AssemblyDocument
  component_instances[]
  constraints[]
```

Public APIs include:

```text
add_constraint(AssemblyConstraint)
constraints()
constraint_count()
find_constraint(AssemblyConstraintId)
```

`add_constraint` validates unique ids and existing target component instances.

## Record-layer boundary

Adding or loading a constraint does not:

- mutate component transforms
- infer constraints from free placement
- resolve semantic geometry
- evaluate component transforms
- construct residuals
- solve placement
- enforce grounding
- compute remaining DOF
- classify constraint rank
- recompute member part geometry

JSON loading follows the same ownership and validation path and cannot silently move components.

## Downstream implemented layers

### Active relationship connectivity

```text
AssemblyConstraintGraph
```

The graph uses component endpoints and active/inactive state. It does not copy semantic geometry.

### Semantic target geometry

```text
AssemblyConstraintTargetResolver::resolve
AssemblyConstraintTargetResolver::resolve_axis
```

These resolve supported generated planar faces and the first generated-axis family to component-local descriptors.

### Transform evaluation

```text
AssemblyTransformEvaluator::evaluate_plane
AssemblyTransformEvaluator::evaluate_axis
```

These map local descriptors into assembly coordinates under the canonical persisted `RigidTransform` convention.

### Planar Mate/Distance residual construction

```text
AssemblyConstraintEquationBuilder
```

Mate:

```text
normal_opposition    = nA + nB
signed_separation_mm = dot(oB - oA, nA)
```

Distance:

```text
normal_parallelism   = cross(nA, nB)
signed_separation_mm = dot(oB - oA, nA)
distance_residual_mm = signed_separation_mm - target_distance_mm
```

The current shared numeric system, solver, and DOF analyzer consume these planar descriptors.

### Concentric residual construction

```text
AssemblyConcentricConstraintEquationBuilder
```

For semantic assembly-space axes:

```text
direction_parallelism = cross(dA, dB)
axis_offset_mm         = cross(oB - oA, dA)
```

Equal and opposed axis directions are accepted. Axial translation and rotation about the common axis are intentionally free.

Concentric residual descriptors are implemented and read-only. Their integration into the shared numeric system, rigid-body solver, and DOF diagnostics is the next block.

## Project structure validation

`Project::validate_assembly_constraints()` verifies that every persisted target refers to an owned assembly component instance.

`Project::validate_assembly_structure()` validates:

1. assembly members resolve to project-owned parts
2. component instances reference valid member/project parts
3. assembly constraints reference existing component instances

Several component instances may reference one owned `PartDocument`. Constraints relate occurrences, not duplicated part model records.

Semantic geometry resolution remains a downstream geometry-layer concern. A non-empty unsupported semantic token may be stored as intent and fail only when a geometry consumer attempts to interpret it.

## JSON representation

Assembly JSON retains the historical compatibility marker:

```text
blcad.assembly_document.mvp4
version 1
```

MVP-5 component instances and constraints extend this schema additively.

A Concentric record using the implemented axis family can be stored as:

```json
{
  "assembly_constraints": [
    {
      "id": "constraint.hole_alignment",
      "name": "Hole alignment",
      "type": "concentric",
      "target_a": {
        "component_instance": "component.plate.1",
        "semantic_reference": "feature.hole.axis"
      },
      "target_b": {
        "component_instance": "component.plate.2",
        "semantic_reference": "feature.hole.axis"
      },
      "state": "active"
    }
  ]
}
```

Compatibility rules:

- files without `assembly_constraints` load with zero constraints
- unsupported type/state strings are rejected
- Distance JSON must carry a millimeter distance value
- target component validation runs through `AssemblyDocument::add_constraint`
- project JSON inherits constraint persistence through embedded assembly JSON

No graph cache, resolved plane/axis descriptor, evaluated assembly-space geometry, residual descriptor, solver result, Jacobian, rank, DOF state, or solved-transform cache is serialized by this record block.

`feature.hole.axis` uses the already-existing semantic-reference string field. No schema field was added for semantic axes.

## Headless inspection

`blcad_inspect_project_components` reports stored component instances and assembly constraints, including target A/B semantic strings.

It also prints a derived active-constraint graph summary.

The inspector does not resolve semantic plane/axis geometry or run a solver.

## Tests

Core constraint tests cover target validation, type-specific distance rules, ownership, JSON roundtrip, and transform immutability.

Downstream geometry tests cover:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-equation]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric]"
```

## Current downstream boundary

Constraint model intent already supports Concentric records and the semantic generated-axis/Concentric residual path is implemented.

The next assembly step is to integrate those Concentric residuals into the shared numeric residual/Jacobian system, rigid-body solver, and local remaining-DOF diagnostics.

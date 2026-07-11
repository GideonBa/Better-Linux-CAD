# Assembly Constraint Model Intent MVP-5

Status: implemented solver-independent assembly relationship records for Mate, Concentric, and Distance. Geometry resolution, transform evaluation, and planar Mate/Distance residual construction exist as separate downstream read-only layers.

## Goal

This block makes assembly relationships persistent model intent independently from geometry caches and solver output.

It stores what two component occurrences are intended to relate. It does not compute where either component must move.

## Typed identity

Assembly constraints use:

```text
AssemblyConstraintId
```

The id is independent from component, document, and feature ids.

Constraint ids must be non-empty and unique within one `AssemblyDocument`.

## Semantic component targets

`AssemblyConstraintTarget` combines:

```text
component_instance   ComponentInstanceId
semantic_reference  persistent semantic token
```

Examples:

```text
component.bolt.1 : bolt.main_axis
component.plate  : feature.base_extrude.top
```

Validation guarantees:

- component instance id is non-empty
- semantic reference token is non-empty
- when a constraint enters an `AssemblyDocument`, both target component ids already exist in that assembly

The semantic token is intentionally opaque at this record layer. Raw OCCT names such as `Face17` or `Edge4` are not the intended persistent representation.

## Constraint types

The implemented persistent seed is limited to:

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

Target A/B order is persistent intent and must be preserved by downstream consumers. This matters for the implemented signed planar Distance residual convention.

`AssemblyConstraintState::Inactive` persists disabled relationship intent without deleting the record.

## Type-specific distance validation

Distance is stored as a BLCAD `Quantity`.

Rules:

- Distance requires one length quantity
- the current length rules require a finite positive value
- Mate must not carry a distance value
- Concentric must not carry a distance value
- count quantities are rejected for Distance

Direct binding of a constraint distance to an assembly parameter remains deferred until an explicit constraint dependency model exists.

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

Adding a constraint is read-only with respect to component placement/state.

This persistent record block does not:

- mutate component `RigidTransform` values
- infer constraints from free-placement edits
- resolve semantic geometry
- evaluate component transforms
- construct residuals
- solve placement
- enforce grounding
- compute remaining degrees of freedom
- detect under/fully/overconstrained assemblies
- recompute member part geometry

Loading constraints from JSON follows the same ownership and validation path and cannot silently move components.

## Downstream implemented layers

The persistent record is now consumed by three independent read-only layers.

### Active relationship connectivity

```text
AssemblyConstraintGraph
```

The graph uses component endpoints and active/inactive state. It does not copy semantic geometry.

### Semantic target geometry and transform evaluation

```text
AssemblyConstraintTargetResolver
AssemblyTransformEvaluator
```

These layers resolve supported generated-face targets and evaluate them in assembly coordinates.

### Planar equation/residual construction

```text
AssemblyConstraintEquationBuilder
```

The builder consumes active Mate and Distance records with supported generated planar targets.

Canonical Mate residuals:

```text
normal_opposition = nA + nB
signed_separation_mm = dot(oB - oA, nA)
```

Canonical planar Distance residuals:

```text
normal_parallelism = cross(nA, nB)
signed_separation_mm = dot(oB - oA, nA)
distance_residual_mm = signed_separation_mm - target_distance_mm
```

Distance separation is target-order dependent. The record layer therefore must preserve target A/B order exactly.

Concentric residual construction remains unsupported because semantic axis target resolution is not implemented.

See `docs/assembly-planar-constraint-equations-mvp5.md`.

## Project structure validation

`Project::validate_assembly_constraints()` verifies that every persisted target still resolves to an owned assembly component instance.

`Project::validate_assembly_structure()` validates:

1. assembly members resolve to project-owned parts
2. component instances reference valid member/project parts
3. assembly constraints reference existing component instances

Several component instances may reference one owned `PartDocument`. Constraints relate occurrences, not duplicated part model records.

## JSON representation

Assembly JSON retains the historical compatibility marker:

```text
blcad.assembly_document.mvp4
version 1
```

MVP-5 component instances and constraints extend this schema additively.

Constraints are stored in optional `assembly_constraints`:

```json
{
  "assembly_constraints": [
    {
      "id": "constraint.bolt_spacing",
      "name": "Bolt spacing",
      "type": "distance",
      "target_a": {
        "component_instance": "component.bolt.1",
        "semantic_reference": "bolt.main_axis"
      },
      "target_b": {
        "component_instance": "component.bolt.2",
        "semantic_reference": "bolt.main_axis"
      },
      "state": "active",
      "distance": {
        "unit": "mm",
        "value": 40.0
      }
    }
  ]
}
```

Compatibility rules:

- files without `assembly_constraints` load with zero constraints
- unsupported type/state strings are rejected
- Distance JSON must carry a millimeter distance value
- target validation runs through `AssemblyDocument::add_constraint`
- project JSON inherits constraint persistence through embedded assembly JSON

No graph cache, resolved target geometry, evaluated frame, residual descriptor, solver result, DOF state, or solved transform cache is serialized by this record block.

## Headless inspection

`blcad_inspect_project_components` reports component instances and stored assembly constraints.

For each constraint it prints:

- id and name
- type
- active/inactive state
- target A component id and semantic token
- target B component id and semantic token
- Distance value when present

The inspector also prints a derived active-constraint graph summary.

## Tests

`tests/core/assembly_constraint_tests.cpp` covers:

- empty target component ids
- empty semantic tokens
- empty constraint id/name
- Mate and Concentric rejecting distance values
- Distance requiring a length value
- Distance rejecting count quantities
- duplicate ids
- missing target components
- unchanged component transforms after adding constraints
- Mate/Concentric/Distance JSON roundtrip
- active/inactive state roundtrip
- semantic target order/token roundtrip
- Distance value roundtrip
- backward-compatible loading without `assembly_constraints`
- invalid JSON constraint targets
- project JSON roundtrip
- shared owned part intent across constrained occurrences
- project assembly-structure validation

Targeted record-layer command:

```bash
./build/dev/blcad_core_tests "[core][assembly-constraint]"
```

Graph, target-resolution, transform-evaluation, and equation-builder tests live in their own suites.

## Deliberate limitations

The record block remains independent from:

- semantic geometry interpretation
- local-to-assembly transform evaluation
- constraint residual construction
- rigid-body solving
- transform mutation from constraints
- remaining-DOF computation
- grounding enforcement
- under/fully/overconstrained analysis
- richer constraints and joints
- collision/interference analysis
- subassemblies
- assembly-level geometry instancing and STEP export

Geometry interpretation, transform evaluation, and planar Mate/Distance residual construction are implemented downstream but intentionally remain outside persistent record ownership.

## Current downstream boundary

The repository-wide next assembly block is a first rigid-body solver seed.

It should consume deterministic connected groups and supported active Mate/Distance residual descriptors, define grounded/fixed participation, variable transform representation, residual weighting, convergence behavior, and an explicit proposed-transform application boundary.

Concentric remains deferred until semantic axis targets and Concentric residual construction exist.

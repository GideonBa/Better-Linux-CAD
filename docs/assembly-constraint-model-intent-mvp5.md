# Assembly Constraint Model Intent MVP-5

Status: implemented solver-independent assembly relationship records for Mate, Concentric, and Distance.

## Goal

This block makes assembly relationships persistent model intent before BLCAD introduces a constraint graph, semantic target geometry resolution, or a rigid-body solver.

The implementation deliberately stores what two component occurrences are intended to relate. It does not yet compute where either component must move.

## Typed identity

Assembly constraints use a dedicated typed id:

```text
AssemblyConstraintId
```

The id is independent from `ComponentInstanceId`, `DocumentId`, and part feature ids.

Constraint ids must be non-empty and unique within one `AssemblyDocument`.

## Semantic component targets

`AssemblyConstraintTarget` combines:

```text
component_instance   ComponentInstanceId
semantic_reference  persistent semantic reference token
```

Example:

```text
component.bolt.1 : bolt.main_axis
component.plate  : feature.base_extrude.top
```

Validation guarantees:

- component instance id is non-empty
- semantic reference token is non-empty
- when a constraint enters an `AssemblyDocument`, target A and target B component instance ids must already exist in that assembly

The semantic reference token is intentionally opaque at this layer. The record layer does not resolve it to an OCCT face, edge, axis, vertex, or workplane.

Raw OCCT topology names such as `Face17` or `Edge4` are not the intended persistent model representation. Semantic target resolution remains a later geometry/assembly layer.

## Constraint types

The implemented seed is intentionally limited to:

```text
AssemblyConstraintType::Mate
AssemblyConstraintType::Concentric
AssemblyConstraintType::Distance
```

No Flush, Coincident, Insert, Angle, Tangent, Lock, or joint records are added by this block.

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

The record has stable identity and two semantic component targets.

`AssemblyConstraintState::Inactive` persists disabled relationship intent without deleting the record.

## Type-specific distance validation

Distance is stored as a BLCAD `Quantity` so units and finite numeric validation remain explicit.

Rules:

- `Distance` requires exactly one length quantity
- the current `Quantity::length_mm` rules require a finite value greater than zero
- `Mate` must not carry a distance value
- `Concentric` must not carry a distance value
- count quantities are rejected for Distance

This block stores a concrete distance value. Binding a distance constraint directly to an assembly parameter is deferred until the assembly dependency/constraint graph design is introduced.

## Assembly ownership

`AssemblyDocument` owns constraints by value:

```text
AssemblyDocument
  component_instances[]
  constraints[]
```

Public APIs added by this block:

```text
add_constraint(AssemblyConstraint)
constraints()
constraint_count()
find_constraint(AssemblyConstraintId)
```

`add_constraint` validates:

- unique constraint id
- existing target A component instance
- existing target B component instance

Target construction already validates non-empty component ids and semantic reference tokens. `AssemblyConstraint::create` already validates identity, name, and type-specific distance requirements.

## No-solver boundary

Adding a constraint is read-only with respect to all component placement/state records.

The implementation does not:

- mutate either component `RigidTransform`
- infer a constraint from a free-placement edit
- resolve semantic target geometry
- construct a constraint graph
- move grounded or free components
- compute remaining degrees of freedom
- detect underdefined, fully constrained, or overconstrained assemblies
- recompute member part geometry

Loading constraints from JSON follows the same path: component instances are loaded first, then constraints are validated and added. JSON loading therefore cannot silently solve or move component instances.

## Project structure validation

`Project::validate_assembly_constraints()` verifies that every persisted constraint target still resolves to an owned assembly component instance.

`Project::validate_assembly_structure()` now validates, in order:

1. assembly member parts resolve to project-owned `PartDocument`s
2. component instances reference valid member/project parts
3. assembly constraints reference existing component instances

Several component instances may still reference one owned `PartDocument`. Constraints relate occurrences, not duplicated part model records.

## JSON representation

Assembly JSON keeps the compatibility marker:

```text
blcad.assembly_document.mvp4
version 1
```

The marker is historical. MVP-5 component instances and assembly constraints extend the schema additively.

Constraints are stored in the optional array:

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

Mate and Concentric records omit `distance`.

Compatibility rules:

- old files without `assembly_constraints` load with zero constraints
- unsupported constraint type/state strings are rejected
- Distance JSON must carry a millimeter distance value
- target validation runs through the normal `AssemblyDocument::add_constraint` path
- project JSON automatically inherits constraint persistence because it embeds assembly JSON

No solver result, resolved OCCT topology, DOF state, or solved transform cache is serialized by this block.

## Headless inspection

`blcad_inspect_project_components` now reports both component instances and stored assembly constraints.

For each constraint it prints:

- constraint id and name
- type
- active/inactive state
- target A component id and semantic token
- target B component id and semantic token
- distance in millimeters when present

The checked-in `examples/component_instances.blcad.project.json` contains Mate, Concentric, and Distance records for this read-only inspection path.

The example intentionally uses semantic reference tokens without geometry resolution. That is the exact boundary of this MVP block.

## Test coverage

`tests/core/assembly_constraint_tests.cpp` covers:

- empty target component ids
- empty semantic reference tokens
- empty constraint id/name
- Mate and Concentric rejecting distance values
- Distance requiring a length value
- Distance rejecting count quantities
- duplicate constraint ids
- missing target component instances
- unchanged component transforms after adding Mate, Concentric, and Distance
- Mate/Concentric/Distance assembly JSON roundtrip
- active/inactive state roundtrip
- semantic target roundtrip
- Distance value roundtrip
- backward-compatible loading without `assembly_constraints`
- invalid constraint targets from JSON
- project JSON roundtrip
- shared owned `PartDocument` intent across multiple constrained component instances
- project assembly-structure validation with constraints

Targeted test command after a core build:

```bash
./build/dev/blcad_core_tests "[core][assembly-constraint]"
```

Complete core workflow:

```bash
cmake --workflow --preset dev-build-test
```

## Deferred work

The following remain intentionally outside this block:

- semantic target geometry resolution
- constraint graph construction
- rigid-body solving
- transform mutation from constraints
- remaining DOF computation and display
- enforced grounding
- underdefined/fully constrained/overconstrained state analysis
- Insert, Angle, Tangent, Flush, Coincident, and Lock constraints
- joints and limits
- collision/interference checks
- subassemblies
- assembly-level geometry instancing and STEP export

## Next technical step

Build the first read-only assembly constraint graph over the now-stable persistent records.

The graph should use `ComponentInstanceId` nodes and active `AssemblyConstraintId` edges, provide deterministic adjacency and connected-component queries, ignore inactive constraints, and validate graph construction without resolving geometry or changing `RigidTransform` values.

Rigid-body solving and remaining-DOF computation should come only after this graph seed is stable.

# Assembly Constraint Model Intent MVP-5

Status: implemented solver-independent assembly relationship records for Mate, Concentric, and Distance. A separate read-only connectivity graph is now also implemented in `docs/assembly-constraint-graph-mvp5.md`.

## Goal

This block makes assembly relationships persistent model intent before semantic target geometry resolution or a rigid-body solver.

The implementation deliberately stores what two component occurrences are intended to relate. It does not compute where either component must move.

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

This block stores a concrete distance value. Binding a distance constraint directly to an assembly parameter remains deferred until an explicit assembly parameter-to-constraint dependency model exists.

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

This record block does not itself:

- mutate either component `RigidTransform`
- infer a constraint from a free-placement edit
- resolve semantic target geometry
- move grounded or free components
- compute remaining degrees of freedom
- detect underdefined, fully constrained, or overconstrained assemblies
- recompute member part geometry

A separate derived `AssemblyConstraintGraph` now reads these records to expose active connectivity. Graph construction also remains read-only and does not change this record-layer boundary.

Loading constraints from JSON follows the same path: component instances are loaded first, then constraints are validated and added. JSON loading therefore cannot silently solve or move component instances.

## Project structure validation

`Project::validate_assembly_constraints()` verifies that every persisted constraint target still resolves to an owned assembly component instance.

`Project::validate_assembly_structure()` validates, in order:

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

No solver result, resolved OCCT topology, graph cache, DOF state, or solved transform cache is serialized. The connectivity graph is fully regenerated from component and constraint intent.

## Headless inspection

`blcad_inspect_project_components` reports component instances and stored assembly constraints.

For each constraint it prints:

- constraint id and name
- type
- active/inactive state
- target A component id and semantic token
- target B component id and semantic token
- distance in millimeters when present

The inspector now also builds `AssemblyConstraintGraph` and prints a compact active-edge and connected-group summary. Graph details are documented in `docs/assembly-constraint-graph-mvp5.md`.

The checked-in `examples/component_instances.blcad.project.json` contains Mate, Concentric, and Distance records for this read-only inspection path.

The example intentionally uses semantic reference tokens without geometry resolution. That remains the exact target-resolution boundary.

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

The separate graph suite is documented in `docs/assembly-constraint-graph-mvp5.md`.

Targeted record-layer test command after a core build:

```bash
./build/dev/blcad_core_tests "[core][assembly-constraint]"
```

Complete core workflow:

```bash
cmake --workflow --preset dev-build-test
```

## Deferred work

The following remain intentionally outside this record block:

- semantic target geometry resolution
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

The read-only connectivity graph is no longer deferred; it is implemented as a separate derived block and intentionally does not change the persistent record schema.

## Next technical step

The completed read-only constraint graph is documented in `docs/assembly-constraint-graph-mvp5.md`.

The next assembly block is a read-only semantic target resolution seed. It should resolve an `AssemblyConstraintTarget` through its component occurrence to the referenced project-owned `PartDocument` and support the currently implemented generated-face semantic reference family as component-local planar descriptors.

Unsupported semantic target families must fail explicitly. Constraint equation construction, assembly-space transform math, rigid-body solving, remaining DOF, and solved transform updates remain later steps.

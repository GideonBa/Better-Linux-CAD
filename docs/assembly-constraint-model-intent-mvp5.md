# Assembly Constraint Model Intent MVP-5

Status: implemented solver-independent assembly relationship records for Mate, Concentric, and Distance. The read-only constraint graph, generated-face target resolver, and rigid-transform evaluator are implemented as separate downstream layers.

## Goal

This block makes assembly relationships persistent model intent independently from geometry resolution, transform evaluation, equation construction, and rigid-body solving.

The record layer stores what two component occurrences are intended to relate. It does not compute where either component must move.

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

Examples:

```text
component.bolt.1 : bolt.main_axis
component.plate  : feature.base_extrude.top
```

Validation guarantees:

- component instance id is non-empty
- semantic reference token is non-empty
- when a constraint enters an `AssemblyDocument`, target A and target B component instance ids must already exist in that assembly

The semantic reference token is intentionally opaque at this layer. The record layer does not resolve it to a face, edge, axis, vertex, workplane, or OCCT topology id.

Raw OCCT topology names such as `Face17` or `Edge4` are not the intended persistent model representation.

The separate `AssemblyConstraintTargetResolver` now interprets the first supported generated planar face family. That does not change the general persistent token model or add geometry logic to this record layer.

## Constraint types

The implemented seed is limited to:

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

Target construction already validates non-empty component ids and semantic reference tokens. `AssemblyConstraint::create` validates identity, name, and type-specific distance requirements.

## No-solver boundary

Adding a constraint is read-only with respect to all component placement/state records.

This record block does not itself:

- mutate either component `RigidTransform`
- infer a constraint from a free-placement edit
- resolve semantic target geometry
- evaluate component-local geometry in assembly space
- construct constraint equations or residuals
- move grounded or free components
- compute remaining degrees of freedom
- detect underdefined, fully constrained, or overconstrained assemblies
- recompute member part geometry

Separate derived layers now build graph connectivity, resolve supported generated-face targets, and evaluate valid persisted transforms. None of them changes this record-layer boundary.

Loading constraints from JSON follows the same path: component instances are loaded first, then constraints are validated and added. JSON loading cannot silently solve or move component instances.

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

No solver result, resolved target descriptor, evaluated assembly-space frame, graph cache, equation cache, DOF state, or solved transform cache is serialized. Derived assembly data is regenerated from model intent.

## Downstream read-only pipeline

The current implemented downstream path is:

```text
AssemblyConstraint record
  -> AssemblyConstraintGraph              # active relationship connectivity
  -> AssemblyConstraintTargetResolver     # supported local target frames
  -> AssemblyTransformEvaluator           # assembly-space target frames
  -> future equation/residual builder
  -> future rigid-body solver
```

The record layer remains the persistent source of relationship intent. Each downstream layer consumes records without rewriting them.

## Headless inspection

`blcad_inspect_project_components` reports component instances and stored assembly constraints.

For each constraint it prints:

- constraint id and name
- type
- active/inactive state
- target A component id and semantic token
- target B component id and semantic token
- distance in millimeters when present

The inspector also builds `AssemblyConstraintGraph` and prints a compact connectivity summary.

The checked-in `examples/component_instances.blcad.project.json` contains Mate, Concentric, and Distance records for this inspection path.

The example may contain semantic token families that are not yet supported by the geometry resolver. Persistent record support is intentionally broader than the currently implemented geometric target families.

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

Graph, target-resolution, and transform-evaluation suites are documented separately.

Targeted record-layer test command after a core build:

```bash
./build/dev/blcad_core_tests "[core][assembly-constraint]"
```

Complete core workflow:

```bash
cmake --workflow --preset dev-build-test
```

## Deferred work from the record block

The following remain outside this persistent record layer:

- geometry interpretation of semantic tokens
- local-to-assembly transform evaluation
- constraint equation/residual construction
- rigid-body solving
- transform mutation from constraints
- remaining DOF computation and display
- enforced grounding
- underdefined/fully constrained/overconstrained analysis
- richer constraint and joint families
- collision/interference checks
- subassemblies
- assembly-level geometry instancing and STEP export

Geometry interpretation and transform evaluation are now implemented as separate read-only layers. They remain intentionally outside this record block.

## Next technical step

The repository-wide next assembly block is read-only planar Mate/Distance equation/residual construction.

It should consume active persisted records, resolve the currently supported generated-face targets, evaluate both planes in assembly space, and construct documented deterministic residual data without solving or mutating component transforms. Concentric remains deferred until semantic axis references are implemented.

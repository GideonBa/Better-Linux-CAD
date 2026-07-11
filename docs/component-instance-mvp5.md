# MVP 5 Seed: Component instances without constraints

Status: implemented seed. Component instances are stored as assembly structure records and reference project-owned part documents.

This document describes the first MVP-5 assembly-structure increment above the MVP-4 project container.

## Goal

The goal is to allow one part document to appear multiple times in an assembly without duplicating the part geometry or part model intent.

The seed must not:

- solve assembly constraints
- create mate, concentric, distance, angle, insert, or tangent constraints
- compute degrees of freedom
- perform collision checks
- generate an assembly-level STEP file
- duplicate `PartDocument` geometry for every visible instance

## Implemented records

The implemented API lives in `include/blcad/core/assembly_document.hpp`.

```text
ComponentInstance
  id
  name
  referenced_part_document
  visibility
  suppression_state
  grounding_state
  transform

RigidTransform
  translation_mm
  rotation_deg
```

`ComponentInstanceId` is a typed id in `include/blcad/core/id.hpp`.

The first state enums are:

```text
ComponentVisibility
  visible
  hidden

ComponentSuppressionState
  active
  suppressed

ComponentGroundingState
  free
  grounded
```

The transform is a simple free-placement record. It is model intent for display/placement, not the result of a solver.

## Assembly ownership

`AssemblyDocument` owns component instances by value.

```text
AssemblyDocument
  member_parts[]
  component_instances[]
```

A component instance must reference an already registered `member_part`. This keeps the instance graph tied to assembly members and prevents arbitrary document ids from appearing in the assembly structure.

The first seed supports multiple component instances referencing the same member part document. That is the intended way to represent repeated parts without duplicating part model intent.

## Project validation

`Project` still owns the concrete `PartDocument` objects.

Project-level validation now has three relevant checks:

```text
validate_member_parts()
validate_component_instances()
validate_assembly_structure()
```

`validate_member_parts` checks that every assembly member id resolves to an owned project part document.

`validate_component_instances` checks that every component instance references an assembly member and that the referenced member resolves to an owned project part document.

`validate_assembly_structure` runs both checks and is used when loading project JSON and before project-level assembly parameter propagation.

## JSON persistence

Component instances are serialized as part of the existing assembly document JSON schema:

```text
blcad.assembly_document.mvp4
```

A representative entry has this shape:

```json
{
  "id": "component.bolt.1",
  "name": "Bolt 1",
  "referenced_part_document": "part.bolt",
  "visibility": "visible",
  "suppression_state": "active",
  "grounding_state": "grounded",
  "transform": {
    "translation_mm": {"x": 10.0, "y": 20.0, "z": 30.0},
    "rotation_deg": {"x": 0.0, "y": 0.0, "z": 45.0}
  }
}
```

Because `Project` embeds the assembly JSON, project JSON automatically persists component instances as well.

Old assembly documents without `component_instances` still deserialize because the field is optional and defaults to an empty list.

## Headless inspection

The seed adds a small non-geometry inspection executable:

```text
blcad_inspect_project_components <input.blcad.project.json>
```

It loads a project file, validates assembly structure, and prints each component instance with its referenced part document and state fields.

This is an inspection hook only. It does not export an assembly, solve transforms, or create component geometry.

## Test coverage

The tests cover:

- required `ComponentInstance` identity fields
- transform storage
- `AssemblyDocument` rejecting component instances that do not reference member parts
- unique component instance ids
- assembly JSON roundtrip for component instances
- project-level validation against owned part documents
- two component instances referencing one owned part document without duplicating the part document
- project JSON roundtrip preserving those two instances

## Deliberate limitations

This block does not implement transform editing APIs beyond construction and JSON loading.

It does not implement constraints, solver output, remaining degrees of freedom, grounding behavior beyond stored state, suppression effects on recompute/export, collision checks, subassemblies, or assembly-level STEP export.

A later seed should add explicit component-placement update APIs for changing free transforms and state fields while preserving the no-solver boundary.

# MVP 5: Component instances and explicit free placement

Status: implemented component-instance seed and explicit placement/state update block. Component instances are stored as assembly model-intent records, reference project-owned part documents, and can be edited without an assembly solver.

This document describes the first MVP-5 assembly-structure increments above the MVP-4 project container.

## Goal

The goal is to allow one part document to appear multiple times in an assembly without duplicating the part geometry or part model intent, while allowing existing component instances to receive explicit placement and state edits.

The current block must not:

- solve assembly constraints
- create mate, concentric, distance, angle, insert, or tangent constraints
- compute degrees of freedom
- infer a placement from grounding or other component state
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

The implemented state enums are:

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

The transform remains a simple free-placement record. It is model intent for display and placement, not solver output.

## Explicit component updates

`ComponentInstance` provides copy-style update operations that preserve component identity, display name, and referenced part document:

```text
with_transform(...)
with_visibility(...)
with_suppression_state(...)
with_grounding_state(...)
```

`AssemblyDocument` exposes the public mutation boundary for stored component instances:

```text
set_component_instance_transform(...)
set_component_instance_visibility(...)
set_component_instance_suppression_state(...)
set_component_instance_grounding_state(...)
```

Each `AssemblyDocument` update:

- requires an existing `ComponentInstanceId`
- replaces only the requested placement/state field
- preserves component id, name, and `referenced_part_document`
- returns the updated component-instance index
- does not modify or duplicate a `PartDocument`
- does not infer constraints, solve transforms, or recompute degrees of freedom
- does not trigger part dependency-graph invalidation or part geometry recompute

The no-solver boundary is deliberate. For example, a component currently marked `grounded` can still receive a direct transform update in this block. `grounded` is stored model intent only until a later assembly solver enforces it as a fixed rigid-body condition.

Visibility and suppression are also stored state only. They do not yet remove components from geometry execution, assembly export, collision checks, or solver input because those assembly consumers do not exist yet.

## Assembly ownership

`AssemblyDocument` owns component instances by value.

```text
AssemblyDocument
  member_parts[]
  component_instances[]
```

A component instance must reference an already registered `member_part`. This keeps the instance graph tied to assembly members and prevents arbitrary document ids from appearing in the assembly structure.

Multiple component instances may reference the same member part document. That is the intended representation for repeated parts without duplicating part model intent.

Placement/state updates cannot change `referenced_part_document`; changing the referenced model is intentionally not part of this update API.

## Project validation

`Project` still owns the concrete `PartDocument` objects.

Project-level validation has three relevant checks:

```text
validate_member_parts()
validate_component_instances()
validate_assembly_structure()
```

`validate_member_parts` checks that every assembly member id resolves to an owned project part document.

`validate_component_instances` checks that every component instance references an assembly member and that the referenced member resolves to an owned project part document.

`validate_assembly_structure` runs both checks and is used when loading project JSON and before project-level assembly parameter propagation.

Because the new placement/state APIs preserve `referenced_part_document`, a valid project remains structurally valid after those updates. Tests explicitly cover two instances referencing one owned part document before and after state/placement changes.

## JSON persistence

Component instances are serialized as part of the existing assembly document JSON schema:

```text
blcad.assembly_document.mvp4
```

The schema version does not change for this block because no new persisted fields are introduced. The update API changes existing component-instance values that were already serialized by the first MVP-5 seed.

A representative updated entry has this shape:

```json
{
  "id": "component.bolt.1",
  "name": "Bolt 1",
  "referenced_part_document": "part.bolt",
  "visibility": "hidden",
  "suppression_state": "suppressed",
  "grounding_state": "grounded",
  "transform": {
    "translation_mm": {"x": -12.5, "y": 8.0, "z": 72.25},
    "rotation_deg": {"x": 15.0, "y": 30.0, "z": 90.0}
  }
}
```

Assembly JSON roundtrip preserves transform, visibility, suppression state, and grounding state after explicit updates.

Because `Project` embeds the assembly JSON, project JSON automatically persists the updated component state as well.

Old assembly documents without `component_instances` still deserialize because the field is optional and defaults to an empty list.

## Headless inspection

The non-geometry inspection executable remains:

```text
blcad_inspect_project_components <input.blcad.project.json>
```

It loads a project file, validates assembly structure, and prints each component instance with:

```text
referenced part document
visibility
suppression state
grounding state
translation_mm
rotation_deg
```

Therefore a project file written after component placement/state updates exposes the changed values directly through the existing inspection tool. The tool is still read-only: it does not edit a project, export an assembly, solve transforms, or create component geometry.

## Test coverage

The tests cover:

- required `ComponentInstance` identity fields
- initial transform storage
- `AssemblyDocument` rejecting component instances that do not reference member parts
- unique component instance ids
- explicit transform, visibility, suppression, and grounding updates
- rejection of placement/state updates for unknown component instance ids
- direct transform updates while the component is still marked `grounded`, proving the no-solver boundary
- preservation of component id, name, and referenced part document across updates
- assembly JSON roundtrip for updated placement and state values
- project-level validation against owned part documents
- two component instances referencing one owned part document without duplicating the part document
- project JSON roundtrip preserving updated component placement/state and shared part ownership

## Deliberate limitations

This block does not implement semantic component reference targets, assembly constraint records, constraint graphs, solver output, remaining degrees of freedom, enforced grounding behavior, suppression effects on assembly consumers, collision checks, subassemblies, or assembly-level STEP export.

The next MVP-5 increment should introduce semantic component reference targets and solver-independent `AssemblyConstraint` model-intent records for Mate, Concentric, and Distance constraints. Constraint solving remains a separate later step.

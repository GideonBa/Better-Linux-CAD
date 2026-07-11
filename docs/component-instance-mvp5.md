# MVP 5: Component instances and explicit free placement

Status: implemented component-instance seed and explicit placement/state update block. Component instances are stored as assembly model-intent records, reference project-owned part documents, and can be edited without an assembly solver.

This document is the canonical detailed description of the implemented MVP-5 component-instance and free-placement block above the MVP-4 project container. The later implemented constraint-record block is documented separately in `docs/assembly-constraint-model-intent-mvp5.md`.

## Goal

The goal is to let one project-owned part document appear as multiple assembly occurrences without duplicating the owned `PartDocument` or its model intent, while allowing existing component instances to receive explicit placement and state edits.

Assembly-level geometry instancing is not implemented yet. This block models assembly occurrences and placement/state intent only.

The component-instance block itself must not:

- solve assembly constraints
- create constraint records as a side effect of placement/state updates
- compute degrees of freedom
- infer a placement from grounding or other component state
- perform collision checks
- generate an assembly-level STEP file
- duplicate an owned `PartDocument` for every component occurrence

Mate, Concentric, and Distance constraint records now exist as a separate solver-independent MVP-5 layer. They do not change the guarantees of this component-placement API.

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

The transform is a simple free-placement record. Translation is stored in millimeters and rotation in degrees. All six numeric transform components must be finite; `NaN` and positive or negative infinity are rejected during `ComponentInstance::create` and therefore also during transform updates.

This finite-value rule keeps persisted transform values within the normal JSON number domain and protects assembly/project JSON roundtrip.

## Explicit component updates

`ComponentInstance` provides copy-style update operations that preserve component identity, display name, and referenced part document:

```text
with_transform(...)
with_visibility(...)
with_suppression_state(...)
with_grounding_state(...)
```

`AssemblyDocument` exposes the update boundary for component instances stored in the assembly:

```text
set_component_instance_transform(...)
set_component_instance_visibility(...)
set_component_instance_suppression_state(...)
set_component_instance_grounding_state(...)
```

Each stored-instance update:

- rejects an empty component instance id
- requires the target `ComponentInstanceId` to exist
- replaces only the requested placement/state field
- preserves component id, name, and `referenced_part_document`
- returns the updated component-instance index
- rejects a transform update with any non-finite component
- leaves the stored instance unchanged when validation fails
- does not modify or duplicate a `PartDocument`
- does not infer constraints, solve transforms, or recompute degrees of freedom
- does not trigger part dependency-graph invalidation or part geometry recompute

The four public update entry points share one internal lookup/replacement path so empty-id handling, missing-instance handling, validation propagation, and replacement semantics cannot drift independently.

The no-solver boundary is deliberate. For example, a component currently marked `grounded` can still receive a direct transform update. `grounded` is stored model intent only until a later assembly solver enforces it as a fixed rigid-body condition.

Visibility and suppression are also stored state only. They do not yet remove components from assembly geometry execution, assembly export, collision checks, or solver input because those assembly consumers do not exist yet.

Adding or loading an `AssemblyConstraint` also leaves these free-placement transforms unchanged. Constraint records and placement edits are separate model-intent operations.

## Assembly ownership

`AssemblyDocument` owns component instances by value.

```text
AssemblyDocument
  member_parts[]
  component_instances[]
```

A component instance must reference an already registered `member_part`. This keeps the instance graph tied to assembly members and prevents arbitrary document ids from appearing in the assembly structure.

Multiple component instances may reference the same member part document. That is the intended representation for repeated assembly occurrences without duplicating the owned part model intent.

Placement/state updates cannot change `referenced_part_document`; changing the referenced model is intentionally not part of this update API.

## Project validation

`Project` owns the concrete `PartDocument` objects.

Project-level validation has four relevant checks:

```text
validate_member_parts()
validate_component_instances()
validate_assembly_constraints()
validate_assembly_structure()
```

`validate_member_parts` checks that every assembly member id resolves to an owned project part document.

`validate_component_instances` checks that every component instance references an assembly member and that the referenced member resolves to an owned project part document.

`validate_assembly_constraints` belongs to the later constraint-record layer and verifies constraint component targets. See `docs/assembly-constraint-model-intent-mvp5.md`.

`validate_assembly_structure` runs all current structure checks and is used when loading project JSON and before project-level assembly parameter propagation.

Because placement/state updates preserve `referenced_part_document`, a structurally valid project remains structurally valid after those updates. Tests explicitly cover two instances referencing one owned part document before and after state/placement changes.

## JSON persistence

Component instances are serialized as an optional field of the existing assembly document JSON schema marker:

```text
blcad.assembly_document.mvp4
version 1
```

The `mvp4` marker is retained as a historical compatibility marker. It does not mean that component instances are an MVP-4 feature. The first MVP-5 component-instance seed extended the existing schema additively with optional `component_instances`; older assembly documents without that field still load as an assembly with zero component instances.

The placement/state update block introduces no additional persisted fields. It changes values of fields that were already part of `component_instances`, so the existing marker and version remain unchanged.

The later constraint-record block additively introduces optional `assembly_constraints` while retaining the same compatibility marker. Its JSON rules are documented in `docs/assembly-constraint-model-intent-mvp5.md`.

A representative updated component entry has this shape:

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

## Headless inspection

The non-geometry inspection executable is:

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

The inspector now also prints stored assembly constraints, but that output belongs to the separate constraint model-intent block. See `docs/assembly-constraint-model-intent-mvp5.md`.

The tool is read-only: it does not edit a project, export an assembly, solve transforms, or create component geometry.

## Test coverage

The component-instance tests cover:

- required `ComponentInstance` identity fields
- initial transform storage
- rejection of non-finite translation and rotation components
- `AssemblyDocument` rejecting component instances that do not reference member parts
- unique component instance ids
- explicit transform, visibility, suppression, and grounding updates
- rejection of empty and unknown component instance ids in the update path
- rejection of non-finite transform updates without changing the previously stored transform
- direct transform updates while the component is still marked `grounded`, proving the no-solver boundary
- preservation of component id, name, and referenced part document across updates
- assembly JSON roundtrip for updated placement and state values
- project-level validation against owned part documents
- two component instances referencing one owned part document without duplicating the owned `PartDocument`
- project JSON roundtrip preserving updated component placement/state and shared part ownership

Assembly constraint tests are intentionally separate in `tests/core/assembly_constraint_tests.cpp`.

## Deliberate limitations

This component-instance block does not implement constraint graph construction, semantic target geometry resolution, solver output, remaining degrees of freedom, enforced grounding behavior, suppression effects on assembly consumers, collision checks, subassemblies, assembly-level geometry instancing, or assembly-level STEP export.

Solver-independent semantic Mate, Concentric, and Distance records are now implemented separately. The next assembly increment is the read-only active-constraint graph described in `docs/assembly-constraint-model-intent-mvp5.md`, `docs/mvp-plan.md`, and `docs/assembly-system.md`.

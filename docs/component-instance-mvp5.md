# MVP 5: Component Instances and Explicit Free Placement

Status: implemented component-instance and explicit placement/state update block. Component instances are assembly model-intent records that reference project-owned part documents and remain directly editable without a solver.

## Goal

One project-owned part document can appear as multiple assembly occurrences without duplicating the owned `PartDocument` or its feature intent.

Existing component instances can receive explicit placement and state edits.

This record/update block itself must not:

- solve assembly constraints
- create constraints as a side effect of placement edits
- compute remaining degrees of freedom
- infer placement from grounding
- perform collision checks
- export assembly-level STEP
- duplicate an owned part for each occurrence

Downstream graph, target-resolution, transform-evaluation, and planar residual layers are implemented separately.

## Implemented records

The API lives in `include/blcad/core/assembly_document.hpp`.

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

Implemented state enums:

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

Translation is stored in millimeters and rotation in degrees. All six transform components must be finite; `NaN` and positive/negative infinity are rejected.

The exact transform geometry semantics are defined in `docs/assembly-rigid-transform-evaluation-mvp5.md`.

## Explicit component updates

`ComponentInstance` provides copy-style operations:

```text
with_transform(...)
with_visibility(...)
with_suppression_state(...)
with_grounding_state(...)
```

`AssemblyDocument` exposes stored-instance updates:

```text
set_component_instance_transform(...)
set_component_instance_visibility(...)
set_component_instance_suppression_state(...)
set_component_instance_grounding_state(...)
```

Each update:

- rejects an empty component id
- requires the target component to exist
- replaces only the requested placement/state field
- preserves component id, name, and referenced part document
- returns the updated component index
- rejects non-finite transform values
- leaves the stored instance unchanged on validation failure
- does not modify or duplicate a `PartDocument`
- does not infer constraints or run a solver
- does not trigger part invalidation or geometry recompute

The four public update entry points share one internal lookup/replacement path.

## No-solver boundary

A component marked `grounded` can still receive a direct transform update. Grounding is stored model intent until the assembly solver defines fixed-body participation.

Visibility and suppression are also stored state only. They do not currently remove components from graph connectivity, target resolution, transform evaluation, residual construction, geometry execution, export, or collision analysis.

Adding constraints, deriving graph connectivity, resolving targets, evaluating transforms, and constructing residuals leave stored component transforms unchanged.

## Assembly ownership

`AssemblyDocument` owns component instances by value:

```text
AssemblyDocument
  member_parts[]
  component_instances[]
```

A component must reference an already registered member part.

Multiple component instances may reference the same member part. This is the intended representation for repeated occurrences.

Placement/state updates cannot change `referenced_part_document`.

## Project validation

`Project` owns the concrete `PartDocument` objects.

Relevant validation APIs are:

```text
validate_member_parts()
validate_component_instances()
validate_assembly_constraints()
validate_assembly_structure()
```

`validate_member_parts` checks that every assembly member resolves to an owned project part.

`validate_component_instances` checks each component against assembly membership and project ownership.

`validate_assembly_constraints` checks constraint component targets.

`validate_assembly_structure` runs the current structure checks.

Because component updates preserve `referenced_part_document`, a structurally valid project remains structurally valid after placement/state edits.

## JSON persistence

Component instances are an optional field of the existing assembly compatibility schema:

```text
blcad.assembly_document.mvp4
version 1
```

The marker is historical. MVP-5 component instances extended the schema additively.

A representative component entry is:

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

Assembly and project JSON roundtrip preserve component placement/state.

Derived graph, target, assembly-space frame, and residual data are not component persistence fields.

## Downstream assembly layers

The current component record is consumed read-only by:

```text
AssemblyConstraintGraph
AssemblyConstraintTargetResolver
AssemblyTransformEvaluator
AssemblyConstraintEquationBuilder
```

`AssemblyConstraintGraph` uses component identity for relationship connectivity.

`AssemblyConstraintTargetResolver` maps a target component occurrence to its referenced project-owned part and supported local generated-face frame.

`AssemblyTransformEvaluator` applies the persisted `RigidTransform` convention to points, vectors, and planar frames.

`AssemblyConstraintEquationBuilder` evaluates active planar Mate/Distance targets and constructs residual descriptors.

None of those layers mutates a stored component transform.

## Headless inspection

The core inspection executable is:

```text
blcad_inspect_project_components <input.blcad.project.json>
```

It reports:

- referenced part document
- visibility
- suppression state
- grounding state
- translation
- rotation
- stored constraints
- derived graph-group summary

The tool is read-only.

## Tests

The component-instance tests cover:

- required identity fields
- initial transform storage
- rejection of non-finite transforms
- member-part reference validation
- unique component ids
- explicit transform and state updates
- empty/unknown update ids
- failed transform updates preserving stored placement
- direct transform updates while grounded
- identity and referenced-part preservation across updates
- assembly JSON roundtrip
- project-level part ownership validation
- repeated instances sharing one owned part
- project JSON roundtrip preserving placement/state and shared ownership

Targeted command:

```bash
./build/dev/blcad_core_tests "[core][component-instance]"
```

Constraint, graph, target-resolution, transform-evaluation, and equation-builder tests are intentionally separate.

## Deliberate limitations

This component block itself does not implement:

- solver output
- remaining degrees of freedom
- enforced grounding
- suppression effects on assembly consumers
- collision checks
- subassemblies
- assembly geometry instancing
- assembly-level STEP export

Persistent constraint records, graph connectivity, generated-face target resolution, rigid-transform evaluation, and planar Mate/Distance residual construction are implemented as separate layers.

## Current downstream boundary

The next repository-wide assembly increment is a first rigid-body solver seed.

That solver should define grounded components as fixed participants for its first scope, specify variable transform representation, residual weighting, convergence/failure behavior, and return proposed transforms before any explicit application step.

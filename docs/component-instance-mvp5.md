# Component Instances and Explicit Placement MVP-5

Status: implemented persistent component occurrence, placement, visibility, suppression, and grounding records plus explicit update APIs. The shared Mate/Distance/Concentric solver consumes these records and may propose transforms; a separate applier owns the successful-result mutation boundary. Insert target/residual semantics are implemented downstream without adding component fields.

## Goal

A project-owned part may appear several times in one assembly without duplicating `PartDocument` intent.

Each occurrence needs stable identity and independent placement/state.

## Component identity

`ComponentInstanceId` identifies an assembly occurrence independently from `DocumentId`.

```text
part.bolt
  -> component.bolt.1
  -> component.bolt.2
  -> component.bolt.3
```

Occurrences may reference the same project-owned part while retaining independent transforms/state.

## Component record

```text
ComponentInstance
  id
  name
  referenced_part_document
  visibility
  suppression_state
  grounding_state
  transform
```

State enums:

```text
ComponentVisibility
  Visible
  Hidden

ComponentSuppressionState
  Active
  Suppressed

ComponentGroundingState
  Free
  Grounded
```

Placement:

```text
RigidTransform
  translation_mm
  rotation_deg
```

Translation is millimeters and rotation is degrees. All transform components must be finite.

## Transform semantics

`AssemblyTransformEvaluator` interprets persisted placement as:

```text
active right-handed fixed-axis rotations
X -> Y -> Z
Rz * Ry * Rx for column vectors
```

Points/origins rotate and translate. Vectors, normals, and semantic axis directions rotate only.

Insert reuses the same component transform for both its local semantic axis and local seating plane. No Insert-specific placement field or transform convention exists.

## Ownership and validation

`AssemblyDocument` owns component instances by value.

A component may reference only a registered assembly member part.

`Project::validate_component_instances()` additionally requires every component reference to resolve to a project-owned `PartDocument`.

Occurrences do not duplicate part parameters, sketches, features, or shape-cache ownership.

## Explicit update APIs

`AssemblyDocument` exposes explicit updates for:

- transform
- visibility
- suppression state
- grounding state

Copy-style `ComponentInstance::with_*` operations preserve stable component identity and referenced part identity.

Invalid ids/transforms are rejected before the stored record is replaced. Failed updates preserve previous state.

## Storage versus solver grounding semantics

The storage layer permits explicit direct transform updates even when a component is grounded.

This keeps model editing separate from solver policy.

The solver interprets:

```text
Grounded -> fixed solve participant
Free     -> variable solve participant
```

At least one selected group component must be grounded. Multiple grounded components remain fixed.

Solver snapshots include every group component's source transform and state, so moving a grounded anchor invalidates an old solve result.

Current shared solving applies to Mate, Distance, and Concentric.

Insert will reuse the same fixed/free policy after numeric integration.

## Suppression and visibility

Visibility does not affect current solving.

A selected graph group containing a suppressed component is rejected instead of silently removing it or its relationships.

This is solver policy, not storage mutation policy.

A future suppression-aware solve path must define graph and relationship participation together.

## Downstream consumers

```text
ComponentInstance
  -> AssemblyConstraintGraph
  -> semantic target resolver
  -> AssemblyTransformEvaluator
  -> geometry-family residual builder
  -> optional shared numeric residual/Jacobian system
  -> AssemblyRigidBodySolver
  -> AssemblySolveResult
  -> AssemblySolveResultApplier
  -> AssemblySolveDiagnosticsAnalyzer
```

Planar branch:

```text
AssemblyConstraintEquationBuilder
  -> Mate/Distance residuals
  -> shared numeric path
```

Axis branch:

```text
resolve_axis
  -> evaluate_axis
  -> AssemblyConcentricConstraintEquationBuilder
  -> Concentric residual
  -> shared numeric path
```

Insert branch:

```text
resolve_insert
  -> local primary axis + local seating plane + component transform
  -> evaluate_axis + evaluate_plane
  -> AssemblyInsertConstraintEquationBuilder
  -> Insert residual
```

Insert currently stops before the shared numeric path.

Responsibilities remain separate:

- graph: active relationship connectivity
- resolver: component/part semantic lookup and local geometry
- evaluator: exact local-to-assembly mapping
- residual builders: geometry-family semantics
- numeric system: scalar residual/variable/Jacobian interpretation
- solver: fixed/variable participation and solve on project copies
- applier: fresh-converged-result mutation boundary
- diagnostics: local rank and remaining DOF over the shared numeric system

No downstream descriptor becomes a `ComponentInstance` field.

## Relationship freedoms do not become component state

Regular Concentric leaves axial translation and common-axis rotation free. Shared diagnostics observe two null directions and two remaining DOF.

Regular Insert adds axial seating and leaves only common-axis rotation free. A direct seven-scalar residual Jacobian proves rank `5/6`.

These freedoms are not persisted as flags such as:

```text
can_slide
can_rotate
insert_locked
```

A future null-space or per-component presentation layer may derive richer labels without changing the component record.

## JSON representation

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

Assembly/project roundtrip preserves placement/state.

Graph connectivity, resolved plane/axis/seat targets, assembly-space geometry, residuals, numeric vectors, Jacobians, solve results/proposals, rank, and DOF diagnostics are not component persistence fields.

After a fresh converged result is explicitly applied, later serialization stores the updated existing `transform` field exactly like any valid direct placement edit.

The format does not persist transform provenance as manual versus solver-applied or constraint-family specific.

## Headless inspection

```text
blcad_inspect_project_components <input.blcad.project.json>
```

The inspector reports component placement/state, stored constraints, and a derived graph-group summary.

It is read-only and does not run the solver or resolve semantic target geometry.

## Tests

```bash
./build/dev/blcad_core_tests "[core][component-instance]"
./build/dev/blcad_core_tests "[core][assembly-insert]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-diagnostics]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-insert]"
```

These suites separately cover component storage/update behavior, solver participation/application, local rank/DOF, Concentric transform use, Insert composite endpoint transform use, and read-only Insert residual construction.

## Component-record limitations

The component record layer itself does not infer constraints from placement, resolve semantic geometry, optimize transforms, compute DOF/rank, persist null-space directions, block explicit grounded edits, remove suppressed components from graph connectivity, instantiate assembly geometry, or export assembly STEP.

Those responsibilities belong downstream.

## Current downstream boundary

Mate, Distance, and Concentric solving plus shared local Jacobian-rank/remaining-DOF diagnostics are implemented without changing `ComponentInstance`.

Insert relationship intent, composite `.seat` target resolution, and read-only residual semantics are also implemented without adding any component field.

The next block integrates Insert into the existing shared numeric/solver/application/diagnostics pipeline. `ComponentInstance` still requires no new field.

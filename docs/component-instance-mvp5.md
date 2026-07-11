# Component Instances and Explicit Placement MVP-5

Status: implemented persistent component occurrence, placement, visibility, suppression, and grounding records plus explicit state-update APIs. The rigid-body solver consumes these records and may propose new transforms, while a separate applier owns the explicit successful-result mutation boundary.

Downstream local DOF diagnostics, semantic generated-axis resolution, and read-only Concentric residual construction are also implemented without adding fields to `ComponentInstance`.

## Goal

A project-owned part model may appear several times in one assembly without duplicating `PartDocument` model intent.

Each occurrence needs stable identity and independent placement/state.

## Component identity

`ComponentInstanceId` identifies an assembly occurrence independently from `DocumentId`.

Example:

```text
part.bolt
  -> component.bolt.1
  -> component.bolt.2
  -> component.bolt.3
```

All occurrences may reference the same project-owned part while retaining independent transforms and state.

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

Translation components are millimeters and rotation components are degrees. All transform components must be finite.

## Transform semantics

`AssemblyTransformEvaluator` interprets the persisted transform using:

```text
active right-handed fixed-axis rotations
X -> Y -> Z
Rz * Ry * Rx for column vectors
```

Points and geometry origins rotate and translate. Vectors, planar normals, and semantic axis directions rotate only.

The exact convention is documented in `docs/assembly-rigid-transform-evaluation-mvp5.md`.

## Ownership and validation

`AssemblyDocument` owns component instances by value.

A component may reference only a registered assembly member part.

`Project::validate_component_instances()` additionally requires every component reference to resolve to a project-owned `PartDocument`.

Several component instances may reference one owned part. Component occurrences do not duplicate part parameters, sketches, features, or shape-cache ownership.

## Explicit update APIs

`AssemblyDocument` exposes explicit component updates for:

- transform
- visibility
- suppression state
- grounding state

Copy-style `ComponentInstance::with_*` operations preserve stable component identity and `referenced_part_document`.

The shared assembly update path rejects empty/unknown ids and invalid transforms before replacing the stored record.

Failed updates preserve the previous component record.

## Storage-layer versus solver grounding semantics

The component storage layer permits an explicit direct transform update even when a component is grounded.

This keeps explicit model edits separate from solver behavior.

The rigid-body solver interprets:

```text
Grounded -> fixed solve participant
Free     -> variable solve participant
```

At least one component in a selected connected group must be grounded. Multiple grounded components are allowed and all remain fixed during solve.

Changing a grounded anchor after a solve invalidates the old solve result because solver snapshots include every group component's source transform and state.

## Suppression and visibility

Visibility does not affect the current solver.

The current solver rejects a selected graph group containing a suppressed component instead of silently removing it or its constraints.

This is a solver policy, not a storage-layer mutation rule.

A later suppression-aware graph/solve path must define connectivity and constraint participation together.

## Downstream consumers

The assembly path consumes component records through separate layers:

```text
ComponentInstance
  -> AssemblyConstraintGraph
  -> semantic target resolver
  -> AssemblyTransformEvaluator
  -> planar or axis assembly-space descriptor
  -> residual builder
  -> optional numeric solve/DOF path
```

Implemented planar path:

```text
AssemblyConstraintEquationBuilder
  -> Mate/Distance residuals
  -> shared numeric system
  -> AssemblyRigidBodySolver
  -> AssemblySolveResult
  -> AssemblySolveResultApplier
  -> AssemblySolveDiagnosticsAnalyzer
```

Implemented semantic axis/Concentric path:

```text
AssemblyConstraintTargetResolver::resolve_axis
  -> AssemblyTransformEvaluator::evaluate_axis
  -> AssemblyConcentricConstraintEquationBuilder
  -> ConcentricResidualDescriptor
```

Concentric numeric-system/solver/DOF integration is the next block.

Responsibilities remain separate:

- graph: active relationship connectivity
- target resolver: component/part semantic lookup and component-local geometry
- transform evaluator: exact local-to-assembly mapping
- residual builders: constraint-family geometry semantics
- numeric system: deterministic scalar residual/variable/Jacobian interpretation
- rigid-body solver: fixed/variable participation and numeric solve on project copies
- result applier: fresh-converged-result mutation boundary
- diagnostics: local rank and remaining DOF over the exact shared numeric system

No downstream descriptor becomes a new `ComponentInstance` field.

## JSON representation

Representative component JSON:

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

Assembly/project JSON roundtrip preserves component placement and state.

Graph connectivity, resolved planar/axis targets, assembly-space planes/axes, residuals, Jacobians, solve results, proposed transforms, rank values, and DOF diagnostics are not component persistence fields.

After a fresh converged result is explicitly applied, a later save serializes the updated existing `transform` field exactly like any other valid transform edit.

The file format does not persist transform provenance as manual versus solver-applied.

## Headless inspection

```text
blcad_inspect_project_components <input.blcad.project.json>
```

The inspector reports component placement/state, stored constraints, and a derived graph-group summary.

It is read-only and does not run the solver or resolve semantic axis geometry.

## Tests

```bash
./build/dev/blcad_core_tests "[core][component-instance]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-diagnostics]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric]"
```

These suites separately cover component storage/update behavior, solver participation/application boundaries, local rank/DOF behavior, and semantic axis/Concentric residual use of component transforms.

## Deliberate component-record limitations

The component record layer itself does not:

- infer assembly constraints from placement
- resolve semantic geometry
- run numeric optimization
- compute remaining DOF
- classify constraint rank
- automatically block direct edits while grounded
- automatically remove suppressed components from graph connectivity
- instantiate assembly geometry
- export assembly STEP

Those responsibilities belong to separate downstream layers.

## Current downstream boundary

Local Jacobian-rank/remaining-DOF diagnostics and read-only semantic-axis/Concentric residual construction are implemented downstream without changing `ComponentInstance`.

The next repository-wide assembly block is Concentric integration into the shared numeric residual/Jacobian system, rigid-body solver, and DOF diagnostics. No new component persistence field is required.

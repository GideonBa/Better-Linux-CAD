# Component Instances and Explicit Placement MVP-5

Status: implemented persistent component occurrence, placement, visibility, suppression, and grounding records plus explicit state-update APIs. The first rigid-body solver now consumes these records and may propose new transforms, while a separate applier owns the explicit successful-result mutation boundary.

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

All occurrences may reference the same project-owned part while retaining independent transforms and component state.

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

State enums are:

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

Placement is:

```text
RigidTransform
  translation_mm
  rotation_deg
```

Translation components are millimeters and rotation components are degrees.

All transform components must be finite.

## Transform semantics

The persisted transform record is interpreted by `AssemblyTransformEvaluator` using the canonical active right-handed fixed-axis rotation order:

```text
X -> Y -> Z
Rz * Ry * Rx for column vectors
```

Points are rotated and translated. Vectors, axes, and normals are rotated only.

The exact convention is documented in `docs/assembly-rigid-transform-evaluation-mvp5.md`.

## Ownership and validation

`AssemblyDocument` owns component instances by value.

A component may reference only a registered assembly member part.

`Project::validate_component_instances()` additionally requires every component reference to resolve to a project-owned `PartDocument`.

Several component instances may reference one owned part document. Component occurrences do not duplicate part parameters, sketches, features, or shape-cache ownership.

## Explicit update APIs

`AssemblyDocument` exposes explicit component updates for:

- transform
- visibility
- suppression state
- grounding state

Copy-style `ComponentInstance::with_*` operations preserve stable component identity and `referenced_part_document`.

The shared assembly update path rejects empty/unknown ids and invalid transforms before replacing the stored record.

Failed updates preserve the previously stored component record.

## Storage-layer versus solver grounding semantics

The component storage layer deliberately permits an explicit direct transform update even when a component is marked grounded.

This allows explicit model edits and keeps record semantics independent from solver behavior.

The first rigid-body solver is the first consumer that enforces grounding as solve policy:

```text
Grounded -> fixed solve participant
Free     -> variable solve participant
```

At least one component in a selected connected group must be grounded. Multiple grounded components are allowed and all remain fixed during solve.

Changing a grounded anchor after a solve invalidates the old solve result because solver snapshots include every group component's source transform and state.

## Suppression and visibility

The records persist both suppression and visibility.

Visibility does not affect the first rigid-body solver.

The first solver rejects a selected graph group containing a suppressed component instead of silently removing it or its constraints.

This is a solver policy, not a storage-layer mutation rule.

Future suppression-aware graph/solve participation must define connectivity and constraint participation together.

## Constraint and solver consumers

The current assembly path consumes component records through separate layers:

```text
ComponentInstance
  -> AssemblyConstraintGraph
  -> AssemblyConstraintTargetResolver
  -> AssemblyTransformEvaluator
  -> AssemblyConstraintEquationBuilder
  -> AssemblyRigidBodySolver
  -> AssemblySolveResult
  -> AssemblySolveResultApplier
```

Responsibilities remain separate:

- graph: active relationship connectivity
- target resolver: component/part semantic lookup and local plane construction
- transform evaluator: exact local-to-assembly coordinate mapping
- equation builder: planar Mate/Distance residual semantics
- rigid-body solver: fixed/variable participation and numeric solve on a private project copy
- result applier: explicit fresh-converged-result transform mutation boundary

The solver does not mutate the source project.

A successful apply updates the existing component transform records through the normal assembly transform update path.

No separate solved-transform record is added to `ComponentInstance`.

## JSON representation

Assembly JSON retains the historical compatibility marker:

```text
blcad.assembly_document.mvp4
version 1
```

The marker is historical. MVP-5 component instances extended the schema additively.

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

Graph, resolved targets, assembly-space frames, residuals, Jacobians, `AssemblySolveResult`, and proposed transforms are not component persistence fields.

After a fresh converged solve result is explicitly applied, a later save serializes the updated existing `transform` field exactly like any other valid transform edit.

The file format does not persist transform provenance as "manual" versus "solver-applied".

## Headless inspection

```text
blcad_inspect_project_components <input.blcad.project.json>
```

The current inspector reports:

- referenced part document
- visibility
- suppression state
- grounding state
- translation
- rotation
- stored constraints
- derived graph-group summary

The tool is read-only and does not run the solver.

## Tests

Component-instance core tests cover:

- required identity fields
- initial transform storage
- non-finite transform rejection
- member-part reference validation
- unique component ids
- explicit transform/state updates
- empty and unknown update ids
- failed update immutability
- direct transform edits while grounded
- identity and part-reference preservation across updates
- assembly/project JSON roundtrip
- project-owned part validation
- repeated occurrences sharing one part

Targeted command:

```bash
./build/dev/blcad_core_tests "[core][component-instance]"
```

The rigid-body solver suite separately covers grounded/fixed participation, suppression rejection, source-project immutability, stale component snapshots, and explicit transform application:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-solver]"
```

## Deliberate component-record limitations

The component record layer itself does not:

- infer assembly constraints from placement
- run numeric optimization
- compute remaining degrees of freedom
- classify under/fully/overconstrained state
- automatically block direct edits while grounded
- automatically remove suppressed components from graph connectivity
- instantiate assembly geometry
- export assembly STEP

Persistent constraint records, graph connectivity, target resolution, transform evaluation, planar residual construction, rigid-body solving, and explicit successful-result application are implemented as separate layers.

## Current downstream boundary

The repository-wide next assembly block is read-only Jacobian-rank and remaining-degree-of-freedom diagnostics.

That diagnostics layer will consume the solver's deterministic variable ordering and local numeric model without adding persistent DOF fields to `ComponentInstance`.

# Assembly System with Constraints

Status: component instances, explicit placement/state updates, Mate/Concentric/Distance/Insert intent records, deterministic active-constraint connectivity, generated planar-face/circular-axis/circular-seat target resolution, explicit rigid-transform evaluation, Mate/Distance/Concentric/Insert residual construction, Mate/Distance/Concentric numeric solving, explicit atomic result application, and read-only local Jacobian-rank/remaining-DOF diagnostics are implemented.

Insert numeric-system, solver, application, and shared DOF integration is the next assembly block.

## Core model

A project owns one assembly and project-owned part documents. An assembly contains component occurrences referencing those parts.

```text
PartDocument Screw_M6x25
AssemblyDocument HousingAssembly
  ComponentInstance Screw_01 -> Screw_M6x25
  ComponentInstance Screw_02 -> Screw_M6x25
```

One part model may be reused by several component occurrences.

Persistent records store model intent. Graphs, resolved geometry, residuals, Jacobians, solve results, and DOF diagnostics remain derived unless a fresh converged transform proposal is explicitly applied.

## Current assembly pipeline

```text
persistent ComponentInstance + AssemblyConstraint intent
  -> AssemblyConstraintGraph
  -> deterministic graph-ordered active constraint ids

planar face target
  -> AssemblyConstraintTargetResolver::resolve
  -> component-local plane
  -> AssemblyTransformEvaluator::evaluate_plane
  -> AssemblyConstraintEquationBuilder
  -> Mate / Distance residual

axis target
  -> AssemblyConstraintTargetResolver::resolve_axis
  -> component-local axis
  -> AssemblyTransformEvaluator::evaluate_axis
  -> AssemblyConcentricConstraintEquationBuilder
  -> Concentric residual

circular seat target
  -> AssemblyConstraintTargetResolver::resolve_insert
  -> component-local primary axis + oriented seating plane
  -> evaluate_axis + evaluate_plane
  -> AssemblyInsertConstraintEquationBuilder
  -> composite Insert residual

Mate / Distance / Concentric residual
  -> shared numeric residual/Jacobian system
  -> AssemblyRigidBodySolver on Project copies
  -> AssemblySolveResult
  -> explicit AssemblySolveResultApplier
  -> AssemblySolveDiagnosticsAnalyzer

Insert residual
  -> next: shared numeric / solver / application / diagnostics integration
```

Geometry-family interpretation is explicit. The current numeric solver remains one common path for every integrated family.

## Component instances and placement

Canonical document: `docs/component-instance-mvp5.md`.

Each `ComponentInstance` stores occurrence identity, referenced part, visibility, suppression, grounding, and:

```text
RigidTransform
  translation_mm
  rotation_deg
```

Direct placement/state edits are explicit storage operations. They do not infer constraints or run the solver.

## Assembly constraint intent

Canonical document: `docs/assembly-constraint-model-intent-mvp5.md`.

```text
AssemblyConstraintTarget
  component_instance
  semantic_reference

AssemblyConstraint
  id
  name
  type = mate | concentric | distance | insert
  target_a
  target_b
  state = active | inactive
  distance = optional positive length
```

Distance is the only type that may carry `distance`.

Mate, Concentric, and Insert are distance-free persistent relationship intent.

The record layer keeps target semantic strings opaque. It does not resolve geometry or solve placement.

Target A/B order is persistent intent and is never normalized away.

## Active-constraint connectivity

`AssemblyConstraintGraph` is a deterministic read-only relationship graph:

```text
nodes = every ComponentInstanceId
edges = active AssemblyConstraintId records
```

Inactive constraints do not affect connectivity. Multi-edges are legal. Node, edge, adjacency, and connected-group order is lexicographic.

The graph does not distinguish planar, axis, or composite seat geometry. An active Insert record is the same relationship-edge form as any other active constraint.

Exact connected groups are current rigid-body solve partition boundaries.

## Semantic planar face targets

Implemented family:

```text
feature.<feature-id>.top
feature.<feature-id>.bottom
feature.<feature-id>.right
feature.<feature-id>.left
feature.<feature-id>.front
feature.<feature-id>.back
```

For supported `AdditiveExtrude` features, `resolve` reuses `WorkplaneResolver::resolve_generated_face` and returns a component-local plane plus separate persisted component transform.

## Semantic generated-axis targets

Canonical document: `docs/assembly-semantic-axis-concentric-residuals-mvp5.md`.

First axis token:

```text
feature.<feature-id>.axis
```

The first producer is one `SubtractiveExtrude` whose source sketch contains exactly one `CircleProfile` and exactly one total profile.

The axis is derived from constructive intent:

```text
origin = mapped CircleProfile.center
direction = source workplane normal adjusted by ExtrudeDirection
```

Exact source feature/profile identity is preserved.

## Semantic seating targets and composite Insert endpoints

Canonical document: `docs/assembly-insert-intent-composite-residuals-mvp5.md`.

First seating token:

```text
feature.<feature-id>.seat
```

The first producer uses the same intentionally narrow single-circle `SubtractiveExtrude` family as `.axis`.

`AssemblyConstraintTargetResolver::resolve_insert` derives:

```text
ResolvedAssemblyInsertConstraintTarget
  component_instance
  referenced_part_document
  source_feature
  source_profile
  axis = Primary
  seating_plane = Primary
  local_axis
  local_seating_plane
  component_transform
```

The local axis and seat come from the same exact feature/profile identity.

```text
axis.origin = mapped CircleProfile.center
axis.direction = extrude direction
seat.origin = axis.origin
seat.normal = axis.direction
```

For `OppositeSketchNormal`, the seat Y basis and normal are both reversed so the local seat frame remains right-handed.

`CircularHolePattern` is not assigned one ambiguous `.axis` or `.seat` token because it produces several distinct holes. Stable per-instance pattern identity remains future work.

## Rigid-transform evaluation

`RigidTransform` uses:

```text
translation_mm: millimeters
rotation_deg: degrees
active right-handed fixed-axis rotations
apply X, then Y, then Z
R = Rz * Ry * Rx for column vectors
```

Evaluation rules:

```text
point/origin   -> rotate, then translate
vector         -> rotate only
normal         -> rotate only
axis direction -> rotate only
```

Insert reuses `evaluate_axis` for its primary axis and `evaluate_plane` for its seating plane.

A focused test proves that local seat/axis origin `(4,-6,0)` under translation `(10,20,30)` and X rotation `90 deg` evaluates to `(14,20,24)`, while local +Z axis/seat normal evaluates to `(0,-1,0)`.

## Mate residual

```text
normal_opposition    = nA + nB
signed_separation_mm = dot(oB - oA, nA)
```

Tangential plane-origin separation is intentionally absent.

## Distance residual

```text
normal_parallelism   = cross(nA, nB)
signed_separation_mm = dot(oB - oA, nA)
distance_residual_mm = signed_separation_mm - target_distance_mm
```

Equal or opposed parallel normals are accepted. Target A defines signed separation.

## Concentric residual

```text
direction_parallelism = cross(dA, dB)
axis_offset_mm         = cross(oB - oA, dA)
```

Equal and opposed directions are accepted. Axial origin separation is absent, so Concentric leaves axial translation and rotation about the common axis free.

Regular one-free-body Concentric rank is `4/6` with two remaining local DOF.

## Composite Insert residual

The dedicated read-only builder is:

```text
AssemblyInsertConstraintEquationBuilder
```

One `.seat` endpoint is resolved and evaluated to:

```text
AssemblySpaceInsertConstraintTargetDescriptor
  component_instance
  semantic_reference
  source_feature
  source_profile
  axis
  seating_plane
```

For target axis lines `(oA,dA)`, `(oB,dB)` and seating planes with origins `sA`, `sB` and target-A normal `nA`:

```text
direction_parallelism       = cross(dA, dB)
axis_offset_mm               = cross(oB - oA, dA)
signed_seating_separation_mm = dot(sB - sA, nA)
```

The seat normal is canonically aligned with the same endpoint's semantic axis direction.

The first two fields reuse stable Concentric axis-line semantics. The third removes Concentric's axial freedom by seating the target planes.

Target A defines the signed seating direction.

No separate seat-normal vector is included because axis-direction parallelism already constrains the two regular tilt directions.

Insert therefore exposes seven scalar residual components:

```text
3 direction components
3 axis-offset components
1 seating-separation component
```

A direct central finite-difference `7 x 6` Jacobian test proves regular rank `5`, leaving only rotation about the common axis free.

The rank is measured from residual sensitivity, not hard-coded from the Insert type.

## Shared numeric residual/Jacobian system

The private shared numeric implementation is used by solver and diagnostics.

Current numeric families:

```text
Mate
Distance
Concentric
```

Constraint records use lexicographic graph order. Each free component contributes:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

Current scalar flattening:

```text
Mate:
  normal_opposition.x
  normal_opposition.y
  normal_opposition.z
  signed_separation_mm / length_residual_scale_mm

Distance:
  normal_parallelism.x
  normal_parallelism.y
  normal_parallelism.z
  distance_residual_mm / length_residual_scale_mm

Concentric:
  direction_parallelism.x
  direction_parallelism.y
  direction_parallelism.z
  axis_offset_mm.x / length_residual_scale_mm
  axis_offset_mm.y / length_residual_scale_mm
  axis_offset_mm.z / length_residual_scale_mm
```

Every finite-difference perturbation re-resolves semantic geometry and reconstructs residuals from current project intent.

Insert is not yet flattened in this shared path.

## Rigid-body solver

Participation policy:

```text
Grounded   -> fixed participant
Free       -> six numeric variables
Suppressed -> selected group rejected
Visibility -> no participation effect
```

At least one grounded component is required. Multiple grounded components may remain fixed.

The current solver uses deterministic damped Gauss-Newton:

```text
evaluate r
  -> central finite-difference J
  -> J^T J + lambda I
  -> -J^T r
  -> partial-pivot Gaussian elimination
  -> deterministic backtracking line search
  -> damping escalation
```

Mate, Distance, and Concentric participate through the same residual evaluator.

Insert currently fails explicitly before numeric solving through the dedicated composite-target boundary. It is never silently ignored and the source project remains unchanged.

## Explicit result application

`AssemblySolveResult` contains complete group component snapshots and proposed transforms for free components.

`AssemblySolveResultApplier`:

- accepts only converged results
- verifies complete source snapshots
- rejects changed transforms, grounding, or suppression state
- detects moved grounded anchors
- validates all proposals
- applies to a private project copy
- replaces the caller project only after every update succeeds

No solve result is persisted.

## Local Jacobian-rank and remaining-DOF diagnostics

For a converged integrated numeric state:

```text
variable_count  = 6 * free_component_count
constrained_dof = rank(J)
remaining_dof   = variable_count - rank(J)
```

Rank uses:

```text
max(rank_absolute_tolerance,
    rank_relative_tolerance * maximum_abs_jacobian_entry)
```

DOF, consistency, and residual-row rank structure are separate classifications.

Concentric rank is already evaluated through this shared analyzer.

Insert rank is currently proven by a focused direct residual/Jacobian test because Insert is not yet a shared numeric family. The next integration block must prove the same rank `5/6` and one remaining DOF through `AssemblySolveDiagnosticsAnalyzer`.

## Grounding, visibility, and suppression

Grounding is persisted model intent. Direct storage-level transform updates remain legal while grounded; solver policy interprets grounded components as fixed.

Visibility does not affect current solving.

Suppressed selected components are rejected rather than silently removed because suppression-aware graph and constraint participation has not yet been defined together.

## Persistence boundary

Assembly/project JSON persists:

- component identity and referenced part documents
- visibility, suppression, grounding
- `RigidTransform`
- Mate/Concentric/Distance/Insert constraint records
- target A/B semantic-reference strings

The following are derived and unpersisted:

- graph connectivity
- local plane/axis/seat descriptors
- assembly-space plane/axis/seat geometry
- Mate/Distance/Concentric/Insert residual descriptors
- flattened numeric residuals
- numeric Jacobians
- normal equations and damping state
- solve results and proposals
- rank/DOF diagnostics

`feature.hole.seat` uses the existing semantic-reference field. Insert uses the existing type string field. No new persistence field is introduced.

Only explicit application of a fresh converged solve result changes the existing persistent component transform.

## Next technical step

The next assembly block is **Insert integration into the shared numeric residual/Jacobian system, rigid-body solver, explicit result application, and local remaining-DOF diagnostics**.

Exact Insert flattening must be:

```text
direction_parallelism.x
direction_parallelism.y
direction_parallelism.z
axis_offset_mm.x / length_residual_scale_mm
axis_offset_mm.y / length_residual_scale_mm
axis_offset_mm.z / length_residual_scale_mm
signed_seating_separation_mm / length_residual_scale_mm
```

The integration must preserve all current deterministic ordering, finite-difference, damping, line-search, solve-state, grounding, suppression, snapshot, stale-result, and atomic-application contracts. It must solve lateral offset, tilt, and axial seating, preserve rotation about the common axis, and prove rank `5/6` with one remaining DOF through the exact shared diagnostics Jacobian.

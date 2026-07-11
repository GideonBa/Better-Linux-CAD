# Assembly System with Constraints

Status: component instances, explicit placement/state updates, Mate/Concentric/Distance intent records, deterministic active-constraint connectivity, generated planar-face and first generated-axis target resolution, explicit rigid-transform evaluation, planar Mate/Distance residual construction, read-only Concentric residual construction, deterministic planar rigid-body solving, explicit atomic result application, and read-only local Jacobian-rank/remaining-DOF diagnostics are implemented.

Concentric numeric-system, solver, and DOF integration is the next assembly block.

## Core model

A project owns one assembly and project-owned part documents. An assembly contains component occurrences referencing those parts.

```text
PartDocument Screw_M6x25
AssemblyDocument HousingAssembly
  ComponentInstance Screw_01 -> Screw_M6x25
  ComponentInstance Screw_02 -> Screw_M6x25
```

One part model may be reused by several component occurrences.

Persistent records store model intent. Graphs, resolved geometry, residuals, Jacobians, solve results, and DOF diagnostics remain derived unless an explicit successful solve proposal is applied to a persisted component transform.

## Current assembly pipeline

Planar solve path:

```text
persistent component and constraint intent
  -> AssemblyConstraintGraph
  -> AssemblyConstraintTargetResolver::resolve
  -> ComponentLocalPlanarDescriptor
  -> AssemblyTransformEvaluator::evaluate_plane
  -> AssemblySpacePlanarDescriptor
  -> AssemblyConstraintEquationBuilder
  -> planar Mate/Distance residuals
  -> shared deterministic numeric residual/Jacobian system
  -> AssemblyRigidBodySolver on Project copies
  -> AssemblySolveResult
  -> explicit AssemblySolveResultApplier
  -> AssemblySolveDiagnosticsAnalyzer
```

Concentric semantic path:

```text
Concentric target feature.<feature-id>.axis
  -> AssemblyConstraintTargetResolver::resolve_axis
  -> ComponentLocalAxisDescriptor
  -> AssemblyTransformEvaluator::evaluate_axis
  -> AssemblySpaceAxisDescriptor
  -> AssemblyConcentricConstraintEquationBuilder
  -> ConcentricResidualDescriptor
  -> next: shared numeric system / solver / DOF diagnostics
```

The layers are intentionally separate.

## Component instances and placement

Canonical document: `docs/component-instance-mvp5.md`.

`AssemblyDocument` owns `ComponentInstance` values. Each occurrence carries:

```text
ComponentInstanceId
name
referenced_part_document
visibility
suppression_state
grounding_state
RigidTransform
  translation_mm
  rotation_deg
```

Direct placement/state edits are explicit storage operations. They do not infer constraints or run a solver.

## Assembly constraint intent

Canonical document: `docs/assembly-constraint-model-intent-mvp5.md`.

```text
AssemblyConstraintTarget
  component_instance
  semantic_reference

AssemblyConstraint
  id
  name
  type = mate | concentric | distance
  target_a
  target_b
  state = active | inactive
  distance = optional positive length
```

The record layer validates identity and type-specific fields but keeps semantic target tokens opaque. It does not resolve geometry, evaluate transforms, construct residuals, solve, or compute DOF.

Target A/B order is persistent relationship intent and is never normalized away by the graph or geometry layers.

## Active-constraint connectivity

Canonical document: `docs/assembly-constraint-graph-mvp5.md`.

`AssemblyConstraintGraph` is a deterministic read-only relationship graph:

```text
nodes = every ComponentInstanceId
edges = active AssemblyConstraintId records
```

Inactive constraints do not affect connectivity. Multi-edges are legal. Nodes, edges, adjacency lists, and connected groups use lexicographic deterministic ordering.

The graph does not own semantic geometry or numeric equations.

Exact connected groups are the current rigid-body solve partition boundary.

## Semantic planar target resolution

Canonical document: `docs/assembly-constraint-target-resolution-mvp5.md`.

The implemented generated-face target family is:

```text
feature.<feature-id>.top
feature.<feature-id>.bottom
feature.<feature-id>.right
feature.<feature-id>.left
feature.<feature-id>.front
feature.<feature-id>.back
```

`AssemblyConstraintTargetResolver::resolve` performs:

```text
component occurrence
  -> project-owned PartDocument
  -> SemanticFaceReference
  -> supported AdditiveExtrude
  -> WorkplaneResolver::resolve_generated_face
  -> ComponentLocalPlanarDescriptor
  + separate component RigidTransform
```

The resolver does not apply component placement.

## Semantic generated-axis target resolution

Canonical document: `docs/assembly-semantic-axis-concentric-residuals-mvp5.md`.

The first implemented axis token is:

```text
feature.<feature-id>.axis
```

The first supported axis producer is a `SubtractiveExtrude` whose source sketch contains exactly one `CircleProfile` and exactly one total profile.

`AssemblyConstraintTargetResolver::resolve_axis` performs:

```text
component occurrence
  -> project-owned PartDocument
  -> SemanticAxisReference
  -> supported SubtractiveExtrude
  -> source Sketch
  -> exactly one CircleProfile
  -> source sketch workplane resolution
  -> CircleProfile.center mapped to the workplane
  -> direction from workplane normal and ExtrudeDirection
  -> ComponentLocalAxisDescriptor
  + separate component RigidTransform
```

The returned descriptor also preserves source feature and source profile identity.

This definition follows the existing circular through-all cut geometry path rather than querying OCCT topology.

`CircularHolePattern` is excluded from the single `.axis` family because one pattern creates multiple distinct axes. A later pattern-axis family needs stable per-instance semantic identity.

## Rigid-transform evaluation

Canonical document: `docs/assembly-rigid-transform-evaluation-mvp5.md`.

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
point/origin -> rotate, then translate
vector       -> rotate only
normal       -> rotate only
axis direction -> rotate only
```

The geometry layer exposes distinct component-local and assembly-space plane/axis descriptor types.

## Planar Mate residuals

For assembly-space planes `(oA,nA)` and `(oB,nB)`:

```text
normal_opposition    = nA + nB
signed_separation_mm = dot(oB - oA, nA)
```

A Mate aligns supporting planes and opposes normals. Tangential frame-origin separation is not a residual.

## Planar Distance residuals

```text
normal_parallelism   = cross(nA, nB)
signed_separation_mm = dot(oB - oA, nA)
distance_residual_mm = signed_separation_mm - target_distance_mm
```

Parallel equal or opposed normals are accepted. Signed separation uses target A's normal and is therefore target-order dependent.

The planar residual builder remains read-only and supports Mate/Distance only.

## Concentric residuals

The dedicated read-only builder is:

```text
AssemblyConcentricConstraintEquationBuilder
```

For axis lines `(oA,dA)` and `(oB,dB)`:

```text
direction_parallelism = cross(dA, dB)
axis_offset_mm         = cross(oB - oA, dA)
```

A satisfied Concentric relationship has both vectors equal to zero.

`cross(dA,dB) = 0` accepts equal and opposed axis directions.

`axis_offset_mm` ignores origin separation parallel to the axis. Therefore Concentric does not constrain axial translation.

For one regular free body relative to a fixed body, Concentric should locally constrain four independent directions:

```text
two axis-tilt rotations
two translations perpendicular to the axis
```

It should leave:

```text
translation along the axis
rotation about the axis
```

free.

This expected rank-four/two-remaining-DOF result is not yet part of the numeric analyzer because the shared numeric system still supports planar Mate/Distance only.

## Shared numeric residual/Jacobian system

The current shared numeric implementation is private to the geometry layer and is used by both solver and diagnostics.

Current supported numeric families:

```text
Mate
Distance
```

Each free component contributes six variables in lexicographic component-id order:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

Constraints are consumed in lexicographic `AssemblyConstraintId` order.

Current planar flattening:

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
```

Default length scale:

```text
1.0 mm
```

The central finite-difference Jacobian uses separate translation and rotation perturbation steps.

The next block extends this exact shared path with six Concentric scalar residual components without changing current Mate/Distance ordering.

## Rigid-body solver

Canonical document: `docs/assembly-rigid-body-solver-mvp5.md`.

Participation policy:

```text
Grounded  -> fixed participant
Free      -> six numeric variables
Suppressed -> selected group rejected
Visibility -> no solve participation effect
```

At least one grounded component is required. Multiple grounded components are allowed and remain fixed.

The first numeric method is deterministic damped Gauss-Newton:

```text
evaluate r
  -> central finite-difference J
  -> J^T J + lambda I
  -> -J^T r
  -> partial-pivot Gaussian elimination
  -> deterministic backtracking line search
  -> damping escalation
```

Solve states:

```text
Converged
MaximumIterationsReached
FixedGeometryInconsistent
NumericalFailure
```

The solver changes transforms only on private project copies.

It currently solves planar Mate/Distance groups. Active Concentric records still fail through the existing planar numeric path until the next integration block.

## Explicit result application

`AssemblySolveResult` contains complete group component snapshots and proposed transforms for free components.

`AssemblySolveResultApplier`:

- accepts only converged results
- verifies the complete component partition/snapshots
- rejects changed transforms, grounding, or suppression state
- detects moved grounded anchors
- validates all proposals
- applies to a private project copy
- replaces the caller project only after every update succeeds

No solver result is persisted.

## Local Jacobian-rank and remaining-DOF diagnostics

Canonical document: `docs/assembly-solve-diagnostics-mvp5.md`.

For a converged numeric solve state, diagnostics evaluate the same shared Jacobian on a private project copy.

```text
variable_count  = 6 * free_component_count
constrained_dof = rank(J)
remaining_dof   = variable_count - rank(J)
```

Rank uses a configurable threshold:

```text
max(rank_absolute_tolerance,
    rank_relative_tolerance * maximum_abs_jacobian_entry)
```

DOF classification, consistency classification, and residual-row rank structure are separate concepts.

Redundant scalar residual rows do not automatically imply semantic overconstraint.

The current diagnostic path supports the same Mate/Distance numeric system as the solver. Concentric rank diagnostics are next.

## Grounding

`ComponentGroundingState::Grounded` is persisted model intent.

Storage-level direct transform updates remain legal while grounded. This keeps explicit model edits separate from solver policy.

The current solver is the consumer that interprets grounded components as fixed participants.

A group with no grounded component is rejected instead of receiving a hidden floating-group gauge condition.

## Visibility and suppression

Visibility and suppression are persistent editable state.

Visibility does not affect the current solver.

Suppression does not currently rewrite graph connectivity or constraint participation, so the solver rejects a selected connected group containing a suppressed component rather than silently dropping it.

A later suppression-aware solve path must define graph and constraint participation together.

## Persistence boundary

The assembly/project format persists:

- component identity and referenced part documents
- visibility, suppression, grounding
- `RigidTransform`
- assembly constraint records and target semantic-reference strings

The following are derived and unpersisted:

- graph connectivity
- component-local plane and axis descriptors
- assembly-space planes and axes
- Mate/Distance/Concentric residual descriptors
- flattened residual vectors
- numeric Jacobians
- normal equations and damping state
- solve results and proposals
- rank and DOF diagnostics

`feature.hole.axis` requires no new JSON field; it is stored in the existing semantic-reference string field.

Only explicit application of a fresh converged solver result changes the existing persisted component transform.

## Motion, joints, and limits

Underdefined components should eventually be draggable only within allowed DOF. A drag should become solver input projected into valid constraint space.

Future joint families include Revolute, Prismatic, Cylindrical, Planar, Ball, Rigid, Gear, and Screw relations.

These remain downstream of a stable rigid constraint/DOF model.

## Next technical step

The next assembly block is explicit Concentric integration into the shared numeric residual/Jacobian path, rigid-body solver, and DOF analyzer.

Canonical intended scalar order:

```text
direction_parallelism.x
direction_parallelism.y
direction_parallelism.z
axis_offset_mm.x / length_residual_scale_mm
axis_offset_mm.y / length_residual_scale_mm
axis_offset_mm.z / length_residual_scale_mm
```

The block must preserve all existing deterministic component/constraint order, variable order, finite-difference semantics, grounding/suppression policy, solve states, read-only solve boundary, component snapshots, stale-result detection, and atomic application rules.

It should prove one regular Concentric relationship between a grounded body and one free body has local Jacobian rank four and two remaining DOF.

Insert remains downstream because it combines axis alignment with explicit axial seating semantics.

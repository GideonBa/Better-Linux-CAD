# Assembly System with Constraints

Status: component instances, explicit placement/state updates, Mate/Concentric/Distance intent records, deterministic active-constraint connectivity, generated planar-face and generated-axis target resolution, explicit rigid-transform evaluation, Mate/Distance/Concentric residual construction, one shared numeric residual/Jacobian system, deterministic rigid-body solving, explicit atomic result application, and read-only local Jacobian-rank/remaining-DOF diagnostics are implemented.

Stable Insert intent and read-only composite Insert residual semantics are the next assembly block.

## Core model

A project owns one assembly and project-owned part documents. An assembly contains component occurrences referencing those parts.

```text
PartDocument Screw_M6x25
AssemblyDocument HousingAssembly
  ComponentInstance Screw_01 -> Screw_M6x25
  ComponentInstance Screw_02 -> Screw_M6x25
```

One part model may therefore be reused by several component occurrences.

Persistent records store model intent. Graphs, resolved geometry, residuals, Jacobians, solve results, and DOF diagnostics remain derived unless an explicit successful solve proposal is applied to a persisted component transform.

## Current assembly pipeline

```text
persistent component and constraint intent
  -> AssemblyConstraintGraph
  -> deterministic graph-ordered active constraints

planar target
  -> AssemblyConstraintTargetResolver::resolve
  -> ComponentLocalPlanarDescriptor
  -> AssemblyTransformEvaluator::evaluate_plane
  -> AssemblySpacePlanarDescriptor
  -> AssemblyConstraintEquationBuilder
  -> Mate / Distance residual descriptor

axis target
  -> AssemblyConstraintTargetResolver::resolve_axis
  -> ComponentLocalAxisDescriptor
  -> AssemblyTransformEvaluator::evaluate_axis
  -> AssemblySpaceAxisDescriptor
  -> AssemblyConcentricConstraintEquationBuilder
  -> Concentric residual descriptor

supported residual descriptor
  -> one shared numeric residual/Jacobian system
  -> AssemblyRigidBodySolver on Project copies
  -> AssemblySolveResult
  -> explicit AssemblySolveResultApplier
  -> AssemblySolveDiagnosticsAnalyzer
```

Geometry-family interpretation is explicit before all supported constraint families rejoin one numeric path.

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

The record layer validates identity and type-specific fields but keeps semantic target strings opaque.

Target A/B order is persistent relationship intent and is never normalized away.

## Active-constraint connectivity

Canonical document: `docs/assembly-constraint-graph-mvp5.md`.

`AssemblyConstraintGraph` is a deterministic read-only relationship graph:

```text
nodes = every ComponentInstanceId
edges = active AssemblyConstraintId records
```

Inactive constraints do not affect connectivity. Multi-edges are legal. Nodes, edges, adjacency lists, and connected groups use lexicographic deterministic ordering.

The graph does not own semantic geometry or numeric equations.

Exact connected groups are rigid-body solve partition boundaries.

## Semantic planar target resolution

Canonical document: `docs/assembly-constraint-target-resolution-mvp5.md`.

Implemented generated-face family:

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

First implemented axis token:

```text
feature.<feature-id>.axis
```

The first supported producer is a `SubtractiveExtrude` whose source sketch contains exactly one `CircleProfile` and exactly one total profile.

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

The descriptor preserves source feature and source profile identity.

The definition follows existing circular through-all cut model intent rather than querying OCCT topology.

`CircularHolePattern` remains excluded from one ambiguous `.axis` token because one pattern produces several distinct axes.

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
point/origin    -> rotate, then translate
vector          -> rotate only
normal          -> rotate only
axis direction  -> rotate only
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

Parallel equal or opposed normals are accepted. Signed separation uses target A's normal and is target-order dependent.

The planar residual builder remains Mate/Distance-specific.

## Concentric residuals

Canonical documents:

- `docs/assembly-semantic-axis-concentric-residuals-mvp5.md`
- `docs/assembly-concentric-numeric-solver-dof-mvp5.md`

The dedicated geometric builder is:

```text
AssemblyConcentricConstraintEquationBuilder
```

For axis lines `(oA,dA)` and `(oB,dB)`:

```text
direction_parallelism = cross(dA, dB)
axis_offset_mm         = cross(oB - oA, dA)
```

A satisfied Concentric relationship has both vectors equal to zero.

`cross(dA,dB) = 0` accepts equal and opposed directions.

`axis_offset_mm` ignores origin separation parallel to target A's axis. Therefore Concentric does not constrain axial translation.

A regular Concentric relationship between one grounded and one free body constrains four independent local directions:

```text
two axis-tilt rotational directions
two translations perpendicular to the common axis
```

It leaves:

```text
translation along the common axis
rotation about the common axis
```

free.

## Shared numeric residual/Jacobian system

The shared numeric implementation is private to the geometry layer:

```text
src/geometry/assembly_constraint_numeric_system.hpp
src/geometry/assembly_constraint_numeric_system.cpp
```

It is used by both solver and diagnostics.

Supported numeric families:

```text
Mate
Distance
Concentric
```

The numeric evaluator selects:

```text
Concentric
  -> AssemblyConcentricConstraintEquationBuilder

Mate / Distance
  -> AssemblyConstraintEquationBuilder
```

Constraint order is lexicographic `AssemblyConstraintId` order from the graph. Persistent target A/B order remains unchanged inside each record.

Each free component contributes variables in this exact order:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

Exact scalar residual flattening:

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

Mate and Distance contribute four scalar residuals. Concentric contributes six.

Default length scale:

```text
1.0 mm
```

The central finite-difference Jacobian uses separate translation and rotation perturbation steps.

Every plus/minus evaluation re-resolves semantic targets and rebuilds the appropriate geometric residual descriptor.

A residual-dimension change during finite differences fails explicitly.

## Rigid-body solver

Canonical document: `docs/assembly-rigid-body-solver-mvp5.md`.

Participation policy:

```text
Grounded   -> fixed participant
Free       -> six numeric variables
Suppressed -> selected group rejected
Visibility -> no solve participation effect
```

At least one grounded component is required. Multiple grounded components are allowed and remain fixed.

The numeric method is deterministic damped Gauss-Newton:

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

Mate, Distance, and Concentric all participate through the shared numeric system. There is no Concentric-specific solver branch.

The implemented Concentric solve path corrects lateral axis offset and axis tilt.

Equal and opposed coincident axis directions are valid states.

Axial translation and rotation about the common axis remain unconstrained by the Concentric residual definition.

## Explicit result application

`AssemblySolveResult` contains complete group component snapshots and proposed transforms for free components.

`AssemblySolveResultApplier`:

- accepts only converged results
- verifies complete component snapshots
- rejects changed transforms, grounding, or suppression state
- detects moved grounded anchors
- validates every proposal
- applies to a private project copy
- replaces the caller project only after every update succeeds

This same atomic application boundary handles Mate, Distance, and Concentric solver results.

Non-converged Concentric results cannot partially mutate persistent placement.

## Local Jacobian-rank and remaining-DOF diagnostics

Canonical document: `docs/assembly-solve-diagnostics-mvp5.md`.

For a converged numeric state, diagnostics evaluate the same shared Jacobian on a private project copy.

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

DOF classification, consistency classification, and residual-row rank structure are separate concepts.

Redundant scalar residual rows do not automatically imply semantic overconstraint.

Regular one-free-body Concentric result:

```text
residual_component_count = 6
variable_count           = 6
jacobian_rank            = 4
constrained_dof          = 4
remaining_dof            = 2
residual_row_redundancy  = 2
```

The rank is measured from the shared central finite-difference Jacobian. It is not hard-coded from the constraint type.

A regular Distance plus Concentric system with the plane normal following the common axis produces:

```text
residual_component_count = 10
jacobian_rank            = 5
remaining_dof            = 1
```

The remaining local freedom is rotation about the common axis.

## Grounding

`ComponentGroundingState::Grounded` is persisted model intent.

Storage-level direct transform updates remain legal while grounded. This keeps explicit model edits separate from solver policy.

The solver interprets grounded components as fixed participants.

A group with no grounded component is rejected instead of receiving a hidden floating-group gauge condition.

An all-grounded group with unsatisfied Concentric residuals is reported as `FixedGeometryInconsistent` rather than moving an anchor.

## Visibility and suppression

Visibility and suppression are persistent editable state.

Visibility does not affect the solver.

Suppression does not currently rewrite graph connectivity or constraint participation, so the solver rejects a selected connected group containing a suppressed component instead of silently dropping it.

A later suppression-aware solve path must define graph and constraint participation together.

## Failure propagation

Semantic target failures propagate through the shared numeric path unchanged.

An unsupported Concentric token such as:

```text
bolt.main_axis
```

fails with:

```text
unsupported assembly semantic axis reference family
```

The solver and diagnostics no longer report Concentric as globally unsupported.

## Persistence boundary

The assembly/project format persists:

- component identity and referenced part documents
- visibility, suppression, grounding
- `RigidTransform`
- assembly constraint records and semantic target strings

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

`feature.hole.axis` uses the existing semantic-reference string field.

Only explicit application of a fresh converged solver result changes the existing persisted component transform.

## Motion, joints, and limits

Underdefined components should eventually be draggable only within allowed DOF. A drag should become solver input projected into valid constraint space.

Future joint families include Revolute, Prismatic, Cylindrical, Planar, Ball, Rigid, Gear, and Screw relations.

These remain downstream of a stable rigid constraint/DOF model and future null-space presentation.

## Next technical step

The next assembly block is stable Insert constraint intent and a read-only composite Insert residual model.

Insert must explicitly combine:

```text
semantic axis alignment
semantic axial seating-plane intent
```

The first block should define stable seating-plane references for the supported circular-cut feature family, add explicit Insert record/JSON semantics, preserve target A/B order for signed axial seating, and construct a dedicated derived residual descriptor.

A regular Insert relationship should preserve Concentric's four independent axis-line constraints and add one independent axial seating direction:

```text
variable_count  = 6
jacobian_rank   = 5
remaining_dof   = 1
```

The remaining freedom should be rotation about the common axis.

Insert numeric-system, solver, application, and DOF integration remain a separate following block after those target and residual semantics are stable.

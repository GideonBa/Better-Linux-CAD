# Concentric Numeric Solver and DOF Integration MVP-5

Status: implemented Concentric integration into the shared assembly numeric residual/Jacobian system, deterministic rigid-body solver, explicit solve-result application path, and read-only local Jacobian-rank/remaining-DOF diagnostics.

Stable Insert intent and composite Insert residual semantics are now implemented separately. Insert numeric integration is the next shared-solver block.

## Goal

This block makes semantic-axis Concentric residuals a first-class numeric assembly constraint without creating a second solver path.

```text
persistent active Concentric constraint
  -> feature.<feature-id>.axis resolution
  -> component-local semantic axes
  -> assembly-space axis evaluation
  -> Concentric residual descriptor
  -> shared numeric residual flattening
  -> shared central finite-difference Jacobian
  -> existing damped Gauss-Newton solver
  -> proposed transforms
  -> explicit atomic result application
  -> shared Jacobian-rank / remaining-DOF diagnostics
```

No Concentric-specific optimizer, transform representation, Jacobian implementation, or persistence cache exists.

## Semantic geometry contract

Canonical document: `docs/assembly-semantic-axis-concentric-residuals-mvp5.md`.

First supported target family:

```text
feature.<feature-id>.axis
```

First producer: one `SubtractiveExtrude` whose source sketch contains exactly one `CircleProfile` and exactly one total profile.

For assembly-space axis lines:

```text
direction_parallelism = cross(dA, dB)
axis_offset_mm         = cross(oB - oA, dA)
```

This integration block does not change those geometric semantics.

## Shared numeric-system selection

The private geometry-layer numeric system is:

```text
src/geometry/assembly_constraint_numeric_system.hpp
src/geometry/assembly_constraint_numeric_system.cpp
```

Builder selection:

```text
Concentric
  -> AssemblyConcentricConstraintEquationBuilder

Mate / Distance
  -> AssemblyConstraintEquationBuilder
```

The selection occurs inside the one residual evaluator used by both:

```text
AssemblyRigidBodySolver
AssemblySolveDiagnosticsAnalyzer
```

Solver and diagnostics therefore cannot interpret Concentric differently.

## Exact scalar residual order

Constraints use lexicographic `AssemblyConstraintId` order from the deterministic graph. Persistent target A/B order remains unchanged inside each record.

Mate:

```text
normal_opposition.x
normal_opposition.y
normal_opposition.z
signed_separation_mm / length_residual_scale_mm
```

Distance:

```text
normal_parallelism.x
normal_parallelism.y
normal_parallelism.z
distance_residual_mm / length_residual_scale_mm
```

Concentric:

```text
direction_parallelism.x
direction_parallelism.y
direction_parallelism.z
axis_offset_mm.x / length_residual_scale_mm
axis_offset_mm.y / length_residual_scale_mm
axis_offset_mm.z / length_residual_scale_mm
```

The first three Concentric values are dimensionless. The final three are millimeter-valued and divided by the existing length scale.

Default scale remains `1.0 mm`.

No Concentric-specific weight exists.

## Mixed residual vectors

Mixed Mate/Distance/Concentric systems use one flat graph-ordered vector.

For:

```text
constraint.a.distance
constraint.z.concentric
```

numeric order is:

```text
4 Distance scalars
6 Concentric scalars
```

for total dimension `10`.

Constraint insertion order does not change this ordering.

## Finite-difference Jacobian

Free component variables remain:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

For variable `j`:

```text
J[:,j] = (r(x + h_j e_j) - r(x - h_j e_j)) / (2 h_j)
```

Every plus/minus evaluation re-runs semantic target resolution, transform evaluation, residual construction, and flattening.

No resolved-axis or Concentric Jacobian cache is retained.

The existing residual-dimension guard also applies to Concentric and mixed systems.

## Solver integration

`AssemblyRigidBodySolver` has no Concentric branch. It consumes the shared residual system:

```text
r(x)
  -> central finite-difference J
  -> J^T J + lambda I
  -> -J^T r
  -> partial-pivot Gaussian elimination
  -> deterministic backtracking line search
  -> damping escalation
```

Existing policies remain unchanged:

- solve exactly one deterministic connected group
- require at least one grounded component
- keep every grounded component fixed
- allow multiple grounded components
- reject selected suppressed components
- ignore visibility for participation
- never mutate the source project during solve
- return unpersisted transform proposals

## Solved Concentric behavior

### Lateral axis offset

A perpendicular axis-line offset produces nonzero `axis_offset_mm`. Translation finite-difference columns and the existing Gauss-Newton path remove the lateral separation.

### Axis tilt

Nonparallel axes produce nonzero `direction_parallelism`. Rotation finite-difference columns and the same solver path correct the tilt.

No analytic Concentric rotation derivative is introduced.

### Equal and opposed directions

```text
cross(dA, dB) = 0
```

for both `dB=dA` and `dB=-dA`.

The solver therefore accepts equal or opposed coincident axis directions and does not flip an already-valid opposed axis solely to make directions equal.

### Remaining freedoms

Regular Concentric intentionally leaves:

```text
translation along the common axis
rotation about the common axis
```

free.

No hidden residual or variable lock is added.

## Explicit result application

Concentric uses the existing:

```text
AssemblySolveResult
AssemblySolveResultApplier
```

The applier accepts only converged results, validates complete source snapshots, detects changed transforms/grounding/suppression and moved grounded anchors, validates proposals, and applies atomically through a project copy.

Non-converged Concentric results remain unapplable.

## Fixed inconsistency and non-convergence

An all-grounded group with Concentric residual RMS above tolerance returns `FixedGeometryInconsistent`; grounded components are not moved and no proposals are produced.

Concentric uses the same iteration, damping, and line-search limits as every supported numeric family.

A deliberately iteration-limited unsolved case remains non-converged, leaves the source project unchanged, and cannot be applied.

## Local Jacobian rank and remaining DOF

`AssemblySolveDiagnosticsAnalyzer` evaluates the exact shared numeric Jacobian after a private converged solve state.

For one regular Concentric relationship between one grounded and one free body:

```text
variable_count           = 6
residual_component_count = 6
jacobian_rank            = 4
constrained_dof          = 4
remaining_dof            = 2
residual_row_redundancy  = 2
```

Classification:

```text
Underconstrained
LocallyConsistent
RedundantResidualComponents
```

The result is computed by the generic rank analyzer. There is no hard-coded `Concentric -> rank 4` rule.

## Why six residual components produce rank four

Near a regular aligned unit-axis state, `direction_parallelism` has only two independent first-order tilt sensitivities.

`axis_offset_mm` is perpendicular to target A's axis and has only two independent lateral sensitivities.

Six scalar rows therefore carry four independent local directions.

Residual-row redundancy is not automatically semantic overconstraint.

## Mixed Distance plus Concentric rank

For one free body with one Concentric relationship and one planar Distance whose normal follows the common axis:

```text
variable_count           = 6
residual_component_count = 10
jacobian_rank            = 5
remaining_dof            = 1
```

Distance adds axial separation while its orientation sensitivities overlap Concentric tilt sensitivities.

The remaining freedom is rotation about the common axis.

## Error propagation

Concentric semantic target failures propagate unchanged through the shared numeric path.

An unsupported token such as `bolt.main_axis` remains:

```text
unsupported assembly semantic axis reference family
```

rather than a generic solver failure.

## Persistence boundary

This block adds no persistent records or JSON fields.

Derived and unpersisted data includes:

- resolved local semantic axes
- assembly-space axes
- Concentric residual descriptors
- flattened residual vectors
- finite-difference Jacobians
- normal equations and damping attempts
- solve iteration state
- solve results and unapplied proposals
- rank summaries and remaining-DOF diagnostics

Only explicit application of a fresh converged `AssemblySolveResult` changes `component_instances[].transform`.

## Tests

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric-solver]"
```

Coverage includes exact mixed residual ordering/dimension, Concentric scaling, deterministic repeated residual evaluation, lateral offset correction, axis tilt correction, equal/opposed valid axes, preserved axial and axis-rotation freedoms, all-grounded inconsistency, non-convergence, explicit result application, semantic target failure propagation, rank `4/6`, and mixed Distance+Concentric rank `5/6`.

## Downstream Insert boundary

Stable Insert semantics are now implemented in `docs/assembly-insert-intent-composite-residuals-mvp5.md`.

Insert reuses Concentric axis alignment and adds signed seating:

```text
direction_parallelism       = cross(dA, dB)
axis_offset_mm               = cross(oB - oA, dA)
signed_seating_separation_mm = dot(sB - sA, nA)
```

A direct `7 x 6` finite-difference Jacobian proves regular rank `5`, leaving rotation about the common axis free.

Insert is not yet selected or flattened by the shared numeric system.

## Next technical step

The next block integrates Insert into the same shared residual/Jacobian, solver, application, and diagnostics path.

Exact future Insert flattening:

```text
direction_parallelism.x
direction_parallelism.y
direction_parallelism.z
axis_offset_mm.x / length_residual_scale_mm
axis_offset_mm.y / length_residual_scale_mm
axis_offset_mm.z / length_residual_scale_mm
signed_seating_separation_mm / length_residual_scale_mm
```

Mate/Distance/Concentric ordering and every existing solver/application contract must remain unchanged. Shared diagnostics must reproduce Insert rank `5/6` with one remaining DOF rather than hard-code it.

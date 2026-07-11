# Concentric Numeric Solver and DOF Integration MVP-5

Status: implemented Concentric integration into the shared assembly numeric residual/Jacobian system, deterministic rigid-body solver, explicit solve-result application path, and read-only local Jacobian-rank/remaining-DOF diagnostics.

## Goal

This block makes the previously implemented semantic-axis and Concentric residual model a first-class numeric assembly constraint without creating a second solver path.

The integrated path is:

```text
persistent active Concentric constraint
  -> feature.<feature-id>.axis target resolution
  -> component-local semantic axes
  -> assembly-space axis evaluation
  -> Concentric residual descriptor
  -> shared numeric residual flattening
  -> shared central finite-difference Jacobian
  -> existing damped Gauss-Newton solver
  -> proposed component transforms
  -> explicit atomic result application
  -> shared Jacobian-rank / remaining-DOF diagnostics
```

No Concentric-specific optimizer, transform representation, Jacobian implementation, or persistence cache is introduced.

## Existing semantic geometry contract

Canonical target/residual document:

```text
docs/assembly-semantic-axis-concentric-residuals-mvp5.md
```

The first supported semantic axis family is:

```text
feature.<feature-id>.axis
```

The first supported producer is one `SubtractiveExtrude` whose source sketch contains exactly one `CircleProfile` and exactly one total profile.

For assembly-space axis lines:

```text
oA = target A axis origin
dA = target A axis direction

oB = target B axis origin
dB = target B axis direction
```

The geometric Concentric residual descriptor remains:

```text
direction_parallelism = cross(dA, dB)
axis_offset_mm         = cross(oB - oA, dA)
```

This integration block does not change those semantics.

## Shared numeric-system selection

The private geometry-layer numeric system remains:

```text
src/geometry/assembly_constraint_numeric_system.hpp
src/geometry/assembly_constraint_numeric_system.cpp
```

`evaluate_residuals(...)` now selects the residual builder from the persistent constraint type:

```text
Concentric
  -> AssemblyConcentricConstraintEquationBuilder

Mate / Distance
  -> AssemblyConstraintEquationBuilder
```

The selection happens inside the one shared residual evaluator used by both:

```text
AssemblyRigidBodySolver
AssemblySolveDiagnosticsAnalyzer
```

The solver and diagnostics therefore cannot silently interpret Concentric differently.

## Exact scalar residual order

Constraint records are still consumed in lexicographic `AssemblyConstraintId` order from the deterministic active-constraint graph.

Each constraint preserves its persistent target A/B order.

### Mate

Unchanged:

```text
normal_opposition.x
normal_opposition.y
normal_opposition.z
signed_separation_mm / length_residual_scale_mm
```

Four scalar components.

### Distance

Unchanged:

```text
normal_parallelism.x
normal_parallelism.y
normal_parallelism.z
distance_residual_mm / length_residual_scale_mm
```

Four scalar components.

### Concentric

Implemented exact order:

```text
direction_parallelism.x
direction_parallelism.y
direction_parallelism.z
axis_offset_mm.x / length_residual_scale_mm
axis_offset_mm.y / length_residual_scale_mm
axis_offset_mm.z / length_residual_scale_mm
```

Six scalar components.

The first three values are dimensionless direction residuals.

The final three values are millimeter-valued axis-offset components divided by the existing configured length residual scale.

Default scale remains:

```text
1.0 mm
```

No Concentric-specific weight is introduced.

## Mixed residual vectors

Mixed Mate/Distance/Concentric systems use one flat vector in graph constraint-id order.

Example:

```text
constraint.a.distance
constraint.z.concentric
```

produces:

```text
4 Distance scalar residuals
then
6 Concentric scalar residuals
```

for a total of:

```text
10 residual components
```

Constraint insertion order in `AssemblyDocument` does not change this numeric order because graph edges are deterministically sorted by `AssemblyConstraintId`.

## Finite-difference Jacobian

The existing central finite-difference implementation is reused unchanged.

Free component variable order remains:

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

Translation and rotation perturbation steps remain independently configured.

Every plus/minus residual evaluation re-runs:

```text
semantic target resolution
  -> assembly-space axis evaluation
  -> Concentric residual construction
  -> numeric flattening
```

No resolved axis or Concentric Jacobian cache is retained between perturbations.

The existing residual-dimension guard also applies to Concentric and mixed systems. If a perturbation changes residual-vector dimension, Jacobian construction fails explicitly.

## Solver integration

`AssemblyRigidBodySolver` is not given a Concentric branch.

It already consumes the shared numeric system, so Concentric now automatically participates in the same deterministic solve path:

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

- solve exactly one deterministic graph connected group
- require at least one grounded component
- every grounded component is fixed
- multiple grounded components are supported
- suppressed group components are rejected
- visibility does not affect solve participation
- source project is never mutated during solve
- result proposals are derived and unpersisted

## Solved Concentric behavior

### Lateral axis offset

For two parallel Z axes with:

```text
oB - oA = (3, -4, 25) mm
```

Concentric sees only the lateral X/Y separation.

The solver corrects the X/Y offset while axial Z separation remains unconstrained.

### Axis tilt

A free component whose semantic axis is tilted relative to target A produces non-zero `direction_parallelism`.

The existing rotational finite-difference columns and Gauss-Newton path correct the tilt.

No analytic Concentric rotation derivative is introduced.

### Equal and opposed directions

Because:

```text
cross(dA, dB) = 0
```

for both:

```text
dB = dA
dB = -dA
```

same-direction and opposed-direction coincident axis lines are both valid Concentric states.

The solver therefore does not flip an already opposed valid axis merely to enforce direction equality.

## Intentionally remaining freedoms

For one regular Concentric relationship between one grounded and one free rigid body, the relationship leaves two local rigid-body freedoms:

```text
translation along the common axis
rotation about the common axis
```

The numeric solver does not add hidden residuals for either freedom.

With a centered local semantic axis, a lateral-offset-only solve leaves the existing axial translation and rotation-about-axis variables unchanged because their Jacobian columns are zero for the Concentric residual at that state.

This is a numeric consequence of the residual definition, not a hard-coded variable lock.

## Explicit result application

Concentric uses the existing:

```text
AssemblySolveResult
AssemblySolveResultApplier
```

The solver still returns complete group component snapshots and proposed transforms for free components.

The applier still:

- accepts only `Converged`
- validates every group component snapshot
- detects transform, grounding, or suppression changes
- detects moved grounded anchors
- validates every proposal
- applies to a project copy
- replaces the caller project only after every update succeeds

A converged Concentric result is therefore applied through exactly the same atomic mutation boundary as Mate/Distance.

Non-converged Concentric results remain unapplable.

## Fixed Concentric inconsistency

For an all-grounded connected group, no numeric variables exist.

If Concentric residual RMS exceeds the configured convergence tolerance, the existing solver returns:

```text
AssemblySolveState::FixedGeometryInconsistent
```

Grounded components are not moved to repair a lateral axis offset or tilt.

The result contains no transform proposals.

## Non-convergence behavior

Concentric uses the same iteration, damping, and line-search limits as every other supported numeric constraint.

A constrained test with one permitted iteration proves that an unsolved Concentric system returns a non-converged solve state and leaves the source project unchanged.

The result applier rejects that result.

No partial Concentric transform is committed implicitly.

## Local Jacobian rank and remaining DOF

`AssemblySolveDiagnosticsAnalyzer` already evaluates the exact shared numeric Jacobian after a private converged solve state is obtained.

Because Concentric is now part of the shared numeric system, diagnostics require no Concentric-specific rank rule.

For one regular Concentric relationship between one grounded and one free body:

```text
variable_count             = 6
residual_component_count   = 6
jacobian_rank              = 4
constrained_dof            = 4
remaining_dof              = 2
residual_row_redundancy    = 2
```

Classification:

```text
dof_classification
  = Underconstrained

consistency_classification
  = LocallyConsistent

residual_rank_structure
  = RedundantResidualComponents
```

The result proves the expected four independent local constraints numerically through the central finite-difference Jacobian.

It is not hard-coded from `AssemblyConstraintType::Concentric`.

## Why six residual components produce rank four

`direction_parallelism` is a three-component vector, but near a regular unit-axis alignment it has only two independent first-order rotational directions.

`axis_offset_mm` is also a three-component vector, but it is perpendicular to target A's unit axis and has only two independent lateral directions.

Therefore the six scalar rows have four independent local sensitivities in the regular case.

The existing residual-row-rank classification correctly reports two redundant residual components without calling the relationship semantically overconstrained.

## Mixed Distance plus Concentric rank

For one free body with:

```text
one Concentric relationship
one planar Distance relationship whose plane normal follows the same common axis
```

Concentric contributes four independent local directions.

The planar Distance relationship adds axial separation but its two orientation sensitivities overlap the Concentric axis-tilt sensitivities.

The regular combined local result is:

```text
variable_count           = 6
residual_component_count = 10
jacobian_rank            = 5
remaining_dof            = 1
```

The remaining freedom is rotation about the common axis.

Again, this is measured from the actual shared Jacobian rather than inferred by summing nominal constraint DOF counts.

## Error propagation

Concentric semantic target failures now reach solver and diagnostics through the shared numeric path.

For example, an unsupported token family such as:

```text
bolt.main_axis
```

fails with the semantic-axis resolver error:

```text
unsupported assembly semantic axis reference family
```

The numeric system does not translate that failure into the historical generic "Concentric unsupported" error.

Target-resolution errors remain geometry-layer errors and propagate unchanged.

## Read-only and persistence boundary

This block adds no persistent records or JSON fields.

The following remain derived and unpersisted:

- resolved component-local semantic axes
- evaluated assembly-space axes
- Concentric residual descriptors
- flattened numeric residual vectors
- finite-difference Jacobians
- normal equations and damping attempts
- solve iteration state
- solve results and unapplied proposals
- rank summaries
- remaining-DOF diagnostics

The source `Project` remains unchanged during solve and diagnostics.

Only explicit successful application of a fresh converged `AssemblySolveResult` changes the existing persisted:

```text
component_instances[].transform
```

No `concentric_state`, `solved_axis`, `solver_pose`, `dof`, or Jacobian cache field is added.

## Tests

The focused integration suite is:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric-solver]"
```

It covers:

- exact mixed Distance/Concentric constraint-id order
- exact four-scalar Distance then six-scalar Concentric flattening
- Concentric millimeter residual scaling
- repeated deterministic residual evaluation
- mixed residual dimension `10`
- mixed central finite-difference Jacobian dimension `10 x 6`
- lateral axis-offset correction
- axial translation preservation
- rotation-about-axis preservation for the centered-axis regular case
- axis-tilt correction
- equal-direction initial Concentric state
- opposed-direction initial Concentric state
- deterministic repeated Concentric solving
- source-project immutability during solve
- successful explicit result application
- post-application zero Concentric residuals
- all-grounded Concentric inconsistency
- non-converged Concentric solve boundaries
- unapplable non-converged results
- regular Concentric Jacobian rank `4/6`
- two remaining local DOF
- two redundant Concentric residual rows
- deterministic mixed Distance/Concentric diagnostics
- mixed rank `5/6` and one remaining DOF
- fixed inconsistent diagnostics without false rank
- non-converged diagnostics without false rank

Existing solver and diagnostics suites also continue to verify their previous Mate/Distance, grounding, suppression, snapshot, stale-result, and application contracts.

## Deliberate limitations

This block does not implement:

- analytic Concentric Jacobians
- sparse Jacobian or normal-equation storage
- SVD-based rank analysis
- explicit null-space bases
- per-component semantic DOF labels
- drag projection into the Concentric null space
- pattern-hole axis instance references
- shaft/revolve/construction-axis semantic families
- Insert constraints
- axial seating semantics
- Angle, Tangent, Flush, Coincident, or Lock constraints
- joints or limits
- motion
- collision/interference analysis
- subassemblies
- component geometry instancing
- assembly-level STEP export

## Next technical step

The next assembly block is the first stable Insert constraint intent and read-only composite residual model.

Insert must not be implemented as "Concentric plus an undocumented Z offset".

The next block should first define explicit semantic axial-seating geometry for supported circular features, then define a persistent Insert relationship whose targets can derive both:

```text
axis alignment intent
axial seating-plane intent
```

The block should:

- define stable semantic entry/seating plane references for the first supported circular-cut feature family without raw OCCT topology ids
- introduce `AssemblyConstraintType::Insert` only with explicit JSON compatibility semantics
- define whether Insert accepts equal/opposed axis direction and which seating-plane normal orientation is canonical
- define signed axial seating using persistent target A/B order
- construct a read-only composite Insert residual descriptor from semantic axes plus seating planes
- preserve Concentric's four axis-line constraints and add exactly one independent axial seating direction in the regular case
- prove the expected regular local rank is five with one remaining rotation-about-axis DOF before solver integration
- keep Insert solving/application integration as a separate downstream block after target and residual semantics are stable

No persistent solved transform, residual, Jacobian, or DOF cache is required for that next block.

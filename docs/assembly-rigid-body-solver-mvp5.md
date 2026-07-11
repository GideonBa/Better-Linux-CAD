# Rigid-Body Assembly Solver MVP-5

Status: implemented deterministic rigid-body solver for supported Mate, Distance, and Concentric connected groups with one shared numeric residual/Jacobian path and an explicit atomic result-application boundary.

## Goal

The solver derives proposed component placements from persistent assembly relationship intent without mutating the source project during solve.

It consumes exactly one deterministic connected group from `AssemblyConstraintGraph`, keeps grounded components fixed, evaluates the shared assembly numeric system, optimizes free component transforms on private `Project` copies, and returns explicit transform proposals.

`AssemblySolveResultApplier` is the only transform-application boundary introduced by this solver path.

## API

```text
AssemblySolveState
  Converged
  MaximumIterationsReached
  FixedGeometryInconsistent
  NumericalFailure

AssemblyRigidBodySolverOptions
AssemblySolveResidualSummary
AssemblySolveComponentSnapshot
ProposedComponentTransform
AssemblySolveResult
AssemblyRigidBodySolver
AssemblySolveResultApplier
```

Public header:

```text
include/blcad/geometry/assembly_rigid_body_solver.hpp
```

Concentric integration adds no new public solver type or solver method.

## Exact connected-group contract

The solver input must exactly match one group returned by:

```text
AssemblyConstraintGraph::connected_components()
```

including lexicographic component order.

The solver does not silently sort, merge, split, or expand a caller-provided group.

Independent graph groups remain independently solvable.

## Grounding policy

```text
Grounded -> fixed participant
Free     -> six numeric transform variables
```

At least one component must be grounded. The solver does not invent a floating-group gauge condition.

Multiple grounded components are allowed and remain fixed.

An all-grounded group returns:

- `Converged` if residual RMS is within tolerance
- `FixedGeometryInconsistent` otherwise

Grounded components receive no transform proposals.

This policy is identical for Mate, Distance, and Concentric groups.

## Suppression and visibility

Suppressed components are explicitly unsupported by the current solver.

A selected group containing a suppressed component fails before numeric solving.

Visibility does not affect solve participation.

These are solver policies; the component storage layer still permits explicit state and transform edits.

## Supported numeric constraint families

The shared numeric system now consumes:

```text
Mate
Distance
Concentric
```

Geometry-family builders remain separate:

```text
Mate / Distance
  -> AssemblyConstraintEquationBuilder

Concentric
  -> AssemblyConcentricConstraintEquationBuilder
```

The private shared numeric system selects the correct builder from `AssemblyConstraintType`, flattens the resulting descriptor, and supplies one residual vector to the solver.

The solver itself has no Concentric-specific optimization branch.

Canonical Concentric integration detail:

```text
docs/assembly-concentric-numeric-solver-dof-mvp5.md
```

## Deterministic residual ordering

Constraints are consumed in lexicographic `AssemblyConstraintId` order inherited from the graph.

Persistent target A/B order remains unchanged inside each constraint.

### Mate

```text
normal_opposition.x
normal_opposition.y
normal_opposition.z
signed_separation_mm / length_residual_scale_mm
```

Four scalar residuals.

### Distance

```text
normal_parallelism.x
normal_parallelism.y
normal_parallelism.z
distance_residual_mm / length_residual_scale_mm
```

Four scalar residuals.

### Concentric

```text
direction_parallelism.x
direction_parallelism.y
direction_parallelism.z
axis_offset_mm.x / length_residual_scale_mm
axis_offset_mm.y / length_residual_scale_mm
axis_offset_mm.z / length_residual_scale_mm
```

Six scalar residuals.

The first three Concentric components are dimensionless. The final three are millimeter-valued axis-offset components divided by the configured length scale.

Default length residual scale:

```text
1.0 mm
```

Mate and Distance flattening are unchanged by Concentric integration.

## Deterministic variable ordering

Free components follow lexicographic connected-group order.

Each free component contributes:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

The solver uses persisted `RigidTransform` coordinates directly. It does not hide a quaternion, rotation vector, or alternative Euler convention at the API boundary.

Every residual evaluation continues through `AssemblyTransformEvaluator`, whose active right-handed fixed-axis X-then-Y-then-Z convention is authoritative.

## Numeric method

The solver is deterministic damped Gauss-Newton.

At each iteration:

```text
1. evaluate current flattened residual vector r
2. construct central finite-difference Jacobian J
3. form J^T J + lambda I
4. form -J^T r
5. solve with partial-pivot Gaussian elimination
6. try the step with deterministic backtracking line search
7. if no step decreases RMS, escalate damping and retry
8. accept the first decreasing candidate
```

Default numeric contract:

```text
length residual scale                 1.0 mm
convergence RMS                       1.0e-8
translation finite-difference step    1.0e-4 mm
rotation finite-difference step       1.0e-4 deg
initial damping                       1.0e-6
maximum iterations                    100
maximum damping attempts              8
maximum line-search steps             12
```

Central finite-difference column:

```text
J[:,j] = (r(x + h_j e_j) - r(x - h_j e_j)) / (2 h_j)
```

Every plus/minus perturbation re-evaluates current semantic target geometry and reconstructs the relevant residual descriptor.

No analytic Jacobian or sparse linear algebra is implemented.

## Concentric solve behavior

Concentric geometric residuals remain:

```text
direction_parallelism = cross(dA, dB)
axis_offset_mm         = cross(oB - oA, dA)
```

The shared solver now corrects:

```text
lateral axis offset
axis tilt
```

### Lateral axis offset

A free axis offset perpendicular to the grounded target axis produces non-zero `axis_offset_mm`.

The translational finite-difference columns and existing Gauss-Newton path remove that lateral separation.

### Axis tilt

A nonparallel free axis produces non-zero `direction_parallelism`.

The rotational finite-difference columns and same Gauss-Newton path correct the tilt.

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

same-direction and opposed-direction coincident axes are both valid Concentric states.

The solver does not rotate an already valid opposed axis merely to make directions equal.

### Remaining axis freedoms

Concentric deliberately does not constrain:

```text
translation along the common axis
rotation about the common axis
```

The solver adds no hidden residual or variable lock for those directions.

In the regular centered-axis test case, an offset-only solve preserves the existing axial translation and rotation-about-axis variables because the corresponding Concentric Jacobian columns carry no local sensitivity.

## Solve states

### Converged

```text
residual_rms <= convergence_rms
```

Only a converged result may be applied.

### MaximumIterationsReached

The iteration limit was reached with residual RMS above tolerance.

Current best free-component transforms remain in the result for diagnostics, but the applier rejects the result.

### FixedGeometryInconsistent

Returned for all-grounded groups whose residual RMS exceeds tolerance.

Grounded components are not moved to repair Mate, Distance, or Concentric inconsistency.

### NumericalFailure

Free variables exist but no configured damping/line-search attempt produces a decreasing RMS step.

The current best proposals are retained for diagnostics and remain unapplable.

Validation and semantic-target failures use `Result<T>` errors rather than solve states.

## Read-only solve boundary

`AssemblyRigidBodySolver::solve` copies the input `Project` before changing candidate transforms.

All iterative placement updates occur on private project copies.

The source project remains unchanged for every solve state and for validation or semantic-target errors.

`AssemblySolveResult` stores component snapshots from the original input and proposed free-component transforms from the private current/best solve state.

Solver output is derived and unpersisted.

## Explicit atomic application boundary

`AssemblySolveResultApplier::apply` accepts only `Converged`.

Every group component snapshot includes:

```text
component id
grounding state
suppression state
source transform
```

The applier rejects stale results if any snapshotted solve input changed after solve, including a moved grounded anchor.

Proposals must match free component snapshots and valid transform values.

After complete prevalidation, proposals are applied to another project copy. The caller project is replaced only after every update succeeds.

The application boundary is atomic at the current `Project` value boundary.

Concentric results use exactly this existing application path.

## Semantic target error propagation

The shared numeric system does not translate geometry-layer failures into a generic solver error.

For example, an unsupported Concentric semantic token:

```text
bolt.main_axis
```

fails through the axis resolver with:

```text
unsupported assembly semantic axis reference family
```

The solver propagates that failure unchanged.

## Tests

Core solver regression suite:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-solver]"
```

Concentric numeric/solver integration suite:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric-solver]"
```

Coverage includes:

- existing Mate/Distance solve behavior
- deterministic connected-group and variable ordering
- multiple grounded participants
- suppression and zero-ground rejection
- solver option validation
- source-project immutability
- stale result detection
- atomic successful application
- exact Concentric six-scalar residual participation
- lateral axis-offset correction
- axis-tilt correction
- preserved axial slide and axis rotation in the regular centered-axis case
- equal and opposed valid axis directions
- all-grounded Concentric inconsistency
- non-converged Concentric result boundaries
- unapplable non-converged Concentric results
- semantic axis target failure propagation

## Persistence boundary

The following remain unpersisted:

```text
resolved target descriptors
assembly-space planes and axes
Mate/Distance/Concentric residual descriptors
flattened residual vectors
solver Jacobians and normal equations
AssemblySolveResult
proposed component transforms
residual summaries
rank and DOF diagnostics
```

Persisted component transforms change only after explicit successful application through the normal assembly transform update path.

## Deliberate limitations

The current solver does not implement:

- Insert constraints or axial seating semantics
- analytic Jacobians
- sparse matrix storage/solve
- trust-region methods
- per-constraint weights
- angle canonicalization
- floating-group gauge fixing
- suppressed-component solving
- semantic pattern-axis identities
- per-component null-space labels
- drag projection into valid DOF
- richer constraint families
- joints or motion
- collision/interference analysis
- subassemblies
- component geometry instancing
- assembly-level STEP export

## Next technical step

The next assembly block is stable Insert constraint intent and a read-only composite Insert residual model.

That block must first define stable semantic axial-seating geometry for supported circular features, combine explicit axis and seating-plane intent, define signed seating semantics in target A/B order, and prove the regular composite residual has local rank five with one remaining rotation-about-axis DOF before Insert is connected to this shared numeric solver.

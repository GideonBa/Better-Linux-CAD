# Rigid-Body Assembly Solver MVP-5

Status: implemented deterministic rigid-body solver for supported planar Mate/Distance connected groups, with an explicit atomic application boundary. The solver shares its planar numeric residual/Jacobian path with read-only DOF diagnostics.

Semantic generated axes and read-only Concentric residual descriptors are implemented separately. Concentric solver integration is the next block.

## Goal

The solver derives proposed component placement values from persistent assembly constraint intent without mutating the source project during solve.

It consumes exactly one deterministic connected group from `AssemblyConstraintGraph`, keeps grounded components fixed, evaluates the currently supported numeric residual system, optimizes free component transforms on private `Project` copies, and returns explicit transform proposals.

`AssemblySolveResultApplier` is the only transform-application boundary introduced by this block.

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

At least one component must be grounded. The first solver does not invent a floating-group gauge condition.

Multiple grounded components are allowed and remain fixed.

An all-grounded group returns:

- `Converged` if residual RMS is within tolerance
- `FixedGeometryInconsistent` otherwise

Grounded components receive no transform proposals.

## Suppression and visibility

Suppressed components are explicitly unsupported by the current solver.

A selected group containing a suppressed component fails before numeric solving.

Visibility does not affect solve participation.

These are solver policies; the storage layer still permits explicit state and transform edits.

## Current supported numeric constraints

The shared numeric system currently consumes:

```text
planar Mate
planar Distance
```

The generated planar face restrictions of `AssemblyConstraintTargetResolver::resolve` apply.

Semantic generated-axis targets and `ConcentricResidualDescriptor` now exist, but `AssemblyConstraintNumericSystem` does not flatten them yet.

Therefore an active Concentric record still reaches the existing planar builder rejection path when passed to the solver. The solver does not silently ignore it.

## Deterministic residual ordering

Constraints are consumed in lexicographic `AssemblyConstraintId` order inherited from the graph.

Current Mate flattening:

```text
normal_opposition.x
normal_opposition.y
normal_opposition.z
signed_separation_mm / length_residual_scale_mm
```

Current Distance flattening:

```text
normal_parallelism.x
normal_parallelism.y
normal_parallelism.z
distance_residual_mm / length_residual_scale_mm
```

Default length residual scale:

```text
1.0 mm
```

The three orientation components are dimensionless. Length residuals are divided by the configured millimeter scale before optimization.

The next Concentric integration must preserve both existing planar orders exactly.

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

The solver uses persisted `RigidTransform` coordinate values directly. It does not hide a quaternion, rotation vector, or alternative Euler convention at the API boundary.

Every residual evaluation continues through `AssemblyTransformEvaluator`, whose active right-handed fixed-axis X-then-Y-then-Z convention is authoritative.

## Numeric method

The current solver is deterministic damped Gauss-Newton.

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
initial damping                        1.0e-6
maximum iterations                     100
maximum damping attempts               8
maximum line-search steps              12
```

Central finite-difference column:

```text
J[:,j] = (r(x + h_j e_j) - r(x - h_j e_j)) / (2 h_j)
```

No analytic Jacobian or sparse linear algebra is implemented.

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

Returned only for all-grounded groups whose residual RMS exceeds tolerance.

Grounded components are not moved to repair inconsistency.

### NumericalFailure

Free variables exist but no configured damping/line-search attempt produces a decreasing RMS step.

The current best proposals are retained for diagnostics and remain unapplable.

Validation and unsupported-family problems use `Result<T>` failures rather than solve states.

## Read-only solve boundary

`AssemblyRigidBodySolver::solve` copies the input `Project` before changing candidate transforms.

All iterative placement updates occur on private project copies.

The source project remains unchanged for every solve state and for validation/unsupported-family errors.

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

## Concentric semantics now available downstream

Canonical document: `docs/assembly-semantic-axis-concentric-residuals-mvp5.md`.

The implemented Concentric builder produces:

```text
direction_parallelism = cross(dA, dB)
axis_offset_mm         = cross(oB - oA, dA)
```

Equal and opposed axis directions are valid.

Axial translation and rotation about the common axis are deliberately unconstrained.

Those semantics are not yet part of solver residual flattening.

## Next numeric integration contract

The next solver increment should extend the shared numeric system with this exact Concentric scalar order:

```text
direction_parallelism.x
direction_parallelism.y
direction_parallelism.z
axis_offset_mm.x / length_residual_scale_mm
axis_offset_mm.y / length_residual_scale_mm
axis_offset_mm.z / length_residual_scale_mm
```

The first three components are dimensionless. The three axis-offset components are millimeter-valued and use the existing configured length scale.

The integration must reuse the current:

- lexicographic constraint ordering
- free-component variable ordering
- central finite-difference path
- transform evaluator convention
- Gauss-Newton/damping/line-search method
- solve states
- grounding and suppression policy
- read-only solve boundary
- complete source snapshots
- stale-result validation
- atomic explicit application

## Tests

Current solver tests:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-solver]"
```

They cover Mate, Distance, rotation correction, chains, deterministic ordering, multiple grounded participants, fixed inconsistency, zero-grounded and suppression rejection, input validation, non-convergence, stale results, read-only solve behavior, and successful explicit application.

Semantic axis/Concentric residual tests are separate:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric]"
```

No current test claims Concentric solve support.

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

- Concentric numeric flattening or solving
- Concentric rank/DOF diagnostics
- analytic Jacobians
- sparse matrix storage/solve
- trust-region methods
- per-constraint weights
- angle canonicalization
- floating-group gauge fixing
- suppressed-component solving
- semantic pattern-axis identities
- Insert or richer constraints
- joints or motion
- collision/interference analysis
- subassemblies
- component geometry instancing
- assembly-level STEP export

## Next technical step

The next assembly block is Concentric integration into `AssemblyConstraintNumericSystem`, `AssemblyRigidBodySolver`, and `AssemblySolveDiagnosticsAnalyzer`.

It should solve lateral axis offset and tilt, preserve axial slide and axis rotation freedom, support equal/opposed directions, and prove local rank four with two remaining DOF for a regular one-free-body Concentric relationship.

Insert remains downstream until axial seating semantics are explicitly defined.

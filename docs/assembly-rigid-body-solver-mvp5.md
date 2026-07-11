# Rigid-Body Assembly Solver MVP-5

Status: implemented deterministic rigid-body solver for supported Mate, Distance, and Concentric connected groups with one shared numeric residual/Jacobian path and an explicit atomic result-application boundary. Insert target/residual semantics are implemented separately but are not yet numeric solver inputs.

## Goal

The solver derives proposed component placements from persistent assembly relationship intent without mutating the source project during solve.

It consumes exactly one deterministic connected group from `AssemblyConstraintGraph`, keeps grounded components fixed, evaluates the shared assembly numeric system, optimizes free component transforms on private `Project` copies, and returns explicit transform proposals.

`AssemblySolveResultApplier` is the only transform-application boundary.

## Public API

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

Constraint-family integrations do not add solver methods or family-specific solve result types.

## Exact connected-group contract

Solver input must exactly match one group returned by:

```text
AssemblyConstraintGraph::connected_components()
```

including lexicographic component order.

The solver does not silently sort, merge, split, or expand caller-provided groups.

Independent graph groups remain independently solvable.

## Grounding policy

```text
Grounded -> fixed participant
Free     -> six numeric transform variables
```

At least one component must be grounded. The solver does not invent a floating-group gauge condition.

Multiple grounded components are allowed and remain fixed.

An all-grounded supported numeric group returns:

- `Converged` if residual RMS is within tolerance
- `FixedGeometryInconsistent` otherwise

Grounded components receive no transform proposals.

## Suppression and visibility

A selected group containing a suppressed component fails before numeric solving.

Visibility does not affect solve participation.

These are solver policies; storage APIs still permit explicit component state/transform edits.

## Supported numeric constraint families

The shared numeric system currently consumes:

```text
Mate
Distance
Concentric
```

Builder selection:

```text
Mate / Distance
  -> AssemblyConstraintEquationBuilder

Concentric
  -> AssemblyConcentricConstraintEquationBuilder
```

The shared numeric evaluator selects the builder from `AssemblyConstraintType`, flattens the descriptor, and supplies one residual vector to solver and diagnostics.

The solver has no Concentric-specific optimization branch.

## Insert boundary

Canonical Insert semantics are implemented in `docs/assembly-insert-intent-composite-residuals-mvp5.md`.

Insert now has:

```text
AssemblyConstraintType::Insert
feature.<feature-id>.seat
ResolvedAssemblyInsertConstraintTarget
AssemblyInsertConstraintEquationBuilder
InsertResidualDescriptor
```

Canonical Insert residual:

```text
direction_parallelism       = cross(dA, dB)
axis_offset_mm               = cross(oB - oA, dA)
signed_seating_separation_mm = dot(sB - sA, nA)
```

However, `AssemblyConstraintNumericSystem` does not yet route/flatten Insert.

An active Insert record currently reaches the explicit planar-builder rejection:

```text
Insert equation construction requires dedicated composite target support
```

The solver does not ignore the Insert edge or pretend it is solved. The source project remains unchanged.

## Deterministic residual ordering

Constraints use lexicographic `AssemblyConstraintId` order from the graph. Persistent target A/B order remains unchanged inside every constraint.

### Mate

```text
normal_opposition.x
normal_opposition.y
normal_opposition.z
signed_separation_mm / length_residual_scale_mm
```

### Distance

```text
normal_parallelism.x
normal_parallelism.y
normal_parallelism.z
distance_residual_mm / length_residual_scale_mm
```

### Concentric

```text
direction_parallelism.x
direction_parallelism.y
direction_parallelism.z
axis_offset_mm.x / length_residual_scale_mm
axis_offset_mm.y / length_residual_scale_mm
axis_offset_mm.z / length_residual_scale_mm
```

Default length residual scale:

```text
1.0 mm
```

The next Insert integration must preserve every existing scalar order exactly.

Documented future Insert order:

```text
direction_parallelism.x
direction_parallelism.y
direction_parallelism.z
axis_offset_mm.x / length_residual_scale_mm
axis_offset_mm.y / length_residual_scale_mm
axis_offset_mm.z / length_residual_scale_mm
signed_seating_separation_mm / length_residual_scale_mm
```

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

The solver uses persisted `RigidTransform` coordinate values directly.

Every residual evaluation follows the authoritative active right-handed fixed-axis X-then-Y-then-Z transform convention.

## Numeric method

The current method is deterministic damped Gauss-Newton.

```text
1. evaluate flattened residual vector r
2. construct central finite-difference Jacobian J
3. form J^T J + lambda I
4. form -J^T r
5. solve with partial-pivot Gaussian elimination
6. try the step with deterministic backtracking line search
7. escalate damping if no candidate decreases RMS
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

Concentric residuals are:

```text
direction_parallelism = cross(dA, dB)
axis_offset_mm         = cross(oB - oA, dA)
```

The generic solver corrects lateral axis offset and axis tilt.

Equal and opposed coincident directions are valid because `cross(dA,dB)=0` for both.

The residual intentionally leaves axial translation and rotation about the common axis free. The solver adds no hidden lock for those directions.

## Solve states

### Converged

```text
residual_rms <= convergence_rms
```

Only a converged result may be applied.

### MaximumIterationsReached

Iteration limit reached above tolerance. Current best proposals remain diagnostic only and cannot be applied.

### FixedGeometryInconsistent

Returned for all-grounded supported numeric groups above tolerance. Grounded components are not moved to repair inconsistency.

### NumericalFailure

Free variables exist but no configured damping/line-search attempt produces a decreasing RMS step. Current best proposals remain unapplable.

Validation and semantic-target failures use `Result<T>` errors rather than solve states.

## Read-only solve boundary

`AssemblyRigidBodySolver::solve` copies the input `Project` before changing candidate transforms.

All iterations occur on private project copies.

The source project remains unchanged for every solve state and every validation/semantic-target error.

`AssemblySolveResult` stores complete component snapshots from the original input and proposed free-component transforms from the private solve state.

Solver output is derived and unpersisted.

## Explicit atomic application boundary

`AssemblySolveResultApplier::apply` accepts only `Converged`.

Every group component snapshot contains:

```text
component id
grounding state
suppression state
source transform
```

The applier rejects stale results if any snapshotted solve input changed, including a moved grounded anchor.

Proposals must match free-component snapshots and valid transform values.

After full prevalidation, proposals are applied to another project copy. The caller project is replaced only after every update succeeds.

The current application path is shared by Mate, Distance, and Concentric results.

Insert will use this same boundary after numeric integration; no Insert-specific mutation API is planned.

## Semantic target error propagation

The shared numeric system propagates geometry-layer failures unchanged.

For example, an unsupported Concentric token such as `bolt.main_axis` remains an axis-resolver failure rather than becoming a generic solver error.

Likewise, future Insert numeric integration must propagate `.seat` resolution failures through the same boundary.

## Tests

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-insert]"
```

Current coverage includes Mate/Distance solve regressions, deterministic groups/variables, grounding/suppression policy, option validation, source-project immutability, stale result detection, atomic application, Concentric six-scalar numeric participation, offset/tilt correction, preserved axial/axis-rotation freedoms, equal/opposed axes, fixed inconsistency, non-convergence, and semantic target failures.

The Insert suite currently verifies the explicit unsupported numeric boundary and unchanged project placement.

## Persistence boundary

Unpersisted data includes:

```text
resolved target descriptors
assembly-space plane/axis/seat geometry
Mate/Distance/Concentric/Insert residual descriptors
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

- Insert numeric flattening or solving
- Insert shared diagnostics rank evaluation
- analytic Jacobians
- sparse matrix storage/solve
- trust-region methods
- per-constraint weights
- angle canonicalization
- floating-group gauge fixing
- suppressed-component solving
- semantic pattern axis/seat identities
- per-component null-space labels
- drag projection into valid DOF
- richer constraints
- joints or motion
- collision/interference analysis
- subassemblies
- component geometry instancing
- assembly-level STEP export

## Next technical step

The next assembly block is **Insert integration into `AssemblyConstraintNumericSystem`, `AssemblyRigidBodySolver`, `AssemblySolveResultApplier` coverage, and `AssemblySolveDiagnosticsAnalyzer`**.

It must route Insert through the dedicated composite builder, preserve Mate/Distance/Concentric flattening, flatten the seven Insert scalars in the documented order, reuse the existing finite-difference/Gauss-Newton path, solve lateral offset/tilt/axial seating, preserve rotation about the common axis, and prove shared local rank `5/6` with one remaining DOF.

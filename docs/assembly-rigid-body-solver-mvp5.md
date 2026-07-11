# Rigid-Body Assembly Solver MVP-5

Status: implemented deterministic rigid-body solver for supported planar Mate/Distance connected groups, with an explicit atomic application boundary. The solver now shares its numeric residual/Jacobian path with the implemented read-only DOF diagnostics.

## Goal

The solver derives proposed component placement values from persistent assembly constraint intent without mutating the source project during solve.

It consumes exactly one deterministic connected group from `AssemblyConstraintGraph`, keeps grounded components fixed, evaluates the supported planar Mate/Distance numeric system, optimizes free component transforms on private `Project` copies, and returns explicit transform proposals.

`AssemblySolveResultApplier` remains the only transform-application boundary introduced by this block.

## API

Public header:

```text
include/blcad/geometry/assembly_rigid_body_solver.hpp
```

Main types:

```text
AssemblySolveState
AssemblyRigidBodySolverOptions
AssemblySolveResidualSummary
AssemblySolveComponentSnapshot
ProposedComponentTransform
AssemblySolveResult
AssemblyRigidBodySolver
AssemblySolveResultApplier
```

Solve states:

```text
Converged
MaximumIterationsReached
FixedGeometryInconsistent
NumericalFailure
```

## Input group contract

The caller supplies exactly one group returned by:

```text
AssemblyConstraintGraph::connected_components()
```

The group must match the deterministic lexicographic component order exactly.

The solver does not silently sort, merge, split, or expand caller input.

Independent graph groups remain independently solvable.

## Participation policy

```text
Grounded -> fixed rigid-body participant
Free     -> six numeric variables
Suppressed -> rejected by the first solver
Visible/Hidden -> no participation effect
```

At least one component in the selected group must be grounded.

The first solver does not invent a floating-group gauge condition.

Multiple grounded components are allowed and all remain fixed.

When every component is grounded:

- residual RMS within tolerance -> `Converged`
- residual RMS above tolerance -> `FixedGeometryInconsistent`

No transform proposals are produced for grounded components.

This is solver policy. Storage-level explicit transform updates remain legal on grounded components.

## Supported constraints

The solver consumes active graph constraints through `AssemblyConstraintEquationBuilder` and the shared numeric-system path.

Supported residual families:

```text
planar Mate
planar Distance
```

Concentric remains unsupported until semantic generated-axis targets and canonical Concentric residuals exist.

An active Concentric record is rejected through the existing equation-builder diagnostic; it is never silently ignored.

## Shared numeric-system path

Solver and DOF diagnostics now share:

```text
src/geometry/assembly_constraint_numeric_system.hpp
src/geometry/assembly_constraint_numeric_system.cpp
```

The shared path owns:

- deterministic graph constraint-id collection
- Mate/Distance residual flattening
- residual RMS and maximum-absolute residual
- free-component variable extraction
- numeric variable application to private project copies
- central finite-difference Jacobian construction

The solver no longer owns a duplicate implementation of those rules.

This ensures `AssemblySolveDiagnosticsAnalyzer` evaluates the same local numeric system that `AssemblyRigidBodySolver` optimizes.

## Deterministic residual ordering

Active graph edges are already ordered lexicographically by `AssemblyConstraintId`. The numeric path preserves that order.

Each supported constraint contributes exactly four scalar residual components.

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

Default length scale:

```text
1.0 mm
```

Orientation residuals are dimensionless. Length residuals are divided by the configured millimeter scale before numeric optimization.

The first solver does not implement per-constraint weights.

## Deterministic variable ordering

Free components retain the lexicographic order of the deterministic connected group.

Each free component contributes six variables in exact order:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

These are the persisted `RigidTransform` coordinate values directly.

No hidden quaternion, rotation vector, matrix, or alternate Euler convention is introduced at the solver API boundary.

Residual evaluation still passes through `AssemblyTransformEvaluator`, so the canonical active right-handed fixed-axis X-then-Y-then-Z convention remains authoritative.

## Numeric method

The first solver uses deterministic damped Gauss-Newton iteration.

At each accepted iteration:

```text
1. evaluate flattened residual vector r
2. build central finite-difference Jacobian J
3. form J^T J + lambda I
4. form -J^T r
5. solve damped normal equations with partial-pivot Gaussian elimination
6. try deterministic backtracking step scales
7. accept the first candidate with lower residual RMS
8. if no candidate is accepted, increase damping and retry
```

Central finite-difference column:

```text
J[:,j] = (r(x + h_j e_j) - r(x - h_j e_j)) / (2 h_j)
```

Translation and rotation variables use different configured perturbation steps because their API units are millimeters and degrees.

Default options:

```text
length_residual_scale_mm               1.0
convergence_rms                        1.0e-8
finite_difference_translation_step_mm  1.0e-4
finite_difference_rotation_step_deg    1.0e-4
initial_damping                        1.0e-6
maximum_iterations                     100
maximum_damping_attempts               8
maximum_line_search_steps              12
```

Positive floating-point solver options must be finite and greater than zero. Iteration/search limits must be greater than zero.

No analytic Jacobian or sparse linear algebra is implemented by this seed.

## Solve states

### Converged

Returned when:

```text
residual_rms <= convergence_rms
```

A converged result may be passed to `AssemblySolveResultApplier`.

### MaximumIterationsReached

Returned when the accepted-iteration budget is exhausted while RMS remains above tolerance.

Current best proposals are retained for diagnostics, but the applier rejects the result.

### FixedGeometryInconsistent

Returned only for an all-grounded group whose residual RMS remains above tolerance.

The solver does not move grounded components to repair fixed geometry.

### NumericalFailure

Returned when free variables exist but no damping attempt and line-search candidate lowers the current residual RMS.

Current best proposals are retained for diagnostics, but the applier rejects the result.

Validation and unsupported-family failures remain `Result<T>` failures rather than solve states.

## Read-only solve boundary

`AssemblyRigidBodySolver::solve` copies the input `Project` before changing any candidate transform.

The source project therefore remains unchanged for:

- converged results before explicit application
- maximum-iteration results
- fixed-geometry inconsistency
- numerical failure
- validation errors
- unsupported target/residual families

`AssemblySolveResult` stores source component snapshots and proposed free-component transforms separately.

Solver output is derived data and is not persisted.

## Explicit atomic application boundary

`AssemblySolveResultApplier::apply` accepts only `Converged` results.

Each source snapshot contains:

```text
component id
grounding state
suppression state
source RigidTransform
```

The applier rejects stale input if any snapshotted solve input changed after solve. This includes a moved grounded anchor.

Each proposal carries its source transform and must correspond to a free component snapshot.

After prevalidation, proposals are applied to another project copy. The caller project is replaced only after every transform update succeeds.

Application is therefore atomic at the current `Project` value boundary.

## Residual summary

Every solve result reports:

```text
residual_component_count
initial_rms
final_rms
final_max_abs
```

These values describe the flattened scaled residual vector used by the numeric system.

They are not persistent constraint-state classifications.

## Implemented downstream DOF diagnostics

Canonical document: `docs/assembly-solve-diagnostics-mvp5.md`.

The repository now has `AssemblySolveDiagnosticsAnalyzer`.

For a converged solve result, the analyzer:

```text
solve selected group
  -> apply converged proposals only to a private Project copy
  -> reuse shared residual/variable/Jacobian path
  -> compute deterministic local Jacobian rank
  -> compute constrained and remaining DOF
  -> classify local DOF state
```

The solver therefore no longer lists Jacobian-rank or remaining-DOF diagnostics as future functionality.

The diagnostics intentionally do not modify solver state or persistence.

Solver damping is excluded from rank analysis because damping must not manufacture constrained directions.

## Persistence boundary

The following remain regenerable and unpersisted:

```text
constraint graph connectivity
resolved target descriptors
assembly-space target frames
Mate/Distance residual descriptors
flattened numeric residual vectors
numeric Jacobians and normal equations
solver damping and iteration state
AssemblySolveResult
unapplied proposed transforms
residual summaries
Jacobian rank summaries
constrained and remaining DOF
local DOF/consistency/rank-structure classifications
```

Persisted component `RigidTransform` values change only after explicit successful application through the normal assembly transform update path.

No solver/Jacobian/DOF cache JSON fields are added.

## Tests

`tests/geometry/assembly_rigid_body_solver_tests.cpp` covers:

- one grounded and one movable planar Mate
- deterministic repeated solve results
- source-project immutability before application
- explicit successful application and residual verification
- planar Distance translation
- nonparallel Distance normal correction
- degree-variable rotation solving
- three-component Mate chain
- deterministic results independent from insertion order
- multiple grounded participants
- satisfied and inconsistent all-grounded groups
- zero-grounded rejection
- suppressed-component rejection
- exact connected-group validation
- invalid numeric options
- unsupported Concentric propagation
- maximum-iteration state
- non-converged application rejection
- stale grounded-anchor detection

Targeted command:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-solver]"
```

The shared numeric-system refactor is additionally exercised by `tests/geometry/assembly_solve_diagnostics_tests.cpp`, because those diagnostics consume the same residual and Jacobian path.

## Deliberate limitations

The solver does not yet implement:

- analytic Jacobians
- sparse matrix storage or sparse linear solves
- trust-region methods
- per-constraint weights
- angle canonicalization
- floating-group gauge fixing
- suppressed-component solve participation
- semantic axis targets
- Concentric residuals or solving
- Insert and richer constraint families
- joints or motion
- collision/interference analysis
- subassemblies
- component geometry instancing
- assembly-level STEP export

Remaining-DOF diagnostics are no longer deferred; they are implemented as a separate downstream read-only layer.

## Next technical step

The next assembly block is a stable semantic generated-axis reference family and a read-only Concentric target/residual pipeline.

That block must define feature-produced semantic axis tokens, component-local axis descriptors, project/component/part resolution, assembly-space axis evaluation under the existing transform convention, and canonical Concentric residuals without raw OCCT topology ids.

Only after semantic axis and Concentric residual semantics are stable should the solver consume Concentric constraints. Insert remains downstream because it combines axis alignment with axial seating semantics.

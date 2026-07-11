# Rigid-Body Assembly Solver MVP-5

Status: implemented first deterministic rigid-body solve seed for supported planar Mate and Distance constraint groups.

## Goal

This block is the first assembly layer that derives new component placement values from persistent constraint intent.

The solver consumes one exact deterministic connected group from `AssemblyConstraintGraph`, keeps grounded components fixed, evaluates the existing planar Mate/Distance residual descriptors, optimizes free component transforms on a private `Project` copy, and returns explicit proposed transform updates.

The solve phase does not mutate the input project. A separate `AssemblySolveResultApplier` is the only transform-application boundary introduced by this block.

## API

The public geometry-layer API lives in:

```text
include/blcad/geometry/assembly_rigid_body_solver.hpp
```

Main types:

```text
AssemblySolveState
  Converged
  MaximumIterationsReached
  FixedGeometryInconsistent
  NumericalFailure

AssemblyRigidBodySolverOptions
  length_residual_scale_mm
  convergence_rms
  finite_difference_translation_step_mm
  finite_difference_rotation_step_deg
  initial_damping
  maximum_iterations
  maximum_damping_attempts
  maximum_line_search_steps

AssemblySolveResidualSummary
  residual_component_count
  initial_rms
  final_rms
  final_max_abs

AssemblySolveComponentSnapshot
  component_instance
  grounding_state
  suppression_state
  source_transform

ProposedComponentTransform
  component_instance
  source_transform
  proposed_transform

AssemblySolveResult
  state
  iterations
  component_group
  fixed_components
  component_snapshots
  proposed_transforms
  residual_summary

AssemblyRigidBodySolver
  solve(Project, connected_group, options)

AssemblySolveResultApplier
  apply(Project, AssemblySolveResult)
```

## Input group contract

The solver operates on exactly one connected component returned by:

```text
AssemblyConstraintGraph::connected_components()
```

The supplied `connected_group` must match one returned group exactly, including deterministic lexicographic component order.

The solver does not silently sort, merge, split, or expand a caller-provided group. This makes group identity and solve ordering explicit.

Independent connected groups remain independently solvable.

## Grounding policy

For this first solver seed:

```text
Grounded component -> fixed rigid-body participant
Free component     -> six solver variables
```

At least one component in the connected group must be grounded.

A group with zero grounded components fails before numeric solve because the first seed requires an absolute rigid-body reference and does not invent a floating-group gauge condition.

Multiple grounded components are allowed. Every grounded transform remains fixed.

If every component is grounded:

- residual RMS within tolerance -> `Converged`
- residual RMS above tolerance -> `FixedGeometryInconsistent`

No transform proposals are produced for grounded components.

This policy is solver-specific. `AssemblyDocument::set_component_instance_transform` still permits explicit placement edits on grounded components because the storage layer does not enforce solve semantics.

## Suppression and visibility policy

Suppressed components are explicitly unsupported by the first solver seed.

If any connected-group component has:

```text
ComponentSuppressionState::Suppressed
```

solve fails before residual evaluation.

The solver does not silently remove a suppressed component or its constraints from the connected group.

Visibility does not affect solver participation. Hidden and visible active components use the same constraint and grounding rules.

## Supported constraints

The solver consumes active constraints through the existing deterministic graph and `AssemblyConstraintEquationBuilder`.

Supported residual families are:

```text
planar Mate
planar Distance
```

The current generated planar face target restrictions still apply.

Concentric remains unsupported because stable semantic axis targets and Concentric residual construction are not implemented.

An active Concentric constraint in the selected group therefore fails through the existing equation-builder diagnostic instead of being ignored.

## Deterministic residual ordering

Active graph edges are already ordered lexicographically by `AssemblyConstraintId`.

The solver preserves that order.

Each supported constraint contributes exactly four scalar residual components.

Mate flattening:

```text
normal_opposition.x
normal_opposition.y
normal_opposition.z
signed_separation_mm / length_residual_scale_mm
```

Distance flattening:

```text
normal_parallelism.x
normal_parallelism.y
normal_parallelism.z
distance_residual_mm / length_residual_scale_mm
```

The default length residual scale is:

```text
1.0 mm
```

The orientation residuals are dimensionless. Length residuals are divided by the configured millimeter scale before numeric optimization so the mixed residual vector has an explicit weighting convention.

The first seed does not add per-constraint weights.

## Deterministic variable ordering

Free components follow the lexicographic order already present in the deterministic connected group.

Each free component contributes six variables in this exact order:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

The solver therefore uses:

```text
component A: tx, ty, tz, rx, ry, rz
component B: tx, ty, tz, rx, ry, rz
...
```

The internal variable representation deliberately uses the persisted `RigidTransform` coordinate values directly.

It does not introduce a hidden quaternion, rotation vector, matrix, or alternative Euler order at the API boundary.

Every residual evaluation continues through `AssemblyTransformEvaluator`, so the canonical active right-handed fixed-axis X-then-Y-then-Z rotation convention remains the source of truth.

The first seed does not canonicalize angles into a particular degree interval.

## Numeric method

The implemented solver is a deterministic damped Gauss-Newton seed.

At each iteration:

```text
1. evaluate the current flattened residual vector r
2. construct a central finite-difference Jacobian J
3. form J^T J + lambda I
4. form -J^T r
5. solve the damped normal equations with partial-pivot Gaussian elimination
6. try the step with deterministic backtracking line search
7. if no step decreases RMS, increase damping and retry
8. accept the first decreasing candidate
```

Finite-difference perturbations are applied to a private project copy.

The default numeric contract is:

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

All positive floating-point options must be finite and greater than zero. Iteration and search limits must be greater than zero.

## Central finite-difference Jacobian

For variable `x_j` with step `h_j`:

```text
J[:, j] = (r(x + h_j e_j) - r(x - h_j e_j)) / (2 h_j)
```

Translation and rotation variables use separate configured steps because their API units are millimeters and degrees.

The residual dimension must remain constant across perturbations. A dimension change is treated as an internal solver error.

No analytic Jacobian is implemented by this seed.

## Solve states

### Converged

Returned when:

```text
residual_rms <= convergence_rms
```

A converged result may be passed to `AssemblySolveResultApplier`.

### MaximumIterationsReached

Returned when accepted iterations reach `maximum_iterations` while final residual RMS remains above tolerance.

The best current free-component transforms are retained as proposals for diagnostics, but the applier rejects the result.

### FixedGeometryInconsistent

Returned only when the connected group has no free components and the fixed geometry residual RMS exceeds tolerance.

The solver does not move grounded components to repair the inconsistency.

### NumericalFailure

Returned when free variables exist but no damping attempt and line-search step can produce a lower residual RMS from the current iterate.

The best current proposals are retained for diagnostics. The applier rejects the result.

Validation and unsupported-family errors continue to use `Result<T>` failures rather than solve states.

## Read-only solve boundary

`AssemblyRigidBodySolver::solve` copies the input `Project` before changing any candidate transform.

All iterative placement updates occur on that private copy.

The source project therefore remains unchanged for:

- converged solves before explicit application
- maximum-iteration results
- fixed-geometry inconsistency
- numerical failure
- validation failure
- unsupported residual or target families

`AssemblySolveResult` stores component snapshots from the original solve input and proposed transforms from the private solved copy.

Solver output remains derived data and is not persisted.

## Explicit atomic application boundary

`AssemblySolveResultApplier::apply` accepts only `AssemblySolveState::Converged`.

Before mutation it checks the solve-input snapshots against the current project.

The snapshot includes:

```text
component id
grounding state
suppression state
source RigidTransform
```

A result is stale if any snapshotted solve input changed after solve.

This includes changes to a grounded anchor transform. A solver proposal derived from an old fixed reference therefore cannot be silently applied after that anchor moves.

Each proposal also carries its source transform and must correspond to a free component snapshot.

After prevalidation, proposals are applied to another project copy. The original project is replaced only after every transform update succeeds.

Application is therefore atomic at the `Project` value boundary used by this seed.

The applier does not persist a solved-state cache and does not rewrite constraints.

## Residual summary

Each solve result reports:

```text
residual_component_count
initial_rms
final_rms
final_max_abs
```

The values describe the flattened, scaled residual vector used by the numeric solver.

They are diagnostics, not persistent constraint-state classifications.

The first seed does not label a group underconstrained, fully constrained, or overconstrained.

## Tests

`tests/geometry/assembly_rigid_body_solver_tests.cpp` covers:

- one grounded and one movable planar Mate
- deterministic repeated solve results
- unchanged project transform before explicit application
- successful explicit application
- post-application Mate residual verification
- planar Distance translation solve
- nonparallel Distance normal correction
- direct degree-variable rotation solving through the canonical transform evaluator
- a three-component two-Mate chain
- deterministic results independent from constraint insertion order
- multiple grounded components as fixed participants
- satisfied all-grounded group
- inconsistent all-grounded group
- zero-grounded rejection
- suppressed-component rejection
- exact deterministic connected-group validation
- invalid numeric option rejection
- unsupported Concentric propagation
- maximum-iteration state
- non-converged application rejection
- stale grounded-anchor detection
- unchanged movable component when stale application is rejected

Targeted test command after a geometry build:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-solver]"
```

Complete geometry workflow:

```bash
cmake --workflow --preset dev-geometry-build-test
```

## Persistence boundary

The following remain unpersisted and regenerable:

```text
AssemblyConstraintGraph connectivity
resolved target descriptors
assembly-space target frames
Mate/Distance residual descriptors
solver Jacobians and normal equations
AssemblySolveResult
proposed component transforms
residual summaries
```

Persisted component `RigidTransform` values change only after explicit successful application through the normal assembly transform update path.

No solver-result, Jacobian, damping, iteration, or residual-summary JSON field is added.

## Deliberate limitations

This first solver seed does not implement:

- analytic Jacobians
- sparse matrix storage or sparse linear solves
- trust-region methods
- adaptive persistent solver configuration
- per-constraint residual weights
- angle canonicalization
- floating-group gauge fixing when no component is grounded
- solving suppressed components
- remaining-DOF computation
- Jacobian rank diagnostics
- underconstrained, fully constrained, or overconstrained classification
- semantic axis targets
- Concentric residuals or solving
- Insert, Angle, Tangent, Flush, Coincident, or Lock constraints
- joints or motion
- collision/interference analysis
- subassemblies
- component geometry instancing
- assembly-level STEP export

## Next technical step

The next assembly block is read-only solve diagnostics and remaining-degree-of-freedom analysis over the now-implemented deterministic solver ordering and numeric Jacobian model.

That block should define Jacobian-rank tolerance, compute group-level constrained versus remaining rigid-body DOF, distinguish underconstrained and locally fully constrained groups, and provide explicit overconstraint/inconsistency diagnostics without adding persistent DOF cache state.

Concentric and richer constraint families remain deferred until their semantic target and residual families exist.

# Assembly Solve Diagnostics and Remaining DOF MVP-5

Status: implemented read-only local Jacobian-rank and remaining-degree-of-freedom diagnostics for the shared Mate, Distance, and Concentric rigid-body numeric path.

## Goal

This block answers:

```text
At the current locally solved assembly state,
how many independent rigid-body variable directions are constrained
by the supported active numeric constraint system,
and how many local DOF remain?
```

The analyzer reuses the exact deterministic residual, variable, and central finite-difference Jacobian model used by `AssemblyRigidBodySolver`.

It does not infer DOF from constraint type names.

The result is local, linearized, regenerable derived data and is not persisted.

## API

```text
AssemblyDofClassification
  NotEvaluated
  NoVariableDof
  Underconstrained
  LocallyFullyConstrained

AssemblyConstraintConsistencyClassification
  LocallyConsistent
  FixedGeometryInconsistent
  SolverDidNotConverge

AssemblyResidualRankStructure
  NotEvaluated
  FullRowRank
  RedundantResidualComponents

AssemblySolveDiagnosticsOptions
  solver_options
  rank_absolute_tolerance
  rank_relative_tolerance

AssemblyJacobianRankSummary
  rank_evaluated
  residual_component_count
  variable_count
  jacobian_rank
  constrained_dof
  remaining_dof
  residual_row_redundancy
  maximum_abs_jacobian_entry
  pivot_threshold

AssemblySolveDiagnosticsAnalyzer
  analyze(Project, connected_group, options)
```

Concentric integration adds no second diagnostics API.

## Shared numeric-system path

Solver and diagnostics share:

```text
src/geometry/assembly_constraint_numeric_system.hpp
src/geometry/assembly_constraint_numeric_system.cpp
```

The shared path owns:

- deterministic constraint-id collection
- constraint-type residual-builder selection
- Mate/Distance/Concentric scalar residual flattening
- residual summaries
- free-component variable extraction
- numeric variable application to project copies
- central finite-difference Jacobian construction

The analyzer does not maintain a second residual or Jacobian interpretation.

Current numeric families are:

```text
Mate
Distance
Concentric
```

Concentric detail is canonicalized in:

```text
docs/assembly-concentric-numeric-solver-dof-mvp5.md
```

## Analysis pipeline

For one exact deterministic connected group:

```text
Project + connected group
  -> AssemblyRigidBodySolver::solve
  -> preserve solve state and residual summary
  -> collect deterministic constraint order
  -> FixedGeometryInconsistent:
       return explicit inconsistency
       do not claim DOF rank
  -> MaximumIterationsReached or NumericalFailure:
       return explicit non-convergence
       do not claim DOF rank
  -> Converged:
       apply proposals to a private Project copy
       evaluate shared variables/residuals/Jacobian
       compute local Jacobian rank
       compute constrained and remaining DOF
       classify local variable state
```

The input project remains unchanged.

## Local Jacobian rank convention

Let:

```text
J = m x n numeric Jacobian
m = residual component count
n = free rigid-body variable count
```

The maximum entry magnitude is:

```text
maximum_abs_jacobian_entry = max(abs(J[i,j]))
```

Pivot threshold:

```text
pivot_threshold = max(
  rank_absolute_tolerance,
  rank_relative_tolerance * maximum_abs_jacobian_entry
)
```

Defaults:

```text
rank_absolute_tolerance = 1.0e-12
rank_relative_tolerance = 1.0e-8
```

Both tolerances must be finite and non-negative and may not both be zero.

Rank is computed deterministically by row-echelon elimination with columns scanned left-to-right and maximum-magnitude row pivot selection in the current column.

Solver damping is not included in diagnostic rank because damping is a solve-stabilization mechanism and must not create artificial constrained directions.

## DOF counts

Each free component contributes:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

Therefore:

```text
variable_count = 6 * free_component_count
```

For an evaluated Jacobian:

```text
constrained_dof = jacobian_rank
remaining_dof   = variable_count - jacobian_rank
```

These are local nullity/rank values in the solver's direct `RigidTransform` coordinates.

They do not claim a global configuration-space dimension across singularities, alternate assembly modes, or semantic-reference changes.

## DOF classification

### NotEvaluated

Used when no locally converged numeric state is suitable for rank-based DOF classification.

This includes:

```text
FixedGeometryInconsistent
MaximumIterationsReached
NumericalFailure
```

### NoVariableDof

Used for a converged group with zero free components.

The absence of variables comes from grounding policy. It does not mean constraints alone fully constrained the bodies.

### Underconstrained

```text
solve_state == Converged
variable_count > 0
remaining_dof > 0
```

Proven examples now include:

```text
one planar Mate:
  variable_count = 6
  jacobian_rank  = 3
  remaining_dof  = 3

one Concentric:
  variable_count = 6
  jacobian_rank  = 4
  remaining_dof  = 2

one aligned Distance + one Concentric:
  variable_count = 6
  jacobian_rank  = 5
  remaining_dof  = 1
```

### LocallyFullyConstrained

```text
solve_state == Converged
variable_count > 0
remaining_dof == 0
```

The local numeric Jacobian has full column rank.

Three orthogonal planar Mates provide the first proven rank-six case.

## Consistency classification

DOF classification and consistency are separate fields.

### LocallyConsistent

The supported numeric system converged and rank was evaluated at the private converged transform state.

### FixedGeometryInconsistent

An all-grounded group has residual RMS above the configured convergence tolerance.

This applies equally to planar and Concentric residual systems.

The analyzer preserves the explicit solver state and does not invent a DOF classification.

### SolverDidNotConverge

Used for `MaximumIterationsReached` and `NumericalFailure`.

The residual summary and deterministic order remain visible, but `rank_evaluated` is false.

## Residual row rank is not semantic overconstraint

For an evaluated Jacobian:

```text
residual_row_redundancy = residual_component_count - jacobian_rank
```

The descriptor distinguishes:

```text
FullRowRank
RedundantResidualComponents
```

`RedundantResidualComponents` means only that flattened residual rows are not all linearly independent at the evaluated state.

It does not mean the assembly is semantically overconstrained.

Examples include vector residuals with fewer independent geometric directions, duplicate constraints, and locally overlapping sensitivities.

The analyzer therefore does not expose an `Overconstrained` state from `residual_count > rank(J)` alone.

## Proven Concentric rank behavior

Canonical Concentric residuals are:

```text
direction_parallelism = cross(dA, dB)
axis_offset_mm         = cross(oB - oA, dA)
```

A regular Concentric relationship between one grounded and one free body constrains:

```text
two rotational axis-tilt directions
two translations perpendicular to the common axis
```

It leaves:

```text
translation along the common axis
rotation about the common axis
```

free.

The actual shared central finite-difference Jacobian now proves:

```text
residual_component_count = 6
variable_count           = 6
jacobian_rank            = 4
constrained_dof          = 4
remaining_dof            = 2
residual_row_redundancy  = 2
```

This result is computed by the generic rank analyzer. There is no `if Concentric then rank = 4` rule.

## Why Concentric has two redundant residual rows

`direction_parallelism` has three scalar components, but near a regular aligned unit-axis state only two first-order rotational directions tilt the axis.

`axis_offset_mm` also has three scalar components, but it is perpendicular to target A's axis and has only two independent lateral directions.

Therefore six residual rows carry four independent local sensitivities in the regular case.

The correct classification is:

```text
Underconstrained
LocallyConsistent
RedundantResidualComponents
```

not semantic overconstraint.

## Proven mixed Distance plus Concentric behavior

For one free body with one Concentric relationship and one planar Distance relationship whose normal follows the common axis:

```text
residual_component_count = 10
variable_count           = 6
jacobian_rank            = 5
constrained_dof          = 5
remaining_dof            = 1
```

The planar Distance adds axial separation. Its two orientation sensitivities overlap the existing Concentric axis-tilt sensitivities.

The remaining local freedom is rotation about the common axis.

This is measured from the shared Jacobian rather than inferred by summing nominal constraint DOF counts.

## Deterministic ordering

Diagnostics preserve:

```text
component_group
fixed_components
variable_components
constraint_order
```

Variable components use the solver's lexicographic order.

Constraints use graph lexicographic `AssemblyConstraintId` order.

Mixed constraint families do not change this order.

Repeated analysis of the same project state/options produces equal descriptors.

## Semantic target failure propagation

Diagnostics consume the solver and shared numeric system rather than pre-validating semantic targets independently.

An unsupported Concentric target such as:

```text
bolt.main_axis
```

therefore propagates:

```text
unsupported assembly semantic axis reference family
```

The analyzer does not convert that failure into a generic "Concentric unsupported" diagnostic.

## Read-only and persistence boundary

`AssemblySolveDiagnosticsAnalyzer::analyze` does not:

- mutate the input project
- mutate component transforms
- change component state
- rewrite constraints or target order
- change part intent
- persist numeric Jacobians
- persist rank or DOF values
- persist local classifications

The analysis is regenerated from persistent model intent, current component placement/state, semantic target geometry, canonical residual conventions, and configured numeric policy.

No assembly/project JSON field is added.

## Tests

Core diagnostics suite:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-diagnostics]"
```

Concentric numeric/solver/diagnostic integration suite:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric-solver]"
```

Coverage includes:

- Mate rank `3/6`
- Distance rank `3/6`
- three orthogonal Mates rank `6/6`
- two-Mate chain rank `6/12`
- deterministic ordering
- all-grounded consistency/inconsistency
- non-convergence propagation
- rank tolerance
- duplicate residual-row redundancy
- read-only behavior
- semantic target failure propagation
- Concentric rank `4/6`
- Concentric remaining DOF `2`
- Concentric residual-row redundancy `2`
- deterministic repeated Concentric analysis
- mixed Distance/Concentric residual dimension `10`
- mixed Distance/Concentric rank `5/6`
- mixed remaining DOF `1`
- fixed inconsistent Concentric diagnostics without false rank
- non-converged Concentric diagnostics without false rank

## Deliberate limitations

This diagnostics block does not implement:

- global configuration-space dimension analysis
- singularity classification
- analytic Jacobians
- SVD or sparse rank factorization
- per-component DOF labels
- a null-space basis for drag projection
- semantic overconstraint classification for arbitrary conflicting free-body constraints
- constraint-removal recommendations
- persistent DOF caches
- Insert diagnostics
- joints or motion

## Next technical step

The next assembly block is stable Insert constraint intent and a read-only composite Insert residual model.

The first Insert residual contract should combine stable axis-line alignment with explicit semantic axial seating geometry and prove regular rank five with one remaining rotation-about-axis DOF before Insert enters the shared numeric solver and diagnostics path.

# Assembly Solve Diagnostics and Remaining DOF MVP-5

Status: implemented read-only local Jacobian-rank and remaining-degree-of-freedom diagnostics for the shared Mate, Distance, and Concentric rigid-body numeric path. Insert residual semantics and a direct rank-five proof exist separately; shared Insert diagnostics integration is the next block.

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

Constraint-family integrations do not add a second diagnostics API.

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

Current integrated numeric families are:

```text
Mate
Distance
Concentric
```

Insert is not yet integrated into this shared path.

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

An unsupported numeric family such as current Insert fails through the solver/shared-numeric boundary and therefore produces no false rank classification.

## Local Jacobian rank convention

Let:

```text
J = m x n numeric Jacobian
m = residual component count
n = free rigid-body variable count
```

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

Rank is computed deterministically by row-echelon elimination with left-to-right columns and maximum-magnitude row pivot selection in the current column.

Solver damping is excluded from rank because damping is solve stabilization and must not create artificial constrained directions.

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

These are local rank/nullity values in direct persisted `RigidTransform` coordinates.

They do not claim global configuration-space dimension across singularities, alternate assembly modes, or semantic-reference changes.

## DOF classification

### NotEvaluated

Used when no locally converged integrated numeric state is suitable for rank-based classification.

Examples:

```text
FixedGeometryInconsistent
MaximumIterationsReached
NumericalFailure
unsupported shared numeric constraint family
```

### NoVariableDof

Used for a converged group with zero free components. The absence of variables comes from grounding policy and is not a claim that constraints alone fully constrain bodies.

### Underconstrained

```text
solve_state == Converged
variable_count > 0
remaining_dof > 0
```

Proven shared-path examples:

```text
one Mate:
  rank 3/6
  remaining 3

one Concentric:
  rank 4/6
  remaining 2

one aligned Distance + one Concentric:
  rank 5/6
  remaining 1
```

### LocallyFullyConstrained

```text
solve_state == Converged
variable_count > 0
remaining_dof == 0
```

Three orthogonal planar Mates provide the first proven rank-six case.

## Consistency classification

DOF classification and consistency remain separate.

### LocallyConsistent

The supported numeric system converged and rank was evaluated at the private converged state.

### FixedGeometryInconsistent

An all-grounded supported numeric group has residual RMS above convergence tolerance.

The analyzer preserves the solver state and does not invent a DOF classification.

### SolverDidNotConverge

Used for `MaximumIterationsReached` and `NumericalFailure`.

Residual summary/order remain visible but `rank_evaluated` is false.

## Residual row rank is not semantic overconstraint

For an evaluated Jacobian:

```text
residual_row_redundancy = residual_component_count - jacobian_rank
```

The result distinguishes:

```text
FullRowRank
RedundantResidualComponents
```

`RedundantResidualComponents` means flattened residual rows are not all linearly independent at the evaluated state.

It does not mean the assembly is semantically overconstrained.

Vector residuals commonly contain more scalar rows than independent geometric sensitivities.

## Proven Concentric rank behavior

Canonical residual:

```text
direction_parallelism = cross(dA, dB)
axis_offset_mm         = cross(oB - oA, dA)
```

A regular one-free-body Concentric relationship constrains two tilt rotations and two lateral translations. It leaves axial translation and axis rotation free.

The actual shared Jacobian proves:

```text
residual_component_count = 6
variable_count           = 6
jacobian_rank            = 4
constrained_dof          = 4
remaining_dof            = 2
residual_row_redundancy  = 2
```

There is no `if Concentric then rank = 4` rule.

## Proven mixed Distance plus Concentric behavior

For one Concentric relationship and one planar Distance whose normal follows the common axis:

```text
residual_component_count = 10
variable_count           = 6
jacobian_rank            = 5
constrained_dof          = 5
remaining_dof            = 1
```

Distance adds axial separation while its orientation sensitivities overlap the existing Concentric tilt sensitivities.

The remaining local freedom is rotation about the common axis.

## Insert residual rank seed

Canonical Insert document: `docs/assembly-insert-intent-composite-residuals-mvp5.md`.

Insert residuals are implemented read-only:

```text
direction_parallelism       = cross(dA, dB)
axis_offset_mm               = cross(oB - oA, dA)
signed_seating_separation_mm = dot(sB - sA, nA)
```

The first two fields carry the four regular Concentric sensitivities. The seating scalar adds axial translation sensitivity.

A focused geometry test directly central-finite-differences the seven raw Insert scalar residuals over all six direct transform variables and applies deterministic row-echelon rank evaluation.

Proven regular result:

```text
residual_component_count = 7
variable_count           = 6
jacobian_rank            = 5
remaining_local_dof      = 1
```

Pure rotation about the common axis remains a zero-residual state.

This is an architecture seed, not an analyzer result. Because Insert is not yet flattened by `AssemblyConstraintNumericSystem`, `AssemblySolveDiagnosticsAnalyzer` does not currently report Insert rank.

The next integration block must reproduce rank `5/6` through the exact shared solver/diagnostics Jacobian rather than hard-code it from the Insert type.

## Deterministic ordering

Diagnostics preserve:

```text
component_group
fixed_components
variable_components
constraint_order
```

Variable components use solver lexicographic order.

Constraints use graph lexicographic `AssemblyConstraintId` order.

Mixed integrated constraint families do not change this ordering.

Repeated analysis of the same project/options produces equal descriptors.

## Semantic target failure propagation

Diagnostics consume the solver/shared numeric path rather than pre-validating semantic targets independently.

Unsupported semantic targets propagate their geometry-layer error unchanged.

Future Insert integration must preserve this behavior for malformed or unsupported `.seat` targets.

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

Analysis is regenerated from current persistent model intent, placement/state, semantic target geometry, canonical residual conventions, and configured numeric policy.

No assembly/project JSON field is added.

The direct Insert rank proof is also unpersisted derived test evidence.

## Tests

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-diagnostics]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-insert]"
```

Coverage includes Mate rank `3/6`, Distance rank `3/6`, three orthogonal Mates rank `6/6`, two-Mate chain rank `6/12`, deterministic ordering, all-grounded consistency/inconsistency, non-convergence propagation, rank tolerance, duplicate residual-row redundancy, read-only behavior, semantic target failures, Concentric rank `4/6`, mixed Distance+Concentric rank `5/6`, and the separate direct Insert rank-five seed.

## Next technical step

The next diagnostics increment is Insert integration through the one shared numeric system and solver.

After successful Insert solve integration, `AssemblySolveDiagnosticsAnalyzer` must evaluate the same seven-scalar Insert residual/Jacobian path and prove:

```text
variable_count = 6
jacobian_rank  = 5
remaining_dof  = 1
```

No Insert-specific DOF table or hard-coded nominal constraint count should be added.

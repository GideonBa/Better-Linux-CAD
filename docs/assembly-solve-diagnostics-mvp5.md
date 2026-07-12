# Assembly Solve Diagnostics and Remaining DOF MVP-5

Status: implemented read-only local Jacobian-rank and remaining-degree-of-freedom diagnostics for the shared Mate, Distance, Concentric, Insert, and Angle rigid-body numeric path. Block 27 additionally reuses the same matrix-rank implementation for authority-scoped cross-hierarchy diagnostics.

## Goal

The local analyzer answers:

```text
At the current locally solved assembly state,
how many independent direct rigid-body variable directions are constrained
by the supported active local numeric relationship system,
and how many local DOF remain?
```

The analyzer reuses the deterministic residual, variable, and central finite-difference Jacobian semantics used by `AssemblyRigidBodySolver`.

It does not infer DOF from relationship type names.

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

Block 27 adds a separate `AssemblyCrossHierarchySolveDiagnosticsAnalyzer` for transform-authority groups, but both analyzers reuse the same `AssemblyJacobianRankSummary`, classifications, option contract, and matrix-rank implementation.

## Shared local numeric-system path

Local solver and diagnostics share:

```text
src/geometry/assembly_constraint_numeric_system.hpp
src/geometry/assembly_constraint_numeric_system.cpp
```

The local path owns deterministic constraint-id collection, all five geometric residual-builder selections, canonical scalar flattening, residual summaries, free-component variable extraction/application on Project copies, and central finite-difference Jacobian construction.

Current integrated local numeric families are:

```text
Mate
Distance
Concentric
Insert
Angle
```

The analyzer does not maintain a second local residual or Jacobian interpretation.

## Analysis pipeline

For one exact deterministic local connected group:

```text
Project + connected group
  -> AssemblyRigidBodySolver::solve
  -> preserve solve state and residual summary
  -> collect deterministic active constraint order
  -> FixedGeometryInconsistent:
       explicit inconsistency
       no rank claim
  -> MaximumIterationsReached or NumericalFailure:
       explicit non-convergence
       no rank claim
  -> Converged:
       apply fresh proposals to a private Project copy
       evaluate shared variables/residuals/Jacobian
       compute Jacobian rank
       compute constrained and remaining DOF
       classify local variable state
```

The source Project remains unchanged.

Because diagnostics apply the solve result to a private Project copy through `AssemblySolveResultApplier`, exact component and canonical semantic target PartDocument freshness is validated before the evaluation pose is accepted.

## Shared matrix-rank implementation

Local and Block-27 cross-hierarchy diagnostics now call:

```text
compute_assembly_matrix_rank
```

The utility receives a finite dense Jacobian, expected column count, absolute tolerance, relative tolerance, and diagnostic object id.

Let:

```text
J = m x n numeric Jacobian
m = residual component count
n = variable count
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

Solver damping is excluded because damping is numerical stabilization and must not create artificial constrained directions.

## Local variable order and DOF counts

Each free active local component contributes:

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
variable_count = 6 * free_active_component_count
```

For an evaluated Jacobian:

```text
constrained_dof = jacobian_rank
remaining_dof   = variable_count - jacobian_rank
```

These are local rank/nullity values in direct persisted `RigidTransform` coordinates.

They do not claim global configuration-space dimension across singularities, alternate assembly modes, or semantic-reference changes.

Cross-hierarchy diagnostics use a different variable-owner identity (`ComponentTransformAuthority`) but the same six-variable block layout and shared rank utility.

## DOF classification

### NotEvaluated

Used when no converged numeric state is suitable for rank-based classification:

```text
FixedGeometryInconsistent
MaximumIterationsReached
NumericalFailure
```

### NoVariableDof

Used for a converged group with zero free variables. The absence of variables comes from grounding policy and is not a claim that relationships alone constrain all bodies.

### Underconstrained

```text
solve_state == Converged
variable_count > 0
remaining_dof > 0
```

Covered regular one-free-body examples:

```text
one Mate:
  rank 3/6
  remaining 3

one Distance:
  rank 3/6
  remaining 3

one Concentric:
  rank 4/6
  remaining 2

one Insert:
  rank 5/6
  remaining 1

one non-extremal Angle:
  rank 1/6
  remaining 5

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

Three orthogonal planar Mates provide a covered rank-six local case.

`LocallyFullyConstrained` means full column rank in the finite-difference Jacobian at the evaluated pose.

## Consistency classification

DOF classification and consistency remain separate.

### LocallyConsistent

The supported numeric system converged and rank was evaluated on a private converged Project copy.

### FixedGeometryInconsistent

An all-grounded active numeric group remains above convergence tolerance.

The analyzer preserves the solver state and does not invent a DOF classification.

### SolverDidNotConverge

Used for `MaximumIterationsReached` and `NumericalFailure`.

Residual summary and deterministic relationship order remain visible, but `rank_evaluated` is false.

## Residual row rank is not semantic overconstraint

For an evaluated Jacobian:

```text
residual_row_redundancy = residual_component_count - jacobian_rank
```

Classification:

```text
FullRowRank
RedundantResidualComponents
```

`RedundantResidualComponents` means flattened scalar residual rows are not all linearly independent at the evaluated state.

It does not mean the assembly is semantically overconstrained.

Vector residuals often contain more scalar rows than independent geometric sensitivities.

## Proven family behavior

### Mate / Distance

A regular planar relationship constrains one signed separation translation and two tilt rotations:

```text
residual components = 4
variables = 6
rank = 3
remaining DOF = 3
residual row redundancy = 1
```

### Concentric

Canonical residual:

```text
direction_parallelism = cross(dA, dB)
axis_offset_mm         = cross(oB - oA, dA)
```

A regular one-free-body Concentric relationship constrains two tilt rotations and two lateral translations. Axial translation and axis rotation remain free:

```text
residual components = 6
variables = 6
rank = 4
remaining DOF = 2
residual row redundancy = 2
```

### Insert

Canonical residual:

```text
direction_parallelism       = cross(dA, dB)
axis_offset_mm               = cross(oB - oA, dA)
signed_seating_separation_mm = dot(sB - sA, nA)
```

The seating scalar adds axial translation sensitivity to the four regular Concentric sensitivities:

```text
residual components = 7
variables = 6
rank = 5
remaining DOF = 1
```

Pure rotation about the common axis remains free.

Insert is integrated into the shared numeric and diagnostics paths; this is no longer only a standalone rank seed.

### Angle

The current planar Angle seed uses:

```text
angle_alignment = dot(nA, nB) - cos(target_angle_deg)
```

At a regular non-extremal target such as `90 deg`, one independent rotational sensitivity is present:

```text
residual components = 1
variables = 6
rank = 1
remaining DOF = 5
```

At cosine extrema (`0 deg`, `180 deg`) the first-order derivative can be singular. Diagnostics report the actual finite-difference Jacobian rank at the evaluated pose rather than hard-coding one DOF from the family name.

### Distance plus Concentric

For one Concentric relationship and one planar Distance whose normal follows the common axis:

```text
residual components = 10
variables = 6
rank = 5
remaining DOF = 1
```

Distance adds axial separation while orientation sensitivities overlap Concentric tilt sensitivities. Rotation about the common axis remains free.

## Deterministic ordering

Local diagnostics preserve:

```text
component_group
fixed_components
variable_components
constraint_order
```

`variable_components` follow the exact local solver proposal/variable order.

`constraint_order` follows the deterministic active local graph/numeric relationship order.

Insertion-order changes do not change diagnostics.

Cross-hierarchy diagnostics preserve separate authority and relationship identities and are canonical in `docs/assembly-cross-hierarchy-application-diagnostics-mvp5.md`.

## Failure policy

Diagnostics fail closed on invalid options, solver validation/target-resolution failures, stale private result application, residual dimension changes during central finite differences, non-finite Jacobian entries, or Jacobian row-width mismatch.

A valid non-converged solve state is returned as diagnostics with `rank_evaluated = false`; it is not converted into a generic error.

## Persistence boundary

Persist model intent, not diagnostic products.

Derived and unpersisted local diagnostics data includes:

```text
solved evaluation Project copy
free-component variable order
scaled residual vector
finite-difference Jacobian
matrix rank
pivot threshold
constrained/remaining DOF
residual row redundancy
classification values
```

The shared matrix-rank utility itself stores no cache.

## Focused coverage

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-diagnostics]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-diagnostics]"
```

Local coverage proves option validation, deterministic order, Mate/Distance rank `3/6`, Concentric rank `4/6`, Insert rank `5/6`, non-extremal Angle rank `1/6`, mixed Distance+Concentric rank `5/6`, rank-six full constraint, all-grounded consistency/inconsistency, non-convergence classification, residual row redundancy semantics, and source Project immutability.

Cross-hierarchy coverage proves authority-based variable counting, repeated-occurrence shared authority semantics, mixed local/cross relationship ordering, and reuse of the same matrix-rank contract.

## Current boundary

Local diagnostics remain local to one temporary solve view's root `AssemblyDocument` and count free local components.

Cross-hierarchy diagnostics count unique free `ComponentTransformAuthority` values.

Neither analyzer persists a rank/null-space cache or exposes null-space basis vectors.

## Next diagnostics-adjacent work

Block 28 is cross-hierarchy Revolute motion, not a new rank algorithm.

Any motion diagnostics introduced there must reuse the existing shared numeric Jacobian and matrix-rank semantics rather than infer constrained directions from joint family names.

Per-authority/null-space basis presentation and free-motion direction extraction remain deferred.

# Assembly Solve Diagnostics and Remaining DOF MVP-5

Status: implemented read-only local Jacobian-rank and remaining-degree-of-freedom diagnostics for the supported planar Mate/Distance rigid-body solve path.

## Goal

This block answers a question that the first rigid-body solver deliberately left open:

```text
At the current locally solved assembly state, how many independent rigid-body variable directions are constrained by the supported active constraints, and how many local DOF remain?
```

The implementation reuses the exact deterministic residual, variable, and central finite-difference Jacobian model used by `AssemblyRigidBodySolver`.

It does not create a second equation interpretation and does not infer DOF from constraint type names.

The result is a local linearized diagnostic at the evaluated transform state. It is regenerable derived data and is not persisted.

## API

The public geometry-layer API lives in:

```text
include/blcad/geometry/assembly_solve_diagnostics.hpp
```

Main types:

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

AssemblySolveDiagnostics
  solve_state
  dof_classification
  consistency_classification
  residual_rank_structure
  component_group
  fixed_components
  variable_components
  constraint_order
  rank_summary
  residual_summary

AssemblySolveDiagnosticsAnalyzer
  analyze(Project, connected_group, options)
```

## Shared numeric-system path

The solver and diagnostics now share one private geometry-layer numeric-system implementation:

```text
src/geometry/assembly_constraint_numeric_system.hpp
src/geometry/assembly_constraint_numeric_system.cpp
```

That implementation owns the common numeric interpretation of the supported assembly equations:

```text
collect deterministic constraint ids
flatten Mate/Distance residual descriptors
read free-component variables
apply numeric variable vectors to a Project copy
build the central finite-difference Jacobian
compute residual RMS and maximum absolute residual
```

The first solver was refactored to call this shared path.

The diagnostics analyzer calls the same path.

Therefore the DOF result is based on the same:

- lexicographic connected-group component order
- lexicographic `AssemblyConstraintId` order
- four residual components per supported planar constraint
- orientation-first and scaled-length-last residual flattening
- six `RigidTransform` variables per free component
- translation and rotation finite-difference steps
- component transform evaluation convention

No duplicate Jacobian implementation exists in the diagnostics block.

## Analysis pipeline

For one exact deterministic connected group:

```text
Project + connected group
  -> AssemblyRigidBodySolver::solve
  -> preserve solve state and residual summary
  -> collect deterministic constraint order
  -> if FixedGeometryInconsistent:
       return explicit inconsistency diagnostic
       do not claim DOF rank
  -> if MaximumIterationsReached or NumericalFailure:
       return explicit non-convergence diagnostic
       do not claim DOF rank
  -> if Converged:
       apply proposals to a private Project copy
       evaluate shared numeric variables/residuals/Jacobian
       compute local Jacobian rank
       compute constrained and remaining DOF
       classify the local variable state
```

The input `Project` remains unchanged.

The converged solver result is applied only to the analyzer's private project copy. The existing `AssemblySolveResultApplier` remains the transform-application contract and validates the solver snapshots before the diagnostic state is evaluated.

## Local Jacobian rank convention

Let:

```text
J = m x n numeric Jacobian
m = residual component count
n = free rigid-body variable count
```

The diagnostics first compute:

```text
maximum_abs_jacobian_entry = max(abs(J[i,j]))
```

The pivot threshold is:

```text
pivot_threshold = max(
  rank_absolute_tolerance,
  rank_relative_tolerance * maximum_abs_jacobian_entry
)
```

Default values:

```text
rank_absolute_tolerance = 1.0e-12
rank_relative_tolerance = 1.0e-8
```

Both tolerances must be finite and non-negative. They may not both be zero.

Rank is computed deterministically by row-echelon elimination with:

- columns scanned from left to right
- maximum-magnitude row pivot selection within the current column
- row swaps only
- a pivot accepted only when its magnitude is strictly greater than `pivot_threshold`
- forward elimination below each accepted pivot

The diagnostic block does not use solver damping when computing rank. Damping is a solve-stabilization mechanism and must not create artificial constrained directions in the rank result.

## DOF counts

Each free component contributes exactly six local variables:

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

For a successfully evaluated Jacobian:

```text
constrained_dof = jacobian_rank
remaining_dof   = variable_count - jacobian_rank
```

`constrained_dof` is the number of locally independent numeric constraint directions detected in the current Jacobian.

`remaining_dof` is the Jacobian nullity in the solver's current direct `RigidTransform` variable coordinates.

These are local linearized values. They do not claim a global configuration-space dimension across singularities, alternate assembly modes, or discontinuous semantic-reference changes.

## DOF classification

### NotEvaluated

Used when the solver did not provide a locally converged state suitable for rank-based DOF classification.

This includes:

```text
FixedGeometryInconsistent
MaximumIterationsReached
NumericalFailure
```

The original `AssemblySolveState` remains present in the diagnostic descriptor.

### NoVariableDof

Used for a converged connected group with zero free components.

Every component is grounded and therefore fixed by the first solver participation policy.

The numeric variable count and remaining DOF are both zero.

This does not mean the constraints themselves fully constrained the bodies. The absence of solver variables is caused by grounding policy.

### Underconstrained

Used when:

```text
solve_state == Converged
variable_count > 0
remaining_dof > 0
```

Example: one planar Mate between one grounded component and one free component.

Locally it constrains:

- two rotational directions that change the target normal
- one translation direction normal to the plane

The free component still has:

- two tangential translations
- one rotation about the plane normal

For the current numeric system:

```text
variable_count = 6
jacobian_rank = 3
remaining_dof = 3
```

### LocallyFullyConstrained

Used when:

```text
solve_state == Converged
variable_count > 0
remaining_dof == 0
```

The local numeric Jacobian has full column rank.

The wording intentionally includes `Locally`. This block does not claim global uniqueness of the assembly configuration.

Three orthogonal planar Mates between one grounded box and one free box provide the first focused test case:

```text
variable_count = 6
jacobian_rank = 6
remaining_dof = 0
```

## Consistency classification

DOF classification and consistency are separate fields.

### LocallyConsistent

The solver converged and the Jacobian was evaluated at the private converged transform state.

### FixedGeometryInconsistent

The solver found an all-grounded group whose residual RMS remains above the configured convergence tolerance.

No body is movable under the current grounding policy, so the diagnostics preserve the explicit solver state and do not compute a misleading DOF classification.

### SolverDidNotConverge

Used when the solver returns:

```text
MaximumIterationsReached
NumericalFailure
```

The diagnostics preserve the solver residual summary and deterministic group/constraint/variable ordering, but `rank_evaluated` is false.

A Jacobian rank at a known non-converged intermediate state could be inspected by a future lower-level numeric debugging API, but this MVP does not label that rank as remaining assembly DOF.

## Residual rank structure is not semantic overconstraint

For an evaluated Jacobian:

```text
residual_row_redundancy = residual_component_count - jacobian_rank
```

The descriptor reports:

```text
FullRowRank
RedundantResidualComponents
```

when rank evaluation succeeds.

`RedundantResidualComponents` means only that the flattened residual rows are not all linearly independent at the evaluated state.

It does not mean the assembly is semantically overconstrained.

Examples of benign row dependence include:

- the three-component unit-normal residual vector having only two local rotational directions
- duplicate constraints
- algebraically redundant relationships
- constraint combinations whose current local sensitivities overlap

The first diagnostics block therefore does not expose an `Overconstrained` DOF state.

Semantic overconstraint requires a stronger inconsistency/redundancy model than:

```text
residual_count > jacobian_rank
```

The explicit current inconsistency boundary is `FixedGeometryInconsistent`. More general conflicting free-body constraint diagnostics remain future work.

## Deterministic ordering

The descriptor preserves:

```text
component_group
fixed_components
variable_components
constraint_order
```

The variable component order is the same lexicographic order used by the solver.

The constraint order is the graph's lexicographic `AssemblyConstraintId` order.

Repeated analysis of the same project state and options produces equal diagnostic descriptors.

Constraint insertion order does not change the numeric ordering or rank result.

## Read-only and persistence boundary

`AssemblySolveDiagnosticsAnalyzer::analyze` does not:

- mutate the input project
- mutate component transforms
- change grounding, suppression, or visibility
- rewrite constraints or target order
- modify part parameters or feature intent
- persist the numeric Jacobian
- persist rank values
- persist constrained or remaining DOF
- persist local DOF classification
- persist residual rank structure
- persist solver residual summaries

The analysis is regenerated from:

```text
persistent project/assembly/part intent
+ current component transforms and state
+ deterministic active-constraint graph
+ supported semantic target geometry
+ canonical transform convention
+ canonical planar residual conventions
+ solver and rank options
```

No assembly or project JSON field is added.

## Tests

`tests/geometry/assembly_solve_diagnostics_tests.cpp` covers:

- one planar Mate with rank 3 over 6 variables and 3 remaining DOF
- one planar Distance with rank 3 over 6 variables and 3 remaining DOF
- read-only analysis preserving the source component transform
- one free component constrained by three orthogonal Mates with rank 6 and zero remaining DOF
- a two-Mate three-component chain with rank 6 over 12 variables
- deterministic diagnostics independent from constraint insertion order
- deterministic constraint ordering
- a consistent all-grounded group with `NoVariableDof`
- explicit `FixedGeometryInconsistent` propagation
- explicit non-converged solver-state propagation without DOF classification
- duplicate Mate constraints producing residual-row redundancy without semantic overconstraint classification
- configurable rank tolerance changing the accepted local rank
- rejection of an invalid all-zero rank tolerance pair
- unsupported Concentric diagnostic propagation
- stable string labels for the new diagnostic enums

Targeted test command after a geometry build:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-diagnostics]"
```

Complete geometry workflow:

```bash
cmake --workflow --preset dev-geometry-build-test
```

## Deliberate limitations

This block does not implement:

- a global configuration-space dimension analysis
- singularity classification
- analytic Jacobians
- singular-value decomposition
- sparse rank factorization
- per-component DOF labels such as `free_tx` or `free_rz`
- a null-space basis for drag projection
- semantic overconstraint classification for arbitrary conflicting free-body constraints
- constraint removal recommendations
- persistent DOF caches
- solved-state JSON caches
- semantic axis references
- Concentric target geometry or residuals
- Concentric solving
- Insert or richer constraint families
- joints or motion
- collision/interference analysis
- subassemblies
- component geometry instancing
- assembly-level STEP export

## Next technical step

The next assembly block is a semantic generated-axis reference family and read-only Concentric target/residual pipeline.

That block should define stable axis tokens produced by supported feature intent, resolve component-local axis descriptors through project-owned part documents, evaluate axes in assembly space with the existing transform convention, and construct canonical Concentric residual descriptors without raw OCCT topology ids.

Only after the semantic-axis and Concentric residual conventions are stable should `AssemblyRigidBodySolver` consume Concentric constraints. Insert remains downstream because it combines axis alignment with axial seating semantics.

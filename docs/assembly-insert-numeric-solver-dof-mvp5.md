# Insert Numeric Solver and DOF Integration MVP-5

Status: implemented Insert integration into the shared assembly numeric residual/Jacobian system, deterministic rigid-body solver, explicit solve-result application path, and read-only local Jacobian-rank/remaining-DOF diagnostics.

## Goal

This block makes the stable composite Insert residual a first-class numeric assembly constraint without creating a second solver path.

```text
persistent active Insert constraint
  -> feature.<feature-id>.seat resolution
  -> component-local semantic axis plus oriented seating plane
  -> assembly-space axis/seat evaluation
  -> composite Insert residual descriptor
  -> shared numeric residual flattening
  -> shared central finite-difference Jacobian
  -> existing damped Gauss-Newton solver
  -> proposed transforms
  -> explicit atomic result application
  -> shared Jacobian-rank / remaining-DOF diagnostics
```

No Insert-specific optimizer, transform representation, Jacobian implementation, or persistence cache exists.

## Semantic geometry contract

Canonical document: `docs/assembly-insert-intent-composite-residuals-mvp5.md`.

Target family `feature.<feature-id>.seat`, first producer one `SubtractiveExtrude` whose source sketch contains exactly one `CircleProfile` and exactly one total profile. Composite residual:

```text
direction_parallelism        = cross(dA, dB)
axis_offset_mm               = cross(oB - oA, dA)
signed_seating_separation_mm = dot(sB - sA, nA)
```

This integration block does not change those geometric semantics. Equal and opposed aligned axis directions stay accepted, and rotation about the common axis stays free in the regular case.

## Shared numeric-system selection

Builder selection inside the one shared residual evaluator:

```text
Concentric -> AssemblyConcentricConstraintEquationBuilder
Insert     -> AssemblyInsertConstraintEquationBuilder
otherwise  -> AssemblyConstraintEquationBuilder (planar Mate/Distance)
```

Exact Insert flattening order:

```text
direction_parallelism.x
direction_parallelism.y
direction_parallelism.z
axis_offset_mm.x / length_residual_scale_mm
axis_offset_mm.y / length_residual_scale_mm
axis_offset_mm.z / length_residual_scale_mm
signed_seating_separation_mm / length_residual_scale_mm
```

Insert reuses the lexicographic graph constraint order, persistent target A/B order, six-variable free-component order, central finite differences, damped Gauss-Newton with damping escalation and line search, solve states, grounding and suppression policy, source-project immutability, complete snapshots, stale-result detection, and the explicit atomic application boundary — all unchanged.

## Proven regular one-free-body result

```text
variable_count           = 6
residual_component_count = 7
jacobian_rank            = 5
constrained_dof          = 5
remaining_dof            = 1
residual_row_redundancy  = 2
```

The one remaining DOF is rotation about the common axis. Mixed Concentric/Insert on the same axis stays rank `5` with `13` residual components and redundancy `8`, deterministically ordered regardless of constraint insertion order.

## Covered by tests

`tests/geometry/assembly_insert_numeric_solver_tests.cpp`:

- deterministic mixed planar/Insert flattening with exact expected residual values and `11 x 6` Jacobian shape.
- solver corrects lateral offset and axial seating while preserving rotation about the axis and the source project.
- solver corrects axis tilt plus seating through the shared finite-difference path; applied result has a near-zero Insert residual.
- equal and opposed aligned axis directions converge with zero iterations and unchanged transforms.
- offset grounded seats classify as fixed-geometry inconsistent with seven residual components and no proposals.
- non-converged results stay read-only and unapplable.
- diagnostics prove rank `5/6` with one remaining DOF and redundancy `2`, twice, with identical results.
- mixed Concentric/Insert diagnostics are insertion-order independent.

`tests/geometry/assembly_insert_constraint_equation_builder_tests.cpp` now asserts that the shared solver solves Insert instead of rejecting it.

## Still not implemented

- richer constraint families beyond Mate/Distance/Concentric/Insert (an Angle family is the next block).
- suppressed components inside a solved group.
- joints, limits, motion, subassemblies, collision checks, and assembly-level STEP export.
- persistent solve/DOF caches (derived data stays regenerable per the persistence rule).

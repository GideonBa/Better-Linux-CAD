# Insert Numeric Solver and DOF Integration MVP-5

Status: implemented Insert integration into the shared assembly numeric residual/Jacobian system, deterministic local rigid-body solver, explicit solve-result application path, and read-only local Jacobian-rank/remaining-DOF diagnostics.

## Goal

This block makes the stable composite Insert residual a first-class numeric local assembly constraint without creating a second solver path.

```text
persistent active Insert constraint
  -> feature.<feature-id>.seat resolution
  -> component-local semantic axis + oriented seating plane
  -> assembly-space axis/seat evaluation
  -> composite Insert residual descriptor
  -> shared numeric residual flattening
  -> shared central finite-difference Jacobian
  -> existing damped Gauss-Newton solver
  -> proposed direct component transforms
  -> explicit atomic result application
  -> shared Jacobian-rank / remaining-DOF diagnostics
```

No Insert-specific optimizer, transform representation, Jacobian implementation, or persistence cache exists.

## Semantic geometry contract

Canonical document: `docs/assembly-insert-intent-composite-residuals-mvp5.md`.

Target family:

```text
feature.<feature-id>.seat
```

The first producer is one `SubtractiveExtrude` whose source sketch contains exactly one `CircleProfile` and one total profile.

Composite residual:

```text
direction_parallelism       = cross(dA, dB)
axis_offset_mm               = cross(oB - oA, dA)
signed_seating_separation_mm = dot(sB - sA, nA)
```

Equal and opposed aligned axis directions are accepted. Rotation about the common axis remains free in the regular case.

## Shared numeric-system selection

Builder selection inside the shared local residual evaluator:

```text
Concentric -> AssemblyConcentricConstraintEquationBuilder
Insert     -> AssemblyInsertConstraintEquationBuilder
otherwise  -> AssemblyConstraintEquationBuilder
```

The final path now also includes the later planar Angle family through `AssemblyConstraintEquationBuilder`.

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

Insert reuses local graph ordering, persistent target A/B order, six-variable free-component ordering, central finite differences, damped Gauss-Newton solving, damping escalation, line search, solve states, grounding/suppression participation, source-project immutability, complete snapshots, stale-result validation, and atomic application.

## Proven regular one-free-body result

```text
variable_count           = 6
residual_component_count = 7
jacobian_rank            = 5
constrained_dof          = 5
remaining_dof            = 1
residual_row_redundancy  = 2
```

The one remaining DOF is rotation about the common axis.

Mixed Concentric/Insert on the same axis remains rank `5` with `13` residual components and redundancy `8`, deterministically ordered regardless of constraint insertion order.

## Covered by tests

`tests/geometry/assembly_insert_numeric_solver_tests.cpp` proves:

- deterministic mixed planar/Insert flattening with exact residual values and `11 x 6` Jacobian shape;
- lateral offset and axial seating correction while preserving axis rotation and the source project;
- tilt plus seating correction through the shared finite-difference path;
- equal/opposed aligned axis directions converging with zero iterations;
- fixed-geometry inconsistency for offset grounded seats;
- read-only non-converged results;
- rank `5/6` with one remaining DOF and redundancy `2`;
- insertion-order-independent mixed Concentric/Insert diagnostics.

`tests/geometry/assembly_insert_constraint_equation_builder_tests.cpp` verifies Insert reaches the shared solver path.

## Persistence boundary

Insert adds no solve cache or numeric persistence.

Persistent authority remains the existing local `AssemblyConstraint` record and, after explicit successful application, the affected `ComponentInstance::transform()` records.

Derived data includes resolved axis/seat geometry, residual descriptors, flattened scalar residuals, finite-difference Jacobians, solve states, snapshots, proposals, and DOF diagnostics.

## Current boundaries

Implemented after this block in separate canonical documents:

- planar Angle constraint integration;
- suppressed-component filtering;
- posed assembly STEP export;
- Revolute joint/limit motion;
- rigid nested subassemblies;
- posed interference/clearance analysis;
- document-scoped flexible child solving;
- read-only cross-hierarchy target/residual semantics.

Still deferred from the current architecture:

- richer geometric relationship families beyond Mate/Distance/Concentric/Insert/Angle;
- oriented signed planar Angle semantics;
- persistent solve/DOF caches;
- occurrence-local flexible child poses;
- project-level persistent cross-hierarchy constraints and solving, now sequenced in `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`;
- cross-hierarchy joints and nested motion propagation.

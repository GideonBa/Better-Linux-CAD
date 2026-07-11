# Planar Angle Constraint Family MVP-5

Status: implemented the first richer assembly constraint family — a planar Angle constraint with a persistent angle value, integrated into the shared numeric residual/Jacobian system, rigid-body solver, explicit application path, and DOF diagnostics in one block.

## Intent

```text
AssemblyConstraint
  type    = angle
  target_a = feature.<feature-id>.<face> on component A
  target_b = feature.<feature-id>.<face> on component B
  angle    = signed angle in degrees (required, AngleDeg quantity)
```

- `QuantityKind::AngleDeg` with `Quantity::angle_deg` validates finite signed degree values (unit `"deg"`, accessor `degrees()`).
- Angle constraints require an angle value and reject length and count quantities; every other family rejects angle values (mirroring the distance rule).
- JSON spelling `angle` with `"angle": {"unit": "deg", "value": ...}`; roundtrip does not change the assembly schema shape.

## Residual

Both targets resolve through the existing generated-face planar resolution and the shared transform evaluator. The dedicated read-only residual is one dimensionless scalar:

```text
normal_dot      = dot(nA, nB)
angle_alignment = normal_dot - cos(target_angle_deg)
```

A satisfied Angle has `angle_alignment = 0`. No length scaling applies during flattening; the component enters the shared residual vector as-is, in the same lexicographic constraint order as every family:

```text
angle_alignment
```

`PlanarAngleResidualDescriptor` joins the planar variant next to Mate and Distance; the builder, numeric system, solver, application path, and diagnostics all stay single-path.

## Semantics and limits of the seed

- the residual constrains only the relative normal orientation (one DOF in the regular case); position is free.
- the cosine form is direction-symmetric: an angle of `θ` and `-θ` are the same state. Signed/oriented angle measurement around a reference axis is a later extension.
- at extremal targets (`0°`, `180°`) the residual gradient vanishes at the solution, so those targets contribute no rank at the solved state; diagnostics report that honestly as redundancy instead of hiding it.

## Proven regular one-free-body result

```text
variable_count           = 6
residual_component_count = 1
jacobian_rank            = 1
constrained_dof          = 1
remaining_dof            = 5
residual_row_redundancy  = 0   (full row rank)
```

## Covered by tests

`tests/core/assembly_angle_constraint_tests.cpp`:

- angle quantity validation (finite, signed, unit `"deg"`, kind checks against length/count).
- intent validation matrix: missing angle, wrong quantity kind, distance-on-angle, angle-on-mate.
- JSON roundtrip with type `angle`, unit `deg`, inactive state, and unchanged placements.

`tests/geometry/assembly_angle_numeric_solver_tests.cpp`:

- residual values against expected cosines for aligned and rotated targets.
- deterministic mixed Distance/Angle flattening with exact expected values (`5` components).
- solver converges a tilted free component to the target angle; applied result has near-zero alignment; source project unchanged; deterministic double-solve.
- already-satisfied target converges with zero iterations and unchanged transforms.
- grounded wrong-angle pairs classify as fixed-geometry inconsistent.
- non-converged results stay read-only and unapplable.
- diagnostics prove rank `1/6`, five remaining DOF, full row rank, twice, with identical results.
- mixed Mate/Angle diagnostics stay insertion-order independent (opposed normals with a consistent `180°` target).

## Still not implemented

- oriented/signed angle measurement around a reference axis and angle limits.
- angle constraints on axis or seat targets (planar faces only).
- suppressed components inside a solved group.
- joints, limits, motion, subassemblies, and assembly-level STEP export.

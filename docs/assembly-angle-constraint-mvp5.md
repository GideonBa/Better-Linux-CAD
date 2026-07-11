# Planar Angle Constraint Family MVP-5

Status: implemented the first richer local assembly constraint family after Mate/Distance/Concentric/Insert: a planar Angle constraint with persistent degree intent, shared residual/Jacobian integration, local rigid-body solving, explicit application, and local DOF diagnostics.

## Intent

```text
AssemblyConstraint
  type     = angle
  target_a = feature.<feature-id>.<face> on component A
  target_b = feature.<feature-id>.<face> on component B
  angle    = degree quantity (required)
```

`QuantityKind::AngleDeg` and `Quantity::angle_deg` validate finite degree values.

Angle constraints require an angle value and reject Distance values. Every non-Angle family rejects angle values.

JSON persists:

```json
"angle": {"unit": "deg", "value": 35.0}
```

## Residual

Both endpoints resolve through existing generated planar-face semantics and the shared transform evaluator.

```text
normal_dot      = dot(nA, nB)
angle_alignment = normal_dot - cos(target_angle_deg)
```

The flattened residual contributes one dimensionless scalar:

```text
angle_alignment
```

No length scaling applies.

`PlanarAngleResidualDescriptor` joins the planar Mate/Distance descriptor family. Target resolution, numeric flattening, solver execution, result application, and diagnostics remain single-path.

## Semantics and limits of the seed

The residual constrains relative normal orientation only. Position remains free.

The cosine form is direction-symmetric:

```text
+theta and -theta describe the same normal alignment
```

Oriented signed angle measurement around an explicit reference axis is a later relationship extension.

At satisfied `0 deg` and `180 deg` targets, the cosine residual gradient vanishes. Local rank diagnostics report that degeneracy rather than hiding it.

## Proven regular one-free-body result

Away from the extremal targets:

```text
variable_count           = 6
residual_component_count = 1
jacobian_rank            = 1
constrained_dof          = 1
remaining_dof            = 5
residual_row_redundancy  = 0
```

## Covered by tests

`tests/core/assembly_angle_constraint_tests.cpp` proves:

- finite signed degree quantity behavior;
- value-family validation;
- required Angle value and forbidden values on other families;
- JSON roundtrip with `deg`, inactive state, and unchanged placement.

`tests/geometry/assembly_angle_numeric_solver_tests.cpp` proves:

- residual values against expected cosines;
- deterministic mixed Distance/Angle flattening;
- convergence of a tilted free component;
- source-project immutability before application;
- zero-iteration already-satisfied cases;
- grounded fixed inconsistency;
- read-only non-convergence;
- local rank `1/6` with five remaining DOF;
- insertion-order-independent mixed Mate/Angle diagnostics.

## Persistence boundary

Persistent authority is the existing local `AssemblyConstraint` record with its semantic targets, active/inactive state, and Angle quantity.

Derived data includes resolved planar target geometry, Angle residual descriptors, flattened residual scalars, Jacobian rows, solve results, proposals, and rank/DOF diagnostics.

Only explicit successful solve-result application changes component transforms.

## Current boundaries

Still deferred:

- oriented/signed planar angle measurement around a reference axis;
- angle limits as geometric constraint intent;
- Angle endpoints on semantic axes or seating frames.

Implemented later in separate blocks:

- suppressed-component solving;
- Revolute joint limits and motion;
- rigid nested subassemblies and posed STEP export;
- interference/clearance analysis;
- document-scoped flexible child solving;
- read-only cross-hierarchy target/residual semantics.

Persistent project-level cross-hierarchy geometric constraints and solving remain deferred to `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

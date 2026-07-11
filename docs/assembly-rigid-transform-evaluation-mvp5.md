# Assembly Rigid-Transform Evaluation MVP-5

Status: implemented deterministic read-only component-local-to-assembly-space evaluation for persisted `RigidTransform` values. The solver and local DOF diagnostics both evaluate candidate geometry through this exact convention.

## Goal

This block gives persisted component placement one explicit geometry meaning.

It maps component-local points, vectors, and planar frames into assembly coordinates without mutating component records or persisting evaluated geometry.

## API

```text
AssemblySpacePlanarDescriptor
  origin
  x_axis
  y_axis
  normal

AssemblyTransformEvaluator
  evaluate_point(RigidTransform, Point3)
  evaluate_vector(RigidTransform, Vector3)
  evaluate_plane(RigidTransform, ComponentLocalPlanarDescriptor)
```

`AssemblySpacePlanarDescriptor` is distinct from `ComponentLocalPlanarDescriptor` so coordinate space remains explicit in the API.

## Persisted transform record

```text
RigidTransform
  translation_mm
  rotation_deg
```

Translation values are millimeters.

Rotation values are degrees.

The evaluator does not infer radians and does not introduce a quaternion or matrix as alternate persisted placement intent.

## Canonical rotation convention

`rotation_deg = (rx, ry, rz)` means:

```text
active
right-handed
fixed-axis rotations
apply X first
then Y
then Z
```

For column vectors:

```text
R = Rz(rz) * Ry(ry) * Rx(rx)
```

The implementation applies elementary rotations directly rather than depending on a third-party Euler convention.

Positive 90-degree examples:

```text
Rx: +Y -> +Z
Ry: +Z -> +X
Rz: +X -> +Y
```

## Point and vector evaluation

For component-local point `p`:

```text
p_assembly = Rz * Ry * Rx * p_local + translation_mm
```

For component-local vector `v`:

```text
v_assembly = Rz * Ry * Rx * v_local
```

Translation is applied to points only.

The vector rule also applies to basis axes, planar normals, and the future semantic-axis direction vector.

## Planar-frame evaluation

```text
ComponentLocalPlanarDescriptor
  origin
  x_axis
  y_axis
  normal

-> AssemblySpacePlanarDescriptor
  origin = evaluate_point(...)
  x_axis = evaluate_vector(...)
  y_axis = evaluate_vector(...)
  normal = evaluate_vector(...)
```

Rigid rotation preserves vector magnitude and frame orthogonality within floating-point tolerance.

## Combined-order proof

For:

```text
rotation_deg = (90, 90, 90)
v = (1, 2, 3)
```

The documented sequence is:

```text
Rx(90): (1, -3, 2)
Ry(90): (2, -3, -1)
Rz(90): (3, 2, -1)
```

Therefore:

```text
v_assembly = (3, 2, -1)
```

The focused tests prove this combined order explicitly.

## Assembly pipeline use

```text
AssemblyConstraintTarget
  -> AssemblyConstraintTargetResolver
  -> ComponentLocalPlanarDescriptor + RigidTransform
  -> AssemblyTransformEvaluator
  -> AssemblySpacePlanarDescriptor
  -> AssemblyConstraintEquationBuilder
  -> Mate/Distance residual descriptor
  -> shared numeric system
  -> AssemblyRigidBodySolver
  -> AssemblySolveDiagnosticsAnalyzer
```

The solver variable representation directly uses:

```text
tx_mm, ty_mm, tz_mm, rx_deg, ry_deg, rz_deg
```

for each free component.

Every solver residual evaluation and central finite-difference perturbation changes candidate `RigidTransform` values only on private project copies and then uses the normal target/equation/evaluator path.

The shared numeric path is also reused by the DOF diagnostics. Rank is therefore measured from Jacobian sensitivities produced under the same persisted transform convention.

Neither solver nor diagnostics can silently use another Euler order.

## Valid transform boundary

Component creation and explicit transform update paths reject non-finite transform components.

The evaluator assumes a validated `RigidTransform` and remains a direct read-only geometry helper rather than returning `Result<T>`.

Numeric candidate transforms also pass through `AssemblyDocument::set_component_instance_transform` before residual evaluation.

## Read-only and persistence boundary

Evaluation does not:

- mutate input transforms or geometry descriptors
- update a component occurrence
- change component state
- change constraints
- recompute part intent
- own a `ShapeCache`
- persist evaluated assembly-space geometry

No matrix, quaternion, or evaluated-frame JSON field exists.

Solver candidate transforms remain on private project copies. Only explicit application of a fresh converged solve result may update the existing persisted component transform field.

Rank and remaining-DOF diagnostics are also unpersisted.

## Tests

`tests/geometry/assembly_transform_evaluator_tests.cpp` covers identity, translation, positive X/Y/Z rotations, combined order, end-to-end target resolution plus plane evaluation, deterministic repeated evaluation, input immutability, vector-length preservation, and frame orthogonality.

Targeted command:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-transform]"
```

Solver and diagnostics tests separately verify that degree-variable orientation correction and Jacobian-rank evaluation continue through this convention:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-diagnostics]"
```

## Deliberate evaluator-layer limitations

This evaluator does not own residual semantics, solver participation, Jacobian policy, optimization, transform application, or DOF classification. Those are separate downstream responsibilities.

It currently exposes planar-frame evaluation but no distinct assembly-space axis descriptor API.

## Current downstream boundary

Jacobian-rank and remaining-DOF diagnostics are implemented and preserve this transform convention.

The next assembly block is semantic generated-axis resolution and read-only Concentric residual construction. Axis point evaluation must use `evaluate_point` semantics and axis direction evaluation must use `evaluate_vector` semantics.

# Assembly Rigid-Transform Evaluation MVP-5

Status: implemented deterministic read-only component-local-to-assembly-space evaluation for points, vectors, planar frames, and generated axis lines under persisted `RigidTransform` values. Mate, Distance, and Concentric residual/Jacobian evaluation all use this exact transform convention.

## Goal

This block gives persisted component placement one explicit geometry meaning.

It maps component-local geometry descriptors into assembly coordinates without mutating component records or persisting evaluated geometry.

## API

```text
AssemblySpacePlanarDescriptor
  origin
  x_axis
  y_axis
  normal

AssemblySpaceAxisDescriptor
  origin
  direction

AssemblyTransformEvaluator
  evaluate_point(RigidTransform, Point3)
  evaluate_vector(RigidTransform, Vector3)
  evaluate_plane(RigidTransform, ComponentLocalPlanarDescriptor)
  evaluate_axis(RigidTransform, ComponentLocalAxisDescriptor)
```

Distinct local and assembly-space descriptor types make coordinate space explicit.

## Persisted transform record

```text
RigidTransform
  translation_mm
  rotation_deg
```

Translation components are millimeters. Rotation components are degrees.

No matrix or quaternion is alternative persisted placement intent.

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

Positive 90-degree examples:

```text
Rx: +Y -> +Z
Ry: +Z -> +X
Rz: +X -> +Y
```

The implementation applies elementary rotations directly rather than depending on an external library's implicit Euler convention.

## Point evaluation

```text
p_assembly = Rz * Ry * Rx * p_local + translation_mm
```

Rotation is applied first. Translation is applied second.

## Vector evaluation

```text
v_assembly = Rz * Ry * Rx * v_local
```

Translation is not applied to vectors.

The same rule applies to basis axes, normals, and semantic axis directions.

Rigid rotation preserves vector magnitude within floating-point tolerance.

## Planar-frame evaluation

```text
AssemblySpacePlanarDescriptor
  origin = evaluate_point(transform, local_plane.origin)
  x_axis = evaluate_vector(transform, local_plane.x_axis)
  y_axis = evaluate_vector(transform, local_plane.y_axis)
  normal = evaluate_vector(transform, local_plane.normal)
```

Translation affects only the origin.

Unit frame vectors remain unit length and orthogonality is preserved within floating-point tolerance.

## Generated-axis evaluation

```text
AssemblySpaceAxisDescriptor
  origin    = evaluate_point(transform, local_axis.origin)
  direction = evaluate_vector(transform, local_axis.direction)
```

Therefore a component translation moves the axis line origin but cannot change its direction.

A component rotation changes both the axis origin position and direction consistently under the same persisted X-then-Y-then-Z convention.

Example:

```text
local axis origin = (4, -6, 0)
local direction   = (0, 0, 1)
translation       = (10, 20, 30)
rotation_deg      = (90, 0, 0)
```

Result:

```text
assembly origin    = (14, 20, 24)
assembly direction = (0, -1, 0)
```

## Combined-axis order proof

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

A different Euler order would generally produce a different result.

## Semantic-target bridges

Planar bridge:

```text
AssemblyConstraintTarget
  -> AssemblyConstraintTargetResolver::resolve
  -> ComponentLocalPlanarDescriptor + RigidTransform
  -> evaluate_plane
  -> AssemblySpacePlanarDescriptor
```

Generated-axis bridge:

```text
AssemblyConstraintTarget feature.<feature-id>.axis
  -> AssemblyConstraintTargetResolver::resolve_axis
  -> ComponentLocalAxisDescriptor + RigidTransform
  -> evaluate_axis
  -> AssemblySpaceAxisDescriptor
```

Target resolution remains component-local and does not apply placement itself.

## Downstream use

Planar assembly-space frames feed:

```text
AssemblyConstraintEquationBuilder
  -> Mate/Distance residuals
  -> shared numeric system
```

Assembly-space axes feed:

```text
AssemblyConcentricConstraintEquationBuilder
  -> ConcentricResidualDescriptor
  -> same shared numeric system
```

The shared numeric system then feeds both:

```text
AssemblyRigidBodySolver
AssemblySolveDiagnosticsAnalyzer
```

Every baseline, line-search candidate, and central finite-difference plus/minus evaluation uses the same `AssemblyTransformEvaluator` convention.

Concentric axis-tilt solving and the measured regular rank `4/6` therefore use these exact X-then-Y-then-Z transform semantics rather than a second rotation interpretation.

## Valid transform boundary

`ComponentInstance` creation and transform updates reject non-finite transform components.

The evaluator assumes a validated persisted `RigidTransform` and therefore remains a direct geometry helper rather than returning `Result<T>`.

The solver candidate path uses `AssemblyDocument::set_component_instance_transform`, so non-finite candidates cannot become valid project placement state.

## Read-only and persistence boundary

Evaluation does not:

- mutate the input transform
- mutate input points, vectors, planes, or axes
- update a component instance
- change assembly constraints
- recompute a part
- own or mutate `ShapeCache`
- persist evaluated assembly-space geometry

Assembly-space planes and axes are regenerated from component-local semantic geometry plus persisted placement.

The solver and diagnostics repeatedly regenerate these descriptors while evaluating residuals and finite differences.

No transform matrix, quaternion, evaluated-plane, evaluated-axis, or Jacobian JSON field is added.

## Tests

Transform suite:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-transform]"
```

Axis/Concentric semantic suite:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric]"
```

Concentric numeric solver suite:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric-solver]"
```

Coverage includes identity, translation, single-axis rotations, combined rotation order, planar-frame preservation, arbitrary vector magnitude, generated-axis origin/direction mapping, input immutability, Concentric target evaluation, axis-tilt solving, and local rank evaluation through the shared transform convention.

## Current downstream boundary

Transform evaluation for planar and semantic-axis geometry is implemented and consumed by the shared Mate/Distance/Concentric numeric solver and DOF diagnostics.

The next assembly geometry block is stable semantic axial-seating plane geometry for Insert. That geometry must also be evaluated through this same transform convention.

# Assembly Rigid-Transform Evaluation MVP-5

Status: implemented deterministic read-only component-local-to-assembly-space evaluation for points, vectors, planar frames, and generated axis lines under persisted `RigidTransform` values.

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

The result is:

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
  -> current shared numeric system
  -> solver and DOF diagnostics
```

Assembly-space axes feed:

```text
AssemblyConcentricConstraintEquationBuilder
  -> ConcentricResidualDescriptor
```

The Concentric residual path is implemented. Concentric numeric-system/solver/DOF integration is next.

## Valid transform boundary

`ComponentInstance` creation and transform updates reject non-finite transform components.

The evaluator assumes a validated persisted `RigidTransform` and therefore remains a direct geometry helper rather than returning `Result<T>`.

The current solver candidate path uses `AssemblyDocument::set_component_instance_transform`, so non-finite candidates cannot become valid project placement state.

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

No transform matrix, quaternion, evaluated-plane, or evaluated-axis JSON field is added.

## Tests

Transform suite:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-transform]"
```

Axis/Concentric suite:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric]"
```

Coverage includes identity, translation, single-axis rotations, combined rotation order, planar-frame preservation, arbitrary vector magnitude, generated-axis origin/direction mapping, input immutability, and downstream Concentric target evaluation.

## Current downstream boundary

Transform evaluation for semantic axes is implemented.

The next assembly step is Concentric integration into the shared numeric residual/Jacobian path, rigid-body solver, and local remaining-DOF diagnostics. That integration must continue using this exact transform convention for every baseline and finite-difference evaluation.

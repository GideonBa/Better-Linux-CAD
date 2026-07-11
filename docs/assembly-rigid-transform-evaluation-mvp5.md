# Assembly Rigid-Transform Evaluation MVP-5

Status: implemented deterministic read-only component-local-to-assembly-space evaluation for points, vectors, planar frames, and generated axis lines under persisted `RigidTransform` values. Mate, Distance, Concentric, and the read-only Insert endpoint/residual path all use this exact convention.

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

A component translation moves the axis origin but cannot change its direction.

A component rotation changes axis origin and direction consistently under the same persisted X-then-Y-then-Z convention.

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

```text
Rx(90): (1, -3, 2)
Ry(90): (2, -3, -1)
Rz(90): (3, 2, -1)
```

A different Euler order generally produces a different result.

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
feature.<feature-id>.axis
  -> AssemblyConstraintTargetResolver::resolve_axis
  -> ComponentLocalAxisDescriptor + RigidTransform
  -> evaluate_axis
  -> AssemblySpaceAxisDescriptor
```

Composite Insert bridge:

```text
feature.<feature-id>.seat
  -> AssemblyConstraintTargetResolver::resolve_insert
  -> ComponentLocalAxisDescriptor
     + ComponentLocalPlanarDescriptor seating plane
     + RigidTransform
  -> evaluate_axis + evaluate_plane
  -> assembly-space axis + seating plane
```

Target resolution remains component-local and does not apply placement itself.

## Insert transform reuse

Insert introduces no transform representation or Euler convention.

The same persisted component transform is applied independently to both derived pieces of one composite Insert endpoint:

```text
local primary axis
  -> evaluate_axis

local seating plane
  -> evaluate_plane
```

The focused Insert test proves that local origin `(4,-6,0)` under translation `(10,20,30)` and X rotation `90 deg` evaluates to:

```text
axis origin       = (14, 20, 24)
axis direction    = (0, -1, 0)
seat origin       = (14, 20, 24)
seat normal       = (0, -1, 0)
```

The axis and seating plane therefore remain geometrically coupled after placement without storing evaluated assembly-space geometry.

## Downstream use

Planar frames feed:

```text
AssemblyConstraintEquationBuilder
  -> Mate/Distance residuals
  -> shared numeric system
```

Assembly-space axes feed:

```text
AssemblyConcentricConstraintEquationBuilder
  -> Concentric residuals
  -> shared numeric system
```

Composite Insert assembly-space geometry feeds:

```text
AssemblyInsertConstraintEquationBuilder
  -> InsertResidualDescriptor
```

Insert currently stops before shared numeric flattening.

The shared numeric system feeds both `AssemblyRigidBodySolver` and `AssemblySolveDiagnosticsAnalyzer` for Mate, Distance, and Concentric.

Every baseline, candidate, and central finite-difference evaluation for integrated families uses this same transform convention.

The next Insert numeric integration must continue to re-resolve `.seat` targets and evaluate both axis and seating plane through this exact evaluator for every residual/Jacobian evaluation.

## Valid transform boundary

`ComponentInstance` creation and transform updates reject non-finite transform components.

The evaluator assumes a validated persisted `RigidTransform` and remains a direct geometry helper rather than returning `Result<T>`.

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

No transform matrix, quaternion, evaluated-plane, evaluated-axis, evaluated-seat, or Jacobian JSON field is added.

## Tests

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-transform]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-insert]"
```

Coverage includes identity, translation, single-axis rotations, combined rotation order, planar-frame preservation, arbitrary vector magnitude, generated-axis mapping, input immutability, Concentric axis-tilt/rank use of the shared convention, and composite Insert axis/seating evaluation under nontrivial placement.

## Current downstream boundary

Transform evaluation for planar, semantic-axis, and semantic-seating geometry is implemented.

Mate, Distance, and Concentric consume it through the shared numeric solver and diagnostics. Insert already consumes the same evaluator for read-only composite residual construction.

The next step is Insert integration into the shared numeric residual/Jacobian, solver, application, and diagnostics path without changing this transform convention.

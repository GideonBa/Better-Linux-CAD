# Assembly Rigid-Transform Evaluation MVP-5

Status: implemented deterministic read-only component-local-to-assembly-space evaluation for the persisted `RigidTransform` free-placement record.

## Goal

This block gives the existing component placement record one explicit geometry-evaluation convention before assembly constraint equations consume resolved target frames.

It answers three narrow questions:

1. How is one component-local point evaluated in assembly coordinates?
2. How is one component-local vector evaluated without applying translation?
3. How is a `ComponentLocalPlanarDescriptor` converted into an assembly-space planar frame?

The evaluator is derived geometry logic. It does not change model intent, solve constraints, or persist evaluated geometry.

## API

The geometry-layer API lives in `include/blcad/geometry/assembly_transform_evaluator.hpp`.

```text
AssemblySpacePlanarDescriptor
  origin
  x_axis
  y_axis
  normal

AssemblyTransformEvaluator
  evaluate_point(RigidTransform, Point3)
    -> Point3

  evaluate_vector(RigidTransform, Vector3)
    -> Vector3

  evaluate_plane(RigidTransform, ComponentLocalPlanarDescriptor)
    -> AssemblySpacePlanarDescriptor
```

`AssemblySpacePlanarDescriptor` is a separate type from `ComponentLocalPlanarDescriptor`. The coordinate-space distinction therefore remains visible in the API instead of relying on comments or caller discipline.

## Persisted transform units

The existing `RigidTransform` remains:

```text
RigidTransform
  translation_mm
  rotation_deg
```

The interpretation is now explicit:

- `translation_mm.x`, `.y`, and `.z` are assembly-coordinate translations in millimeters
- `rotation_deg.x`, `.y`, and `.z` are angles in degrees
- positive rotations follow the right-hand rule
- rotations are active rotations of component-local geometry
- rotations are applied about fixed axes in X, then Y, then Z order

The evaluator converts each degree value to radians only for the `std::sin` and `std::cos` calculations. The persisted representation remains degrees and the JSON schema does not change.

## Exact rotation convention

Let:

```text
rx = rotation_deg.x
ry = rotation_deg.y
rz = rotation_deg.z
```

For a component-local column vector `v`, BLCAD evaluates:

```text
v1 = Rx(rx) * v
v2 = Ry(ry) * v1
v3 = Rz(rz) * v2
```

Equivalently:

```text
v_assembly = Rz(rz) * Ry(ry) * Rx(rx) * v_local
```

This is an active, right-handed, fixed-axis X-then-Y-then-Z convention.

It is not an implicit library Euler convention and it must not be silently replaced by intrinsic/body-axis XYZ rotation or by Z-then-Y-then-X application order.

A combined-axis test proves the convention:

```text
rotation_deg = (90, 90, 90)
local vector = (1, 2, 3)

Rx(90): (1, -3, 2)
Ry(90): (2, -3, -1)
Rz(90): (3, 2, -1)
```

The expected assembly-space vector is therefore:

```text
(3, 2, -1)
```

This test exists specifically so later solver, import/export, GUI manipulator, and assembly-instancing code cannot infer a different order from the field names alone.

## Point evaluation

Points are rotated first and translated second:

```text
p_assembly = Rz * Ry * Rx * p_local + translation_mm
```

Example with no rotation:

```text
translation_mm = (10, -20, 30)
local point    = (1.25, -2.5, 3.75)

assembly point = (11.25, -22.5, 33.75)
```

Identity rotation and zero translation preserve the point exactly within the existing `double` representation.

## Vector evaluation

Vectors are rotated only:

```text
v_assembly = Rz * Ry * Rx * v_local
```

Translation is never added to vectors, basis axes, or normals.

The evaluator does not normalize arbitrary vectors. A rigid rotation must preserve vector magnitude, so returning a normalized value would incorrectly destroy meaningful vector length. For unit basis axes and unit normals, rotation preserves unit length and orthogonality within floating-point tolerance.

The focused tests use a `1e-12` numeric tolerance for rotated-value, magnitude, and orthogonality checks.

## Planar-frame evaluation

A component-local target frame is:

```text
ComponentLocalPlanarDescriptor
  origin
  x_axis
  y_axis
  normal
```

The evaluator produces:

```text
AssemblySpacePlanarDescriptor
  origin = evaluate_point(transform, local.origin)
  x_axis = evaluate_vector(transform, local.x_axis)
  y_axis = evaluate_vector(transform, local.y_axis)
  normal = evaluate_vector(transform, local.normal)
```

Only the origin receives translation.

For example:

```text
local origin = (0, 0, 8)
local x-axis = (1, 0, 0)
local y-axis = (0, 1, 0)
local normal = (0, 0, 1)

translation_mm = (10, 20, 30)
rotation_deg   = (0, 0, 90)
```

The assembly-space frame is:

```text
origin = (10, 20, 38)
x-axis = (0, 1, 0)
y-axis = (-1, 0, 0)
normal = (0, 0, 1)
```

## Bridge from semantic target resolution

The completed target-resolution block returns:

```text
ResolvedAssemblyConstraintTarget
  local_plane
  component_transform
```

The explicit bridge is now:

```text
AssemblyConstraintTarget
  -> AssemblyConstraintTargetResolver::resolve(...)
  -> ResolvedAssemblyConstraintTarget
  -> AssemblyTransformEvaluator::evaluate_plane(
       resolved.component_transform,
       resolved.local_plane)
  -> AssemblySpacePlanarDescriptor
```

Target resolution still owns semantic component/part/feature lookup and generated-face frame construction. Transform evaluation owns only the component-local-to-assembly-space mapping.

The two responsibilities remain separate so semantic reference resolution does not duplicate transform math and transform evaluation does not depend on part feature semantics.

## Valid transform boundary

`ComponentInstance::create` and the explicit component transform update path reject non-finite translation or rotation components. Persisted project model intent therefore provides finite `RigidTransform` values to this evaluator.

The evaluator is intentionally a direct geometry helper and does not repeat model-record validation or return `Result<T>` for already validated persisted transforms.

## Read-only and persistence boundary

Evaluation does not:

- mutate the input `RigidTransform`
- mutate the input local point, vector, or planar descriptor
- update a `ComponentInstance`
- change grounding, visibility, or suppression state
- add or change assembly constraints
- recompute a `PartDocument`
- own or mutate a `ShapeCache`
- persist an assembly-space point, vector, or planar descriptor

Assembly-space evaluated geometry is regenerable from persisted component placement plus component-local geometry. No transform-evaluation cache field is added to assembly or project JSON.

## Tests

`tests/geometry/assembly_transform_evaluator_tests.cpp` covers:

- exact identity point/vector/frame behavior
- pure translation applied to points and plane origins only
- positive 90-degree X rotation
- positive 90-degree Y rotation
- positive 90-degree Z rotation
- combined X-then-Y-then-Z rotation order
- conversion of a resolved-target-shaped local plane into assembly space
- deterministic repeated frame evaluation
- unchanged input transform and local frame
- unit-length preservation for planar basis axes and normals
- preserved planar-frame orthogonality
- arbitrary vector magnitude preservation

Targeted test command after a geometry build:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-transform]"
```

Complete geometry workflow:

```bash
cmake --workflow --preset dev-geometry-build-test
```

## Deferred work

This block does not implement:

- Mate or Distance constraint equations
- Concentric equations or semantic axis references
- rigid-body solving
- solved component transform updates
- remaining-DOF computation
- underconstrained, fully constrained, or overconstrained state analysis
- enforced grounding
- suppression participation rules for assembly solving
- component geometry instancing
- assembly-level STEP export

## Next technical step

The next assembly block is the first read-only planar Mate/Distance constraint equation-construction seed over assembly-space target descriptors.

That block should consume persistent active constraint records, resolve supported generated-face targets, evaluate both local target planes into assembly space, and construct deterministic geometric equation/residual data without solving for or mutating component transforms.

Concentric equation construction remains deferred until a stable semantic axis-reference family exists.

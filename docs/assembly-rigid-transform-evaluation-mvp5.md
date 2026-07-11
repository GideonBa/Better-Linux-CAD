# Assembly Rigid-Transform Evaluation MVP-5

Status: implemented deterministic read-only component-local-to-assembly-space evaluation for persisted `RigidTransform` placement intent.

## Goal

This block gives the existing persisted component transform one explicit geometry-evaluation convention before assembly equations or a solver consume target geometry.

It evaluates:

- component-local points
- component-local vectors
- component-local planar frames

The result is derived assembly-space geometry. No stored component transform is changed.

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
  evaluate_vector(RigidTransform, Vector3)
  evaluate_plane(RigidTransform, ComponentLocalPlanarDescriptor)
```

`AssemblySpacePlanarDescriptor` is distinct from `ComponentLocalPlanarDescriptor` so coordinate space remains explicit in the API vocabulary.

## Persisted transform convention

`RigidTransform` stores:

```text
translation_mm
rotation_deg
```

The canonical interpretation is:

- translation components are millimeters
- rotation components are degrees
- rotations are active rotations of component-local geometry
- rotations use fixed assembly X, Y, and Z axes
- positive angles follow the right-hand rule
- X rotation is applied first
- Y rotation is applied second
- Z rotation is applied third

For column vectors:

```text
v_assembly = Rz * Ry * Rx * v_local
```

This convention is part of the semantic meaning of persisted `rotation_deg`. Changing it later would change model interpretation even if the JSON field shape remained identical.

## Point evaluation

For a component-local point `p`:

```text
p_rotated = Rz * Ry * Rx * p
p_assembly = p_rotated + translation_mm
```

Translation is applied after rotation.

A pure translation therefore maps:

```text
p_local = (1.25, -2.5, 3.75)
translation_mm = (10, -20, 30)

p_assembly = (11.25, -22.5, 33.75)
```

## Vector evaluation

Vectors represent direction and magnitude rather than position.

```text
v_assembly = Rz * Ry * Rx * v_local
```

Translation is never applied to vectors, basis axes, or normals.

Rigid rotation preserves vector magnitude within floating-point tolerance.

## Positive single-axis rotations

The tests lock the right-handed convention explicitly.

Positive X rotation:

```text
(0, 1, 0) --Rx(90 deg)--> (0, 0, 1)
```

Positive Y rotation:

```text
(0, 0, 1) --Ry(90 deg)--> (1, 0, 0)
```

Positive Z rotation:

```text
(1, 0, 0) --Rz(90 deg)--> (0, 1, 0)
```

## Combined rotation-order proof

The implementation does not rely on an unstated Euler convention.

For:

```text
rotation_deg = (90, 90, 90)
v_local = (1, 2, 3)
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

The combined-axis test exists specifically to prove order, not only single-axis sign conventions.

## Planar-frame evaluation

For:

```text
ComponentLocalPlanarDescriptor
  origin
  x_axis
  y_axis
  normal
```

`evaluate_plane` performs:

```text
assembly origin = evaluate_point(transform, local origin)
assembly x_axis = evaluate_vector(transform, local x_axis)
assembly y_axis = evaluate_vector(transform, local y_axis)
assembly normal = evaluate_vector(transform, local normal)
```

Translation applies only to the origin.

Rigid rotation preserves unit basis lengths and frame orthogonality within numeric tolerance.

## Bridge from semantic target resolution

The target resolver returns:

```text
ResolvedAssemblyConstraintTarget
  local_plane
  component_transform
```

The evaluator consumes those fields directly:

```text
AssemblyConstraintTarget
  -> AssemblyConstraintTargetResolver
  -> local_plane + component_transform
  -> AssemblyTransformEvaluator::evaluate_plane
  -> AssemblySpacePlanarDescriptor
```

The responsibilities remain separate:

- target resolution owns semantic lookup and component-local frame construction
- transform evaluation owns component-local-to-assembly coordinate mapping

## Downstream residual construction

Planar Mate/Distance residual construction is now implemented separately in `docs/assembly-planar-constraint-equations-mvp5.md`.

`AssemblyConstraintEquationBuilder` reuses this evaluator for both targets before constructing residuals:

```text
AssemblyConstraint
  -> target A/B resolution
  -> target A/B transform evaluation
  -> assembly-space planes
  -> Mate or Distance residual descriptor
```

The transform evaluator itself still has no knowledge of constraint type or residual semantics.

## Valid transform boundary

`ComponentInstance::create` and explicit transform updates reject non-finite translation or rotation components. Persisted model intent therefore supplies finite `RigidTransform` values to this evaluator.

The evaluator is a direct geometry helper and does not repeat record validation or return `Result<T>` for already validated persisted transforms.

## Read-only and persistence boundary

Evaluation does not:

- mutate the input `RigidTransform`
- mutate the input point, vector, or local planar descriptor
- update a `ComponentInstance`
- change grounding, visibility, or suppression state
- change assembly constraints
- recompute a `PartDocument`
- own or mutate a `ShapeCache`
- persist evaluated assembly-space geometry

Assembly-space geometry is regenerated from component-local geometry plus persisted placement. No transform matrix, quaternion, or evaluated-frame JSON field is added.

## Tests

`tests/geometry/assembly_transform_evaluator_tests.cpp` covers:

- exact identity point/vector/frame behavior
- pure translation applied to points and plane origins only
- positive 90-degree X rotation
- positive 90-degree Y rotation
- positive 90-degree Z rotation
- combined X-then-Y-then-Z order
- end-to-end target resolution followed by plane evaluation
- deterministic repeated frame evaluation
- unchanged input transform and local frame
- unit-length preservation for planar basis axes and normals
- frame orthogonality preservation
- arbitrary vector magnitude preservation

Targeted test command:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-transform]"
```

Complete geometry workflow:

```bash
cmake --workflow --preset dev-geometry-build-test
```

## Deliberate limitations

This block itself does not implement:

- assembly constraint residual semantics
- rigid-body solving
- solved component transform updates
- residual Jacobians or weighting
- remaining-DOF computation
- under/fully/overconstrained analysis
- enforced grounding
- suppression participation rules
- component geometry instancing
- assembly-level STEP export

Planar Mate/Distance residual construction is no longer deferred globally; it is implemented as the separate `AssemblyConstraintEquationBuilder` layer.

## Current downstream boundary

The repository-wide next assembly step is a first rigid-body solver seed over the active-constraint graph and the implemented planar residual descriptors.

The solver must preserve this transform convention at its API boundary and explicitly define its internal variable representation, fixed/grounded participation, residual weighting, convergence behavior, solve-result representation, and transform-application boundary.

Concentric remains deferred until semantic axis targets and Concentric residual construction exist.

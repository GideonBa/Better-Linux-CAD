# Assembly Rigid-Transform Evaluation MVP-5

Status: implemented deterministic read-only component-local-to-assembly-space evaluation for persisted `RigidTransform` values. The first rigid-body solver now uses this exact convention for every residual and finite-difference evaluation.

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

`AssemblySpacePlanarDescriptor` is distinct from `ComponentLocalPlanarDescriptor` so coordinate space remains explicit in the API vocabulary.

## Persisted transform record

```text
RigidTransform
  translation_mm
  rotation_deg
```

Translation components are millimeters.

Rotation components are degrees.

The evaluator does not infer radians from stored values and does not introduce a quaternion or matrix as alternate persisted placement intent.

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

The implementation applies the elementary rotations directly in that order rather than depending on an external library's implicit Euler convention.

Positive 90-degree examples:

```text
Rx: +Y -> +Z
Ry: +Z -> +X
Rz: +X -> +Y
```

## Point evaluation

For component-local point `p`:

```text
p_assembly = Rz * Ry * Rx * p_local + translation_mm
```

Rotation is applied first. Translation is applied second.

## Vector evaluation

For a component-local vector `v`:

```text
v_assembly = Rz * Ry * Rx * v_local
```

Translation is not applied to vectors.

The same rule applies to basis axes and normals.

Rigid rotation preserves vector magnitude within floating-point tolerance.

## Planar-frame evaluation

For:

```text
ComponentLocalPlanarDescriptor
  origin
  x_axis
  y_axis
  normal
```

The evaluator returns:

```text
AssemblySpacePlanarDescriptor
  origin = evaluate_point(...)
  x_axis = evaluate_vector(...)
  y_axis = evaluate_vector(...)
  normal = evaluate_vector(...)
```

Translation affects only the origin.

Unit frame vectors remain unit length and frame orthogonality is preserved within floating-point tolerance.

## Combined-axis order proof

The focused tests explicitly prove rotation order rather than only checking single axes.

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

A different Euler order would generally produce a different result.

## Semantic-target bridge

The implemented target path is:

```text
AssemblyConstraintTarget
  -> AssemblyConstraintTargetResolver
  -> ResolvedAssemblyConstraintTarget.local_plane
     + ResolvedAssemblyConstraintTarget.component_transform
  -> AssemblyTransformEvaluator::evaluate_plane(...)
  -> AssemblySpacePlanarDescriptor
```

Target resolution remains component-local and does not apply placement itself.

## Downstream residual and solver use

The downstream path is now:

```text
AssemblySpacePlanarDescriptor A/B
  -> AssemblyConstraintEquationBuilder
  -> planar Mate/Distance residual descriptors
  -> AssemblyRigidBodySolver
```

The solver variable representation directly uses:

```text
tx_mm, ty_mm, tz_mm, rx_deg, ry_deg, rz_deg
```

for each free component.

Every solver residual evaluation and central finite-difference perturbation updates candidate `RigidTransform` values on a private project copy and then calls the normal target/equation path.

The solver therefore cannot silently use a different rotation order from persisted placement semantics.

The solver does not canonicalize angles into a particular degree interval in the first seed.

## Valid transform boundary

`ComponentInstance::create`, copy-style transform validation, and the explicit assembly transform update path reject non-finite transform components.

The evaluator assumes a validated persisted `RigidTransform` and therefore remains a direct geometry helper rather than returning `Result<T>`.

The solver candidate path also uses `AssemblyDocument::set_component_instance_transform`, so non-finite candidate values cannot become valid project placement state.

## Read-only and persistence boundary

Evaluation does not:

- mutate the input `RigidTransform`
- mutate an input point, vector, or local planar descriptor
- update a `ComponentInstance`
- change grounding, visibility, or suppression
- change assembly constraints
- recompute a `PartDocument`
- own or mutate `ShapeCache`
- persist evaluated assembly-space geometry

Assembly-space geometry is regenerated from component-local geometry plus persisted placement.

No transform matrix, quaternion, or evaluated-frame JSON field is added.

The downstream solver also keeps candidate transforms on private project copies. Only explicit application of a fresh converged `AssemblySolveResult` may update the existing persistent component transform field.

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

Targeted command:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-transform]"
```

The solver suite separately verifies that orientation residuals are corrected through these persisted degree variables:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-solver]"
```

## Deliberate evaluator-layer limitations

This evaluator itself does not implement:

- assembly constraint residual semantics
- fixed/variable solver participation
- numeric Jacobians or residual weighting
- rigid-body optimization
- solved transform application
- remaining-DOF computation
- under/fully/overconstrained classification
- grounding or suppression solver policy
- component geometry instancing
- assembly STEP export

Planar residual construction, rigid-body solving, and explicit successful-result application are implemented as separate downstream layers.

## Current downstream boundary

The repository-wide next assembly step is read-only Jacobian-rank and remaining-degree-of-freedom diagnostics over the implemented solver ordering and numeric model.

That diagnostics layer must preserve this transform convention when evaluating local rank at a selected component state.

Concentric remains deferred until semantic axis targets and Concentric residual construction exist.

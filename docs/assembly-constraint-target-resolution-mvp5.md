# Assembly Constraint Target Resolution MVP-5

Status: implemented read-only generated-planar-face resolution and the first read-only generated-axis resolution family. Planar targets feed Mate/Distance; generated axes feed Concentric. Both residual families now join the shared numeric solver and DOF path downstream.

## Goal

`AssemblyConstraintTargetResolver` bridges persistent semantic assembly target strings to deterministic component-local geometry.

It owns semantic target lookup and local geometry construction only.

It does not apply component placement, construct constraint residuals, decide solver participation, optimize transforms, or compute DOF.

## API

Planar target types:

```text
ComponentLocalPlanarDescriptor
  origin
  x_axis
  y_axis
  normal

ResolvedAssemblyConstraintTarget
  component_instance
  referenced_part_document
  source_feature
  face
  local_plane
  component_transform
```

Axis target types:

```text
ComponentLocalAxisDescriptor
  origin
  direction

ResolvedAssemblyAxisConstraintTarget
  component_instance
  referenced_part_document
  source_feature
  source_profile
  axis
  local_axis
  component_transform
```

Resolver methods:

```text
AssemblyConstraintTargetResolver
  resolve(Project, target)
    -> ResolvedAssemblyConstraintTarget

  resolve_axis(Project, target)
    -> ResolvedAssemblyAxisConstraintTarget
```

Plane and axis APIs are separate so callers cannot accidentally reinterpret one geometry family as the other.

## Shared ownership resolution

Both paths first resolve:

```text
AssemblyConstraintTarget.component_instance
  -> Project.assembly().find_component_instance
  -> component.referenced_part_document
  -> Project.find_part_document
```

The component occurrence and project-owned part must exist before semantic feature geometry is interpreted.

The resolver never duplicates the referenced `PartDocument` as new model intent.

## Generated planar-face family

Supported tokens:

```text
feature.<feature-id>.top
feature.<feature-id>.bottom
feature.<feature-id>.right
feature.<feature-id>.left
feature.<feature-id>.front
feature.<feature-id>.back
```

The source feature must be a supported `AdditiveExtrude`.

Face geometry is delegated to:

```text
WorkplaneResolver::resolve_generated_face
```

Derived workplanes and assembly face targets therefore share one implementation of generated-face frame geometry.

The returned local plane contains origin, basis axes, and normal. Component placement remains separate in `component_transform`.

## Generated-axis family

Canonical detail: `docs/assembly-semantic-axis-concentric-residuals-mvp5.md`.

Supported first token:

```text
feature.<feature-id>.axis
```

The source feature must be:

```text
FeatureType::SubtractiveExtrude
```

Its input sketch must contain:

```text
exactly one CircleProfile
and exactly one total profile
```

The circle diameter parameter must resolve to a length parameter.

Axis geometry reuses the source sketch workplane:

```text
WorkplaneResolver::resolve_for_sketch
```

The local axis is:

```text
origin
  = WorkplaneResolver::evaluate_point(workplane, CircleProfile.center)

direction
  = workplane.normal
    or -workplane.normal for OppositeSketchNormal
```

This is the same intent interpretation used by the existing circular-cut geometry execution path.

The resolved descriptor preserves the source `CircleProfile` id in addition to source feature identity.

## Why circular-hole patterns are excluded

A `CircularHolePattern` generates several distinct hole axes.

One token such as:

```text
feature.pattern.axis
```

would be ambiguous.

A later pattern-axis family must define stable per-instance semantic identity. The resolver does not infer identity from OCCT topology order or hidden vector position.

## Component-local versus assembly-space geometry

Resolver outputs remain component-local.

Planar path:

```text
local_plane + component_transform
  -> AssemblyTransformEvaluator::evaluate_plane
  -> AssemblySpacePlanarDescriptor
```

Axis path:

```text
local_axis + component_transform
  -> AssemblyTransformEvaluator::evaluate_axis
  -> AssemblySpaceAxisDescriptor
```

This keeps persisted placement semantics centralized in the transform evaluator.

## Downstream use

Planar branch:

```text
resolve
  -> AssemblyTransformEvaluator::evaluate_plane
  -> AssemblyConstraintEquationBuilder
  -> Mate/Distance residuals
  -> shared numeric system
  -> rigid-body solver / DOF diagnostics
```

Axis branch:

```text
resolve_axis
  -> AssemblyTransformEvaluator::evaluate_axis
  -> AssemblyConcentricConstraintEquationBuilder
  -> Concentric residuals
  -> same shared numeric system
  -> same rigid-body solver / DOF diagnostics
```

The resolver remains unchanged by numeric integration. The shared numeric system simply re-enters this path for every Concentric residual and finite-difference evaluation.

## Failure behavior

Shared failures include:

- target component does not exist
- referenced part is not project-owned

Planar failures include malformed/unsupported face tokens, missing source features, unsupported feature type, and shared generated-face resolution failures.

Axis failures include:

- malformed axis token
- unsupported axis suffix
- missing source feature
- source feature is not a subtractive extrude
- missing source sketch
- source sketch does not contain exactly one `CircleProfile` and one total profile
- invalid/missing length diameter parameter
- unresolved source sketch workplane

Failures use existing `Result<T>`/`Error` propagation.

Axis target failures propagate unchanged through Concentric residual construction, the shared numeric system, solver, and diagnostics.

## Read-only and persistence boundary

Resolution does not:

- mutate component transforms or component state
- change constraints or target strings
- modify part parameters, sketches, profiles, features, or derived workplanes
- execute a solver
- own `ShapeCache`
- persist a resolved target descriptor

Resolved descriptors are regenerated from current model intent.

No solver or Jacobian cache owns resolved target bindings.

## Tests

Planar tests:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-target]"
```

Axis and Concentric semantic tests:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric]"
```

Concentric downstream numeric/solver tests:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric-solver]"
```

Coverage includes all six generated faces, semantic axis identity, CircleProfile-center axis origins, extrude-direction axis orientation, unsupported feature/profile families, determinism, unchanged model intent, and repeated target re-resolution through numeric solver/Jacobian evaluation.

## Current downstream boundary

Generated-axis target resolution and Concentric shared-numeric/solver/DOF consumption are implemented.

The next semantic target-resolution work is stable axial-seating geometry for Insert. That target family must remain semantic and component-local before the transform evaluator, just like existing planar and axis targets.

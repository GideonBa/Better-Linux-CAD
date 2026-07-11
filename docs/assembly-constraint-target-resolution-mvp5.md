# Assembly Constraint Target Resolution MVP-5

Status: implemented read-only generated planar-face resolution, primary circular-feature axis resolution, and primary circular-feature seating/composite Insert endpoint resolution.

## Goal

`AssemblyConstraintTargetResolver` bridges persistent semantic assembly target strings to deterministic component-local geometry.

It owns semantic target lookup and local geometry construction only. It does not apply component placement, construct residuals, decide solver participation, optimize transforms, or compute DOF.

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

Composite Insert target type:

```text
ResolvedAssemblyInsertConstraintTarget
  component_instance
  referenced_part_document
  source_feature
  source_profile
  axis
  seating_plane
  local_axis
  local_seating_plane
  component_transform
```

Resolver methods:

```text
resolve(Project, target)
  -> generated planar face

resolve_axis(Project, target)
  -> primary generated axis

resolve_insert(Project, target)
  -> primary generated axis + oriented seating plane
```

The APIs remain explicit so callers cannot reinterpret one geometry family accidentally.

## Shared ownership resolution

Every path first resolves:

```text
AssemblyConstraintTarget.component_instance
  -> Project.assembly().find_component_instance
  -> component.referenced_part_document
  -> Project.find_part_document
```

The component occurrence and project-owned part must exist before semantic feature geometry is interpreted.

The resolver never duplicates the referenced `PartDocument` as model intent.

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

Face geometry delegates to:

```text
WorkplaneResolver::resolve_generated_face
```

The result remains component-local and preserves component placement separately.

## Generated-axis family

Canonical detail: `docs/assembly-semantic-axis-concentric-residuals-mvp5.md`.

Supported first token:

```text
feature.<feature-id>.axis
```

The first producer is:

```text
FeatureType::SubtractiveExtrude
+ exactly one CircleProfile in the source sketch
+ exactly one total profile in the source sketch
+ circle diameter resolves to a length parameter
```

Axis geometry uses the source sketch workplane:

```text
origin = WorkplaneResolver::evaluate_point(workplane, CircleProfile.center)

direction = workplane.normal
          or -workplane.normal for OppositeSketchNormal
```

The result preserves source feature and source `CircleProfile` identity.

## Generated seating family

Canonical detail: `docs/assembly-insert-intent-composite-residuals-mvp5.md`.

Supported first token:

```text
feature.<feature-id>.seat
```

The `.seat` producer uses the same narrow single-circle `SubtractiveExtrude` requirements as `.axis`.

`resolve_insert` deliberately interprets one persistent seat endpoint as a composite circular-feature target:

```text
feature.hole.seat
  -> exact source Feature
  -> exact source CircleProfile
  -> source Sketch workplane
  -> mapped CircleProfile.center
  -> primary local axis
  -> oriented local seating plane
```

The local primary axis is:

```text
origin = mapped CircleProfile.center
direction = extrude direction
```

The local seating plane is:

```text
origin = axis.origin
normal = axis.direction
```

For `SketchNormal`:

```text
x_axis = workplane.x_axis
y_axis = workplane.y_axis
normal = workplane.normal
```

For `OppositeSketchNormal`:

```text
x_axis =  workplane.x_axis
y_axis = -workplane.y_axis
normal = -workplane.normal
```

Reversing Y with the normal preserves a right-handed seating frame.

No OCCT opening face is queried to define the seat.

## Why one seat target derives two geometry descriptors

Insert requires axis-line alignment plus axial seating.

A persistent endpoint such as `feature.hole.seat` identifies one exact circular feature/profile. From that constructive identity, the primary axis and seating plane are deterministic and inseparable for this first feature family.

The record therefore does not store a hidden second target and does not expand Insert into four target strings.

The composite derived descriptor makes the coupling explicit while keeping persistent intent compact.

## Why circular-hole patterns are excluded

`CircularHolePattern` produces several distinct holes.

Tokens such as:

```text
feature.pattern.axis
feature.pattern.seat
```

would be ambiguous.

A future pattern target family must define stable per-instance semantic identity. The resolver does not infer identity from transient OCCT topology or hidden vector order.

## Component-local versus assembly-space geometry

Resolver outputs remain component-local.

Planar path:

```text
local_plane + component_transform
  -> AssemblyTransformEvaluator::evaluate_plane
```

Axis path:

```text
local_axis + component_transform
  -> AssemblyTransformEvaluator::evaluate_axis
```

Insert path:

```text
local_axis + local_seating_plane + component_transform
  -> evaluate_axis + evaluate_plane
```

This centralizes persisted placement semantics in `AssemblyTransformEvaluator`.

## Downstream use

Planar branch:

```text
resolve
  -> evaluate_plane
  -> AssemblyConstraintEquationBuilder
  -> Mate/Distance residuals
  -> shared numeric system
  -> solver / DOF diagnostics
```

Axis branch:

```text
resolve_axis
  -> evaluate_axis
  -> AssemblyConcentricConstraintEquationBuilder
  -> Concentric residuals
  -> shared numeric system
  -> solver / DOF diagnostics
```

Insert branch:

```text
resolve_insert
  -> evaluate_axis + evaluate_plane
  -> AssemblyInsertConstraintEquationBuilder
  -> Insert residuals
```

Insert numeric/solver/DOF consumption is the next block.

## Failure behavior

Shared failures include:

- target component does not exist
- referenced part is not project-owned

Planar failures include malformed/unsupported face tokens, missing source features, unsupported feature type, and generated-face resolution failures.

Axis failures include malformed axis tokens, unsupported suffixes, missing source features/sketches, wrong feature type, ambiguous/non-circle profile content, invalid diameter parameters, and unresolved source workplanes.

Seat/Insert target failures use parallel explicit messages for malformed/unsupported seating tokens and unsupported circular-feature source intent.

Failure values propagate through downstream builders unchanged.

## Read-only and persistence boundary

Resolution does not:

- mutate component transforms or state
- change constraints or target strings
- modify part parameters, sketches, profiles, features, or workplanes
- execute a solver
- own `ShapeCache`
- persist resolved descriptors

Resolved plane, axis, and seat descriptors are regenerated from current model intent.

No solver or Jacobian cache owns persistent target bindings.

## Tests

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-target]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-insert]"
```

Coverage includes all six generated faces, primary circular axis identity, primary circular seat identity, CircleProfile-center mapping, extrude-direction orientation, right-handed opposite seat frames, source feature/profile preservation, unsupported source families, determinism, read-only behavior, and downstream assembly-space evaluation.

## Current downstream boundary

Generated planar-face, circular-axis, and circular-seat/composite Insert endpoint resolution are implemented.

Mate, Distance, and Concentric already join the shared numeric solver/DOF path. Insert target/residual semantics are stable and read-only; the next step is Insert integration into that existing shared numeric path.

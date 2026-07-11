# Semantic Generated Axes and Concentric Residuals MVP-5

Status: implemented stable generated-axis semantic references for the first supported circular-cut feature family, deterministic Concentric target/residual construction, and downstream integration into the shared numeric solver and local DOF diagnostics.

The same circular-feature identity is now also reused by the separate `.seat`/Insert endpoint family documented in `docs/assembly-insert-intent-composite-residuals-mvp5.md`.

## Goal

This block defines semantic geometry and geometric residual semantics for Concentric relationships:

```text
Which feature-produced axis does a persistent target mean?
Where is it in component-local coordinates?
Where is it in assembly coordinates?
What deterministic residual describes Concentric error?
```

Axis resolution and residual construction remain read-only. A separate shared numeric layer consumes the descriptors for solving and diagnostics.

## Stable semantic axis family

First token:

```text
feature.<feature-id>.axis
```

Example:

```text
feature.hole.axis
```

Core identity:

```text
SemanticAxis::Primary
SemanticAxisReference
  source_feature
  axis
  node_id()
```

```text
SemanticAxisReference(feature.hole, Primary).node_id()
  -> feature.hole.axis
```

The token identifies constructive feature meaning and never stores an OCCT face, edge, wire, or transient topology index.

## First supported producer

The first axis producer is:

```text
FeatureType::SubtractiveExtrude
+ exactly one CircleProfile in the source sketch
+ exactly one total profile in the sketch
+ circle diameter resolves to a length parameter
```

This matches the current circular through-all cut model path.

Axis geometry:

```text
origin = CircleProfile.center mapped through the resolved source-sketch workplane

direction = workplane.normal
          or -workplane.normal for OppositeSketchNormal
```

Diameter does not change the axis line, but valid circular-profile intent is required before the feature may expose a semantic axis.

## Why generic additive extrudes are not axis producers

The current additive-extrude path supports several profile families but does not generally interpret every profile as one unambiguous cylindrical axis producer.

BLCAD therefore exposes semantic axes only where current feature intent has stable axis meaning.

## Why one circular pattern does not equal one axis

`CircularHolePattern` produces several holes.

```text
feature.bolt_pattern.axis
```

would be ambiguous.

Pattern axis references require stable per-instance semantic identity and must not rely on OCCT topology order or hidden vector position.

## Component-local axis descriptor

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

`source_profile` preserves the exact `CircleProfile` producing the semantic axis.

The local axis and persisted component transform remain separate.

## Axis resolution API

```text
AssemblyConstraintTargetResolver::resolve_axis(Project, target)
  -> ResolvedAssemblyAxisConstraintTarget
```

Resolution order:

```text
AssemblyConstraintTarget
  -> component instance
  -> project-owned referenced PartDocument
  -> parse feature.<feature-id>.axis
  -> source Feature
  -> require SubtractiveExtrude
  -> source Sketch
  -> require exactly one CircleProfile and one total profile
  -> require length diameter parameter
  -> WorkplaneResolver::resolve_for_sketch
  -> map CircleProfile.center
  -> apply ExtrudeDirection to workplane normal
  -> ComponentLocalAxisDescriptor
  + separate component RigidTransform
```

Component/part ownership is resolved before feature geometry, preserving deterministic failure precedence.

## Assembly-space axis evaluation

`AssemblyTransformEvaluator` exposes:

```text
AssemblySpaceAxisDescriptor
  origin
  direction

evaluate_axis(RigidTransform, ComponentLocalAxisDescriptor)
```

Canonical transform convention:

```text
active right-handed fixed-axis rotations
X, then Y, then Z
R = Rz * Ry * Rx
```

```text
axis.origin_assembly = evaluate_point(transform, local_axis.origin)
axis.direction_assembly = evaluate_vector(transform, local_axis.direction)
```

Translation affects the origin but not direction. Rigid rotation preserves direction magnitude within floating-point tolerance.

## Concentric equation API

```text
AssemblyConcentricConstraintEquationBuilder
  build(Project, AssemblyConstraint)
    -> AssemblyConcentricConstraintEquationDescriptor
```

Descriptor family:

```text
AssemblySpaceAxisConstraintTargetDescriptor
  component_instance
  semantic_reference
  axis

ConcentricResidualDescriptor
  direction_parallelism
  axis_offset_mm

AssemblyConcentricConstraintEquationDescriptor
  constraint
  target_a
  target_b
  residual
```

The builder accepts only active `AssemblyConstraintType::Concentric` records.

Inactive and wrong-type records fail before target resolution.

Target A resolves before target B.

## Canonical Concentric residual

For assembly-space axis lines `(oA,dA)` and `(oB,dB)`:

```text
direction_parallelism = cross(dA, dB)
axis_offset_mm         = cross(oB - oA, dA)
```

A satisfied relationship has both vectors equal to zero.

## Direction semantics

```text
cross(dA, dB) = 0
```

accepts:

```text
dB = dA
dB = -dA
```

Concentric aligns axis lines without imposing same-direction versus opposed-direction orientation.

## Perpendicular axis offset

```text
cross(oB - oA, dA)
```

removes origin separation parallel to target A's axis.

Example:

```text
oA = (0,0,0)
dA = (0,0,1)
oB = (0,0,25)
```

produces zero offset.

For:

```text
oB = (3,0,25)
```

```text
axis_offset_mm = (0,-3,0)
```

The axes are parallel but separated laterally by 3 mm.

## Target-order behavior

The zero condition is symmetric, but raw `axis_offset_mm` is target-order and direction dependent because it uses target A's direction.

Persistent A/B order is preserved through graph lookup, residual construction, numeric flattening, solving, and diagnostics.

## Regular freedoms and proven rank

One regular Concentric relationship between one grounded and one free body constrains:

```text
2 axis-tilt rotations
2 translations perpendicular to the common axis
```

It leaves:

```text
translation along the common axis
rotation about the common axis
```

free.

The six-scalar vector residual has only four independent local sensitivities.

The shared finite-difference Jacobian and generic rank analyzer prove:

```text
residual_component_count = 6
variable_count           = 6
jacobian_rank            = 4
constrained_dof          = 4
remaining_dof            = 2
residual_row_redundancy  = 2
```

No Concentric-specific DOF rule is hard-coded.

## Separate builders, shared numeric consumer

The planar builder remains:

```text
AssemblyConstraintEquationBuilder
```

for Mate and Distance.

The axis builder remains:

```text
AssemblyConcentricConstraintEquationBuilder
```

for Concentric.

The private shared numeric system selects the builder from persistent `AssemblyConstraintType`.

Exact Concentric flattening:

```text
direction_parallelism.x
direction_parallelism.y
direction_parallelism.z
axis_offset_mm.x / length_residual_scale_mm
axis_offset_mm.y / length_residual_scale_mm
axis_offset_mm.z / length_residual_scale_mm
```

The same residual evaluator is consumed by `AssemblyRigidBodySolver` and `AssemblySolveDiagnosticsAnalyzer`.

Canonical numeric integration detail: `docs/assembly-concentric-numeric-solver-dof-mvp5.md`.

## Solver behavior

The shared solver corrects lateral axis offset and axis tilt.

It preserves Concentric-defined axial and rotation-about-axis freedoms rather than adding hidden residuals.

Equal and opposed coincident axes are valid states.

All-grounded unsatisfied Concentric geometry returns `FixedGeometryInconsistent`.

Non-converged results remain read-only and cannot be applied.

A converged result uses the existing snapshot, stale-result, and atomic-application contracts.

## Reuse by Insert

The separate Insert block reuses the same first circular-feature identity but persists a `.seat` endpoint:

```text
feature.<feature-id>.seat
```

`resolve_insert` derives from one exact source feature/profile:

```text
primary axis
oriented seating plane
```

The Insert axis uses the same origin/direction convention as `.axis`.

Insert then reuses Concentric axis-line residuals and adds signed seating:

```text
direction_parallelism       = cross(dA, dB)
axis_offset_mm               = cross(oB - oA, dA)
signed_seating_separation_mm = dot(sB - sA, nA)
```

This does not change Concentric semantics or its numeric flattening.

A direct Insert residual Jacobian proves regular rank `5/6`, leaving only rotation about the common axis free.

Insert is not yet a shared numeric solver family.

## Read-only and persistence boundary

Axis resolution and Concentric residual construction do not mutate project intent.

The solver mutates private `Project` copies only until explicit successful application.

Derived and unpersisted data includes:

- semantic-axis parser interpretation
- component-local axis descriptors
- assembly-space axis descriptors
- Concentric residual descriptors
- flattened Concentric residual scalars
- finite-difference Jacobians
- solver state/results/proposals
- rank and remaining-DOF diagnostics

No assembly/project JSON field is added for axis, residual, numeric, or solver integration.

Persistent inputs remain component transforms, semantic target strings, and part sketch/profile/feature intent.

## Failure behavior

Axis resolution fails explicitly for missing components/parts, malformed or unsupported axis tokens, missing/wrong source features, missing source sketches, ambiguous/non-circle profile content, invalid diameter parameters, or unresolved workplanes.

Concentric equation construction rejects inactive and non-Concentric records before target resolution.

Target-resolution failures propagate unchanged through the shared numeric system, solver, and diagnostics.

## Tests

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric-solver]"
```

The semantic suite covers target identity, local/assembly axis geometry, opposite extrude direction, residual semantics, target order, determinism, read-only behavior, and explicit failures.

The numeric suite covers exact six-scalar flattening, mixed ordering/dimensions, offset/tilt solving, equal/opposed states, preserved freedoms, result application, non-convergence, fixed inconsistency, semantic failure propagation, rank `4/6`, and mixed Distance+Concentric rank `5/6`.

## Current downstream boundary

Concentric semantic geometry, residual construction, shared numeric flattening, solving, explicit application, and local DOF analysis are implemented.

Stable `.seat` targets and read-only composite Insert residuals are also implemented as a separate downstream family.

The next assembly step is Insert integration into the existing shared numeric residual/Jacobian, solver, application, and diagnostics path. Concentric semantics remain unchanged by that integration.

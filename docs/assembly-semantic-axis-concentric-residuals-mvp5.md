# Semantic Generated Axes and Concentric Residuals MVP-5

Status: implemented stable generated-axis semantic references for the first supported circular-cut feature family and a deterministic read-only Concentric target/residual pipeline.

## Goal

This block closes the semantic-geometry gap that previously prevented Concentric constraints from being interpreted geometrically.

The implemented path answers:

```text
Which feature-produced axis does a persistent assembly target mean?
Where is that axis in component-local coordinates?
Where is it in assembly coordinates under the persisted RigidTransform?
What deterministic residual data describes Concentric error between two axes?
```

The block does not solve Concentric constraints and does not mutate component transforms.

## Stable semantic axis family

The first implemented generated-axis token is:

```text
feature.<feature-id>.axis
```

Example:

```text
feature.hole.axis
```

The token identifies the primary generated axis of one supported feature. It never stores an OCCT face, edge, wire, or transient topology index.

Core semantic identity is represented by:

```text
SemanticAxis
  Primary

SemanticAxisReference
  source_feature
  axis
  node_id()
```

For the first family:

```text
SemanticAxis::Primary -> "axis"
SemanticAxisReference(feature.hole, Primary).node_id()
  -> feature.hole.axis
```

The enum deliberately leaves room for future feature families that expose more than one named axis without changing the rule that references are BLCAD model intent rather than kernel topology ids.

## First supported axis producer

The first feature family that exposes `feature.<feature-id>.axis` is:

```text
FeatureType::SubtractiveExtrude
+ exactly one CircleProfile in the input sketch
+ no second profile in that sketch
```

This matches the existing circular through-all cut path in `GeometryRecomputeExecutor`.

The generated axis is derived from the same intent used to create the circular cut:

```text
axis origin
  = CircleProfile.center mapped through the resolved sketch workplane

axis direction
  = resolved workplane normal
    or its negation for OppositeSketchNormal
```

The circle diameter parameter must still resolve to a length parameter. Diameter does not change the axis line, but requiring valid circular-profile intent prevents an invalid circular cut record from becoming a valid axis producer accidentally.

## Why additive extrudes are not generic axis producers

The current additive-extrude geometry path supports rectangle, closed, arc-closed, composite, and detected-region profiles. It does not currently execute a single-circle additive extrusion as a cylinder feature.

Therefore this block does not invent a cylindrical axis for every additive extrude.

A semantic axis is exposed only where current feature intent has an unambiguous axis-producing meaning.

## Why circular hole patterns are not mapped to one `axis`

`CircularHolePattern` produces several distinct hole axes.

A token such as:

```text
feature.bolt_pattern.axis
```

would be ambiguous because the feature has multiple generated axes.

Pattern-axis references require a stable per-instance semantic identity such as an explicitly documented hole-instance token family. That identity is deferred rather than inferred from transient OCCT topology or hidden vector order.

## Component-local axis descriptor

Axis target resolution adds:

```text
ComponentLocalAxisDescriptor
  origin
  direction
```

and:

```text
ResolvedAssemblyAxisConstraintTarget
  component_instance
  referenced_part_document
  source_feature
  source_profile
  axis
  local_axis
  component_transform
```

`source_profile` preserves the exact `CircleProfile` that produces the first semantic axis.

The local axis and the persisted component transform remain separate.

## Axis target resolution API

`AssemblyConstraintTargetResolver` now provides two explicit geometry families:

```text
resolve(Project, AssemblyConstraintTarget)
  -> generated planar face target

resolve_axis(Project, AssemblyConstraintTarget)
  -> generated axis target
```

The APIs remain separate so a caller cannot accidentally reinterpret a plane as an axis or vice versa.

The axis-resolution path is:

```text
AssemblyConstraintTarget
  -> component instance
  -> referenced project-owned PartDocument
  -> parse feature.<feature-id>.axis
  -> source Feature
  -> require SubtractiveExtrude
  -> source Sketch
  -> require exactly one CircleProfile and one total profile
  -> require a length diameter parameter
  -> WorkplaneResolver::resolve_for_sketch
  -> map CircleProfile.center into the workplane frame
  -> apply ExtrudeDirection to the workplane normal
  -> ComponentLocalAxisDescriptor
  + separate component RigidTransform
```

Target component and part ownership are resolved before feature geometry, preserving the existing deterministic error precedence.

## Assembly-space axis evaluation

`AssemblyTransformEvaluator` adds:

```text
AssemblySpaceAxisDescriptor
  origin
  direction

evaluate_axis(RigidTransform, ComponentLocalAxisDescriptor)
```

The canonical transform convention is unchanged:

```text
active right-handed fixed-axis rotations
X, then Y, then Z
R = Rz * Ry * Rx
```

Axis evaluation is:

```text
axis.origin_assembly
  = evaluate_point(transform, local_axis.origin)

axis.direction_assembly
  = evaluate_vector(transform, local_axis.direction)
```

Therefore translation affects the axis origin but not the direction.

Rigid rotation preserves direction magnitude within floating-point tolerance.

## Concentric equation API

The dedicated read-only builder is:

```text
AssemblyConcentricConstraintEquationBuilder
  build(Project, AssemblyConstraint)
    -> AssemblyConcentricConstraintEquationDescriptor
```

The descriptor contains:

```text
AssemblySpaceAxisConstraintTargetDescriptor
  component_instance
  semantic_reference
  axis : AssemblySpaceAxisDescriptor

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

Inactive records and non-Concentric types fail before target resolution.

Target A is resolved before target B, so failure precedence remains deterministic.

## Canonical Concentric residual convention

For assembly-space axis lines:

```text
oA = target A axis origin
dA = target A unit axis direction

oB = target B axis origin
dB = target B unit axis direction
```

The residuals are:

```text
direction_parallelism = cross(dA, dB)
axis_offset_mm         = cross(oB - oA, dA)
```

A satisfied Concentric relationship has:

```text
direction_parallelism = (0, 0, 0)
axis_offset_mm         = (0, 0, 0)
```

## Direction parallelism

```text
cross(dA, dB) = 0
```

accepts both:

```text
dB = dA
```

and:

```text
dB = -dA
```

Concentric therefore aligns axis lines without imposing a same-direction or opposed-direction orientation convention.

That is deliberate. A separate constraint family may later impose orientation or angular direction when required.

## Perpendicular axis offset

```text
cross(oB - oA, dA)
```

removes the component of the origin delta parallel to target A's axis.

Therefore axial displacement is not a Concentric residual.

Example:

```text
oA = (0, 0, 0)
dA = (0, 0, 1)

oB = (0, 0, 25)
dB = (0, 0, 1)
```

produces zero residual.

The two axes are the same infinite line even though their chosen origins differ by 25 mm along the line.

For:

```text
oB = (3, 0, 25)
```

the offset residual is:

```text
cross((3, 0, 25), (0, 0, 1))
  = (0, -3, 0) mm
```

The axis lines are parallel but separated laterally by 3 mm.

## Remaining Concentric freedoms

For one free rigid body relative to one fixed body, a regular Concentric relationship locally constrains four independent rigid-body directions:

```text
two rotational directions that tilt the axis
two translational directions perpendicular to the axis
```

It deliberately leaves:

```text
translation along the axis
rotation about the axis
```

free.

The vector residual representation has six scalar components, but only four are locally independent in the regular case. The existing Jacobian-rank diagnostics are specifically designed not to equate residual component count with constrained DOF.

This expected rank behavior will be proved when Concentric is integrated into the shared numeric system and solver.

## Target-order behavior

The satisfaction condition is symmetric, but the raw `axis_offset_mm` vector is target-order and direction dependent because it uses target A's direction:

```text
cross(oB - oA, dA)
```

Swapping A and B may reverse the residual vector.

This is acceptable and intentional. Persistent target A/B order is preserved, and later numeric consumers must keep the documented residual convention rather than silently reorder targets.

## Separate planar and Concentric builders

The existing:

```text
AssemblyConstraintEquationBuilder
```

remains the planar Mate/Distance builder consumed by the first rigid-body solver.

The new:

```text
AssemblyConcentricConstraintEquationBuilder
```

owns the semantic-axis Concentric path.

This separation is deliberate for this MVP increment:

```text
semantic axis and Concentric residual semantics
  -> implemented and stable

Concentric numeric flattening and solver participation
  -> next block
```

The existing planar builder's Concentric rejection path remains unchanged so the already-tested solver and DOF-diagnostics behavior does not silently change before the numeric system explicitly supports the new residual family.

## Read-only and persistence boundary

This block does not:

- mutate component transforms
- change component grounding, visibility, or suppression
- rewrite assembly constraints
- reorder target A/B intent
- change part parameters
- change sketches or profiles
- change feature intent
- add derived workplanes
- execute OCCT topology queries
- persist local axis descriptors
- persist assembly-space axes
- persist Concentric residual descriptors
- run the rigid-body solver
- apply proposed transforms
- persist Jacobian or DOF data

All axis and residual descriptors are regenerated from current project model intent.

No new assembly or project JSON field is added.

The only persistent data used by this block already exists:

```text
component_instances[].transform
assembly_constraints[].target_a.semantic_reference
assembly_constraints[].target_b.semantic_reference
part sketch/profile/feature intent
```

## Failure behavior

Axis resolution fails explicitly when:

- the component instance does not exist
- the referenced part is not project-owned
- the axis token is malformed
- the token suffix is not `axis`
- the source feature does not exist
- the source feature is not a subtractive extrude
- the source feature input sketch does not exist
- the source sketch does not contain exactly one `CircleProfile` and one total profile
- the circle diameter does not resolve to a length parameter
- the source sketch workplane cannot be resolved

Concentric equation construction fails before target resolution when:

- the constraint is inactive
- the constraint type is not Concentric

Target-resolution errors are propagated unchanged.

## Tests

`tests/geometry/assembly_concentric_constraint_equation_builder_tests.cpp` covers:

- stable `feature.<feature-id>.axis` resolution
- component, part, feature, and source-profile identity
- CircleProfile-center mapping into component-local axis geometry
- sketch-normal axis direction
- opposite-extrude-direction axis reversal
- separate persisted component transform preservation
- assembly-space axis origin translation
- assembly-space axis direction rotation
- satisfied Concentric axes with axial origin separation
- satisfied Concentric axes with opposed directions
- parallel offset axes
- nonparallel axes with coincident origins
- deterministic target A then target B construction
- target-order-sensitive offset-vector sign
- repeated deterministic equation construction
- unchanged project transforms, constraints, and part workplane intent
- malformed axis tokens
- unsupported axis suffixes
- missing source features
- unsupported source feature types
- non-circle source profiles
- inactive-constraint validation before target resolution
- non-Concentric type validation before target resolution

Core tests also cover `SemanticAxisReference` identity and the stable `feature.hole.axis` node id.

Targeted commands after building:

```bash
./build/dev/blcad_core_tests "[core][semantic-axis]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric]"
```

Complete geometry workflow:

```bash
cmake --workflow --preset dev-geometry-build-test
```

## Deliberate limitations

This block does not implement:

- Concentric residual flattening in `AssemblyConstraintNumericSystem`
- Concentric finite-difference Jacobian evaluation through the solver
- Concentric rigid-body solving
- Concentric remaining-DOF diagnostics
- Concentric solve-result application tests
- analytic Jacobians
- per-component null-space direction labels
- pattern-hole semantic axis identities
- cylindrical additive-feature axis producers
- explicit construction-axis assembly targets
- Insert constraints
- Angle, Tangent, Flush, Coincident, or Lock constraints
- joints or motion
- collision/interference analysis
- subassemblies
- component geometry instancing
- assembly-level STEP export

## Next technical step

The next assembly block is explicit Concentric integration into the shared numeric residual/Jacobian system, rigid-body solver, and remaining-DOF diagnostics.

That block should:

- teach the shared numeric system to select the dedicated Concentric builder for Concentric records
- flatten `direction_parallelism` first and scaled `axis_offset_mm` second in one documented scalar order
- apply the existing millimeter residual scale to the three offset components
- keep Mate/Distance flattening unchanged
- solve Concentric groups through the existing deterministic finite-difference and damped Gauss-Newton path
- prove the regular one-free-body Concentric Jacobian rank is four with two remaining DOF
- preserve all existing grounding, suppression, snapshot, stale-result, and explicit application contracts
- test parallel offset correction, axis tilt correction, opposed directions, axial freedom, rotation-about-axis freedom, deterministic ordering, non-convergence, and read-only diagnostics
- keep Insert deferred until axial seating semantics are explicitly defined on top of stable Concentric behavior

No persistent solver, Jacobian, or DOF cache is required for that next block.

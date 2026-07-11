# Semantic Generated Axes and Concentric Residuals MVP-5

Status: implemented stable generated-axis semantic references for the first supported circular-cut feature family, deterministic Concentric target/residual construction, and downstream integration into the shared numeric solver and local DOF diagnostics.

## Goal

This block defines the semantic geometry and geometric residual contract for Concentric relationships.

It answers:

```text
Which feature-produced axis does a persistent assembly target mean?
Where is that axis in component-local coordinates?
Where is it in assembly coordinates under the persisted RigidTransform?
What deterministic residual data describes Concentric error between two axes?
```

The axis and residual builders remain read-only. A separate shared numeric layer now consumes the resulting descriptors for solving and diagnostics.

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

Core semantic identity:

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

The enum leaves room for future feature families with more than one named axis while preserving the rule that references describe BLCAD model intent rather than kernel topology ids.

## First supported axis producer

The first feature family exposing `feature.<feature-id>.axis` is:

```text
FeatureType::SubtractiveExtrude
+ exactly one CircleProfile in the input sketch
+ exactly one total profile in that sketch
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

The circle diameter parameter must resolve to a length parameter. Diameter does not change the axis line, but requiring valid circular-profile intent prevents an invalid circular cut record from becoming a valid axis producer accidentally.

## Why additive extrudes are not generic axis producers

The current additive-extrude geometry path supports rectangle, closed, arc-closed, composite, and detected-region profiles. It does not currently execute a single-circle additive extrusion as a cylinder feature.

Therefore no cylindrical axis is invented for every additive extrude.

A semantic axis is exposed only where current feature intent has an unambiguous axis-producing meaning.

## Why circular hole patterns are not mapped to one `axis`

`CircularHolePattern` produces several distinct hole axes.

A token such as:

```text
feature.bolt_pattern.axis
```

would be ambiguous.

Pattern-axis references require stable per-instance semantic identity. That identity is deferred rather than inferred from transient OCCT topology or hidden vector order.

## Component-local axis descriptor

Axis target resolution uses:

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

`source_profile` preserves the exact `CircleProfile` producing the semantic axis.

The local axis and persisted component transform remain separate.

## Axis target resolution API

`AssemblyConstraintTargetResolver` provides explicit geometry-family methods:

```text
resolve(Project, AssemblyConstraintTarget)
  -> generated planar face target

resolve_axis(Project, AssemblyConstraintTarget)
  -> generated axis target
```

The APIs remain separate so a caller cannot accidentally reinterpret a plane as an axis or vice versa.

Axis resolution:

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

Target component and part ownership are resolved before feature geometry, preserving deterministic error precedence.

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

Axis evaluation:

```text
axis.origin_assembly
  = evaluate_point(transform, local_axis.origin)

axis.direction_assembly
  = evaluate_vector(transform, local_axis.direction)
```

Translation affects the axis origin but not its direction.

Rigid rotation preserves direction magnitude within floating-point tolerance.

## Concentric equation API

The dedicated read-only builder is:

```text
AssemblyConcentricConstraintEquationBuilder
  build(Project, AssemblyConstraint)
    -> AssemblyConcentricConstraintEquationDescriptor
```

Descriptor types:

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

Target A resolves before target B, preserving deterministic failure precedence.

## Canonical Concentric residual convention

For assembly-space axis lines:

```text
oA = target A axis origin
dA = target A unit axis direction

oB = target B axis origin
dB = target B unit axis direction
```

Residuals:

```text
direction_parallelism = cross(dA, dB)
axis_offset_mm         = cross(oB - oA, dA)
```

A satisfied Concentric relationship has both vectors equal to zero.

## Direction parallelism

```text
cross(dA, dB) = 0
```

accepts both:

```text
dB = dA
dB = -dA
```

Concentric aligns axis lines without imposing a same-direction or opposed-direction orientation convention.

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

## Remaining Concentric freedoms and proven rank

For one free rigid body relative to one fixed body, a regular Concentric relationship locally constrains four independent rigid-body directions:

```text
two rotational directions that tilt the axis
two translational directions perpendicular to the axis
```

It deliberately leaves:

```text
translation along the common axis
rotation about the common axis
```

free.

The vector residual representation has six scalar components, but only four are locally independent in the regular case.

The shared finite-difference Jacobian and generic rank analyzer now prove:

```text
residual_component_count = 6
variable_count           = 6
jacobian_rank            = 4
constrained_dof          = 4
remaining_dof            = 2
residual_row_redundancy  = 2
```

No Concentric-specific DOF rule is hard-coded.

## Target-order behavior

The satisfaction condition is symmetric, but raw `axis_offset_mm` is target-order and direction dependent because it uses target A's direction:

```text
cross(oB - oA, dA)
```

Swapping A and B may reverse the residual vector.

Persistent target A/B order is therefore preserved through graph lookup, residual construction, numeric flattening, solver evaluation, and diagnostics.

## Separate residual builders, shared numeric consumer

The planar builder remains:

```text
AssemblyConstraintEquationBuilder
```

for Mate and Distance.

The axis-line builder remains:

```text
AssemblyConcentricConstraintEquationBuilder
```

for Concentric.

The private shared numeric system now selects the appropriate builder from persistent `AssemblyConstraintType`.

Exact Concentric numeric order:

```text
direction_parallelism.x
direction_parallelism.y
direction_parallelism.z
axis_offset_mm.x / length_residual_scale_mm
axis_offset_mm.y / length_residual_scale_mm
axis_offset_mm.z / length_residual_scale_mm
```

The same flattened residual evaluator is used by:

```text
AssemblyRigidBodySolver
AssemblySolveDiagnosticsAnalyzer
```

Geometry-family semantics remain explicit while numeric interpretation is shared.

Canonical downstream integration document:

```text
docs/assembly-concentric-numeric-solver-dof-mvp5.md
```

## Solver behavior

The shared rigid-body solver now corrects:

```text
lateral axis offset
axis tilt
```

It preserves the Concentric-defined freedoms rather than adding hidden axial or rotational residuals.

Equal and opposed coincident axis directions are valid initial states.

All-grounded unsatisfied Concentric geometry is reported as `FixedGeometryInconsistent`.

Non-converged Concentric solve results remain read-only and cannot be applied.

A converged Concentric result uses the existing `AssemblySolveResultApplier` snapshot, stale-result, and atomic-application contracts.

## Read-only and persistence boundary

Axis resolution and Concentric residual construction do not mutate project intent.

The downstream solver mutates only private `Project` copies until explicit successful result application.

The following are unpersisted derived data:

- semantic-axis parser interpretation
- component-local axis descriptors
- assembly-space axis descriptors
- Concentric residual descriptors
- flattened Concentric residual scalars
- finite-difference Jacobians
- solve iteration state and results
- unapplied transform proposals
- Jacobian-rank and remaining-DOF diagnostics

No new assembly or project JSON field is added for this axis/residual/numeric integration.

Persistent inputs remain:

```text
component_instances[].transform
assembly_constraints[].target_a.semantic_reference
assembly_constraints[].target_b.semantic_reference
part sketch/profile/feature intent
```

Only explicit application of a fresh converged solve result changes the existing component transform field.

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

Target-resolution failures propagate unchanged through the shared numeric system, solver, and diagnostics.

For example:

```text
bolt.main_axis
```

currently fails with:

```text
unsupported assembly semantic axis reference family
```

## Tests

Semantic axis and geometric residual suite:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric]"
```

Numeric solver and DOF integration suite:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric-solver]"
```

Coverage includes:

- stable `feature.<feature-id>.axis` resolution
- source feature/profile identity
- CircleProfile-center mapping
- extrude-direction axis orientation
- assembly-space axis evaluation
- equal and opposed direction semantics
- parallel offset and nonparallel residual construction
- target-order-sensitive offset vectors
- deterministic read-only equation construction
- exact six-scalar numeric flattening
- length residual scaling
- mixed residual ordering and dimension
- lateral axis-offset solving
- axis-tilt solving
- preserved regular axial slide and rotation-about-axis freedoms
- source-project immutability
- explicit successful result application
- fixed Concentric inconsistency
- non-converged Concentric boundaries
- regular Jacobian rank `4/6`
- two remaining local DOF
- mixed Distance/Concentric rank `5/6`
- semantic target failure propagation

Core tests also cover `SemanticAxisReference` identity and the stable `feature.hole.axis` node id.

## Deliberate limitations

This axis/Concentric path does not implement:

- analytic Concentric Jacobians
- explicit null-space bases or semantic per-component DOF labels
- pattern-hole semantic axis identities
- cylindrical additive-feature axis producers
- explicit construction-axis assembly targets
- Insert constraints or axial seating semantics
- richer constraint families
- joints or motion
- collision/interference analysis
- subassemblies
- component geometry instancing
- assembly-level STEP export

## Next technical step

The next assembly block is stable Insert constraint intent and a read-only composite Insert residual model.

That block should first define stable semantic axial-seating geometry for the supported circular-cut feature family, then define explicit persistent Insert relationship semantics and a derived residual combining axis alignment with signed seating-plane separation.

The regular composite residual should prove local rank five over six rigid-body variables, leaving only rotation about the common axis free, before Insert is connected to the shared numeric solver.

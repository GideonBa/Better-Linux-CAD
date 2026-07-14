# Assembly Constraint Target Resolution MVP-5

Status: implemented read-only generated planar-face resolution, primary circular-feature axis resolution, and primary circular-feature seating/composite Insert endpoint resolution. The broader typed target/capability expansion is planned in `docs/assembly-general-geometric-target-roadmap.md`.

## Goal

`AssemblyConstraintTargetResolver` bridges persistent semantic assembly target strings to deterministic component-local geometry.

It owns semantic target lookup and local geometry construction only. It does not apply component placement, construct residuals, decide solver participation, optimize transforms, or compute DOF.

## Current API

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

Supported current token:

```text
feature.<feature-id>.axis
```

The current producer is:

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

Supported current token:

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

Insert and the current Revolute joint seed require axis-line alignment plus an oriented seating/reference frame.

A persistent endpoint such as `feature.hole.seat` identifies one exact circular feature/profile. From that constructive identity, the primary axis and seating plane are deterministic and inseparable for this feature family.

The record therefore does not store a hidden second target and does not expand Insert into four target strings.

The composite derived descriptor makes the coupling explicit while keeping persistent intent compact.

## Why circular-hole patterns are currently excluded

`CircularHolePattern` produces several distinct holes.

Tokens such as:

```text
feature.pattern.axis
feature.pattern.seat
```

would be ambiguous.

A future generated-topology target family must define stable per-instance semantic identity. The resolver does not infer identity from transient OCCT topology or hidden vector order.

This requirement is explicitly planned in Block 35 of `docs/assembly-general-geometric-target-roadmap.md`.

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

Occurrence-qualified cross-hierarchy target resolution later reuses the same local semantic meaning and applies the exact component plus parent transform chain into root-assembly space.

## Current downstream use

Planar branch:

```text
resolve
  -> evaluate_plane
  -> AssemblyConstraintEquationBuilder
  -> Mate / Distance / Angle residuals
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

Seat branch:

```text
resolve_insert
  -> evaluate_axis + evaluate_plane
  -> AssemblyInsertConstraintEquationBuilder
  -> Insert residuals
  -> shared numeric system
  -> solver / DOF diagnostics
```

The same `.seat` semantic family also feeds the shared local/cross-hierarchy Revolute residual path.

Mate, Distance, Angle, Concentric, Insert, and Revolute execution are already integrated in their documented local/cross-hierarchy numeric or motion paths.

## Current expressiveness boundary

The current resolver is intentionally narrower than an Inventor-/SolidWorks-like assembly selector.

Current assembly-selectable source families are effectively:

```text
generated planar feature face
narrow generated circular-feature axis
narrow generated circular-feature seat frame
```

The following are not yet general assembly target sources:

```text
DatumPlane
DatumAxis
construction line
construction point
generated cylindrical face
generated linear edge
generated circular edge
generated vertex
```

The solver architecture is not the primary blocker. The missing layer is a general semantic geometric target taxonomy, stable source identity for generated subelements, and an explicit target compatibility matrix.

## Planned general target architecture

Canonical roadmap:

`docs/assembly-general-geometric-target-roadmap.md`

The refined post-Block-30 sequence is:

```text
31 typed geometric target taxonomy and capability projection
  -> 32 assembly-selectable reference geometry Core intent
  -> 33 reference geometry serialization and structure validation
  -> 34 datum/axis/line/point target resolution
  -> 35 stable semantic generated topology identity/recovery
  -> 36 generated face/edge/vertex target resolution
  -> 37 explicit target compatibility matrix
  -> 38 generic geometric relationship Core intent + JSON
  -> 39 generic relationship equations + shared solve integration
  -> 40 joint target compatibility + oriented Frame contract (implemented)
  -> 41 general joint coordinate/limit Core model (implemented)
  -> 42 general joint coordinate JSON/backward compatibility (implemented)
  -> 43 vector joint drives + holding/freshness/atomic application (implemented)
  -> 44 Prismatic joint
  -> 45 Cylindrical joint
  -> 46 Planar joint
  -> 47 Ball/Spherical joint
```

The core distinction is:

```text
selected semantic source kind
  !=
solver geometric capability
```

Examples:

```text
GeneratedCylindricalFace
  -> Axis capability
  -> Cylinder capability

GeneratedCircularEdge
  -> Circle capability
  -> Axis capability
  -> center Point capability

DatumPlane
  -> Plane capability

CircularFeatureSeat
  -> Frame capability
  -> Axis capability
  -> Plane capability
```

Relationship equation builders consume requested capabilities through the Block-37 compatibility layer rather than branching on feature/source kind.

The first new persistent generic relationship types are planned only in Block 38, after compatibility semantics are stable. Their equations and numeric integration follow in Block 39.

Block 40 joint target compatibility is implemented with deterministic oriented `Frame / Frame` selection and explicit Axis-only rejection. Blocks 41–43 implement family-defined typed coordinates, additive compatible JSON, and shared vector drives. Blocks 44–46 implement Prismatic, Cylindrical, and Planar on that boundary; Block 47 implements passive `Point / Point` Spherical center coincidence. Blocks 48–73 Part Construction through DraftFeature Core intent/JSON are implemented; Block 74 DraftFeature Geometry is next.

## Failure behavior

Shared failures include:

- target component does not exist;
- referenced part is not project-owned.

Planar failures include malformed/unsupported face tokens, missing source features, unsupported feature type, and generated-face resolution failures.

Axis failures include malformed axis tokens, unsupported suffixes, missing source features/sketches, wrong feature type, ambiguous/non-circle profile content, invalid diameter parameters, and unresolved source workplanes.

Seat/Insert target failures use parallel explicit messages for malformed/unsupported seating tokens and unsupported circular-feature source intent.

Failure values propagate through downstream builders unchanged.

The future general target taxonomy retains fail-closed unsupported capability projection and relationship/joint compatibility behavior.

## Read-only and persistence boundary

Resolution does not:

- mutate component transforms or state;
- change constraints or target strings;
- modify part parameters, sketches, profiles, features, or workplanes;
- execute a solver;
- own `ShapeCache`;
- persist resolved descriptors.

Resolved plane, axis, and seat descriptors are regenerated from current model intent.

No solver or Jacobian cache owns persistent target bindings.

Future Plane/Axis/Line/Point/Circle/Cylinder/Frame capabilities likewise remain derived. Assembly endpoints continue to persist semantic reference identity, not resolved coordinates or raw OCCT topology.

## Tests

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-target]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-insert]"
```

Current coverage includes all six generated faces, primary circular axis identity, primary circular seat identity, `CircleProfile`-center mapping, extrude-direction orientation, right-handed opposite seat frames, source feature/profile preservation, unsupported source families, determinism, read-only behavior, and downstream assembly-space evaluation.

## Current handoff

The implemented target resolver supports generated plane/axis/seat semantics and all current Mate/Distance/Angle/Concentric/Insert/Revolute/Prismatic/Cylindrical/Planar consumers.

Block 30 remains the immediate next technical step for richer contact/swept-motion analysis.

After Block 30, Block 31 begins the general assembly target roadmap with a typed derived geometric target taxonomy and explicit capability projection. Existing target strings and current relationship behavior remain backward compatible.

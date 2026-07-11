# Stable Insert Intent and Composite Residual Semantics MVP-5

Status: implemented persistent Insert relationship intent, stable circular-feature seating-plane references, composite axis/seat target resolution, and deterministic read-only Insert residual construction. The follow-up integration into the shared numeric system, rigid-body solver, result application, and DOF analyzer is implemented in `docs/assembly-insert-numeric-solver-dof-mvp5.md`.

## Goal

Insert must be explicit CAD model intent rather than an undocumented Concentric constraint plus a hidden axial offset.

This block answers:

```text
What persistent relationship type means Insert?
Which stable feature-produced geometry is one Insert endpoint?
How are the endpoint axis and axial seating plane derived?
What deterministic residual data describes Insert error?
Which local rigid-body freedom remains in the regular case?
```

The block stabilizes target and residual semantics before numeric solver integration.

## Persistent relationship intent

`AssemblyConstraintType` now includes:

```text
Insert
```

String/JSON spelling:

```text
insert
```

An Insert record uses the existing `AssemblyConstraintTarget` A/B fields and carries no distance quantity.

Representative JSON:

```json
{
  "id": "constraint.insert",
  "name": "Insert",
  "type": "insert",
  "target_a": {
    "component_instance": "component.plate.1",
    "semantic_reference": "feature.hole.seat"
  },
  "target_b": {
    "component_instance": "component.plate.2",
    "semantic_reference": "feature.hole.seat"
  },
  "state": "active"
}
```

The historical assembly schema/version marker remains unchanged because the enum value is an additive interpretation of the existing constraint record shape.

## Stable semantic seating-plane family

The first seating-plane token family is:

```text
feature.<feature-id>.seat
```

Core identity:

```text
SemanticSeatingPlane::Primary
SemanticSeatingPlaneReference
  source_feature
  plane
  node_id()
```

Example:

```text
SemanticSeatingPlaneReference(feature.hole, Primary)
  -> feature.hole.seat
```

The token is constructive feature intent. It is not a `TopoDS_Face`, cylindrical-face id, wire id, or transient topology index.

## First supported producer

The first `.seat` producer is intentionally the same narrow circular-cut feature family that already exposes `.axis`:

```text
FeatureType::SubtractiveExtrude
+ source sketch contains exactly one CircleProfile
+ source sketch contains exactly one total profile
+ circle diameter resolves to a length parameter
```

This corresponds to the existing first circular through-all cut path.

`CircularHolePattern` remains excluded because one pattern produces multiple distinct hole instances. A stable per-instance pattern target family is still required before pattern seats or axes can be referenced.

## Composite Insert endpoint contract

One Insert endpoint stores one semantic target string:

```text
feature.hole.seat
```

`AssemblyConstraintTargetResolver::resolve_insert` interprets that one target as a composite circular-feature endpoint and derives:

```text
ResolvedAssemblyInsertConstraintTarget
  component_instance
  referenced_part_document
  source_feature
  source_profile
  axis = SemanticAxis::Primary
  seating_plane = SemanticSeatingPlane::Primary
  local_axis
  local_seating_plane
  component_transform
```

The endpoint therefore identifies one feature-produced primary axis and one oriented seating plane from the same exact source feature/profile identity.

No second hidden target is stored in the constraint record.

## Component-local axis geometry

Let the resolved source-sketch workplane be:

```text
origin
x_axis
y_axis
normal
```

and let the `CircleProfile.center` be mapped through that workplane.

Then:

```text
axis.origin
  = WorkplaneResolver::evaluate_point(workplane, CircleProfile.center)

axis.direction
  = workplane.normal
    for SketchNormal

  = -workplane.normal
    for OppositeSketchNormal
```

This matches the existing semantic generated-axis convention.

## Component-local seating plane

The seating plane passes through the circular feature center on the source sketch plane:

```text
seat.origin = axis.origin
```

Its normal is canonically aligned with the endpoint's primary axis direction:

```text
seat.normal = axis.direction
```

For `SketchNormal`:

```text
seat.x_axis = workplane.x_axis
seat.y_axis = workplane.y_axis
seat.normal = workplane.normal
```

For `OppositeSketchNormal`:

```text
seat.x_axis =  workplane.x_axis
seat.y_axis = -workplane.y_axis
seat.normal = -workplane.normal
```

Flipping Y together with the normal preserves a right-handed frame:

```text
cross(seat.x_axis, seat.y_axis) = seat.normal
```

The seating plane is not inferred from an OCCT opening face after BRep generation. It is regenerated from stable sketch/feature intent.

## Assembly-space endpoint evaluation

The resolver remains component-local and preserves the component transform separately.

The dedicated Insert builder evaluates both geometry families through the existing transform convention:

```text
local_axis
  -> AssemblyTransformEvaluator::evaluate_axis
  -> AssemblySpaceAxisDescriptor

local_seating_plane
  -> AssemblyTransformEvaluator::evaluate_plane
  -> AssemblySpacePlanarDescriptor
```

The persisted transform convention remains:

```text
translation_mm in millimeters
rotation_deg in degrees
active right-handed fixed-axis rotations
X then Y then Z
R = Rz * Ry * Rx
```

No Insert-specific transform convention exists.

## Insert equation API

Public header:

```text
include/blcad/geometry/assembly_insert_constraint_equation_builder.hpp
```

API family:

```text
AssemblySpaceInsertConstraintTargetDescriptor
  component_instance
  semantic_reference
  source_feature
  source_profile
  axis
  seating_plane

InsertResidualDescriptor
  direction_parallelism
  axis_offset_mm
  signed_seating_separation_mm

AssemblyInsertConstraintEquationDescriptor
  constraint
  target_a
  target_b
  residual

AssemblyInsertConstraintEquationBuilder
  build(Project, AssemblyConstraint)
```

The builder accepts only active `AssemblyConstraintType::Insert` records.

Target A is resolved before target B so failure precedence remains deterministic.

## Canonical composite Insert residual

For assembly-space endpoints:

```text
oA = target A axis origin
dA = target A axis direction
sA = target A seating-plane origin
nA = target A seating-plane normal

oB = target B axis origin
dB = target B axis direction
sB = target B seating-plane origin
```

with the canonical invariant:

```text
nA = dA
nB = dB
```

Insert residuals are:

```text
direction_parallelism
  = cross(dA, dB)

axis_offset_mm
  = cross(oB - oA, dA)

signed_seating_separation_mm
  = dot(sB - sA, nA)
```

A satisfied Insert relationship has all three residual fields equal to zero.

## Axis alignment semantics

The first two residuals are exactly the stable Concentric axis-line convention:

```text
cross(dA, dB)
cross(oB - oA, dA)
```

Therefore equal and opposed axis directions are accepted:

```text
dB = dA
or
dB = -dA
```

Insert aligns the two infinite axis lines without imposing a same-direction orientation convention.

## Signed seating semantics

The seating residual is:

```text
dot(sB - sA, nA)
```

Target A defines the positive seating direction.

Example:

```text
nA = (0, 0, 1)
sA = (0, 0, 0)
sB = (0, 0, 12)
```

produces:

```text
signed_seating_separation_mm = +12 mm
```

Swapping target A and B for equal axis directions produces `-12 mm`.

This target-order sensitivity is intentional and preserves persistent A/B relationship intent.

Tangential separation of seat origins is not measured by the seating scalar because lateral displacement is already owned by `axis_offset_mm`.

## Why there is no separate seating-normal residual

The seating-plane normal is canonically derived from the same feature direction as the primary semantic axis:

```text
seat.normal = axis.direction
```

Axis direction parallelism already constrains the two independent tilt directions.

Adding another plane-normal vector residual would duplicate orientation information without adding a new regular rigid-body constraint direction.

The composite residual therefore contains:

```text
3 direction components
3 axis-offset components
1 signed seating component
= 7 scalar residual components
```

but only five are locally independent in the regular one-free-body case.

## Regular local rank and remaining freedom

For one free rigid body relative to one fixed body, a regular Insert relationship constrains:

```text
2 axis-tilt rotations
2 translations perpendicular to the axis
1 translation along the axis through seating
```

It leaves:

```text
rotation about the common axis
```

free.

The focused geometry test constructs the seven-scalar Insert residual directly, central-finite-differences all six persisted `RigidTransform` variables, and computes matrix rank from the resulting `7 x 6` Jacobian.

Proven regular result:

```text
variable_count           = 6
residual_component_count = 7
jacobian_rank            = 5
remaining_local_dof      = 1
```

The rank is measured from the residual Jacobian. It is not hard-coded from the word `Insert`.

A pure rotation about the already-aligned common axis remains a zero-residual state in the regular centered-axis test.

## Read-only boundary

Target resolution and Insert equation construction do not:

- mutate component transforms
- change component visibility, suppression, or grounding
- rewrite constraint target order
- change part parameters, sketches, profiles, or features
- execute the rigid-body solver
- apply transform proposals
- persist local target descriptors
- persist assembly-space endpoint geometry
- persist Insert residual descriptors
- persist Jacobian or DOF values

All target and residual data is regenerated from current model intent and placement.

## Numeric-system boundary

At the time of this block the shared `AssemblyConstraintNumericSystem` consumed only Mate, Distance, and Concentric; Insert reached an explicit planar-builder rejection. That boundary was intentional: target identity and residual semantics were stabilized before solver participation changed.

The boundary has since been removed by the follow-up block: Insert is now selected and flattened by the same shared numeric system. See `docs/assembly-insert-numeric-solver-dof-mvp5.md`.

## Tests

Core intent tests:

```bash
./build/dev/blcad_core_tests "[core][assembly-insert]"
./build/dev/blcad_core_tests "[core][semantic-seat]"
```

Geometry/residual tests:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-insert]"
```

Coverage includes:

- stable `feature.<feature-id>.seat` identity
- Insert type string and distance-free validation
- assembly JSON roundtrip
- no placement mutation during record creation/load
- exact source feature/profile preservation
- CircleProfile-center seat/axis origin
- seating normal aligned with semantic axis
- right-handed opposite-extrude seat frame
- assembly-space axis and seating evaluation through existing transform semantics
- satisfied aligned Insert state
- equal/opposed axis acceptance
- lateral axis-offset residual
- axis-tilt residual
- signed axial seating residual
- target-order sign reversal
- deterministic repeated construction
- read-only behavior
- inactive/type validation before target resolution
- unsupported semantic target family
- unsupported non-circle source profile
- direct central finite-difference `7 x 6` residual Jacobian
- regular Jacobian rank `5`
- rotation-about-axis freedom
- shared-solver participation (added by the follow-up integration block) and source-project immutability

## Deliberate limitations

This block does not implement:

- Insert numeric residual flattening
- Insert solver participation
- Insert result-application tests
- Insert DOF analyzer integration
- Insert-specific numeric weights
- nonzero Insert axial offset parameters
- same-direction versus opposed-direction Insert modes
- pattern-hole seat/axis instance references
- generic cylindrical feature seats
- shaft/revolve semantic axes
- counterbore/countersink shoulder selection
- joints, motion, or collision

## Follow-up block

The follow-up block — Insert numeric-system, rigid-body solver, explicit application, and DOF-diagnostics integration — is implemented; see `docs/assembly-insert-numeric-solver-dof-mvp5.md` and the current next MVP in `docs/mvp-plan.md`.

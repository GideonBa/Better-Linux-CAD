# Assembly Geometric Target Taxonomy and Capability Projection MVP-5

Status: implemented as Block 31 of `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

This document is canonical for the Geometry-layer derived assembly target taxonomy, typed geometric descriptors, source-to-capability projection matrix, local/root-space resolved target contract, and compatibility adaptation of the existing generated face, `.axis`, and `.seat` families.

## Scope

Implemented:

- derived `AssemblyGeometricTargetSourceKind` classification;
- derived `AssemblyGeometricTargetCapability` values;
- exact local and occurrence-qualified endpoint identity retention;
- explicit component-local versus root-assembly coordinate space;
- typed Plane, Axis, Line, Point, Circle, Cylinder, and Frame descriptors;
- one `AssemblyResolvedGeometricTarget` value/variant boundary;
- canonical deterministic capability ordering;
- explicit fail-closed capability projection helpers;
- all six current generated planar-face tokens as `GeneratedPlanarFace -> Plane`;
- the current single-circle subtractive-extrude `.axis` producer as `GeneratedCylindricalFace -> Axis + Cylinder`;
- current `.seat` as `CircularFeatureSeat -> Plane + Axis + Frame`;
- local component-space typed resolution;
- exact hierarchy/root-space typed resolution;
- compatibility adapters for existing local/hierarchy `resolve`, `resolve_axis`, and `resolve_insert` APIs;
- existing equation/residual consumers routed through the projection boundary;
- source Project immutability.

Not implemented:

- new semantic-reference grammar;
- assembly-selectable DatumPlane/DatumAxis/ConstructionLine/ConstructionPoint source intent;
- first-class `DatumAxis` Core records;
- JSON/save-format changes;
- stable generated cylindrical-face/edge/vertex topology identity;
- relationship compatibility matrix;
- Coincident, Parallel, or Perpendicular intent/equations;
- richer joint families.

## Authority boundary

Persistent endpoint authority remains Core model intent.

Local:

```text
(local ComponentInstanceId,
 semantic_reference)
```

Occurrence-qualified:

```text
(occurrence_path,
 local ComponentInstanceId,
 semantic_reference)
```

Block 31 does not replace those identities and does not persist a source kind, descriptor, capability vector, coordinate space, or transform context.

Derived Geometry products are:

```text
AssemblyGeometricTargetSourceKind
AssemblyGeometricTargetCapability
AssemblyGeometricTargetSourceMetadata
AssemblyGeometricTargetDescriptor
AssemblyResolvedGeometricTarget
capability vectors
project_* projection results
```

They are regenerated from the current Project/PartDocument model intent and exact semantic endpoint string.

## Source kind versus capability

Source kind answers:

```text
which semantic model source produced this target?
```

Capability answers:

```text
which typed geometric primitive may a consumer request?
```

Implemented source kinds:

```text
GeneratedPlanarFace
GeneratedCylindricalFace
GeneratedLinearEdge
GeneratedCircularEdge
GeneratedVertex
DatumPlane
DatumAxis
ConstructionLine
ConstructionPoint
CircularFeatureSeat
```

Implemented capabilities in global canonical order:

```text
Plane
Axis
Line
Point
Circle
Cylinder
Frame
```

A source kind is not itself an equation primitive. Consumers obtain geometry through explicit capability projection.

## Canonical capability matrix

```text
GeneratedPlanarFace
  -> Plane

GeneratedCylindricalFace
  -> Axis
  -> Cylinder

GeneratedLinearEdge
  -> Line

GeneratedCircularEdge
  -> Axis
  -> Point
  -> Circle

GeneratedVertex
  -> Point

DatumPlane
  -> Plane

DatumAxis
  -> Axis
  -> Line

ConstructionLine
  -> Line

ConstructionPoint
  -> Point

CircularFeatureSeat
  -> Plane
  -> Axis
  -> Frame
```

Every resolved target stores the exact canonical subsequence for its source kind. A value/order mismatch fails validation.

## Endpoint identity variants

`AssemblyGeometricTargetEndpointIdentity` is a derived variant.

Local:

```text
AssemblyLocalGeometricTargetEndpointIdentity
  component_instance
  semantic_reference
```

Hierarchy/root-space:

```text
AssemblyHierarchyGeometricTargetEndpointIdentity
  occurrence_path
  component_instance
  semantic_reference
```

The persistent semantic-reference string is retained verbatim. The hierarchy path remains the exact ordered root-to-current `SubassemblyInstanceId` sequence; the empty path remains the explicit root occurrence.

Joined path strings, OCCT topology ids, XDE labels, STEP entity ids, and `ComponentTransformAuthority` never replace endpoint identity.

## Coordinate-space contract

Resolved targets explicitly store:

```text
ComponentLocal
RootAssembly
```

Local target:

```text
coordinate_space = ComponentLocal
transforms_inner_to_outer = [component_transform]
```

The descriptor itself is component-local. The direct component transform is retained as placement context and is not already applied to the descriptor.

Hierarchy target:

```text
coordinate_space = RootAssembly
transforms_inner_to_outer =
  [T_component,
   T_inner_parent,
   ...,
   T_outer_parent]
```

The hierarchy descriptor has already been evaluated through that exact chain into root-assembly space. Retaining both coordinate-space identity and the exact chain prevents accidental double application.

## Source metadata

`AssemblyGeometricTargetSourceMetadata` currently stores:

```text
referenced_part_document
optional source_feature
optional source_profile
optional semantic_face
optional semantic_axis
optional semantic_seating_plane
```

The endpoint semantic-reference remains identity authority. Metadata is derived resolved model context for validation, compatibility adapters, diagnostics, and future presentation.

Current metadata requirements:

```text
GeneratedPlanarFace
  source_feature + semantic_face

GeneratedCylindricalFace
  source_feature + source_profile

GeneratedLinearEdge / GeneratedCircularEdge / GeneratedVertex
  source_feature

CircularFeatureSeat
  source_feature + source_profile
  + semantic_axis + semantic_seating_plane
```

Datum/reference source-specific ids and semantic-reference grammar are intentionally deferred to Block 32.

## Typed descriptor variant

`AssemblyGeometricTargetDescriptor` is an explicit variant:

```text
AssemblyPlanarTargetDescriptor
AssemblyAxisTargetDescriptor
AssemblyLineTargetDescriptor
AssemblyPointTargetDescriptor
AssemblyCircularEdgeTargetDescriptor
AssemblyCylindricalSurfaceTargetDescriptor
AssemblyFrameTargetDescriptor
```

It is not a bag of unrelated optional fields.

### Plane

```text
origin
x_axis
y_axis
normal
```

A Plane preserves the existing BLCAD workplane contract: `x_axis`, `y_axis`, and the independently oriented face `normal` are finite unit and pairwise orthogonal. Plane does **not** require `cross(x_axis, y_axis) == normal`.

This is required for historical generated-face semantics. For example the existing Bottom face is:

```text
x_axis = +X
y_axis = +Y
normal = -Z
```

and remains valid and numerically unchanged.

### Axis

```text
origin
direction
```

### Line

```text
origin
direction
```

Axis and Line are separate semantic geometric types even though both currently carry one origin and one direction.

### Point

```text
point
```

### Circle

```text
center
x_axis
y_axis
normal
radius_mm
```

Circle carries a finite positive radius and a right-handed orthonormal orientation frame.

### Cylinder

```text
axis_origin
axis_direction
radius_mm
```

### Frame

```text
origin
x_axis
y_axis
z_axis
```

Frame is finite, orthonormal, and right-handed. It is not an Euler-angle record.

## Geometric validation

Every projection first validates the complete resolved target.

Common requirements:

```text
endpoint ids/references non-empty
hierarchy path ids non-empty
coordinate space matches endpoint identity kind
transform context finite
source metadata satisfies source kind
source kind matches descriptor variant
capability vector exactly matches canonical matrix/order
```

Descriptor requirements:

```text
Plane
  finite origin
  unit x/y/normal
  x, y, normal pairwise orthogonal
  handedness intentionally not constrained

Axis / Line
  finite origin
  finite unit non-degenerate direction

Point
  finite point

Circle
  finite center
  finite positive radius
  unit pairwise-orthogonal x/y/normal
  right-handed frame

Cylinder
  finite axis origin
  finite unit non-degenerate axis direction
  finite positive radius

Frame
  finite origin
  unit pairwise-orthogonal x/y/z
  right-handed frame
```

The current unit/orthogonality/handedness tolerance is `1.0e-9`.

Malformed public aggregate values fail closed through `validate_resolved_geometric_target` and through every projector.

## Capability projection API

Supported projection functions:

```text
project_plane
project_axis
project_line
project_point
project_circle
project_cylinder
project_frame
```

Every projector performs:

```text
validate complete resolved target
  -> require requested capability in canonical source matrix
  -> derive exact typed projection
  -> return geometry
```

Unsupported projections fail closed.

Projection rules:

```text
Plane
  Plane descriptor -> Plane
  Frame descriptor -> origin/x/y/z as Plane origin/x/y/normal

Axis
  Axis descriptor -> Axis
  Circle descriptor -> center + normal
  Cylinder descriptor -> axis_origin + axis_direction
  Frame descriptor -> origin + z_axis

Line
  Line descriptor -> Line
  Axis descriptor -> same origin/direction as Line

Point
  Point descriptor -> Point
  Circle descriptor -> center

Circle
  Circle descriptor -> Circle

Cylinder
  Cylinder descriptor -> Cylinder

Frame
  Frame descriptor -> Frame
```

Equation builders do not infer geometry from source-kind enums.

## Current generated planar-face migration

Existing persistent tokens remain unchanged:

```text
feature.<feature-id>.top
feature.<feature-id>.bottom
feature.<feature-id>.right
feature.<feature-id>.left
feature.<feature-id>.front
feature.<feature-id>.back
```

The existing additive-extrude generated-face resolver and `WorkplaneResolver` remain semantic geometry authority.

Typed result:

```text
source_kind = GeneratedPlanarFace
descriptor = AssemblyPlanarTargetDescriptor
capabilities = [Plane]
```

Source metadata retains:

```text
PartDocumentId
FeatureId
SemanticFace
```

The complete generated Plane descriptor remains numerically identical to the historical resolver, including the existing independently oriented Bottom/side face normal convention.

## Current `.axis` migration

Persistent token remains:

```text
feature.<feature-id>.axis
```

Current support remains narrow:

```text
source feature is SubtractiveExtrude
source sketch exists
source sketch has exactly one profile
that profile is exactly one CircleProfile
circle diameter resolves to Length
```

This current producer is a circular subtractive-extrude cylindrical feature source. Block 31 classifies the derived producer as:

```text
GeneratedCylindricalFace
```

Descriptor:

```text
AssemblyCylindricalSurfaceTargetDescriptor
  axis_origin    = existing circle-center/workplane evaluation
  axis_direction = existing extrude-direction axis
  radius_mm      = circle_diameter_mm / 2
```

Capabilities:

```text
Axis
Cylinder
```

The endpoint still says `.axis`; `SemanticAxis::Primary` remains source metadata. No new persistent cylindrical-face token exists.

Existing Concentric behavior obtains the exact historical Axis through:

```text
resolve_geometric
  -> project_axis
  -> legacy axis descriptor adapter
```

Cylinder is additional derived Geometry information only.

## Current `.seat` migration

Persistent token remains:

```text
feature.<feature-id>.seat
```

The same single-CircleProfile subtractive-extrude preconditions remain.

Typed source:

```text
CircularFeatureSeat
```

Frame construction is:

```text
origin = existing seating-plane origin
x_axis = existing seating-plane x_axis
z_axis = existing semantic axis direction
y_axis = z_axis × x_axis
```

`Y = Z × X` canonicalizes the typed Frame to a right-handed orientation even when a historical workplane stores an independently oriented `y_axis`/normal convention such as Bottom.

Residual-relevant legacy geometry remains unchanged:

```text
Insert
  uses Axis and seating normal

Revolute
  uses seat X reference and semantic Axis Z
  plus seating normal for separation
```

Therefore canonicalizing Frame-Y does not change current Insert or Revolute numeric semantics.

Capabilities:

```text
Plane
Axis
Frame
```

Projection behavior:

```text
project_plane -> canonical seat Frame as Plane
project_axis  -> existing semantic axis
project_frame -> complete right-handed seat frame
```

For existing right-handed workplanes, including the current XY and OppositeSketchNormal fixtures, projected Plane/Axis values remain completely identical to historical Insert target descriptors.

Existing Insert and Revolute equations preserve directed axis, seating normal, and signed-twist X-reference semantics.

## Compatibility APIs

Historical methods remain:

```text
AssemblyConstraintTargetResolver::resolve
AssemblyConstraintTargetResolver::resolve_axis
AssemblyConstraintTargetResolver::resolve_insert

AssemblyHierarchyConstraintTargetResolver::resolve_planar
AssemblyHierarchyConstraintTargetResolver::resolve_axis
AssemblyHierarchyConstraintTargetResolver::resolve_insert
```

They are adapters, not parallel target-geometry authorities.

Local flow:

```text
family parser precheck
  -> resolve_geometric
  -> project_plane / project_axis
  -> legacy descriptor shape
```

The family parser precheck preserves established malformed/unsupported semantic-reference failure behavior.

Hierarchy flow:

```text
exact rooted occurrence context
  -> hierarchy resolve_geometric
  -> project_plane / project_axis
  -> legacy hierarchy descriptor shape
```

Existing Mate, Distance, Angle, Concentric, Insert, and Revolute consumers continue to receive legacy descriptor types, but their geometry now originates at the typed capability-projection boundary.

No equation formula, residual order, finite-difference logic, transform authority, freshness contract, or application rule changes in Block 31.

## Local typed resolution

`AssemblyConstraintTargetResolver::resolve_geometric` currently recognizes only existing feature semantic families.

It reuses:

```text
existing target component/PartDocument ownership checks
existing generated-face parser/resolution
existing single-circle circular-feature validation
existing WorkplaneResolver
existing extrude-direction orientation
existing seating-plane origin/X/normal semantics
```

The returned descriptor is component-local and retains:

```text
transforms_inner_to_outer = [component_transform]
```

The direct component transform is context and is not applied to the local descriptor.

## Hierarchy/root-space typed resolution

`AssemblyHierarchyConstraintTargetResolver::resolve_geometric` reuses:

```text
AssemblyHierarchyTraversal
exact occurrence_path resolution
local child-as-target Project view
AssemblyConstraintTargetResolver::resolve_geometric
AssemblyHierarchyTransformEvaluator
```

Exact chain:

```text
[T_component,
 T_inner_parent,
 ...,
 T_outer_parent]
```

Descriptor transform behavior:

```text
point-like positions
  rotate + translate through every transform

directions / axes / frame vectors
  rotate only through every transform

Circle/Cylinder radius
  unchanged by rigid transforms
```

The output retains exact hierarchy endpoint identity, source kind, source metadata, capabilities, `RootAssembly` coordinate space, and the exact transform chain.

No composed hierarchy transform is persisted or converted back to Euler angles.

## Failure policy

Typed local/hierarchy resolution and projection fail closed on:

- missing component;
- missing referenced PartDocument;
- malformed semantic reference;
- unsupported current semantic source family;
- invalid current generated-face producer;
- invalid current circular-feature producer;
- missing sketch/profile/diameter input;
- unresolved workplane;
- invalid hierarchy path or assembly context;
- invalid source metadata;
- source-kind/descriptor mismatch;
- non-finite geometry;
- non-unit or degenerate Axis/Line direction;
- non-orthogonal Plane axes/normal;
- non-orthogonal or non-right-handed Circle/Frame axes;
- non-positive Circle/Cylinder radius;
- non-canonical capability vector/order;
- non-finite transform context;
- unsupported capability projection.

Plane handedness alone is not a failure because existing workplane semantics intentionally allow an independently oriented face normal.

Projection failure does not mutate model intent and does not fall back to another geometric interpretation.

## Focused coverage

Focused tag:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-geometric-target-taxonomy]"
```

The suite proves:

- all six current generated planar-face tokens classify as `GeneratedPlanarFace`;
- all six expose exactly `[Plane]`;
- typed Plane projections equal complete historical local target geometry, including Bottom;
- current `.axis` classifies as `GeneratedCylindricalFace`;
- current `.axis` exposes `[Axis, Cylinder]`;
- projected Axis equals historical local Axis geometry;
- current hole diameter `20 mm` derives Cylinder radius `10 mm`;
- Plane projection from current `.axis` fails;
- current `.seat` classifies as `CircularFeatureSeat`;
- current `.seat` exposes `[Plane, Axis, Frame]`;
- projected Plane/Axis equal historical XY-seat target geometry;
- Frame origin/Z preserve seating/semantic-axis orientation;
- Circle projection from `.seat` fails;
- synthetic DatumAxis projects Axis + Line;
- synthetic ConstructionLine projects only Line;
- synthetic ConstructionPoint projects Point;
- synthetic GeneratedCircularEdge projects Axis + Point(center) + Circle;
- synthetic GeneratedCylindricalFace projects Axis + Cylinder;
- capability order is deterministic;
- wrong capability order fails;
- source-kind/descriptor mismatch fails;
- non-finite Plane geometry fails;
- degenerate Axis direction fails;
- zero Cylinder radius fails;
- left-handed Frame fails;
- left-handed Circle fails;
- non-unit Plane axes fail;
- hierarchy typed Plane/Axis/Seat projections equal historical root-space geometry for current fixtures;
- exact hierarchy endpoint identity/path is retained;
- exact component-plus-parent transform chain is retained;
- rigid hierarchy transform preserves Cylinder radius;
- local and hierarchy source Projects serialize identically before/after queries.

Existing full Geometry workflows remain required to prove all legacy Mate/Distance/Angle/Concentric/Insert/Revolute and cross-hierarchy consumers remain green through the adapters.

## Persistence and file-format rule

Block 31 adds no JSON field.

Do not serialize:

```text
source_kind
source_metadata
descriptor variant
capability vector
coordinate_space
transform context
projection result
```

`docs/file-format.md` therefore remains unchanged.

Persistent endpoints continue to contain their existing semantic-reference strings.

## Next technical step — Block 32

Implement Block 32 only from `docs/assembly-general-geometric-target-roadmap.md`: assembly-selectable reference geometry Core intent and semantic source identity.

Block 32 must:

- reuse existing persistent DatumPlane, construction-line, and construction-point identities;
- add first-class `DatumAxis` PartDocument intent if still absent;
- freeze the first supported DatumAxis definition family/families, validation, ownership, dependency, invalidation, and removal behavior;
- freeze unambiguous persistent semantic-reference grammar for DatumPlane, DatumAxis, ConstructionLine, and ConstructionPoint;
- prove ids containing `.`, `/`, and `%` cannot make parsing ambiguous;
- preserve existing feature target spellings;
- keep assembly endpoint JSON shape unchanged;
- perform no Geometry target resolution;
- make no JSON serialization change yet.

Planned tags:

```text
[core][datum-axis]
[core][assembly-reference-target-intent]
```

Reference-geometry JSON belongs to Block 33. Geometry resolution into Block-31 capabilities belongs to Block 34.

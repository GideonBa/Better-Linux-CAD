# Assembly Geometric Target Taxonomy and Capability Projection MVP-5

Status: implemented as Block 31 of `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

This document is canonical for the Geometry-layer derived assembly target taxonomy, typed geometric descriptors, source-to-capability projection matrix, local/root-space resolved target contract, and the compatibility adaptation of the current generated face, `.axis`, and `.seat` families.

## Scope

Implemented:

- derived `AssemblyGeometricTargetSourceKind` classification;
- derived `AssemblyGeometricTargetCapability` values;
- exact local and occurrence-qualified endpoint identity retention;
- explicit component-local versus root-assembly coordinate-space identity;
- typed Plane, Axis, Line, Point, Circle, Cylinder, and Frame descriptors;
- one `AssemblyResolvedGeometricTarget` variant/value boundary;
- canonical deterministic capability order;
- explicit fail-closed capability projection helpers;
- current six generated planar-face tokens as `GeneratedPlanarFace -> Plane`;
- current single-circle subtractive-extrude `.axis` producer as `GeneratedCylindricalFace -> Axis + Cylinder`;
- current `.seat` as `CircularFeatureSeat -> Plane + Axis + Frame`;
- local component-space typed resolution;
- exact hierarchy/root-space typed resolution;
- compatibility adapters for existing `resolve`, `resolve_axis`, `resolve_insert` APIs;
- existing equation/residual consumers routed through the projection boundary;
- source Project immutability.

Not implemented:

- new semantic-reference grammar;
- assembly-selectable DatumPlane/DatumAxis/ConstructionLine/ConstructionPoint intent;
- first-class `DatumAxis` Core records;
- JSON/save-format changes;
- generated cylindrical-face/edge/vertex persistent semantic topology identity;
- relationship compatibility matrix;
- Coincident, Parallel, or Perpendicular relationship intent;
- richer joint families.

## Authority boundary

Persistent assembly endpoint authority remains Core model intent.

Local endpoint:

```text
(local ComponentInstanceId,
 semantic_reference)
```

Occurrence-qualified endpoint:

```text
(occurrence_path,
 local ComponentInstanceId,
 semantic_reference)
```

Block 31 does not replace these identities and does not persist a source-kind enum or geometric descriptor.

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

These values are regenerated from the current Project/PartDocument model intent and exact semantic endpoint string.

No Block-31 type is a save-format authority.

## Semantic source kind versus geometric capability

The selected/current semantic model source and the geometry an equation may consume are separate concepts.

Source kind answers:

```text
what model source produced the resolved target?
```

Capability answers:

```text
what typed geometric primitive may a consumer request from that target?
```

The implemented source-kind taxonomy is:

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

The implemented capability taxonomy is:

```text
Plane
Axis
Line
Point
Circle
Cylinder
Frame
```

A source kind is not itself a solver primitive. Equation and compatibility consumers obtain geometry through explicit capability projection.

## Canonical capability order

The global deterministic capability order is:

```text
Plane
Axis
Line
Point
Circle
Cylinder
Frame
```

Every resolved target stores the exact canonical subsequence for its source kind.

Current matrix:

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

The displayed order is capability order, not implementation or enum insertion order chosen by one caller.

A resolved target whose capability vector differs in value or order from this matrix fails validation.

## Endpoint identity variants

`AssemblyGeometricTargetEndpointIdentity` is a derived variant with two exact shapes.

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

The exact persistent semantic-reference string is retained verbatim.

The hierarchy path remains the exact root-to-current ordered `SubassemblyInstanceId` sequence. The empty path remains an explicit root occurrence.

No joined path string, OCCT topology id, XDE label, STEP entity id, or `ComponentTransformAuthority` replaces endpoint identity.

## Coordinate-space contract

One resolved target explicitly records:

```text
ComponentLocal
RootAssembly
```

Local endpoints use:

```text
coordinate_space = ComponentLocal
transforms_inner_to_outer = [component_transform]
```

The typed descriptor itself remains component-local. The direct component transform is retained separately as current placement context.

Occurrence-qualified targets use:

```text
coordinate_space = RootAssembly
transforms_inner_to_outer =
  [T_component,
   T_inner_parent,
   ...,
   T_outer_parent]
```

The typed descriptor has already been evaluated into root-assembly space through the exact stored transform chain.

This separation prevents accidental double application of component placement.

## Source metadata

`AssemblyGeometricTargetSourceMetadata` currently retains:

```text
referenced_part_document
optional source_feature
optional source_profile
optional semantic_face
optional semantic_axis
optional semantic_seating_plane
```

The endpoint semantic-reference string remains exact identity authority. Metadata is derived resolved model context for diagnostics, compatibility adapters, and future presentation.

Current metadata validation:

```text
GeneratedPlanarFace
  requires source_feature + semantic_face

GeneratedCylindricalFace
  requires source_feature + source_profile

GeneratedLinearEdge / GeneratedCircularEdge / GeneratedVertex
  require source_feature

CircularFeatureSeat
  requires source_feature + source_profile
           + semantic_axis + semantic_seating_plane
```

Datum/reference source-specific ids and their unambiguous semantic-reference grammar are intentionally not interpreted by Block 31. That authority boundary belongs to Block 32.

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

It is not a bag of unrelated optional geometric fields.

### Plane

```text
origin
x_axis
y_axis
normal
```

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

Axis and Line are separate descriptor/capability types even though both currently carry one origin plus one direction. Axis represents rotational/cylindrical semantic geometry; Line represents linear geometry.

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

`Frame` is finite, orthonormal, and right-handed. It is not an Euler-angle record.

## Geometric validation

All resolved target values are validated before a capability projection succeeds.

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
  pairwise orthogonal
  cross(x, y) aligned with normal

Axis / Line
  finite origin
  finite unit non-degenerate direction

Point
  finite point

Circle
  finite center
  finite positive radius
  unit orthogonal x/y/normal
  right-handed frame

Cylinder
  finite axis origin
  finite unit non-degenerate axis direction
  finite positive radius

Frame
  finite origin
  unit orthogonal x/y/z
  right-handed frame
```

The current unit/orthogonality/handedness validation tolerance is `1.0e-9`.

Malformed public aggregate values fail closed through `validate_resolved_geometric_target` and through every projection helper.

## Capability projection API

The only supported path from an `AssemblyResolvedGeometricTarget` to typed consumer geometry is:

```text
project_plane
project_axis
project_line
project_point
project_circle
project_cylinder
project_frame
```

Every projector:

```text
validate complete resolved target
  -> require requested capability in canonical source matrix
  -> derive exact descriptor projection
  -> return typed geometry
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

## Current generated planar face migration

The existing six semantic tokens remain unchanged:

```text
feature.<feature-id>.top
feature.<feature-id>.bottom
feature.<feature-id>.right
feature.<feature-id>.left
feature.<feature-id>.front
feature.<feature-id>.back
```

The existing additive-extrude generated-face resolver and `WorkplaneResolver` remain semantic geometry authority.

The typed result is:

```text
source_kind = GeneratedPlanarFace
descriptor  = AssemblyPlanarTargetDescriptor
capabilities = [Plane]
```

Source metadata retains:

```text
PartDocumentId
FeatureId
SemanticFace
```

The generated Plane coordinates remain numerically identical to the historical resolver result.

## Current `.axis` migration

The persistent semantic token remains:

```text
feature.<feature-id>.axis
```

Current support remains deliberately narrow:

```text
source feature is SubtractiveExtrude
source sketch exists
source sketch contains exactly one profile
that profile is exactly one CircleProfile
circle diameter resolves to a Length parameter
```

The current `.axis` producer is therefore a single circular subtractive-extrude cylindrical feature source.

Block 31 classifies that derived model producer as:

```text
GeneratedCylindricalFace
```

and resolves:

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

The exact endpoint string still says `.axis`, and `semantic_axis = Primary` remains in source metadata. No new persistent cylindrical-face token is introduced.

Thus existing Concentric behavior obtains exactly the previous Axis through:

```text
resolve_geometric
  -> project_axis
  -> legacy axis descriptor adapter
```

The new Cylinder capability is additional derived Geometry information only.

## Current `.seat` migration

The persistent token remains:

```text
feature.<feature-id>.seat
```

The same current single-CircleProfile subtractive-extrude preconditions remain.

Typed source:

```text
CircularFeatureSeat
```

Frame construction:

```text
origin = existing seating-plane origin
x_axis = existing seating-plane x_axis
y_axis = existing seating-plane y_axis
z_axis = existing semantic axis direction
```

The existing seating-plane helper already flips Y and normal/axis together for `OppositeSketchNormal`. The resulting Frame remains right-handed.

Capabilities:

```text
Plane
Axis
Frame
```

Projection preserves current behavior:

```text
project_plane -> existing seating plane
project_axis  -> existing semantic axis
project_frame -> complete oriented seat frame
```

Existing Insert and Revolute consumers therefore keep the same directed axis, seating normal, and signed-twist reference orientation.

## Compatibility APIs

The public historical methods remain:

```text
AssemblyConstraintTargetResolver::resolve
AssemblyConstraintTargetResolver::resolve_axis
AssemblyConstraintTargetResolver::resolve_insert

AssemblyHierarchyConstraintTargetResolver::resolve_planar
AssemblyHierarchyConstraintTargetResolver::resolve_axis
AssemblyHierarchyConstraintTargetResolver::resolve_insert
```

They are compatibility adapters, not parallel target-geometry authorities.

Local flow:

```text
family parser precheck
  -> resolve_geometric
  -> project_plane / project_axis
  -> legacy descriptor shape
```

The family parser precheck preserves the established family-specific malformed/unsupported semantic-reference error behavior.

Hierarchy flow:

```text
exact rooted occurrence context
  -> hierarchy resolve_geometric
  -> project_plane / project_axis
  -> legacy hierarchy descriptor shape
```

All existing Mate, Distance, Angle, Concentric, Insert, and Revolute equation/residual consumers continue to receive their old descriptor types, but those descriptors now originate from the typed capability-projection boundary.

No equation formula, residual order, finite-difference logic, transform authority, freshness contract, or application rule changes in Block 31.

## Local typed resolution

`AssemblyConstraintTargetResolver::resolve_geometric` currently recognizes only the existing feature semantic families.

It reuses:

```text
existing target component/PartDocument ownership checks
existing generated-face parser/resolution
existing single-circle circular-feature validation
existing WorkplaneResolver
existing extrude-direction orientation
existing seating-plane orientation
```

It returns component-local descriptor geometry plus:

```text
transforms_inner_to_outer = [component_transform]
```

The component transform is not applied to the local descriptor.

## Hierarchy/root-space typed resolution

`AssemblyHierarchyConstraintTargetResolver::resolve_geometric` reuses:

```text
AssemblyHierarchyTraversal
exact occurrence_path resolution
local child-as-target Project view
AssemblyConstraintTargetResolver::resolve_geometric
AssemblyHierarchyTransformEvaluator
```

The transform chain remains:

```text
[T_component,
 T_inner_parent,
 ...,
 T_outer_parent]
```

Descriptor transform behavior:

```text
point-like positions
  -> rotate + translate through every transform

directions / axes / frame vectors
  -> rotate only through every transform

Circle/Cylinder radius
  -> unchanged by rigid transforms
```

The output retains the exact hierarchy endpoint identity, source kind, source metadata, canonical capabilities, exact transform chain, and `RootAssembly` coordinate space.

No composed hierarchy transform is persisted or converted back to Euler angles.

## Failure policy

Typed local/hierarchy resolution fails closed on:

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
- non-unit or degenerate direction;
- non-orthogonal Plane/Circle/Frame axes;
- non-right-handed Plane/Circle/Frame axes;
- non-positive Circle/Cylinder radius;
- non-canonical capability vector/order;
- non-finite transform context;
- unsupported capability projection.

Projection failures do not mutate model intent and do not fall back to another geometric interpretation.

## Focused coverage

Focused tag:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-geometric-target-taxonomy]"
```

The suite proves:

- all six current generated planar-face tokens classify as `GeneratedPlanarFace`;
- all six expose exactly `[Plane]`;
- typed Plane projections equal historical local target geometry;
- current `.axis` classifies as `GeneratedCylindricalFace`;
- current `.axis` exposes `[Axis, Cylinder]`;
- projected Axis equals historical local Axis geometry;
- current hole diameter `20 mm` derives cylinder radius `10 mm`;
- Plane projection from current `.axis` fails;
- current `.seat` classifies as `CircularFeatureSeat`;
- current `.seat` exposes `[Plane, Axis, Frame]`;
- projected Plane/Axis equal historical Insert target geometry;
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
- hierarchy typed Plane/Axis/Seat projections equal historical root-space geometry;
- exact hierarchy endpoint identity/path is retained;
- exact component-plus-parent transform chain is retained;
- rigid hierarchy transform preserves Cylinder radius;
- local and hierarchy source Projects serialize identically before/after queries.

Existing full Geometry workflows remain required to prove all legacy Mate/Distance/Angle/Concentric/Insert/Revolute and cross-hierarchy consumers remain green through the new adapters.

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

`docs/file-format.md` therefore does not change for Block 31.

Persistent endpoints still contain their existing semantic-reference strings.

## Next technical step: Block 32

Implement Block 32 only from `docs/assembly-general-geometric-target-roadmap.md`: assembly-selectable reference geometry Core intent and semantic source identity.

Block 32 must:

- reuse existing persistent DatumPlane, construction-line, and construction-point identities;
- add first-class `DatumAxis` PartDocument intent if still absent;
- freeze the first supported DatumAxis definition family/families, validation, ownership, dependency, and invalidation semantics;
- freeze an unambiguous persistent semantic-reference grammar for DatumPlane, DatumAxis, ConstructionLine, and ConstructionPoint;
- explicitly prove ids containing `.`, `/`, and `%` cannot make source parsing ambiguous;
- preserve all existing feature target spellings;
- keep assembly endpoint JSON shape unchanged;
- perform no Geometry target resolution;
- make no JSON serialization change yet.

Acceptance tags remain planned as:

```text
[core][datum-axis]
[core][assembly-reference-target-intent]
```

Reference-geometry JSON belongs to Block 33. Geometry resolution of those sources into Block-31 capabilities belongs to Block 34.

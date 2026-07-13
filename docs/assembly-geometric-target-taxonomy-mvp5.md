# Assembly Geometric Target Taxonomy and Capability Projection MVP-5

Status: implemented as Block 31 of `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`. Blocks 32–39 have since extended semantic source identity, generated-topology resolution, explicit target compatibility, generic relationship intent, and shared generic equations without changing persistent endpoint authority.

This document is canonical for the Geometry-layer derived assembly target taxonomy, typed geometric descriptors, source-to-capability projection matrix, local/root-space resolved target contract, target compatibility selection, and compatibility adaptation of the existing generated face, `.axis`, and `.seat` families.

Generated-topology Core identity/recovery is canonical in `docs/assembly-generated-topology-reference-mvp5.md`. Geometry resolution of those Block-35 `topo:` identities through this taxonomy is implemented as Block 36; explicit relationship target compatibility is implemented as Block 37. Block 38 adds persistent generic relationship intent. Block 39 extends capability compatibility and shared equation execution for the documented Point/Line/Axis/Plane generic relationship matrix. Their canonical contracts live in `docs/assembly-general-geometric-target-roadmap.md`, `docs/assembly-generic-relationship-intent-mvp5.md`, and `docs/assembly-generic-relationship-equations-mvp5.md`.

## Scope

Implemented in the target taxonomy:

- derived `AssemblyGeometricTargetSourceKind` classification;
- derived `AssemblyGeometricTargetCapability` values;
- exact local and occurrence-qualified endpoint identity retention;
- explicit component-local versus root-assembly coordinate space;
- typed Plane, Axis, Line, Point, Circle, Cylinder, and Frame descriptors;
- one `AssemblyResolvedGeometricTarget` value/variant boundary;
- canonical deterministic capability ordering;
- explicit fail-closed capability projection helpers;
- all six current generated planar-face tokens as `GeneratedPlanarFace -> Plane`;
- the historical single-circle subtractive `.axis` path as `GeneratedCylindricalFace -> Axis + Cylinder`;
- current `.seat` as `CircularFeatureSeat -> Plane + Axis + Frame`;
- Block-34 DatumPlane/DatumAxis/ConstructionLine/ConstructionPoint resolution;
- Block-36 `topo:` generated cylindrical-face/linear-edge/circular-edge/vertex target resolution;
- local component-space typed resolution;
- exact hierarchy/root-space typed resolution;
- deterministic Block-37 `AssemblyTargetCompatibilityResolver` selection for existing relationship types;
- compatibility adapters for existing local/hierarchy `resolve`, `resolve_axis`, and `resolve_insert` APIs;
- existing equation/residual consumers routed through capability projection;
- source Project immutability.

Not implemented at this Geometry boundary yet:

- Coincident, Parallel, or Perpendicular equations;
- richer joint families.

## Authority boundary

Persistent endpoint authority remains Core model intent.

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

The endpoint semantic-reference string may now be a legacy feature-role spelling, a Block-32 `ref:` spelling, or a Block-35 `topo:` spelling. Block 31 does not replace endpoint identity and does not persist source kind, descriptor, capability vector, coordinate space, or transform context.

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

They are regenerated from current Project/PartDocument model intent and the exact semantic endpoint string.

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

Implemented capabilities in canonical global order:

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
  -> Point(center)
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

Every resolved target stores the exact canonical subsequence for its source kind. A value or order mismatch fails validation.

## Endpoint identity variants

`AssemblyGeometricTargetEndpointIdentity` is a derived variant.

```text
AssemblyLocalGeometricTargetEndpointIdentity
  component_instance
  semantic_reference

AssemblyHierarchyGeometricTargetEndpointIdentity
  occurrence_path
  component_instance
  semantic_reference
```

The persistent semantic-reference string is retained verbatim. The hierarchy path remains the exact ordered root-to-current `SubassemblyInstanceId` sequence; the empty path is the explicit root occurrence.

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

The hierarchy descriptor has already been evaluated through that exact chain into root-assembly space. Retaining coordinate-space identity and the exact chain prevents accidental double application.

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

Block 35 now freezes richer producer/profile/role identity for generated topology at the Core endpoint boundary. Block 36 may extend derived source metadata as needed for diagnostics, but semantic endpoint identity remains authoritative and no raw OCCT id may be persisted.

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

Plane preserves the BLCAD workplane contract: X, Y, and independently oriented face normal are finite unit and pairwise orthogonal. Plane does not require `cross(x_axis, y_axis) == normal` because historical Bottom semantics use `+X`, `+Y`, `-Z normal`.

### Axis and Line

```text
origin
direction
```

Axis and Line are separate semantic geometric types even though both carry one origin and one direction. Directions are finite unit non-degenerate vectors.

### Point

```text
point
```

Point coordinates are finite.

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

Cylinder carries a finite positive radius and finite unit non-degenerate axis direction.

### Frame

```text
origin
x_axis
y_axis
z_axis
```

Frame is finite, orthonormal, and right-handed. It is not an Euler-angle record.

## Complete resolved-target validation

Every projector first validates the complete target:

```text
endpoint ids/references non-empty
hierarchy path ids non-empty
coordinate space matches endpoint identity kind
transform context finite
source metadata satisfies source kind
source kind matches descriptor variant
capability vector exactly matches canonical matrix/order
descriptor geometry satisfies type-specific invariants
```

The current unit/orthogonality/handedness tolerance is `1.0e-9`.

Malformed public aggregate values fail closed through `validate_resolved_geometric_target` and every projector.

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

Equation builders do not infer geometry from source-kind enums. Unsupported projections fail closed and do not fall back to another geometric interpretation.

## Current legacy generated source migration

Existing generated planar-face tokens remain unchanged:

```text
<feature-id>.top
<feature-id>.bottom
<feature-id>.right
<feature-id>.left
<feature-id>.front
<feature-id>.back
```

The additive-extrude generated-face resolver and `WorkplaneResolver` remain geometry authority. Typed result is `GeneratedPlanarFace`, planar descriptor, capabilities `[Plane]`.

Current `.axis` remains the historical narrow single-CircleProfile subtractive path. It classifies as `GeneratedCylindricalFace` with `AssemblyCylindricalSurfaceTargetDescriptor`; radius derives from circle diameter and Axis projection preserves historical geometry. The endpoint still says `.axis`.

Current `.seat` remains `CircularFeatureSeat -> Plane + Axis + Frame`. Frame construction is:

```text
origin = existing seating-plane origin
x_axis = existing seating-plane x_axis
z_axis = existing semantic axis direction
y_axis = z_axis × x_axis
```

This canonicalizes a right-handed Frame while preserving Insert and Revolute residual-relevant axis, seating-normal, and signed-twist X-reference semantics.

## Reference geometry source resolution

Blocks 32–33 implemented first-class DatumAxis intent, the canonical `ref:<family>:<encoded-id>` grammar, and additive `datum_axes` JSON.

Block 34 resolves:

```text
DatumPlane        -> Plane
DatumAxis         -> Axis + Line
ConstructionLine  -> Line
ConstructionPoint -> Point
```

Resolution reuses existing workplane/construction execution. Local targets stay component-local; hierarchy targets evaluate through the exact rooted transform chain. Canonical PartDocument snapshots remain freshness authority.

Focused Block-34 tag:

```text
[geometry][assembly-reference-target-resolution]
```

## Block 35 generated topology identity handoff

Canonical document: `docs/assembly-generated-topology-reference-mvp5.md`.

Block 35 implements stable Core producer-driven semantic identities for generated cylindrical faces, linear edges, circular edges, and vertices. Canonical `topo:` spellings retain exact feature/profile/role identity and parse fail closed.

Supported first producer matrices are:

```text
RectangularAdditiveExtrude
  12 named linear edges, expected cardinality 1 each
  8 named vertices, expected cardinality 1 each

SingleCircleSubtractiveExtrude
  wall, expected cardinality 1
  source_rim, expected cardinality 1
  opposite_rim, expected cardinality 1
```

Pattern-generated subelements remain unavailable until stable per-instance semantic identity exists. Recovery is read-only and never writes raw OCCT identity.

Block 35 intentionally adds no `resolve_geometric` branch for `topo:` sources. Therefore Block-31 synthetic capability tests continue to prove descriptor/projection invariants, while real Block-35 generated topology sources are not yet Geometry-resolvable.

## Local and hierarchy typed resolution

`AssemblyConstraintTargetResolver::resolve_geometric` returns component-local descriptors and retains:

```text
transforms_inner_to_outer = [component_transform]
```

`AssemblyHierarchyConstraintTargetResolver::resolve_geometric` reuses hierarchy traversal, exact occurrence resolution, the local resolver, and `AssemblyHierarchyTransformEvaluator`.

Exact chain:

```text
[T_component,
 T_inner_parent,
 ...,
 T_outer_parent]
```

Point-like positions rotate and translate through each transform. Directions, normals, axes, and frame vectors rotate only. Circle/Cylinder radius is unchanged by rigid transforms.

The hierarchy result retains exact endpoint identity, source kind, source metadata, capabilities, `RootAssembly` coordinate space, and exact transform chain. No composed transform is persisted or converted back to Euler angles.

## Failure policy

Typed local/hierarchy resolution and projection fail closed on missing model context, malformed or unsupported semantic source, invalid producer inputs, unresolved workplanes, invalid hierarchy path, invalid source metadata, source-kind/descriptor mismatch, invalid descriptor geometry, non-canonical capability order, invalid transform context, and unsupported projection.

Block 36 additionally fails closed on malformed `topo:` sources, unsupported Block-35 producer roles, wrong source profile identity, and family/role mismatches, via `validate_generated_topology_reference`.

## Focused coverage

Block-31 taxonomy/projection tests:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-geometric-target-taxonomy]"
```

Block-34 reference target tests:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-reference-target-resolution]"
```

Block-35 Core identity/recovery tests:

```bash
./build/dev/blcad_core_tests "[core][semantic-generated-topology-reference]"
./build/dev/blcad_core_tests "[core][semantic-generated-topology-recovery]"
```

Existing full Geometry workflows remain required to prove legacy Mate/Distance/Angle/Concentric/Insert/Revolute and cross-hierarchy consumers remain green through compatibility adapters.

## Persistence and file-format rule

Do not serialize Geometry target query products:

```text
source_kind
source_metadata
descriptor variant
capability vector
coordinate_space
transform context
projection result
```

Persistent endpoints retain their exact semantic-reference strings. Block 35 adds canonical `topo:` identity strings but no new endpoint JSON field or Geometry cache field.

`docs/file-format.md` remains save-format authority.

## Block 36 — generated-topology resolution — Implemented

Block 36 parses and resolves Block-35 `topo:` generated-topology identities through this Block-31 taxonomy.

Implemented mapping:

```text
GeneratedCylindricalFace -> Cylinder + Axis
GeneratedLinearEdge      -> Line
GeneratedCircularEdge    -> Circle + Axis + Point(center)
GeneratedVertex          -> Point
```

Resolution starts from validated producer semantic identity, computes descriptors analytically from current feature/sketch/profile model intent, preserves component-local versus exact rooted transform semantics, and mutates no source model. The canonical Block-36 contract lives in `docs/assembly-general-geometric-target-roadmap.md`.

## Block 37 — target compatibility selection — Implemented

Block 37 adds `AssemblyTargetCompatibilityResolver`, which consumes already-resolved target capability vectors and relationship type, then returns one exact ordered capability pair or an explicit incompatibility.

Implemented matrix:

```text
Mate         Plane <-> Plane
Distance     Plane <-> Plane | Point <-> Point | Point <-> Plane | Plane <-> Point
Angle        Plane <-> Plane | Line <-> Line | Axis <-> Axis | Line <-> Axis | Axis <-> Line
Concentric   Axis <-> Axis
Insert       Frame <-> Frame
```

Local and cross-hierarchy equation builders now resolve typed targets, select compatibility, and then project only the selected existing equation shape. No new relationship enum, residual equation, JSON field, or persisted Geometry query product is introduced by Block 37.

Focused tags:

```text
[geometry][assembly-target-compatibility]
[geometry][assembly-cross-hierarchy-target-compatibility]
```

## Blocks 38–39 — generic relationship intent and equations — Implemented

Block 38 adds persistent `Coincident`, `Parallel`, and `Perpendicular` relationship types and JSON spellings at Core level.

Block 39 extends `AssemblyTargetCompatibilityResolver` for the documented Coincident Point/Line/Plane pairs and Parallel/Perpendicular Line/Axis/Plane direction pairs. The selected capability pair is projected through this taxonomy and consumed by `AssemblyGenericRelationshipEquationBuilder`.

The same builder closes non-planar Line/Axis Angle execution already selected by Block 37. No source-kind-specific equation table is introduced: source kinds expose capabilities, compatibility selects a pair, and equation semantics consume the selected projection.

Canonical contracts:

- `docs/assembly-generic-relationship-intent-mvp5.md`
- `docs/assembly-generic-relationship-equations-mvp5.md`

## Block 40 — Joint target compatibility and oriented Frame — Implemented

`AssemblyJointTargetCompatibilityResolver` selects the typed `Frame / Frame` contract before local or cross-hierarchy Revolute projection. `.seat` remains the current deterministic Frame source. Axis-only Revolute fails closed because Axis supplies no reference X direction, and Geometry never synthesizes one from an arbitrary world axis. Canonical contract: `docs/assembly-joint-target-compatibility-mvp5.md`.

## Next technical step — Block 41

The next technical step is Block 41: general joint coordinate/limit Core model.

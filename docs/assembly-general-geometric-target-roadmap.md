# General Assembly Geometric Target and Relationship Roadmap

Status: Block 31 is implemented. Block 32 is the current next technical step. Blocks 32–47 remain planned headless architecture.

This document is canonical for the sequenced expansion from the current generated planar-face / circular-feature axis / seat families to an Inventor-/SolidWorks-like assembly target model that can deliberately select semantic faces, cylindrical surfaces, edges, vertices, datum planes, datum axes, construction lines, and construction points without persisting raw OCCT topology identity.

Implemented Block-31 detail is canonical in `docs/assembly-geometric-target-taxonomy-mvp5.md`.

## Non-negotiable architecture rules

1. Persistent assembly endpoints remain semantic references owned by Core model intent.
2. Raw `TopoDS_Face`, `TopoDS_Edge`, `TopoDS_Vertex`, OCCT indices/hashes/map positions, XDE labels, STEP entity ids, and memory addresses are never persistent target identity.
3. Local and occurrence-qualified cross-hierarchy endpoints share one semantic-reference contract.
4. Root-space evaluation composes the existing exact component/parent transform chain.
5. `ComponentTransformAuthority` remains `(AssemblyDocumentId, local ComponentInstanceId)`.
6. Existing residual/Jacobian, finite-difference, numeric solve, diagnostics, freshness, and atomic-application boundaries remain authoritative.
7. Existing `.top/.bottom/.right/.left/.front/.back`, `.axis`, and `.seat` strings remain backward compatible.
8. Typed target taxonomy exists before reference/generated topology expansion.
9. Target compatibility exists before new relationship equations.
10. Generic relationship persistent intent/JSON exists before Geometry execution.
11. Joint target compatibility is generalized before multi-coordinate joint state.
12. Multi-coordinate Core state is serialized before vector-drive motion execution.
13. Each richer joint family is one block after the vector-drive boundary.
14. GUI picking is a later presentation consumer.

## Implemented Block 31 — Source taxonomy and geometric capability projection

Canonical document: `docs/assembly-geometric-target-taxonomy-mvp5.md`.

Source kinds:

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

Capabilities in canonical order:

```text
Plane
Axis
Line
Point
Circle
Cylinder
Frame
```

Implemented source matrix:

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

`AssemblyResolvedGeometricTarget` retains exact endpoint identity, source kind, derived source model metadata, typed descriptor variant, canonical capability vector, coordinate space, and exact transform context.

Implemented projections:

```text
project_plane
project_axis
project_line
project_point
project_circle
project_cylinder
project_frame
```

Every projection validates the complete resolved target and fails closed when the requested capability is absent.

Current source migration:

```text
existing generated planar faces
  -> GeneratedPlanarFace -> Plane

existing .axis single-circle subtractive-extrude producer
  -> GeneratedCylindricalFace -> Axis + Cylinder

existing .seat
  -> CircularFeatureSeat -> Plane + Axis + Frame
```

Existing local and hierarchy compatibility resolver APIs adapt from the projection boundary. Existing relationship/joint equations therefore retain their descriptor shapes and numeric semantics.

Block 31 adds no persistence or JSON.

## Mandatory continuation order

```text
32 assembly-selectable reference geometry Core intent
  -> 33 reference geometry serialization and structure validation
  -> 34 datum/axis/line/point target resolution
  -> 35 stable semantic generated topology identity/recovery
  -> 36 generated face/edge/vertex target resolution
  -> 37 explicit target compatibility matrix
  -> 38 generic geometric relationship Core intent + JSON
  -> 39 generic relationship equations + shared solve integration
  -> 40 joint target compatibility + oriented Frame contract
  -> 41 general joint coordinate/limit Core model
  -> 42 general joint coordinate JSON/backward compatibility
  -> 43 vector joint drives + holding/freshness/atomic application
  -> 44 Prismatic joint
  -> 45 Cylindrical joint
  -> 46 Planar joint
  -> 47 Ball/Spherical joint
```

Do not merge these blocks.

## Block 32 — Assembly-selectable reference geometry Core intent — Next

Primary boundary: Core persistent model intent and semantic source identity.

### Existing Core sources to reuse

BLCAD already owns persistent identities for:

```text
DatumPlane
ConstructionLine
ConstructionPoint
```

Block 32 must make these valid assembly-selectable semantic source identities without resolving Geometry.

### DatumAxis

If no first-class `DatumAxis` exists, add one as PartDocument model intent.

The first supported DatumAxis definition family/families must be deliberately narrow and fully specified. Block 32 must freeze:

```text
identity
name
definition inputs
validation
PartDocument ownership
dependency graph nodes/edges
invalidation propagation
removal/dependent behavior
```

Do not invent a broad axis-feature system in one block.

### Semantic-reference grammar

Freeze exact persistent semantic-reference spellings for:

```text
DatumPlane
DatumAxis
ConstructionLine
ConstructionPoint
```

The grammar must be unambiguous for arbitrary non-empty typed ids.

Tests must include ids containing:

```text
.
/
%
```

Parser design may not assume these bytes are unavailable inside ids.

Existing feature target spellings remain unchanged:

```text
feature.<feature-id>.top
feature.<feature-id>.bottom
feature.<feature-id>.right
feature.<feature-id>.left
feature.<feature-id>.front
feature.<feature-id>.back
feature.<feature-id>.axis
feature.<feature-id>.seat
```

Assembly endpoint persistence remains:

```text
component/occurrence identity
+
semantic_reference string
```

Do not add a second endpoint variant to JSON merely because a source is reference geometry.

### Block-32 boundary

Block 32 does not:

- serialize new DatumAxis/reference target intent;
- resolve Plane/Axis/Line/Point geometry;
- modify the Block-31 capability matrix;
- add relationship compatibility;
- add relationship types;
- add joint types.

Planned focused tags:

```text
[core][datum-axis]
[core][assembly-reference-target-intent]
```

## Block 33 — Reference geometry serialization and structure validation

Primary boundary: Core serialization/compatibility.

Serialize Block-32 PartDocument reference-geometry intent additively. Historical files remain loadable.

Requirements:

```text
DatumAxis roundtrip if introduced
exact semantic source reference strings roundtrip byte-for-byte
local relationship endpoints preserved
local joint endpoints preserved
cross-hierarchy relationship endpoints preserved
cross-hierarchy joint endpoints preserved
```

Unknown/malformed new intent fails closed.

No Plane/Axis/Line/Point geometry is resolved during load or structure validation.

## Block 34 — Datum, axis, line, and point target resolution

Primary boundary: Geometry semantic target resolution.

Resolve Block-32 semantic sources into Block-31 typed targets:

```text
DatumPlane
  -> Plane

DatumAxis
  -> Axis + Line

ConstructionLine
  -> Line

ConstructionPoint
  -> Point
```

Reuse existing workplane/construction geometry resolvers rather than duplicating coordinate semantics.

Local target geometry remains component-local with direct component transform context.

Occurrence-qualified resolution reuses exact existing component-plus-parent transform chains and outputs root-space descriptors.

Existing canonical PartDocument snapshots remain semantic target freshness authority.

No generated topology identity work belongs in Block 34.

## Block 35 — Stable semantic generated topology identity and recovery

Primary boundary: Core semantic source/recovery.

Define producer-driven semantic identities for:

```text
GeneratedCylindricalFace
GeneratedLinearEdge
GeneratedCircularEdge
GeneratedVertex
```

Each supported feature producer publishes a finite semantic-role matrix with expected cardinality and ambiguity behavior.

Persistent identity may derive from BLCAD model intent such as:

```text
FeatureId
ProfileId
SketchEntityId
producer-defined semantic role
stable patterned instance identity only when explicitly modeled
```

Forbidden persistent identity sources:

```text
OCCT traversal index
TopoDS hash as model identity
BRep map position
XDE label tag
STEP entity number
memory address
unordered OCCT result order
```

Patterned subelements remain unavailable until stable per-instance semantic identity exists.

Reference recovery/invalidation behavior must be explicit.

## Block 36 — Generated face, edge, and vertex target resolution

Primary boundary: Geometry semantic target resolution.

Resolve Block-35 semantic sources into Block-31 capabilities:

```text
GeneratedPlanarFace
  -> Plane

GeneratedCylindricalFace
  -> Cylinder + Axis

GeneratedLinearEdge
  -> Line

GeneratedCircularEdge
  -> Circle + Axis + Point(center)

GeneratedVertex
  -> Point
```

The semantic producer identity remains authoritative. OCCT topology is execution data only after the intended semantic subelement is known.

Local and hierarchy coordinate-space contracts remain exactly Block 31.

## Block 37 — Explicit target compatibility matrix

Primary boundary: derived relationship compatibility semantics.

Introduce one deterministic resolver:

```text
relationship type
+ target A capabilities
+ target B capabilities
-> one exact ordered capability pair/bundle
OR explicit incompatibility
```

Initial compatibility:

```text
Mate
  Plane <-> Plane

Distance
  Plane <-> Plane
  Point <-> Point
  Point <-> Plane
  Plane <-> Point

Angle
  Plane <-> Plane
  Line/Axis <-> Line/Axis

Concentric
  Axis <-> Axis

Insert
  Frame <-> Frame
```

Capability choice must be deterministic when a source exposes multiple compatible capabilities.

No new relationship enum or equation in Block 37.

## Block 38 — Generic geometric relationship Core intent and JSON

Primary boundary: persistent Core relationship model and serialization.

Add local and Project-level relationship types:

```text
Coincident
Parallel
Perpendicular
```

Preserve:

```text
existing endpoint JSON shapes
exact target A/B order
active/inactive state semantics
local AssemblyDocument relationship id scope
Project-level cross relationship id scope
historical five-family compatibility
```

No residual/equation integration in Block 38.

## Block 39 — Generic relationship equations and shared solve integration

Primary boundary: Geometry relationship semantics and existing numeric/diagnostic execution.

Initial Coincident capability pairs:

```text
Point <-> Point
Point <-> Line
Line <-> Point
Point <-> Plane
Plane <-> Point
```

Parallel and Perpendicular initially support:

```text
Line/Axis direction pairs
Plane normal pairs
```

All new families reuse:

```text
existing local/cross graphs
ComponentTransformAuthority
Block-31 target/capability projection
Block-37 compatibility selection
shared residual scaling
shared central finite differences
shared Gauss-Newton solving
existing PartDocument freshness
atomic application
Jacobian-rank diagnostics
```

No generic-target-specific solver is allowed.

## Block 40 — Joint target compatibility and oriented Frame contract

Primary boundary: derived joint compatibility semantics.

Current Revolute compatibility remains:

```text
Frame <-> Frame
```

Current `.seat` sources project `Frame`.

Axis alone is insufficient for signed twist because it provides no deterministic reference X direction. Axis-only Revolute remains incompatible until persistent semantic model intent supplies a deterministic oriented Frame.

Geometry may not choose an arbitrary world axis.

No joint persistence change or new joint family in Block 40.

## Block 41 — General joint coordinate/limit Core model

Primary boundary: persistent Core joint state.

Generalize scalar Revolute coordinate/limit state into typed family-defined coordinate slots with stable roles and explicit angular/linear kinds.

Representative roles:

```text
Revolute
  rotation

Prismatic
  translation

Cylindrical
  translation
  rotation

Planar
  translation_u
  translation_v
  rotation_normal
```

Current Revolute APIs remain supported through an explicit compatibility/adaptation boundary.

No JSON or vector-drive execution in Block 41.

## Block 42 — General joint coordinate JSON and backward compatibility

Primary boundary: Core serialization.

Serialize coordinate slots with:

```text
stable role spelling
quantity kind/unit
deterministic family-defined order
value
optional limits
```

Historical scalar Revolute files remain loadable.

Duplicate, missing, or unknown coordinate roles fail closed.

No motion execution changes in Block 42.

## Block 43 — Vector joint drives, holding semantics, freshness, atomic application

Primary boundary: Geometry motion execution/result/application.

Generalize selected-joint scalar drives into family-defined coordinate drive vectors.

Non-selected active joints hold all authored coordinate slots. Undriven roles of the selected joint initially hold authored values.

Motion snapshots protect:

```text
coordinate-slot definitions
roles/order
quantity kinds
values
limits
selected drive values
```

Atomic application writes transform authority proposals plus only selected joint coordinate roles explicitly driven by the result.

Authority variables remain six direct component-transform variables per unique free authority. Joint coordinates remain drive parameters, not transform-authority variables.

No new joint family in Block 43.

## Block 44 — Prismatic joint

One new family in one block.

Preferred first target compatibility:

```text
Frame <-> Frame
```

Coordinate:

```text
translation : LengthMm
```

Freeze persistent type/JSON, translation limits, residual order, holding-drive semantics, local/cross motion integration, freshness, and atomic application.

## Block 45 — Cylindrical joint

One new two-coordinate family.

Coordinates:

```text
translation : LengthMm
rotation    : AngleDeg
```

Prove both drives coexist through the shared vector-drive/numeric path without duplicate transform variables.

## Block 46 — Planar joint

One new three-coordinate family.

Required capability:

```text
Frame <-> Frame
```

Coordinates:

```text
translation_u   : LengthMm
translation_v   : LengthMm
rotation_normal : AngleDeg
```

The Frame defines U/V and oriented normal.

## Block 47 — Ball/Spherical joint

Required target compatibility:

```text
Point <-> Point
```

Preferred first seed is passive center coincidence with three free rotational DOF.

A driven spherical orientation is not represented by arbitrary Euler angles without a separately frozen singularity/order contract. If the first seed remains passive-only, selecting it as a driven joint fails explicitly.

## GUI selection boundary

Blocks 31–47 remain headless semantic model/query/equation/motion work.

A future picker consumes:

```text
persistent semantic source candidate
  -> resolved source kind
  -> available capabilities
  -> selected relationship/joint type
  -> compatibility resolver
```

The UI may highlight OCCT faces/edges/vertices and reference geometry, but committing selection persists BLCAD semantic source identity. Raw `TopoDS_Shape` or selection index never becomes the endpoint.

## Persistence rule

Persist semantic model intent.

Regenerate source classification, typed descriptors, capabilities, projections, transformed target geometry, compatibility selection, connectivity, authority mapping, residuals/Jacobians/results, freshness snapshots, proposals, diagnostics, exchange products, posed geometry, contact records, and sweep analyses.

## Next technical step

Implement Block 32 only: assembly-selectable reference geometry Core intent and unambiguous semantic-reference identity.

Do not implement Block-33 JSON or Block-34 Geometry resolution in Block 32.

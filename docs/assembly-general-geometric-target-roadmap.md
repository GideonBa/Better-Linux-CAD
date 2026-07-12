# General Assembly Geometric Target and Relationship Roadmap

Status: Blocks 31–33 are implemented. Block 34 is the current next technical step. Blocks 34-47 remain planned headless architecture.

This document is the active status and sequencing authority for the expansion from the current assembly target layer to semantic reference geometry, stable generated topology targets, generic geometric relationships, and richer motion-joint families.

Implemented Block-31 through Block-33 semantics are canonical in:

- `docs/assembly-geometric-target-taxonomy-mvp5.md`
- `docs/assembly-reference-geometry-intent-mvp5.md`

The complete pre-implementation planning detail for Blocks 32-47 is preserved byte-for-byte in:

- `docs/assembly-general-geometric-target-roadmap-planning-baseline.md`

For Blocks 32-47, that companion document's per-block required work, failure policy, acceptance tags, proof matrices, compatibility tables, residual seeds, and explicit deferrals are incorporated by reference here. Its old top-level implementation-status statements and its Block-31 planning section are historical planning text, not current status authority.

## Why this roadmap exists

The current assembly solver architecture is already general in transform authority, residual/Jacobian execution, cross-hierarchy solving, freshness, and atomic application. The remaining route to a mature CAD assembly selector is primarily semantic target expressiveness and compatibility.

Current persistent feature target strings remain:

```text
feature.<feature-id>.top|bottom|right|left|front|back
feature.<feature-id>.axis
feature.<feature-id>.seat
```

Block 31 now resolves those existing families through a typed Geometry-layer source/capability boundary. Future blocks deliberately add reference-geometry identity, stable generated topology identity, compatibility, relationship intent/equations, and richer joints in separate authority steps.

## Non-negotiable architecture rules

1. Persistent assembly endpoints remain Core-owned semantic references.
2. Raw `TopoDS_Face`, `TopoDS_Edge`, `TopoDS_Vertex`, OCCT traversal/map identities, XDE labels, STEP entity ids, and memory addresses are never persistent target identity.
3. Local and occurrence-qualified endpoints continue to share one semantic-reference contract.
4. Root-space hierarchy evaluation continues to compose the exact existing component/parent transform chain.
5. `ComponentTransformAuthority` remains `(AssemblyDocumentId, local ComponentInstanceId)`.
6. Existing residual/Jacobian, central finite-difference, numeric solve, rank diagnostics, freshness, and atomic-application boundaries remain authoritative.
7. Existing `.top/.bottom/.right/.left/.front/.back`, `.axis`, and `.seat` strings remain backward compatible.
8. Typed target taxonomy precedes new source expansion.
9. Target compatibility precedes new relationship equations.
10. Generic relationship persistent intent/JSON precedes Geometry execution.
11. Joint target compatibility precedes multi-coordinate joint state.
12. Multi-coordinate Core state is serialized before vector-drive motion execution.
13. Each richer joint family is implemented in its own block after the shared vector-drive boundary.
14. GUI picking is a later presentation consumer.

## Block 31 — Typed geometric target taxonomy and capability projection — Implemented

Primary boundary: Geometry-layer derived query semantics.

Block 31 introduces source kinds:

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

and geometric capabilities in canonical order:

```text
Plane
Axis
Line
Point
Circle
Cylinder
Frame
```

One `AssemblyResolvedGeometricTarget` retains:

```text
exact local or occurrence-qualified endpoint identity
source kind
derived source model metadata
typed descriptor variant
canonical capability vector
ComponentLocal or RootAssembly coordinate space
exact transforms_inner_to_outer context
```

Typed descriptor alternatives are:

```text
AssemblyPlanarTargetDescriptor
AssemblyAxisTargetDescriptor
AssemblyLineTargetDescriptor
AssemblyPointTargetDescriptor
AssemblyCircularEdgeTargetDescriptor
AssemblyCylindricalSurfaceTargetDescriptor
AssemblyFrameTargetDescriptor
```

Capability projection is explicit:

```text
project_plane
project_axis
project_line
project_point
project_circle
project_cylinder
project_frame
```

Every projector validates the complete resolved target and fails closed when the source does not expose the requested capability.

The implemented source matrix is:

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

Current source migration is:

```text
.top/.bottom/.right/.left/.front/.back
  -> GeneratedPlanarFace
  -> Plane

.axis current single-CircleProfile subtractive-extrude producer
  -> GeneratedCylindricalFace
  -> Axis + Cylinder

.seat
  -> CircularFeatureSeat
  -> Plane + Axis + Frame
```

The persistent target strings remain unchanged.

Current `.axis` resolution derives Cylinder radius from the existing diameter parameter while preserving the historical Axis geometry. Current `.seat` resolves an oriented Frame from the existing seating X/Y axes and semantic axis Z, preserving Insert/Revolute orientation and signed-twist behavior.

Local and hierarchy legacy `resolve`, `resolve_axis`, and `resolve_insert` APIs remain compatibility adapters. Their geometry now originates from the typed projection boundary, while existing equation/residual descriptor shapes and numeric semantics remain unchanged.

Focused acceptance tag:

```text
[geometry][assembly-geometric-target-taxonomy]
```

Block 31 adds no new persistent source kind, descriptor, capability vector, source grammar, relationship family, joint family, or JSON field.

## Mandatory continuation order

Blocks 32 and 33 are implemented; the remaining order is unchanged.

```text
32 assembly-selectable reference geometry Core intent (implemented)
  -> 33 reference geometry serialization and structure validation (implemented)
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

Do not merge these blocks into one feature block.

## Block 32 — Assembly-selectable reference geometry Core intent — Implemented

Canonical document: `docs/assembly-reference-geometry-intent-mvp5.md`.

Implemented first-class `DatumAxis` PartDocument intent with the two frozen definition families `Explicit` (finite origin, unit direction, optional unique parameter dependencies) and `FromConstructionLine` (owned `ConstructionLineId` identity only). Ownership, id uniqueness, dependency edges, and invalidation propagation follow existing construction-geometry semantics.

Implemented the frozen reference semantic-source grammar:

```text
ref:datum_plane:<encoded-id>
ref:datum_axis:<encoded-id>
ref:construction_line:<encoded-id>
ref:construction_point:<encoded-id>
```

Every id byte outside `[A-Za-z0-9_-]` is escaped as uppercase `%HH`, so valid reference spellings contain no `.` while every feature spelling contains one: the two grammars accept provably disjoint string sets and arbitrary ids containing `.`, `/`, and `%` remain unambiguous. Parsing fails closed and each identity has exactly one canonical spelling.

Assembly endpoints persist only component/occurrence identity plus the semantic-reference string. No resolved Plane/Axis/Line/Point geometry is copied into constraints or joints.

Focused acceptance tags:

```text
[core][datum-axis]
[core][assembly-reference-target-intent]
```

Block 32 added no JSON/serialization changes and performs no Geometry target resolution.

## Block 33 — Reference geometry serialization and structure validation — Implemented

Canonical document: `docs/assembly-reference-geometry-intent-mvp5.md`. Save-format authority: `docs/file-format.md`.

Implemented the additive optional `datum_axes` part-document array for both DatumAxis families with historical-file compatibility and load-time ownership/family validation through `PartDocument::add_datum_axis`. Existing local and cross-hierarchy endpoint JSON shapes are unchanged and `ref:` semantic-reference strings roundtrip byte-for-byte.

No Plane/Axis/Line/Point geometry is resolved during JSON load or structure validation.

Focused acceptance tags:

```text
[core][datum-axis-json]
[core][assembly-reference-target-json]
```

## Block 34 — Datum, axis, line, and point target resolution

Primary boundary: Geometry semantic target resolution.

Resolve Block-32 semantic sources into Block-31 capabilities:

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

Reuse the existing `WorkplaneResolver`, construction-line resolver, and construction-point resolver. Local targets remain component-local; hierarchy targets reuse the exact existing component-plus-parent transform chain. Existing canonical PartDocument snapshots remain freshness authority.

## Block 35 — Stable semantic generated topology identity and recovery

Primary boundary: Core semantic reference model and recovery rules.

Define producer-driven semantic identities for:

```text
GeneratedCylindricalFace
GeneratedLinearEdge
GeneratedCircularEdge
GeneratedVertex
```

Every supported feature producer must publish a finite semantic-role matrix, expected cardinality, and ambiguity/failure behavior.

Forbidden persistent identity sources remain:

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

## Block 36 — Generated face, edge, and vertex target resolution

Primary boundary: Geometry semantic target resolution.

Resolve Block-35 semantic sources into Block-31 descriptors/capabilities:

```text
GeneratedPlanarFace      -> Plane
GeneratedCylindricalFace -> Cylinder + Axis
GeneratedLinearEdge      -> Line
GeneratedCircularEdge    -> Circle + Axis + Point(center)
GeneratedVertex          -> Point
```

Semantic producer identity remains authoritative. OCCT topology is execution data only after semantic resolution identifies the intended generated subelement.

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

Initial matrix:

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
  Line <-> Line
  Axis <-> Axis
  Line <-> Axis
  Axis <-> Line

Concentric
  Axis <-> Axis

Insert
  Frame <-> Frame
```

Capability choice is deterministic for multi-capability targets. Equation builders consume the compatibility result, not source-kind enums.

No new relationship enum or equation is added in Block 37.

## Block 38 — Generic geometric relationship Core intent and JSON

Primary boundary: persistent Core relationship model and serialization.

Add local and Project-level:

```text
Coincident
Parallel
Perpendicular
```

Preserve endpoint shapes, target A/B order, active/inactive state semantics, local versus Project-level id scopes, and historical five-family compatibility.

No equation or graph participation changes belong in Block 38.

## Block 39 — Generic relationship equations and shared solve integration

Primary boundary: Geometry equation semantics plus existing numeric/diagnostic execution.

Initial Coincident pairs:

```text
Point <-> Point
Point <-> Line
Line <-> Point
Point <-> Plane
Plane <-> Point
```

Initial residual seeds:

```text
Point-Point
  pB - pA

Point-Line
  cross(point - line.origin, line.direction)

Point-Plane
  dot(point - plane.origin, plane.normal)
```

Parallel and Perpendicular initially support Line/Axis direction pairs and Plane normal pairs.

All new families reuse existing local/cross graphs, `ComponentTransformAuthority`, Block-31 projection, Block-37 compatibility selection, shared residual scaling, central finite differences, Gauss-Newton solving, PartDocument freshness, atomic application, and Jacobian-rank diagnostics.

No generic-target-specific solver is allowed.

## Block 40 — Joint target compatibility and oriented Frame contract

Primary boundary: derived joint compatibility semantics.

Current Revolute remains:

```text
Frame <-> Frame
```

Current `.seat` sources project Frame.

Axis alone is insufficient for signed twist because it has no deterministic reference X direction. Geometry may not synthesize a Frame from Axis using an arbitrary world axis.

No joint persistence change or new joint family belongs in Block 40.

## Blocks 41-43 — Multi-coordinate joint foundation

Block 41 generalizes the persistent local/Project-level joint coordinate/limit model into family-defined typed coordinate slots with explicit angular/linear kinds.

Block 42 serializes that coordinate-slot model additively and preserves historical scalar Revolute compatibility.

Block 43 generalizes selected-joint scalar drives to deterministic coordinate drive vectors with holding semantics, complete coordinate-slot freshness, and atomic application.

Authority variables remain six direct component-transform variables per unique free authority. Joint coordinates remain drive parameters.

## Blocks 44-47 — Richer joint families

One family per block:

```text
44 Prismatic
  preferred first compatibility: Frame <-> Frame
  translation : LengthMm

45 Cylindrical
  translation : LengthMm
  rotation    : AngleDeg

46 Planar
  Frame <-> Frame
  translation_u   : LengthMm
  translation_v   : LengthMm
  rotation_normal : AngleDeg

47 Ball/Spherical
  Point <-> Point
  preferred first seed: passive center coincidence
```

A driven spherical orientation is not represented by arbitrary Euler angles without a separately frozen singularity/order contract.

## Generic relationship compatibility target

After Block 39, the planned initial compatibility boundary is:

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

Coincident
  Point <-> Point
  Point <-> Line
  Line <-> Point
  Point <-> Plane
  Plane <-> Point

Parallel
  Line/Axis <-> Line/Axis
  Plane <-> Plane

Perpendicular
  Line/Axis <-> Line/Axis
  Plane <-> Plane
```

This table is planned future compatibility, not a claim that those combinations are currently implemented.

## GUI selection boundary

Blocks 31-47 remain headless semantic model/query/equation/motion work.

A future picker consumes:

```text
persistent semantic source candidate
-> resolved source kind
-> available capabilities
-> selected relationship/joint type
-> compatibility resolver
```

The UI may highlight OCCT presentation objects, but committing selection persists BLCAD semantic source identity. Raw `TopoDS_Shape` or selection index never becomes the assembly endpoint.

## Persistence rule

Persist semantic model intent.

Regenerate:

```text
source classification
typed descriptors
capability vectors and projections
transformed target geometry
compatibility selection
connectivity and authority mappings
residuals/Jacobians/results
freshness snapshots/proposals/diagnostics
exchange products
posed geometry/contact records/sweep analyses
```

## Immediate next technical step

Block 34 is the current next technical step.

Implement Block 34 only: Geometry resolution of DatumPlane/DatumAxis/ConstructionLine/ConstructionPoint `ref:` sources into the implemented Block-31 taxonomy, reusing existing workplane/construction execution and exact transform chains.

Stable generated topology identity remains Block 35.

# General Assembly Geometric Target and Relationship Roadmap

Status: Blocks 31–35 are implemented. Block 36 is the current next technical step. Blocks 36–47 remain planned headless architecture.

This document is the active status and sequencing authority for the expansion from the current assembly target layer to semantic reference geometry, stable generated topology targets, generic geometric relationships, and richer motion-joint families.

Implemented contracts are canonical in:

- `docs/assembly-geometric-target-taxonomy-mvp5.md`
- `docs/assembly-reference-geometry-intent-mvp5.md`
- `docs/assembly-generated-topology-reference-mvp5.md`

The complete pre-implementation planning detail for Blocks 32–47 is preserved byte-for-byte in:

- `docs/assembly-general-geometric-target-roadmap-planning-baseline.md`

For still-planned blocks, that companion document's required work, failure policy, acceptance tags, proof matrices, compatibility tables, residual seeds, and explicit deferrals are incorporated by reference here. Historical top-level status statements in the planning baseline are not current implementation-status authority.

## Why this roadmap exists

The current assembly solver architecture is already general in transform authority, residual/Jacobian execution, cross-hierarchy solving, freshness, and atomic application. The remaining route to a mature CAD assembly selector is primarily semantic target expressiveness and deterministic compatibility.

Persistent semantic endpoint families now include:

```text
legacy generated feature roles
  <feature-id>.top|bottom|right|left|front|back
  <feature-id>.axis
  <feature-id>.seat

reference geometry
  ref:datum_plane:<encoded-id>
  ref:datum_axis:<encoded-id>
  ref:construction_line:<encoded-id>
  ref:construction_point:<encoded-id>

generated topology identity
  topo:cylindrical_face:<encoded-feature-id>:<encoded-profile-id>:wall
  topo:linear_edge:<encoded-feature-id>:<role>
  topo:circular_edge:<encoded-feature-id>:<encoded-profile-id>:<role>
  topo:vertex:<encoded-feature-id>:<role>
```

The Geometry target taxonomy remains derived. Persistent endpoint strings remain Core model intent.

## Non-negotiable architecture rules

1. Persistent assembly endpoints remain Core-owned semantic references.
2. Raw `TopoDS_Face`, `TopoDS_Edge`, `TopoDS_Vertex`, OCCT traversal/map identities, XDE labels, STEP entity ids, and memory addresses are never persistent target identity.
3. Local and occurrence-qualified endpoints continue to share one semantic-reference contract.
4. Root-space hierarchy evaluation continues to compose the exact existing component/parent transform chain.
5. `ComponentTransformAuthority` remains `(AssemblyDocumentId, local ComponentInstanceId)`.
6. Existing residual/Jacobian, central finite-difference, numeric solve, rank diagnostics, freshness, and atomic-application boundaries remain authoritative.
7. Existing legacy target strings remain backward compatible.
8. Typed target taxonomy precedes new source expansion.
9. Stable semantic source identity precedes Geometry topology lookup.
10. Target compatibility precedes new relationship equations.
11. Generic relationship persistent intent/JSON precedes Geometry execution.
12. Joint target compatibility precedes multi-coordinate joint state.
13. Multi-coordinate Core state is serialized before vector-drive motion execution.
14. Each richer joint family is implemented in its own block after the shared vector-drive boundary.
15. GUI picking is a later presentation consumer.

## Block 31 — Typed geometric target taxonomy and capability projection — Implemented

Primary boundary: Geometry-layer derived query semantics.

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

Implemented geometric capabilities in canonical order:

```text
Plane
Axis
Line
Point
Circle
Cylinder
Frame
```

`AssemblyResolvedGeometricTarget` retains:

```text
exact local or occurrence-qualified endpoint identity
source kind
derived source model metadata
typed descriptor variant
canonical capability vector
ComponentLocal or RootAssembly coordinate space
exact transforms_inner_to_outer context
```

Explicit projection APIs are:

```text
project_plane
project_axis
project_line
project_point
project_circle
project_cylinder
project_frame
```

Canonical source capability matrix:

```text
GeneratedPlanarFace      -> Plane
GeneratedCylindricalFace -> Axis + Cylinder
GeneratedLinearEdge      -> Line
GeneratedCircularEdge    -> Axis + Point(center) + Circle
GeneratedVertex          -> Point
DatumPlane               -> Plane
DatumAxis                -> Axis + Line
ConstructionLine         -> Line
ConstructionPoint        -> Point
CircularFeatureSeat      -> Plane + Axis + Frame
```

Current legacy migration remains:

```text
.top/.bottom/.right/.left/.front/.back
  -> GeneratedPlanarFace -> Plane

.axis current single-CircleProfile subtractive producer
  -> GeneratedCylindricalFace -> Axis + Cylinder

.seat
  -> CircularFeatureSeat -> Plane + Axis + Frame
```

Existing equation and residual consumers are routed through the projection boundary without changing their numeric semantics.

Focused tag:

```text
[geometry][assembly-geometric-target-taxonomy]
```

## Mandatory continuation order

Blocks 32 through 35 are implemented. The remaining order is unchanged.

```text
32 assembly-selectable reference geometry Core intent (implemented)
  -> 33 reference geometry serialization and structure validation (implemented)
  -> 34 datum/axis/line/point target resolution (implemented)
  -> 35 stable semantic generated topology identity/recovery (implemented)
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

Implemented first-class `DatumAxis` PartDocument intent with `Explicit` and `FromConstructionLine` definition families, exact ownership/dependency/invalidation rules, and the canonical reference semantic-source grammar:

```text
ref:datum_plane:<encoded-id>
ref:datum_axis:<encoded-id>
ref:construction_line:<encoded-id>
ref:construction_point:<encoded-id>
```

Every id byte outside `[A-Za-z0-9_-]` is escaped as uppercase `%HH`. Valid reference spellings are dot-free, disjoint from legacy feature-role spellings, and fail closed on non-canonical encoding.

Focused tags:

```text
[core][datum-axis]
[core][assembly-reference-target-intent]
```

## Block 33 — Reference geometry serialization and structure validation — Implemented

Canonical document: `docs/assembly-reference-geometry-intent-mvp5.md`. Save-format authority: `docs/file-format.md`.

Implemented the additive optional `datum_axes` PartDocument array for both DatumAxis families with historical-file compatibility and load-time ownership/family validation. Existing local and cross-hierarchy endpoint JSON shapes remain unchanged; `ref:` semantic-reference strings roundtrip byte-for-byte. JSON loading resolves no geometry.

Focused tags:

```text
[core][datum-axis-json]
[core][assembly-reference-target-json]
```

## Block 34 — Datum, axis, line, and point target resolution — Implemented

Primary boundary: Geometry semantic target resolution.

Implemented:

```text
DatumPlane        -> Plane
DatumAxis         -> Axis + Line
ConstructionLine  -> Line
ConstructionPoint -> Point
```

Resolution reuses `WorkplaneResolver`, construction-line resolution, and construction-point resolution. Local targets remain component-local; hierarchy targets use the exact existing component-plus-parent transform chain. Canonical PartDocument snapshots remain freshness authority.

Focused tag:

```text
[geometry][assembly-reference-target-resolution]
```

## Block 35 — Stable semantic generated topology identity and recovery — Implemented

Canonical document: `docs/assembly-generated-topology-reference-mvp5.md`.

Primary boundary: Core semantic reference model and recovery rules.

Implemented generated topology identity families:

```text
GeneratedCylindricalFace
GeneratedLinearEdge
GeneratedCircularEdge
GeneratedVertex
```

The typed Core identity variant is:

```text
SemanticCylindricalFaceReference
SemanticEdgeReference
SemanticCircularEdgeReference
SemanticVertexReference
```

Canonical `topo:` spellings retain exact semantic producer identity. Cylindrical-face and circular-edge references retain exact source `FeatureId` plus source `ProfileId`; rectangular linear-edge and vertex references retain source `FeatureId` plus finite named role.

Supported producer matrix:

```text
RectangularAdditiveExtrude
  exactly one RectangleProfile, no other profile
  -> 12 named linear-edge roles, cardinality 1 each
  -> 8 named vertex roles, cardinality 1 each

SingleCircleSubtractiveExtrude
  exactly one CircleProfile, no other profile
  no CircularHolePattern
  direct target = supported RectangularAdditiveExtrude
  -> cylindrical wall, cardinality 1
  -> source_rim, cardinality 1
  -> opposite_rim, cardinality 1
```

Producer classification and `validate_generated_topology_reference` inspect only PartDocument model intent. They execute no OCCT operation. Unsupported producers, mixed/multiple-circle ambiguity, wrong profile ids, unsupported family/role pairs, and non-cardinality-one contracts fail closed.

Pattern-generated subelements remain unavailable until the pattern model has persistent per-instance semantic identity. Expanded result-vector position is not identity.

`ReferenceRecoveryEvaluator` now evaluates a `GeneratedTopologyReferenceIdentity` read-only. Supported exact producer/profile/role intent returns `Resolved`; missing, unsupported, ambiguous, patterned, or mismatched intent returns `Lost` with the exact validation message. Recovery writes no raw OCCT id and mutates no model intent.

Focused tags:

```text
[core][semantic-generated-topology-reference]
[core][semantic-generated-topology-recovery]
```

Block 35 adds no Geometry target resolver branch, equation, compatibility rule, relationship type, or joint family.

## Block 36 — Generated face, edge, and vertex target resolution — Next

Primary boundary: Geometry semantic target resolution.

Resolve Block-35 semantic sources into Block-31 descriptors/capabilities:

```text
GeneratedPlanarFace      -> Plane
GeneratedCylindricalFace -> Cylinder + Axis
GeneratedLinearEdge      -> Line
GeneratedCircularEdge    -> Circle + Axis + Point(center)
GeneratedVertex          -> Point
```

Resolution must start from canonical semantic producer identity. For `topo:` sources the resolver must parse before the legacy feature-role grammar, validate the Block-35 identity/recovery contract, identify the exact current generated subelement for the producer role, and prove current topology type and expected cardinality before descriptor construction.

Semantic producer identity remains authoritative. OCCT topology is execution data only after semantic resolution identifies the intended generated subelement.

Geometric validation remains:

```text
Line      finite non-degenerate unit direction
Circle    finite positive radius and right-handed frame
Cylinder  finite positive radius and non-degenerate unit axis
Plane     finite orthogonal frame under the established Plane convention
Point     finite
source topology type matches semantic producer role
```

Local resolution returns component-local descriptors. Hierarchy resolution evaluates requested capabilities through the exact rooted occurrence transform chain. Repeated rooted occurrences may share one PartDocument semantic target while retaining distinct root-space geometry.

Focused acceptance tag:

```text
[geometry][assembly-generated-topology-target-resolution]
```

Required proofs remain the Block-36 proofs from the planning baseline: cylinder Axis/Cylinder projection, circular edge Circle/Axis/center Point projection, linear edge Line projection, vertex Point projection, exact local/root transform semantics, repeated-root occurrence behavior, unsupported producer failure, and source-model immutability.

Block 36 adds no compatibility rule and no new relationship type.

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

Capability choice is deterministic for multi-capability targets. Equation builders consume the compatibility result, not source-kind enums. No new relationship enum or equation is added in Block 37.

## Block 38 — Generic geometric relationship Core intent and JSON

Primary boundary: persistent Core relationship model and serialization.

Add local and Project-level:

```text
Coincident
Parallel
Perpendicular
```

Preserve endpoint shapes, target A/B order, active/inactive state semantics, local versus Project-level id scopes, and historical five-family compatibility. No equation or graph participation changes belong in Block 38.

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

All new families reuse existing local/cross graphs, `ComponentTransformAuthority`, Block-31 projection, Block-37 compatibility selection, shared residual scaling, central finite differences, Gauss-Newton solving, PartDocument freshness, atomic application, and Jacobian-rank diagnostics. No generic-target-specific solver is allowed.

## Block 40 — Joint target compatibility and oriented Frame contract

Primary boundary: derived joint compatibility semantics.

Current Revolute remains:

```text
Frame <-> Frame
```

Current `.seat` sources project Frame. Axis alone is insufficient for signed twist because it has no deterministic reference X direction. Geometry may not synthesize a Frame from Axis using an arbitrary world axis.

No joint persistence change or new joint family belongs in Block 40.

## Blocks 41–43 — Multi-coordinate joint foundation

Block 41 generalizes the persistent local/Project-level joint coordinate/limit model into family-defined typed coordinate slots with explicit angular/linear kinds.

Block 42 serializes that coordinate-slot model additively and preserves historical scalar Revolute compatibility.

Block 43 generalizes selected-joint scalar drives to deterministic coordinate drive vectors with holding semantics, complete coordinate-slot freshness, and atomic application.

Authority variables remain six direct component-transform variables per unique free authority. Joint coordinates remain drive parameters.

## Blocks 44–47 — Richer joint families

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

## Planned generic relationship compatibility target

After Block 39, the planned initial compatibility boundary is:

```text
Mate          Plane <-> Plane
Distance      Plane <-> Plane | Point <-> Point | Point <-> Plane | Plane <-> Point
Angle         Plane <-> Plane | Line/Axis <-> Line/Axis
Concentric    Axis <-> Axis
Insert        Frame <-> Frame
Coincident    Point <-> Point | Point <-> Line | Line <-> Point | Point <-> Plane | Plane <-> Point
Parallel      Line/Axis <-> Line/Axis | Plane <-> Plane
Perpendicular Line/Axis <-> Line/Axis | Plane <-> Plane
```

This table is planned future compatibility, not a claim that those combinations are currently implemented.

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

The UI may highlight OCCT presentation objects, but committing selection persists BLCAD semantic source identity. Raw `TopoDS_Shape` or selection index never becomes the assembly endpoint.

## Persistence rule

Persist semantic model intent, including exact endpoint semantic-reference strings.

Regenerate:

```text
generated topology producer classification
producer role matrices and validation results
reference recovery query results
resolved source classification
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

Block 36 is the current next technical step.

Implement Block 36 only: Geometry resolution of Block-35 semantic generated topology references into generated cylindrical-face, linear-edge, circular-edge, and vertex descriptors/capabilities, including exact producer-role topology type/cardinality checks and local/root-space transform behavior.

Do not implement Block-37 compatibility rules in Block 36.

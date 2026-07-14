# General Assembly Geometric Target and Relationship Roadmap

Status: Blocks 31–47 and Part Construction Blocks 48–71 are implemented. The Assembly MVP handoff is complete; Block 72 is the current next technical step.

This document is the active status and sequencing authority for the expansion from the current assembly target layer to semantic reference geometry, stable generated topology targets, generic geometric relationships, and richer motion-joint families.

Implemented contracts are canonical in:

- `docs/assembly-geometric-target-taxonomy-mvp5.md`
- `docs/assembly-reference-geometry-intent-mvp5.md`
- `docs/assembly-generated-topology-reference-mvp5.md`
- `docs/assembly-generic-relationship-intent-mvp5.md`
- `docs/assembly-generic-relationship-equations-mvp5.md`
- `docs/assembly-joint-target-compatibility-mvp5.md`
- `docs/assembly-joint-coordinate-model-mvp5.md`
- `docs/assembly-joint-coordinate-json-mvp5.md`
- `docs/assembly-vector-joint-drive-mvp5.md`
- `docs/assembly-spherical-joint-mvp5.md`

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

Blocks 32 through 43 are implemented. The remaining order is unchanged.

```text
32 assembly-selectable reference geometry Core intent (implemented)
  -> 33 reference geometry serialization and structure validation (implemented)
  -> 34 datum/axis/line/point target resolution (implemented)
  -> 35 stable semantic generated topology identity/recovery (implemented)
  -> 36 generated face/edge/vertex target resolution (implemented)
  -> 37 explicit target compatibility matrix (implemented)
  -> 38 generic geometric relationship Core intent + JSON (implemented)
  -> 39 generic relationship equations + shared solve integration (implemented)
  -> 40 joint target compatibility + oriented Frame contract (implemented)
  -> 41 general joint coordinate/limit Core model (implemented)
  -> 42 general joint coordinate JSON/backward compatibility (implemented)
  -> 43 vector joint drives + holding/freshness/atomic application (implemented)
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

## Block 36 — Generated face, edge, and vertex target resolution — Implemented

Primary boundary: Geometry semantic target resolution.

Implemented resolution of the four Block-35 `topo:` generated-topology families into Block-31 descriptors/capabilities (generated planar faces already resolve through the unchanged legacy `.top/.bottom/...` path):

```text
GeneratedCylindricalFace -> Cylinder + Axis
GeneratedLinearEdge      -> Line
GeneratedCircularEdge    -> Circle + Axis + Point(center)
GeneratedVertex          -> Point
```

`AssemblyConstraintTargetResolver::resolve_geometric` parses canonical `topo:` producer identity before the legacy feature-role grammar (valid `topo:` spellings contain no `.`), validates the Block-35 identity/recovery contract through `validate_generated_topology_reference`, then builds each descriptor from current model intent:

- the single-circle cylindrical wall reuses the existing circular-feature geometry (axis origin/direction and radius) already shared with the legacy `.axis` producer;
- circular rims are the two coaxial circles at the box faces the through-all cut passes through — `source_rim` is the plane nearest the sketched circle and `opposite_rim` the far plane, obtained by projecting the target box onto the hole axis exactly as `CircularCutAdapter` does;
- rectangular linear edges and vertices are derived from the target box extent (rectangle center/width/height plus extrude thickness) under the established Right=+x, Front=+y, Top=+z box convention.

Semantic producer identity remains authoritative. OCCT topology is not consulted; descriptors are computed analytically from validated feature/sketch/profile intent.

Geometric validation is enforced by the Block-31 descriptor validators:

```text
Line      finite non-degenerate unit direction
Circle    finite positive radius and right-handed frame
Cylinder  finite positive radius and non-degenerate unit axis
Point     finite
descriptor variant matches the semantic producer family
```

Local resolution returns component-local descriptors. Hierarchy resolution reuses the same local resolver and evaluates the descriptor through the exact rooted occurrence transform chain. Repeated rooted occurrences share one PartDocument semantic target while retaining distinct root-space geometry.

Focused acceptance tag:

```text
[geometry][assembly-generated-topology-target-resolution]
```

`tests/geometry/assembly_generated_topology_target_resolver_tests.cpp` proves cylinder Axis/Cylinder projection, circular source/opposite rim Circle/Axis/center Point projection, all twelve linear-edge Line projections, all eight vertex Point projections, exact local and rooted transform semantics, repeated-root occurrence behaviour, fail-closed unsupported producer/profile/role/spelling handling, and source-model immutability.

Block 36 adds no compatibility rule, no new relationship type, and no JSON field.

## Block 37 — Explicit target compatibility matrix — Implemented

Primary boundary: derived relationship compatibility semantics.

Implemented one deterministic resolver:

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

Capability choice is deterministic for multi-capability targets. Implemented API:

```text
AssemblyTargetCompatibilityResolver
  relationship type + target A/B resolved capability vectors
  -> AssemblyTargetCompatibility ordered capability pair
  -> explicit incompatibility error
```

Existing local and cross-hierarchy Mate/Distance/Angle/Concentric/Insert equation builders consume the compatibility result before projection. They still only construct the pre-Block-37 equation families; compatible non-Plane Distance/Angle combinations are selected by the compatibility layer but do not gain new residual equations in this block.

Focused tags:

```text
[geometry][assembly-target-compatibility]
[geometry][assembly-cross-hierarchy-target-compatibility]
```

`tests/geometry/assembly_target_compatibility_tests.cpp` proves the supported matrix, incompatible matrix representatives, deterministic multi-capability choices, DatumPlane-to-generated-face Mate, DatumAxis-to-cylindrical-face Concentric, circular-edge-to-DatumAxis Concentric, current `.seat` Insert as Frame/Frame, local/hierarchy compatibility agreement, compatibility failure before equation construction, and source-model immutability.

No new relationship enum, equation, JSON field, graph rule, or persisted Geometry query product is added in Block 37.

## Block 38 — Generic geometric relationship Core intent and JSON — Implemented

Canonical document: `docs/assembly-generic-relationship-intent-mvp5.md`.

Primary boundary: persistent Core relationship model and serialization.

Implemented local and Project-level:

```text
Coincident
Parallel
Perpendicular
```

The shared `AssemblyConstraintType` enum now carries all three families. Local `AssemblyConstraint` and Project-level `AssemblyHierarchyConstraint` reuse unchanged endpoint records, preserve target A/B order, reuse active/inactive state semantics, and retain independent local versus Project-level id scopes.

Canonical JSON spellings are:

```text
coincident
parallel
perpendicular
```

All three new families carry neither `distance` nor `angle`; typed Core construction remains value-family authority and JSON loading reuses it fail closed. Existing Mate/Concentric/Distance/Insert/Angle JSON spellings and shapes remain unchanged.

Because existing graph builders are generic over `AssemblyConstraint`, Block 38 adds an explicit private participation guard preserving the historical five equation-enabled graph families. Coincident/Parallel/Perpendicular remain absent from current local solve, cross-hierarchy solve, and cross-hierarchy motion graphs until Block 39.

Focused tags:

```text
[core][assembly-generic-relationship-intent]
[core][assembly-generic-relationship-json]
```

`tests/core/assembly_generic_relationship_tests.cpp` proves local and occurrence-qualified intent, independent id scopes, target order, state, scalar rejection, lowercase JSON roundtrips, historical five-family compatibility, transform immutability, and persistent-only nonparticipation in current solve/motion graphs.

Block 38 adds no residual/equation, no target compatibility entry for the new families, no JSON field/schema bump, and no Geometry resolution during Core construction or loading.

## Block 39 — Generic relationship equations and shared solve integration — Implemented

Canonical document: `docs/assembly-generic-relationship-equations-mvp5.md`.

Primary boundary: Geometry equation semantics plus existing numeric/diagnostic execution.

Implemented Coincident pairs:

```text
Point <-> Point
Point <-> Line
Line <-> Point
Point <-> Plane
Plane <-> Point
```

Canonical residuals are:

```text
Point-Point
  pB - pA

Point-Line
  cross(point - line.origin, line.direction)

Point-Plane
  dot(point - plane.origin, plane.normal)
```

Parallel and Perpendicular support Line/Axis direction pairs and Plane normal pairs. Parallel uses `cross(dA, dB)` and accepts parallel or anti-parallel directions; Perpendicular uses `dot(dA, dB)`.

The shared capability-driven equation builder also closes Line/Axis Angle execution selected by Block 37 through `dot(dA, dB) - cos(target_angle_deg)`.

Coincident/Parallel/Perpendicular now participate in existing local/cross solve and motion connectivity. All new equations reuse existing `ComponentTransformAuthority`, target projection, deterministic relationship order, residual scaling, central finite differences, Gauss-Newton solving, PartDocument freshness, atomic application, and Jacobian-rank diagnostics.

Focused tags:

```text
[geometry][assembly-generic-relationships]
[geometry][assembly-generic-relationships-solver]
[geometry][assembly-generic-relationships-cross-hierarchy]
[geometry][assembly-generic-relationships-diagnostics]
```

No generic-target-specific solver, graph, transform authority, finite-difference implementation, endpoint model, persistence field, or JSON change was added.

## Block 40 — Joint target compatibility and oriented Frame contract — Implemented

Primary boundary: derived joint compatibility semantics.

Current Revolute remains:

```text
Frame <-> Frame
```

Current `.seat` sources project Frame. Axis alone is insufficient for signed twist because it has no deterministic reference X direction. Geometry may not synthesize a Frame from Axis using an arbitrary world axis.

No joint persistence change or new joint family belongs in Block 40.

Implemented `AssemblyJointTargetCompatibilityResolver` as the derived compatibility authority for joint endpoints. The initial deterministic matrix remains exactly `Revolute -> Frame / Frame`. Both local and cross-hierarchy Revolute builders now resolve typed geometric targets, consume this compatibility before projection, preserve original target scope/identity, and route the selected oriented Frames through one shared residual constructor.

Axis-only Revolute fails explicitly through the joint compatibility authority because Axis supplies no deterministic reference X direction. No arbitrary world-axis Frame synthesis is permitted. The canonical implemented contract is `docs/assembly-joint-target-compatibility-mvp5.md`.

## Block 41 — General joint coordinate/limit Core model — Implemented

Persistent `AssemblyJoint` and `AssemblyHierarchyJoint` state now owns ordered, family-defined `AssemblyJointCoordinateSlot` values. Every slot has a stable semantic role, an explicit Angular/Linear kind, a typed authored value, and independently optional typed bounds. Angular coordinates use `AngleDeg`; signed linear coordinates use the dedicated `LinearDisplacementMm` quantity rather than positive construction length or a unitless scalar.

The implemented role vocabulary is `rotation`, `translation`, `translation_u`, `translation_v`, and `rotation_normal`. Current Revolute validates exactly one bounded Angular `rotation` slot. Existing scalar constructors, `with_coordinate`, `limits()`, and `coordinate_deg()` remain explicit Revolute compatibility views over that slot. Local and exact rooted target scopes remain separate.

No JSON record changed and no vector motion drive or new joint family was introduced. Canonical contract: `docs/assembly-joint-coordinate-model-mvp5.md`.

Focused tags:

```text
[core][assembly-joint-coordinate-model]
[core][assembly-cross-hierarchy-joint-coordinate-model]
```

## Block 42 — General joint coordinate JSON and compatibility — Implemented

Local and Project-level joint records now serialize deterministic family-ordered `coordinates[]` using canonical role/kind spellings and typed `deg`/`mm` quantities. Current Revolute writers also retain historical `limits` plus scalar `coordinate` fields, so old readers keep working without a schema/version change.

Readers accept slot-only, historical-only, or exactly matching dual records. Unknown/duplicate/missing roles, unknown kinds, wrong units, invalid family order/limits, partial legacy fields, and conflicting dual state fail closed. Canonical contract: `docs/assembly-joint-coordinate-json-mvp5.md`.

Focused tags:

```text
[core][assembly-joint-coordinate-json]
[core][assembly-cross-hierarchy-joint-coordinate-json]
```

## Block 43 — Multi-coordinate drives — Implemented

Local and Project-level solvers now accept typed `AssemblyJointDrive` requests addressed by stable coordinate role. Family order is restored deterministically; omitted selected roles and every non-selected active joint slot hold authored values. Results protect complete coordinate-slot definitions and explicit drives, and appliers update transforms plus exactly driven selected roles on one Project copy. Scalar Revolute entry points remain adapters. Canonical contract: `docs/assembly-vector-joint-drive-mvp5.md`.

Focused tags:

```text
[geometry][assembly-vector-joint-drive]
[geometry][assembly-cross-hierarchy-vector-joint-drive]
[geometry][assembly-vector-joint-drive-application]
```

Authority variables remain six direct component-transform variables per unique free authority. Joint coordinates remain drive parameters.

## Block 44 — Prismatic joint family — Implemented

Canonical contract: `docs/assembly-prismatic-joint-mvp5.md`.

Prismatic uses oriented `Frame <-> Frame` endpoints and exactly one bounded Linear
`translation` coordinate. It participates in local and cross-hierarchy graphs and shared
vector-drive motion, freshness, and atomic application.

## Block 45 — Cylindrical joint family — Implemented

Canonical contract: `docs/assembly-cylindrical-joint-mvp5.md`.

Cylindrical composes bounded Linear `translation` and Angular `rotation` coordinates on oriented
`Frame <-> Frame` endpoints. Both roles share the local/root-space graph, numeric, holding,
freshness, and atomic application paths.

## Block 46 — Planar joint family — Implemented

Canonical contract: `docs/assembly-planar-joint-mvp5.md`.

Planar uses bounded Linear `translation_u`, Linear `translation_v`, and Angular
`rotation_normal` coordinates on oriented `Frame <-> Frame` endpoints and the shared local/root
motion, holding, freshness, and atomic application paths.

## Block 47 — Ball/Spherical joint family — Implemented

Canonical contract: `docs/assembly-spherical-joint-mvp5.md`.

```text
Spherical
  persistent spelling: spherical
  coordinates: []
  compatibility: Point <-> Point
  residual: point_b - point_a
```

The passive family participates in local and cross-hierarchy motion closure while a different joint
is selected. Selecting Spherical itself as a drive fails explicitly. A driven spherical orientation
is not represented by arbitrary Euler angles without a separately frozen singularity/order
contract.

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

Blocks 31–47 are complete headless semantic model/query/equation/motion work.

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

## Current next technical step — Block 72

Block 47 is implemented in `docs/assembly-spherical-joint-mvp5.md` and completes the Assembly MVP
handoff. Blocks 48–71 Body identity, body-scoped recompute/inspection, Body Booleans, and
BodyTransform/SketchOwnership intent plus associative Geometry execution, reusable Part-feature
input references, richer Extrude/Cut intent/JSON plus Geometry, persistent plus executed
Revolve/RevolveCut, General Part Pattern Core intent/JSON, Linear/Circular Pattern Geometry, and
MirrorFeature Core intent/JSON plus Geometry, Fillet/Chamfer Core intent/JSON plus Geometry, and
ShellFeature Core intent/JSON are implemented. Add ShellFeature Geometry according to
`docs/part-construction-sequence-mvp6.md`.

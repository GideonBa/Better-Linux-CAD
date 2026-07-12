# General Assembly Geometric Target and Relationship Roadmap

Status: planned headless architecture after Block 30. Blocks 31-37 are not implemented yet.

This document is canonical for the planned expansion from the current narrow generated plane/axis/seat target families to an Inventor-/SolidWorks-like assembly target model that can deliberately select semantic faces, cylindrical surfaces, edges, vertices, datum planes, datum axes, construction lines, and construction points without persisting raw OCCT topology identity.

## Why this roadmap exists

The current assembly solver architecture is already general in its transform authority, residual/Jacobian, cross-hierarchy, freshness, and atomic-application boundaries. The current limitation is target expressiveness.

Today the implemented resolver primarily understands:

```text
feature.<feature-id>.top|bottom|right|left|front|back
feature.<feature-id>.axis
feature.<feature-id>.seat
```

Those references resolve to generated planar geometry, a narrow circular-feature axis, or the composite circular seat used by Insert/Revolute semantics.

That is intentionally stable but not yet equivalent to a mature CAD assembly picker.

The planned target architecture must eventually support source selections conceptually equivalent to:

```text
planar generated face
cylindrical generated face
linear generated edge
circular generated edge
generated vertex
DatumPlane
DatumAxis
construction line
construction point
```

A relationship equation must consume geometric capabilities, not hard-code which feature family produced them.

## Non-negotiable architecture rules

1. Persistent assembly endpoints remain semantic references owned by Core model intent.
2. Raw `TopoDS_Face`, `TopoDS_Edge`, `TopoDS_Vertex`, OCCT indices, XDE labels, and STEP entity ids are never persistent target identity.
3. Local and occurrence-qualified cross-hierarchy endpoints continue to share one semantic-reference contract.
4. Root-space hierarchy evaluation continues to compose the exact existing component/parent transform chains.
5. `ComponentTransformAuthority` remains `(AssemblyDocumentId, local ComponentInstanceId)`.
6. The shared six-variable rigid-body numeric engine, finite-difference contract, matrix-rank diagnostics, freshness, and atomic application remain authoritative.
7. Existing `.top/.bottom/.right/.left/.front/.back`, `.axis`, and `.seat` references remain backward compatible.
8. Target taxonomy must be implemented before adding generic relationship families. Do not add one-off resolver branches directly inside equation builders.
9. Compatibility between selected targets and relationship families must be explicit and fail closed.
10. GUI picking is a later presentation consumer. Blocks 31-37 establish headless semantic target contracts first.

## Source identity versus geometric capability

A mature assembly system must distinguish what the user selected from what geometric primitive a solver may consume.

Example:

```text
selected source = cylindrical generated face
solver capability = axis
```

or:

```text
selected source = circular generated edge
solver capabilities = circle + axis + point
```

Therefore source kind and geometric capability must not be one enum.

### Planned source kinds

The first planned source taxonomy is:

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

`CircularFeatureSeat` retains the existing composite `.seat` meaning and remains a semantic feature selection rather than raw topology.

Additional source kinds require a separate roadmap update.

### Planned geometric capability families

The first planned derived capability families are:

```text
Plane
Axis
Line
Point
Circle
Cylinder
SeatFrame
```

Capabilities are derived/unpersisted.

Representative projections:

```text
GeneratedPlanarFace
  -> Plane

DatumPlane
  -> Plane

DatumAxis
  -> Axis
  -> Line

ConstructionLine
  -> Line

ConstructionPoint
  -> Point

GeneratedVertex
  -> Point

GeneratedCircularEdge
  -> Circle
  -> Axis
  -> Point(center)

GeneratedCylindricalFace
  -> Cylinder
  -> Axis

CircularFeatureSeat
  -> SeatFrame
  -> Axis
  -> Plane
```

A relationship family may request one capability from each selected source. The selected source identity remains intact for freshness, diagnostics, and future presentation.

## Planned typed resolved-target boundary

Block 31 should introduce a Geometry-layer value concept equivalent to:

```text
AssemblyResolvedGeometricTarget
  endpoint identity
  source kind
  source model identity metadata
  typed geometry descriptor variant
  available capability set
  current component transform context
```

The descriptor variant should be explicit rather than a bag of unrelated optional fields. Candidate descriptors:

```text
AssemblyPlanarTargetDescriptor
AssemblyAxisTargetDescriptor
AssemblyLineTargetDescriptor
AssemblyPointTargetDescriptor
AssemblyCircularEdgeTargetDescriptor
AssemblyCylindricalSurfaceTargetDescriptor
AssemblySeatTargetDescriptor
```

Capability projection helpers should be the only supported path from a resolved descriptor to solver geometry.

Examples:

```text
project_plane(target)
project_axis(target)
project_line(target)
project_point(target)
project_circle(target)
project_cylinder(target)
project_seat_frame(target)
```

A projection must fail closed when the source does not expose the requested capability.

Equation builders must not `std::visit` source kinds and infer compatibility independently.

## Block 31 — Typed geometric target taxonomy and capability projection

Primary boundary: Geometry-layer derived query semantics.

### Goal

Generalize the current plane/axis/seat resolver outputs into one typed target taxonomy while preserving all existing local and hierarchy target behavior.

### Required work

1. Introduce `AssemblyGeometricTargetSourceKind` as derived source classification.
2. Introduce typed resolved descriptor values for Plane, Axis, Line, Point, Circle/CircularEdge, Cylinder/CylindricalSurface, and SeatFrame targets.
3. Introduce one resolved-target variant/value that retains exact persistent endpoint identity and source metadata.
4. Introduce explicit capability projection helpers.
5. Adapt the current generated planar face, `.axis`, and `.seat` resolver paths to produce the new descriptors.
6. Keep existing `resolve`, `resolve_axis`, and `resolve_insert` compatibility APIs until downstream builders have migrated.
7. Add equivalent local-space and hierarchy/root-space capability projection rules.
8. Persist no taxonomy/capability cache.

### Stable ordering/identity

No new persistent id is introduced.

Local endpoint identity remains:

```text
(local ComponentInstanceId, semantic_reference)
```

Cross-hierarchy endpoint identity remains:

```text
(occurrence_path,
 local ComponentInstanceId,
 semantic_reference)
```

Capability ordering for deterministic diagnostics/tests:

```text
Plane
Axis
Line
Point
Circle
Cylinder
SeatFrame
```

### Failure policy

Fail closed on unsupported semantic source, malformed resolved source geometry, non-finite descriptors, degenerate direction vectors, non-orthonormal plane/seat frames, non-positive circle/cylinder radius, and unsupported capability projection.

### Acceptance tag

```text
[geometry][assembly-geometric-target-taxonomy]
```

### Required proofs

- all current six generated planar face tokens resolve as `GeneratedPlanarFace -> Plane`;
- current single-circle `.axis` resolves as `Axis`;
- current `.seat` resolves as `SeatFrame`, `Axis`, and `Plane` capabilities;
- capability projection is deterministic;
- unsupported projections fail closed;
- local and hierarchy target geometry remain numerically unchanged;
- all existing Mate/Distance/Angle/Concentric/Insert/Revolute tests remain compatible;
- no Project or PartDocument mutation.

### Explicitly deferred

No new semantic source grammar, relationship family, joint family, or JSON shape in Block 31.

## Block 32 — Assembly-selectable reference geometry intent and serialization

Primary boundary: Core persistent model intent and serialization.

### Goal

Make datum/reference geometry deliberately selectable by assembly endpoints instead of treating only generated feature geometry as assembly target sources.

### Existing intent to reuse

BLCAD already has persistent `DatumPlane`, construction lines, and construction points in part model intent.

Block 32 must reuse those identities rather than duplicate them as assembly-specific geometry records.

### New DatumAxis intent

If no first-class `DatumAxis` record exists at implementation time, add one to `PartDocument`.

Planned minimal persistent record:

```text
DatumAxisId
name
origin definition
axis direction definition
visibility
```

The exact origin/direction definition should use existing parametric/reference infrastructure where possible. A first implementation may support an explicit model-space origin plus direction or a line-derived axis, but the canonical Block-32 document must freeze the dependency and invalidation semantics before code is merged.

`DatumAxis` must participate in:

```text
PartDocument ownership
id uniqueness
JSON roundtrip
dependency/invalidation when parameter/reference driven
canonical PartDocument model-intent serialization
```

### Semantic target families to freeze

Block 32 must freeze exact parseable semantic-reference spellings for:

```text
DatumPlane selection
DatumAxis selection
ConstructionLine selection
ConstructionPoint selection
```

The grammar must remain unambiguous even though BLCAD typed ids currently require only non-empty strings and may contain dots, slashes, or percent characters.

Do not adopt a delimiter grammar until parser ambiguity has been explicitly tested.

Existing feature target spellings remain unchanged.

### Persistence rule

Assembly endpoints continue to persist only their semantic-reference string plus component/occurrence identity.

No resolved plane, axis, line, or point coordinates are serialized into constraints/joints.

### Acceptance tags

```text
[core][datum-axis]
[core][assembly-reference-target-intent]
[core][assembly-reference-target-json]
```

### Required proofs

- existing DatumPlane identity can be represented as an assembly semantic target;
- DatumAxis model intent validates and roundtrips if introduced;
- construction line and construction point identity can be represented without copying geometry into assembly records;
- ambiguous/malformed semantic target tokens fail closed;
- target strings roundtrip byte-for-byte;
- existing project files remain compatible;
- no geometry resolution during Core JSON load.

### Explicitly deferred

Block 32 does not resolve geometry and does not modify relationship compatibility.

## Block 33 — Datum, axis, line, and point target resolution

Primary boundary: Geometry semantic target resolution.

### Goal

Resolve the Block-32 semantic reference sources into the Block-31 target taxonomy.

### Required source-to-capability mapping

```text
DatumPlane
  -> Plane

DatumAxis
  -> Axis
  -> Line

ConstructionLine
  -> Line

ConstructionPoint
  -> Point
```

The existing `WorkplaneResolver`, construction line resolver, and construction point resolver must be reused where applicable.

Do not implement a second workplane or construction-geometry evaluator inside the assembly resolver.

### Local and cross-hierarchy behavior

Local resolution returns component-local descriptors plus the direct component transform context.

Hierarchy resolution applies the exact existing transform chain:

```text
[T_component, T_inner_parent, ..., T_outer_parent]
```

using `AssemblyHierarchyTransformEvaluator`.

Points translate and rotate. Axis/line directions rotate only. Plane frames retain handedness.

### Freshness

The existing exact canonical `PartDocument` model-intent snapshot already conservatively protects these target-producing records. Block 33 must prove that local and cross-hierarchy application becomes stale when the selected datum/construction source changes.

Do not introduce a second target-specific revision counter.

### Acceptance tag

```text
[geometry][assembly-reference-target-resolution]
```

### Required proofs

- planar generated face and DatumPlane expose equivalent Plane capability to equation builders;
- DatumAxis and a compatible generated circular-feature axis expose equivalent Axis capability;
- ConstructionLine resolves as an oriented Line;
- ConstructionPoint resolves as a Point;
- local and nested cross-hierarchy transform evaluation is correct;
- source edits invalidate old local/cross solve results through existing PartDocument freshness;
- source Project remains unchanged.

### Explicitly deferred

No generated edge/vertex/cylindrical-surface target identity in Block 33.

## Block 34 — Stable semantic generated topology identity

Primary boundary: Part semantic-reference model and recovery rules.

### Goal

Define stable BLCAD semantic identities for generated cylindrical faces, linear edges, circular edges, and vertices before assembly target resolution consumes them.

This block is required because raw OCCT topology order is not acceptable persistent identity.

### Planned semantic source families

```text
GeneratedCylindricalFace
GeneratedLinearEdge
GeneratedCircularEdge
GeneratedVertex
```

### Producer-driven semantic identity

Every supported feature producer must define its own finite semantic subelement roles from parametric model intent.

Examples of acceptable identity sources:

```text
feature identity
source sketch/profile identity
profile entity identity
feature direction/side role
pattern instance semantic identity when already stable
named generated-face role
named generated-edge role
named generated-vertex role
```

Unacceptable identity sources:

```text
TopoDS_Shape::HashCode as model identity
TopExp traversal index
BRepTools map position
XDE label tag
STEP entity number
memory address
unordered OCCT result order
```

### Required design output

Before geometry resolution is added, Block 34 must publish a producer support matrix:

```text
Feature producer
  -> supported semantic face roles
  -> supported semantic edge roles
  -> supported semantic vertex roles
  -> expected cardinality
  -> ambiguity/failure behavior
```

The first implementation does not need every BLCAD feature family. It must explicitly list supported producers and reject all others.

### Pattern identity

The current target resolver already rejects generic `CircularHolePattern.axis/seat` because multiple holes are ambiguous.

Block 34 must define stable per-instance pattern identity before any patterned cylindrical face/edge/vertex is selectable.

A pattern target must identify a semantic pattern instance from model intent. It must not use the current vector position unless that position is itself an explicitly frozen persistent instance identity contract.

### Reference recovery

Where the existing semantic reference/recovery infrastructure can represent the new generated subelement roles, reuse it.

If recovery requires additional source metadata, persist only semantic producer-role intent. Do not persist OCCT topology handles.

### Acceptance tags

```text
[core][semantic-generated-topology-reference]
[core][semantic-generated-topology-recovery]
```

### Required proofs

- supported producer roles are unique and deterministic;
- feature insertion order and OCCT topology traversal order cannot change semantic target identity;
- unsupported/ambiguous producers fail closed;
- patterned instances are not addressable until stable per-instance identity exists;
- JSON roundtrip preserves semantic references;
- reference recovery never writes raw OCCT ids into model intent.

### Explicitly deferred

Block 34 defines identity/recovery only. It does not add assembly equations.

## Block 35 — Generated face, edge, and vertex target resolution

Primary boundary: Geometry semantic target resolution.

### Goal

Resolve Block-34 semantic generated topology references into Block-31 typed descriptors and capabilities.

### Required source-to-capability mapping

```text
GeneratedPlanarFace
  -> Plane

GeneratedCylindricalFace
  -> Cylinder
  -> Axis

GeneratedLinearEdge
  -> Line

GeneratedCircularEdge
  -> Circle
  -> Axis
  -> Point(center)

GeneratedVertex
  -> Point
```

### Resolution policy

Resolution starts from semantic producer identity and reconstructs or queries the current recomputed geometry for that exact semantic subelement.

The source semantic identity remains authoritative. OCCT topology is execution data used only after semantic resolution has identified the intended generated subelement.

A descriptor must preserve source metadata sufficient for diagnostics and future presentation:

```text
referenced PartDocumentId
source feature id
source semantic role
optional source sketch/profile/entity identity
resolved source kind
```

### Geometric validation

- line direction finite and non-degenerate;
- circle radius finite and positive;
- circle frame finite and right-handed;
- cylinder radius finite and positive;
- cylinder axis finite and non-degenerate;
- planar frame finite and right-handed;
- point finite;
- source topology type matches the semantic source role.

### Local and hierarchy behavior

The local resolver returns component-local descriptors.

The hierarchy resolver evaluates the projected capability through the exact occurrence transform chain. It must not transform or copy OCCT topology as persistent identity.

### Acceptance tag

```text
[geometry][assembly-generated-topology-target-resolution]
```

### Required proofs

- cylindrical face resolves to Cylinder and Axis capabilities;
- circular edge resolves to Circle, Axis, and center Point capabilities;
- linear edge resolves to Line;
- vertex resolves to Point;
- local and cross-hierarchy geometry agree with exact transform-chain semantics;
- repeated rooted occurrences retain distinct world-space geometry while sharing one source PartDocument semantic target;
- unsupported semantic producer roles fail closed;
- no source model mutation.

### Explicitly deferred

Block 35 adds no new relationship family.

## Block 36 — Explicit target compatibility matrix and generic geometric relationships

Primary boundary: relationship semantics plus shared numeric integration.

### Goal

Replace source-family-specific target assumptions with an explicit compatibility matrix based on Block-31 geometric capabilities, then add the first generic relationship families needed for Inventor-/SolidWorks-like assembly workflows.

### Compatibility resolution rule

For every relationship type:

```text
relationship type
  + target A source capabilities
  + target B source capabilities
  -> one exact ordered capability pair
  OR explicit incompatibility
```

Compatibility selection must be deterministic. It must not choose a different interpretation because a target exposes several capabilities unless the relationship's canonical priority rule says so.

### Planned initial compatibility matrix

#### Mate

```text
Plane <-> Plane
```

Sources may be generated planar faces or DatumPlanes.

#### Distance

Initial required pairs:

```text
Plane <-> Plane
Point <-> Point
Point <-> Plane
Plane <-> Point
```

The canonical sign/direction rule must be frozen separately for each ordered pair.

Do not silently reinterpret Line/Axis distance in the first block unless exact signed/unsigned semantics are documented.

#### Angle

Initial required pairs:

```text
Plane <-> Plane
Line <-> Line
Axis <-> Axis
Line <-> Axis
Axis <-> Line
```

Directed versus direction-symmetric behavior must be explicit. The existing plane cosine semantics may remain direction-symmetric for backward compatibility, but generic line/axis angle semantics must document whether opposed directions are equivalent.

#### Concentric

```text
Axis <-> Axis
```

The Axis capability may originate from:

```text
current generated circular feature axis
DatumAxis
GeneratedCylindricalFace
GeneratedCircularEdge
CircularFeatureSeat
```

No Concentric equation builder should care which source produced the Axis capability.

#### Insert

```text
SeatFrame <-> SeatFrame
```

The existing `.seat` family remains valid.

A later compatibility expansion may construct an Insert from separate Axis + Plane selections, but that is not implicit in Block 36.

### New planned relationship families

#### Coincident

Initial pairs:

```text
Point <-> Point
Point <-> Line
Line <-> Point
Point <-> Plane
Plane <-> Point
```

Candidate residual semantics:

```text
Point-Point:
  pB - pA

Point-Line:
  cross(pA - line.origin, line.direction)

Point-Plane:
  dot(point - plane.origin, plane.normal)
```

Canonical scalar/vector flattening order and length scaling must be frozen before implementation.

#### Parallel

Initial pairs:

```text
Line <-> Line
Axis <-> Axis
Line <-> Axis
Axis <-> Line
Plane <-> Plane
```

Candidate directional residual:

```text
cross(dA, dB)
```

For planes, directions are normals.

The relationship must explicitly state whether parallel and anti-parallel are both accepted. The proposed first seed accepts both through cross-product residual semantics.

#### Perpendicular

Initial pairs mirror Parallel.

Candidate scalar residual:

```text
dot(dA, dB)
```

For planes, directions are normals.

### Persistence

If new relationship enum values are added:

```text
Coincident
Parallel
Perpendicular
```

add them to local and Project-level persistent relationship type parsing/serialization in one compatibility block.

Existing constraint JSON remains backward compatible.

The endpoint JSON shape does not change.

### Graph/numeric/freshness reuse

New relationship families must enter:

```text
AssemblyConstraintGraph
AssemblyCrossHierarchyConstraintGraph
shared local numeric relationship order
cross-hierarchy mixed relationship order
fresh relationship snapshots
matrix-rank diagnostics
```

through the existing relationship-family extension points.

Do not create a generic-target-only solver.

### Acceptance tags

```text
[core][assembly-target-compatibility]
[core][assembly-generic-relationship-intent]
[core][assembly-generic-relationship-json]
[geometry][assembly-generic-relationships]
[geometry][assembly-generic-relationships-solver]
[geometry][assembly-generic-relationships-cross-hierarchy]
[geometry][assembly-generic-relationships-diagnostics]
```

### Required proofs

- complete deterministic compatibility table tests;
- every supported source kind projects to the same equation semantics when capabilities are equivalent;
- DatumPlane-to-generated-face Mate;
- DatumAxis-to-cylindrical-face Concentric;
- circular-edge-to-DatumAxis Concentric;
- line-line Angle;
- point-point Coincident;
- point-line Coincident;
- point-plane Coincident;
- Parallel and Perpendicular for line/axis and plane pairs;
- local and cross-hierarchy solving;
- repeated occurrence shared-authority behavior unchanged;
- freshness invalidates when target-producing part intent changes;
- rank diagnostics use actual Jacobian rank rather than relationship-name DOF tables;
- old five-family project files still load/solve unchanged.

### Explicitly deferred

No tangent relationship, spherical/cylindrical slider joint, gear joint, or contact relationship in Block 36.

## Block 37 — Joint target capability expansion and richer joint families

Primary boundary: persistent joint intent, motion connectivity, equations, and application.

### Goal

Make joint target compatibility consume the same Block-31 capability taxonomy instead of requiring only the current composite `.seat` target.

### First compatibility expansion

Revolute should support:

```text
SeatFrame <-> SeatFrame
Axis <-> Axis
```

For `Axis <-> Axis`, the joint equation must additionally define the rotational reference frame used to measure signed twist. An axis alone does not provide an X reference vector.

Therefore Block 37 must choose one explicit model-intent strategy before implementing Axis-only Revolute:

```text
A. joint stores an additional reference-direction target for A and B
B. joint stores a semantic frame target that exposes Axis + reference direction
C. Axis-only Revolute remains unsupported until such frame intent exists
```

Do not invent an arbitrary world-axis reference in Geometry.

Option B is preferred if a general semantic frame target exists by then; otherwise keep existing SeatFrame Revolute compatibility and defer Axis-only twist measurement.

### Planned richer joint families

After the frame requirement is resolved, plan explicit Core/JSON/equation blocks for at least:

```text
Prismatic
Cylindrical
Planar
Ball/Spherical
```

Potential capability requirements:

```text
Prismatic
  -> Axis/Line frame with translation coordinate

Cylindrical
  -> Axis frame with translation + twist coordinates

Planar
  -> Plane frame

Ball/Spherical
  -> Point <-> Point center coincidence with free rotation
```

Every joint family must freeze:

```text
persistent coordinate/limit fields
exact target capability requirements
motion DOF and coordinates
residual scalar order
holding-drive semantics
combined motion graph participation
freshness snapshots
atomic application behavior
local + Project-level JSON spellings
```

Do not model these as geometric constraints with hidden state.

### Acceptance tags

Family-specific tags should follow:

```text
[core][assembly-<joint>-joint]
[core][assembly-cross-hierarchy-<joint>-joint]
[geometry][assembly-<joint>-joint]
[geometry][assembly-cross-hierarchy-<joint>-motion]
```

### Required cross-family proof

A target source such as `DatumAxis` or `GeneratedCylindricalFace` must expose one Axis capability contract consumed identically by Concentric and any joint family that declares Axis compatibility.

### Explicitly deferred

General contact joints, cams, gears, screw/helical joints, motion laws, motors, forces, and physics response remain later work unless separately planned.

## GUI and selection presentation boundary

An Inventor-/SolidWorks-like picker is not part of Blocks 31-37 because BLCAD has no GUI yet.

When GUI work starts, the picker should query:

```text
persistent semantic source candidates
-> resolved source kind
-> available geometric capabilities
-> currently selected relationship/joint type
-> compatibility matrix
```

The UI may highlight faces, edges, vertices, datum planes, axes, lines, and points through OCCT presentation objects, but committing a selection must store the corresponding BLCAD semantic reference identity.

The UI must never persist the picked `TopoDS_Shape` or selection index as the assembly endpoint.

## Planned implementation order

After Block 30:

```text
31 typed geometric target taxonomy and capability projection
  -> 32 assembly-selectable datum/reference geometry intent + serialization
  -> 33 datum/axis/line/point target resolution
  -> 34 stable semantic generated topology identity/recovery
  -> 35 generated face/edge/vertex target resolution
  -> 36 explicit compatibility matrix + generic geometric relationships
  -> 37 joint target capability expansion + richer joint-family sequence
```

Do not merge Blocks 31-37 into one feature PR.

The sequence deliberately establishes identity before geometry, geometry before compatibility, and compatibility before new relationship/joint families.

## Immediate next technical step

Block 30 remains the current next technical step.

After Block 30, implement Block 31 only: typed derived geometric target taxonomy and capability projection while preserving all existing target strings and relationship behavior.

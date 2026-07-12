# General Assembly Geometric Target and Relationship Roadmap

Status: planned headless architecture after Block 30. Blocks 31-47 are not implemented yet.

This document is canonical for the planned expansion from the current narrow generated plane/axis/seat target families to an Inventor-/SolidWorks-like assembly target model that can deliberately select semantic faces, cylindrical surfaces, edges, vertices, datum planes, datum axes, construction lines, and construction points without persisting raw OCCT topology identity.

It also defines the sequenced path from general target compatibility to generic geometric relationships and richer motion-joint families.

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

The planned target architecture must support semantic source selections conceptually equivalent to:

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

A relationship or joint equation must consume geometric capabilities, not hard-code which feature family produced them.

## Non-negotiable architecture rules

1. Persistent assembly endpoints remain semantic references owned by Core model intent.
2. Raw `TopoDS_Face`, `TopoDS_Edge`, `TopoDS_Vertex`, OCCT indices, XDE labels, and STEP entity ids are never persistent target identity.
3. Local and occurrence-qualified cross-hierarchy endpoints continue to share one semantic-reference contract.
4. Root-space hierarchy evaluation continues to compose the exact existing component/parent transform chains.
5. `ComponentTransformAuthority` remains `(AssemblyDocumentId, local ComponentInstanceId)`.
6. The shared rigid-body numeric engine, finite-difference contract, matrix-rank diagnostics, freshness, and atomic application remain authoritative.
7. Existing `.top/.bottom/.right/.left/.front/.back`, `.axis`, and `.seat` references remain backward compatible.
8. Target taxonomy is implemented before general relationship compatibility.
9. Compatibility is implemented before new relationship equations.
10. Generic relationship persistent intent/JSON is implemented before its Geometry execution path.
11. Joint target compatibility is generalized before multi-coordinate joint state is introduced.
12. Multi-coordinate joint Core state is serialized before the motion engine accepts vector drives.
13. Each richer joint family is integrated in its own block after the shared vector-drive boundary exists.
14. GUI picking is a later presentation consumer. Blocks 31-47 establish headless semantic target and motion contracts first.

## Source identity versus geometric capability

A mature assembly system must distinguish what the user selected from what geometric primitive a solver may consume.

Example:

```text
selected source = cylindrical generated face
solver capability = Axis
```

or:

```text
selected source = circular generated edge
solver capabilities = Circle + Axis + Point
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

### Planned geometric capabilities

The first planned derived capability families are:

```text
Plane
Axis
Line
Point
Circle
Cylinder
Frame
```

Capabilities are derived/unpersisted.

`Frame` means one finite right-handed oriented frame:

```text
origin
x_axis
y_axis
z_axis
```

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
  -> Frame
  -> Axis
  -> Plane
```

A relationship family may request one capability or one documented capability bundle from each selected source. The selected source identity remains intact for freshness, diagnostics, and future presentation.

## Planned typed resolved-target boundary

Block 31 should introduce a Geometry-layer value concept equivalent to:

```text
AssemblyResolvedGeometricTarget
  exact endpoint identity
  source kind
  source model identity metadata
  typed geometry descriptor variant
  available capabilities
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
AssemblyFrameTargetDescriptor
```

Capability projection helpers should be the only supported path from a resolved descriptor to solver geometry:

```text
project_plane(target)
project_axis(target)
project_line(target)
project_point(target)
project_circle(target)
project_cylinder(target)
project_frame(target)
```

A projection fails closed when the source does not expose the requested capability.

Equation builders must not inspect source kind and independently infer compatibility.

# Blocks 31-40 — General target and relationship foundation

## Block 31 — Typed geometric target taxonomy and capability projection

Primary boundary: Geometry-layer derived query semantics.

### Goal

Generalize the current plane/axis/seat resolver outputs into one typed target taxonomy while preserving all existing local and hierarchy target behavior.

### Required work

1. Introduce `AssemblyGeometricTargetSourceKind` as derived source classification.
2. Introduce typed resolved descriptor values for Plane, Axis, Line, Point, Circle/CircularEdge, Cylinder/CylindricalSurface, and Frame targets.
3. Introduce one resolved-target variant/value that retains exact persistent endpoint identity and source metadata.
4. Introduce explicit capability projection helpers.
5. Adapt the current generated planar face, `.axis`, and `.seat` resolver paths to produce the new descriptors.
6. Current `.seat` resolves to a Frame that also projects Axis and Plane capabilities.
7. Keep existing `resolve`, `resolve_axis`, and `resolve_insert` compatibility APIs until downstream builders have migrated.
8. Add equivalent local-space and hierarchy/root-space capability projection rules.
9. Persist no taxonomy/capability cache.

### Stable identity/order

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

Capability order for deterministic diagnostics/tests:

```text
Plane
Axis
Line
Point
Circle
Cylinder
Frame
```

### Failure policy

Fail closed on unsupported semantic source, malformed resolved source geometry, non-finite descriptors, degenerate directions, non-right-handed Plane/Frame descriptors, non-positive Circle/Cylinder radius, and unsupported capability projection.

### Acceptance tag

```text
[geometry][assembly-geometric-target-taxonomy]
```

### Required proofs

- all current six generated planar face tokens resolve as `GeneratedPlanarFace -> Plane`;
- current single-circle `.axis` resolves to Axis;
- current `.seat` resolves to Frame, Axis, and Plane capabilities;
- capability projection is deterministic;
- unsupported projections fail closed;
- local and hierarchy target geometry remain numerically unchanged;
- all existing Mate/Distance/Angle/Concentric/Insert/Revolute tests remain compatible;
- no Project or PartDocument mutation.

### Explicitly deferred

No new semantic source grammar, compatibility matrix, relationship family, joint family, or JSON shape in Block 31.

## Block 32 — Assembly-selectable reference geometry Core intent

Primary boundary: Core persistent model intent and semantic source identity.

### Goal

Make datum/reference geometry deliberately addressable by assembly semantic endpoints without resolving Geometry or changing endpoint JSON shape.

### Existing intent to reuse

BLCAD already has persistent `DatumPlane`, construction lines, and construction points in part model intent.

Block 32 reuses those identities rather than duplicating assembly-specific geometry records.

### New DatumAxis intent

If no first-class `DatumAxis` exists at implementation time, add one to `PartDocument`.

Planned minimal persistent concept:

```text
DatumAxisId
name
axis definition
visibility
```

The canonical Block-32 implementation document must freeze the first supported axis-definition families before code. Candidate initial families are:

```text
explicit origin + direction
construction-line-derived axis
```

Parameter/reference-driven definitions must declare dependency and invalidation edges.

### Semantic target grammar

Block 32 freezes exact parseable semantic-reference spellings for:

```text
DatumPlane selection
DatumAxis selection
ConstructionLine selection
ConstructionPoint selection
```

The grammar must remain unambiguous even though BLCAD typed ids currently require only non-empty strings and may contain dots, slashes, or percent characters.

Do not adopt a delimiter grammar until ambiguity cases are explicitly tested.

Existing feature target spellings remain unchanged.

### Persistence rule

Assembly endpoints continue to persist only their semantic-reference string plus component/occurrence identity.

No resolved plane, axis, line, or point coordinates are copied into constraints or joints.

### Acceptance tags

```text
[core][datum-axis]
[core][assembly-reference-target-intent]
```

### Required proofs

- existing DatumPlane identity can be represented as one assembly semantic source;
- DatumAxis model intent validates if introduced;
- construction line/point identities can be represented without copying geometry into assembly records;
- semantic source grammar is unambiguous for ids containing `.`, `/`, and `%`;
- malformed semantic source tokens fail closed at the Core identity/parser boundary;
- no geometry resolution occurs.

### Explicitly deferred

No JSON/serialization changes and no target geometry resolution in Block 32.

## Block 33 — Reference geometry serialization and structure validation

Primary boundary: Core serialization and compatibility.

### Goal

Roundtrip any new Block-32 PartDocument reference-geometry intent and prove that existing assembly endpoint JSON preserves the new semantic source strings unchanged.

### Required work

1. Add `DatumAxis` PartDocument JSON only if Block 32 introduced persistent DatumAxis records.
2. Preserve missing-field compatibility for historical part files.
3. Preserve existing local and cross-hierarchy endpoint JSON shape.
4. Roundtrip Block-32 semantic-reference strings byte-for-byte.
5. Validate PartDocument ownership/id uniqueness and DatumAxis value-family rules at load.
6. Do not resolve Plane/Axis/Line/Point geometry during JSON load.

### Acceptance tags

```text
[core][datum-axis-json]
[core][assembly-reference-target-json]
```

### Required proofs

- historical files without DatumAxis data remain valid;
- DatumAxis records roundtrip if introduced;
- local constraint/joint endpoint strings roundtrip exactly;
- occurrence-qualified Project endpoint strings roundtrip exactly;
- ids containing delimiters/escape-like bytes retain exact identity;
- malformed persistent DatumAxis intent fails closed;
- loading does not execute OCCT or mutate transforms.

### Explicitly deferred

No Geometry target resolution or relationship compatibility in Block 33.

## Block 34 — Datum, axis, line, and point target resolution

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

The existing `WorkplaneResolver`, construction line resolver, and construction point resolver are reused where applicable.

Do not implement a second workplane or construction-geometry evaluator inside assembly target resolution.

### Local and cross-hierarchy behavior

Local resolution returns component-local descriptors plus direct component transform context.

Hierarchy resolution applies the exact existing chain:

```text
[T_component, T_inner_parent, ..., T_outer_parent]
```

using the established hierarchy transform evaluator.

Points translate and rotate. Axis/Line directions rotate only. Plane frames retain handedness.

### Freshness

Existing exact canonical `PartDocument` model-intent snapshots conservatively protect these target-producing records.

Block 34 proves local and cross-hierarchy results become stale when selected datum/construction source intent changes.

Do not introduce a target-specific revision counter.

### Acceptance tag

```text
[geometry][assembly-reference-target-resolution]
```

### Required proofs

- generated planar face and DatumPlane expose equivalent Plane capability to consumers;
- DatumAxis and a compatible generated circular-feature axis expose equivalent Axis capability;
- ConstructionLine resolves as an oriented Line;
- ConstructionPoint resolves as a Point;
- local and nested cross-hierarchy transform evaluation is correct;
- source edits invalidate old local/cross solve results through existing PartDocument freshness;
- source Project remains unchanged.

### Explicitly deferred

No generated edge/vertex/cylindrical-surface semantic identity in Block 34.

## Block 35 — Stable semantic generated topology identity and recovery

Primary boundary: Core semantic reference model and recovery rules.

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

Every supported feature producer defines its own finite semantic subelement roles from parametric model intent.

Acceptable identity sources include:

```text
feature identity
source sketch/profile identity
profile entity identity
feature direction/side role
pattern instance semantic identity when already stable
named generated face/edge/vertex role
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

### Required support matrix

Before Geometry resolution is added, Block 35 publishes a producer matrix:

```text
Feature producer
  -> supported semantic face roles
  -> supported semantic edge roles
  -> supported semantic vertex roles
  -> expected cardinality
  -> ambiguity/failure behavior
```

The first implementation does not need every BLCAD feature family. It explicitly lists supported producers and rejects all others.

### Pattern identity

The current resolver rejects generic `CircularHolePattern.axis/seat` because multiple holes are ambiguous.

Block 35 defines stable per-instance semantic identity before any patterned cylindrical face/edge/vertex is selectable.

A pattern target identifies a semantic pattern instance from model intent. It does not use transient result-vector position unless that position has first been frozen as persistent instance identity.

### Reference recovery

Reuse existing semantic reference/recovery infrastructure where it can represent the new producer-role identities.

If recovery needs additional source metadata, persist semantic producer-role intent only. Never persist OCCT topology handles.

### Acceptance tags

```text
[core][semantic-generated-topology-reference]
[core][semantic-generated-topology-recovery]
```

### Required proofs

- supported producer roles are unique and deterministic;
- feature insertion order and OCCT traversal order cannot change semantic target identity;
- unsupported/ambiguous producers fail closed;
- patterned instances are not addressable until stable per-instance identity exists;
- JSON roundtrip preserves semantic references;
- reference recovery never writes raw OCCT ids into model intent.

### Explicitly deferred

Block 35 defines identity/recovery only. It adds no Assembly Geometry target resolver branch or equation.

## Block 36 — Generated face, edge, and vertex target resolution

Primary boundary: Geometry semantic target resolution.

### Goal

Resolve Block-35 semantic generated topology references into Block-31 typed descriptors and capabilities.

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

Resolution starts from semantic producer identity and reconstructs or queries current recomputed geometry for that exact semantic subelement.

The source semantic identity remains authoritative. OCCT topology is execution data used only after semantic resolution identifies the intended generated subelement.

A descriptor preserves source metadata sufficient for diagnostics and future presentation:

```text
referenced PartDocumentId
source feature id
source semantic role
optional source sketch/profile/entity identity
resolved source kind
```

### Geometric validation

- Line direction finite and non-degenerate;
- Circle radius finite and positive;
- Circle frame finite and right-handed;
- Cylinder radius finite and positive;
- Cylinder axis finite and non-degenerate;
- Plane frame finite and right-handed;
- Point finite;
- source topology type matches semantic source role.

### Local and hierarchy behavior

The local resolver returns component-local descriptors.

The hierarchy resolver evaluates requested capability through the exact occurrence transform chain. It never transforms or copies OCCT topology as persistent identity.

### Acceptance tag

```text
[geometry][assembly-generated-topology-target-resolution]
```

### Required proofs

- cylindrical face resolves to Cylinder and Axis capabilities;
- circular edge resolves to Circle, Axis, and center Point capabilities;
- linear edge resolves to Line;
- vertex resolves to Point;
- local and cross-hierarchy geometry agree with exact transform semantics;
- repeated rooted occurrences retain distinct world-space geometry while sharing one source PartDocument semantic target;
- unsupported producer roles fail closed;
- no source model mutation.

### Explicitly deferred

Block 36 adds no compatibility rule and no new relationship type.

## Block 37 — Explicit target compatibility matrix

Primary boundary: derived relationship compatibility semantics.

### Goal

Introduce one deterministic compatibility resolver based on Block-31 geometric capabilities before adding any new relationship family.

### Compatibility resolution rule

For every relationship type:

```text
relationship type
  + target A capabilities
  + target B capabilities
  -> one exact ordered capability pair/bundle
  OR explicit incompatibility
```

Compatibility selection is deterministic. It does not choose a different interpretation merely because a source exposes several capabilities unless the relationship's canonical priority rule says so.

### Initial compatibility matrix

#### Mate

```text
Plane <-> Plane
```

#### Distance

```text
Plane <-> Plane
Point <-> Point
Point <-> Plane
Plane <-> Point
```

The canonical sign/direction rule is frozen separately for each ordered pair.

Line/Axis distance is not silently added until exact signed/unsigned semantics are documented.

#### Angle

```text
Plane <-> Plane
Line <-> Line
Axis <-> Axis
Line <-> Axis
Axis <-> Line
```

Directed versus direction-symmetric behavior is explicit per ordered capability pair.

#### Concentric

```text
Axis <-> Axis
```

Axis capability may originate from:

```text
current generated circular feature axis
DatumAxis
GeneratedCylindricalFace
GeneratedCircularEdge
CircularFeatureSeat
```

#### Insert

```text
Frame <-> Frame
```

The existing `.seat` source projects Frame capability.

A later expansion may construct a frame from separate Axis + Plane references, but Block 37 does not infer hidden extra targets.

### Compatibility APIs

Plan one derived query boundary equivalent to:

```text
AssemblyConstraintTargetCompatibilityResolver
  resolve(type, target_a, target_b)
    -> ordered capability projection contract
```

The result remains derived/unpersisted.

Equation builders consume the compatibility result, not semantic source kinds.

### Acceptance tags

```text
[geometry][assembly-target-compatibility]
[geometry][assembly-cross-hierarchy-target-compatibility]
```

### Required proofs

- complete supported/incompatible table tests;
- deterministic interpretation for multi-capability targets;
- DatumPlane-to-generated-face Mate resolves Plane/Plane;
- DatumAxis-to-cylindrical-face Concentric resolves Axis/Axis;
- circular-edge-to-DatumAxis Concentric resolves Axis/Axis;
- current `.seat` Insert resolves Frame/Frame;
- current existing target families retain numeric-equivalent compatibility;
- local and hierarchy compatibility results agree after target resolution;
- incompatibility fails before equation construction;
- no model mutation or persistence change.

### Explicitly deferred

No new relationship enum or equation in Block 37.

## Block 38 — Generic geometric relationship Core intent and JSON

Primary boundary: Core persistent relationship model and serialization.

### Goal

Add the first generic relationship families to local and Project-level persistent intent after target compatibility exists.

### New relationship families

```text
Coincident
Parallel
Perpendicular
```

### Core intent work

1. Extend the shared relationship type enum used by local and Project-level records.
2. Reuse current local target and occurrence-qualified endpoint identity shapes.
3. Define which new families carry no explicit scalar quantity.
4. Reuse active/inactive state semantics.
5. Preserve target A/B order.
6. Validate family/value rules fail-closed.
7. Adding relationship intent does not resolve geometry or mutate transforms.

### JSON work

Add lower-case spellings:

```text
coincident
parallel
perpendicular
```

Existing local and `cross_hierarchy_constraints[]` JSON shapes remain unchanged apart from accepted type values.

Historical files remain compatible.

### Acceptance tags

```text
[core][assembly-generic-relationship-intent]
[core][assembly-generic-relationship-json]
```

### Required proofs

- local Coincident/Parallel/Perpendicular intent;
- Project-level occurrence-qualified intent;
- independent local/project id scopes remain unchanged;
- target A/B order preserved;
- state preserved;
- no explicit Distance/Angle values allowed for the new families;
- JSON roundtrip for all new types;
- historical five-family files unchanged;
- adding/loading intent performs no geometry resolution or transform mutation.

### Explicitly deferred

No residual/equation or graph participation changes in Block 38.

## Block 39 — Generic geometric relationship equations and shared solve integration

Primary boundary: Geometry equation semantics plus existing numeric/diagnostic execution.

### Goal

Implement Coincident, Parallel, and Perpendicular through Block-37 compatibility results and integrate them into all existing local/cross-hierarchy solve/application/diagnostic paths.

### Coincident capability pairs

Initial pairs:

```text
Point <-> Point
Point <-> Line
Line <-> Point
Point <-> Plane
Plane <-> Point
```

Canonical residual seeds to freeze in the Block-39 implementation document:

```text
Point-Point
  pB - pA

Point-Line
  cross(point - line.origin, line.direction)

Point-Plane
  dot(point - plane.origin, plane.normal)
```

The ordered Line-Point and Plane-Point forms reuse the same geometric distance semantics with explicit target-order diagnostics.

### Parallel capability pairs

```text
Line <-> Line
Axis <-> Axis
Line <-> Axis
Axis <-> Line
Plane <-> Plane
```

Candidate residual:

```text
cross(dA, dB)
```

For Plane targets, `d` is the normal.

The first seed accepts parallel and anti-parallel directions.

### Perpendicular capability pairs

Same source capability pairs as Parallel.

Candidate residual:

```text
dot(dA, dB)
```

For Plane targets, `d` is the normal.

### Shared integration requirements

The new families enter:

```text
AssemblyConstraintGraph participation
AssemblyCrossHierarchyConstraintGraph participation
local deterministic relationship order
cross-hierarchy deterministic relationship order
shared residual flattening/scaling
central finite differences
Gauss-Newton solve
fresh relationship snapshots
atomic application
local/cross Jacobian-rank diagnostics
```

Do not create a generic-target solver.

### Acceptance tags

```text
[geometry][assembly-generic-relationships]
[geometry][assembly-generic-relationships-solver]
[geometry][assembly-generic-relationships-cross-hierarchy]
[geometry][assembly-generic-relationships-diagnostics]
```

### Required proofs

- point-point Coincident;
- point-line Coincident;
- point-plane Coincident;
- Parallel and Perpendicular for Line/Axis pairs;
- Parallel and Perpendicular for Plane pairs;
- line-line Angle through Block-37 compatibility;
- DatumPlane-to-generated-face Mate;
- DatumAxis/cylindrical-face/circular-edge/current `.axis` Concentric all use the same Axis equation semantics;
- local and cross-hierarchy solving;
- repeated occurrence shared-authority behavior unchanged;
- target-producing part edits stale old results;
- rank diagnostics use actual Jacobian rank rather than family-name DOF tables;
- existing five-family solve tests remain unchanged.

### Explicitly deferred

No Tangent relationship, contact relationship, or motion-joint family in Block 39.

## Block 40 — Joint target compatibility and oriented Frame contract

Primary boundary: derived joint compatibility semantics.

### Goal

Make current Revolute target compatibility consume the Block-31 capability taxonomy and freeze the oriented-frame rule required before Axis-only motion joints can exist.

### Current Revolute compatibility

```text
Frame <-> Frame
```

Current `.seat` sources project Frame capability and remain fully supported.

### Why Axis alone is insufficient for current Revolute motion

The existing signed twist residual needs:

```text
axis direction
reference x direction
```

An Axis provides only origin + direction.

Therefore this is invalid as an implicit rule:

```text
Axis <-> Axis
  -> choose arbitrary world axis as twist reference
```

That would make motion coordinate meaning depend on Geometry implementation accidents.

### Required decision

Block 40 must freeze one explicit semantic oriented-frame strategy before Axis-only Revolute support.

Candidate strategies:

```text
A. joint record carries additional reference-direction endpoints for A and B
B. semantic target source projects a full Frame capability
C. Axis-only Revolute remains incompatible until a Frame source exists
```

Preferred policy:

```text
Frame capability is the first-class motion orientation contract.
```

Datum/reference or generated geometry may later expose Frame only when model intent defines orientation deterministically.

Do not synthesize Frame from Axis alone.

### Planned API

A derived boundary equivalent to:

```text
AssemblyJointTargetCompatibilityResolver
  resolve(joint_type, target_a, target_b)
    -> ordered capability projection contract
```

The first supported rule remains:

```text
Revolute -> Frame <-> Frame
```

### Acceptance tags

```text
[geometry][assembly-joint-target-compatibility]
[geometry][assembly-cross-hierarchy-joint-target-compatibility]
```

### Required proofs

- current `.seat` local and cross-hierarchy Revolute resolve Frame/Frame;
- unsupported Axis-only Revolute fails explicitly rather than choosing a hidden reference direction;
- equivalent future Frame-capable sources reach the same Revolute residual constructor;
- local/cross compatibility results agree;
- no joint persistence change in Block 40;
- existing Revolute motion tests remain numerically unchanged.

### Explicitly deferred

No new joint family or coordinate schema in Block 40.

# Blocks 41-47 — Multi-coordinate motion foundation and richer joint families

## Block 41 — General joint coordinate/limit Core model

Primary boundary: Core persistent joint state.

### Goal

Generalize the current single scalar Revolute coordinate/limit representation so richer joint families can own explicit one- or multi-coordinate state without hidden fields.

### Required coordinate model

The canonical implementation document must freeze a typed joint coordinate/value model equivalent in capability to:

```text
JointCoordinateKind
  Angular
  Linear

JointCoordinateSlot
  semantic coordinate role
  kind
  authored value
  optional lower limit
  optional upper limit
```

Coordinate role identity must be stable and family-defined.

Representative planned roles:

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

Ball/Spherical orientation representation is not frozen until its family block.

### Backward compatibility

Current Revolute public APIs and JSON semantics must remain supported through an explicit compatibility/adaptation boundary.

Do not silently reinterpret degrees as generic doubles or mix linear/angular units.

### Local and Project-level parity

The generalized coordinate contract applies to:

```text
AssemblyJoint
AssemblyHierarchyJoint
```

without merging their target identity scopes.

### Acceptance tags

```text
[core][assembly-joint-coordinate-model]
[core][assembly-cross-hierarchy-joint-coordinate-model]
```

### Explicitly deferred

No JSON change or vector motion solver in Block 41.

## Block 42 — General joint coordinate JSON and compatibility

Primary boundary: Core serialization.

### Goal

Serialize the Block-41 coordinate-slot model additively while preserving historical Revolute project compatibility.

### Requirements

- define canonical coordinate role spellings;
- retain explicit units per coordinate kind;
- preserve deterministic family-defined slot order;
- load historical Revolute `limits` + scalar `coordinate` representation;
- choose/document whether writers continue emitting legacy Revolute shape or emit the generalized representation after migration;
- fail closed on duplicate/missing/unknown coordinate roles;
- validate limits and authored values by unit/family;
- apply equally to local and `cross_hierarchy_joints[]` records.

### Acceptance tags

```text
[core][assembly-joint-coordinate-json]
[core][assembly-cross-hierarchy-joint-coordinate-json]
```

### Explicitly deferred

No motion execution changes in Block 42.

## Block 43 — Vector joint drives, holding semantics, freshness, and atomic application

Primary boundary: Geometry motion execution/result/application.

### Goal

Generalize the current selected-joint scalar drive boundary to deterministic family-defined coordinate drive vectors.

### Planned drive concept

```text
AssemblyJointDrive
  selected joint identity
  requested coordinate values by stable coordinate role
```

The drive must exactly match the selected joint family's drivable coordinate roles.

### Holding semantics

Every non-selected active joint in the same combined motion closure receives holding drives for all authored coordinate slots.

The selected joint receives requested values for explicitly driven roles and must define policy for any undriven coordinate roles.

Preferred first policy:

```text
undriven selected-joint roles hold authored values
```

### Result/freshness

Motion result snapshots protect complete coordinate-slot definitions, authored values, limits, and selected drive values.

Atomic application writes:

```text
one direct component transform per free authority proposal
+
selected joint authored coordinate values for exactly driven roles
```

No non-selected joint authored coordinate changes.

### Numeric reuse

Authority variables remain six direct component transform variables per unique free authority.

Joint coordinate values are drive parameters, not additional transform-authority variables.

The existing shared finite-difference and Gauss-Newton engine remains authoritative.

### Acceptance tags

```text
[geometry][assembly-vector-joint-drive]
[geometry][assembly-cross-hierarchy-vector-joint-drive]
[geometry][assembly-vector-joint-drive-application]
```

### Explicitly deferred

No new joint family in Block 43.

## Block 44 — Prismatic joint family

Primary boundary: one new joint family across Core intent, compatibility, equations, and shared vector-drive execution.

### Planned capability requirement

The canonical family document must freeze one oriented linear-motion frame requirement.

A bare Line/Axis may define translation direction but not necessarily all orientation constraints required by the intended Prismatic joint semantics.

Preferred first target contract:

```text
Frame <-> Frame
```

with one `translation` linear coordinate along the selected frame axis.

If a reduced Axis/Line contract is desired, its remaining orientation constraints must be explicit before implementation.

### Required family work

- add `Prismatic` persistent local/Project type;
- coordinate role `translation` with length units/limits;
- JSON spelling;
- compatibility rule;
- residual component order;
- local and root-space equation builders using shared target capabilities;
- motion graph participation;
- vector-drive support;
- freshness and atomic application;
- local/cross tests.

### Acceptance tags

```text
[core][assembly-prismatic-joint]
[core][assembly-cross-hierarchy-prismatic-joint]
[geometry][assembly-prismatic-joint]
[geometry][assembly-cross-hierarchy-prismatic-motion]
```

## Block 45 — Cylindrical joint family

Primary boundary: one new two-coordinate joint family.

### Planned target capability

```text
Frame <-> Frame
```

or another explicitly oriented cylindrical frame capability frozen before implementation.

### Planned coordinates

```text
translation : LengthMm
rotation    : AngleDeg
```

### Required proof

Translation and twist drives must coexist without introducing a second optimizer or duplicate transform variables.

Holding drives protect both authored coordinates on non-selected Cylindrical joints.

### Acceptance tags

```text
[core][assembly-cylindrical-joint]
[core][assembly-cross-hierarchy-cylindrical-joint]
[geometry][assembly-cylindrical-joint]
[geometry][assembly-cross-hierarchy-cylindrical-motion]
```

## Block 46 — Planar joint family

Primary boundary: one new three-coordinate joint family.

### Required target capability

```text
Frame <-> Frame
```

### Planned coordinates

```text
translation_u      : LengthMm
translation_v      : LengthMm
rotation_normal    : AngleDeg
```

The Frame defines U/V directions and oriented normal.

### Required proof

The family must preserve plane coincidence/orientation while allowing the three planned in-plane coordinates through the shared motion execution boundary.

### Acceptance tags

```text
[core][assembly-planar-joint]
[core][assembly-cross-hierarchy-planar-joint]
[geometry][assembly-planar-joint]
[geometry][assembly-cross-hierarchy-planar-motion]
```

## Block 47 — Ball/Spherical joint family

Primary boundary: one new spherical joint family.

### Required target capability

```text
Point <-> Point
```

for center coincidence.

### Orientation/motion boundary

A passive Ball/Spherical relationship can enforce center coincidence while leaving three rotational DOF.

A user-driven spherical orientation needs a stable orientation coordinate representation. Do not encode that as three arbitrary Euler angles without freezing singularity/order semantics.

Block 47 must explicitly choose one of:

```text
A. passive spherical joint only in the first implementation
B. quaternion orientation drive with normalized persistent/motion semantics
C. another documented singularity-aware orientation parameterization
```

Preferred first seed:

```text
passive Ball/Spherical joint
```

with no selected motion drive until a general orientation-drive contract is planned.

### Required family work

- add `Ball` or `Spherical` persistent type with one canonical spelling;
- Point/Point compatibility;
- center-coincidence residual;
- local/cross motion-closure participation;
- freshness/application semantics;
- explicit rejection as a selected driven joint if the first seed is passive-only.

### Acceptance tags

```text
[core][assembly-spherical-joint]
[core][assembly-cross-hierarchy-spherical-joint]
[geometry][assembly-spherical-joint]
[geometry][assembly-cross-hierarchy-spherical-joint]
```

## Generic relationship compatibility matrix summary

After Block 39, the first target compatibility set should be:

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

This table is a planned initial compatibility boundary, not a claim that all combinations are currently implemented.

## GUI and selection presentation boundary

An Inventor-/SolidWorks-like picker is not part of Blocks 31-47 because BLCAD has no GUI yet.

When GUI work starts, the picker should query:

```text
persistent semantic source candidates
-> resolved source kind
-> available geometric capabilities
-> currently selected relationship/joint type
-> compatibility resolver
```

The UI may highlight faces, edges, vertices, datum planes, axes, lines, and points through OCCT presentation objects, but committing a selection stores the corresponding BLCAD semantic reference identity.

The UI never persists the picked `TopoDS_Shape` or selection index as the assembly endpoint.

## Planned implementation order

After Block 30:

```text
31 typed geometric target taxonomy and capability projection
  -> 32 assembly-selectable reference geometry Core intent
  -> 33 reference geometry serialization and structure validation
  -> 34 datum/axis/line/point target resolution
  -> 35 stable semantic generated topology identity/recovery
  -> 36 generated face/edge/vertex target resolution
  -> 37 explicit target compatibility matrix
  -> 38 generic geometric relationship Core intent + JSON
  -> 39 generic geometric relationship equations + shared solve integration
  -> 40 joint target compatibility + oriented Frame contract
  -> 41 general joint coordinate/limit Core model
  -> 42 general joint coordinate JSON/backward compatibility
  -> 43 vector joint drives + holding/freshness/atomic application
  -> 44 Prismatic joint
  -> 45 Cylindrical joint
  -> 46 Planar joint
  -> 47 Ball/Spherical joint
```

Do not merge these blocks into one feature PR.

The sequence establishes identity before geometry, geometry before compatibility, compatibility before relationship intent, intent before equations, joint compatibility before coordinate-schema generalization, and shared multi-coordinate drive semantics before richer joint families.

## Immediate next technical step

Block 30 remains the current next technical step.

After Block 30, implement Block 31 only: typed derived geometric target taxonomy and capability projection while preserving all existing target strings and relationship behavior.

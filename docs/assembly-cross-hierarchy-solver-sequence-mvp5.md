# Cross-Hierarchy Assembly Relationship Sequence MVP-5

Status: Blocks 23-29 are implemented. Block 30 is the current next technical step. Blocks 31-47 are explicitly planned as the general assembly geometric-target, relationship, and richer-joint sequence.

This document is the canonical numbered implementation sequence for assembly relationships, motion, hierarchy-aware exchange, posed analysis, and the handoff into general Inventor-/SolidWorks-like semantic target compatibility.

Implemented contracts are canonical in:

- `docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`
- `docs/assembly-cross-hierarchy-constraint-json-mvp5.md`
- `docs/assembly-cross-hierarchy-incidence-groups-mvp5.md`
- `docs/assembly-cross-hierarchy-numeric-solver-mvp5.md`
- `docs/assembly-cross-hierarchy-application-diagnostics-mvp5.md`
- `docs/assembly-cross-hierarchy-revolute-motion-mvp5.md`
- `docs/assembly-structured-step-products-mvp5.md`
- `docs/file-format.md`

Planned general target/joint expansion is canonical in:

- `docs/assembly-general-geometric-target-roadmap.md`

## Sequencing rule

Cross-document assembly work crosses separate authority boundaries:

```text
Core persistent model intent
  -> serialization and structure compatibility
  -> semantic source identity
  -> derived occurrence/authority connectivity
  -> typed target geometry and capability projection
  -> explicit relationship/joint compatibility
  -> persistent relationship/joint state
  -> local or root-space equation evaluation
  -> shared numeric residual/Jacobian execution
  -> derived solve/motion results and proposals
  -> complete modeled-input freshness
  -> atomic authority-qualified application
  -> deterministic occurrence-aware exchange
  -> posed analysis consumers
```

Do not:

- persist Geometry query/execution types;
- persist raw OCCT topology as semantic target identity;
- duplicate one shared child component transform into independent variables per rooted occurrence;
- duplicate local model-definition relationships per rooted occurrence;
- add residual rows merely to encode shared transform authority;
- persist graph, residual, Jacobian, freshness snapshot, proposal, diagnostic, XDE label, or STEP entity caches;
- mutate `SubassemblyInstance::transform()` through component authority variables;
- introduce a second optimizer, finite-difference contract, hierarchy traversal, or transform convention;
- add one-off feature-specific branches directly to generic equation builders once the target-capability layer exists;
- infer a joint twist reference direction from an arbitrary world axis;
- add multi-coordinate joint execution before coordinate state and JSON contracts exist;
- merge several richer joint families into one implementation block.

## Frozen solve/motion identities

### Occurrence-qualified semantic endpoint

```text
AssemblyHierarchyConstraintEndpoint =
  (occurrence_path,
   local ComponentInstanceId,
   semantic_reference)
```

The empty path addresses the explicit root occurrence. Non-empty paths are exact ordered root-to-current `SubassemblyInstanceId` sequences.

### Geometric component occurrence

```text
HierarchyComponentOccurrence =
  (occurrence_path,
   local ComponentInstanceId)
```

### Direct component transform authority

```text
ComponentTransformAuthority =
  (assembly_document: DocumentId,
   local ComponentInstanceId)
```

Repeated rooted occurrences can remain different geometric/motion contexts while mapping to one shared child-document transform authority.

## Frozen Block-29 exchange identities

### Assembly occurrence

```text
AssemblyExchangeAssemblyOccurrenceIdentity =
  exact rooted SubassemblyInstance occurrence path
```

### Part component occurrence

```text
AssemblyExchangeComponentOccurrenceIdentity =
  (containing assembly occurrence path,
   local ComponentInstanceId)
```

### Part product definition

```text
AssemblyExchangePartProductDefinitionIdentity =
  referenced PartDocumentId
```

These exchange identities are derived and unpersisted. They are not aliases for OCCT topology ids, TDF label tags, or STEP entity ids.

## Blocks 23-27: cross-hierarchy geometric solve chain — implemented

### 23. Core endpoint contract and persistent Project-level intent

Canonical document: `docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`.

Implemented Core-owned occurrence-qualified endpoint identity and persistent Project-owned Mate/Concentric/Distance/Insert/Angle relationships.

### 24. Additive JSON and exact structure validation

Canonical document: `docs/assembly-cross-hierarchy-constraint-json-mvp5.md`.

Implemented additive:

```text
cross_hierarchy_constraints[]
```

with exact target/path order and exact root-to-reached-component structure validation after ordinary hierarchy validation.

### 25. Relationship-to-authority incidence and solve groups

Canonical document: `docs/assembly-cross-hierarchy-incidence-groups-mvp5.md`.

Implemented deterministic active local/Project relationship incidence over unique `ComponentTransformAuthority` values. Local relationships are collected once per containing `AssemblyDocument`; repeated rooted endpoints retain exact occurrence context while mapping to shared authorities.

### 26. Shared numeric residual/Jacobian integration

Canonical document: `docs/assembly-cross-hierarchy-numeric-solver-mvp5.md`.

Implemented one six-variable block per unique free active authority:

```text
tx_mm ty_mm tz_mm rx_deg ry_deg rz_deg
```

Local relationships evaluate once in document-local space. Project-level relationships evaluate through exact rooted parent chains in root-assembly space. Existing residual flattening, central finite differences, damping/backtracking, and Gauss-Newton machinery are reused.

### 27. Complete freshness, atomic application, and diagnostics

Canonical document: `docs/assembly-cross-hierarchy-application-diagnostics-mvp5.md`.

Implemented complete authority/relationship/path-boundary/semantic-PartDocument freshness, exact solve-group revalidation, atomic direct-authority transform application, and Jacobian-rank/remaining-DOF diagnostics.

Semantic target PartDocument freshness uses exact canonical:

```text
serialize_part_document_to_json(part)
```

payload comparison.

Focused tags:

```text
[geometry][assembly-cross-hierarchy-solver]
[geometry][assembly-cross-hierarchy-application]
[geometry][assembly-cross-hierarchy-diagnostics]
[geometry][assembly-semantic-freshness]
```

## Block 28: occurrence-qualified Revolute motion — implemented

Canonical document: `docs/assembly-cross-hierarchy-revolute-motion-mvp5.md`.

Implemented:

- persistent Project-level `AssemblyHierarchyJoint` Revolute intent;
- additive `cross_hierarchy_joints[]` JSON;
- exact occurrence-qualified `.seat` endpoints;
- combined motion closure over local geometry, local joints, Project cross geometry, and Project cross joints;
- exact endpoint-to-transform-authority mapping;
- two endpoint mappings but one unique incidence for different rooted endpoints sharing one authority;
- one shared local/cross Revolute residual constructor;
- exact root-space parent-chain target evaluation;
- authored-coordinate holding drives for non-selected active Revolute joints;
- shared authority variable order, finite differences, and numeric engine;
- complete four-family relationship snapshots;
- shared Block-27 authority/boundary/PartDocument freshness helpers;
- atomic authority transforms plus selected Project-level joint coordinate application.

Focused tags:

```text
[core][assembly-cross-hierarchy-joint]
[core][assembly-cross-hierarchy-joint-json]
[core][assembly-cross-hierarchy-motion-graph]
[geometry][assembly-cross-hierarchy-revolute]
[geometry][assembly-cross-hierarchy-motion]
```

## Block 29: component geometry instancing and structured STEP assembly products — implemented

Canonical document: `docs/assembly-structured-step-products-mvp5.md`.

Block 29 adds no persistent record and no JSON field.

Implemented derived identities:

```text
assembly occurrence
  = exact rooted SubassemblyInstance path

component occurrence
  = (containing rooted path, local ComponentInstanceId)

part product definition
  = PartDocumentId
```

`AssemblyExchangeGraph` reuses:

```text
AssemblyHierarchyTraversal
AssemblyLeafOccurrenceResolver
```

The export set is the explicit root plus every rooted assembly path prefix required by one visible-active leaf.

Ordering:

```text
assembly occurrence:
  lexicographic exact SubassemblyInstanceId path sequence

component occurrence:
  containing path
  then local ComponentInstanceId

part product definition:
  PartDocumentId
```

`AssemblyPartShapeDefinitionBuilder` performs one recompute and one private `ShapeCache` per unique exported `PartDocumentId`.

The flattened posed-leaf path and structured STEP path reuse one OCCT rigid-transform conversion contract.

`AssemblyStructuredStepExporter` builds:

```text
one XDE part definition label per PartDocumentId
one XDE assembly label per rooted assembly occurrence
part component references to shared definitions
parent -> child assembly occurrence references
```

The explicit root label is transferred through `STEPCAFControl_Writer` with names enabled. The existing `AssemblyStepExporter` remains a flattened compound compatibility consumer.

Focused tags:

```text
[core][assembly-exchange-graph]
[geometry][assembly-structured-step-export]
```

## Block 30: richer collision/contact and swept-motion analysis — Next

Goal: extend the existing posed leaf/interference/clearance analysis boundary using the frozen rooted occurrence identities and current motion result/application contracts.

Do not implement a general physics engine.

Required planning boundary before implementation:

1. define persistent authority versus derived contact/sweep products;
2. define stable unordered or directed occurrence-pair identity using exact rooted component occurrence identity;
3. freeze static contact classification semantics beyond current positive-volume interference and scalar minimum clearance;
4. define a deterministic swept-motion query input over one selected supported Revolute motion interval;
5. state whether the sweep samples authored/current Project copies or transient solver results and how source immutability is guaranteed;
6. freeze deterministic sample ordering and failure behavior;
7. reuse `AssemblyLeafOccurrenceResolver`, shared part definitions/posed geometry, and current motion solver/application boundaries instead of introducing a second pose model;
8. define focused acceptance tags and one precise next handoff.

The first static classifications to freeze are:

```text
Separated
Touching
Interfering
```

Any tolerance used to distinguish touching from separated must be explicit and validated.

A first swept-motion seed remains deterministic and bounded. It may sample one requested Revolute coordinate interval at explicit caller-provided or validated fixed resolution; it must not claim continuous collision detection unless an actual continuous algorithm is implemented.

The result should identify exact rooted component occurrence pairs and the motion coordinate/sample at which a classification or clearance event was observed.

## Blocks 31-47: general assembly target, relationship, and joint sequence — Planned

Canonical roadmap: `docs/assembly-general-geometric-target-roadmap.md`.

These blocks address the gap between the current generated plane/axis/seat target layer and a mature CAD assembly target system.

The mandatory order is:

```text
31 typed geometric target taxonomy and capability projection
  -> 32 assembly-selectable reference geometry Core intent
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

Do not merge these blocks into one feature block.

### 31. Typed geometric target taxonomy and capability projection

Primary boundary: Geometry-derived query semantics.

Separate selected semantic source kind from solver geometric capability.

Planned source kinds:

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

Planned capabilities:

```text
Plane
Axis
Line
Point
Circle
Cylinder
Frame
```

Representative projections:

```text
GeneratedCylindricalFace -> Cylinder + Axis
GeneratedCircularEdge -> Circle + Axis + Point(center)
DatumPlane -> Plane
DatumAxis -> Axis + Line
CircularFeatureSeat -> Frame + Axis + Plane
```

Existing generated face, `.axis`, and `.seat` behavior migrates without changing persistent target strings or numeric semantics.

No new source grammar, compatibility matrix, relationship family, joint family, or JSON shape in Block 31.

### 32. Assembly-selectable reference geometry Core intent

Primary boundary: Core persistent model intent and semantic source identity.

Reuse existing `DatumPlane`, construction-line, and construction-point identities as assembly-selectable semantic sources.

If no first-class `DatumAxis` exists, add one with an explicitly frozen initial definition family, validation, dependency/invalidation semantics, and PartDocument ownership.

Freeze exact ambiguity-tested semantic-reference spellings for DatumPlane, DatumAxis, ConstructionLine, and ConstructionPoint.

BLCAD ids are arbitrary non-empty strings; parser design may not assume dots, slashes, or percent characters are unavailable.

No JSON or Geometry resolution in Block 32.

### 33. Reference geometry serialization and structure validation

Primary boundary: Core serialization/compatibility.

Serialize any new DatumAxis PartDocument intent additively, preserve historical files, and prove all local/cross-hierarchy semantic endpoint strings roundtrip byte-for-byte.

No Plane/Axis/Line/Point geometry is resolved during load.

### 34. Datum, axis, line, and point target resolution

Primary boundary: Geometry semantic target resolution.

Resolve:

```text
DatumPlane -> Plane
DatumAxis -> Axis + Line
ConstructionLine -> Line
ConstructionPoint -> Point
```

Reuse existing workplane/construction-geometry resolvers.

Local resolution remains component-local. Cross-hierarchy resolution applies the exact component plus parent transform chain. Existing canonical PartDocument snapshots remain freshness authority.

### 35. Stable semantic generated topology identity and recovery

Primary boundary: Core semantic reference/recovery.

Define producer-driven semantic identities for:

```text
GeneratedCylindricalFace
GeneratedLinearEdge
GeneratedCircularEdge
GeneratedVertex
```

Each supported feature producer publishes a finite matrix of semantic face/edge/vertex roles, expected cardinality, and ambiguity behavior.

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

### 36. Generated face, edge, and vertex target resolution

Primary boundary: Geometry semantic target resolution.

Resolve Block-35 semantic sources into:

```text
GeneratedPlanarFace -> Plane
GeneratedCylindricalFace -> Cylinder + Axis
GeneratedLinearEdge -> Line
GeneratedCircularEdge -> Circle + Axis + Point(center)
GeneratedVertex -> Point
```

Semantic producer identity remains authoritative. OCCT topology is execution data only after the intended semantic subelement is known.

### 37. Explicit target compatibility matrix

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

No new relationship enum/equation in Block 37.

### 38. Generic geometric relationship Core intent and JSON

Primary boundary: Core persistent relationship model and serialization.

Add:

```text
Coincident
Parallel
Perpendicular
```

for local and Project-level relationship intent.

Preserve existing endpoint JSON shapes, target A/B order, state semantics, independent id scopes, and historical five-family compatibility.

No residual/equation integration in Block 38.

### 39. Generic relationship equations and shared solve integration

Primary boundary: Geometry relationship semantics and existing numeric/diagnostic execution.

Initial Coincident pairs:

```text
Point <-> Point
Point <-> Line
Line <-> Point
Point <-> Plane
Plane <-> Point
```

Parallel and Perpendicular initially support Line/Axis direction pairs and Plane normal pairs.

All new families reuse existing local/cross graphs, `ComponentTransformAuthority`, shared residual scaling, central finite differences, Gauss-Newton solving, freshness, atomic application, and Jacobian-rank diagnostics.

No generic-target-specific solver is allowed.

### 40. Joint target compatibility and oriented Frame contract

Primary boundary: derived joint compatibility semantics.

Current Revolute compatibility remains:

```text
Frame <-> Frame
```

Current `.seat` sources project Frame.

Axis alone is insufficient for signed twist because it has no reference X direction. Axis-only Revolute remains incompatible until semantic model intent supplies a deterministic oriented Frame.

Geometry may not choose an arbitrary world axis.

No joint persistence change or new joint family in Block 40.

### 41. General joint coordinate/limit Core model

Primary boundary: Core persistent joint state.

Generalize the current single scalar Revolute coordinate/limit representation into typed family-defined coordinate slots with stable roles and explicit Angular/Linear kinds.

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

No JSON or vector drive execution in Block 41.

### 42. General joint coordinate JSON and backward compatibility

Primary boundary: Core serialization.

Serialize coordinate slots with stable role spellings, units, deterministic family-defined order, values, and optional limits.

Historical scalar Revolute files remain loadable. Duplicate/missing/unknown coordinate roles fail closed.

No motion execution changes in Block 42.

### 43. Vector joint drives, holding semantics, freshness, and atomic application

Primary boundary: Geometry motion result/execution/application.

Generalize selected-joint scalar drives into family-defined coordinate drive vectors.

Non-selected active joints hold all authored coordinate slots. Undriven roles of the selected joint initially hold authored values.

Motion snapshots protect coordinate-slot definitions, authored values, limits, and selected drive values.

Atomic application writes authority transform proposals plus only the selected joint coordinate roles explicitly driven by the result.

Authority variables remain six direct component-transform variables per unique free authority. Joint coordinates remain drive parameters, not transform-authority variables.

No new joint family in Block 43.

### 44. Prismatic joint

One new family in one block.

Preferred first capability contract:

```text
Frame <-> Frame
```

Coordinate:

```text
translation : LengthMm
```

The family document freezes persistent type/JSON, translation limits, residual order, holding-drive semantics, local/cross motion integration, freshness, and atomic application.

### 45. Cylindrical joint

One new two-coordinate family.

Planned coordinates:

```text
translation : LengthMm
rotation    : AngleDeg
```

The family must prove translation and twist drives coexist through the shared vector-drive/numeric path without duplicate transform variables.

### 46. Planar joint

One new three-coordinate family.

Required target capability:

```text
Frame <-> Frame
```

Planned coordinates:

```text
translation_u   : LengthMm
translation_v   : LengthMm
rotation_normal : AngleDeg
```

The Frame defines U/V and oriented normal.

### 47. Ball/Spherical joint

One new spherical family.

Required target compatibility:

```text
Point <-> Point
```

Preferred first seed is passive center coincidence with three free rotational DOF.

A driven spherical orientation is not represented by arbitrary Euler angles without a separately frozen singularity/order contract. If the first seed remains passive-only, selecting it as a driven joint fails explicitly.

## GUI selection boundary

Blocks 31-47 remain headless model/query/equation/motion work.

A future Inventor-/SolidWorks-like picker consumes:

```text
semantic source candidate
-> resolved source kind
-> available capabilities
-> selected relationship/joint type
-> compatibility resolver
```

The UI may highlight OCCT faces, edges, vertices, datum planes, axes, lines, and points, but committing a selection persists BLCAD semantic reference identity. Raw `TopoDS_Shape` or selection index never becomes the assembly endpoint.

## Persistence rule

Persist model intent.

Current persistent assembly intent includes:

```text
component placement/state
local geometric constraints
local Revolute joint/limit/coordinate records
Project-owned child assembly documents
rigid SubassemblyInstance placement/state
Project-level cross-hierarchy geometric constraints
Project-level occurrence-qualified cross-hierarchy Revolute joints
```

Project JSON fields include:

```text
cross_hierarchy_constraints[]
cross_hierarchy_joints[]
```

The planned target expansion continues to persist semantic endpoint identity. Resolved source classification, Plane/Axis/Line/Point/Circle/Cylinder/Frame capabilities, transformed target geometry, and compatibility projections remain derived.

New generic relationship type intent becomes persistent in Block 38. Generalized joint coordinate slots become persistent in Block 41 and serialized in Block 42.

Regenerate connectivity, hierarchy traversal, parent chains, leaves, target geometry/capabilities, compatibility projections, transform authorities, solve/motion groups, holding drives, residuals/Jacobians, solve/motion results, freshness snapshots, proposals, diagnostics, exchange identities/graphs/names, part shape definitions, XDE labels/component references, STEP entities, posed shapes, and analysis products.

## Next technical step

Implement Block 30 only: freeze richer posed contact classification and a bounded deterministic swept-Revolute analysis contract over exact rooted component occurrence identities.

After Block 30, implement Block 31 only from `docs/assembly-general-geometric-target-roadmap.md`: typed derived geometric target taxonomy and explicit capability projection while preserving all existing semantic target strings and relationship behavior.

# Cross-Hierarchy Assembly Relationship Sequence MVP-5

Status: Blocks 23-29 are implemented. Block 30 is the current next technical step. Blocks 31-37 are now explicitly planned as the general assembly geometric-target and relationship-expansion sequence.

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

Planned general target expansion is canonical in:

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
- infer a joint twist reference direction from an arbitrary world axis.

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

Generated exchange names remain derived deterministic metadata and percent-encode authored id bytes that would otherwise collide with path separators/escape syntax.

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

## Blocks 31-37: general assembly geometric targets and relationship compatibility — Planned

Canonical roadmap: `docs/assembly-general-geometric-target-roadmap.md`.

These blocks address the current gap between BLCAD's narrow generated plane/axis/seat target families and an Inventor-/SolidWorks-like assembly target system.

The mandatory order is:

```text
31 typed target taxonomy/capabilities
  -> 32 persistent selectable reference-geometry intent
  -> 33 reference-geometry resolution
  -> 34 stable semantic generated-topology identity
  -> 35 generated topology resolution
  -> 36 relationship compatibility + generic relationships
  -> 37 joint capability expansion
```

Do not merge these into one feature block.

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
SeatFrame
```

Representative projections:

```text
GeneratedCylindricalFace -> Cylinder + Axis
GeneratedCircularEdge -> Circle + Axis + Point(center)
DatumPlane -> Plane
DatumAxis -> Axis + Line
CircularFeatureSeat -> SeatFrame + Axis + Plane
```

Existing generated face, `.axis`, and `.seat` target behavior must migrate without changing persistent target strings or numeric semantics.

No new semantic target grammar or relationship family in Block 31.

Focused tag:

```text
[geometry][assembly-geometric-target-taxonomy]
```

### 32. Assembly-selectable datum/reference geometry intent and serialization

Primary boundary: Core persistent intent/JSON.

Reuse existing `DatumPlane`, construction-line, and construction-point identities as assembly-selectable semantic sources.

If no first-class `DatumAxis` exists at implementation time, add one with explicit ownership, validation, dependency/invalidation semantics, and JSON roundtrip.

Freeze exact unambiguous semantic-reference spellings for:

```text
DatumPlane
DatumAxis
ConstructionLine
ConstructionPoint
```

BLCAD ids are arbitrary non-empty strings. The semantic-reference grammar must therefore be ambiguity-tested rather than assuming dots, slashes, or percent characters are unavailable.

Assembly endpoints continue to persist semantic source identity only. No resolved coordinates are serialized.

Focused tags:

```text
[core][datum-axis]
[core][assembly-reference-target-intent]
[core][assembly-reference-target-json]
```

### 33. Datum, axis, line, and point target resolution

Primary boundary: Geometry semantic target resolution.

Resolve:

```text
DatumPlane -> Plane
DatumAxis -> Axis + Line
ConstructionLine -> Line
ConstructionPoint -> Point
```

Reuse existing workplane/construction-geometry resolvers. Do not implement a second evaluator inside assembly target resolution.

Local resolution remains component-local. Cross-hierarchy resolution applies the exact existing component plus parent transform chain.

Existing exact canonical `PartDocument` snapshots remain the freshness authority for target-producing model changes.

Focused tag:

```text
[geometry][assembly-reference-target-resolution]
```

### 34. Stable semantic generated topology identity and recovery

Primary boundary: Part semantic-reference model and recovery.

Define producer-driven BLCAD semantic identities for:

```text
GeneratedCylindricalFace
GeneratedLinearEdge
GeneratedCircularEdge
GeneratedVertex
```

Each supported feature producer must publish a finite support matrix:

```text
producer
-> semantic face roles
-> semantic edge roles
-> semantic vertex roles
-> expected cardinality
-> ambiguity/failure behavior
```

Forbidden persistent identity sources:

```text
OCCT topology traversal index
TopoDS hash as model identity
BRep map position
XDE label tag
STEP entity number
memory address
unordered OCCT result order
```

Patterned subelements remain unsupported until stable per-instance semantic identity is frozen.

Focused tags:

```text
[core][semantic-generated-topology-reference]
[core][semantic-generated-topology-recovery]
```

### 35. Generated face, edge, and vertex target resolution

Primary boundary: Geometry semantic target resolution.

Resolve Block-34 semantic sources into Block-31 capabilities:

```text
GeneratedPlanarFace -> Plane
GeneratedCylindricalFace -> Cylinder + Axis
GeneratedLinearEdge -> Line
GeneratedCircularEdge -> Circle + Axis + Point(center)
GeneratedVertex -> Point
```

Semantic producer identity remains authoritative. OCCT topology is execution data used only after the intended semantic generated subelement has been identified.

Local and cross-hierarchy evaluation retain exact current transform semantics.

Focused tag:

```text
[geometry][assembly-generated-topology-target-resolution]
```

### 36. Explicit target compatibility matrix and generic geometric relationships

Primary boundary: relationship semantics and shared numeric integration.

Introduce one deterministic compatibility resolver:

```text
relationship type
+ target A capabilities
+ target B capabilities
-> one exact ordered capability pair
OR explicit incompatibility
```

Initial planned compatibility:

```text
Mate:
  Plane <-> Plane

Distance:
  Plane <-> Plane
  Point <-> Point
  Point <-> Plane
  Plane <-> Point

Angle:
  Plane <-> Plane
  Line/Axis <-> Line/Axis

Concentric:
  Axis <-> Axis

Insert:
  SeatFrame <-> SeatFrame
```

Initial planned generic relationship families:

```text
Coincident
Parallel
Perpendicular
```

New families must reuse existing local/cross-hierarchy graphs, `ComponentTransformAuthority`, shared finite differences/Gauss-Newton solving, freshness, atomic application, and actual Jacobian-rank diagnostics.

A generated cylindrical face, circular edge, DatumAxis, and current feature `.axis` that all project Axis capability must reach the same Concentric equation semantics.

The exact compatibility table, sign/direction rules, residual scalar order, and length scaling are frozen in the Block-36 canonical implementation document before code is merged.

### 37. Joint target capability expansion and richer joint-family sequence

Primary boundary: persistent joint intent, motion equations, and application.

Joint compatibility consumes the same target capability taxonomy.

The existing:

```text
SeatFrame <-> SeatFrame
```

Revolute contract remains valid.

Axis-only Revolute is not automatically enabled because an Axis does not define the reference direction required for signed twist measurement. Before `Axis <-> Axis` Revolute support, Block 37 must choose one explicit frame/reference-direction model-intent strategy.

No Geometry implementation may invent an arbitrary world-axis reference.

After the frame contract is resolved, dedicated family blocks should cover at least:

```text
Prismatic
Cylindrical
Planar
Ball/Spherical
```

Every family freezes:

```text
persistent coordinates and limits
required target capabilities
residual component order
holding-drive semantics
motion-graph participation
freshness snapshots
atomic application behavior
local and Project-level JSON spelling
```

Do not model stateful motion joints as geometric constraints with hidden coordinates.

## GUI selection boundary

Blocks 31-37 remain headless model/query/equation work.

A future Inventor-/SolidWorks-like picker should consume:

```text
semantic source candidate
-> resolved source kind
-> available capabilities
-> selected relationship/joint type
-> compatibility matrix
```

The UI may highlight OCCT faces, edges, vertices, datum planes, axes, lines, and points, but committing a selection persists the BLCAD semantic reference identity. Raw `TopoDS_Shape` or selection index never becomes the assembly endpoint.

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

The planned target expansion continues to persist semantic endpoint identity. Resolved source classification, Plane/Axis/Line/Point/Circle/Cylinder/SeatFrame capabilities, transformed target geometry, and compatibility projections remain derived.

Regenerate connectivity, hierarchy traversal, parent chains, leaves, target geometry/capabilities, transform authorities, solve/motion groups, holding drives, residuals/Jacobians, solve/motion results, freshness snapshots, proposals, diagnostics, exchange identities/graphs/names, part shape definitions, XDE labels/component references, STEP entities, posed shapes, and analysis products.

## Next technical step

Implement Block 30 only: freeze richer posed contact classification and a bounded deterministic swept-Revolute analysis contract over exact rooted component occurrence identities.

After Block 30, implement Block 31 only from `docs/assembly-general-geometric-target-roadmap.md`: typed derived geometric target taxonomy and explicit capability projection while preserving all existing semantic target strings and relationship behavior.

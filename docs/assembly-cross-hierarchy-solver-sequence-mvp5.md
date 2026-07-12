# Cross-Hierarchy Assembly Relationship Sequence MVP-5

Status: Blocks 23-30 are implemented. Block 31 is the current next technical step. Blocks 31-47 are explicitly planned as the general assembly geometric-target, relationship, and richer-joint sequence.

This document is the canonical numbered implementation sequence for assembly relationships, motion, hierarchy-aware exchange, posed analysis, and the handoff into general Inventor-/SolidWorks-like semantic target compatibility.

Implemented contracts are canonical in:

- `docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`
- `docs/assembly-cross-hierarchy-constraint-json-mvp5.md`
- `docs/assembly-cross-hierarchy-incidence-groups-mvp5.md`
- `docs/assembly-cross-hierarchy-numeric-solver-mvp5.md`
- `docs/assembly-cross-hierarchy-application-diagnostics-mvp5.md`
- `docs/assembly-cross-hierarchy-revolute-motion-mvp5.md`
- `docs/assembly-structured-step-products-mvp5.md`
- `docs/assembly-contact-swept-motion-mvp5.md`
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
- persist graph, residual, Jacobian, freshness snapshot, proposal, diagnostic, XDE label, STEP entity, contact, or sweep caches;
- mutate `SubassemblyInstance::transform()` through component authority variables;
- introduce a second optimizer, finite-difference contract, hierarchy traversal, pose model, or transform convention;
- call sampled sweep analysis continuous collision detection;
- add one-off feature-specific branches directly to generic equation builders once the target-capability layer exists;
- infer a joint twist reference direction from an arbitrary world axis;
- add multi-coordinate joint execution before coordinate state and JSON contracts exist;
- merge several richer joint families into one implementation block.

## Frozen solve/motion identities

Occurrence-qualified semantic endpoint:

```text
AssemblyHierarchyConstraintEndpoint =
  (occurrence_path,
   local ComponentInstanceId,
   semantic_reference)
```

Geometric component occurrence:

```text
HierarchyComponentOccurrence =
  (occurrence_path,
   local ComponentInstanceId)
```

Direct component transform authority:

```text
ComponentTransformAuthority =
  (assembly_document: DocumentId,
   local ComponentInstanceId)
```

Repeated rooted occurrences can remain different geometric/motion contexts while mapping to one shared child-document transform authority.

## Frozen exchange/contact occurrence identities

Assembly exchange occurrence:

```text
AssemblyExchangeAssemblyOccurrenceIdentity =
  exact rooted SubassemblyInstance occurrence path
```

Component exchange occurrence:

```text
AssemblyExchangeComponentOccurrenceIdentity =
  (containing assembly occurrence path,
   local ComponentInstanceId)
```

Part product definition:

```text
AssemblyExchangePartProductDefinitionIdentity =
  referenced PartDocumentId
```

Block-30 unordered contact pair identity:

```text
AssemblyComponentOccurrencePairIdentity =
  canonical ordered pair of
  AssemblyExchangeComponentOccurrenceIdentity values
```

Pair order is exact occurrence path sequence, then local `ComponentInstanceId`.

These identities are derived and unpersisted. They are not OCCT topology ids, TDF label tags, STEP entity ids, or aliases for `ComponentTransformAuthority`.

## Blocks 23-27: cross-hierarchy geometric solve chain — implemented

### 23. Core endpoint contract and Project-level geometric intent

Canonical document: `docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`.

Implemented occurrence-qualified Core endpoints and persistent Project-owned Mate/Concentric/Distance/Insert/Angle relationships.

### 24. Additive JSON and exact structure validation

Canonical document: `docs/assembly-cross-hierarchy-constraint-json-mvp5.md`.

Implemented additive:

```text
cross_hierarchy_constraints[]
```

with exact target/path order and root-to-reached-component structure validation after hierarchy validation.

### 25. Relationship-to-authority incidence and solve groups

Canonical document: `docs/assembly-cross-hierarchy-incidence-groups-mvp5.md`.

Implemented deterministic local/Project relationship incidence over unique `ComponentTransformAuthority` values. Local relationships appear once per containing `AssemblyDocument`; repeated rooted endpoints retain exact occurrence context while mapping to shared authorities.

### 26. Shared numeric residual/Jacobian integration

Canonical document: `docs/assembly-cross-hierarchy-numeric-solver-mvp5.md`.

Implemented one six-variable block per unique free active authority:

```text
tx_mm ty_mm tz_mm rx_deg ry_deg rz_deg
```

Local relationships evaluate in document-local space. Project-level relationships evaluate through exact rooted parent chains in root space. Existing residual flattening, central finite differences, damping/backtracking, and Gauss-Newton machinery are reused.

### 27. Complete freshness, atomic application, and diagnostics

Canonical document: `docs/assembly-cross-hierarchy-application-diagnostics-mvp5.md`.

Implemented authority/relationship/path-boundary/exact-semantic-PartDocument freshness, current solve-group revalidation, atomic direct-authority transform application, and Jacobian-rank/remaining-DOF diagnostics.

Focused tags:

```text
[geometry][assembly-cross-hierarchy-solver]
[geometry][assembly-cross-hierarchy-application]
[geometry][assembly-cross-hierarchy-diagnostics]
[geometry][assembly-semantic-freshness]
```

## Block 28: occurrence-qualified Revolute motion — implemented

Canonical document: `docs/assembly-cross-hierarchy-revolute-motion-mvp5.md`.

Implemented persistent Project-level `AssemblyHierarchyJoint` Revolute intent and additive:

```text
cross_hierarchy_joints[]
```

The combined motion closure spans local geometry, local joints, Project cross geometry, and Project cross joints. Exact rooted `.seat` endpoints evaluate in root space and reuse the shared directed axis/seating/signed-twist residual constructor.

Selected Project-level joints receive requested drives. Other active Revolute joints in the motion group hold authored coordinates. Motion reuses shared authority variables, finite differences, numeric solve machinery, complete freshness, and atomic authority-plus-selected-coordinate application.

Focused tags:

```text
[core][assembly-cross-hierarchy-joint]
[core][assembly-cross-hierarchy-joint-json]
[core][assembly-cross-hierarchy-motion-graph]
[geometry][assembly-cross-hierarchy-revolute]
[geometry][assembly-cross-hierarchy-motion]
```

## Block 29: structured assembly exchange and STEP product relationships — implemented

Canonical document: `docs/assembly-structured-step-products-mvp5.md`.

Block 29 adds no persistent record and no JSON field.

`AssemblyExchangeGraph` derives exact rooted assembly/component occurrence identities from `AssemblyHierarchyTraversal` plus canonical visible-active `AssemblyLeafOccurrenceResolver` output.

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

`AssemblyStructuredStepExporter` builds one XDE part definition per part, one assembly label per rooted occurrence, part component references to shared definitions, and parent-to-child assembly references. The flattened `AssemblyStepExporter` remains a compatibility consumer.

Focused tags:

```text
[core][assembly-exchange-graph]
[geometry][assembly-structured-step-export]
```

## Block 30: rooted contact classification and bounded sampled Revolute sweep — implemented

Canonical document: `docs/assembly-contact-swept-motion-mvp5.md`.

### Static contact classification

`AssemblyContactAnalyzer` reuses `AssemblyPosedLeafShapeBuilder` and evaluates every visible-active unordered rooted component occurrence pair exactly once.

Options:

```text
touching_tolerance_mm = 1.0e-6
minimum_overlap_volume_mm3 = 1.0e-6
```

Classification order:

```text
overlap_volume_mm3 > minimum_overlap_volume_mm3
  -> Interfering

otherwise minimum_distance_mm <= touching_tolerance_mm
  -> Touching

otherwise
  -> Separated
```

`AssemblyContactAnalysis::records` contains one complete classification record per evaluated pair. `Interfering` records have no minimum-distance value; non-interfering records retain the finite measured distance.

The historical `AssemblyInterferenceAnalyzer` and `AssemblyClearanceAnalyzer` remain unchanged compatibility query contracts.

### Sampled Revolute sweep

`AssemblyRevoluteSweepAnalyzer` supports exactly:

```text
RootAssemblyLocal
ProjectCrossHierarchy
```

Selected joints must exist in that scope, remain active, and be Revolute. Start/end values use `AngleDeg` and must lie within authored joint limits.

Sample count is bounded:

```text
2 <= sample_count <= 1001
```

Samples are linear, inclusive, and preserve caller interval direction.

Every sample starts from a fresh source Project copy:

```text
Project sample_project = source_project
```

Then the analyzer delegates to the existing scope-specific motion solver and atomic applier, requires convergence, and runs `AssemblyContactAnalyzer` over the applied sample copy.

No sample starts from the previous sample. No source transform or authored joint coordinate changes.

Each sample stores:

```text
sample_index
coordinate_deg
applied_transform_count
complete AssemblyContactAnalysis
```

The result therefore ties pair classification and measured non-interfering clearance to exact rooted occurrence pair, sample index, and requested coordinate.

This is deterministic discrete sampling, not continuous collision detection. Events occurring only between two sample coordinates may be missed.

Focused tags:

```text
[geometry][assembly-contact]
[geometry][assembly-revolute-sweep]
```

## Blocks 31-47: general assembly target, relationship, and joint sequence — planned

Canonical detail roadmap: `docs/assembly-general-geometric-target-roadmap.md`.

Mandatory order:

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

### 31. Typed geometric target taxonomy and capability projection — Next

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

Existing generated face, `.axis`, and `.seat` behavior must migrate without changing persistent target strings or numeric semantics.

Block 31 adds no source grammar, compatibility matrix, relationship family, joint family, or JSON shape.

### 32-36. Reference and generated-topology source identity/resolution

Blocks 32-36 implement, in order:

```text
32 Core assembly-selectable DatumPlane / DatumAxis / ConstructionLine / ConstructionPoint intent
33 additive serialization and structure validation
34 Plane / Axis / Line / Point reference-geometry resolution
35 stable producer-driven semantic cylindrical-face / linear-edge / circular-edge / vertex identity and recovery
36 generated face / edge / vertex capability resolution
```

Raw OCCT traversal indices, hashes, BRep positions, XDE labels, STEP entity numbers, memory addresses, and unordered topology result order remain forbidden persistent identity sources.

### 37-40. Compatibility and generic relationship/joint target semantics

Blocks 37-40 implement, in order:

```text
37 deterministic relationship target compatibility matrix
38 local + Project-level Coincident / Parallel / Perpendicular persistent intent and JSON
39 generic relationship equations through existing local/cross numeric/freshness/diagnostic paths
40 joint target compatibility and deterministic oriented Frame contract
```

Axis alone remains insufficient for signed Revolute twist. Geometry may not invent a world-axis reference direction.

### 41-43. General joint coordinate and vector-drive infrastructure

Blocks 41-43 implement, in order:

```text
41 family-defined typed coordinate/limit slots
42 coordinate-slot JSON and historical scalar Revolute compatibility
43 vector drives, authored holding semantics, complete freshness, and atomic selected-role application
```

Joint coordinates remain drive parameters, not component transform-authority variables.

### 44-47. Richer joint families

One family per block:

```text
44 Prismatic
45 Cylindrical
46 Planar
47 Ball/Spherical
```

Exact target capability, coordinate roles/units, limits, residual ordering, local/cross integration, freshness, and application semantics are canonical in `docs/assembly-general-geometric-target-roadmap.md` and each future family document.

## GUI selection boundary

Blocks 31-47 remain headless model/query/equation/motion work.

A future picker consumes:

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

Block 30 adds no JSON field. Contact pair identities, contact records, sample Project copies, sample motion results, and sampled sweep results are derived.

The planned target expansion continues to persist semantic endpoint identity. Resolved source classification, Plane/Axis/Line/Point/Circle/Cylinder/Frame capabilities, transformed target geometry, and compatibility projections remain derived.

New generic relationship type intent becomes persistent in Block 38. Generalized joint coordinate slots become persistent in Block 41 and serialized in Block 42.

Regenerate connectivity, hierarchy traversal, parent chains, leaves, target geometry/capabilities, compatibility projections, transform authorities, solve/motion groups, holding drives, residuals/Jacobians, solve/motion results, freshness snapshots, proposals, diagnostics, exchange identities/graphs/names, part shape definitions, XDE labels/component references, STEP entities, posed shapes, contact records, and sampled sweep products.

## Next technical step

Implement Block 31 only from `docs/assembly-general-geometric-target-roadmap.md`: typed geometric target taxonomy and explicit capability projection.

Preserve all current semantic target strings and Mate/Distance/Angle/Concentric/Insert/Revolute numeric behavior. Do not add DatumAxis persistence, reference-geometry JSON, generic relationship families, or richer joint families in Block 31.

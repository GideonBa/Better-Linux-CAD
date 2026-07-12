# Cross-Hierarchy Assembly Relationship Sequence MVP-5

Status: Blocks 23–33 are implemented. Block 34 is the current next technical step. Blocks 34–47 remain the explicit general assembly target, relationship, and richer-joint continuation.

This document is the canonical numbered handoff sequence for cross-document assembly intent, solving, motion, exchange, posed analysis, and general geometric target architecture.

Implemented contracts are canonical in:

- `docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`
- `docs/assembly-cross-hierarchy-constraint-json-mvp5.md`
- `docs/assembly-cross-hierarchy-incidence-groups-mvp5.md`
- `docs/assembly-cross-hierarchy-numeric-solver-mvp5.md`
- `docs/assembly-cross-hierarchy-application-diagnostics-mvp5.md`
- `docs/assembly-cross-hierarchy-revolute-motion-mvp5.md`
- `docs/assembly-structured-step-products-mvp5.md`
- `docs/assembly-contact-swept-motion-mvp5.md`
- `docs/assembly-geometric-target-taxonomy-mvp5.md`
- `docs/file-format.md`

Future target/relationship/joint details are canonical in:

- `docs/assembly-general-geometric-target-roadmap.md`

## Sequencing rule

```text
Core persistent model intent
  -> serialization and structure compatibility
  -> stable semantic source identity
  -> deterministic derived occurrence/authority connectivity
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
- duplicate one local relationship per rooted occurrence;
- add residual rows merely to encode shared transform authority;
- persist graph/residual/Jacobian/freshness/proposal/diagnostic/exchange/contact/sweep caches;
- mutate `SubassemblyInstance::transform()` through component authority variables;
- introduce a second optimizer, finite-difference contract, hierarchy traversal model, pose model, or transform convention;
- infer equation geometry from source-kind enums when capability projection exists;
- infer Revolute twist reference from an arbitrary world axis;
- call sampled sweep analysis continuous collision detection;
- add multi-coordinate joint execution before coordinate state and JSON contracts exist;
- merge several richer joint families into one block.

## Frozen identities

Occurrence-qualified semantic endpoint:

```text
(occurrence_path,
 local ComponentInstanceId,
 semantic_reference)
```

Geometric component occurrence:

```text
(occurrence_path,
 local ComponentInstanceId)
```

Direct component transform authority:

```text
ComponentTransformAuthority =
  (assembly_document: DocumentId,
   local ComponentInstanceId)
```

Assembly exchange occurrence:

```text
exact rooted SubassemblyInstance path
```

Component exchange occurrence:

```text
(containing rooted assembly path,
 local ComponentInstanceId)
```

Part product definition:

```text
referenced PartDocumentId
```

Rooted contact pair:

```text
canonical ordered pair of
AssemblyExchangeComponentOccurrenceIdentity values
```

Block-31 resolved target endpoint identity remains either the exact local endpoint or exact occurrence-qualified endpoint above. Source kind, descriptor, capability vector, coordinate space, and transform context are derived.

## Blocks 23–27 — Cross-hierarchy geometric solve chain — implemented

### 23. Core endpoint contract and Project-level geometric intent

Canonical: `docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`.

Implemented occurrence-qualified Core endpoints and persistent Project-owned Mate/Distance/Angle/Concentric/Insert relationships.

### 24. Additive JSON and exact structure validation

Canonical: `docs/assembly-cross-hierarchy-constraint-json-mvp5.md`.

Implemented `cross_hierarchy_constraints[]` with exact endpoint/path order and reached-component structure validation after hierarchy validation.

### 25. Relationship-to-authority incidence and solve groups

Canonical: `docs/assembly-cross-hierarchy-incidence-groups-mvp5.md`.

Implemented deterministic local/Project relationship incidence over unique `ComponentTransformAuthority` values. Local relationships appear once per containing `AssemblyDocument`; rooted endpoints retain exact occurrence context while mapping to shared authorities.

### 26. Shared numeric residual/Jacobian integration

Canonical: `docs/assembly-cross-hierarchy-numeric-solver-mvp5.md`.

Each unique free active authority contributes:

```text
tx_mm ty_mm tz_mm rx_deg ry_deg rz_deg
```

Local relationships evaluate in document-local space. Project-level relationships evaluate through exact rooted chains in root space. Shared residual flattening, central finite differences, damping/backtracking, and Gauss-Newton execution remain authoritative.

### 27. Complete freshness, atomic application, diagnostics

Canonical: `docs/assembly-cross-hierarchy-application-diagnostics-mvp5.md`.

Implemented authority/relationship/path-boundary/exact-PartDocument freshness, current solve-group revalidation, atomic direct-authority application, and Jacobian-rank/remaining-DOF diagnostics.

Focused tags:

```text
[geometry][assembly-cross-hierarchy-solver]
[geometry][assembly-cross-hierarchy-application]
[geometry][assembly-cross-hierarchy-diagnostics]
[geometry][assembly-semantic-freshness]
```

## Block 28 — Occurrence-qualified Revolute motion — implemented

Canonical: `docs/assembly-cross-hierarchy-revolute-motion-mvp5.md`.

Implemented Project-level `AssemblyHierarchyJoint` Revolute intent, `cross_hierarchy_joints[]`, exact rooted `.seat` endpoints, combined local/cross geometry+joint motion closure, holding drives for non-selected active Revolute joints, shared numeric execution, complete freshness, and atomic authority-plus-selected-coordinate application.

Focused tags:

```text
[core][assembly-cross-hierarchy-joint]
[core][assembly-cross-hierarchy-joint-json]
[core][assembly-cross-hierarchy-motion-graph]
[geometry][assembly-cross-hierarchy-revolute]
[geometry][assembly-cross-hierarchy-motion]
```

## Block 29 — Structured assembly exchange — implemented

Canonical: `docs/assembly-structured-step-products-mvp5.md`.

`AssemblyExchangeGraph` derives exact rooted assembly/component occurrence identities from canonical hierarchy and visible-active leaf resolution. Ordering is deterministic by exact path sequence, then local component id, then part id.

`AssemblyPartShapeDefinitionBuilder` recomputes each unique referenced `PartDocumentId` once. Structured XDE/STEP export creates shared part definitions, rooted assembly labels, component references, and exact parent-child assembly references.

Focused tags:

```text
[core][assembly-exchange-graph]
[geometry][assembly-structured-step-export]
```

Block 29 adds no JSON field.

## Block 30 — Rooted contact classification and sampled Revolute sweep — implemented

Canonical: `docs/assembly-contact-swept-motion-mvp5.md`.

Static classification:

```text
overlap_volume_mm3 > minimum_overlap_volume_mm3
  -> Interfering

otherwise minimum_distance_mm <= touching_tolerance_mm
  -> Touching

otherwise
  -> Separated
```

`AssemblyContactAnalysis::records` contains one complete record per visible-active unordered rooted component pair.

`AssemblyRevoluteSweepAnalyzer` supports existing root-local and Project cross-hierarchy Revolute motion. It accepts `2..1001` inclusive linear samples and starts every sample from a fresh copy of the same source Project before existing motion solve/application and contact analysis.

This is deterministic discrete sampling, not continuous collision detection.

Focused tags:

```text
[geometry][assembly-contact]
[geometry][assembly-revolute-sweep]
```

Block 30 adds no persistent analysis record or JSON field.

## Block 31 — Typed geometric target taxonomy and capability projection — implemented

Canonical: `docs/assembly-geometric-target-taxonomy-mvp5.md`.

Implemented source-kind taxonomy:

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

Implemented capability taxonomy in canonical order:

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
derived source metadata
typed descriptor variant
canonical capability vector
ComponentLocal or RootAssembly coordinate space
exact transforms_inner_to_outer context
```

Projection is explicit:

```text
project_plane
project_axis
project_line
project_point
project_circle
project_cylinder
project_frame
```

Every projector validates the complete resolved target and rejects unsupported capability projection.

Current source adaptation:

```text
.top/.bottom/.right/.left/.front/.back
  -> GeneratedPlanarFace
  -> Plane

.axis current single-circle subtractive-extrude producer
  -> GeneratedCylindricalFace
  -> Axis + Cylinder

.seat
  -> CircularFeatureSeat
  -> Plane + Axis + Frame
```

Existing semantic-reference strings remain unchanged.

Local and hierarchy legacy target resolver APIs now adapt from the typed projection boundary. Existing Mate/Distance/Angle/Concentric/Insert/Revolute descriptor shapes and equation/residual semantics remain unchanged.

Focused tag:

```text
[geometry][assembly-geometric-target-taxonomy]
```

Block 31 adds no persistent source kind, descriptor, capability vector, source grammar, relationship family, joint family, or JSON field.

## Blocks 32–47 — Planned continuation

Canonical detail roadmap: `docs/assembly-general-geometric-target-roadmap.md`.

Mandatory order:

```text
32 assembly-selectable reference geometry Core intent
33 reference geometry serialization and structure validation
34 datum/axis/line/point target resolution
35 stable semantic generated topology identity/recovery
36 generated face/edge/vertex target resolution
37 explicit target compatibility matrix
38 generic geometric relationship Core intent + JSON
39 generic relationship equations + shared solve integration
40 joint target compatibility + oriented Frame contract
41 general joint coordinate/limit Core model
42 general joint coordinate JSON/backward compatibility
43 vector joint drives + holding/freshness/atomic application
44 Prismatic joint
45 Cylindrical joint
46 Planar joint
47 Ball/Spherical joint
```

Do not merge these blocks.

## Block 32 — Assembly-selectable reference geometry Core intent — implemented

Canonical document: `docs/assembly-reference-geometry-intent-mvp5.md`.

Primary boundary: Core persistent model intent and semantic source identity.

Implemented first-class `DatumAxis` PartDocument intent with the frozen `Explicit` and `FromConstructionLine` definition families, existing-construction-geometry ownership/dependency/invalidation semantics, and the frozen reference semantic-source grammar:

```text
ref:datum_plane:<encoded-id>
ref:datum_axis:<encoded-id>
ref:construction_line:<encoded-id>
ref:construction_point:<encoded-id>
```

Uppercase `%HH` escaping of every id byte outside `[A-Za-z0-9_-]` keeps valid reference spellings dot-free and therefore provably disjoint from feature target spellings; ids containing `.`, `/`, and `%` stay unambiguous and parsing fails closed. Existing feature target spellings and local/cross endpoint JSON shapes are unchanged. Block 32 performs no Geometry target resolution and adds no JSON field.

Focused tags:

```text
[core][datum-axis]
[core][assembly-reference-target-intent]
```

## Block 33 — Reference geometry serialization and structure validation — implemented

Canonical document: `docs/assembly-reference-geometry-intent-mvp5.md`. Save-format authority: `docs/file-format.md`.

Implemented the additive optional `datum_axes` part-document array for both DatumAxis families, historical-file compatibility, load-time ownership/family validation through `PartDocument::add_datum_axis`, unchanged local/cross endpoint JSON shapes, and byte-for-byte `ref:` spelling roundtrips. No target geometry resolution during load.

Focused tags:

```text
[core][datum-axis-json]
[core][assembly-reference-target-json]
```

## Block 34 — Datum/axis/line/point target resolution — Next

Resolve:

```text
DatumPlane       -> Plane
DatumAxis        -> Axis + Line
ConstructionLine -> Line
ConstructionPoint -> Point
```

Reuse existing workplane/construction geometry execution. Local results remain component-local; hierarchy results reuse exact root-space transform chains. Existing canonical PartDocument snapshots remain freshness authority.

## Blocks 35–36 — Stable generated topology identity and resolution

Block 35 defines producer-driven semantic identities/recovery for generated cylindrical faces, linear edges, circular edges, and vertices. Raw topology traversal/hash/map/XDE/STEP/memory identity remains forbidden.

Block 36 resolves only Block-35 supported semantic producers into Block-31 descriptors/capabilities.

## Blocks 37–40 — Compatibility and generic relationship/joint target semantics

Block 37 introduces one deterministic relationship compatibility resolver from relationship type plus target capability sets to one exact ordered capability pair/bundle or explicit incompatibility.

Block 38 adds persistent local/Project-level `Coincident`, `Parallel`, and `Perpendicular` intent plus JSON.

Block 39 adds their equations through existing graph, authority, residual/Jacobian, freshness, atomic application, and diagnostics paths.

Block 40 migrates joint target compatibility to capability semantics and freezes oriented `Frame` requirements. Current Revolute remains `Frame <-> Frame`; Axis alone is insufficient for signed twist.

## Blocks 41–43 — General joint coordinate foundation

Block 41 generalizes persistent joint coordinates/limits into family-defined typed coordinate slots.

Block 42 serializes those slots while preserving historical scalar Revolute compatibility.

Block 43 generalizes selected-joint drives to deterministic coordinate drive vectors with holding semantics, freshness, and atomic application.

Authority variables remain six direct component-transform variables per free authority. Joint coordinates remain drive parameters.

## Blocks 44–47 — Richer joint families

One family per block:

```text
44 Prismatic
45 Cylindrical
46 Planar
47 Ball/Spherical
```

Exact target compatibility, coordinate roles, limits, residual order, holding-drive behavior, motion integration, freshness, and application semantics remain canonical in `docs/assembly-general-geometric-target-roadmap.md` until each block is implemented.

## Persistence rule

Persist model intent.

Current persistent assembly intent includes:

```text
component placement/state
local geometric relationships
local Revolute joints/limits/coordinate
Project-owned child assemblies
SubassemblyInstance placement/state
cross_hierarchy_constraints[]
cross_hierarchy_joints[]
```

Regenerate:

```text
hierarchy traversal and exact transform chains
relationship/joint connectivity
ComponentTransformAuthority mappings
resolved target source classification
Plane/Axis/Line/Point/Circle/Cylinder/Frame descriptors
capability vectors and projections
residuals/Jacobians/solve results
freshness snapshots/proposals/diagnostics
exchange graph/product identities
XDE labels and STEP entities
posed shapes/contact records/sweep analyses
```

## Next technical step

Implement Block 34 only: Geometry resolution of DatumPlane/DatumAxis/ConstructionLine/ConstructionPoint `ref:` sources into the Block-31 taxonomy, reusing existing workplane/construction execution and exact transform chains.

Do not implement Block-35 generated topology identity in Block 34.

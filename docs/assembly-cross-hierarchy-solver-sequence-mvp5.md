# Cross-Hierarchy Assembly Relationship Sequence MVP-5

Status: Blocks 23–47 and Part Construction Blocks 48–94 are implemented. The Assembly MVP sequence is complete; Block 83 is implemented; Block 84 is implemented; Block 85 is implemented; Block 86 is implemented; Block 87 is implemented; Block 88 is implemented; Block 89 is implemented; Block 90 is implemented; Block 91 is implemented; Block 92 is implemented; Block 93 is implemented; Block 94 is implemented; Block 95 Qt application shell, GUI document session, and command architecture is implemented; Block 96 document lifecycle, transactions, recompute, and diagnostics is implemented; Block 97 OCCT viewport, navigation, display, and semantic picking is implemented; Block 98 browser, property editor, and selection synchronization is the current next technical step.

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
- `docs/assembly-reference-geometry-intent-mvp5.md`
- `docs/assembly-generated-topology-reference-mvp5.md`
- `docs/assembly-generic-relationship-intent-mvp5.md`
- `docs/assembly-generic-relationship-equations-mvp5.md`
- `docs/assembly-joint-target-compatibility-mvp5.md`
- `docs/assembly-joint-coordinate-model-mvp5.md`
- `docs/assembly-joint-coordinate-json-mvp5.md`
- `docs/assembly-spherical-joint-mvp5.md`
- `docs/assembly-vector-joint-drive-mvp5.md`
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

A Block-31 resolved target retains the exact local or occurrence-qualified endpoint identity. Source kind, descriptor, capability vector, coordinate space, and transform context remain derived.

Block 35 adds stable semantic generated-topology identity at the endpoint-string/Core reference boundary. It does not replace occurrence identity or transform authority.

## Blocks 23–27 — Cross-hierarchy geometric solve chain — Implemented

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

## Block 28 — Occurrence-qualified Revolute motion — Implemented

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

## Block 29 — Structured assembly exchange — Implemented

Canonical: `docs/assembly-structured-step-products-mvp5.md`.

`AssemblyExchangeGraph` derives exact rooted assembly/component occurrence identities from canonical hierarchy and visible-active leaf resolution. Ordering is deterministic by exact path sequence, then local component id, then part id.

`AssemblyPartShapeDefinitionBuilder` recomputes each unique referenced `PartDocumentId` once. Structured XDE/STEP export creates shared part definitions, rooted assembly labels, component references, and exact parent-child assembly references.

Focused tags:

```text
[core][assembly-exchange-graph]
[geometry][assembly-structured-step-export]
```

Block 29 adds no JSON field.

## Block 30 — Rooted contact classification and sampled Revolute sweep — Implemented

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

`AssemblyRevoluteSweepAnalyzer` supports existing root-local and Project cross-hierarchy Revolute motion. It accepts `2..1001` inclusive linear samples and starts every sample from a fresh copy of the source Project before existing motion solve/application and contact analysis.

This is deterministic discrete sampling, not continuous collision detection.

Focused tags:

```text
[geometry][assembly-contact]
[geometry][assembly-revolute-sweep]
```

Block 30 adds no persistent analysis record or JSON field.

## Block 31 — Typed geometric target taxonomy and capability projection — Implemented

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

One `AssemblyResolvedGeometricTarget` retains exact endpoint identity, source kind, derived source metadata, typed descriptor variant, canonical capability vector, coordinate space, and exact transform context.

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

Current source adaptation:

```text
.top/.bottom/.right/.left/.front/.back -> GeneratedPlanarFace -> Plane
.axis current single-circle subtractive producer -> GeneratedCylindricalFace -> Axis + Cylinder
.seat -> CircularFeatureSeat -> Plane + Axis + Frame
```

Existing semantic-reference strings remain unchanged. Local and hierarchy legacy target resolver APIs adapt from typed projections. Existing Mate/Distance/Angle/Concentric/Insert/Revolute descriptor and residual semantics remain unchanged.

Focused tag:

```text
[geometry][assembly-geometric-target-taxonomy]
```

## Blocks 32–47 — General target, relationship, and richer-joint continuation

Canonical detail roadmap: `docs/assembly-general-geometric-target-roadmap.md`.

Mandatory order:

```text
32 assembly-selectable reference geometry Core intent (implemented)
33 reference geometry serialization and structure validation (implemented)
34 datum/axis/line/point target resolution (implemented)
35 stable semantic generated topology identity/recovery (implemented)
36 generated face/edge/vertex target resolution (implemented)
37 explicit target compatibility matrix (implemented)
38 generic geometric relationship Core intent + JSON (implemented)
39 generic relationship equations + shared solve integration (implemented)
40 joint target compatibility + oriented Frame contract (implemented)
41 general joint coordinate/limit Core model (implemented)
42 general joint coordinate JSON/backward compatibility (implemented)
43 vector joint drives + holding/freshness/atomic application (implemented)
44 Prismatic joint
45 Cylindrical joint
46 Planar joint
47 Ball/Spherical joint
```

Do not merge these blocks.

## Block 32 — Assembly-selectable reference geometry Core intent — Implemented

Canonical document: `docs/assembly-reference-geometry-intent-mvp5.md`.

Implemented first-class `DatumAxis` PartDocument intent with the frozen `Explicit` and `FromConstructionLine` families and the canonical reference semantic-source grammar:

```text
ref:datum_plane:<encoded-id>
ref:datum_axis:<encoded-id>
ref:construction_line:<encoded-id>
ref:construction_point:<encoded-id>
```

Uppercase `%HH` escaping outside `[A-Za-z0-9_-]` keeps valid reference spellings dot-free and disjoint from legacy feature-role spellings. Parsing fails closed. Block 32 adds no JSON field and performs no Geometry target resolution.

Focused tags:

```text
[core][datum-axis]
[core][assembly-reference-target-intent]
```

## Block 33 — Reference geometry serialization and structure validation — Implemented

Canonical document: `docs/assembly-reference-geometry-intent-mvp5.md`. Save-format authority: `docs/file-format.md`.

Implemented the additive optional `datum_axes` PartDocument array, historical-file compatibility, load-time ownership/family validation, unchanged endpoint JSON shapes, and byte-for-byte `ref:` spelling roundtrips. No target geometry is resolved during load.

Focused tags:

```text
[core][datum-axis-json]
[core][assembly-reference-target-json]
```

## Block 34 — Datum/axis/line/point target resolution — Implemented

Implemented:

```text
DatumPlane        -> Plane
DatumAxis         -> Axis + Line
ConstructionLine  -> Line
ConstructionPoint -> Point
```

Resolution reuses existing workplane/construction geometry execution. Local results remain component-local; hierarchy results reuse exact root-space transform chains. Existing canonical PartDocument snapshots remain freshness authority.

Focused tag:

```text
[geometry][assembly-reference-target-resolution]
```

## Block 35 — Stable generated topology identity and recovery — Implemented

Canonical document: `docs/assembly-generated-topology-reference-mvp5.md`.

Block 35 defines Core producer-driven semantic identities for generated cylindrical faces, linear edges, circular edges, and vertices.

Canonical `topo:` endpoint spellings retain exact feature/profile/role intent. The first producer matrices are:

```text
RectangularAdditiveExtrude
  -> 12 named linear edges, cardinality 1 each
  -> 8 named vertices, cardinality 1 each

SingleCircleSubtractiveExtrude
  -> cylindrical wall, cardinality 1
  -> source_rim, cardinality 1
  -> opposite_rim, cardinality 1
```

The single-circle producer retains exact source `CircleProfileId`. Unsupported producers, ambiguous multi/mixed-circle sources, wrong profile ids, and unsupported family/role pairs fail closed. Pattern-generated subelements remain unavailable until stable per-instance identity exists.

`ReferenceRecoveryEvaluator` now provides read-only generated-topology recovery. No OCCT traversal/hash/map/XDE/STEP/memory identity is persisted or written by recovery.

Focused tags:

```text
[core][semantic-generated-topology-reference]
[core][semantic-generated-topology-recovery]
```

Block 35 adds no Geometry target resolver branch or equation.

## Block 36 — Generated face, edge, and vertex target resolution — Implemented

Primary boundary: Geometry semantic target resolution.

Implemented resolution of the four Block-35 `topo:` semantic producers into Block-31 descriptors/capabilities (generated planar faces already resolve through the unchanged legacy `.top/.bottom/...` path):

```text
GeneratedCylindricalFace -> Cylinder + Axis
GeneratedLinearEdge      -> Line
GeneratedCircularEdge    -> Circle + Axis + Point(center)
GeneratedVertex          -> Point
```

`topo:` spellings are parsed before legacy feature-role parsing. Resolution starts from `validate_generated_topology_reference` producer identity and then constructs typed descriptors analytically from validated feature/sketch/profile intent — the single-circle wall reuses the existing circular-feature geometry, circular rims are the two coaxial circles at the box faces the through-all cut passes through, and rectangular linear edges/vertices are derived from the target box extent. OCCT topology is not consulted.

Local targets remain component-local. Hierarchy targets reuse the local resolver through the exact component-plus-parent transform chain. Repeated rooted occurrences have different root-space geometry while sharing one PartDocument semantic source identity.

Focused tag:

```text
[geometry][assembly-generated-topology-target-resolution]
```

Block 36 adds no compatibility rule, no new relationship type, and no JSON field.

## Block 37 — Explicit target compatibility matrix — Implemented

Implemented one deterministic relationship compatibility resolver from relationship type plus target capability sets to one exact ordered capability pair/bundle or explicit incompatibility:

```text
Mate         Plane <-> Plane
Distance     Plane <-> Plane | Point <-> Point | Point <-> Plane | Plane <-> Point
Angle        Plane <-> Plane | Line <-> Line | Axis <-> Axis | Line <-> Axis | Axis <-> Line
Concentric   Axis <-> Axis
Insert       Frame <-> Frame
```

Local and cross-hierarchy equation builders consume compatibility before projection. Block 37 adds no new relationship enum, equation, graph rule, JSON field, or persisted Geometry query product.

Focused tags:

```text
[geometry][assembly-target-compatibility]
[geometry][assembly-cross-hierarchy-target-compatibility]
```

## Block 38 — Generic relationship Core intent and JSON — Implemented

Canonical: `docs/assembly-generic-relationship-intent-mvp5.md`.

The shared Core relationship enum now includes `Coincident`, `Parallel`, and `Perpendicular` for both local and Project-level records. Existing endpoint shapes, exact target A/B order, active/inactive state, and independent local/Project id scopes remain unchanged.

The three new families carry no `distance` or `angle`. JSON accepts only lowercase `coincident`, `parallel`, and `perpendicular`; local `assembly_constraints[]` and Project `cross_hierarchy_constraints[]` shapes are unchanged and historical five-family files remain compatible.

The new families remain persistent-only until Block 39. A private graph-participation guard keeps them out of current local solve, cross-hierarchy solve, and cross-hierarchy motion connectivity, preserving the historical five equation-enabled graph families.

Focused tags:

```text
[core][assembly-generic-relationship-intent]
[core][assembly-generic-relationship-json]
```

## Block 39 — Generic relationship equations and shared solve integration — Implemented

Canonical: `docs/assembly-generic-relationship-equations-mvp5.md`.

Coincident, Parallel, and Perpendicular now consume Block-37 capability compatibility and produce shared residual descriptors over Point/Line/Axis/Plane projections. Local and cross-hierarchy numeric paths flatten those residuals through the existing scale rules and reuse the single central-difference/Gauss-Newton engine.

The three Block-38 families now participate in existing solve/motion graphs. Repeated occurrence authority identity, semantic PartDocument freshness, relationship/group freshness, atomic application, and actual Jacobian-rank diagnostics remain unchanged and apply to the new families automatically.

Block 39 also executes non-planar Line/Axis Angle compatibility selected in Block 37 through the same direction-dot equation semantics.

Focused tags:

```text
[geometry][assembly-generic-relationships]
[geometry][assembly-generic-relationships-solver]
[geometry][assembly-generic-relationships-cross-hierarchy]
[geometry][assembly-generic-relationships-diagnostics]
```

## Block 40 — Joint target compatibility and oriented Frame contract — Implemented

Block 40 migrates joint target compatibility to capability semantics and freezes oriented `Frame` requirements. Current Revolute remains `Frame <-> Frame`; Axis alone is insufficient for signed twist.

The implemented `AssemblyJointTargetCompatibilityResolver` is consumed before projection by both local and cross-hierarchy Revolute builders. Existing `.seat` targets remain supported, exact local/rooted identities remain unchanged, and Axis-only targets fail closed without arbitrary Frame synthesis. Both paths share the unchanged Revolute residual constructor. Canonical contract: `docs/assembly-joint-target-compatibility-mvp5.md`.

## Block 41 — General joint coordinate/limit Core model — Implemented

Local and Project-level joints now share ordered family-defined coordinate slots with stable semantic roles, explicit Angular/Linear kinds, typed authored values, and optional typed bounds. Revolute requires one bounded Angular `rotation` slot; legacy scalar construction/access is an explicit adapter. Signed linear coordinates have a dedicated millimeter quantity and are not conflated with positive construction lengths.

No target scope, JSON representation, drive shape, or joint family changed. Canonical contract: `docs/assembly-joint-coordinate-model-mvp5.md`.

Focused tags: `[core][assembly-joint-coordinate-model]` and `[core][assembly-cross-hierarchy-joint-coordinate-model]`.

## Block 42 — General joint coordinate JSON and compatibility — Implemented

Local and Project-level writers emit deterministic typed `coordinates[]` plus the historical Revolute scalar fields. Readers accept slot-only, historical-only, or exactly matching dual records and fail closed on malformed or conflicting coordinate state. Canonical contract: `docs/assembly-joint-coordinate-json-mvp5.md`.

Focused tags: `[core][assembly-joint-coordinate-json]` and `[core][assembly-cross-hierarchy-joint-coordinate-json]`.

## Block 43 — General joint coordinate drives — Implemented

Local and Project-level motion now shares typed role-addressed drive validation, family-order restoration, authored holding semantics, complete coordinate-slot/drive freshness, and atomic application of transforms plus exactly driven selected roles. Scalar Revolute APIs delegate to this boundary. Canonical contract: `docs/assembly-vector-joint-drive-mvp5.md`.

Focused tags: `[geometry][assembly-vector-joint-drive]`, `[geometry][assembly-cross-hierarchy-vector-joint-drive]`, and `[geometry][assembly-vector-joint-drive-application]`.

Authority variables remain six direct component-transform variables per free authority. Joint coordinates remain drive parameters.

## Blocks 44–45 — Implemented richer joint families

One family per block:

```text
44 Prismatic
45 Cylindrical
```

Canonical contracts: `docs/assembly-prismatic-joint-mvp5.md` and
`docs/assembly-cylindrical-joint-mvp5.md`.

## Block 46 — Planar joint family — Implemented

Canonical contract: `docs/assembly-planar-joint-mvp5.md`.

## Block 47 — Ball/Spherical joint family — Implemented

```text
Spherical -> passive Point/Point center coincidence
```

Canonical contract: `docs/assembly-spherical-joint-mvp5.md`. Spherical has no coordinate slots,
contributes three center-offset rows to local and cross-hierarchy motion closures, reuses existing
freshness/application authority, and cannot be selected as a drive.

## Persistence rule

Persist model intent.

Current persistent assembly intent includes:

```text
component placement/state
local geometric relationships including persistent Coincident/Parallel/Perpendicular intent
local Revolute, Prismatic, Cylindrical, Planar, and passive Spherical joint intent
Project-owned child assemblies
SubassemblyInstance placement/state
cross_hierarchy_constraints[]
cross_hierarchy_joints[]
exact endpoint semantic-reference strings including ref: and topo: families
```

Regenerate:

```text
hierarchy traversal and exact transform chains
relationship/joint connectivity
ComponentTransformAuthority mappings
generated topology producer matrices/classification/recovery results
resolved target source classification
Plane/Axis/Line/Point/Circle/Cylinder/Frame descriptors
capability vectors and projections
residuals/Jacobians/solve results
freshness snapshots/proposals/diagnostics
exchange graph/product identities
XDE labels and STEP entities
posed shapes/contact records/sweep analyses
```

## Blocks 46–47 — Final Assembly joint families — Implemented

Block 46 is implemented in `docs/assembly-planar-joint-mvp5.md`: three-coordinate local and
Project-level Planar intent, JSON, Frame/Frame equations, and shared vector-drive motion.

Block 47 is implemented in `docs/assembly-spherical-joint-mvp5.md`: passive local and Project-level
Spherical intent, JSON, Point/Point equations, shared motion closure, and selected-drive rejection.
Blocks 48–94 Body identity, body-scoped recompute/inspection, Body Booleans, and
BodyTransform/SketchOwnership intent plus associative Geometry execution are implemented. The
Block-59 richer Extrude/Cut intent/JSON, Block-60 Geometry, and Block-61 Revolve/RevolveCut intent
Block 62 Revolve/RevolveCut Geometry, Block 63 General Part Pattern Core intent/JSON, and Blocks
64–65 Linear/Circular Pattern Geometry and Block 66 MirrorFeature Core intent/JSON are also
implemented. Fillet/Chamfer Core intent and JSON are implemented in Block 68, with Geometry in
Blocks 69–70; ShellFeature Core intent/JSON and Geometry are implemented in Blocks 71–72, and
DraftFeature Core intent/JSON in Block 73, Geometry in Block 74, the Basic 3D Sketch Core in Block
75, richer 3D curve intent in Block 76, strict 3D Sketch JSON in Block 77, deterministic transient
OCCT Geometry conversion in Block 78, connected PathCurve Core/JSON intent in Block 79,
persistent Sweep/SweepCut/SweepSurface intent in Block 80, basic planar Sweep execution in
Block 81, spatial path, twist, and guide execution in Block 82, path-following Extrude and
Extruded Cut in Block 83, persistent Loft intent in Block 84, two-section Loft Geometry in Block
85, Multi-section Loft in Block 86, guided/continuity-controlled Loft in Block 87, Surface feature
Core intent/JSON in Block 88, and Boundary/Fill Surface Geometry in Block 89. The current next
technical step is Block 90 Trim and Extend Surface Geometry.
Blocks 40–47 compatibility, persistence, numeric,
freshness, and application contracts remain authoritative.

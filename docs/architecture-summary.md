# Architecture Summary

This document summarizes implemented BLCAD architecture and current direction. Feature-specific documents remain canonical for exact formulas, save-format spellings, validation/failure policy, ordering, and focused proofs.

## Goal and authority model

BLCAD is an independent parametric CAD system for Linux. BLCAD owns semantic model intent; OCCT/Open CASCADE is a computed geometry and data-exchange kernel.

Fundamental decisions:

- persist parametric and semantic intent rather than final BRep authority;
- persist semantic references rather than raw OCCT topology ids;
- keep Core model intent below Geometry query/execution types;
- separate stable semantic source identity from Geometry topology lookup;
- separate semantic source classification from solver geometric capability;
- separate direct component transform authority from rooted occurrence identity;
- separate authored hierarchy boundaries from derived traversal/composition;
- separate geometric relationships from motion-joint intent;
- separate exchange/product identity from OCCT/XDE/STEP identity;
- treat a verified external STEP asset reference and its digest as model intent while keeping STEP
  bytes and imported BRep derived;
- keep contact/sweep products derived and unpersisted;
- require complete freshness validation before solve/motion results mutate model intent.

## Layer direction

```text
user interface / commands
Core parametric model intent
part feature/reference semantic identity
assembly relationship / joint / hierarchy intent
Core stable source identity
Core derived occurrence + transform-authority connectivity
Geometry semantic target resolution
Geometry source classification + typed descriptor
Geometry capability projection
relationship / joint compatibility semantics
relationship / joint equation evaluation
shared numeric residual/Jacobian execution
freshness validation + atomic application
Core derived exchange occurrence/product identity
posed geometry / diagnostics / contact / sweep / exchange consumers
OCCT geometry + XDE data-exchange kernel
file and exchange formats
```

A lower authority layer must not depend on a higher execution layer for persistent identity.

## Part and Project ownership

`PartDocument` is persistent parametric definition authority. Implemented part intent includes typed quantities/parameters, unit-aware formulas, datum/derived workplanes, construction geometry, sketches/profiles, projected/reference-driven geometry, semantic generated references/recovery, sketch constraints/diagnostics/repair helpers, additive/subtractive extrudes, `CircularHolePattern`, dependency/invalidation/recompute planning, and a deterministic collection of immutable Solid/Surface `Body` records with stable `BodyId` and visibility intent.

OCCT shapes and `ShapeCache` values are computed products. Block 49 persists Body records; Block 50
adds explicit NewBody/Join/Cut/Intersect result intent; Block 51 persists that context and connects
`body:<BodyId>` producer/consumer chains to invalidation and recompute planning. Block 52 executes
NewBody/Cut results into deterministic Body-scoped cache entries without moving shape identity into
Core. Block 53 exposes those products through checked Body metadata/summary inspection and rejects
ambiguous historical final-shape consumption. Block 54 adds persistent BodyBooleanFeature intent,
JSON, and target/tool/result graph authority. Block 55 executes those operations transactionally in
OCCT with deterministic multi-tool and incremental-recompute behavior. Block 56 adds ordered
BodyTransform and SketchOwnership intent, compatible JSON, producer advancement, and reference
dependencies. Block 57 executes those transforms transactionally and keeps cumulative Body,
owned-frame, construction-reference, semantic, and projected results derived in `ShapeCache`.
Block 58 adds the reusable Core boundary for typed Part-feature input sources, expected geometric
capabilities, feature-specific roles, dependency node identity, and read-only document validation.
Block 59 extends both Extrude families with persistent seven-mode extent intent, optional taper and
thin intent, typed limit-face references, Length-parameter validation/dependencies, and compatible
JSON. Block 60 resolves that intent into deterministic OCCT spans/tools, exact ToNext intersection,
semantic planar limits, tapered/thin solids, and the four Body operations. Exact historical
Distance/ThroughAll fast paths remain unchanged.
Block 61 adds typed persistent Revolve/RevolveCut profile-region, axis, angle/side, and Body-result
intent. Its profile/axis/body dependencies participate in the same graph and invalidation authority;
strict optional-on-read `revolve_features[]` JSON preserves older files. OCCT revolution and
self-intersection checks remain the Block-62 Geometry boundary.

`Project` owns:

```text
one explicit root AssemblyDocument
Project-owned child AssemblyDocument records
Project-owned PartDocument records
Project-owned AssemblyHierarchyConstraint records
Project-owned AssemblyHierarchyJoint records
```

Persistent Project-level cross-hierarchy fields remain:

```text
cross_hierarchy_constraints[]
cross_hierarchy_joints[]
```

A child `AssemblyDocument` is a shared model definition. It enters the rooted hierarchy only through a containing `SubassemblyInstance`.

## Component transform and hierarchy authority

A persistent `ComponentInstance` stores direct local placement/state. Derived mutation identity is:

```text
ComponentTransformAuthority =
  (assembly_document: DocumentId,
   local ComponentInstanceId)
```

Repeated rooted occurrences of one child document may expose different root-space geometry while sharing the same child-document component transform authority.

A persistent `SubassemblyInstance` stores a referenced child assembly and one authored rigid boundary transform. It is not currently a six-variable solve authority and has no grounding field.

Project structure validation rejects invalid child references, root-as-child references, hierarchy cycles, invalid component/member structure, invalid local relationship/joint endpoints, and invalid occurrence-qualified endpoint path/component structure.

`AssemblyHierarchyTraversal` derives deterministic root-first occurrence descriptors. For:

```text
root --outer--> child --inner--> grandchild
```

Grandchild context is:

```text
occurrence_path = [outer, inner]
parent_transforms_inner_to_outer = [T_inner, T_outer]
```

Every authored `RigidTransform` uses millimeters, degrees, right-hand positive rotation, active fixed-axis X-then-Y-then-Z rotation, and `Rz * Ry * Rx` column-vector composition.

For:

```text
[T_component, T_inner, T_outer]
```

point evaluation is:

```text
T_outer(T_inner(T_component(p)))
```

Directions, normals, axes, and frame vectors rotate but do not translate.

## Visible-active leaf boundary

`AssemblyLeafOccurrenceResolver` is the canonical hierarchy-to-part-leaf boundary for posed geometry consumers.

A leaf retains:

```text
containing AssemblyDocumentId
exact rooted SubassemblyInstance path
local ComponentInstanceId
referenced PartDocumentId
exact transforms_inner_to_outer
```

Hidden/suppressed hierarchy branches remove descendants. Hidden/suppressed local components are absent. The leaf boundary is deterministic and unpersisted.

## Local and cross-hierarchy solving

Persistent geometric relationship families are:

```text
Mate
Distance
Angle
Concentric
Insert
Coincident
Parallel
Perpendicular
```

All eight persistent relationship families are equation-enabled: Mate, Distance, Angle, Concentric, Insert, Coincident, Parallel, and Perpendicular. Block 39 adds capability-driven generic equations and closes Line/Axis Angle execution selected by Block 37.

Local endpoint identity:

```text
(local ComponentInstanceId,
 semantic_reference)
```

Project-level endpoint identity:

```text
(occurrence_path,
 local ComponentInstanceId,
 semantic_reference)
```

Local and cross-hierarchy solve/motion graphs derive deterministic relationship incidence over `ComponentTransformAuthority` values for all equation-enabled relationship families, including Block-39 Coincident/Parallel/Perpendicular relationships.

Each unique free active transform authority contributes:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

Local relationships evaluate in the containing assembly-document space. Project-level relationships evaluate through exact rooted chains in root-assembly space.

The shared numeric engine owns scaled residual flattening, central finite differences, damped Gauss-Newton normal equations, dense elimination, damping escalation, backtracking, and solve-state classification. There is no second cross-hierarchy optimizer.

## Reference geometry intent and semantic source identity

Block 32 adds first-class `DatumAxis` PartDocument intent with two frozen definition families:

```text
Explicit               finite origin + unit direction + parameter dependencies
FromConstructionLine   owned ConstructionLineId identity only
```

Datum axes participate in the PartDocument dependency graph and recompute planning through parameter and source-line edges.

Canonical reference source spellings are:

```text
ref:datum_plane:<encoded-id>
ref:datum_axis:<encoded-id>
ref:construction_line:<encoded-id>
ref:construction_point:<encoded-id>
```

Uppercase `%HH` escaping outside `[A-Za-z0-9_-]` keeps valid `ref:` spellings dot-free and disjoint from legacy `<feature-id>.<role>` spellings. Parsing fails closed.

Block 33 serializes DatumAxis intent through additive optional `datum_axes`, preserves historical files, validates ownership/family rules at load, and roundtrips `ref:` endpoint spellings byte-for-byte without resolving geometry.

Block 34 resolves:

```text
DatumPlane        -> Plane
DatumAxis         -> Axis + Line
ConstructionLine  -> Line
ConstructionPoint -> Point
```

Local reference targets remain component-local; hierarchy targets use the exact rooted transform chain. Canonical PartDocument snapshots remain freshness authority.

## Stable generated topology identity and recovery

Block 35 adds the Core semantic identity boundary required before arbitrary generated topology becomes an assembly target.

Canonical generated-topology endpoint spellings are:

```text
topo:cylindrical_face:<encoded-feature-id>:<encoded-profile-id>:wall
topo:linear_edge:<encoded-feature-id>:<role>
topo:circular_edge:<encoded-feature-id>:<encoded-profile-id>:source_rim
topo:circular_edge:<encoded-feature-id>:<encoded-profile-id>:opposite_rim
topo:vertex:<encoded-feature-id>:<role>
```

The `topo:` parser uses the same strict uppercase `%HH` canonical-id discipline. Valid spellings are dot-free and remain distinct from `ref:` and legacy feature-role strings.

`GeneratedTopologyReferenceIdentity` contains:

```text
SemanticCylindricalFaceReference
SemanticEdgeReference
SemanticCircularEdgeReference
SemanticVertexReference
```

The first producer matrices are finite and producer-owned:

```text
RectangularAdditiveExtrude
  exactly one RectangleProfile and no other profile
  -> 12 named linear edges, expected cardinality 1 each
  -> 8 named vertices, expected cardinality 1 each

SingleCircleSubtractiveExtrude
  exactly one CircleProfile and no other profile
  no CircularHolePattern
  direct supported rectangular additive target
  -> wall, expected cardinality 1
  -> source_rim, expected cardinality 1
  -> opposite_rim, expected cardinality 1
```

Cylindrical-face and circular-edge identity retain exact source `CircleProfileId`. Unsupported producers, mixed/multiple-circle ambiguity, wrong profile identity, and unsupported family/role pairs fail closed.

Pattern-generated subelements remain unavailable until stable persistent per-instance semantic identity exists. Transient result-vector index is not identity.

`ReferenceRecoveryEvaluator` now evaluates generated topology read-only from current PartDocument producer intent and returns `Resolved` or `Lost` plus a diagnostic message. Recovery does not mutate the PartDocument and never writes OCCT traversal/hash/map/XDE/STEP/memory identity.

Canonical contract: `docs/assembly-generated-topology-reference-mvp5.md`.

## Typed geometric target taxonomy and capability projection

Block 31 establishes the derived Geometry boundary between semantic target resolution and equation geometry.

Source kinds:

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

Capabilities in canonical order:

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
exact current transforms_inner_to_outer context
```

Projection functions are:

```text
project_plane
project_axis
project_line
project_point
project_circle
project_cylinder
project_frame
```

Every projection validates endpoint/coordinate-space consistency, source metadata, source-kind/descriptor agreement, descriptor geometry, canonical capability vector/order, and transform context before returning geometry.

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

Plane preserves the existing workplane convention: X, Y, and independently oriented face normal are finite unit and pairwise orthogonal, but Plane handedness is not constrained. Circle and Frame require right-handed orthonormal frames. Axis/Line directions are finite unit vectors. Circle/Cylinder radii are finite and positive.

Current legacy source adaptation remains:

```text
.top/.bottom/.right/.left/.front/.back -> GeneratedPlanarFace -> Plane
current .axis single-circle subtractive producer -> GeneratedCylindricalFace -> Axis + Cylinder
current .seat -> CircularFeatureSeat -> Plane + Axis + Frame
```

The legacy target strings remain unchanged. Historical local/hierarchy `resolve`, `resolve_axis`, and `resolve_insert` APIs remain compatibility adapters whose geometry originates at the typed projection boundary.

Block 36 adds `topo:` Geometry target resolution: `resolve_geometric` parses canonical generated-topology producer identity before the legacy grammar and builds Cylinder/Axis, Line, Circle/Axis/center Point, and Point descriptors analytically from validated feature/sketch/profile intent, for both component-local and exact rooted transform semantics.

Block 37 adds deterministic target compatibility selection through `AssemblyTargetCompatibilityResolver`: relationship type plus target A/B capability vectors produce one ordered capability pair or an explicit incompatibility. Existing local and cross-hierarchy Mate/Distance/Angle/Concentric/Insert equation builders consume that compatibility result before projection, without adding new equations or JSON.

Block 38 extends shared Core relationship intent and existing local/Project JSON type spellings with `Coincident`, `Parallel`, and `Perpendicular`. They reuse endpoint/order/state/id-scope semantics and carry no scalar quantity.

Block 39 adds capability-driven Coincident/Parallel/Perpendicular equations, enables their existing graph participation, reuses shared residual scaling/finite differences/Gauss-Newton/freshness/application/rank diagnostics, and closes Line/Axis Angle execution selected by Block 37.

Canonical target contract: `docs/assembly-geometric-target-taxonomy-mvp5.md`.

## Semantic target freshness

`AssemblySemanticTargetPartSnapshot` stores:

```text
part_document
canonical_model_intent_json
```

The payload is exactly `serialize_part_document_to_json(part)`.

At application, the current `PartDocument` is serialized again and compared byte-for-byte. Local solve application, flexible-child application, local Revolute motion, cross-hierarchy geometric application, and cross-hierarchy Revolute motion reuse this conservative freshness authority.

The typed target taxonomy and Block-35 identity layer add no second target revision model.

## Joint motion

Persistent local `AssemblyJoint` and Project-level `AssemblyHierarchyJoint` intent remain separate from geometric constraints.

Current families are `Revolute`, with principal-angle limits and one authored angular rotation,
`Prismatic`, with bounded signed Linear displacement and one authored translation,
`Cylindrical`, with bounded Linear translation plus bounded Angular rotation, `Planar`, with
bounded U/V Linear translations plus bounded normal Angular rotation, and passive `Spherical`,
with no coordinate slots.

Current `.seat` sources expose Frame, Axis, and Plane. `AssemblyJointTargetCompatibilityResolver` deterministically selects Frame/Frame before projection for local and cross-hierarchy Revolute, Prismatic, Cylindrical, and Planar equations. Axis-only targets fail closed because no deterministic reference X direction exists. Spherical instead requires Point/Point and contributes `point_b - point_a` center coincidence; `.seat` is not silently treated as a Point. The selected capabilities feed the family residual constructors.

```text
directed axis alignment
axis offset
signed seating separation
signed periodic twist sine
signed periodic twist cosine
```

Selected joints receive requested coordinates. Other active Revolute joints in the same combined motion closure hold authored coordinates.

Cross-hierarchy motion connectivity spans local geometry, local joints, Project cross geometry, and Project cross joints. Fresh application protects relationship records, authority inputs, current participation, hierarchy boundaries, exact PartDocument intent, and selected coordinate semantics before atomic mutation.

## Flexible child solving

`AssemblyFlexibleSubassemblySolver` selects one exact active non-root occurrence path and creates a temporary child-as-root Project view. Ordinary local solve/application is reused.

Successful application writes direct child component transforms back to the shared child `AssemblyDocument`. Selected/ancestor `SubassemblyInstance` transforms remain unchanged. Repeated occurrences of one child definition share the same internal component pose.

## Exchange and structured STEP

Derived exchange identities are:

```text
assembly occurrence
  = exact rooted SubassemblyInstance path

component occurrence
  = (containing rooted path,
     local ComponentInstanceId)

part product definition
  = referenced PartDocumentId
```

Exchange component occurrence is not transform authority.

`AssemblyExchangeGraph` derives the explicit root plus every path prefix required by one visible-active leaf. Ordering is lexicographic rooted path, then local component id, then part id.

`AssemblyPartShapeDefinitionBuilder` performs one recompute plus one private `ShapeCache` per unique exported part.

`AssemblyStructuredStepExporter` creates shared XDE part definitions, rooted assembly labels, component references, and exact parent-child assembly references. The flattened STEP path remains a compatibility consumer.

## Posed analysis and sampled motion

`AssemblyPosedLeafShapeBuilder` reuses canonical visible-active leaves, shared part definitions, and exact transform chains.

Historical compatibility analyzers remain `AssemblyInterferenceAnalyzer` and `AssemblyClearanceAnalyzer`.

`AssemblyContactAnalyzer` evaluates every visible-active unordered rooted component occurrence pair once. Pair identity is the canonical ordered pair of exact rooted component exchange identities.

Classification is:

```text
overlap_volume_mm3 > minimum_overlap_volume_mm3
  -> Interfering

otherwise minimum_distance_mm <= touching_tolerance_mm
  -> Touching

otherwise
  -> Separated
```

`AssemblyRevoluteSweepAnalyzer` supports existing root-local and Project cross-hierarchy Revolute motion. It accepts `2..1001` inclusive samples and starts every sample from a fresh source Project copy before existing motion solve/application and contact analysis.

The sweep is deterministic discrete sampling, not continuous collision detection.

## Persistence summary

Persist:

```text
PartDocument parametric intent
AssemblyDocument local model intent
ComponentInstance placement/state
SubassemblyInstance child reference/placement/state
local geometric relationships
local Revolute joints
cross_hierarchy_constraints[]
cross_hierarchy_joints[]
exact endpoint semantic-reference strings including legacy, ref:, and topo: families
```

Regenerate:

```text
dependency/recompute execution products
OCCT shapes and ShapeCache values
hierarchy traversal and transform chains
visible-active leaves
relationship/joint connectivity
ComponentTransformAuthority mappings
generated topology producer role matrices/classification/recovery query results
resolved target source kinds/source metadata
typed Plane/Axis/Line/Point/Circle/Cylinder/Frame descriptors
capability vectors and projection results
transformed target geometry
residuals/Jacobians/results
freshness snapshots/proposals/diagnostics
exchange identities/graphs/names
part shape definitions
XDE labels/component references
STEP products/entities
posed shapes/contact records/sweep analyses
```

Block 33 adds `datum_axes` PartDocument persistence. Block 35 adds canonical semantic endpoint strings but no JSON field or generated-topology cache. Block 38 adds accepted relationship type spellings without changing local or Project-level relationship record shapes. `docs/file-format.md` remains save-format authority.

## Planned STEP import authority after Block 94

Blocks 95–101 in `docs/step-import-sequence-mvp7.md` add two explicit modes without weakening the
authority model:

```text
Reference
  verified external STEP asset -> immutable imported Part definition

EditableBody
  verified external STEP asset -> ImportedBodyFeature -> persistent BodyId outputs
  -> ordinary downstream BLCAD feature intent
```

Persistent import intent is limited to a project-relative asset reference, content digest, explicit
mode, imported product/body selection, BLCAD body ids, and semantic imported-topology catalog.
STEP bytes, BRep shapes, XDE labels, STEP entity numbers, transfer maps, resolved topology, and
capability projections remain derived. Source refresh is explicit and transactional; lost or
ambiguous imported topology fails closed before Assembly or downstream Part intent is changed.

Reference Parts participate as shared `PartDocument` definitions in existing component and nested
Assembly hierarchies. EditableBody does not reconstruct foreign history: it begins BLCAD history at
an immutable `ImportedBodyFeature`. Structured STEP assembly import maps unique product definitions
to shared BLCAD Part/Assembly definitions and direct occurrences to existing instance transforms.

## Current direction

Blocks 23–47 of the Assembly sequence are implemented.

Canonical current target architecture:

- `docs/assembly-geometric-target-taxonomy-mvp5.md`
- `docs/assembly-reference-geometry-intent-mvp5.md`
- `docs/assembly-generated-topology-reference-mvp5.md`
- `docs/assembly-generic-relationship-intent-mvp5.md`
- `docs/assembly-generic-relationship-equations-mvp5.md`
- `docs/assembly-joint-target-compatibility-mvp5.md`
- `docs/assembly-joint-coordinate-model-mvp5.md`
- `docs/assembly-joint-coordinate-json-mvp5.md`
- `docs/assembly-vector-joint-drive-mvp5.md`
- `docs/assembly-prismatic-joint-mvp5.md`
- `docs/assembly-cylindrical-joint-mvp5.md`
- `docs/assembly-planar-joint-mvp5.md`
- `docs/assembly-spherical-joint-mvp5.md`
- `docs/assembly-general-geometric-target-roadmap.md`

The complete original planning baseline for Blocks 32–47 remains in `docs/assembly-general-geometric-target-roadmap-planning-baseline.md` as historical design context; implemented canonical contracts are authoritative.

Canonical numbered sequence:

- `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`
- `docs/part-construction-sequence-mvp6.md`
- `docs/part-body-identity-mvp6.md`
- `docs/part-body-json-mvp6.md`
- `docs/part-feature-body-operation-mvp6.md`
- `docs/part-feature-body-dependency-mvp6.md`
- `docs/part-multi-body-recompute-mvp6.md`
- `docs/part-multi-body-inspection-mvp6.md`
- `docs/part-body-boolean-mvp6.md`
- `docs/part-body-boolean-geometry-mvp6.md`
- `docs/part-body-transform-ownership-mvp6.md`
- `docs/part-body-transform-geometry-mvp6.md`
- `docs/part-feature-input-reference-mvp6.md`
- `docs/part-extrude-extent-intent-mvp6.md`
- `docs/part-extrude-extent-geometry-mvp6.md`
- `docs/part-revolve-intent-mvp6.md`
- `docs/part-revolve-geometry-mvp6.md`
- `docs/part-pattern-core-mvp6.md`
- `docs/part-linear-pattern-geometry-mvp6.md`
- `docs/part-circular-pattern-geometry-mvp6.md`
- `docs/part-mirror-intent-mvp6.md`
- `docs/part-mirror-geometry-mvp6.md`
- `docs/part-edge-treatment-intent-mvp6.md`
- `docs/part-fillet-geometry-mvp6.md`
- `docs/part-chamfer-geometry-mvp6.md`
- `docs/part-shell-intent-mvp6.md`
- `docs/part-shell-geometry-mvp6.md`
- `docs/part-draft-intent-mvp6.md`
- `docs/part-draft-geometry-mvp6.md`
- `docs/part-sketch-3d-core-mvp6.md`
- `docs/part-sketch-3d-curves-core-mvp6.md`
- `docs/part-sketch-3d-json-mvp6.md`
- `docs/part-sketch-3d-geometry-mvp6.md`
- `docs/step-import-sequence-mvp7.md`

Block 47 Spherical completes the Assembly MVP sequence. Blocks 48–78 Body identity, body-scoped
recompute/inspection, Body Booleans, associative BodyTransform/SketchOwnership execution, and
reusable Part-feature semantic input references plus richer Extrude/Cut intent/Geometry and
persistent plus executed Revolve/RevolveCut, general Pattern intent plus Geometry, and persistent
plus executed MirrorFeature Geometry, persistent plus executed Fillet/Chamfer/Shell Geometry, and
persistent plus executed DraftFeature Geometry and model-space 3D Sketch Core intent through
Arc, Spline, Helix, Guide Curve, their deterministic JSON/source-reference grammar, and transient
deterministic OCCT Geometry conversion are implemented. The next technical step is Block 79
connected PathCurve Core intent, JSON, and validation.

Block 47 adds passive Point/Point Spherical intent through the shared local/root-space path. Scalar
Revolute APIs remain adapters; transform variables and the shared numeric engine are unchanged.
Axis-only oriented joints still fail closed without arbitrary reference-direction synthesis. A
driven spherical orientation, occurrence-local child pose overrides, whole-subassembly solve
variables, and general physics remain deferred.

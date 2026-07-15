# Architecture Summary

This document summarizes implemented BLCAD architecture and the current technical direction.
Feature-specific documents remain canonical for exact formulas, save-format spellings,
validation/failure policy, ordering, and focused proof.

## Goal and authority model

BLCAD is an independent parametric CAD system for Linux. BLCAD owns semantic model intent;
OCCT/Open CASCADE is the computed geometry and data-exchange kernel.

Fundamental decisions are:

- persist parametric and semantic intent rather than final BRep authority;
- persist stable semantic references rather than raw OCCT topology ids;
- use stable shared planar point identity rather than equal coordinates for Sketch connectivity;
- allow distinct point identities to occupy the same coordinate when topology does not connect them;
- keep Core model intent below Geometry query/execution types;
- separate stable semantic source identity from Geometry topology lookup;
- separate semantic source classification from geometric capability projection;
- separate direct component transform authority from rooted occurrence identity;
- separate authored hierarchy boundaries from derived traversal/composition;
- separate geometric relationships from motion-joint intent;
- separate exchange/product identity from OCCT/XDE/STEP identity;
- keep viewport pixels, hover, grid, hit stacks, snap candidates, and interaction samples transient;
- use explicit migration when a persistent identity model supersedes a historical representation;
- require complete freshness validation before solve/motion results mutate model intent.

A lower authority layer must not depend on a higher execution or presentation layer for persistent
identity.

## Layer direction

```text
Qt user interface / commands / transient interaction
Core parametric model intent
Core planar Sketch topology identity
Part feature/reference semantic identity
Assembly relationship / joint / hierarchy intent
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

The optional GUI is a client of Core and Geometry. Qt types stay out of `blcad_core`; widgets never
own BRep, constraint mathematics, transform authority, expressions, recompute, or persistent Sketch
topology.

## PartDocument and Part construction authority

`PartDocument` is persistent parametric definition authority. Implemented Part intent includes typed
quantities/parameters, formulas, datum and derived workplanes, construction geometry, historical
planar Sketch/profile records, projected/reference-driven Sketch geometry, Sketch constraints and
dimensions, Body identity, feature history, multi-body result operations, broad solid features,
Sketch3D/PathCurve, Sweep, path Extrude, Loft, Surface features, closed-shell-to-solid conversion,
and multi-body STEP export intent.

Stable `BodyId` and persistent Body result intent let recompute publish deterministic Body-scoped
products. Feature producers and consumers participate in dependency/invalidation planning. Core does
not persist `TopoDS_Shape` values.

`ShapeCache` and OCCT shapes are derived. Recompute validates current model intent, executes Geometry
adapters, and publishes checked Body/feature products. Consumers requiring fresh results use current
recompute/freshness boundaries rather than historical final-shape assumptions.

## Shared planar Sketch topology through Block 108

The historical planar `Sketch` representation embeds `Point2` coordinates directly in line, arc,
spline, and profile-center records. That representation remains load-compatible and remains the
compatibility carrier used by current Geometry consumers.

Block 108 adds the canonical topology identity model used by future solver and direct-manipulation
consumers:

```text
SketchTopology
  SketchId
  points[]
  entities[]
  dependencies[]
```

`SketchPointId` is stable planar point identity. `SketchTopologyPoint` stores:

```text
id
position : Point2
construction
reference
```

`SketchTopologyEntity` stores:

```text
id
kind
ordered point references
ordered entity dependencies
construction
reference
```

Implemented topology kinds cover current Line, Arc, Spline, RectangleProfile, CircleProfile,
ClosedProfile, ArcClosedProfile, CompositeClosedProfile, CircularHolePattern, ProjectedPoint,
ProjectedLine, and ReferenceGeneratedLine intent.

Line uses two point references, Arc three, Spline four, and current rectangle/circle/hole-pattern
profile intent one center point. Closed-profile families retain ordered curve dependencies instead of
duplicating endpoint coordinates. Reference entities do not gain invented resolved coordinates.

Global point and entity collections are canonical lexicographic id order. Defining point roles and
ordered profile dependencies remain semantic order and are never sorted.

### Point identity is not coordinate equality

Two `SketchTopologyPoint` records may have equal coordinates and different `SketchPointId` values.
This is deliberate. Block 109 must be able to model two logically distinct points constrained by
`Coincident`; merging every equal coordinate would erase one solver variable and the constraint
semantics before solving.

Shared identity is established by explicit topology. Multiple entities may reference the same
`SketchPointId` when they own one topological junction.

### Deterministic legacy migration

`SketchTopology::migrate_legacy(...)` projects historical Sketch records into canonical topology.
Stable topology ids use:

```text
entity/<SketchEntityId>
profile/<ProfileId>
```

Point usages propose owner/role ids such as:

```text
entity/line.web/start
entity/line.web/end
entity/arc.outer/mid
profile/profile.hole/center
```

Candidates are processed by canonical topology id. Migration derives shared point equivalence only
from explicit ordered historical profile connectivity:

```text
ClosedProfile
ArcClosedProfile
CompositeClosedProfile outer contour
each CompositeClosedProfile inner contour
```

For each supported editable curve in a contour, current-end and next-start usages are connected,
including last-to-first closure. Their legacy coordinates must agree within `1e-9` and flags must be
compatible. The canonical id of a connected usage group is the lexicographically smallest proposed
point id in that group.

`SketchTopologyMigrationReport` records every non-canonical endpoint usage collapsed into one shared
point. For example, a connected triangle contains six historical endpoint usages but its ordered
closed profile defines three topological junctions, so migration produces three points and three
identity-change records independent of line insertion order.

Two unrelated line endpoints both at `(20, 0)` and without an explicit profile-connectivity relation
remain two different point ids.

Reference and construction are explicit topology flags. `reference && construction` is invalid.
Historical projected points, projected lines, and reference-generated lines migrate as reference
entities and are read-only for Block-108 edit commands.

### Topology dependency records

`SketchTopologyDependency` contains:

```text
consumer_id
source_entity_id
role
```

Legacy migration derives these records from reference constraints, geometric constraints, driving
dimensions, trim/extend operations, tangent continuity, and profile topology references. The table is
canonical consumer/source/role order and provides Block-108 entity-deletion authority. It is not a
replacement for the PartDocument-wide dependency graph.

### Editable Core commands and exact undo

`SketchEditCommandExecutor` implements:

```text
AddPoint
AddEntity
MovePoint
ReplaceEntity
RemovePoint
RemoveEntity
```

Every command executes on disposable point/entity/dependency collections. A candidate is published
only after full `SketchTopology::create(...)` validation.

Add rejects duplicate ids but allows a distinct point id at an already used coordinate. Move addresses
one `SketchPointId`, rejects missing/reference points and non-finite coordinates, and may move one
point into numeric coincidence with another without merging identities. If the point is shared by
connected entities, every connected entity still references the same moved id.

Replace preserves the entity id and validates all point/entity references. Remove rejects points still
referenced by an entity and entities with active dependency consumers.

Every successful command produces one `SketchEditTransaction` containing exact complete `before` and
`after` canonical topology snapshots. `SketchTopologyUndoStack` returns those snapshots directly for
undo and redo; it does not synthesize inverse commands heuristically.

### Canonical topology JSON and historical PartDocument compatibility

Block 108 adds a separate topology schema:

```text
schema  = blcad.sketch_topology.mvp8
version = 1
```

It persists stable point ids and coordinates, point flags, entity id/kind, ordered point references,
ordered entity dependencies, entity flags, and canonical dependency records. Distinct equal-coordinate
point ids are valid and round-trip exactly.

The historical PartDocument marker remains `blcad.part_document.mvp1`. Block 108 does not reinterpret
that schema as if shared point ids had always existed. Instead:

```text
migrate_legacy_part_document_sketch_json(...)
  -> historical PartDocument loader
  -> selected Sketch
  -> SketchTopology::migrate_legacy(...)
  -> canonical topology + migration report
```

`SketchTopologyLegacyMaterializer` and `SketchTopologyPartDocumentEditor` provide a lossless bridge
for topology edits that current PartDocument Sketch records can represent:

```text
current PartDocument Sketch
  -> migrate
  -> edit candidate topology
  -> materialize historical Sketch candidate
  -> re-migrate
  -> require exact topology equality
  -> PartDocument::update_sketch(...)
```

Only exact equality reaches the existing atomic `PartDocument::update_sketch(...)` authority. An edit
whose point identity, flags, explicit orphan records, or ordered topology would be lost by historical
materialization fails closed. Such topology remains fully representable in
`blcad.sketch_topology.mvp8` rather than being silently flattened.

Canonical contract: `docs/sketch-shared-topology-mvp8.md`.

## Semantic Part-feature input and generated topology identity

Part features use typed semantic input references and expected geometric capabilities instead of raw
kernel subshape ids.

Current reference-source spellings are:

```text
ref:datum_plane:<encoded-id>
ref:datum_axis:<encoded-id>
ref:construction_line:<encoded-id>
ref:construction_point:<encoded-id>
```

Canonical generated-topology spellings include cylindrical-face, linear-edge, circular-edge, and
vertex `topo:` producer-role identities. Uppercase `%HH` escaping outside `[A-Za-z0-9_-]` keeps the
semantic grammar canonical and disjoint from historical `<feature-id>.<role>` spellings.

The first generated-topology producer matrices are producer-owned and finite:

```text
RectangularAdditiveExtrude
  -> 12 named linear edges, expected cardinality 1 each
  -> 8 named vertices, expected cardinality 1 each

SingleCircleSubtractiveExtrude
  -> cylindrical wall, expected cardinality 1
  -> source_rim, expected cardinality 1
  -> opposite_rim, expected cardinality 1
```

Pattern result-vector position is not persistent semantic identity. `ReferenceRecoveryEvaluator`
evaluates producer/profile/role intent read-only and never writes OCCT/XDE/STEP/memory identity into
the model.

## Project, AssemblyDocument, and hierarchy ownership

`Project` owns one explicit root `AssemblyDocument`, child AssemblyDocument records, PartDocument
records, cross-hierarchy constraints, and cross-hierarchy joints. A child AssemblyDocument is a shared
model definition and enters rooted hierarchy only through a `SubassemblyInstance`.

A persistent `ComponentInstance` stores direct local placement/state. Derived mutation identity is:

```text
ComponentTransformAuthority =
  (assembly_document: DocumentId,
   local ComponentInstanceId)
```

Repeated rooted occurrences of one child may expose different root-space geometry while sharing the
same child-document component transform authority.

`AssemblyHierarchyTraversal` derives deterministic root-first occurrence descriptors and exact
transform chains. `AssemblyLeafOccurrenceResolver` is the canonical hierarchy-to-Part leaf boundary
for posed geometry consumers. Hidden/suppressed branches and local components are absent.

## Typed Assembly geometric targets

The Geometry boundary between semantic target resolution and equation geometry uses source
classification plus canonical capability projection.

Current source kinds include generated planar/cylindrical faces, generated linear/circular edges,
generated vertices, DatumPlane, DatumAxis, ConstructionLine, ConstructionPoint, and
CircularFeatureSeat.

Capabilities in canonical order are:

```text
Plane
Axis
Line
Point
Circle
Cylinder
Frame
```

Projection uses `project_plane`, `project_axis`, `project_line`, `project_point`, `project_circle`,
`project_cylinder`, and `project_frame`. Every projection validates endpoint/coordinate-space
consistency, source metadata, source-kind/descriptor agreement, descriptor geometry, canonical
capability order, and transform context.

Legacy `.top/.bottom/.right/.left/.front/.back`, `.axis`, and `.seat` target strings remain accepted
through compatibility adapters whose geometry originates at the typed projection boundary.

## Assembly relationships, solving, and freshness

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

All eight are equation-enabled. Local endpoint identity is `(ComponentInstanceId,
semantic_reference)`; Project-level endpoint identity adds the exact occurrence path.

Local and cross-hierarchy solve/motion graphs derive deterministic relationship incidence over
`ComponentTransformAuthority` values. The shared Assembly numeric engine owns scaled residual
flattening, central finite differences, damped Gauss-Newton normal equations, dense elimination,
damping escalation, backtracking, and solve-state classification.

Assembly semantic-target freshness stores exact canonical Part JSON used during solve/motion and
compares it byte-for-byte at application. Local/cross-hierarchy solve and joint-motion application
reuse this conservative freshness authority.

The Block-109 planar Sketch solver is a separate upcoming Core solver over `SketchTopology`. It must
not reuse Assembly six-DOF transform variables or move Assembly authority into Sketches.

## Assembly joints, exchange, and analysis

Implemented joint families are Revolute, Prismatic, Cylindrical, Planar, and passive Spherical.
Oriented joint families use deterministic Frame/Frame compatibility; Spherical uses Point/Point
center coincidence.

Derived exchange identity distinguishes rooted assembly occurrence, rooted component occurrence, and
shared Part product definition. Structured STEP export creates shared XDE Part definitions, rooted
assembly labels, component references, and exact parent-child assembly references. The flattened STEP
path remains a compatibility consumer.

Current analysis consumers include interference, clearance, rooted pair contact classification, and
bounded sampled Revolute sweep. The sweep is deterministic discrete sampling, not continuous
collision detection.

## Optional GUI and Interactive Sketch foundations

Blocks 95–105 implement and accept the optional Qt 6 desktop for exercising Core/Geometry features
through Block 94. The GUI owns transient session, selection, task, command-enable, and presentation
state. Document lifecycle, candidate transactions, undo/redo, recompute, and diagnostics are mediated
by `GuiDocumentSession`.

Block 106 introduces contextual `GuiWorkspace::Sketch` and the staged command lifecycle. Block 107
adds a read-only plane interaction path from historical Sketch intent through
`GuiSketchInteractionSceneBuilder`, `GuiSketchPlaneMapping`, and
`GuiSketchInteractionController` to viewport overlay/status/semantic selection.

Block 108 does not move interaction mathematics into Core. It adds stable persistent point/entity
identity for Block 109 and later direct manipulation. The current Block-107 scene builder still
projects the historical `Sketch` compatibility representation; it does not infer `SketchPointId` from
screen-space candidates or coordinate equality.

Canonical Interactive Sketch contracts are:

- `docs/gui-interactive-sketch-workspace-mvp8.md`
- `docs/gui-sketch-plane-interaction-mvp8.md`
- `docs/sketch-shared-topology-mvp8.md`

The current next technical step is Block 109: deterministic general planar constraint solving over
Block-108 topology, including variable ordering, scaling, convergence policy, DOF accounting, and
stable redundant/conflicting/non-convergent/invalid-reference diagnostics.

## Persistence summary

Persist:

```text
PartDocument parametric and feature intent
historical PartDocument planar Sketch compatibility records
blcad.sketch_topology.mvp8 canonical point/entity topology
SketchPointId and point coordinates
Sketch point/entity construction and reference flags
ordered topology point references and profile dependencies
canonical topology dependency records
Body records and result-operation intent
AssemblyDocument local model intent
component/subassembly placement and state
local and cross-hierarchy relationships/joints
semantic reference spellings including legacy, ref:, and topo: families
```

Regenerate or adapt:

```text
dependency/recompute execution products
OCCT shapes and ShapeCache values
hierarchy traversal and transform chains
relationship/joint connectivity and transform-authority mappings
generated-topology role/recovery query results
typed target descriptors/capability projections
residuals/Jacobians/solve results
freshness snapshots/proposals/diagnostics
exchange identities/graphs/names
XDE/STEP products and entities
posed shapes/contact records/sweep analyses
Sketch interaction scenes and sampled curves
screen/view-ray/workplane mappings
hover, hit cycles, box selection, grid, snap, inference
legacy profile-connectivity migration groups and identity-change reports
PartDocument compatibility materialization candidates
future planar Sketch solver variables/residuals/results
```

`docs/file-format.md` remains authority for historical PartDocument/Project schemas.
`docs/sketch-shared-topology-mvp8.md` is the exact persistence/migration authority for
`blcad.sketch_topology.mvp8`.

## Current direction and canonical documents

Implemented numbered phases currently extend through Block 108.

Canonical sequence and architecture documents include:

- `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`
- `docs/part-construction-sequence-mvp6.md`
- `docs/gui-feature-validation-sequence-mvp7.md`
- `docs/interactive-sketcher-sequence-mvp8.md`
- `docs/gui-interactive-sketch-workspace-mvp8.md`
- `docs/gui-sketch-plane-interaction-mvp8.md`
- `docs/sketch-shared-topology-mvp8.md`
- `docs/interactive-modeling-sequence-mvp9.md`
- `docs/step-import-sequence-mvp10.md`

Block 109 is the current implementation boundary. Interactive Part/Surface/Assembly modeling begins
after Interactive Sketcher acceptance in Block 122. STEP Import begins in Block 132.

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
- use stable shared planar point identity rather than equal coordinates for Interactive Sketcher
  connectivity;
- keep Core model intent below Geometry query/execution types;
- separate stable semantic source identity from Geometry topology lookup;
- separate semantic source classification from geometric capability projection;
- separate direct component transform authority from rooted occurrence identity;
- separate authored hierarchy boundaries from derived traversal/composition;
- separate geometric relationships from motion-joint intent;
- separate exchange/product identity from OCCT/XDE/STEP identity;
- keep viewport pixels, hover, grid, hit stacks, snap candidates, and interaction samples transient;
- use explicit migration when a new persistent identity model supersedes a historical representation;
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

`PartDocument` is persistent parametric definition authority. Implemented Part intent includes:

```text
typed quantities and parameters
unit-aware expressions
datum and derived workplanes
construction geometry
historical planar Sketch/profile records
projected/reference-driven Sketch geometry
Sketch constraints, dimensions, diagnostics, and repair intent
Body identity and visibility
feature history and Body operation/result intent
Extrude / ExtrudedCut
Revolve / RevolveCut
Linear / Circular Pattern
MirrorFeature
Fillet / Chamfer
Shell / Draft
Body Boolean / BodyTransform / SketchOwnership
Sketch3D and model-space curves
PathCurve
Sweep / SweepCut / SweepSurface
path-following Extrude / ExtrudedCut
Loft / LoftCut / LoftSurface
Boundary / Fill Surface
Surface Trim / Extend
Stitch / Knit / Sew shell
closed-shell-to-solid conversion
```

Stable `BodyId` and persistent Body result intent let recompute publish deterministic Body-scoped
products. Feature producers and consumers participate in dependency/invalidation planning. Core does
not persist `TopoDS_Shape` values.

`ShapeCache` and OCCT shapes are derived. Recompute validates current model intent, executes Geometry
adapters, and publishes checked Body/feature products. Consumers that require a fresh result use the
current recompute/freshness boundaries rather than historical final-shape assumptions.

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

`SketchPointId` is stable planar point identity. A `SketchTopologyPoint` stores:

```text
id
position : Point2
construction
reference
```

A `SketchTopologyEntity` stores:

```text
id
kind
ordered point references
ordered entity dependencies
construction
reference
```

Implemented topology kinds cover existing:

```text
Line
Arc
Spline
RectangleProfile
CircleProfile
ClosedProfile
ArcClosedProfile
CompositeClosedProfile
CircularHolePattern
ProjectedPoint
ProjectedLine
ReferenceGeneratedLine
```

Defining point arity is explicit: Line uses two shared points, Arc three, Spline four, and current
rectangle/circle/hole-pattern profile intent uses one center point. Closed-profile families retain
ordered curve dependencies instead of duplicating endpoint coordinates. Projected and
reference-generated entities remain explicit reference intent and do not gain invented resolved
coordinates.

Global points and entities are canonical lexicographic id order. Defining point roles and ordered
profile dependencies remain semantic order and are never sorted.

### Deterministic legacy migration

`SketchTopology::migrate_legacy(...)` projects the historical Sketch records into canonical topology.
Stable topology ids use:

```text
entity/<SketchEntityId>
profile/<ProfileId>
```

Point usages propose ids from the owner topology id and role, for example:

```text
entity/line.web/start
entity/line.web/end
entity/arc.outer/mid
profile/profile.hole/center
```

Candidates are processed by canonical topology id. Same-flag point usages whose coordinates agree
within `1e-9` share one canonical `SketchPointId`. `SketchTopologyMigrationReport` records every
collapsed legacy usage as:

```text
previous_identity
canonical_identity
reason
```

For example, a connected triangle historically contains six line endpoint usages. Migration produces
three canonical point records and three explicit identity-change records. Entity insertion order does
not change the topology or migration report.

Reference and construction are explicit topology flags. `reference && construction` is invalid.
Historical projected points, projected lines, and reference-generated lines migrate as reference
entities. Reference topology is read-only for Block-108 mutation commands.

### Topology dependency records

`SketchTopologyDependency` contains:

```text
consumer_id
source_entity_id
role
```

Legacy migration derives these records from existing reference constraints, geometric constraints,
driving dimensions, trim/extend operations, tangent continuity, and profile topology references.
The table is canonical consumer/source/role order and provides Block-108 deletion authority. It is
not a replacement for the PartDocument-wide dependency graph.

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

Add rejects duplicate ids and duplicate same-flag coincident point identities. Move addresses one
`SketchPointId`, rejects reference points and coordinate identity collisions, and therefore moves one
shared junction for every connected entity that references the same point id. Replace preserves the
entity id and validates all point/entity references. Remove rejects points still referenced by an
entity and entities with active dependency consumers.

Every successful command produces one `SketchEditTransaction` containing exact complete `before` and
`after` canonical topology snapshots. `SketchTopologyUndoStack` returns those snapshots directly for
undo and redo; it does not synthesize inverse commands heuristically.

### Canonical topology JSON and historical PartDocument compatibility

Block 108 adds a separate canonical topology schema:

```text
schema  = blcad.sketch_topology.mvp8
version = 1
```

It persists stable point ids and coordinates, point flags, entity id/kind, ordered point references,
ordered entity dependencies, entity flags, and canonical dependency records.

The historical PartDocument marker remains `blcad.part_document.mvp1`. Block 108 does not silently
reinterpret that schema as if shared point ids had always existed. Instead:

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
whose point identity, flags, or ordered topology would be lost by historical materialization fails
closed. Such topology remains fully representable in `blcad.sketch_topology.mvp8` rather than being
silently flattened.

Canonical contract: `docs/sketch-shared-topology-mvp8.md`.

## Semantic Part-feature input and generated topology identity

Part features use typed semantic input references and expected geometric capabilities instead of raw
kernel subshape ids.

Current reference-source spellings include:

```text
ref:datum_plane:<encoded-id>
ref:datum_axis:<encoded-id>
ref:construction_line:<encoded-id>
ref:construction_point:<encoded-id>
```

Canonical generated-topology spellings include:

```text
topo:cylindrical_face:<encoded-feature-id>:<encoded-profile-id>:wall
topo:linear_edge:<encoded-feature-id>:<role>
topo:circular_edge:<encoded-feature-id>:<encoded-profile-id>:source_rim
topo:circular_edge:<encoded-feature-id>:<encoded-profile-id>:opposite_rim
topo:vertex:<encoded-feature-id>:<role>
```

Uppercase `%HH` escaping outside `[A-Za-z0-9_-]` keeps the semantic grammar canonical and disjoint
from historical `<feature-id>.<role>` spellings.

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

Pattern result-vector position is not persistent semantic identity. Pattern-generated subelements
remain unavailable until stable persistent per-instance identity exists.

`ReferenceRecoveryEvaluator` evaluates current producer/profile/role intent read-only and returns
`Resolved` or `Lost` plus diagnostics. Recovery never writes OCCT traversal/hash/map, XDE, STEP,
memory, or viewport identity into the model.

## Project, AssemblyDocument, and hierarchy ownership

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

A child `AssemblyDocument` is a shared model definition and enters the rooted hierarchy only through
an authored `SubassemblyInstance`.

A persistent `ComponentInstance` stores direct local placement/state. Derived mutation identity is:

```text
ComponentTransformAuthority =
  (assembly_document: DocumentId,
   local ComponentInstanceId)
```

Repeated rooted occurrences of one child document may expose different root-space geometry while
sharing the same child-document component transform authority.

A `SubassemblyInstance` stores a referenced child assembly and one authored rigid boundary transform.
It is not a six-variable solve authority and has no grounding field.

`AssemblyHierarchyTraversal` derives deterministic root-first occurrence descriptors. For:

```text
root --outer--> child --inner--> grandchild
```

Grandchild context is:

```text
occurrence_path = [outer, inner]
parent_transforms_inner_to_outer = [T_inner, T_outer]
```

Every authored `RigidTransform` uses millimeters, degrees, right-hand positive rotation, active
fixed-axis X-then-Y-then-Z rotation, and `Rz * Ry * Rx` column-vector composition. For:

```text
[T_component, T_inner, T_outer]
```

point evaluation is:

```text
T_outer(T_inner(T_component(p)))
```

Directions, normals, axes, and frame vectors rotate but do not translate.

`AssemblyLeafOccurrenceResolver` is the canonical hierarchy-to-Part leaf boundary for posed geometry
consumers. Hidden/suppressed hierarchy branches and local components are absent. Leaf traversal is
deterministic and unpersisted.

## Typed Assembly geometric targets

The derived Geometry boundary between semantic target resolution and equation geometry uses source
classification plus canonical capability projection.

Current source kinds are:

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

One resolved target retains exact local or occurrence-qualified endpoint identity, source kind,
derived source metadata, one typed descriptor variant, canonical capability vector, coordinate space,
and exact current transform context.

Canonical source capability mapping is:

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

All eight relationship families are equation-enabled.

Local endpoint identity is:

```text
(local ComponentInstanceId,
 semantic_reference)
```

Project-level endpoint identity is:

```text
(occurrence_path,
 local ComponentInstanceId,
 semantic_reference)
```

Local and cross-hierarchy solve/motion graphs derive deterministic relationship incidence over
`ComponentTransformAuthority` values. Each unique free active transform authority contributes six
translation/rotation variables.

The shared Assembly numeric engine owns scaled residual flattening, central finite differences,
damped Gauss-Newton normal equations, dense elimination, damping escalation, backtracking, and
solve-state classification. There is no second cross-hierarchy optimizer.

`AssemblySemanticTargetPartSnapshot` stores exact canonical Part JSON used during solve/motion. At
application, current Part intent is serialized again and compared byte-for-byte. Local solve,
flexible-child application, local joint motion, cross-hierarchy geometric application, and
cross-hierarchy joint motion reuse this conservative freshness authority.

The Block-109 planar Sketch solver is a separate upcoming Core solver over `SketchTopology`; it must
not reuse Assembly transform variables or move six-DOF Assembly authority into Sketches.

## Assembly joints and motion

Persistent local `AssemblyJoint` and Project-level `AssemblyHierarchyJoint` intent remains separate
from geometric relationships.

Implemented joint families are:

```text
Revolute    bounded Angular coordinate
Prismatic   bounded Linear coordinate
Cylindrical bounded Linear + Angular coordinates
Planar      bounded U/V Linear + normal Angular coordinates
Spherical   passive Point/Point center coincidence
```

Current `.seat` sources expose Frame, Axis, and Plane. Oriented joint families use deterministic
Frame/Frame compatibility and projection. Axis-only targets fail closed because no deterministic
reference X direction exists. Spherical requires Point/Point and contributes center coincidence.

Cross-hierarchy motion connectivity spans local geometry, local joints, Project cross geometry, and
Project cross joints. Fresh application protects relationship records, transform-authority inputs,
current participation, hierarchy boundaries, exact Part intent, and selected coordinate semantics
before atomic mutation.

`AssemblyFlexibleSubassemblySolver` selects one exact active non-root occurrence path and creates a
temporary child-as-root Project view. Successful application writes direct child component transforms
back to the shared child document; selected and ancestor `SubassemblyInstance` transforms remain
unchanged.

## Exchange, posed geometry, and analysis

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

`AssemblyExchangeGraph` derives the explicit root plus every path prefix required by a visible-active
leaf. `AssemblyPartShapeDefinitionBuilder` performs one recompute plus one private `ShapeCache` per
unique exported Part.

`AssemblyStructuredStepExporter` creates shared XDE Part definitions, rooted assembly labels,
component references, and exact parent-child assembly references. The flattened STEP path remains a
compatibility consumer.

Current analysis consumers include interference, clearance, rooted pair contact classification, and
bounded sampled Revolute sweep. `AssemblyRevoluteSweepAnalyzer` starts every sample from a fresh
source Project copy. This is deterministic discrete sampling, not continuous collision detection.

## Optional GUI and Interactive Sketch foundations

Blocks 95–105 implement and accept the optional Qt 6 desktop for exercising all Core/Geometry feature
families through Block 94. The GUI owns transient session, selection, task, command-enable, and
presentation state. Document lifecycle, candidate transactions, undo/redo, recompute, and structured
diagnostics are mediated by `GuiDocumentSession`.

The GUI interaction path is:

```text
Qt widgets and command/task state
  -> GUI document transaction
  -> public Core intent and persistence
  -> recompute / solve / analysis / exchange APIs
  -> Geometry results
  -> transient OCCT presentations and semantic picking
```

Block 106 introduces the contextual transient `GuiWorkspace::Sketch` and staged command lifecycle.
Enter/Finish Sketch capture and restore workspace, semantic selection, camera bookmark, selection
filter, and transient Sketch focus.

Block 107 adds one read-only plane-interaction authority:

```text
persistent Part + historical Sketch intent
  -> GuiSketchInteractionSceneBuilder
  -> transient plane-space interaction scene
  -> GuiSketchPlaneMapping
  -> GuiSketchInteractionController
  -> OcctViewport overlay / semantic selection
  -> GuiSketchWorkspaceStatus + GuiSelectionModel
```

`GuiSketchPlaneMapping` is the conversion boundary between Qt device-independent pixels, model-space
view rays, active workplane coordinates, and model-space points. Hit priority is
`Point -> Curve -> Dimension -> Glyph`; empty-space drag is Window/Crossing selection; grid, snap, and
inference are transient.

Block 108 does not move this interaction mathematics into Core. Instead it introduces the stable
persistent point/entity topology that Block 109 and later direct-manipulation blocks consume. The
current Block-107 scene builder still projects the historical `Sketch` compatibility representation;
it does not falsely infer `SketchPointId` from screen-space candidates.

Canonical Interactive Sketch contracts are:

- `docs/gui-interactive-sketch-workspace-mvp8.md`
- `docs/gui-sketch-plane-interaction-mvp8.md`
- `docs/sketch-shared-topology-mvp8.md`

The current next technical step is Block 109: deterministic general planar constraint solving over
Block-108 topology, including variable ordering, scaling, convergence policy, DOF accounting, and
stable redundant/conflict/invalid-reference diagnostics.

## Persistence summary

Persist:

```text
PartDocument parametric and feature intent
historical PartDocument planar Sketch compatibility records
blcad.sketch_topology.mvp8 canonical shared point/entity topology
SketchPointId and point coordinates
Sketch point/entity construction and reference flags
ordered topology point references and profile dependencies
canonical topology dependency records
Body records and result-operation intent
AssemblyDocument local model intent
ComponentInstance placement/state
SubassemblyInstance child reference/placement/state
local geometric relationships
local joint intent and coordinates
cross_hierarchy_constraints[]
cross_hierarchy_joints[]
semantic reference spellings including legacy, ref:, and topo: families
```

Regenerate or adapt:

```text
dependency/recompute execution products
OCCT shapes and ShapeCache values
hierarchy traversal and transform chains
visible-active leaves
relationship/joint connectivity
ComponentTransformAuthority mappings
generated-topology producer role matrices and recovery query results
resolved target source kinds and typed descriptors
capability vectors and projection results
transformed target geometry
residuals/Jacobians/solve results
freshness snapshots/proposals/diagnostics
exchange identities/graphs/names
Part shape definitions
XDE labels/component references
STEP products/entities
posed shapes/contact records/sweep analyses
Sketch interaction scenes and sampled curves
screen/view-ray/workplane mappings
hover and stacked-hit state
Window/Crossing rectangles and interaction selection products
grid display products
snap and inference candidates/results
legacy Sketch-topology migration identity-change reports
PartDocument compatibility materialization candidates
future planar Sketch solver variables/residuals/results
```

`docs/file-format.md` remains authority for historical PartDocument/Project schemas.
`docs/sketch-shared-topology-mvp8.md` is the exact persistence and migration authority for
`blcad.sketch_topology.mvp8`.

## Planned STEP import authority after Block 131

Blocks 132–138 add explicit `Reference` and `EditableBody` import modes. Reference keeps a verified
external STEP source as immutable geometry authority while allowing Assembly use through stable
semantic imported topology. EditableBody begins BLCAD history at an immutable `ImportedBodyFeature`
and permits supported downstream BLCAD feature intent.

STEP bytes, BRep shapes, XDE labels, STEP entity numbers, transfer maps, resolved topology, and
capability projections remain derived. Source refresh is explicit and transactional; lost or
ambiguous imported topology fails closed before Assembly or downstream Part intent is changed.

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
- `docs/assembly-geometric-target-taxonomy-mvp5.md`
- `docs/assembly-reference-geometry-intent-mvp5.md`
- `docs/assembly-generated-topology-reference-mvp5.md`
- `docs/assembly-generic-relationship-intent-mvp5.md`
- `docs/assembly-generic-relationship-equations-mvp5.md`
- `docs/assembly-joint-target-compatibility-mvp5.md`
- `docs/assembly-joint-coordinate-model-mvp5.md`
- `docs/assembly-joint-coordinate-json-mvp5.md`
- `docs/assembly-vector-joint-drive-mvp5.md`

Block 109 is the current implementation boundary. Interactive Part/Surface/Assembly modeling begins
after Interactive Sketcher acceptance in Block 122. STEP Import begins in Block 132.

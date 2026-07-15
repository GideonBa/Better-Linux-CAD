# Architecture Summary

This document summarizes the implemented BLCAD architecture and current technical direction.
Feature-specific documents remain canonical for exact formulas, save-format spellings,
validation/failure policy, ordering, and focused proof.

## Goal and authority model

BLCAD is an independent parametric CAD system for Linux. BLCAD owns semantic model intent;
OCCT/Open CASCADE is the computed geometry and data-exchange kernel.

Fundamental decisions are:

- persist parametric and semantic intent rather than final BRep authority;
- persist stable semantic references rather than raw OCCT topology ids;
- keep Core model intent below Geometry query/execution types;
- separate stable semantic source identity from Geometry topology lookup;
- separate semantic source classification from geometric capability projection;
- separate direct component transform authority from rooted occurrence identity;
- separate authored hierarchy boundaries from derived traversal/composition;
- separate geometric relationships from motion-joint intent;
- separate exchange/product identity from OCCT/XDE/STEP identity;
- treat a verified external STEP asset reference and digest as future import intent while keeping
  imported bytes/BRep derived;
- keep viewport pixels, hover, grid, hit stacks, snap candidates, and interaction samples transient;
- require complete freshness validation before solve/motion results mutate model intent.

A lower authority layer must not depend on a higher execution or presentation layer for persistent
identity.

## Layer direction

```text
Qt user interface / commands / transient interaction
Core parametric model intent
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
own BRep, solver, transform, expression, recompute, or persistent Sketch-topology authority.

## PartDocument authority

`PartDocument` is persistent parametric definition authority. Implemented Part intent includes:

```text
typed quantities and parameters
unit-aware expressions
datum and derived workplanes
construction geometry
planar Sketches and profiles
projected/reference-driven Sketch geometry
semantic generated references and recovery
Sketch constraints, diagnostics, and repair intent
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
a reusable connected PathCurve
Sweep / SweepCut / SweepSurface
path-following Extrude / ExtrudedCut
Loft / LoftCut / LoftSurface
Boundary / Fill Surface
Surface Trim / Extend
Stitch / Knit / Sew shell
closed-shell-to-solid conversion
```

Stable `BodyId` and persistent Body result intent let recompute publish deterministic Body-scoped
products. Feature producers and consumers participate in dependency/invalidation planning. The Core
model does not persist `TopoDS_Shape` values.

`ShapeCache` and OCCT shapes are derived. Recompute validates the current model, executes feature
intent through Geometry adapters, and publishes checked Body/feature products. Consumers that need a
fresh result use the current recompute/freshness boundaries rather than historical final-shape
assumptions.

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

A child `AssemblyDocument` is a shared model definition. It enters the rooted hierarchy only through
an authored `SubassemblyInstance`.

A persistent `ComponentInstance` stores direct local placement/state. Derived mutation identity is:

```text
ComponentTransformAuthority =
  (assembly_document: DocumentId,
   local ComponentInstanceId)
```

Repeated rooted occurrences of one child document may expose different root-space geometry while
sharing the same child-document component transform authority.

A `SubassemblyInstance` stores a referenced child assembly and one authored rigid boundary
transform. It is not currently a six-variable solve authority and has no grounding field.

Project validation rejects invalid child references, root-as-child references, hierarchy cycles,
invalid component/member structure, invalid local relationship/joint endpoints, and invalid
occurrence-qualified endpoint path/component structure.

## Hierarchy traversal and visible-active leaves

`AssemblyHierarchyTraversal` derives deterministic root-first occurrence descriptors.

For:

```text
root --outer--> child --inner--> grandchild
```

Grandchild context is:

```text
occurrence_path = [outer, inner]
parent_transforms_inner_to_outer = [T_inner, T_outer]
```

Every authored `RigidTransform` uses millimeters, degrees, right-hand positive rotation, active
fixed-axis X-then-Y-then-Z rotation, and `Rz * Ry * Rx` column-vector composition.

For:

```text
[T_component, T_inner, T_outer]
```

point evaluation is:

```text
T_outer(T_inner(T_component(p)))
```

Directions, normals, axes, and frame vectors rotate but do not translate.

`AssemblyLeafOccurrenceResolver` is the canonical hierarchy-to-Part leaf boundary for posed
geometry consumers. A leaf retains:

```text
containing AssemblyDocumentId
exact rooted SubassemblyInstance path
local ComponentInstanceId
referenced PartDocumentId
exact transforms_inner_to_outer
```

Hidden/suppressed hierarchy branches remove descendants. Hidden/suppressed local components are
absent. The leaf boundary is deterministic and unpersisted.

## Typed assembly geometric targets

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

Every projection validates endpoint and coordinate-space consistency, source metadata,
source-kind/descriptor agreement, descriptor geometry, canonical capability order, and transform
context before returning geometry.

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
`ComponentTransformAuthority` values. Each unique free active transform authority contributes:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

Local relationships evaluate in the containing assembly-document space. Project-level relationships
evaluate through exact rooted chains in root-assembly space.

The shared numeric engine owns scaled residual flattening, central finite differences, damped
Gauss-Newton normal equations, dense elimination, damping escalation, backtracking, and solve-state
classification. There is no second cross-hierarchy optimizer.

`AssemblySemanticTargetPartSnapshot` stores the exact canonical Part JSON used during solve/motion.
At application, current Part intent is serialized again and compared byte-for-byte. Local solve
application, flexible-child application, local joint motion, cross-hierarchy geometric application,
and cross-hierarchy joint motion reuse this conservative freshness authority.

## Assembly joints and motion

Persistent local `AssemblyJoint` and Project-level `AssemblyHierarchyJoint` intent remain separate
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
reference X direction exists. Spherical instead requires Point/Point and contributes center
coincidence.

Selected joints receive requested coordinates. Other active driven joints in the same combined
motion closure hold authored coordinates. Cross-hierarchy motion connectivity spans local geometry,
local joints, Project cross geometry, and Project cross joints. Fresh application protects
relationship records, transform-authority inputs, current participation, hierarchy boundaries,
exact Part intent, and selected coordinate semantics before atomic mutation.

`AssemblyFlexibleSubassemblySolver` selects one exact active non-root occurrence path and creates a
temporary child-as-root Project view. Ordinary local solve/application is reused. Successful
application writes direct child component transforms back to the shared child `AssemblyDocument`;
selected and ancestor `SubassemblyInstance` transforms remain unchanged.

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

`AssemblyPosedLeafShapeBuilder` reuses canonical visible-active leaves, shared Part definitions, and
exact transform chains.

Current analysis consumers include:

```text
interference
clearance
rooted pair contact classification
bounded sampled Revolute sweep
```

`AssemblyContactAnalyzer` evaluates every visible-active unordered rooted component occurrence pair
once. Pair identity is the canonical ordered pair of exact rooted component exchange identities.
Classification is:

```text
overlap_volume_mm3 > minimum_overlap_volume_mm3
  -> Interfering

otherwise minimum_distance_mm <= touching_tolerance_mm
  -> Touching

otherwise
  -> Separated
```

`AssemblyRevoluteSweepAnalyzer` supports root-local and Project cross-hierarchy Revolute motion. It
accepts 2..1001 inclusive samples and starts every sample from a fresh source Project copy before
existing motion solve/application and contact analysis. This is deterministic discrete sampling,
not continuous collision detection.

## Optional GUI architecture through Block 105

Blocks 95–105 implement and accept the optional Qt 6 desktop for exercising all Core/Geometry
features implemented through Block 94.

The CAD shell contains:

```text
tabbed command/workspace area
model / assembly browser
central OCCT viewport
properties / task editor
diagnostics and status surfaces
```

The GUI owns transient session, selection, task, and command-enable state. Document lifecycle,
atomic candidate transactions, undo/redo, recompute, and structured diagnostics remain mediated by
`GuiDocumentSession`.

The interaction path is:

```text
Qt widgets and command/task state
  -> GUI document transaction
  -> public Core intent and persistence
  -> existing recompute / solve / analysis / exchange APIs
  -> Geometry results
  -> transient OCCT presentations and semantic picking
```

Block 97 adds OCCT display/navigation and the stable semantic picking bridge. Block 98 adds the
deterministic browser/property projection and bidirectional semantic selection synchronization.
Blocks 99–104 expose the existing planar Sketch, Part, Surface, Assembly, motion, analysis, and STEP
export authorities. Block 105 closes the phase with machine-checked feature coverage and
GUI/headless equivalence.

Preview/Apply/Cancel and undo/redo operate on candidate document transactions. Failures retain the
last valid displayed result. Viewer owners map back to stable BLCAD semantic references rather than
persisting OCCT subshape identity.

## Interactive Sketch authority through Block 107

Interactive Sketcher MVP-8 is sequenced in `docs/interactive-sketcher-sequence-mvp8.md`.

Block 106 introduces the contextual transient `GuiWorkspace::Sketch`. `GuiSketchWorkspace` projects
Sketch-specific interaction stages onto the existing generic `GuiTaskState` authority:

```text
Idle
Hover
CollectingPicks
NumericInput
Preview
SelectedHandle
DragCandidate
```

`Enter Sketch` captures prior workspace/selection plus a transient camera bookmark and viewport
selection filter, resolves the current workplane, enters normal orthographic view, restricts picking
to SketchEntity/Edge/Vertex, and activates transient Dim/Isolate Sketch focus. `Finish Sketch`
rejects active command stages or current diagnostic errors and restores workspace, semantic
selection, Eye/Target/Up/Scale/projection, selection filter, and cursor state.

Block 107 adds one read-only transient plane-interaction authority:

```text
persistent Part + Sketch intent
  -> GuiSketchInteractionSceneBuilder
  -> transient plane-space interaction scene
  -> GuiSketchPlaneMapping
  -> GuiSketchInteractionController
  -> OcctViewport overlay / semantic selection
  -> GuiSketchWorkspaceStatus + GuiSelectionModel
```

`GuiSketchPlaneMapping` is the single conversion boundary between Qt device-independent pixels,
model-space view rays, active workplane coordinates, and model-space points. The native OCCT provider
uses `V3d_View::ConvertWithProj(...)` and `V3d_View::Convert(...)` with explicit
`devicePixelRatioF()` conversion. The offscreen provider uses an explicit plane center and
model-units-per-DIP scale.

`GuiSketchInteractionSceneBuilder` derives transient curves, point/snap landmarks, annotation
anchors, and intersections from the current persistent Sketch. Current projection covers lines,
three-point arcs, cubic Bezier splines, projected point/line references, reference-generated helper
lines, parameter-driven rectangle/circle profiles, circular hole patterns, driving dimensions, and
existing constraint/tangent anchors. Projected references resolve through the existing Geometry
projector. Lost references are counted and omitted; no replacement geometry is authored.

Interaction curve sampling is presentation/query data only:

```text
three-point arc  48 segments
cubic Bezier     64 segments
circle           72 segments
```

The frozen hit priority is:

```text
Point
Curve
Dimension
Glyph
```

Within a priority class, hit order is screen-distance, model-distance, then stable candidate id.
Repeated clicks at the same position cycle the deterministic hit stack and reset when the pointer or
hit signature changes.

Empty-space drag uses conventional CAD semantics:

```text
left -> right   Window: complete interaction curve contained
right -> left   Crossing: contained or intersecting interaction curve
```

Ordinary click replaces semantic Sketch selection, `Shift` adds, and `Ctrl` toggles. The result is
published into the existing `GuiSelectionModel`; screen coordinates, transient candidate ids, and
sampled polylines do not become persistent identity.

Implemented snap/inference families are:

```text
Origin
Axis
Endpoint
Midpoint
Center
Quadrant
Intersection
Nearest
Grid
HorizontalInference
VerticalInference
AlignmentX
AlignmentY
```

Candidates must lie inside the configured DIP tolerance. Eligible snap candidates are ordered by:

```text
model-space distance
screen-space distance in DIP
stable snap-family priority
stable candidate id
```

The `OcctViewport` owns one transparent interaction overlay for grid, highest-priority hover,
snap marker, and Window/Crossing rubber-band preview. A small binder installs the Block-107
projection on an existing `MainWindow`, rebuilds it after supported Sketch changes, feeds raw plane
cursor and snap/inference text into Block-106 status, and clears the projection on Finish Sketch.
The binder owns no CAD mathematics or mutation authority.

Blocks 106–107 introduce no new JSON field. Screen mappings, hit stacks, grid lines, interaction
samples, hover, rubber-band rectangles, and snap results are regenerated transiently.

Canonical contracts:

- `docs/gui-interactive-sketch-workspace-mvp8.md`
- `docs/gui-sketch-plane-interaction-mvp8.md`

The current next technical step is Block 108: shared persistent planar point/entity topology,
dependency-safe editable Core commands, construction/reference flags, canonical ordering, JSON
migration from the current planar Sketch representation, and exact undo/redo semantics.

## Persistence summary

Persist:

```text
PartDocument parametric and feature intent
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

Regenerate:

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
```

`docs/file-format.md` remains save-format authority.

## Planned STEP import authority after Block 131

Blocks 132–138 in `docs/step-import-sequence-mvp10.md` add two explicit modes without weakening the
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

## Current direction and canonical documents

Implemented numbered phases currently extend through Block 107.

Canonical numbered sequences and current architecture contracts include:

- `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`
- `docs/part-construction-sequence-mvp6.md`
- `docs/gui-feature-validation-sequence-mvp7.md`
- `docs/interactive-sketcher-sequence-mvp8.md`
- `docs/gui-interactive-sketch-workspace-mvp8.md`
- `docs/gui-sketch-plane-interaction-mvp8.md`
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

The complete original Assembly target-planning baseline remains in
`docs/assembly-general-geometric-target-roadmap-planning-baseline.md` as historical design context;
implemented canonical contracts are authoritative.

Block 108 is the current implementation boundary. Interactive Part/Surface/Assembly modeling begins
after Interactive Sketcher acceptance in Block 122. STEP Import begins in Block 132.

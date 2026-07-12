# Architecture Summary

This document summarizes implemented BLCAD architecture and current direction. Feature-specific MVP documents remain canonical for exact formulas, JSON spellings, validation/failure policy, ordering, and focused proofs.

## Goal and authority model

BLCAD is an independent parametric CAD system for Linux. BLCAD owns semantic model intent; OCCT/Open CASCADE is a computed geometry and data-exchange kernel.

Fundamental decisions:

- persist parametric and semantic intent rather than final BRep authority;
- persist semantic references rather than raw OCCT topology ids;
- keep Core model intent below Geometry execution/query types;
- keep selected/resolved source classification separate from geometric capability;
- keep direct component transform authority separate from rooted occurrence identity;
- keep authored hierarchy boundaries separate from derived traversal/composition;
- keep geometric relationships separate from motion-joint intent;
- keep exchange/product identity separate from OCCT/XDE/STEP ids;
- keep contact/sweep products derived rather than persistent state;
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

## Part model

`PartDocument` is persistent parametric definition authority.

Implemented part intent includes:

```text
typed quantities and parameters
unit-aware parameter expressions
datum and derived workplanes
construction points / lines / planes
sketches and profile geometry
projected/reference-driven sketch geometry
semantic generated references and recovery
sketch constraints / dimensions / diagnostics / repair helpers
additive and subtractive extrudes
CircularHolePattern
dependency / invalidation / recompute planning
JSON model-intent serialization
```

OCCT shapes and `ShapeCache` values are computed products.

## Project ownership

`Project` owns:

```text
one explicit root AssemblyDocument
Project-owned child AssemblyDocument records
Project-owned PartDocument records
Project-owned AssemblyHierarchyConstraint records
Project-owned AssemblyHierarchyJoint records
```

Persistent Project-level cross-hierarchy fields are:

```text
cross_hierarchy_constraints[]
cross_hierarchy_joints[]
```

A child `AssemblyDocument` is a shared model definition. It enters the rooted hierarchy only through a containing `SubassemblyInstance`.

## Component transform authority

A persistent `ComponentInstance` stores direct local placement/state:

```text
id
name
referenced_part_document
visibility
suppression_state
grounding_state
RigidTransform
  translation_mm
  rotation_deg
```

Derived mutation identity:

```text
ComponentTransformAuthority =
  (assembly_document: DocumentId,
   local ComponentInstanceId)
```

This identity addresses one owning persistent component record and is not separately serialized.

Repeated rooted occurrences of one child document may expose different root-space geometry while sharing the same child-document component transform authority.

## Rigid subassembly occurrence intent

A persistent `SubassemblyInstance` stores:

```text
id
name
referenced_assembly_document
visibility
suppression_state
RigidTransform
```

Its transform is the authored rigid boundary from a containing assembly occurrence to the referenced child assembly definition.

`SubassemblyInstance` is not currently a six-variable solve authority and has no grounding field. Component solve/application never writes a composed hierarchy transform into a subassembly boundary.

## Hierarchy and transform semantics

Project structure validation rejects invalid child references, root-as-child references, cycles, invalid component/member structure, invalid local relationship/joint endpoints, and invalid cross-hierarchy occurrence-path/component structure.

`AssemblyHierarchyTraversal` derives deterministic root-first pre-order occurrence descriptors.

For:

```text
root --outer--> child --inner--> grandchild
```

Grandchild context is:

```text
occurrence_path = [outer, inner]
parent_transforms_inner_to_outer = [T_inner, T_outer]
```

Every authored `RigidTransform` uses:

```text
translation: millimeters
rotation: degrees
positive rotation: right-hand rule
rotation type: active fixed-axis
application order: X, then Y, then Z
column-vector rotation composition: Rz * Ry * Rx
```

For:

```text
[T_component, T_inner, T_outer]
```

point evaluation is:

```text
T_outer(T_inner(T_component(p)))
```

Directions, normals, axes, and frame vectors rotate but do not translate.

The shared OCCT conversion uses the same active fixed-axis semantics. Flattened posed geometry and structured XDE placements reuse this transform contract.

## Visible-active leaf boundary

`AssemblyLeafOccurrenceResolver` is the canonical hierarchy-to-part-leaf boundary for posed geometry consumers.

One leaf retains:

```text
containing AssemblyDocumentId
exact rooted subassembly path
local ComponentInstanceId
referenced PartDocumentId
exact transforms_inner_to_outer
```

Hidden or suppressed hierarchy branches remove descendants. Hidden or suppressed local components are absent.

The leaf boundary is deterministic and unpersisted.

## Local and cross-hierarchy geometric solving

Persistent geometric families remain:

```text
Mate
Distance
Angle
Concentric
Insert
```

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

`AssemblyConstraintGraph` and `AssemblyCrossHierarchyConstraintGraph` derive deterministic relationship connectivity over `ComponentTransformAuthority` values.

Each unique free active authority contributes six direct transform variables:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

Local relationships evaluate in their containing assembly-document coordinate space. Project-level relationships evaluate through exact rooted parent chains in root-assembly space.

The shared numeric engine owns:

```text
scaled residual flattening
central finite differences
damped Gauss-Newton normal equations
dense elimination
damping escalation
backtracking / line search
convergence classification
```

No separate cross-hierarchy optimizer exists.

## Typed geometric target taxonomy and capability projection

Block 31 adds a derived Geometry-layer boundary between semantic source resolution and equation geometry.

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
AssemblyGeometricTargetSourceKind
derived source model metadata
typed AssemblyGeometricTargetDescriptor variant
canonical capability vector
AssemblyGeometricTargetCoordinateSpace
exact current transforms_inner_to_outer context
```

Coordinate spaces are explicit:

```text
ComponentLocal
RootAssembly
```

Local typed targets keep component-local descriptor geometry plus:

```text
transforms_inner_to_outer = [component_transform]
```

Hierarchy typed targets evaluate the descriptor through:

```text
[T_component,
 T_inner_parent,
 ...,
 T_outer_parent]
```

and store root-assembly descriptor geometry plus the exact chain.

Typed descriptors are:

```text
AssemblyPlanarTargetDescriptor
AssemblyAxisTargetDescriptor
AssemblyLineTargetDescriptor
AssemblyPointTargetDescriptor
AssemblyCircularEdgeTargetDescriptor
AssemblyCylindricalSurfaceTargetDescriptor
AssemblyFrameTargetDescriptor
```

Resolved directions are finite unit vectors. Plane/Circle/Frame axes are orthonormal and right-handed. Circle/Cylinder radii are finite and positive.

Capability projections are:

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

Current source adaptation:

```text
six generated planar-face tokens
  -> GeneratedPlanarFace
  -> Plane

current .axis single-circle subtractive-extrude producer
  -> GeneratedCylindricalFace
  -> Axis + Cylinder

current .seat
  -> CircularFeatureSeat
  -> Plane + Axis + Frame
```

The persistent target strings remain unchanged.

The `.axis` typed Cylinder radius is derived from the existing circle diameter parameter. Its projected Axis is numerically identical to the historical axis resolver.

The `.seat` Frame uses the existing seating X/Y axes and semantic axis as Z, preserving current Insert/Revolute orientation and signed-twist semantics.

Historical local/hierarchy `resolve`, `resolve_axis`, and `resolve_insert` methods remain compatibility adapters. They now obtain their geometry from the typed projection boundary. Existing equation/residual consumers therefore retain old descriptor shapes while target source/capability semantics are centralized.

Canonical contract: `docs/assembly-geometric-target-taxonomy-mvp5.md`.

## Semantic target PartDocument freshness

`AssemblySemanticTargetPartSnapshot` stores:

```text
part_document
canonical_model_intent_json
```

The payload is exactly:

```text
serialize_part_document_to_json(part)
```

At application the current `PartDocument` is serialized again and compared byte-for-byte.

Local solve application, flexible-child application, local Revolute motion, cross-hierarchy geometric application, and cross-hierarchy Revolute motion reuse this conservative freshness authority.

The typed target taxonomy does not add a second target revision model.

## Local and cross-hierarchy Revolute motion

Persistent local `AssemblyJoint` and Project-level `AssemblyHierarchyJoint` intent remain separate from geometric constraints.

The current joint family is `Revolute` with active/inactive state, principal-angle limits, and one authored angular coordinate.

Current `.seat` targets project `Frame`, `Axis`, and `Plane`. Existing compatibility adapters retain the directed axis/seating descriptors used by the shared Revolute residual constructor:

```text
directed axis alignment
axis offset
signed seating separation
signed periodic twist sine
signed periodic twist cosine
```

Selected joints receive requested coordinates. Other active Revolute joints in the combined motion closure hold authored coordinates.

Cross-hierarchy motion connectivity spans:

```text
local geometry
local joints
Project cross geometry
Project cross joints
```

Fresh application protects relationship records, authority inputs, current participation, hierarchy boundaries, exact PartDocument intent, and selected coordinate semantics before atomic mutation.

## Flexible child solving

`AssemblyFlexibleSubassemblySolver` selects one exact active non-root occurrence path and creates a temporary child-as-root local Project view.

Ordinary local solve/application is reused.

Successful application writes direct child component transforms back to the shared Project-owned child document. Selected/ancestor `SubassemblyInstance` boundary transforms remain unchanged.

Repeated occurrences of one child definition share the same internal component pose.

## Exchange occurrence identity and structured STEP

Derived exchange identities are:

```text
AssemblyExchangeAssemblyOccurrenceIdentity
  = exact rooted SubassemblyInstance path

AssemblyExchangeComponentOccurrenceIdentity
  = (containing rooted path,
     local ComponentInstanceId)

AssemblyExchangePartProductDefinitionIdentity
  = referenced PartDocumentId
```

Exchange component occurrence is not transform authority.

`AssemblyExchangeGraph` derives the explicit root plus every path prefix required by one visible-active leaf.

Ordering:

```text
assembly occurrences: lexicographic path
component occurrences: path then ComponentInstanceId
part definitions: PartDocumentId
```

Generated exchange names percent-encode authored bytes outside `A-Z a-z 0-9 . _ -`. Names are metadata, not identity authority.

`AssemblyPartShapeDefinitionBuilder` performs one recompute plus one private `ShapeCache` per unique exported part.

`AssemblyStructuredStepExporter` creates shared XDE part definitions, rooted assembly labels, component references, and exact parent-child assembly references. The flattened STEP path remains a compatibility consumer.

## Posed analysis, rooted contact, and sampled motion

`AssemblyPosedLeafShapeBuilder` reuses canonical visible-active leaves, shared part definitions, and exact transform chains.

Historical compatibility analyzers remain:

```text
AssemblyInterferenceAnalyzer
AssemblyClearanceAnalyzer
```

Block 30 adds complete typed rooted pair classification:

```text
AssemblyComponentOccurrencePairIdentity =
  canonical ordered pair of
  AssemblyExchangeComponentOccurrenceIdentity values
```

Classification:

```text
overlap_volume_mm3 > minimum_overlap_volume_mm3
  -> Interfering

otherwise minimum_distance_mm <= touching_tolerance_mm
  -> Touching

otherwise
  -> Separated
```

`AssemblyContactAnalysis::records` contains one complete record per evaluated visible-active unordered pair.

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
```

Regenerate:

```text
dependency/recompute execution products
OCCT shapes and ShapeCache values
hierarchy traversal and transform chains
visible-active leaves
relationship/joint connectivity
ComponentTransformAuthority mappings
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

Block 31 adds no JSON field. `docs/file-format.md` remains save-format authority.

## Current direction

Blocks 23–31 of the current assembly sequence are implemented.

Canonical current target architecture:

- `docs/assembly-geometric-target-taxonomy-mvp5.md`
- `docs/assembly-general-geometric-target-roadmap.md`

Canonical numbered sequence:

- `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`

The next technical step is Block 32 only: assembly-selectable reference geometry Core intent and unambiguous semantic source identity.

Block 32 reuses existing DatumPlane/construction geometry identities, introduces first-class DatumAxis PartDocument intent if still absent, and freezes persistent semantic-reference grammar for DatumPlane, DatumAxis, ConstructionLine, and ConstructionPoint.

Reference-geometry JSON remains Block 33. Geometry resolution into the Block-31 capabilities remains Block 34.

Occurrence-local child pose overrides, whole-subassembly solve variables, general physics, and richer joints remain separately deferred according to their roadmap blocks.

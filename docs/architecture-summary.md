# Architecture Summary

This document summarizes implemented BLCAD architecture and current direction. Feature-specific documents remain canonical for exact formulas, save-format spellings, validation/failure policy, ordering, and focused proofs.

## Goal and authority model

BLCAD is an independent parametric CAD system for Linux. BLCAD owns semantic model intent; OCCT/Open CASCADE is a computed geometry and data-exchange kernel.

Fundamental decisions:

- persist parametric and semantic intent rather than final BRep authority;
- persist semantic references rather than raw OCCT topology ids;
- keep Core model intent below Geometry query/execution types;
- separate semantic source classification from solver geometric capability;
- separate direct component transform authority from rooted occurrence identity;
- separate authored hierarchy boundaries from derived traversal/composition;
- separate geometric relationships from motion-joint intent;
- separate exchange/product identity from OCCT/XDE/STEP identity;
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

`PartDocument` is persistent parametric definition authority. Implemented part intent includes typed quantities/parameters, unit-aware formulas, datum/derived workplanes, construction geometry, sketches/profiles, projected/reference-driven geometry, semantic generated references/recovery, sketch constraints/diagnostics/repair helpers, additive/subtractive extrudes, `CircularHolePattern`, and dependency/invalidation/recompute planning.

OCCT shapes and `ShapeCache` values are computed products.

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

Persistent geometric relationship families remain:

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

Local and cross-hierarchy graphs derive deterministic relationship incidence over `ComponentTransformAuthority` values.

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

Datum axes join the existing PartDocument dependency graph and recompute planning through their parameter and source-line edges.

Block 32 also freezes one canonical assembly semantic-source spelling per reference family:

```text
ref:datum_plane:<encoded-id>
ref:datum_axis:<encoded-id>
ref:construction_line:<encoded-id>
ref:construction_point:<encoded-id>
```

Uppercase `%HH` escaping of every id byte outside `[A-Za-z0-9_-]` keeps valid reference spellings dot-free and therefore provably disjoint from `<feature-id>.<role>` spellings. Parsing fails closed at the Core boundary; assembly endpoints keep persisting only component/occurrence identity plus the semantic-reference string.

Block 33 serializes DatumAxis intent through the additive optional `datum_axes` part-document array, preserves historical files, validates ownership/family rules at load, and roundtrips `ref:` endpoint spellings byte-for-byte without resolving geometry.

## Typed geometric target taxonomy and capability projection

Block 31 adds the derived Geometry boundary between semantic target resolution and equation geometry.

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

Plane preserves the existing workplane convention: X, Y, and the independently oriented face normal must be finite unit and pairwise orthogonal, but Plane handedness is not constrained. This keeps the historical Bottom face (`+X`, `+Y`, `-Z normal`) valid and numerically unchanged.

Circle and Frame carry right-handed orthonormal orientations. Axis/Line directions are finite unit vectors. Circle/Cylinder radii are finite and positive.

Current source adaptation is:

```text
.top/.bottom/.right/.left/.front/.back
  -> GeneratedPlanarFace
  -> Plane

current .axis single-CircleProfile subtractive-extrude producer
  -> GeneratedCylindricalFace
  -> Axis + Cylinder

current .seat
  -> CircularFeatureSeat
  -> Plane + Axis + Frame
```

The persistent target strings remain unchanged.

Current `.axis` derives Cylinder radius from the existing diameter parameter while projected Axis geometry remains identical to the historical resolver.

Current `.seat` preserves historical seat X and semantic Axis Z and derives typed Frame Y as:

```text
Y = Z × X
```

This produces a right-handed Frame even when the source workplane uses an independently oriented normal convention. Insert residuals still consume Axis plus seating normal; Revolute twist still consumes seat X plus Axis Z. Existing numeric semantics therefore remain unchanged.

Historical local and hierarchy `resolve`, `resolve_axis`, and `resolve_insert` APIs remain compatibility adapters. Their geometry originates at the typed projection boundary; existing equation/residual descriptor shapes remain available.

Canonical contract: `docs/assembly-geometric-target-taxonomy-mvp5.md`.

## Semantic target freshness

`AssemblySemanticTargetPartSnapshot` stores:

```text
part_document
canonical_model_intent_json
```

The payload is exactly `serialize_part_document_to_json(part)`.

At application, the current `PartDocument` is serialized again and compared byte-for-byte. Local solve application, flexible-child application, local Revolute motion, cross-hierarchy geometric application, and cross-hierarchy Revolute motion reuse this conservative freshness authority.

The typed target taxonomy adds no second target revision model.

## Revolute motion

Persistent local `AssemblyJoint` and Project-level `AssemblyHierarchyJoint` intent remain separate from geometric constraints.

The current family is `Revolute` with state, principal-angle limits, and one authored angular coordinate.

Current `.seat` sources expose Frame, Axis, and Plane. Existing compatibility adapters retain the directed axis/seating descriptor shape consumed by the shared Revolute residual constructor:

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

`AssemblyContactAnalysis::records` contains one complete record per evaluated pair.

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

Block 33 adds the additive optional `datum_axes` part-document array. `docs/file-format.md` remains save-format authority.

## Current direction

Blocks 23–34 of the current assembly sequence are implemented.

Canonical current target architecture:

- `docs/assembly-geometric-target-taxonomy-mvp5.md`
- `docs/assembly-reference-geometry-intent-mvp5.md`
- `docs/assembly-general-geometric-target-roadmap.md`

The complete original detailed planning baseline for Blocks 32–47 is preserved in `docs/assembly-general-geometric-target-roadmap-planning-baseline.md` and incorporated by the active roadmap for still-planned block acceptance/failure details.

Canonical numbered sequence:

- `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`

The next technical step is Block 35 only: stable semantic generated topology identity and recovery for generated cylindrical faces, linear edges, circular edges, and vertices.

Generated topology target resolution remains Block 36.

Occurrence-local child pose overrides, whole-subassembly solve variables, general physics, and richer joints remain deferred according to their roadmap blocks.

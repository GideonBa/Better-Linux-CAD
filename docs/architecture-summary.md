# Architecture Summary

This document summarizes implemented BLCAD architecture and current direction. Feature-specific MVP documents remain canonical for exact contracts, formulas, JSON spellings, ordering, failure policy, and focused proofs.

## Goal and authority model

BLCAD is an independent parametric CAD system for Linux. Semantic model intent is owned by BLCAD; OCCT/Open CASCADE is a computed geometry kernel.

Fundamental decisions:

- keep a custom CAD core above OCCT;
- persist model intent rather than final BRep authority;
- persist semantic references rather than raw OCCT topology ids;
- keep selected semantic source identity separate from derived solver geometric capability;
- keep geometric constraints separate from motion-joint intent;
- keep authored hierarchy transforms separate from derived traversal/composition;
- keep solve/motion transform authority separate from rooted geometric occurrence identity;
- keep exchange occurrence/product identity separate from OCCT/XDE/STEP ids;
- keep Core model intent below Geometry execution/query types;
- require explicit fresh-result application before numeric results change persistent state.

## Layer direction

```text
user interface / commands
Core parametric model intent
part feature history + semantic references
assembly geometry / motion / hierarchy intent
Core semantic target source identity
Core derived occurrence and transform-authority connectivity
Geometry typed target descriptors + capability projection
relationship / joint compatibility semantics
Geometry target/equation evaluation
shared numeric residual/Jacobian execution
freshness validation and atomic application
Core derived exchange occurrence/product identity
posed geometry / diagnostics / analysis / exchange consumers
OCCT geometry and XDE data-exchange kernel
file and exchange formats
```

The typed target/capability and compatibility layers are planned after Block 30. The current implementation still uses narrower generated plane/axis/seat resolver APIs.

A lower authority layer must not depend on a higher execution layer for persistent identity. Save-format authority is never a `blcad::geometry` query/result type. OCCT labels, topology ids, and STEP entity ids are never BLCAD model authority.

## Part model

Implemented part architecture includes typed quantities/parameters, unit-aware local formulas, datum/derived workplanes, construction geometry, rich sketch/profile seeds, projected/reference-driven geometry, semantic generated references and recovery, sketch constraints/dimensions/diagnostics/repair helpers, additive/subtractive extrude intent, `CircularHolePattern`, dependency/invalidation/recompute planning, optional OCCT execution through `ShapeCache`, STEP export, and JSON model-intent serialization.

`PartDocument` remains the persistent parametric definition. OCCT shapes are computed cache products.

## Project ownership

`Project` owns:

```text
one explicit root AssemblyDocument
Project-owned child AssemblyDocument records
Project-owned PartDocument records
Project-owned AssemblyHierarchyConstraint records
Project-owned AssemblyHierarchyJoint records
```

`Project::assembly()` is the explicit root assembly API.

Child assembly documents are shared model definitions independent from rooted occurrence placement. A child enters the rooted tree only through a containing `SubassemblyInstance`.

Assembly document ids are unique across root and child collection. The root may not be referenced as a child.

Persistent Project-level cross-hierarchy JSON fields are:

```text
cross_hierarchy_constraints[]
cross_hierarchy_joints[]
```

## Persistent component transform authority

A `ComponentInstance` is a part occurrence record scoped to one `AssemblyDocument`.

Persistent state includes:

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

The direct component transform is persistent local placement authority.

Derived mutation identity:

```text
ComponentTransformAuthority =
  (assembly_document: DocumentId,
   local ComponentInstanceId)
```

This identity is not separately persisted. It addresses one owning `ComponentInstance` record.

Repeated rooted occurrences of a shared child document may read and mutate the same child component transform authority.

## Rigid subassembly occurrence intent

A persistent `SubassemblyInstance` stores:

```text
id
name
referenced_assembly_document
visibility
suppression_state
RigidTransform
  translation_mm
  rotation_deg
```

Its transform is the authored rigid boundary from one containing assembly occurrence to the referenced child assembly definition.

A `SubassemblyInstance` has no grounding field and is not currently a six-variable solve authority.

Component solving/application never converts a composed hierarchy placement into a component transform or `SubassemblyInstance` proposal.

## Hierarchy validity and rooted traversal

Project structure validation rejects invalid child references, root-as-child references, direct/indirect cycles, invalid component/member relationships, invalid local relationship/joint endpoints, and invalid cross-hierarchy endpoint path/component structure.

`AssemblyHierarchyTraversal` derives the rooted occurrence tree in deterministic root-first pre-order after complete Project validation. Child occurrences are ordered lexicographically by local `SubassemblyInstanceId`.

One occurrence descriptor carries:

```text
assembly_document
parent_assembly_document
via_subassembly_instance
occurrence_path
parent_transforms_inner_to_outer
visible_path
active_path
```

For:

```text
root --outer--> child --inner--> grandchild
```

the grandchild context is:

```text
occurrence_path = [outer, inner]
parent_transforms_inner_to_outer = [T_inner, T_outer]
```

At every descent the current direct `SubassemblyInstance::transform()` is prepended. Thus the first parent-chain transform of a non-root occurrence is exactly its direct boundary from the containing assembly occurrence.

Hierarchy descriptors, parent links, and composed transform chains are derived and unpersisted.

## Rigid-transform semantics

Every authored `RigidTransform` uses:

```text
translation: millimeters
rotation: degrees
positive rotation: right-hand rule
rotation type: active fixed-axis
application order: X, then Y, then Z
column-vector composition: Rz * Ry * Rx
```

Points rotate then translate. Vectors/normals/directions rotate only.

For:

```text
chain = [T_component, T_inner, T_outer]
```

point evaluation is:

```text
T_outer(T_inner(T_component(p)))
```

Block 29 extracts the internal OCCT conversion:

```text
to_occt_rigid_transform(RigidTransform)
to_occt_location(RigidTransform)
```

with OCCT transform product:

```text
translation * Rz * Ry * Rx
```

The same conversion is reused by flattened posed geometry and structured XDE placements.

## Visible-active leaf boundary

`AssemblyLeafOccurrenceResolver` is the canonical hierarchy-to-part-leaf boundary for posed geometry consumers.

A leaf stores:

```text
containing AssemblyDocumentId
exact rooted subassembly path
local ComponentInstanceId
referenced PartDocumentId
exact transforms_inner_to_outer
```

Hidden or suppressed subassembly paths remove descendant leaves. Hidden or suppressed local components are absent.

The leaf boundary is deterministic and unpersisted.

## Current local geometric solving and diagnostics

Persistent local geometric families are:

```text
Mate
Concentric
Distance
Insert
Angle
```

Local target identity:

```text
(local ComponentInstanceId, semantic_reference)
```

`AssemblyConstraintGraph` derives deterministic local active connectivity.

The current `AssemblyConstraintTargetResolver` resolves generated planar feature faces plus narrow `.axis` and `.seat` circular-feature targets.

Every free active local component contributes six direct transform variables:

```text
tx_mm ty_mm tz_mm rx_deg ry_deg rz_deg
```

The shared numeric path owns canonical residual flattening, central finite differences, and damped Gauss-Newton solving.

`AssemblySolveResult` is derived/unpersisted and contains complete component snapshots, exact semantic target PartDocument snapshots, and transform proposals.

Local diagnostics use Jacobian rank and:

```text
remaining_dof = variable_count - rank(J)
```

## Exact semantic target PartDocument freshness

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

No hash and no mutable revision counter are used.

The local solver applier, flexible-child application, local Revolute motion, cross-hierarchy geometric application, and cross-hierarchy Revolute motion reuse this boundary.

The planned DatumPlane/DatumAxis/construction/generated-topology target expansion continues to use this exact conservative PartDocument freshness authority unless a deliberately more precise target-input revision model is introduced later.

## Shared numeric execution boundary

Internal numeric execution exposes:

```text
absolute candidate variable vector
  -> deterministic scaled residual vector
```

`solve_numeric_variables` owns option validation, initial residual evaluation, grounded-reference policy, fixed inconsistency, central finite differences, damped Gauss-Newton normal equations, dense solve, damping escalation, backtracking, and solve-state classification.

Local geometric solving, local joint motion, cross-hierarchy geometric solving, and cross-hierarchy motion adapt their model-specific authority/residual evaluation to this one boundary.

There is no second cross-hierarchy optimizer or finite-difference implementation.

The planned generic relationship families and richer joints must also adapt to this same numeric boundary rather than creating target- or family-specific optimizers.

## Local and cross-hierarchy Revolute motion

Persistent local `AssemblyJoint` and Project-level `AssemblyHierarchyJoint` intent remain separate from geometric constraints.

The first family is `Revolute` with active/inactive state, principal-angle limits, and authored current coordinate.

Local target identity is local component + semantic `.seat` reference.

Project-level target identity is:

```text
(occurrence_path,
 local ComponentInstanceId,
 semantic_reference)
```

Both local and hierarchy Revolute equation builders reuse one shared residual constructor for:

```text
directed axis alignment
axis offset
signed seating separation
signed periodic twist sine
signed periodic twist cosine
```

The selected joint receives the requested coordinate. Other active Revolute joints in the same combined motion closure receive holding drives at authored coordinates.

Cross-hierarchy motion connectivity order is:

```text
local geometry
local joint
Project cross geometry
Project cross joint
```

Each relationship maps to every unique `ComponentTransformAuthority` affecting its residual. Repeated rooted endpoints can retain two exact endpoint mappings while sharing one incidence/variable authority.

Fresh motion application protects all four relationship record families, authority inputs, exact current motion-group participation, hierarchy boundaries, exact PartDocument intent, and selected source/request coordinate semantics before atomically writing direct authority transforms plus only the selected Project-level joint coordinate.

## Document-scoped flexible child solving

`AssemblyFlexibleSubassemblySolver` selects one exact active non-root occurrence path and copies the referenced child `AssemblyDocument` into a temporary local-root Project together with Project-owned parts.

The ordinary local solver/application contract is reused.

Successful application writes direct child component transforms back to the shared Project-owned child document. Selected/ancestor `SubassemblyInstance::transform()` values remain rigid and unchanged.

Repeated occurrences of one child document share the same internal component pose.

## Cross-hierarchy geometric endpoint and solve semantics

Core endpoint identity:

```text
AssemblyHierarchyConstraintEndpoint =
  (occurrence_path,
   local ComponentInstanceId,
   semantic_reference)
```

The empty path addresses the root assembly occurrence.

`AssemblyHierarchyConstraintTargetResolver` resolves the exact rooted occurrence, delegates local semantic feature meaning to `AssemblyConstraintTargetResolver`, and evaluates component plus parent chain into root space.

`AssemblyCrossHierarchyConstraintGraph` derives local/Project relationship-to-authority incidence. Local constraints appear once per containing `AssemblyDocument`; cross endpoint context remains separately available.

`AssemblyCrossHierarchyConstraintSolver` solves one exact current group. Unique free active authorities contribute one six-variable block each. Local relationships evaluate in containing-document local space; Project-level relationships evaluate through exact root-space paths.

`AssemblyCrossHierarchySolveResultApplier` protects authority, relationship, current group, hierarchy boundary, and exact PartDocument freshness before atomically applying direct authority transforms.

Cross-hierarchy diagnostics reuse the exact free-authority proposal order and shared central finite-difference/rank implementation.

## Current target expressiveness boundary

The current target resolver is feature-family-specific and intentionally narrow.

Implemented source semantics are effectively:

```text
GeneratedPlanarFace
narrow generated circular-feature Axis
CircularFeatureSeat
```

Current semantic spellings include:

```text
feature.<feature-id>.top|bottom|right|left|front|back
feature.<feature-id>.axis
feature.<feature-id>.seat
```

This is not yet a general Inventor-/SolidWorks-like selector for semantic cylindrical faces, linear/circular edges, generated vertices, datum planes, datum axes, construction lines, or construction points.

The solver, hierarchy, authority, and numeric layers are already suitable for broader target geometry. The missing architecture is semantic source identity plus typed derived target capability and explicit compatibility.

Canonical planned expansion: `docs/assembly-general-geometric-target-roadmap.md`.

## Planned source kind versus geometric capability architecture

The post-Block-30 target roadmap separates two concepts.

### Semantic source kind

What model source was selected:

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

### Geometric capability

What equation geometry can be projected from that source:

```text
Plane
Axis
Line
Point
Circle
Cylinder
Frame
```

`Frame` is a finite right-handed oriented frame with origin and X/Y/Z axes.

Representative projections:

```text
GeneratedPlanarFace -> Plane
DatumPlane -> Plane
DatumAxis -> Axis + Line
ConstructionLine -> Line
ConstructionPoint -> Point
GeneratedVertex -> Point
GeneratedCircularEdge -> Circle + Axis + Point(center)
GeneratedCylindricalFace -> Cylinder + Axis
CircularFeatureSeat -> Frame + Axis + Plane
```

This prevents equation builders from knowing which feature family produced equivalent geometry.

For example, Concentric eventually requests:

```text
Axis <-> Axis
```

and consumes identical axis equation semantics whether each Axis came from the current `.axis`, DatumAxis, GeneratedCylindricalFace, GeneratedCircularEdge, or CircularFeatureSeat.

## Planned Blocks 31-36: typed targets and semantic source resolution

Block 31 introduces a derived target value equivalent to:

```text
AssemblyResolvedGeometricTarget
  exact endpoint identity
  semantic source kind
  source model identity metadata
  typed descriptor variant
  available capabilities
  current component transform context
```

Capability projection operations include:

```text
project_plane
project_axis
project_line
project_point
project_circle
project_cylinder
project_frame
```

Unsupported projections fail closed.

Blocks 32-34 separate reference geometry authority boundaries:

```text
32 Core semantic source intent and optional DatumAxis model
33 serialization/backward compatibility
34 Geometry resolution
```

Existing `DatumPlane`, construction-line, and construction-point identities are reused. If first-class `DatumAxis` is still absent, Block 32 introduces it with explicit definition, dependency, and invalidation semantics.

The exact semantic-reference grammar is ambiguity-tested because typed ids are arbitrary non-empty strings.

Blocks 35-36 handle generated topology:

```text
35 producer-driven semantic generated topology identity/recovery
36 Geometry resolution to typed capabilities
```

Stable identity may use feature/profile/entity/semantic-role model intent. It may not use OCCT traversal indices, topology hashes as model ids, BRep map positions, XDE tags, STEP entity numbers, memory addresses, or unordered topology result order.

Patterned subelements remain unavailable until stable per-instance semantic identity exists.

## Planned Blocks 37-39: compatibility and generic relationships

Block 37 introduces one deterministic compatibility resolver:

```text
relationship type
+ target A capabilities
+ target B capabilities
-> one exact ordered capability pair/bundle
OR explicit incompatibility
```

Initial compatibility plan:

```text
Mate
  Plane <-> Plane

Distance
  Plane <-> Plane
  Point <-> Point
  Point <-> Plane
  Plane <-> Point

Angle
  Plane <-> Plane
  Line/Axis <-> Line/Axis

Concentric
  Axis <-> Axis

Insert
  Frame <-> Frame
```

Block 37 adds no relationship enum or equation.

Block 38 then adds persistent local/Project-level relationship intent and JSON spellings for:

```text
Coincident
Parallel
Perpendicular
```

Block 39 separately adds equations and existing local/cross-hierarchy numeric/diagnostic integration.

Initial Coincident compatibility:

```text
Point <-> Point
Point <-> Line
Line <-> Point
Point <-> Plane
Plane <-> Point
```

Parallel and Perpendicular initially consume Line/Axis direction pairs and Plane normal pairs.

These families reuse current graphs, `ComponentTransformAuthority`, shared residual/Jacobian execution, freshness, atomic application, and actual Jacobian-rank diagnostics. There is no generic-target-specific solver.

## Planned Blocks 40-43: joint compatibility and multi-coordinate motion foundation

Block 40 migrates joint target compatibility to the same capability architecture.

Current Revolute remains:

```text
Frame <-> Frame
```

Axis alone is insufficient for signed twist because it does not provide the reference X direction required by the existing twist residual.

Axis-only Revolute remains incompatible until semantic model intent supplies a deterministic oriented Frame. Geometry may not invent an arbitrary world-axis reference.

Block 41 generalizes persistent joint coordinate state into typed family-defined coordinate slots with stable roles and explicit Angular/Linear kinds.

Representative roles:

```text
Revolute
  rotation

Prismatic
  translation

Cylindrical
  translation
  rotation

Planar
  translation_u
  translation_v
  rotation_normal
```

Block 42 serializes the coordinate slots and preserves historical scalar Revolute compatibility.

Block 43 generalizes selected-joint motion from one scalar drive to deterministic coordinate drive vectors. Non-selected active joints hold all authored coordinate slots; initially undriven roles of the selected joint hold authored values.

Authority variables remain six direct component-transform variables per unique free authority. Joint coordinates remain drive parameters, not transform-authority variables.

## Planned Blocks 44-47: richer joint families

Each family is one block after the shared coordinate/vector-drive boundary exists.

### Prismatic — Block 44

Preferred first target compatibility:

```text
Frame <-> Frame
```

Coordinate:

```text
translation : LengthMm
```

### Cylindrical — Block 45

Planned coordinates:

```text
translation : LengthMm
rotation    : AngleDeg
```

The family must prove both drives coexist through the shared numeric path without duplicate transform variables.

### Planar — Block 46

Required target capability:

```text
Frame <-> Frame
```

Coordinates:

```text
translation_u   : LengthMm
translation_v   : LengthMm
rotation_normal : AngleDeg
```

### Ball/Spherical — Block 47

Required target compatibility:

```text
Point <-> Point
```

Preferred first seed is passive center coincidence with three free rotational DOF.

A driven spherical orientation is not encoded as arbitrary Euler angles without a separately frozen singularity/order contract. A passive-only first seed explicitly rejects selection as a driven joint.

Every richer joint family freezes its persistent type, coordinate/limit roles, JSON spelling, required capabilities, residual order, holding-drive behavior, motion connectivity, freshness, and atomic application semantics.

## Posed geometry and analysis

`AssemblyPosedLeafShapeBuilder` consumes canonical visible-active leaves.

Block 29 extracts `AssemblyPartShapeDefinitionBuilder`:

```text
sort/deduplicate referenced PartDocumentId
-> one private ShapeCache per unique part
-> one recompute per unique part
-> one unposed TopoDS_Shape definition
```

The posed-leaf builder reuses these definitions and the shared OCCT rigid-transform conversion, composes each leaf's exact transform chain, and creates one posed shape per leaf.

Flattened STEP export, interference analysis, and clearance analysis reuse this posed-leaf boundary.

Current static analysis:

```text
positive-volume overlap -> interference
minimum distance below caller threshold -> clearance violation
```

Block 30 will freeze richer `Separated/Touching/Interfering` semantics and a bounded sampled swept-motion query; it must not claim continuous collision detection without a continuous algorithm.

## Derived structured exchange identity

Block 29 adds a separate Core-derived exchange identity layer.

### Assembly exchange occurrence

```text
AssemblyExchangeAssemblyOccurrenceIdentity =
  exact rooted SubassemblyInstance occurrence path
```

The explicit root is `[]`.

Repeated occurrences remain distinct even when they reference one child `AssemblyDocument` definition.

### Component exchange occurrence

```text
AssemblyExchangeComponentOccurrenceIdentity =
  (containing assembly occurrence path,
   local ComponentInstanceId)
```

This is an exchange/presentation occurrence identity, not a transform authority.

Two repeated child occurrences can therefore be distinct exchange components while still mapping to one shared `ComponentTransformAuthority` in solving.

### Part product definition

```text
AssemblyExchangePartProductDefinitionIdentity =
  referenced PartDocumentId
```

Repeated component occurrences reference one shared part definition instead of duplicating parametric part intent.

## AssemblyExchangeGraph and structured STEP

`AssemblyExchangeGraph` builds from:

```text
AssemblyHierarchyTraversal
AssemblyLeafOccurrenceResolver
```

The retained assembly occurrence set is the explicit root plus every exact path prefix required by one visible-active leaf.

One assembly exchange occurrence stores exact path identity, assembly document, optional parent/via occurrence, direct `transform_from_parent`, and deterministic product name.

One component exchange occurrence stores rooted component identity, containing assembly document, shared `PartDocumentId` product definition, exact `transforms_inner_to_outer`, and deterministic occurrence name.

Ordering:

```text
assembly occurrences: lexicographic path
component occurrences: path then ComponentInstanceId
part definitions: PartDocumentId
```

Generated exchange names are metadata, not identity authority. Authored bytes outside `A-Z a-z 0-9 . _ -` are `%HH` encoded so `/`, `%`, and the explicit root sentinel remain collision-free.

`AssemblyStructuredStepExporter` creates one XDE part definition label per `PartDocumentId`, one XDE assembly label per rooted assembly occurrence, one part component reference per rooted component occurrence, and one parent-child assembly reference per non-root occurrence.

Part component location is the first/direct component transform from the canonical leaf chain. Child assembly location is the exact hierarchy-derived direct `transform_from_parent` boundary.

The explicit root assembly label is transferred through `STEPCAFControl_Writer` with name mode enabled and AP214 selected.

`AssemblyStepExporter` remains the flattened compound compatibility consumer.

Neither XDE labels nor STEP product/entity identities are persisted.

## Persistence summary

Persist:

```text
PartDocument parametric intent
AssemblyDocument local model intent
ComponentInstance placement/state
SubassemblyInstance child reference/placement/state
local geometric constraints
local Revolute joint records
Project-level cross_hierarchy_constraints[]
Project-level cross_hierarchy_joints[]
```

The planned target expansion continues to persist semantic source identity in local or occurrence-qualified endpoints. If `DatumAxis` is added, it is PartDocument model intent and receives explicit Core/JSON/dependency semantics.

Block 38 adds generic relationship type intent. Block 41 generalizes persistent joint coordinate slots, and Block 42 serializes that model with historical Revolute compatibility.

Regenerate:

```text
dependency/recompute execution products
OCCT shapes and ShapeCache values
hierarchy traversal and parent chains
visible-active leaves
semantic target source classification
typed target descriptors
Plane/Axis/Line/Point/Circle/Cylinder/Frame capabilities
relationship/joint compatibility projections
transformed target geometry
relationship/joint connectivity
ComponentTransformAuthority mappings
numeric residuals/Jacobians/results
freshness snapshots/proposals/diagnostics
AssemblyExchangeGraph identities/records/names
part shape definitions
XDE documents/labels/component references
STEP products/entities
posed analysis products
```

## Current direction

Blocks 23-29 of the cross-hierarchy assembly sequence are implemented.

Canonical current exchange contract:

`docs/assembly-structured-step-products-mvp5.md`

Canonical numbered implementation sequence:

`docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`

The next technical step remains Block 30 only: richer posed contact classification and bounded deterministic swept-Revolute analysis over exact rooted component occurrence identities.

After Block 30, Blocks 31-47 are explicitly planned in `docs/assembly-general-geometric-target-roadmap.md`:

```text
31-36 semantic target taxonomy, reference geometry, generated topology identity/resolution
37 explicit target compatibility matrix
38 generic relationship Core intent + JSON
39 generic relationship equations + shared solve integration
40 joint target compatibility + oriented Frame contract
41-43 general coordinate state, JSON, and vector-drive motion foundation
44 Prismatic
45 Cylindrical
46 Planar
47 Ball/Spherical
```

This is the planned route to surface/edge/vertex/plane/axis/line/point assembly selection comparable in architecture to mature parametric CAD systems while retaining BLCAD semantic identity rather than raw OCCT topology ids.

Occurrence-local child pose overrides, whole-subassembly solve variables, multi-turn coordinates beyond explicit family contracts, and a general physics engine remain separately deferred.

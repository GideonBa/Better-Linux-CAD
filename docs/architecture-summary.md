# Architecture Summary

This document summarizes implemented BLCAD architecture and current direction. Feature-specific MVP documents remain canonical for exact contracts, formulas, JSON spellings, ordering, failure policy, and focused proofs.

## Goal and authority model

BLCAD is an independent parametric CAD system for Linux. Semantic model intent is owned by BLCAD; OCCT/Open CASCADE is a computed geometry kernel.

Fundamental decisions:

- keep a custom CAD core above OCCT;
- persist model intent rather than final BRep authority;
- persist semantic references rather than raw OCCT topology ids;
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
Core derived occurrence and transform-authority connectivity
Geometry target/equation evaluation
shared numeric residual/Jacobian execution
freshness validation and atomic application
Core derived exchange occurrence/product identity
posed geometry / diagnostics / analysis / exchange consumers
OCCT geometry and XDE data-exchange kernel
file and exchange formats
```

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

## Local geometric solving and diagnostics

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

`AssemblyConstraintTargetResolver` resolves supported planar face, `.axis`, and `.seat` targets.

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

## Shared numeric execution boundary

Internal numeric execution exposes:

```text
absolute candidate variable vector
  -> deterministic scaled residual vector
```

`solve_numeric_variables` owns option validation, initial residual evaluation, grounded-reference policy, fixed inconsistency, central finite differences, damped Gauss-Newton normal equations, dense solve, damping escalation, backtracking, and solve-state classification.

Local geometric solving, local joint motion, cross-hierarchy geometric solving, and cross-hierarchy motion adapt their model-specific authority/residual evaluation to this one boundary.

There is no second cross-hierarchy optimizer or finite-difference implementation.

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

Repeated occurrences:

```text
[left]
[right]
```

remain distinct even if both reference one child `AssemblyDocument` definition.

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

## AssemblyExchangeGraph

`AssemblyExchangeGraph` builds from:

```text
AssemblyHierarchyTraversal
AssemblyLeafOccurrenceResolver
```

The retained assembly occurrence set is:

```text
explicit root
+
every exact path prefix required by one visible-active leaf
```

This means structured export inherits leaf visibility/suppression policy and does not retain hidden/suppressed empty branches.

One assembly exchange occurrence stores:

```text
exact identity path
assembly_document
optional parent identity
optional via SubassemblyInstanceId
direct transform_from_parent
deterministic product_name
```

For non-root occurrences:

```text
transform_from_parent =
  parent_transforms_inner_to_outer.front()
```

One component exchange occurrence stores:

```text
(containing rooted path, local ComponentInstanceId)
containing AssemblyDocumentId
PartDocumentId product definition identity
complete canonical transforms_inner_to_outer
occurrence_name
```

Ordering:

```text
assembly occurrences: lexicographic path
component occurrences: path then ComponentInstanceId
part definitions: PartDocumentId
```

All exchange graph data is derived/unpersisted.

## Collision-free exchange names

Generated exchange names are metadata, not identity authority.

Prefixes:

```text
blcad:assembly-occurrence:
blcad:component-occurrence:
blcad:part-definition:
```

ASCII `A-Z a-z 0-9 . _ -` remains unchanged. Every other UTF-8 byte is uppercase `%HH` encoded.

Therefore authored `/` and `%` bytes cannot collide with path separators or escape syntax.

The explicit root path text is `root`. A non-root one-segment path whose encoded text is exactly `root` uses reserved spelling `%72oot`.

The typed identity remains the source of truth regardless of generated name text.

## Structured XDE/STEP export

`AssemblyStructuredStepExporter` consumes `AssemblyExchangeGraph` plus shared part shape definitions.

It creates:

```text
one XDE part definition label per PartDocumentId
one XDE assembly label per rooted assembly occurrence
one part component reference per rooted component occurrence
one parent -> child assembly reference per non-root assembly occurrence
```

Part component location is the first/direct component transform from the canonical leaf chain.

Child assembly location is the exact hierarchy-derived direct `transform_from_parent` boundary.

The XDE graph therefore represents nested placement composition instead of receiving one precomposed root-space transform per leaf.

The explicit root assembly label is transferred through `STEPCAFControl_Writer` with name mode enabled and AP214 selected.

`AssemblyStepExporter` remains the flattened compound compatibility consumer.

Neither XDE labels nor STEP product/entity identities are persisted.

Headless structured consumer:

```text
blcad_export_structured_assembly <input.blcad.project.json> <output.step>
```

It exports current authored/persisted pose and intentionally performs no implicit solve.

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

Regenerate:

```text
dependency/recompute execution products
OCCT shapes and ShapeCache values
hierarchy traversal and parent chains
visible-active leaves
semantic target geometry
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

Canonical implementation sequence:

`docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`

The next technical step is Block 30 only: richer posed contact classification and bounded deterministic swept-Revolute analysis over exact rooted component occurrence identities.

Occurrence-local child pose overrides, whole-subassembly solve variables, richer joint families, multi-turn coordinates, and a general physics engine remain deferred.

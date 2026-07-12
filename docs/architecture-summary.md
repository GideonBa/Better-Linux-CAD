# Architecture Summary

This document summarizes implemented BLCAD architecture and current direction. Feature-specific MVP documents remain canonical for exact formulas, JSON spellings, failure policy, ordering, and focused proofs.

## Goal and authority model

BLCAD is an independent parametric CAD system for Linux. BLCAD owns semantic model intent; OCCT/Open CASCADE is a computed geometry and exchange kernel.

Fundamental decisions:

- persist parametric/semantic intent rather than final BRep authority;
- persist semantic references rather than raw OCCT topology ids;
- keep Core model intent below Geometry execution/query types;
- keep direct component transform authority separate from rooted occurrence identity;
- keep authored hierarchy transforms separate from derived traversal/composition;
- keep geometric constraints separate from motion-joint intent;
- keep exchange/product identity separate from OCCT/XDE/STEP ids;
- keep contact/sweep results derived rather than persistent state;
- require explicit fresh-result application before numeric solve/motion results mutate model intent.

## Layer direction

```text
user interface / commands
Core parametric model intent
part feature history + semantic references
assembly geometry / joint / hierarchy intent
Core derived occurrence + authority connectivity
Geometry semantic target resolution
Geometry relationship / joint equations
shared numeric residual/Jacobian execution
freshness validation + atomic application
Core derived exchange occurrence/product identity
posed geometry / contact / sweep / diagnostics / exchange consumers
OCCT geometry + XDE data-exchange kernel
file and exchange formats
```

The planned Block-31+ target-capability layer will be inserted between semantic target source identity and relationship/joint equation selection. It remains derived Geometry query semantics, not persistent save-format authority.

## Part model

Implemented part architecture includes typed quantities/parameters, unit-aware local formulas, datum and derived workplanes, construction geometry, rich sketch/profile seeds, projected/reference-driven geometry, semantic generated references and recovery, sketch constraints/dimensions/diagnostics/repair helpers, additive/subtractive extrude intent, `CircularHolePattern`, dependency/invalidation/recompute planning, optional OCCT execution through `ShapeCache`, STEP export, and JSON model-intent serialization.

`PartDocument` remains persistent parametric definition authority. OCCT shapes are computed cache products.

## Project ownership

`Project` owns:

```text
one explicit root AssemblyDocument
Project-owned child AssemblyDocument records
Project-owned PartDocument records
Project-owned AssemblyHierarchyConstraint records
Project-owned AssemblyHierarchyJoint records
```

Persistent Project-level cross-hierarchy JSON fields are:

```text
cross_hierarchy_constraints[]
cross_hierarchy_joints[]
```

Child assembly documents are shared model definitions. A child enters the rooted hierarchy only through a containing `SubassemblyInstance`.

## Component transform authority

A `ComponentInstance` stores direct local placement/state:

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

This identity addresses one owning persistent `ComponentInstance` record and is not separately serialized.

Repeated rooted occurrences of a shared child `AssemblyDocument` can expose different root-space geometry while reading and mutating one shared child-document component transform authority.

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

Its transform is the authored rigid boundary from the containing assembly occurrence to the referenced child assembly definition.

`SubassemblyInstance` is not currently a six-variable solve authority and has no grounding field. Component solving/application never writes a composed hierarchy transform into a `SubassemblyInstance`.

## Hierarchy validity and rooted traversal

Project structure validation rejects invalid child references, root-as-child references, direct/indirect cycles, invalid component/member relationships, invalid local relationship/joint endpoints, and invalid cross-hierarchy occurrence-path/component structure.

`AssemblyHierarchyTraversal` derives deterministic root-first pre-order occurrence descriptors. Child `SubassemblyInstance` records are ordered lexicographically by local id.

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

Grandchild context is:

```text
occurrence_path = [outer, inner]
parent_transforms_inner_to_outer = [T_inner, T_outer]
```

At each descent the current direct boundary transform is prepended. Hierarchy descriptors and composed context remain derived.

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

For:

```text
chain = [T_component, T_inner, T_outer]
```

point evaluation is:

```text
T_outer(T_inner(T_component(p)))
```

Vectors/normals/directions ignore translation.

The shared internal OCCT conversion preserves:

```text
translation * Rz * Ry * Rx
```

Flattened posed geometry and structured XDE placements reuse this one conversion contract.

## Visible-active leaf boundary

`AssemblyLeafOccurrenceResolver` is the canonical hierarchy-to-part-leaf boundary for posed geometry consumers.

One leaf stores:

```text
containing AssemblyDocumentId
exact rooted subassembly path
local ComponentInstanceId
referenced PartDocumentId
exact transforms_inner_to_outer
```

Hidden or suppressed hierarchy paths remove descendant leaves. Hidden or suppressed local components are absent.

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

Current local target identity:

```text
(local ComponentInstanceId,
 semantic_reference)
```

Current target resolution understands generated planar feature faces plus narrow circular-feature `.axis` and `.seat` semantics.

Every free active local component contributes six direct transform variables:

```text
tx_mm ty_mm tz_mm rx_deg ry_deg rz_deg
```

The shared numeric path owns residual flattening, central finite differences, damped Gauss-Newton solving, damping escalation/backtracking, dense elimination, convergence classification, and Jacobian-rank diagnostics.

Local diagnostics use:

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

At application the current part is serialized again and compared byte-for-byte.

The local solver applier, flexible-child application, local Revolute motion, cross-hierarchy geometric application, and cross-hierarchy Revolute motion reuse this conservative freshness boundary.

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

The current joint family is `Revolute` with active/inactive state, principal-angle limits, and one authored angular coordinate.

Both local and hierarchy Revolute equations reuse one residual constructor for:

```text
directed axis alignment
axis offset
signed seating separation
signed periodic twist sine
signed periodic twist cosine
```

Selected joints receive requested coordinates. Other active Revolute joints in the combined motion closure hold authored coordinates.

Cross-hierarchy motion connectivity order is:

```text
local geometry
local joint
Project cross geometry
Project cross joint
```

Each relationship maps to every unique `ComponentTransformAuthority` affecting its residual. Repeated rooted endpoints can retain separate endpoint mappings while sharing one variable authority.

Fresh motion application protects relationship records, authority inputs, current group participation, hierarchy boundaries, exact PartDocument intent, and selected source/request coordinate semantics before atomically writing direct authority transforms plus the selected authored coordinate.

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

`AssemblyCrossHierarchyConstraintGraph` derives local/Project relationship-to-authority incidence. Local constraints appear once per containing `AssemblyDocument`; Project-level endpoints retain exact occurrence context.

`AssemblyCrossHierarchyConstraintSolver` gives one six-variable block to each unique free active authority. Local relationships evaluate in containing-document space; Project-level relationships evaluate through exact root-space paths.

`AssemblyCrossHierarchySolveResultApplier` protects authority, relationship, current group, hierarchy boundary, and exact PartDocument freshness before atomic direct-authority application.

Cross-hierarchy diagnostics reuse exact proposal order plus the shared finite-difference/rank implementation.

## Exchange occurrence identity

Block 29 adds a separate derived exchange identity layer.

Assembly occurrence:

```text
AssemblyExchangeAssemblyOccurrenceIdentity =
  exact rooted SubassemblyInstance occurrence path
```

Component occurrence:

```text
AssemblyExchangeComponentOccurrenceIdentity =
  (containing assembly occurrence path,
   local ComponentInstanceId)
```

Part product definition:

```text
AssemblyExchangePartProductDefinitionIdentity =
  referenced PartDocumentId
```

Exchange component occurrence is not transform authority. Two repeated child occurrences can be distinct exchange components while sharing one solver transform authority.

## AssemblyExchangeGraph and structured STEP

`AssemblyExchangeGraph` builds from:

```text
AssemblyHierarchyTraversal
AssemblyLeafOccurrenceResolver
```

The retained assembly occurrence set is the explicit root plus every exact path prefix required by one visible-active leaf.

Ordering:

```text
assembly occurrences: lexicographic path
component occurrences: path then ComponentInstanceId
part definitions: PartDocumentId
```

Generated exchange names are deterministic metadata. Authored bytes outside `A-Z a-z 0-9 . _ -` are `%HH` encoded so path separators, percent escapes, and the explicit root sentinel remain collision-free.

`AssemblyPartShapeDefinitionBuilder` sorts/deduplicates referenced `PartDocumentId` values and performs one recompute plus one private `ShapeCache` per unique exported part.

`AssemblyStructuredStepExporter` creates one XDE part definition per part, one assembly label per rooted occurrence, one part component reference per rooted component occurrence, and one parent-child assembly reference per non-root occurrence.

Part component location is the direct component transform from the canonical leaf chain. Child assembly location is exact hierarchy-derived `transform_from_parent`.

Exact nested component occurrence names remain BLCAD exchange-graph identity. STEPCAF does not guarantee every nested instance label name verbatim in written Part 21 text.

## Posed geometry and compatibility analysis

`AssemblyPosedLeafShapeBuilder` consumes canonical visible-active leaves, reuses shared part definitions, composes each leaf's exact transform chain, and creates one posed OCCT shape per leaf.

Flattened STEP export and assembly analysis reuse this posed boundary.

Historical compatibility analyzers remain:

```text
AssemblyInterferenceAnalyzer
  -> positive-volume interference records

AssemblyClearanceAnalyzer
  -> clearance violations + interference records
```

Their existing result shapes and historical occurrence-key identity remain unchanged.

## Rooted contact classification

Block 30 adds `AssemblyContactAnalyzer` as the complete typed pair-classification query.

Pair identity:

```text
AssemblyComponentOccurrencePairIdentity =
  canonical ordered pair of
  AssemblyExchangeComponentOccurrenceIdentity values
```

Typed pair order is exact occurrence path sequence, then local `ComponentInstanceId`.

Every visible-active unordered posed pair is evaluated once.

Classification options default to:

```text
touching_tolerance_mm = 1.0e-6
minimum_overlap_volume_mm3 = 1.0e-6
```

Classification order is:

```text
overlap_volume_mm3 > minimum_overlap_volume_mm3
  -> Interfering

otherwise minimum_distance_mm <= touching_tolerance_mm
  -> Touching

otherwise
  -> Separated
```

`AssemblyContactAnalysis::records` contains exactly one record per evaluated pair.

`Interfering` records carry overlap volume and no minimum-distance value. `Touching` and `Separated` records retain the finite OCCT minimum distance.

Contact records, common shapes, distances, and overlap volumes are derived and unpersisted.

## Bounded sampled Revolute sweep

Block 30 adds `AssemblyRevoluteSweepAnalyzer`.

Supported motion scopes:

```text
RootAssemblyLocal
ProjectCrossHierarchy
```

The selected joint must exist in that scope, be active, and be Revolute. Start/end quantities use `AngleDeg` and both endpoints must lie inside authored joint limits.

Sample count is bounded:

```text
2 <= sample_count <= 1001
```

Samples are linear, inclusive, and preserve caller direction.

Every sample independently starts from:

```text
Project sample_project = source_project
```

Then:

```text
existing scope-specific motion solver
-> require converged result
-> existing atomic motion applier
-> AssemblyContactAnalyzer
```

No sample starts from the preceding sample. Source transforms and authored joint coordinates remain unchanged.

One sample stores:

```text
sample_index
coordinate_deg
applied_transform_count
complete AssemblyContactAnalysis
```

This is deterministic discrete sampled analysis, not continuous collision detection. Events existing only between requested sample coordinates may be missed.

Canonical Block-30 contract: `docs/assembly-contact-swept-motion-mvp5.md`.

## Current target expressiveness boundary

The current assembly target resolver is intentionally narrower than an Inventor-/SolidWorks-like picker.

Implemented semantic spellings remain:

```text
feature.<feature-id>.top|bottom|right|left|front|back
feature.<feature-id>.axis
feature.<feature-id>.seat
```

These represent generated planar feature geometry, a narrow circular-feature axis, and a composite circular seat.

The solver, hierarchy, authority, and numeric layers are already suitable for broader target geometry. The missing layer is explicit semantic source classification plus typed geometric capability projection and compatibility.

Canonical future roadmap: `docs/assembly-general-geometric-target-roadmap.md`.

Block 31 is next and introduces the derived taxonomy/capability boundary only. Planned source kinds include generated planar/cylindrical faces, linear/circular edges, vertices, DatumPlane, DatumAxis, construction line/point, and circular feature seat. Planned capabilities are `Plane`, `Axis`, `Line`, `Point`, `Circle`, `Cylinder`, and `Frame`.

Detailed Blocks 31-47 compatibility matrices, generic relationship sequencing, coordinate-slot architecture, and richer joint families remain canonical only in the roadmap document until implemented.

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
posed shapes
contact pair identities and classifications
sample Project copies
sample motion results/proposals
sampled Revolute sweep products
```

Blocks 29 and 30 add no save-format fields.

## Current direction

Blocks 23-30 of the cross-hierarchy assembly sequence are implemented.

Canonical current analysis contract:

`docs/assembly-contact-swept-motion-mvp5.md`

Canonical numbered implementation sequence:

`docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`

The next technical step is Block 31 only from `docs/assembly-general-geometric-target-roadmap.md`: typed geometric target taxonomy and capability projection.

Block 31 must preserve current semantic target strings and Mate/Distance/Angle/Concentric/Insert/Revolute numeric behavior. DatumAxis persistence, reference-geometry JSON, generic relationship families, richer joint families, occurrence-local child pose overrides, whole-subassembly solve variables, and a general physics engine remain outside Block 31.

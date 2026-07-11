# Architecture Summary

This document summarizes implemented BLCAD architecture and the current direction. Feature-specific MVP documents remain canonical for exact contracts, formulas, failure policies, and focused proofs.

## Goal and authority model

BLCAD is an independent parametric CAD system for Linux. BLCAD stores semantic model intent and uses OCCT/Open CASCADE as a computed geometry kernel.

Fundamental decisions:

- keep a custom CAD core above OCCT;
- persist model intent rather than final BRep authority;
- persist semantic references rather than raw OCCT topology ids;
- keep geometric assembly relationships separate from motion-joint intent;
- keep authored hierarchy transforms separate from derived traversal and composed evaluation;
- keep Core model intent below Geometry execution/query types;
- require explicit application before solver results change persistent model state;
- keep GUI/application presentation above CAD-core authority.

Layer direction:

```text
user interface
command/application layer
Core parametric model intent
sketch / construction / semantic-reference layers
part feature history
assembly relationship / motion / hierarchy intent
Geometry and numeric execution
engineering analysis consumers
OCCT geometry kernel
file and exchange formats
```

A lower authority layer must not depend on a higher execution layer for persistent model identity. Save-format authority must therefore not be a `blcad::geometry` query type.

## Part model

The implemented part path includes typed quantities and parameters, part-local unit-aware expressions, datum and derived workplanes, construction geometry, richer sketch/profile families, projected/reference-driven geometry, semantic generated references and recovery helpers, sketch constraints/dimensions/diagnostics/repair helpers, additive/subtractive extrude intent, `CircularHolePattern`, dependency/invalidation/recompute planning, optional OCCT recompute through `ShapeCache`, STEP export, and JSON model-intent serialization.

OCCT shapes are computed cache products.

## Project ownership

`Project` owns:

```text
one explicit root AssemblyDocument
project-owned child AssemblyDocument records
project-owned PartDocument records
project-owned AssemblyHierarchyConstraint records
```

`Project::assembly()` is the explicit root assembly API.

Child assembly documents are owned independently of rooted occurrence placement. A child enters the rooted assembly tree only when a containing assembly stores a `SubassemblyInstance` referencing it.

Assembly document ids are unique across the root and child collection. The root may not be referenced as a child.

The project-level cross-hierarchy constraint collection is separate from every `AssemblyDocument::constraints()` collection because one relationship may address exact endpoints in different rooted assembly occurrences.

## Component transform authority

A `ComponentInstance` is a part occurrence record scoped to one `AssemblyDocument`.

Persistent component state includes:

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

For cross-hierarchy solving, one component transform authority is conceptually:

```text
ComponentTransformAuthority =
  (assembly_document: DocumentId,
   local ComponentInstanceId)
```

This is currently a derived planning identity rather than a public persistent record.

Repeated rooted occurrences of one child assembly may expose different geometry while reading the same child-document component transform authority.

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

Its transform is the authored rigid boundary from one containing assembly occurrence to the referenced child assembly document.

A `SubassemblyInstance` currently has no grounding field and is not itself a six-variable solve node.

Adding or loading a child occurrence never changes child component transforms.

## Hierarchy validity and traversal

Project-level hierarchy validation rejects duplicate assembly document ids, duplicate occurrence ids inside one containing assembly, direct self-reference, root-as-child references, missing child documents, and direct or indirect assembly-reference cycles.

`AssemblyHierarchyTraversal` derives the rooted occurrence tree in deterministic root-first pre-order with child occurrences ordered lexicographically by `SubassemblyInstanceId`.

Repeated occurrences of one child document remain separate traversal descriptors.

`AssemblyHierarchyOccurrenceDescriptor` carries derived context:

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

the grandchild descriptor carries:

```text
occurrence_path = [outer, inner]
parent_transforms_inner_to_outer = [T_inner, T_outer]
```

Hierarchy descriptors, paths, parent links, and transform chains are derived and unpersisted.

## Rigid-transform semantics

Every authored `RigidTransform` uses millimeter translation, degree rotation, active right-handed fixed-axis rotations, and X-then-Y-then-Z application order.

For column vectors:

```text
R = Rz * Ry * Rx
```

Points rotate then translate. Vectors, normals, and directions rotate only.

`AssemblyTransformEvaluator` evaluates one authored level.

`AssemblyHierarchyTransformEvaluator` evaluates transform chains exactly in inner-to-outer order.

For:

```text
[T_component, T_inner, T_outer]
```

point evaluation is:

```text
T_outer(T_inner(T_component(p)))
```

The hierarchy is not persisted as a composed matrix or converted back into a recomputed Euler triple.

## Visible-active leaf flattening

`AssemblyLeafOccurrenceResolver` is the canonical hierarchy-to-leaf boundary for posed geometry consumers.

One derived leaf descriptor stores:

```text
assembly_document
subassembly_path
component_instance
referenced_part_document
transforms_inner_to_outer
```

Leaf order is hierarchy pre-order followed by local `ComponentInstanceId` order.

Hidden or suppressed subassembly occurrences remove their complete descendant subtree. Hidden or suppressed local components are absent from flattened leaves.

## Local geometric relationship intent

Persistent local relationship families are:

```text
Mate
Concentric
Distance
Insert
Angle
```

Local `AssemblyConstraintTarget` identity is:

```text
(local ComponentInstanceId, semantic_reference)
```

Local constraints belong to one containing `AssemblyDocument`. Target A/B order and active/inactive state are persistent intent. Distance carries a length quantity; Angle carries a degree quantity.

`AssemblyConstraintGraph` uses local component nodes and active local relationship multi-edges and derives deterministic connected groups without persisting graph caches.

## Semantic target families

Implemented generated planar target strings:

```text
feature.<feature-id>.top|bottom|right|left|front|back
```

Implemented circular target strings:

```text
feature.<feature-id>.axis
feature.<feature-id>.seat
```

`AssemblyConstraintTargetResolver` is the local authority for mapping supported semantic target strings to component-local target geometry and direct component transform context.

Raw OCCT topology ids are never persistent assembly target identity.

## Local residual and numeric path

Canonical families reuse these residual concepts:

```text
Mate:
  normal_opposition    = nA + nB
  signed_separation_mm = dot(oB - oA, nA)

Distance:
  normal_parallelism   = cross(nA, nB)
  distance_residual_mm = dot(oB - oA, nA) - target_distance_mm

Concentric:
  direction_parallelism = cross(dA, dB)
  axis_offset_mm         = cross(oB - oA, dA)

Insert:
  direction_parallelism       = cross(dA, dB)
  axis_offset_mm               = cross(oB - oA, dA)
  signed_seating_separation_mm = dot(sB - sA, nA)

Angle:
  angle_alignment = dot(nA, nB) - cos(target_angle_deg)
```

Each free active component in one ordinary local solve contributes six direct transform variables.

The shared numeric engine provides scaled residual flattening, central finite differences, damped Gauss-Newton normal equations, deterministic damping escalation, backtracking, explicit solve states, complete component snapshots, and transform proposals.

`AssemblyRigidBodySolver` supplies local geometric relationships.

`AssemblyJointMotionSolver` supplies local geometric relationships plus transient Revolute drives. There is no second motion optimizer.

Suppressed local components and constraints touching them leave the active numeric subgroup, while complete snapshots still protect application freshness.

Only explicit successful result application changes persistent solver placement or a selected joint coordinate.

## Local rank and remaining DOF

`AssemblySolveDiagnosticsAnalyzer` evaluates the same finite-difference Jacobian used by ordinary local solving.

```text
variable_count  = 6 * free_active_component_count
constrained_dof = rank(J)
remaining_dof   = variable_count - rank(J)
```

Covered regular one-free-body results include:

```text
Concentric: rank 4/6, remaining DOF 2
Insert:     rank 5/6, remaining DOF 1
Angle:      rank 1/6, remaining DOF 5 away from extremal targets
```

## Revolute joint motion seed

Persistent local joint intent is separate from geometric relationships.

The first family is `AssemblyJointType::Revolute` with persistent target A/B semantic references, active/inactive state, principal-angle limits, and authored current coordinate.

Motion reuses `.seat` target geometry and directed axis/seating/twist drive residuals through the shared numeric engine.

Other active Revolute joints in the same local combined relationship group are held at their persisted coordinates while the selected joint is driven.

Cross-hierarchy joints remain deferred.

## Document-scoped flexible child solving

Canonical document: `docs/assembly-flexible-subassembly-solving-mvp5.md`.

`AssemblyFlexibleSubassemblySolver` selects one exact active non-root occurrence path and copies the referenced child `AssemblyDocument` into a temporary Project as the local solve root together with project-owned parts.

The existing local graph, target resolver, numeric system, solver, snapshots, and proposal contract are reused.

Successful application atomically writes direct component transforms back to the selected project-owned child document.

The selected `SubassemblyInstance::transform()` remains rigid and unchanged.

Repeated occurrences of one child document share one internal component pose because they share one component transform authority per child-document component record.

## Cross-hierarchy read-only geometric semantics

Canonical document: `docs/assembly-cross-hierarchy-relationship-semantics-mvp5.md`.

The existing Geometry layer resolves exact occurrence-qualified targets into root-assembly space and reuses the canonical Mate, Distance, Angle, Concentric, and Insert residual families.

Its mathematical endpoint shape is:

```text
(occurrence_path,
 local ComponentInstanceId,
 semantic_reference)
```

The empty path addresses the root occurrence. Non-empty paths are exact root-to-current `SubassemblyInstanceId` sequences.

`AssemblyHierarchyConstraintTargetResolver` resolves the exact occurrence, delegates local feature semantics to `AssemblyConstraintTargetResolver`, and evaluates:

```text
[component, inner parent, ..., outer parent]
```

through `AssemblyHierarchyTransformEvaluator`.

Visibility and suppression do not change pure target-resolution geometry.

## Persistent cross-hierarchy geometric relationship intent

Canonical document: `docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`.

Block 23 introduces the Core-owned persistence endpoint:

```text
AssemblyHierarchyConstraintEndpoint =
  (occurrence_path,
   local ComponentInstanceId,
   semantic_reference)
```

`AssemblyHierarchyConstraint` is persistent project-level geometric relationship intent with:

```text
id
name
type: Mate | Concentric | Distance | Insert | Angle
target_a
target_b
state
optional distance
optional angle
```

The record reuses local relationship value-family rules and preserves target A/B order.

`Project` owns:

```text
cross_hierarchy_constraints
```

through add, collection, count, and lookup APIs.

Project-level cross-hierarchy ids are unique within that collection. Local relationship ids remain scoped by containing `AssemblyDocument`.

Block 23 validates endpoint value-local identity and relationship value-family intent only. It does not resolve occurrence paths, reached components, or semantic geometry and never mutates component or subassembly transforms.

`AssemblyHierarchyConstraintQuery::create` accepts Core endpoints and complete persistent `AssemblyHierarchyConstraint` records, bridging persistent Core identity into the existing derived Geometry query layer.

The Geometry query target remains a derived resolver seed and is not save-format authority.

The current project JSON format does not serialize `cross_hierarchy_constraints` yet.

## Cross-hierarchy identity split

Future cross-hierarchy solving preserves three distinct identities:

```text
geometric endpoint
  = (occurrence_path, local ComponentInstanceId, semantic_reference)

geometric component occurrence
  = (occurrence_path, local ComponentInstanceId)

persisted transform authority
  = (assembly_document: DocumentId, local ComponentInstanceId)
```

Two rooted occurrences may have different geometry while mapping to one shared transform authority.

Therefore future numeric variables, component snapshots, proposals, and direct transform application are keyed by transform authority rather than blindly by occurrence path.

## Relationship-to-authority solve connectivity

Future cross-hierarchy solve partitioning uses a bipartite relationship-to-transform-authority incidence model.

Relationship nodes are:

```text
local relationship
  = (assembly_document, AssemblyConstraintId)

project-level cross-hierarchy relationship
  = project-level AssemblyConstraintId
```

Authority nodes are:

```text
(assembly_document, ComponentInstanceId)
```

A local relationship appears once in its containing assembly document and is evaluated once in that local assembly space.

A project-level cross-hierarchy relationship preserves exact occurrence-qualified endpoints and is evaluated in root-assembly space.

Each relationship is incident to every unique transform authority whose candidate direct component transform can affect the residual.

Solve groups are connected components of that active incidence graph. Pure-local groups remain ordinary local solver work. The future cross-hierarchy solver selects groups containing at least one active project-level cross-hierarchy relationship.

Local relationship participation uses local state and local component suppression. Cross-hierarchy relationship participation additionally evaluates suppression along each exact endpoint occurrence path. Visibility remains a presentation/export property.

The complete sequence is canonical in `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

## Posed geometry and analysis

`AssemblyPosedLeafShapeBuilder` and `AssemblyStepExporter` consume the canonical flattened leaf boundary.

The posed pipeline validates project structure, derives visible-active flattened leaves, recomputes each unique referenced part once into one per-consumer `ShapeCache`, reuses caches across repeated occurrences, copies and poses each leaf through exact authored transform chains, and builds one derived OCCT compound for STEP export.

`AssemblyInterferenceAnalyzer` evaluates deterministic unordered posed-leaf pairs and classifies finite positive common solid volume above tolerance as interference.

`AssemblyClearanceAnalyzer` preserves interference records and evaluates minimum distance for remaining pairs. Touching without positive common volume is a zero-distance clearance violation.

Analysis products are derived and unpersisted.

## Persistence boundary

Current persistent model intent includes:

```text
project identity and explicit root assembly
project-owned child assembly documents
project-owned part documents
parameters, formulas, and bindings
component placement/state/grounding
rigid subassembly occurrence placement/state
local geometric relationships
local joint/limit/coordinate records
project-owned AssemblyHierarchyConstraint records
```

The new cross-hierarchy relationship records are currently in-memory Core model intent only. Project JSON does not yet roundtrip them.

Derived and unpersisted data includes graph connectivity, hierarchy traversal descriptors, occurrence paths, parent transform chains, flattened leaves, target-resolution views, Geometry query target seeds, resolved semantic geometry, root-space hierarchy target descriptors, residual descriptors, motion drive sets, relationship-to-authority incidence, numeric residuals/Jacobians, solver state, solve/motion results, snapshots, proposals, diagnostics, `ShapeCache` values, posed leaf shapes, interference/clearance products, OCCT compounds, and STEP entity identity.

Only explicit successful application changes persisted component solver placement or a selected authored joint coordinate. Rigid subassembly placement/state changes only through explicit occurrence edits until a dedicated whole-subassembly solve-variable contract exists.

## Next assembly block

Block 24 is additive cross-hierarchy constraint JSON and Core structure validation.

It must add backward-compatible `cross_hierarchy_constraints[]` project JSON, preserve endpoint occurrence-path and target A/B order exactly, roundtrip Distance/Angle values, load files without the field as an empty collection, and validate that endpoint paths resolve and addressed local components exist after the assembly hierarchy itself has been validated.

It must not add incidence graphs, numeric variables, solving, snapshots, diagnostics, or application.

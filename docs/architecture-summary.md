# Architecture Summary

This document summarizes implemented BLCAD architecture and current direction. Feature-specific MVP documents remain canonical for exact contracts, formulas, JSON spellings, ordering, failure policy, and focused proofs.

## Goal and authority model

BLCAD is an independent parametric CAD system for Linux. BLCAD stores semantic model intent and uses OCCT/Open CASCADE as a computed geometry kernel.

Fundamental decisions:

- do not fork FreeCAD;
- keep a custom CAD core above OCCT;
- persist model intent rather than final BRep authority;
- persist semantic references rather than raw OCCT topology ids;
- keep geometric constraints separate from motion-joint intent;
- keep authored hierarchy transforms separate from derived traversal/composition;
- keep Core model intent below Geometry execution/query types;
- keep GUI code above the CAD core;
- require explicit fresh-result application before solver output changes persistent state.

## Layer direction

```text
user interface
command/application layer
Core parametric model intent
sketch / construction / semantic-reference layers
part feature history
assembly relationship / motion / hierarchy intent
Core derived connectivity and authority identities
Geometry target/equation evaluation
shared numeric residual/Jacobian execution
freshness validation and atomic application
engineering diagnostics/analysis consumers
OCCT geometry kernel
file and exchange formats
```

A lower authority layer must not depend on a higher execution layer for persistent model identity. Save-format authority must not be a `blcad::geometry` query/result type.

## Part model

Implemented part architecture includes typed quantities/parameters, unit-aware local formulas, datum planes and derived workplanes, construction geometry, line/arc/spline/circle/rectangle/composite/detected sketch profiles, projected/reference-driven sketch geometry, semantic generated references, reference recovery helpers, sketch constraints/dimensions/diagnostics/repair helpers, additive/subtractive extrude intent, `CircularHolePattern`, dependency/invalidation/recompute planning, optional OCCT recompute through `ShapeCache`, STEP export, and JSON model-intent serialization.

OCCT shapes are computed cache products.

## Project ownership

`Project` owns:

```text
one explicit root AssemblyDocument
Project-owned child AssemblyDocument records
Project-owned PartDocument records
Project-owned AssemblyHierarchyConstraint records
```

`Project::assembly()` is the explicit root assembly API.

Child assembly documents are model definitions independent from rooted occurrence placement. A child enters the rooted assembly tree only through a containing `SubassemblyInstance`.

Assembly document ids are unique across root and child collection. The root may not be referenced as a child.

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

The derived cross-hierarchy mutation identity is:

```text
ComponentTransformAuthority =
  (assembly_document: DocumentId,
   local ComponentInstanceId)
```

This identity is not separately persisted. It addresses the owning `ComponentInstance` record.

Repeated rooted occurrences of one child document can read and mutate the same child component transform authority.

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

Its transform is the authored rigid boundary from one containing assembly occurrence to the referenced child assembly document.

A `SubassemblyInstance` has no grounding field and is not a six-variable solve node.

Component solving/application never converts a composed hierarchy placement into a component transform or `SubassemblyInstance` proposal.

## Hierarchy validity and traversal

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

Hierarchy descriptors, parent links, and composed transform chains are derived and unpersisted.

Persistent cross-hierarchy endpoint paths are authored identity stored in relationship intent and JSON.

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

The hierarchy is not persisted as a composed matrix or recomputed Euler triple.

## Visible-active leaf flattening

`AssemblyLeafOccurrenceResolver` is the canonical hierarchy-to-leaf boundary for posed geometry consumers.

A leaf stores containing assembly identity, exact subassembly path, local component identity, referenced part document, and exact inner-to-outer transform chain.

Hidden or suppressed subassembly occurrences remove descendant posed leaves. Hidden or suppressed local components are absent from flattened posed leaves.

Leaf flattening is derived and unpersisted.

## Local geometric relationship solving

Persistent local relationship families are:

```text
Mate
Concentric
Distance
Insert
Angle
```

Local target identity is:

```text
(local ComponentInstanceId, semantic_reference)
```

`AssemblyConstraintGraph` derives deterministic local active connectivity.

`AssemblyConstraintTargetResolver` resolves supported planar face, `.axis`, and `.seat` targets.

Every free active component contributes six direct transform variables:

```text
tx_mm ty_mm tz_mm rx_deg ry_deg rz_deg
```

The shared numeric path owns canonical residual flattening, central finite differences, and damped Gauss-Newton solving.

`AssemblySolveResult` is derived/unpersisted and contains complete component snapshots, exact semantic target PartDocument snapshots, and transform proposals.

## Exact semantic target PartDocument freshness

Block 27 chooses a conservative exact model-intent freshness contract used by ordinary local and cross-hierarchy solve results.

`AssemblySemanticTargetPartSnapshot` stores:

```text
part_document
canonical_model_intent_json
```

The payload is exactly:

```text
serialize_part_document_to_json(part)
```

At result application the current PartDocument is serialized again and compared byte-for-byte.

No hash and no mutable revision counter are used.

This protects every serialized model-intent edit in a participating referenced PartDocument. It is intentionally broader than a minimal semantic-target dependency closure.

Snapshots are derived and unpersisted.

The ordinary local applier, flexible-child application, local Revolute motion application, and cross-hierarchy application all reuse this semantic model freshness boundary.

## Shared numeric execution boundary

The internal numeric system exposes:

```text
absolute candidate variable vector
  -> deterministic scaled residual vector
```

The shared central finite-difference implementation perturbs one variable at a time using the established translation and rotation steps.

`solve_numeric_variables` owns option validation, initial residual evaluation, grounded-reference policy, fixed inconsistency, central finite differences, damped Gauss-Newton normal equations, partial-pivot dense solve, damping escalation, backtracking, and solve-state classification.

Ordinary local assembly solving, joint motion, and cross-hierarchy solving adapt their model-specific transform ownership/residual evaluation to this one numeric boundary.

There is no second cross-hierarchy optimizer or finite-difference implementation.

## Local Revolute motion

Persistent local joint intent is separate from geometric constraints.

The first family is `AssemblyJointType::Revolute` with local target A/B, active/inactive state, principal-angle limits, and authored current coordinate.

Motion reuses `.seat` axis/oriented-frame semantics and the shared numeric engine.

A selected joint receives the requested coordinate. Other active Revolute joints in the same combined local geometric/joint closure receive transient holding drives at authored coordinates.

`AssemblyJointMotionResultApplier` validates complete joint inputs, then applies the embedded ordinary `AssemblySolveResult` on a candidate Project copy. It therefore inherits exact semantic PartDocument freshness before committing component transforms and the selected coordinate atomically.

Project-level cross-hierarchy joints remain Block 28.

## Document-scoped flexible child solving

`AssemblyFlexibleSubassemblySolver` selects one exact active non-root occurrence path and copies the referenced child `AssemblyDocument` into a temporary local-root Project together with Project-owned parts.

The ordinary local solver and application contract are reused.

Successful application writes direct child component transforms back to the Project-owned child document. Selected/ancestor `SubassemblyInstance::transform()` values remain rigid and unchanged.

Repeated occurrences of one child document share the same internal component pose.

Because the embedded result is an ordinary `AssemblySolveResult`, exact semantic PartDocument freshness is inherited automatically.

## Cross-hierarchy geometric endpoint semantics

The geometric endpoint identity is:

```text
(occurrence_path,
 local ComponentInstanceId,
 semantic_reference)
```

The empty path addresses the root assembly occurrence. Non-empty paths are exact root-to-current `SubassemblyInstanceId` sequences.

`AssemblyHierarchyConstraintTargetResolver` resolves the exact rooted occurrence, delegates local semantic feature meaning to `AssemblyConstraintTargetResolver`, and evaluates the component plus parent chain into root space.

`AssemblyHierarchyConstraintEquationBuilder` reuses canonical Mate, Distance, Angle, Concentric, and Insert residual descriptors.

Geometry query products are derived and unpersisted.

## Persistent cross-hierarchy geometric intent and JSON

`AssemblyHierarchyConstraintEndpoint` is Core-owned persistent endpoint identity.

`AssemblyHierarchyConstraint` stores Project-level Mate, Concentric, Distance, Insert, or Angle intent with exact target A/B order and state.

Project JSON stores:

```text
cross_hierarchy_constraints[]
```

Files without the field load with an empty collection.

`Project::validate_assembly_structure()` validates ordinary structure and complete hierarchy before following each exact endpoint path from the root and requiring its addressed local component in the reached document.

Core structure validation does not resolve semantic feature geometry.

## Cross-hierarchy active incidence and solve groups

`AssemblyCrossHierarchyConstraintGraph` is the Core-derived connectivity layer.

Relationship identities are:

```text
AssemblyLocalRelationshipIdentity
  = (assembly_document, AssemblyConstraintId)

AssemblyProjectCrossHierarchyRelationshipIdentity
  = Project-level AssemblyConstraintId
```

Local constraints are collected once per containing `AssemblyDocument`.

Cross participation additionally requires every boundary on both exact endpoint paths and both addressed components to be active.

Visibility is ignored for solve participation.

Every participating relationship maps to each unique `ComponentTransformAuthority` whose candidate direct transform affects its residual.

Endpoint occurrence context remains separately available in target-A/target-B endpoint mappings.

A connected bipartite component containing at least one Project-level cross relationship becomes one deterministic `AssemblyCrossHierarchySolveGroup`.

Pure-local components remain ordinary local solver work.

## Cross-hierarchy numeric solving

`AssemblyCrossHierarchyConstraintSolver` solves one exact current cross-hierarchy solve group without mutating the source Project.

Each unique free active authority contributes one six-variable block in canonical authority order. Grounded authorities remain fixed residual context.

Local relationships evaluate once in containing-document local space.

Project-level cross relationships evaluate through exact occurrence parent chains in root space.

All five geometric families use the shared residual flattening and central finite-difference implementations.

A candidate authority transform is applied once to a Project copy. Repeated rooted endpoints can still produce different root-space geometry because they follow different parent chains.

## Cross-hierarchy freshness and atomic application

Canonical document: `docs/assembly-cross-hierarchy-application-diagnostics-mvp5.md`.

`AssemblyCrossHierarchySolveResult` stores:

```text
solve state and residual summary
selected relationship identities
fixed authorities
complete authority snapshots
complete local/cross relationship snapshots
complete participating hierarchy boundary snapshots
exact semantic target PartDocument snapshots
one proposal per free authority
```

Authority snapshots protect referenced part identity, grounding, suppression, and source direct transform.

Relationship snapshots protect complete local/cross records including exact targets and explicit Distance/Angle values.

The Block-25 graph is rebuilt at apply time and the exact result relationship/authority solve group must still exist. New active relationships joining the group therefore stale an old result.

Every `SubassemblyInstance` on participating cross endpoint paths is snapshotted by persistent boundary authority:

```text
(containing AssemblyDocumentId,
 local SubassemblyInstanceId)
```

Boundary snapshots protect referenced child assembly, suppression, and direct boundary transform. Visibility is deliberately not a solve input.

`AssemblyCrossHierarchySolveResultApplier` validates all authority/proposal, relationship/group, hierarchy-boundary, and semantic PartDocument freshness before mutation.

It then applies direct component transform proposals on a Project copy and replaces the source only after every write succeeds.

No composed hierarchy or `SubassemblyInstance` transform is written.

## Shared local/cross matrix-rank diagnostics

The deterministic partial-pivot matrix-rank implementation is shared through:

```text
compute_assembly_matrix_rank
```

Both local and cross-hierarchy diagnostics use:

```text
pivot_threshold = max(absolute_tolerance,
                      relative_tolerance * maximum_abs_jacobian_entry)
```

`AssemblyCrossHierarchySolveDiagnosticsAnalyzer` first obtains a Block-26 result, applies it only to an evaluation Project copy through the fresh Block-27 applier, and then uses the exact same authority read/apply and mixed residual functions as the solver to build the finite-difference Jacobian.

Free authority order is exactly the Block-26 proposal order:

```text
solve_group.authorities
  -> filter Free
```

Therefore:

```text
variable_count = 6 * unique_free_active_transform_authority_count
constrained_dof = rank(J)
remaining_dof = variable_count - rank(J)
```

Repeated rooted occurrences sharing one free authority contribute six variables, not twelve.

## Posed geometry and analysis

`AssemblyPosedLeafShapeBuilder` and `AssemblyStepExporter` consume canonical flattened leaves.

The posed pipeline validates Project structure, derives visible-active leaves, recomputes each unique referenced part once into a per-consumer `ShapeCache`, reuses caches across repeated occurrences, poses each leaf through exact authored transform chains, and builds derived OCCT compounds for STEP export.

`AssemblyInterferenceAnalyzer` evaluates deterministic unordered posed-leaf pairs and classifies positive common solid volume above tolerance as interference.

`AssemblyClearanceAnalyzer` preserves interference records and evaluates minimum distance for remaining pairs.

Analysis products are derived and unpersisted.

## Persistence boundary

Persistent model intent currently includes:

```text
Project identity and explicit root assembly
Project-owned child assembly documents
Project-owned part documents
parameters, formulas, and bindings
component referenced part / placement / state / grounding
rigid subassembly occurrence placement/state
local geometric relationships
local Revolute joint/limit/coordinate records
Project-owned AssemblyHierarchyConstraint records
```

Cross-hierarchy geometric intent roundtrips through `cross_hierarchy_constraints[]`.

Derived and unpersisted products include graph connectivity, hierarchy occurrence descriptors, parent transform chains, flattened leaves, resolved target geometry, residual descriptors/vectors, transient motion drives, transform authorities, relationship/endpoint incidence, solve groups, finite-difference Jacobians, solver state, component/authority/relationship/boundary/semantic-PartDocument snapshots, proposals, freshness validation products, rank products, diagnostics, shape caches, posed leaf shapes, interference/clearance products, OCCT compounds, and STEP entity identity.

## Next assembly block

Block 28 is persistent occurrence-qualified Project-level cross-hierarchy Revolute joint intent and nested motion propagation.

It must reuse:

```text
exact hierarchy endpoint identity
ComponentTransformAuthority mapping
combined relationship connectivity discipline
root-space exact parent-chain target evaluation
local Revolute directed-axis/seating/signed-twist mathematics
shared numeric evaluator and Gauss-Newton engine
Block-27 semantic/authority/relationship/boundary freshness principles
Project-copy atomic application
```

Motion connectivity must close over local geometric relationships, local joints, Project-level cross-hierarchy geometric relationships, and Project-level cross-hierarchy Revolute joints.

Cross-hierarchy joints, not whole-subassembly variables, become the next persistent assembly intent boundary.

Occurrence-local child pose overrides, component geometry instancing, and swept-motion contact analysis remain deferred.

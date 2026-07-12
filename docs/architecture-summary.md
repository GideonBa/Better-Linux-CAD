# Architecture Summary

This document summarizes implemented BLCAD architecture and the current direction. Feature-specific MVP documents remain canonical for exact contracts, formulas, failure policies, JSON spellings, ordering rules, and focused proofs.

## Goal and authority model

BLCAD is an independent parametric CAD system for Linux. BLCAD stores semantic model intent and uses OCCT/Open CASCADE as a computed geometry kernel.

Fundamental decisions:

- do not fork FreeCAD;
- keep a custom CAD core above OCCT;
- persist model intent rather than final BRep authority;
- persist semantic references rather than raw OCCT topology ids;
- keep geometric assembly constraints separate from motion-joint intent;
- keep authored hierarchy transforms separate from derived traversal and composed evaluation;
- keep Core model intent below Geometry execution/query types;
- keep GUI code above the CAD core;
- require explicit application before solver results change persistent model state.

## Layer direction

```text
user interface
command/application layer
Core parametric model intent
sketch / construction / semantic-reference layers
part feature history
assembly relationship / motion / hierarchy intent
Core derived connectivity/query identities
Geometry target/equation evaluation
shared numeric residual/Jacobian execution
engineering analysis consumers
OCCT geometry kernel
file and exchange formats
```

A lower authority layer must not depend on a higher execution layer for persistent model identity. Save-format authority must not be a `blcad::geometry` query type.

## Part model

Implemented part architecture includes typed quantities/parameters, unit-aware part-local parameter expressions, datum planes and derived workplanes, construction geometry, line/arc/spline/circle/rectangle/composite/detected sketch profiles, projected/reference-driven sketch geometry, semantic generated references, recovery helpers, sketch constraints/dimensions/diagnostics/repair helpers, additive and subtractive extrude intent, `CircularHolePattern`, dependency/invalidation/recompute planning, optional OCCT recompute through `ShapeCache`, STEP export, and JSON model-intent serialization.

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

Assembly document ids are unique across the root and project-owned child collection. The root may not be referenced as a child.

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

The identity is not a separate persisted model record. It identifies the persistent `ComponentInstance` whose direct transform, grounding, and suppression state form the authority-scoped solve input.

Repeated rooted occurrences of one child assembly may read the same child-document transform authority.

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

A `SubassemblyInstance` currently has no grounding field and is not a six-variable solve node.

Component solving never treats the boundary transform as a composed component variable or proposal target.

## Hierarchy validity and traversal

Project structure validation rejects invalid child references, root-as-child references, direct self-reference, indirect cycles, invalid component/member relationships, invalid local relationship/joint endpoints, and invalid cross-hierarchy endpoint path/component structure.

`AssemblyHierarchyTraversal` derives the rooted occurrence tree in deterministic root-first pre-order after complete Project structure validation. Child occurrences are ordered lexicographically by local `SubassemblyInstanceId`.

One `AssemblyHierarchyOccurrenceDescriptor` carries:

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

Persistent cross-hierarchy endpoint `occurrence_path` values are different: they are authored endpoint identity stored inside `AssemblyHierarchyConstraintEndpoint` and Project JSON.

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

Points rotate then translate. Vectors, normals, and directions rotate only.

For:

```text
chain = [T_component, T_inner, T_outer]
```

point evaluation is:

```text
T_outer(T_inner(T_component(p)))
```

The hierarchy is not persisted as a composed matrix or converted back into a recomputed Euler triple.

## Visible-active leaf flattening

`AssemblyLeafOccurrenceResolver` is the canonical hierarchy-to-leaf boundary for posed geometry consumers.

A leaf descriptor stores containing assembly identity, exact subassembly occurrence path, local component identity, referenced part document, and exact inner-to-outer transform chain.

Hidden or suppressed subassembly occurrences remove their descendant subtree. Hidden or suppressed local components are absent from flattened posed leaves.

Flattened leaves are derived and unpersisted.

## Local geometric relationship intent and solving

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

Local constraints belong to one containing `AssemblyDocument`. Target A/B order and active/inactive state are persistent. Distance carries a length quantity and Angle carries a degree quantity.

`AssemblyConstraintGraph` derives deterministic local active relationship connectivity.

`AssemblyConstraintTargetResolver` maps supported local semantic target strings to component-local geometry and direct component transform context.

Implemented semantic target strings include:

```text
feature.<feature-id>.top|bottom|right|left|front|back
feature.<feature-id>.axis
feature.<feature-id>.seat
```

Canonical residual families are Mate, Distance, Angle, Concentric, and Insert.

Each free active local component in an ordinary local solve contributes six direct transform variables:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

The local rigid-body solver and Revolute joint motion continue through the same local `solve_numeric_relationships` adapter.

Only explicit successful application changes persistent component solver placement or a selected authored joint coordinate.

## Shared numeric execution boundary

The internal numeric system now exposes one model-agnostic evaluator contract:

```text
absolute candidate variable vector
  -> deterministic scaled residual vector
```

The shared central finite-difference implementation perturbs one variable at a time using the existing translation and rotation step semantics.

The shared `solve_numeric_variables` engine owns:

```text
option validation
initial residual evaluation
grounded-reference contract
fixed-geometry consistency state
central finite-difference Jacobian
damped Gauss-Newton normal equations
partial-pivot dense solve
damping escalation
backtracking line search
convergence / iteration-limit / numerical-failure state
```

Ordinary local assembly solving, joint motion, and cross-hierarchy solving adapt their model-specific transform ownership and residual evaluation to this one numeric boundary.

There is no second cross-hierarchy optimizer and no second central finite-difference implementation.

## Revolute joint motion seed

Persistent local joint intent is separate from geometric constraints.

The first family is `AssemblyJointType::Revolute` with persistent target A/B semantic references, active/inactive state, principal-angle limits, and authored current coordinate.

Motion reuses `.seat` axis/oriented-frame target semantics and the shared numeric engine.

Cross-hierarchy joints remain deferred.

## Document-scoped flexible child solving

`AssemblyFlexibleSubassemblySolver` selects one exact active non-root occurrence path and copies the referenced child `AssemblyDocument` into a temporary Project as the local solve root together with project-owned parts.

The existing local graph, target resolver, numeric system, solver, snapshots, and proposal contract are reused.

Successful application writes direct component transforms back to the selected project-owned child document. The selected `SubassemblyInstance::transform()` remains rigid and unchanged.

Repeated occurrences of one child document share the same internal component pose. This is document-scoped flexible solving, not occurrence-local internal pose overriding.

## Cross-hierarchy read-only geometric semantics

Canonical document: `docs/assembly-cross-hierarchy-relationship-semantics-mvp5.md`.

The geometric endpoint identity is:

```text
(occurrence_path,
 local ComponentInstanceId,
 semantic_reference)
```

The empty path addresses the root assembly occurrence. Non-empty paths are exact root-to-current `SubassemblyInstanceId` sequences.

`AssemblyHierarchyConstraintTargetResolver` resolves the exact rooted occurrence, delegates local semantic feature meaning to `AssemblyConstraintTargetResolver`, and evaluates the component plus parent transform chain into root-assembly space.

`AssemblyHierarchyConstraintEquationBuilder` reuses canonical Mate, Distance, Angle, Concentric, and Insert residual descriptors.

Geometry query products are derived and unpersisted.

## Persistent cross-hierarchy relationship intent and JSON

Canonical documents:

- `docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`
- `docs/assembly-cross-hierarchy-constraint-json-mvp5.md`

`AssemblyHierarchyConstraintEndpoint` is Core-owned persistent endpoint identity.

`AssemblyHierarchyConstraint` stores Project-level Mate, Concentric, Distance, Insert, or Angle intent with exact target A/B order and active/inactive state.

`Project` owns the cross-hierarchy collection. Its ids are unique within that collection and independent from local document-scoped constraint ids.

Project JSON stores the additive field:

```text
cross_hierarchy_constraints[]
```

Files without the field load with an empty collection.

`Project::validate_assembly_structure()` validates ordinary structure and the complete assembly hierarchy before following each cross-hierarchy endpoint path from the root and requiring its local component to exist in the reached document.

Structure validation is Core-only and does not resolve semantic feature geometry.

## Cross-hierarchy active incidence and solve groups

Canonical document: `docs/assembly-cross-hierarchy-incidence-groups-mvp5.md`.

`AssemblyCrossHierarchyConstraintGraph` is the Core-derived connectivity layer.

Relationship identities are:

```text
AssemblyLocalRelationshipIdentity
  = (assembly_document, AssemblyConstraintId)

AssemblyProjectCrossHierarchyRelationshipIdentity
  = Project-level AssemblyConstraintId
```

The public relationship identity is their `AssemblyRelationshipIdentity` variant.

Local constraints are collected once per containing `AssemblyDocument`, not once per rooted occurrence.

Local participation requires active relationship state and active target components.

Cross-hierarchy participation additionally requires every `SubassemblyInstance` on both exact endpoint paths and both addressed components to be active.

Visibility is ignored for solve participation.

Every participating relationship maps to each unique `ComponentTransformAuthority` whose candidate direct local transform can affect its residual.

```text
relationship node
      |
      | AssemblyRelationshipAuthorityIncidence
      v
ComponentTransformAuthority
```

Incidence is unique per `(relationship, authority)` pair.

Cross-hierarchy endpoint occurrence context remains separate in `AssemblyCrossHierarchyEndpointAuthorityMapping`.

Two endpoints can therefore retain different occurrence paths while mapping to one shared authority.

`AssemblyCrossHierarchySolveGroup` is one connected component of the bipartite active graph containing at least one Project-level cross-hierarchy relationship.

Pure-local components remain ordinary local solver work and are omitted from `solve_groups()`.

Ordering is deterministic and insertion-order independent.

## Cross-hierarchy numeric solving

Canonical document: `docs/assembly-cross-hierarchy-numeric-solver-mvp5.md`.

`AssemblyCrossHierarchyConstraintSolver` solves one exact current `AssemblyCrossHierarchySolveGroup` without applying the result.

At solve start it rebuilds the Block-25 graph and requires the supplied group to equal one current deterministic solve group. Participation changes therefore invalidate an old selected group before numeric work.

Each unique free active `ComponentTransformAuthority` contributes one six-variable direct-transform block in canonical authority order.

Grounded authorities remain fixed residual context and snapshots but contribute no variables.

Local relationships are evaluated once in the containing assembly document's local assembly space through a temporary document-as-local-root Project view and the existing local numeric relationship path.

Project-level cross-hierarchy relationships are evaluated in root-assembly space through the existing `AssemblyHierarchyConstraintEquationBuilder`.

All five residual families use one shared flattening implementation and the established scalar order/length scaling:

```text
Mate        4 residual components
Distance    4 residual components
Angle       1 residual component
Concentric  6 residual components
Insert      7 residual components
```

Complete residual blocks are concatenated in exact Block-25 relationship order.

A candidate authority transform is applied once to a Project copy. Every local and cross-hierarchy residual depending on that authority observes the same candidate transform. Repeated rooted endpoints can still produce different root-space geometry because they follow different parent chains.

`AssemblyCrossHierarchySolveResult` stores:

```text
solve state
iteration count
selected relationship identities
fixed authorities
complete participating authority snapshots
at most one proposal per free authority
residual summary
```

Snapshots and proposals are authority-qualified by `(assembly_document, ComponentInstanceId)`.

No Block-26 result is applied automatically. The source Project and every `SubassemblyInstance::transform()` remain unchanged.

## Posed geometry and analysis

`AssemblyPosedLeafShapeBuilder` and `AssemblyStepExporter` consume the canonical flattened leaf boundary.

The posed pipeline validates project structure, derives visible-active leaves, recomputes each unique referenced part once into a per-consumer `ShapeCache`, reuses caches across repeated occurrences, poses each leaf through exact authored transform chains, and builds one derived OCCT compound for STEP export.

`AssemblyInterferenceAnalyzer` evaluates deterministic unordered posed-leaf pairs and classifies finite positive common solid volume above tolerance as interference.

`AssemblyClearanceAnalyzer` preserves interference records and evaluates minimum distance for remaining pairs.

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

Cross-hierarchy relationship intent roundtrips through `cross_hierarchy_constraints[]`.

Derived and unpersisted data includes local graph connectivity, hierarchy traversal descriptors, parent transform chains, flattened leaves, target-resolution views, root-space target descriptors, residual descriptors, motion drive sets, `ComponentTransformAuthority` values, relationship identities, relationship-to-authority incidence, endpoint-to-authority mappings, cross-hierarchy solve groups, mixed scaled residual vectors, finite-difference Jacobians, solver state, authority snapshots, authority proposals, diagnostics, `ShapeCache` values, posed leaf shapes, interference/clearance products, OCCT compounds, and STEP entity identity.

## Next assembly block

Block 27 is complete fresh-result validation, atomic authority-qualified application, and cross-hierarchy rank/remaining-DOF diagnostics.

Freshness must protect participating authority inputs, complete local/cross relationship records, and every participating hierarchy boundary that affects root-space geometry or suppression participation.

Block 27 must explicitly choose whether to add a stable semantic target-producing model revision snapshot or preserve and document the current local-solver limitation for PartDocument/feature edits.

Diagnostics must reuse the exact Block-26 free-authority variable order and mixed residual/Jacobian evaluator:

```text
variable_count = 6 * unique_free_active_transform_authority_count
constrained_dof = rank(J)
remaining_dof = variable_count - rank(J)
```

Cross-hierarchy joints, nested motion propagation, occurrence-local internal pose overrides, and whole-subassembly solve variables remain deferred.

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
diagnostics / analysis / exchange consumers
OCCT geometry kernel
file and exchange formats
```

A lower authority layer must not depend on a higher execution layer for persistent identity. Save-format authority is never a `blcad::geometry` query/result type.

## Part model

Implemented part architecture includes typed quantities/parameters, unit-aware local formulas, datum/derived workplanes, construction geometry, rich sketch/profile seeds, projected/reference-driven geometry, semantic generated references and recovery, sketch constraints/dimensions/diagnostics/repair helpers, additive/subtractive extrude intent, `CircularHolePattern`, dependency/invalidation/recompute planning, optional OCCT execution through `ShapeCache`, STEP export, and JSON model-intent serialization.

OCCT shapes are computed cache products.

## Project ownership

`Project` owns:

```text
one explicit root AssemblyDocument
Project-owned child AssemblyDocument records
Project-owned PartDocument records
Project-owned AssemblyHierarchyConstraint records
Project-owned AssemblyHierarchyJoint records
```

`Project::assembly()` is the explicit root API.

Child assembly documents are model definitions independent from rooted occurrence placement. A child enters the rooted tree through a containing `SubassemblyInstance`.

Project-level cross-hierarchy geometric and motion records have separate Project-scoped id collections from local document-scoped constraint/joint ids.

## Component transform authority

Persistent component state is owned by one `ComponentInstance` inside one `AssemblyDocument`:

```text
id
name
referenced_part_document
visibility
suppression_state
grounding_state
direct RigidTransform
```

Derived cross-hierarchy mutation identity:

```text
ComponentTransformAuthority =
  (assembly_document: DocumentId,
   local ComponentInstanceId)
```

This identity is not separately persisted. It addresses the owning component record.

Repeated rooted occurrences of one child document may expose one shared component transform authority.

## Rigid subassembly occurrence intent

A persistent `SubassemblyInstance` stores local id/name, referenced child assembly document, visibility, suppression, and one direct rigid boundary `RigidTransform`.

A `SubassemblyInstance` has no grounding field and is not a six-variable solve node.

Component solve/motion application never converts a composed hierarchy placement into a component transform or subassembly-boundary proposal.

## Hierarchy validity and traversal

Project structure validation rejects invalid child references, root-as-child references, direct/indirect cycles, invalid local component/member/relationship/joint endpoints, and invalid Project-level cross-hierarchy geometric/joint paths or reached components.

`AssemblyHierarchyTraversal` derives deterministic root-first occurrences. Child occurrences are ordered lexicographically by local `SubassemblyInstanceId`.

Occurrence descriptors carry:

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

Traversal descriptors and composed transform chains are derived. Occurrence paths inside Project-level relationship endpoints are persistent authored endpoint identity.

## Rigid-transform semantics

Every authored `RigidTransform` uses millimeter translation and degree rotation with active right-handed fixed-axis X then Y then Z semantics:

```text
R = Rz * Ry * Rx
```

For:

```text
[T_component, T_inner, T_outer]
```

point evaluation is:

```text
T_outer(T_inner(T_component(p)))
```

No composed matrix or recomputed Euler triple becomes persistent authority.

## Posed leaf authority

`AssemblyLeafOccurrenceResolver` is the canonical hierarchy-to-leaf boundary for posed geometry consumers.

Each leaf stores containing assembly identity, exact subassembly occurrence path, local component identity, referenced part document, and exact inner-to-outer transform chain.

Hidden or suppressed hierarchy/component state filters posed leaves. Flattened leaves are derived and unpersisted.

## Local geometric solving

Persistent local geometric families are Mate, Concentric, Distance, Insert, and Angle.

Local target identity is:

```text
(local ComponentInstanceId, semantic_reference)
```

`AssemblyConstraintGraph` derives active connectivity. `AssemblyConstraintTargetResolver` resolves planar face, `.axis`, and `.seat` semantic targets.

Each free active local component contributes:

```text
tx_mm ty_mm tz_mm rx_deg ry_deg rz_deg
```

The shared numeric path owns residual flattening, central finite differences, and damped Gauss-Newton solving.

`AssemblySolveResult` remains derived and contains complete component snapshots, exact semantic PartDocument model-intent snapshots, and transform proposals.

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

At application the current PartDocument is serialized again and compared byte-for-byte.

No hash and no mutable revision counter are used. The contract is conservative and broader than a minimal semantic-target dependency closure.

The ordinary local applier, flexible-child application, local Revolute motion, cross-hierarchy geometric application, and cross-hierarchy Revolute motion all use this freshness boundary.

## Shared numeric execution boundary

The internal numeric contract is:

```text
absolute candidate variable vector
  -> deterministic scaled residual vector
```

One central finite-difference implementation perturbs translation/rotation variables with the established steps.

`solve_numeric_variables` owns option validation, grounded-reference policy, fixed inconsistency, Jacobian construction, damped Gauss-Newton normal equations, partial-pivot dense solve, damping escalation, backtracking, and solve-state classification.

Local solving, local joint motion, cross-hierarchy geometric solving, and cross-hierarchy motion adapt their model-specific ownership/residual evaluation to this one engine.

## Local Revolute motion

Persistent local `AssemblyJoint` intent is separate from geometry. The first family is Revolute with local target A/B, active/inactive state, principal-angle limits, and authored coordinate.

`.seat` resolves an axis plus oriented seating frame.

The selected local joint receives the requested coordinate. Other active local Revolute joints in the same combined local geometric/joint closure receive transient holding drives at authored coordinates.

`AssemblyJointMotionResultApplier` inherits exact component/PartDocument freshness and atomically commits direct transforms plus the selected local authored coordinate.

## Document-scoped flexible child solving

`AssemblyFlexibleSubassemblySolver` selects one exact active non-root occurrence path and copies the referenced child `AssemblyDocument` into a temporary local-root Project together with Project-owned parts.

The ordinary local solver/application contract is reused. Successful application writes direct child component transforms back to the shared child document. Rigid occurrence boundaries remain unchanged.

Repeated occurrences of one child document share one internal pose.

## Cross-hierarchy endpoint and geometric intent

Persistent endpoint identity:

```text
(occurrence_path,
 local ComponentInstanceId,
 semantic_reference)
```

The empty path addresses the root occurrence.

`AssemblyHierarchyConstraintEndpoint` and `AssemblyHierarchyConstraint` are Core-owned persistent types. Project JSON stores:

```text
cross_hierarchy_constraints[]
```

`AssemblyHierarchyConstraintTargetResolver` resolves exact rooted occurrence paths, delegates local semantic meaning, and evaluates component plus parent transforms into root space.

`AssemblyHierarchyConstraintEquationBuilder` reuses Mate/Distance/Angle/Concentric/Insert residual descriptors.

## Cross-hierarchy geometric connectivity and solving

`AssemblyCrossHierarchyConstraintGraph` derives local/Project cross geometric relationship-to-`ComponentTransformAuthority` incidence.

Local constraints appear once per containing document. Cross target A/B occurrence contexts remain separate endpoint mappings even when both map to one authority.

Participation uses relationship state and suppression. Visibility is ignored.

Connected components containing a Project cross geometric relationship become deterministic geometric solve groups.

`AssemblyCrossHierarchyConstraintSolver` allocates one six-variable block per unique free authority. Local relationships evaluate once in document-local space; Project cross relationships evaluate through exact root-space paths.

The shared numeric engine produces unapplied authority-qualified proposals.

## Cross-hierarchy geometric freshness, application, and diagnostics

`AssemblyCrossHierarchySolveResult` protects complete authority inputs, complete local/cross relationship records, all persistent hierarchy boundaries on participating cross paths, exact PartDocument model intent, and one proposal per free authority.

The current geometric graph is rebuilt at apply time and the exact result group must still exist. New active relationships joining/splitting the group stale old results.

`AssemblyCrossHierarchySolveResultApplier` validates all freshness before applying direct authority transforms on a Project copy.

Shared matrix-rank diagnostics use the exact free-authority order and same residual/Jacobian evaluator:

```text
variable_count = 6 * unique_free_active_transform_authority_count
constrained_dof = rank(J)
remaining_dof = variable_count - rank(J)
```

## Project-level cross-hierarchy Revolute intent

Block 28 adds persistent `AssemblyHierarchyJoint` and:

```text
Project::cross_hierarchy_joints
cross_hierarchy_joints[]
```

The first Project-level family remains Revolute and reuses local state, principal-limit, and authored-coordinate semantics.

Targets use the frozen occurrence-qualified endpoint identity. Exactly equal endpoint identities are invalid, but different rooted endpoints may address the same local component and therefore map to one shared transform authority.

Project-level joint ids are independent from local document-scoped joint ids.

Core structure validation follows each exact path after complete cycle-free hierarchy validation and requires the addressed local component in the reached document. Semantic `.seat` geometry remains a Geometry concern.

## Combined cross-hierarchy motion connectivity

`AssemblyCrossHierarchyMotionGraph` derives a separate motion-specific bipartite graph rather than changing the established geometric solve graph.

Canonical relationship-kind order is:

```text
local geometry
local joint
Project cross geometry
Project cross joint
```

Every participating relationship is incident to each unique transform authority on which its residual depends.

Project cross-joint target A/B endpoint mappings remain separate from unique incidence. Same-authority rooted endpoints may therefore produce two endpoint mappings and one relationship-to-authority edge.

Participation uses active relationship/joint state and suppression of local components or every boundary/component on cross paths. Visibility is ignored.

Connected components containing at least one Project-level cross joint become deterministic `AssemblyCrossHierarchyMotionGroup` values.

## Root-space cross-hierarchy Revolute equations

`AssemblyHierarchyRevoluteJointEquationBuilder` resolves each exact `.seat` endpoint through the hierarchy target resolver into root space.

Local and cross-hierarchy Revolute builders call one shared residual constructor:

```text
direction_alignment          = dA - dB
axis_offset_mm               = cross(oB - oA, dA)
signed_seating_separation_mm = dot(sB - sA, nA)

reference_cosine = dot(xA, xB)
reference_sine   = dot(dA, cross(xA, xB))

twist_sine   = reference_sine*cos(target) - reference_cosine*sin(target)
twist_cosine = reference_cosine*cos(target) + reference_sine*sin(target) - 1
```

Shared numeric flattening uses nine scalar components: direction x/y/z, scaled axis offset x/y/z, scaled seating separation, twist sine, twist cosine.

No second Revolute mathematics exists.

## Cross-hierarchy Revolute motion solving

`AssemblyCrossHierarchyJointMotionSolver` selects one active Project-level Revolute joint and requires one current combined motion group containing it.

Drive policy:

```text
selected Project cross joint
  -> requested in-range coordinate

other active local Revolute joints in group
  -> authored local coordinate holding drive

other active Project cross Revolute joints in group
  -> authored Project coordinate holding drive

all active geometric relationships in group
  -> ordinary geometric residuals
```

One candidate direct transform is applied once per authority to a Project copy. All dependent local/cross geometric and local/cross joint residuals observe the same perturbation.

Repeated rooted endpoints sharing one free authority contribute one six-variable block and at most one proposal.

## Cross-hierarchy motion freshness and atomic application

`AssemblyCrossHierarchyJointMotionResult` stores selected source/request coordinate context, ordered combined relationship identities, complete authority snapshots, complete four-family relationship snapshots, participating hierarchy-boundary snapshots, exact semantic PartDocument snapshots, one proposal per free authority, solve state, and residual summary.

Block 27 authority/proposal and hierarchy-boundary freshness helpers are shared by geometric and motion application.

Motion additionally validates complete local/cross geometric/joint records, exact current motion-group participation, selected joint snapshot uniqueness, selected source coordinate equality, and requested-coordinate limit validity.

`AssemblyCrossHierarchyJointMotionResultApplier` validates everything first, then on one Project copy:

```text
apply authority direct-transform proposals
update selected Project-level joint authored coordinate
commit Project copy
```

It never writes a composed transform, `SubassemblyInstance::transform()`, occurrence-local pose override, or non-selected joint coordinate.

## Posed geometry and analysis

`AssemblyPosedLeafShapeBuilder` and `AssemblyStepExporter` consume the canonical flattened leaf boundary.

The posed pipeline recomputes each unique referenced part once per consumer, reuses caches across repeated occurrences, applies exact transform chains, and currently builds one derived OCCT compound for STEP export.

`AssemblyInterferenceAnalyzer`/clearance analysis evaluate deterministic unordered posed-leaf pairs. Analysis products remain derived.

## Persistence boundary

Current persistent assembly intent includes:

```text
Project/root/child document identity
part model intent
component placement/state/grounding
rigid subassembly occurrence placement/state
local geometric relationships
local Revolute joint/limit/coordinate records
Project-level cross-hierarchy geometric constraints
Project-level occurrence-qualified cross-hierarchy Revolute joints
```

Project JSON includes:

```text
cross_hierarchy_constraints[]
cross_hierarchy_joints[]
```

Derived and unpersisted products include hierarchy traversal, parent chains, flattened leaves, root-space targets, transform authorities, geometric/motion incidence and groups, holding drives, residuals/Jacobians, solve/motion states, freshness snapshots, proposals, diagnostics, shape caches, posed shapes, interference/clearance products, OCCT compounds, and future structured exchange identities/products.

## Next assembly block

Block 29 is component geometry instancing and structured STEP assembly products.

The block must freeze derived exchange identities for rooted assembly occurrences, local component occurrences inside exact containing occurrence paths, and reusable part product definitions keyed by `PartDocumentId`.

It must reuse `AssemblyLeafOccurrenceResolver` transform chains and one recompute/cache per unique referenced part, preserve repeated child/part occurrences as distinct assembly/component instances, define deterministic product/occurrence names/order, and emit structured STEP assembly/product relationships rather than only one anonymous flattened compound.

Occurrence-local child pose overrides, whole-subassembly solve variables, and swept-motion contact simulation remain deferred.

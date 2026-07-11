# Architecture Summary

This document summarizes implemented BLCAD architecture and the current direction. Feature-specific MVP documents remain canonical for exact contracts, formulas, failure policies, JSON spellings, and focused proofs.

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
Geometry and numeric execution
engineering analysis consumers
OCCT geometry kernel
file and exchange formats
```

A lower authority layer must not depend on a higher execution layer for persistent model identity. Save-format authority must not be a `blcad::geometry` query type.

## Part model

Implemented part architecture includes typed quantities/parameters, unit-aware part-local parameter expressions, datum planes and derived workplanes, construction geometry, line/arc/spline/circle/rectangle/composite/detected sketch profiles, projected/reference-driven sketch geometry, semantic generated face/edge/vertex references, recovery helpers, sketch constraints/dimensions/diagnostics/repair helpers, additive and subtractive extrude intent, `CircularHolePattern`, dependency/invalidation/recompute planning, optional OCCT recompute through `ShapeCache`, STEP export, and JSON model-intent serialization.

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

For identity-sensitive mutation and future cross-hierarchy solving, one component transform authority is conceptually:

```text
ComponentTransformAuthority =
  (assembly_document: DocumentId,
   local ComponentInstanceId)
```

This is currently a derived planning identity, not a separately persisted public record.

Repeated rooted occurrences of one child assembly may read the same child-document component transform authority.

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

A `SubassemblyInstance` currently has no grounding field and is not itself a six-variable rigid-body solve node.

Adding or loading a child occurrence never changes child component transforms.

## Hierarchy validity and traversal

Project structure validation rejects duplicate assembly document ids, invalid child references, root-as-child references, direct self-reference, indirect cycles, and invalid component/member relationships.

`AssemblyHierarchyTraversal` derives the rooted occurrence tree in deterministic root-first pre-order after complete project structure validation. Child occurrences are ordered lexicographically by local `SubassemblyInstanceId`.

One `AssemblyHierarchyOccurrenceDescriptor` carries derived context:

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

Hierarchy descriptors, paths, parent links, and transform chains are derived and unpersisted.

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

Hidden or suppressed subassembly occurrences remove their descendant subtree. Hidden or suppressed local components are also absent from flattened posed leaves.

Flattened leaves are derived and unpersisted.

## Local geometric relationship intent

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

`AssemblyConstraintGraph` derives deterministic local active relationship connectivity without persisting graph caches.

## Semantic target families

Implemented target strings include:

```text
feature.<feature-id>.top|bottom|right|left|front|back
feature.<feature-id>.axis
feature.<feature-id>.seat
```

`AssemblyConstraintTargetResolver` is the authority for mapping supported local semantic target strings to component-local geometry and direct component transform context.

Raw OCCT topology ids are never persistent assembly target identity.

## Local residual and numeric solve path

Canonical local residual semantics are reused for Mate, Distance, Concentric, Insert, and Angle.

Each free active local component in an ordinary local solve contributes six direct persisted-transform variables:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

The shared numeric engine implements scaled residual flattening, central finite differences, damped Gauss-Newton normal equations, partial-pivot Gaussian elimination, deterministic damping escalation, backtracking line search, explicit solve states, complete component snapshots, and transform proposals.

`AssemblyRigidBodySolver` supplies local geometric relationships. `AssemblyJointMotionSolver` supplies local geometric relationships plus transient Revolute drives. There is no separate motion optimizer.

Ordinary local solving removes suppressed components and local constraints touching them from the active numeric subgroup while retaining complete snapshots for stale-result validation.

Only explicit successful application changes persistent component solver placement or a selected authored joint coordinate.

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

The implemented geometric endpoint identity is:

```text
(occurrence_path,
 local ComponentInstanceId,
 semantic_reference)
```

The empty path addresses the root assembly occurrence. Non-empty paths are exact root-to-current `SubassemblyInstanceId` sequences.

`AssemblyHierarchyConstraintTargetResolver` resolves the exact rooted occurrence, delegates local semantic feature meaning to `AssemblyConstraintTargetResolver`, and evaluates the component plus parent transform chain into root-assembly space.

`AssemblyHierarchyConstraintEquationBuilder` reuses the canonical Mate, Distance, Angle, Concentric, and Insert residual descriptors.

The Geometry query layer is derived and unpersisted.

## Persistent cross-hierarchy relationship intent

Canonical documents:

- `docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`
- `docs/assembly-cross-hierarchy-constraint-json-mvp5.md`

`AssemblyHierarchyConstraintEndpoint` is Core-owned persistent endpoint identity:

```text
(occurrence_path,
 local ComponentInstanceId,
 semantic_reference)
```

`AssemblyHierarchyConstraint` stores project-level Mate, Concentric, Distance, Insert, or Angle intent with exact target A/B order and active/inactive state.

`Project` owns the cross-hierarchy constraint collection. Its ids are unique within that collection and are independent from local document-scoped constraint ids.

Project JSON now stores:

```text
cross_hierarchy_constraints[]
```

The field is additive. Files without it load with an empty collection.

## Cross-hierarchy structure validation

`Project::validate_assembly_structure()` validates ordinary member/component/local relationship/joint structure and the complete assembly hierarchy before cross-hierarchy endpoints.

Each endpoint path is then followed exactly from the explicit root through authored `SubassemblyInstanceId` values. The local `ComponentInstanceId` must exist in the assembly document reached by that exact path.

Validation is Core-only. It does not resolve semantic feature geometry or call OCCT.

Example semantic text can therefore roundtrip before a Geometry consumer interprets it:

```text
semantic.no_geometry.distance.a
```

A later target resolver may reject unsupported semantic geometry. The JSON/structure layer only protects persistent identity and project structure.

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

Two rooted occurrences may have different root-space geometry while mapping to one shared transform authority.

Future numeric variables, component snapshots, proposals, and direct transform application are therefore keyed by transform authority rather than blindly by occurrence path.

## Relationship-to-authority solve connectivity

The next cross-hierarchy solve partitioning layer is a derived bipartite relationship-to-transform-authority incidence model.

Relationship nodes:

```text
local relationship
  = (assembly_document, AssemblyConstraintId)

project-level cross-hierarchy relationship
  = project-level AssemblyConstraintId
```

Authority nodes:

```text
(assembly_document, ComponentInstanceId)
```

A local relationship appears once in its containing assembly document and is evaluated once in that local assembly space.

A project-level cross-hierarchy relationship preserves exact occurrence-qualified endpoints and is later evaluated in root-assembly space.

Each participating relationship is incident to every unique transform authority whose candidate direct component transform can affect its residual.

Solve groups are deterministic connected components of that active incidence graph. Pure-local groups remain ordinary local solver work. Cross-hierarchy solve groups contain at least one active project-level cross-hierarchy relationship.

Local relationship participation uses local state and local component suppression. Cross-hierarchy relationship participation additionally checks suppression along each exact endpoint occurrence path. Visibility remains a presentation/export property.

The complete sequence is canonical in `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

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

Cross-hierarchy relationship intent now roundtrips in project JSON through `cross_hierarchy_constraints[]`.

Derived and unpersisted data includes local graph connectivity, hierarchy traversal descriptors, parent transform chains, flattened leaves, target-resolution views, Geometry query target seeds, resolved semantic geometry, root-space target descriptors, residual descriptors, motion drive sets, relationship-to-authority incidence, cross-hierarchy solve groups, numeric residuals/Jacobians, solver state, solve/motion results, snapshots, proposals, diagnostics, `ShapeCache` values, posed leaf shapes, interference/clearance products, OCCT compounds, and STEP entity identity.

## Next assembly block

Block 25 is deterministic active relationship-to-`ComponentTransformAuthority` incidence and connected cross-hierarchy solve groups.

It must collect local constraints once per containing assembly document, retain exact occurrence-qualified cross-hierarchy endpoints, map every participating endpoint to its shared document/component transform authority, apply suppression without treating visibility as solve participation, and derive insertion-order-independent connected components.

It must not add semantic target geometry evaluation, numeric residual/Jacobian execution, solver iterations, snapshots, proposals, diagnostics, or result application.

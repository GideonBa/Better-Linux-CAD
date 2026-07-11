# Cross-Hierarchy Relationship Target and Residual Semantics MVP-5

Status: implemented the read-only geometric target-resolution and residual-semantics seed for endpoints in different rooted assembly occurrences.

This document is canonical for the derived Geometry layer. Persistent Core relationship intent is canonical in `docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`. The follow-up implementation sequence is canonical in `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

## Implemented read-only scope

```text
occurrence-qualified endpoint identity
  -> exact rooted AssemblyHierarchyOccurrenceDescriptor
  -> containing AssemblyDocument
  -> local ComponentInstanceId
  -> existing semantic feature reference
  -> existing local AssemblyConstraintTargetResolver
  -> exact [component, inner parent, ..., outer parent] transform chain
  -> root-assembly-space target geometry
  -> canonical Mate/Distance/Angle/Concentric/Insert residual
```

This Geometry layer is read-only. It creates no graph edge, numeric variable, solve result, or application path.

## Endpoint identity

The mathematical endpoint shape is:

```text
(occurrence_path,
 local ComponentInstanceId,
 semantic_reference)
```

The empty occurrence path addresses the explicit root assembly occurrence:

```text
([], component.root, feature.base_extrude.top)
```

A non-empty path is an exact root-to-current `SubassemblyInstanceId` sequence:

```text
([subassembly.outer, subassembly.inner],
 component.shaft,
 feature.bore.axis)
```

Path order is identity. Paths are not sets and are not normalized.

Block 23 moved persistence authority for this value contract into Core as:

```text
AssemblyHierarchyConstraintEndpoint
```

The existing Geometry-specific `AssemblyHierarchyConstraintTarget` remains a derived query seed used by the resolver implementation. It is not model or save-format authority.

`AssemblyHierarchyConstraintQuery::create` now accepts Core endpoints directly and also accepts one complete persistent `AssemblyHierarchyConstraint` record.

## Why occurrence path is required

A local `ComponentInstanceId` is scoped to one `AssemblyDocument` model definition. The same child document may occur multiple times in the rooted assembly tree.

Example:

```text
root
  subassembly.left  -> assembly.gearbox at +100 mm
  subassembly.right -> assembly.gearbox at -100 mm

assembly.gearbox
  component.shaft
```

These endpoints are geometrically distinct:

```text
([subassembly.left],  component.shaft, feature.bore.axis)
([subassembly.right], component.shaft, feature.bore.axis)
```

They reuse the same local component/feature definition but evaluate through different parent transform chains.

Therefore local `ComponentInstanceId` alone is insufficient geometric endpoint identity.

## Transform-authority boundary

Geometric endpoint identity is not persisted transform authority.

The current document-scoped flexible-child contract stores the shaft transform once in the child assembly document:

```text
ComponentTransformAuthority =
  (assembly.gearbox, component.shaft)
```

The architecture therefore distinguishes:

```text
geometric endpoint
  = (occurrence_path, ComponentInstanceId, semantic_reference)

geometric component occurrence
  = (occurrence_path, ComponentInstanceId)

persisted transform authority
  = (assembly_document: DocumentId, ComponentInstanceId)
```

The first two are occurrence-sensitive. The third is document/component-sensitive until occurrence-local internal pose overrides exist.

Future numeric variables, component snapshots, proposals, and direct transform application must use transform-authority identity rather than blindly using occurrence identity.

## Query contract

`AssemblyHierarchyConstraintQuery` stores:

```text
id
AssemblyConstraintType
target_a: derived Geometry query target
target_b: derived Geometry query target
optional distance
optional angle
```

Creation can start from:

```text
AssemblyHierarchyConstraintEndpoint
```

or:

```text
AssemblyHierarchyConstraint
```

The query reuses the established relationship value-family rules:

```text
Mate        -> no distance, no angle
Concentric  -> no distance, no angle
Insert      -> no distance, no angle
Distance    -> length distance required, no angle
Angle       -> degree angle required, no distance
```

The query itself is derived and unpersisted.

Active/inactive relationship participation is not a target-resolution concern. An inactive persistent record can still be converted into a pure mathematical query; the future incidence layer decides participation.

## Exact occurrence resolution

`AssemblyHierarchyConstraintTargetResolver` builds the deterministic rooted hierarchy:

```text
AssemblyHierarchyTraversal::build(project)
```

It then requires one exact `occurrence_path` match.

For the empty path, the match is the explicit root descriptor.

For a non-empty path, the matching descriptor identifies the exact rooted child occurrence and its containing `AssemblyDocument`.

Missing paths fail closed. The resolver does not guess by assembly document id or by the final path element alone.

## Local semantic target authority

After resolving the containing assembly document, the hierarchy resolver creates a temporary local target-resolution view with:

```text
selected AssemblyDocument as temporary root
project-owned PartDocument records
```

It converts the hierarchy endpoint to the existing local target shape:

```text
(component_instance, semantic_reference)
```

and delegates to `AssemblyConstraintTargetResolver`.

That existing resolver remains the only authority for implemented semantic target families:

```text
feature.<feature-id>.top|bottom|right|left|front|back
feature.<feature-id>.axis
feature.<feature-id>.seat
```

The hierarchy layer does not reparse feature history or infer alternative semantic geometry.

## Root-space transform evaluation

The local target resolver returns component-local geometry plus the direct authored component `RigidTransform`.

For an occurrence descriptor with parent chain:

```text
[T_inner_parent, ..., T_outer_parent]
```

the complete target chain is:

```text
[T_component, T_inner_parent, ..., T_outer_parent]
```

`AssemblyHierarchyTransformEvaluator` evaluates the chain exactly in inner-to-outer order.

For point `p`:

```text
T_outer(...T_inner(T_component(p)))
```

Vectors, normals, and axis directions rotate only at every level.

No composed matrix or recomputed Euler triple becomes persistent intent.

## Resolved target descriptors

The hierarchy resolver returns typed derived descriptors.

Planar targets preserve occurrence path, containing assembly document, local component, referenced part, source feature, semantic face/reference, and root-space planar geometry.

Axis targets additionally preserve source profile and semantic axis identity.

Insert targets preserve source profile, semantic axis, semantic seating plane, root-space axis, and root-space seating plane.

All resolved target descriptors are derived and unpersisted.

## Canonical residual semantics

`AssemblyHierarchyConstraintEquationBuilder` reuses the existing residual descriptor types and formulas.

### Mate

```text
normal_opposition    = nA + nB
signed_separation_mm = dot(oB - oA, nA)
```

### Distance

```text
normal_parallelism   = cross(nA, nB)
signed_separation_mm = dot(oB - oA, nA)
distance_residual_mm = signed_separation_mm - target_distance_mm
```

Target A owns signed direction.

### Angle

```text
normal_dot      = dot(nA, nB)
angle_alignment = normal_dot - cos(target_angle_deg)
```

The cosine seed remains direction-symmetric.

### Concentric

```text
direction_parallelism = cross(dA, dB)
axis_offset_mm         = cross(oB - oA, dA)
```

Equal and opposed axis directions are accepted.

### Insert

```text
direction_parallelism       = cross(dA, dB)
axis_offset_mm               = cross(oB - oA, dA)
signed_seating_separation_mm = dot(sB - sA, nA)
```

Target A owns signed seating direction.

Only endpoint identity and transform depth differ from local builders. The mathematics do not.

## Visibility and suppression boundary

The resolver answers:

```text
where is this exact semantic endpoint in root-assembly space?
```

Visibility and suppression do not alter that mathematical target-resolution answer.

Path-sensitive active/suppressed relationship participation belongs to Block 25. Target resolution must not silently become solver-participation policy.

## Persistence boundary after Block 23

Persistent Core model intent now includes:

```text
AssemblyHierarchyConstraintEndpoint
AssemblyHierarchyConstraint
Project::cross_hierarchy_constraints
```

The current project JSON format does not serialize those records yet.

Derived and unpersisted Geometry data includes:

```text
AssemblyHierarchyConstraintTarget
AssemblyHierarchyConstraintQuery
exact occurrence selection
local target-resolution Project view
root-space hierarchy target descriptors
hierarchy transform chains used for target evaluation
AssemblyHierarchyConstraintEquationDescriptor
cross-hierarchy residual descriptors
```

No resolved point, vector, normal, axis, seating plane, composed transform, or residual value becomes model intent.

## Failure policy

The read-only Geometry path fails closed on invalid project structure or cycles, missing occurrence paths, unresolved containing assemblies/components/parts, unsupported semantic target families, missing or wrong-type source features, invalid circular axis/seat producer geometry, workplane-resolution failures, target-family mismatches, and all existing local target-resolution failures.

The source `Project` is never mutated.

Core endpoint/relationship intent validation is documented separately in `docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`.

## Focused coverage

Geometry coverage:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy]"
```

The suite proves exact repeated-occurrence path identity, different root-space positions for repeated rigid child occurrences, root endpoint resolution, all five relationship families, Distance target-order signed semantics, different repeated-occurrence Concentric offsets, fail-closed target resolution, determinism, and source immutability.

Core persistent-intent coverage:

```bash
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-intent]"
```

## Next technical step

Block 24 from `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`:

```text
additive cross_hierarchy_constraints[] project JSON
  -> exact Core endpoint roundtrip
  -> target A/B order preservation
  -> absent-field backward compatibility
  -> fail-closed occurrence-path and reached-component structure validation
```

Do not add incidence graphs, numeric variables, solving, snapshots, diagnostics, or application in the same block.

# Cross-Hierarchy Relationship Target and Residual Semantics MVP-5

Status: implemented the read-only geometric identity, target-resolution, and residual-semantics seed for relationships whose endpoints may live in different rooted assembly occurrences.

Persistent Core intent and project JSON are now implemented separately in:

- `docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`
- `docs/assembly-cross-hierarchy-constraint-json-mvp5.md`

The follow-up solve-connectivity sequence is canonical in `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

## Implemented Geometry scope

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

The Geometry layer resolves and evaluates. It does not own persistent endpoint identity or project relationship storage.

## Geometric endpoint identity

The frozen identity shape is:

```text
(occurrence_path,
 local ComponentInstanceId,
 semantic_reference)
```

The persistent Core value type is:

```text
AssemblyHierarchyConstraintEndpoint
```

The Geometry query target is:

```text
AssemblyHierarchyConstraintTarget
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

Path order is identity.

## Why occurrence path is required

A local `ComponentInstanceId` is scoped to one `AssemblyDocument` model definition. The same child document may occur multiple times in the rooted tree.

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

They reuse one local component/feature definition but evaluate through different parent transform chains.

Local `ComponentInstanceId` alone is insufficient geometric endpoint identity.

## Transform-authority boundary

Geometric endpoint identity is not persisted transform authority.

The current document-scoped flexible-child contract stores the shaft transform once in the child assembly document:

```text
ComponentTransformAuthority =
  (assembly.gearbox, component.shaft)
```

The architecture therefore separates:

```text
geometric endpoint
  = (occurrence_path, ComponentInstanceId, semantic_reference)

geometric component occurrence
  = (occurrence_path, ComponentInstanceId)

persisted transform authority
  = (AssemblyDocumentId, ComponentInstanceId)
```

The first two are occurrence-sensitive. The third is document/component-sensitive until occurrence-local internal pose overrides exist.

Future numeric variables, snapshots, proposals, and direct transform application must not automatically become occurrence-qualified.

## Query contract

`AssemblyHierarchyConstraintQuery` stores:

```text
id
AssemblyConstraintType
target_a
target_b
optional distance
optional angle
```

It can be constructed from Core-owned endpoints or a complete persistent `AssemblyHierarchyConstraint` record.

The query reuses existing value-family rules:

```text
Mate        -> no distance, no angle
Concentric  -> no distance, no angle
Insert      -> no distance, no angle
Distance    -> length distance required, no angle
Angle       -> degree angle required, no distance
```

The query is derived. Persistent project-level intent lives in Core and now roundtrips through Project JSON.

## Exact occurrence resolution

`AssemblyHierarchyConstraintTargetResolver` builds the deterministic rooted hierarchy and requires one exact `occurrence_path` match.

For the empty path, the matching occurrence is the explicit root descriptor.

For a non-empty path, the matching descriptor identifies the exact rooted child occurrence and containing assembly document.

Missing paths fail closed. The resolver does not guess by assembly document id or by the path's final local occurrence id alone.

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

That existing resolver remains the only Geometry authority for implemented target families:

```text
feature.<feature-id>.top|bottom|right|left|front|back
feature.<feature-id>.axis
feature.<feature-id>.seat
```

The hierarchy layer does not reparse feature history or infer alternative semantic geometry.

## Root-space transform evaluation

The local target resolver returns component-local geometry plus the direct authored component transform.

For an occurrence descriptor with parent chain:

```text
[T_inner_parent, ..., T_outer_parent]
```

the complete hierarchy target chain is:

```text
[T_component, T_inner_parent, ..., T_outer_parent]
```

`AssemblyHierarchyTransformEvaluator` evaluates this exactly in inner-to-outer order.

For point `p`:

```text
T_outer(...T_inner(T_component(p)))
```

Vectors, normals, and axis directions rotate only at each level.

No composed matrix or recomputed Euler triple becomes persistent model intent.

## Resolved target descriptors

Planar target descriptors preserve occurrence path, containing assembly document, local component, referenced part, source feature, semantic face, semantic-reference text, and root-space plane.

Axis target descriptors preserve occurrence path, containing assembly document, local component, referenced part, source feature/profile, semantic axis, semantic-reference text, and root-space axis.

Insert target descriptors preserve occurrence path, containing assembly document, local component, referenced part, source feature/profile, semantic axis/seating-plane roles, semantic-reference text, root-space axis, and root-space seating plane.

All resolved descriptors are derived and unpersisted.

## Canonical residual semantics

`AssemblyHierarchyConstraintEquationBuilder` reuses existing residual descriptor types and formulas.

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
normal_dot       = dot(nA, nB)
angle_alignment  = normal_dot - cos(target_angle_deg)
```

The current cosine seed remains direction-symmetric.

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

Only endpoint identity and transform depth differ from the local builders. The mathematics do not.

## Visibility and suppression boundary

The resolver answers a mathematical geometry question:

```text
where is this exact semantic endpoint in root-assembly space?
```

Visibility and suppression do not alter target-resolution mathematics.

Path-sensitive active/suppressed relationship participation belongs to Block 25 graph semantics. Geometric target resolution must not silently become a solver-participation policy.

## Persistence boundary

Persistent Core model intent now includes:

```text
AssemblyHierarchyConstraintEndpoint
AssemblyHierarchyConstraint
Project::cross_hierarchy_constraints
```

Project JSON now serializes those records in:

```text
cross_hierarchy_constraints[]
```

Project loading validates exact endpoint path structure and reached local component identity, but does not resolve semantic feature geometry.

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

Core persistence and structural validation failure rules are documented in `docs/assembly-cross-hierarchy-constraint-json-mvp5.md`.

## Focused coverage

Geometry coverage:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy]"
```

The Geometry suite proves exact repeated-occurrence path identity, different root-space positions for repeated rigid child occurrences, root endpoint resolution, all five relationship families, Distance target-order signed semantics, different repeated-occurrence Concentric offsets, fail-closed target resolution, determinism, and source immutability.

Core persistent-intent coverage:

```bash
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-intent]"
```

Project JSON and structural coverage:

```bash
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-json]"
```

## Next technical step

Implement Block 25 from `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`:

```text
active local and project-level cross-hierarchy relationships
  -> relationship-to-ComponentTransformAuthority incidence
  -> deterministic connected cross-hierarchy solve groups
```

Block 25 must remain a derived Core graph/query layer. Do not add numeric residual/Jacobian execution, solver iterations, snapshots, proposals, diagnostics, or application.

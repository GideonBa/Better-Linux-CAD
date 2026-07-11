# Cross-Hierarchy Relationship Target and Residual Semantics MVP-5

Status: implemented the read-only geometric identity, target-resolution, and residual-semantics seed for relationships whose endpoints may live in different rooted assembly occurrences.

This document is canonical for the implemented read-only layer. The follow-up solver sequence is canonical in `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

## Implemented scope

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

The layer is read-only. It introduces no persistent project-level cross-hierarchy relationship, no graph edge, no numeric variable, no solve result, and no application path.

## Frozen geometric endpoint identity

`AssemblyHierarchyConstraintTarget` represents:

```text
(occurrence_path, local ComponentInstanceId, semantic_reference)
```

The empty occurrence path addresses the explicit root assembly occurrence:

```text
([], component.root, feature.base_extrude.top)
```

A non-empty path is the exact root-to-current `SubassemblyInstanceId` sequence produced by `AssemblyHierarchyTraversal`:

```text
([subassembly.outer, subassembly.inner],
 component.shaft,
 feature.bore.axis)
```

Path order is identity. Paths are not sets and are not normalized.

Every path element, component id, and semantic-reference string must be non-empty.

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

These endpoints are different geometric occurrences:

```text
([subassembly.left],  component.shaft, feature.bore.axis)
([subassembly.right], component.shaft, feature.bore.axis)
```

They reuse the same local component/feature definition but evaluate through different parent transform chains.

Therefore local `ComponentInstanceId` alone is insufficient geometric endpoint identity.

## Important identity boundary for future solving

Geometric endpoint identity must not be confused with persisted transform authority.

The current document-scoped flexible-subassembly contract stores the shaft transform once in the child assembly document. Therefore both occurrence endpoints above map to one persistent component record:

```text
ComponentTransformAuthority =
  (assembly.gearbox, component.shaft)
```

So the current architecture has three distinct identities:

```text
geometric endpoint
  = (occurrence_path, ComponentInstanceId, semantic_reference)

relationship node
  = (occurrence_path, ComponentInstanceId)

persisted transform authority
  = (AssemblyDocumentId, ComponentInstanceId)
```

The first two are occurrence-sensitive. The third is document/component-sensitive until occurrence-local internal pose overrides are implemented.

This distinction corrects the earlier handoff assumption that numeric-variable, snapshot, and proposal identities should automatically be occurrence-qualified. Doing that would create independent solver variables that the current persistent model cannot store independently.

The exact follow-up rules are documented in `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

## Query contract

`AssemblyHierarchyConstraintQuery` is a derived relationship query. It stores:

```text
id
AssemblyConstraintType
target_a: AssemblyHierarchyConstraintTarget
target_b: AssemblyHierarchyConstraintTarget
optional distance
optional angle
```

It reuses the existing local `AssemblyConstraint` value-family validation rules:

```text
Mate        -> no distance, no angle
Concentric  -> no distance, no angle
Insert      -> no distance, no angle
Distance    -> length distance required, no angle
Angle       -> degree angle required, no distance
```

The query is not inserted into `AssemblyDocument::constraints()` and is not serialized.

## Exact occurrence resolution

`AssemblyHierarchyConstraintTargetResolver` first builds the deterministic rooted hierarchy:

```text
AssemblyHierarchyTraversal::build(project)
```

It then requires one exact `occurrence_path` match.

For the empty path, the matching occurrence is the explicit root descriptor.

For a non-empty path, the matching descriptor identifies the exact rooted child occurrence and its containing `AssemblyDocument`.

Missing paths fail closed. The resolver does not guess by assembly document id or by the path's final `SubassemblyInstanceId` alone.

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

The hierarchy layer does not reparse feature history or infer alternative geometry semantics.

## Root-space transform evaluation

The local target resolver returns component-local geometry plus the component's direct authored `RigidTransform`.

For an occurrence descriptor with parent transform chain:

```text
[T_inner_parent, ..., T_outer_parent]
```

the hierarchy target chain is:

```text
[T_component, T_inner_parent, ..., T_outer_parent]
```

`AssemblyHierarchyTransformEvaluator` evaluates that chain exactly in inner-to-outer order.

For point `p`:

```text
T_outer(...T_inner(T_component(p)))
```

For vectors, normals, and axis directions, each level rotates only.

No chain is persisted as a composed matrix or converted back into Euler angles.

## Resolved target descriptors

### Planar target

`AssemblyHierarchyPlanarConstraintTargetDescriptor` preserves:

```text
occurrence_path
assembly_document
component_instance
referenced_part_document
source_feature
SemanticFace
semantic_reference
root-space AssemblySpacePlanarDescriptor
```

### Axis target

`AssemblyHierarchyAxisConstraintTargetDescriptor` preserves:

```text
occurrence_path
assembly_document
component_instance
referenced_part_document
source_feature
source_profile
SemanticAxis
semantic_reference
root-space AssemblySpaceAxisDescriptor
```

### Insert target

`AssemblyHierarchyInsertConstraintTargetDescriptor` preserves:

```text
occurrence_path
assembly_document
component_instance
referenced_part_document
source_feature
source_profile
SemanticAxis
SemanticSeatingPlane
semantic_reference
root-space axis
root-space seating plane
```

All descriptors are derived and unpersisted.

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

Target A owns the signed direction.

### Angle

```text
normal_dot      = dot(nA, nB)
angle_alignment = normal_dot - cos(target_angle_deg)
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

Target A owns the signed seating direction.

Only endpoint identity and transform depth differ from the local builders. The mathematics do not.

## Visibility and suppression boundary

The implemented resolver answers a mathematical geometry question:

```text
where is this exact semantic endpoint in root-assembly space?
```

Visibility and suppression do not alter that target-resolution answer.

Path-sensitive active/suppressed relationship participation belongs to the future relationship-graph block. This separation is intentional because geometric target resolution must not silently become a solver-participation policy.

## Read-only and persistence boundary

No JSON schema change is implemented by this block.

Persisted model intent remains unchanged:

```text
AssemblyDocument local constraints and joints
ComponentInstance placement/state
SubassemblyInstance placement/state
Project-owned assembly hierarchy
```

Derived and unpersisted:

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

The current Geometry-layer target/query types are not save-format authority. Before cross-hierarchy relationship persistence is introduced, the frozen endpoint value contract must be extracted or moved into the Core layer.

## Failure policy

The read-only path fails closed on:

- empty ids inside endpoint occurrence paths;
- empty component ids or semantic references;
- invalid Distance/Angle value-family combinations;
- invalid project structure or hierarchy cycles;
- occurrence paths absent from the rooted hierarchy;
- selected assembly documents that do not resolve;
- local component ids absent from the selected assembly document;
- referenced part documents that do not resolve;
- unsupported semantic target families;
- missing or wrong-type source features;
- invalid circular axis/seat producer geometry;
- local workplane resolution failures;
- target-family mismatches such as a planar face used for Concentric;
- any existing local semantic target-resolution failure.

The source `Project` is never mutated.

## Focused coverage

`tests/geometry/assembly_hierarchy_constraint_equation_builder_tests.cpp` proves:

- endpoint validation with the root empty-path convention;
- query value-family validation through the existing local constraint contract;
- exact repeated-occurrence path identity for one shared child document;
- different root-space positions for repeated rigid child occurrences;
- root endpoint resolution;
- cross-document Mate, Distance, Angle, Concentric, and Insert residual construction;
- Distance target-order signed semantics;
- reuse of all five existing residual descriptor families;
- different repeated-occurrence Concentric axis-offset residuals;
- missing-path, missing-component, and target-family mismatch rejection;
- deterministic repeated construction;
- source transforms and local constraint collections remaining unchanged.

Focused command:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy]"
```

## Explicitly deferred

The following work is deliberately split into blocks 23-27 of `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`:

- Core-owned persistent endpoint/constraint intent;
- project JSON and structure validation;
- occurrence relationship graph semantics;
- transform-authority mapping and solve connectivity;
- shared numeric residual/Jacobian integration;
- transform-authority snapshots and proposals;
- atomic result application;
- cross-hierarchy rank/remaining-DOF diagnostics.

Further deferred:

- cross-hierarchy joints and nested motion propagation;
- occurrence-local internal component pose overrides;
- whole-`SubassemblyInstance` rigid solve variables;
- component geometry instancing;
- contact response or rigid-body physics.

## Next technical step

Implement block 23 from `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md` only:

```text
Core-owned frozen endpoint value contract
  -> persistent Project-owned cross-hierarchy geometric constraint intent
```

Do not add JSON, graph connectivity, numeric variables, solving, snapshots, proposals, diagnostics, or application in the same block.

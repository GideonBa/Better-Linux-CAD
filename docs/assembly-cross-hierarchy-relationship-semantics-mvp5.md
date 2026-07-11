# Cross-Hierarchy Relationship Target and Residual Semantics MVP-5

Status: implemented as Block 22. Blocks 23-26 are implemented follow-ups; Block 27 is next.

This document is canonical for read-only occurrence-qualified target resolution and root-assembly-space Mate, Distance, Angle, Concentric, and Insert residual semantics.

## Scope

Implemented Geometry query types:

```text
AssemblyHierarchyConstraintTarget
AssemblyHierarchyConstraintQuery
AssemblyHierarchyConstraintTargetResolver
AssemblyHierarchyConstraintEquationBuilder
```

The query layer is derived and unpersisted.

Persistent endpoint and Project-level relationship intent are Core-owned by:

```text
AssemblyHierarchyConstraintEndpoint
AssemblyHierarchyConstraint
```

## Endpoint identity

Cross-hierarchy geometric endpoint identity is:

```text
(occurrence_path,
 local ComponentInstanceId,
 semantic_reference)
```

The empty occurrence path addresses the explicit root assembly occurrence.

Non-empty paths are exact root-to-current `SubassemblyInstanceId` sequences.

For repeated child occurrences:

```text
([subassembly.left],  component.child, feature.hole.axis)
([subassembly.right], component.child, feature.hole.axis)
```

the local component and semantic reference can be identical while the rooted endpoint identities remain different.

## Exact occurrence resolution

The hierarchy target resolver validates complete Project structure and resolves the exact rooted occurrence path.

An empty path selects the root assembly document.

A non-empty path selects exactly one rooted occurrence descriptor produced by `AssemblyHierarchyTraversal`.

The containing `AssemblyDocument` is the document reached by that exact path.

The resolver does not globally search for a component id or occurrence id.

## Local semantic target reuse

Once the containing assembly document is known, the hierarchy resolver creates a temporary local target-resolution Project view:

```text
selected containing AssemblyDocument -> temporary root assembly
Project-owned PartDocument records copied into the temporary Project
```

It then delegates semantic target meaning to the existing:

```text
AssemblyConstraintTargetResolver
```

Supported local semantic families remain:

```text
feature.<feature-id>.top|bottom|right|left|front|back
feature.<feature-id>.axis
feature.<feature-id>.seat
```

The hierarchy layer does not reparse feature history or infer a second semantic geometry contract.

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

`AssemblyHierarchyTransformEvaluator` evaluates the chain exactly in inner-to-outer order.

For point `p`:

```text
T_outer(...T_inner(T_component(p)))
```

Vectors, normals, and directions rotate only at each transform level.

No composed matrix or recomputed Euler triple becomes persistent model intent.

## Resolved target descriptors

Planar hierarchy targets preserve:

```text
occurrence path
containing assembly document
local component
referenced part document
source feature
semantic face
semantic reference text
root-space plane
```

Axis targets additionally preserve source profile and root-space axis.

Insert targets preserve axis and seating-plane semantic roles plus root-space axis and seating plane.

All target descriptors are derived and unpersisted.

## Canonical residual semantics

`AssemblyHierarchyConstraintEquationBuilder` reuses the established residual descriptor types and formulas.

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

The current cosine seed is direction-symmetric.

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

Only endpoint identity and transform depth differ from the local builders. The residual mathematics are shared.

## Visibility, suppression, and participation

Target resolution answers a mathematical geometry query:

```text
where is this exact semantic endpoint in root-assembly space?
```

Visibility and suppression do not alter this geometry query.

Block 25 separately implements solve participation:

- inactive local/cross relationships do not participate;
- local relationships touching a suppressed component do not participate;
- cross-hierarchy relationships using a suppressed path boundary or addressed component do not participate;
- visibility does not filter solve participation.

This keeps geometric resolution separate from solve-connectivity policy.

## Endpoint mappings and shared transform authority

Block 25 maps each participating hierarchy endpoint to:

```text
ComponentTransformAuthority =
  (reached assembly document,
   local ComponentInstanceId)
```

The complete endpoint identity remains in `AssemblyCrossHierarchyEndpointAuthorityMapping`.

Therefore:

```text
TargetA = ([left],  component.shaft, feature.left.axis)
TargetB = ([right], component.shaft, feature.right.axis)

both authority:
  (assembly.gearbox, component.shaft)
```

can have one shared transform authority while retaining two distinct parent transform chains.

## Numeric follow-up semantics

Block 26 is canonical in `docs/assembly-cross-hierarchy-numeric-solver-mvp5.md`.

During numeric evaluation, a candidate direct component transform is applied once to its `ComponentTransformAuthority` in a Project copy.

Both same-authority endpoints read that same candidate transform, but each endpoint is then evaluated through its own exact parent chain.

Thus:

```text
same candidate direct transform
+ different occurrence path
+ different parent chain
= potentially different root-space endpoint geometry
```

The existing hierarchy equation builder produces the residual descriptor, and the shared numeric system flattens it with the same scalar order and length scaling as local relationships.

Block 26 does not change the formulas documented here.

## Persistence boundary

Persistent Core model intent includes:

```text
AssemblyHierarchyConstraintEndpoint
AssemblyHierarchyConstraint
Project::cross_hierarchy_constraints
```

Project JSON serializes those records in:

```text
cross_hierarchy_constraints[]
```

Project loading validates exact endpoint path structure and reached local component identity, but does not resolve semantic feature geometry.

Derived and unpersisted data includes:

```text
AssemblyHierarchyConstraintTarget
AssemblyHierarchyConstraintQuery
exact occurrence selection
local target-resolution Project view
root-space hierarchy target descriptors
hierarchy transform chains used for target evaluation
AssemblyHierarchyConstraintEquationDescriptor
cross-hierarchy residual descriptors
ComponentTransformAuthority identities
relationship-to-authority incidence
endpoint-to-authority mappings
cross-hierarchy solve groups
mixed scaled residual vectors
finite-difference Jacobians
cross-hierarchy solve state
authority snapshots
authority transform proposals
```

No resolved geometry, composed transform, residual value, numeric state, or solve result becomes model intent.

## Failure policy

The read-only Geometry path fails closed on invalid Project structure or cycles, missing occurrence paths, unresolved containing assemblies/components/parts, unsupported semantic target families, missing or wrong-type source features, invalid circular axis/seat producer geometry, workplane-resolution failures, target-family mismatches, and all existing local target-resolution failures.

The source Project is never mutated by target/equation resolution.

Persistence and structural validation failure rules are canonical in `docs/assembly-cross-hierarchy-constraint-json-mvp5.md`.

## Focused coverage

Geometry target/equation coverage:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy]"
```

The suite proves exact repeated-occurrence path identity, different root-space positions for repeated child occurrences, root endpoint resolution, all five relationship families, Distance target-order signed semantics, different repeated-occurrence Concentric offsets, fail-closed target resolution, determinism, and source immutability.

Numeric follow-up coverage:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-solver]"
```

Core intent/JSON/connectivity coverage:

```bash
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-intent]"
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-json]"
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-graph]"
```

## Current handoff

The current sequence source of truth is `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

Block 26 now implements authority-scoped mixed local/root-space numeric solving and unapplied proposals.

Next is Block 27 only: complete solve-result freshness validation, an explicit semantic target-producing geometry freshness contract, atomic authority-qualified direct-transform application, and cross-hierarchy rank/remaining-DOF diagnostics over the exact Block-26 free-authority variable order.

# Cross-Hierarchy Relationship Target and Residual Semantics MVP-5

Status: implemented as the read-only geometric semantics seed before persistent cross-hierarchy solver integration.

This document is canonical for occurrence-qualified target resolution and read-only root-space Mate, Distance, Angle, Concentric, and Insert residual construction across assembly-document boundaries.

Persistent endpoint/relationship intent is canonical in `docs/assembly-cross-hierarchy-constraint-intent-mvp5.md` and `docs/assembly-cross-hierarchy-constraint-json-mvp5.md`.

Active relationship connectivity is canonical in `docs/assembly-cross-hierarchy-incidence-groups-mvp5.md`.

## Scope

The read-only Geometry layer answers:

```text
where is this exact semantic endpoint in root-assembly space?

and

what canonical relationship residual descriptor follows from two such endpoints?
```

It does not persist a target, decide solve participation, create numeric variables, or apply transforms.

## Persistent endpoint authority versus Geometry query target

Core-owned persistent identity is:

```text
AssemblyHierarchyConstraintEndpoint =
  (occurrence_path,
   local ComponentInstanceId,
   semantic_reference)
```

The Geometry layer retains:

```text
AssemblyHierarchyConstraintTarget
```

as a derived query seed.

`AssemblyHierarchyConstraintQuery::create` accepts Core endpoint values and complete `AssemblyHierarchyConstraint` records.

Save-format authority remains in Core.

## Exact rooted occurrence selection

The empty path addresses the explicit root assembly occurrence:

```text
[]
```

A non-empty path is followed as an exact root-to-current `SubassemblyInstanceId` sequence.

Example:

```text
[subassembly.outer, subassembly.inner]
```

means:

```text
root
  -> subassembly.outer
  -> reached child assembly
  -> subassembly.inner
  -> reached nested child assembly
```

Repeated occurrences of one child document remain distinct because the paths differ.

## Local semantic target authority reuse

After selecting the exact containing assembly occurrence, the hierarchy resolver creates the appropriate local target-resolution view and delegates semantic feature meaning to:

```text
AssemblyConstraintTargetResolver
```

Supported target families remain:

```text
feature.<feature-id>.top|bottom|right|left|front|back
feature.<feature-id>.axis
feature.<feature-id>.seat
```

The cross-hierarchy layer does not implement a second parser for feature target strings.

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

Vectors, normals, and axis directions rotate only at every level.

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

Target A owns signed seating direction.

Only endpoint identity and transform depth differ from the local builders. The mathematics do not.

## Visibility, suppression, and graph participation

Target resolution answers a mathematical geometry query and does not itself filter by visibility or suppression.

Block 25 now implements participation policy in `AssemblyCrossHierarchyConstraintGraph`:

- inactive local/cross relationships do not participate;
- local relationships touching a suppressed component do not participate;
- cross-hierarchy relationships using a suppressed path boundary or addressed component do not participate;
- visibility does not filter solve participation.

This keeps geometric resolution separate from solve-connectivity policy.

## Endpoint mappings and shared transform authority

Block 25 maps every participating cross-hierarchy endpoint to:

```text
ComponentTransformAuthority =
  (reached assembly document,
   local ComponentInstanceId)
```

The complete endpoint identity remains in `AssemblyCrossHierarchyEndpointAuthorityMapping`.

Therefore two endpoints may read one direct component-transform authority while retaining different parent transform chains:

```text
TargetA = ([left],  component.shaft, feature.left.axis)
TargetB = ([right], component.shaft, feature.right.axis)

both authority:
  (assembly.gearbox, component.shaft)
```

In Block 26 both endpoints must observe the same candidate direct shaft transform but each must still be evaluated through its own path-specific parent chain.

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
```

No resolved point, vector, normal, axis, seating plane, composed transform, residual value, or connectivity cache becomes model intent.

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

Core intent/JSON/connectivity coverage:

```bash
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-intent]"
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-json]"
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-graph]"
```

## Next technical step

Implement Block 26 from `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`:

```text
AssemblyCrossHierarchySolveGroup
  -> authority-scoped candidate transforms and free variables
  -> local relationship residuals in containing-document local space
  -> cross-hierarchy endpoint residuals in root space
  -> deterministic mixed residual vector
  -> central finite-difference Jacobian by authority variable
  -> existing numeric solve engine
```

Block 26 must return unapplied authority-scoped proposals and must not add result application or cross-hierarchy diagnostics.

# Cross-Hierarchy Relationship Target and Residual Semantics MVP-5

Status: implemented as Block 22. Blocks 23-27 are implemented follow-ups; Block 28 is next.

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

Repeated child occurrences may share local component and semantic reference while retaining distinct rooted endpoint identity:

```text
([subassembly.left],  component.child, feature.hole.axis)
([subassembly.right], component.child, feature.hole.axis)
```

## Exact occurrence resolution

The hierarchy target resolver validates complete Project structure and resolves the exact rooted occurrence path.

An empty path selects the root assembly document.

A non-empty path selects exactly one rooted occurrence descriptor produced by `AssemblyHierarchyTraversal`.

The containing `AssemblyDocument` is the document reached by that path.

The resolver does not globally search component or occurrence ids.

## Local semantic target reuse

Once the containing assembly document is known, the hierarchy resolver creates a temporary local target-resolution Project view:

```text
selected containing AssemblyDocument -> temporary root assembly
Project-owned PartDocument records -> copied into temporary Project
```

It delegates semantic target meaning to:

```text
AssemblyConstraintTargetResolver
```

Supported local families remain:

```text
feature.<feature-id>.top|bottom|right|left|front|back
feature.<feature-id>.axis
feature.<feature-id>.seat
```

The hierarchy layer does not implement a second feature-history parser or semantic geometry contract.

## Root-space transform evaluation

The local resolver returns component-local geometry plus the direct component transform.

For occurrence parent chain:

```text
[T_inner_parent, ..., T_outer_parent]
```

the complete target chain is:

```text
[T_component, T_inner_parent, ..., T_outer_parent]
```

`AssemblyHierarchyTransformEvaluator` evaluates the chain exactly inner-to-outer.

For point `p`:

```text
T_outer(...T_inner(T_component(p)))
```

Vectors, normals, and directions rotate only.

No composed matrix or recomputed Euler triple becomes persistent model intent.

## Resolved target descriptors

Planar hierarchy targets preserve occurrence path, containing assembly document, local component, referenced part, source feature, semantic face/reference, and root-space plane.

Axis targets additionally preserve source profile and root-space axis.

Insert targets preserve axis and seating-plane semantic roles plus root-space axis and seating plane.

All target descriptors are derived and unpersisted.

## Canonical residual semantics

`AssemblyHierarchyConstraintEquationBuilder` reuses established residual descriptor types and formulas.

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

Only endpoint identity and transform depth differ from local builders. The residual mathematics are shared.

## Visibility, suppression, and participation

Target resolution answers a mathematical geometry query and does not filter by visibility or suppression.

Block 25 separately implements solve participation:

- inactive local/cross relationships do not participate;
- local relationships touching suppressed components do not participate;
- cross relationships using a suppressed path boundary or addressed component do not participate;
- visibility does not filter solve participation.

This separation is retained by Block 27 freshness: visibility changes do not stale a result, while suppression/path participation changes do.

## Endpoint mappings and shared transform authority

Block 25 maps participating hierarchy endpoints to:

```text
ComponentTransformAuthority =
  (reached assembly document,
   local ComponentInstanceId)
```

The complete endpoint remains in `AssemblyCrossHierarchyEndpointAuthorityMapping`.

Thus:

```text
TargetA = ([left],  component.shaft, feature.left.axis)
TargetB = ([right], component.shaft, feature.right.axis)

both authority:
  (assembly.gearbox, component.shaft)
```

can share one transform authority while retaining two parent chains.

## Numeric follow-up semantics

Block 26 applies one candidate direct component transform once to its authority in a Project copy.

Same-authority endpoints read that candidate transform but each evaluates through its own exact path-specific parent chain:

```text
same candidate direct transform
+ different occurrence path
+ different parent chain
= potentially different root-space geometry
```

The hierarchy equation builder produces the residual descriptor and the shared numeric system flattens it with the same scalar order/length scaling as local relationships.

## Freshness and application follow-up semantics

Block 27 is canonical in `docs/assembly-cross-hierarchy-application-diagnostics-mvp5.md`.

A solve result now protects:

```text
complete transform authority inputs
complete local/cross relationship records
exact current active solve-group participation
every persistent hierarchy boundary on participating cross endpoint paths
exact canonical model intent of participating referenced PartDocuments
```

Hierarchy boundary snapshots protect containing assembly, local `SubassemblyInstanceId`, referenced child assembly, suppression, and direct boundary transform.

Visibility remains outside solve freshness.

Semantic target-producing PartDocument freshness is exact and conservative: the solve result stores the byte-for-byte canonical `serialize_part_document_to_json(part)` payload for participating referenced parts and requires equality at application.

Atomic application writes only direct component transforms addressed by `ComponentTransformAuthority` values. It never writes root-space target placements or `SubassemblyInstance::transform()` values.

## Diagnostics follow-up semantics

Cross-hierarchy diagnostics use the exact Block-26 free-authority order and the same authority candidate application, hierarchy target/equation evaluation, residual flattening, length scaling, and central finite differences as solving.

```text
variable_count = 6 * unique_free_active_transform_authority_count
constrained_dof = rank(J)
remaining_dof = variable_count - rank(J)
```

The count is authority-based, not occurrence-based.

## Persistence boundary

Persistent Core model intent includes:

```text
AssemblyHierarchyConstraintEndpoint
AssemblyHierarchyConstraint
Project::cross_hierarchy_constraints
```

Project JSON serializes these records through:

```text
cross_hierarchy_constraints[]
```

Derived and unpersisted products include hierarchy target/query descriptors, exact occurrence resolution, root-space target geometry, residual descriptors, transform authorities, incidence/mappings, solve groups, numeric residuals/Jacobians, solve state, authority/relationship/boundary/semantic-PartDocument snapshots, proposals, freshness products, and diagnostics.

No resolved geometry, composed transform, residual, snapshot, or solver product becomes model intent.

## Failure policy

The read-only Geometry path fails closed on invalid Project structure/cycles, missing occurrence paths, unresolved assemblies/components/parts, unsupported semantic target families, missing or wrong-type source features, invalid circular axis/seat producer geometry, workplane resolution failures, target-family mismatch, and existing local target-resolution failures.

The source Project is never mutated by target/equation resolution.

## Focused coverage

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-application]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-diagnostics]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-semantic-freshness]"
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-intent]"
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-json]"
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-graph]"
```

## Current handoff

Blocks 22-27 complete the cross-hierarchy geometric intent/structure/connectivity/numeric/fresh-application/diagnostics chain.

Next is Block 28 only: occurrence-qualified Project-level cross-hierarchy Revolute joint intent, joint-to-transform-authority incidence, combined geometric/joint motion connectivity, root-space nested Revolute drive evaluation through exact parent chains, shared authority-scoped numeric solving, and Block-27-style fresh atomic transform-plus-coordinate application.

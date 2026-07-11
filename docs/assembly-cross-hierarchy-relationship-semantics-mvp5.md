# Cross-Hierarchy Relationship Target and Residual Semantics MVP-5

Status: implemented the first read-only cross-hierarchy geometric relationship semantics. Stable occurrence-qualified endpoint identity resolves existing component-local semantic feature targets through exact hierarchy transform chains into root-assembly space, then reuses the canonical Mate, Distance, Angle, Concentric, and Insert residual definitions.

## Scope

This block defines identity, target resolution, and residual semantics only.

```text
occurrence-qualified endpoint identity
  -> exact rooted AssemblyHierarchyOccurrenceDescriptor
  -> containing AssemblyDocument
  -> local ComponentInstanceId
  -> existing semantic feature reference
  -> existing local AssemblyConstraintTargetResolver
  -> exact [component, inner parent, ..., outer parent] transform chain
  -> root-assembly-space target geometry
  -> canonical relationship residual
```

No persistent cross-hierarchy constraint record, graph edge, solve variable, Jacobian row, solve result, or application path is introduced.

The purpose of this block is to make the identity and geometry contract stable before cross-document relationships are allowed into the numeric solver.

## Stable endpoint identity

`AssemblyHierarchyConstraintTarget` stores:

```text
occurrence_path: [SubassemblyInstanceId, ...]
component_instance: ComponentInstanceId
semantic_reference: string
```

The occurrence path uses the existing `AssemblyHierarchyTraversal` identity contract.

An empty path addresses the explicit root assembly occurrence:

```text
[]
```

A direct child occurrence is addressed by:

```text
[subassembly.gearbox.left]
```

A nested occurrence is addressed by the complete root-to-current sequence:

```text
[subassembly.machine, subassembly.gearbox, subassembly.shaft_group]
```

The endpoint then identifies one component local to the selected assembly occurrence and one existing semantic feature target, for example:

```text
occurrence_path = [subassembly.machine, subassembly.gearbox]
component_instance = component.shaft
semantic_reference = feature.bore.axis
```

The tuple is the stable derived target identity:

```text
(occurrence_path, local ComponentInstanceId, semantic_reference)
```

This is necessary because `ComponentInstanceId` is unique only inside one `AssemblyDocument`. Repeated occurrences of one child assembly document may expose the same local component id at different rooted paths.

## Validation

`AssemblyHierarchyConstraintTarget::create` rejects:

- an empty `SubassemblyInstanceId` inside the occurrence path;
- an empty local component id;
- an empty semantic reference.

The empty occurrence path itself is valid and means the root assembly occurrence.

Path existence is resolved later against a concrete `Project` because endpoint creation is model-independent identity construction.

## Read-only relationship query

`AssemblyHierarchyConstraintQuery` stores:

```text
id: AssemblyConstraintId
type: AssemblyConstraintType
target_a: AssemblyHierarchyConstraintTarget
target_b: AssemblyHierarchyConstraintTarget
distance: optional Quantity
angle: optional Quantity
```

The query deliberately reuses the existing relationship family enumeration:

```text
Mate
Concentric
Distance
Insert
Angle
```

It also reuses the existing `AssemblyConstraint::create` value-family validation contract through a temporary validation record:

- Distance requires a length quantity and is the only family allowed to carry `distance`;
- Angle requires a degree quantity and is the only family allowed to carry `angle`;
- Mate, Concentric, and Insert carry neither value.

The temporary validation record is not inserted into any `AssemblyDocument` and is not returned as model intent.

`AssemblyHierarchyConstraintQuery` is a derived read-only query object.

## Exact occurrence resolution

`AssemblyHierarchyConstraintTargetResolver` first calls:

```text
AssemblyHierarchyTraversal::build(project)
```

This preserves the complete existing project-structure and cycle-validation boundary.

The target occurrence is selected by exact path equality against one traversal descriptor.

Examples:

```text
[]                         -> root occurrence
[subassembly.left]         -> exact left child occurrence
[subassembly.right]        -> exact right child occurrence
[subassembly.outer, inner] -> exact nested occurrence
```

A missing path fails closed.

The selected descriptor supplies:

```text
assembly_document
parent_transforms_inner_to_outer
```

The selected assembly document must still resolve in the project.

## Reusing local semantic target authority

The cross-hierarchy resolver does not parse `.top`, `.axis`, or `.seat` references itself.

Instead, the selected `AssemblyDocument` is copied into a temporary local target-resolution view as the view's root assembly. Project-owned part documents are copied into the same view.

The endpoint is converted to the existing local target shape:

```text
AssemblyConstraintTarget(
  local ComponentInstanceId,
  semantic_reference
)
```

Then the existing `AssemblyConstraintTargetResolver` remains the single authority for:

```text
feature.<feature-id>.top|bottom|right|left|front|back
feature.<feature-id>.axis
feature.<feature-id>.seat
```

Therefore the cross-hierarchy layer inherits the existing rules for:

- source-feature existence and type;
- source-sketch existence;
- CircleProfile producer restrictions;
- source-profile identity;
- workplane resolution;
- extrude direction;
- semantic face, axis, and seating-plane identity.

No second semantic-reference parser or feature-geometry inference path is introduced.

## Root-assembly-space transform semantics

The local resolver returns component-local geometry plus the selected component's authored `RigidTransform`.

For a local target inside one nested assembly occurrence, the cross-hierarchy resolver constructs:

```text
[component transform, inner parent transform, ..., outer parent transform]
```

This is the exact transform-chain contract already used by `AssemblyLeafOccurrenceResolver` and posed geometry.

Example:

```text
Tc = component transform
Ti = inner SubassemblyInstance transform
To = outer SubassemblyInstance transform

chain = [Tc, Ti, To]
point = To(Ti(Tc(local_point)))
```

`AssemblyHierarchyTransformEvaluator` applies every authored level in inner-to-outer order.

Points are rotated and translated at each level. Vectors are rotated only.

The chain is never collapsed to a persisted composed transform and is never converted back to X/Y/Z Euler angles.

## Resolved planar target

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

The root-space planar descriptor contains:

```text
origin
x_axis
y_axis
normal
```

## Resolved axis target

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

The root-space axis descriptor contains:

```text
origin
direction
```

## Resolved Insert target

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

Axis and seating-plane geometry are derived from the same existing `.seat` producer contract.

## Repeated child assembly occurrences

Repeated occurrences of one child assembly document are distinct targets because the occurrence path is part of endpoint identity.

Example:

```text
root
  subassembly.left  -> assembly.gearbox, boundary x = +100 mm
  subassembly.right -> assembly.gearbox, boundary x = -100 mm

assembly.gearbox
  component.shaft
```

These endpoints are distinct:

```text
([subassembly.left],  component.shaft, feature.bore.axis)
([subassembly.right], component.shaft, feature.bore.axis)
```

They resolve through the same child `AssemblyDocument` and the same local component/feature identity, but their root-space geometry differs because the parent transform chain differs.

This contract is required before any cross-hierarchy graph can use occurrence-qualified component variables.

## Canonical residual semantics

`AssemblyHierarchyConstraintEquationBuilder` evaluates target A first, then target B, and preserves that order in every signed or directed residual.

The result is `AssemblyHierarchyConstraintEquationDescriptor`:

```text
relationship: AssemblyConstraintId
type: AssemblyConstraintType
target_a: typed hierarchy target descriptor
target_b: typed hierarchy target descriptor
residual: one canonical residual descriptor
```

The target descriptor variant is one of:

```text
AssemblyHierarchyPlanarConstraintTargetDescriptor
AssemblyHierarchyAxisConstraintTargetDescriptor
AssemblyHierarchyInsertConstraintTargetDescriptor
```

The residual variant reuses the existing canonical descriptor types.

### Mate

For root-space planes A and B:

```text
normal_opposition    = nA + nB
signed_separation_mm = dot(oB - oA, nA)
```

A satisfied Mate has both residuals equal to zero.

### Distance

```text
normal_parallelism   = cross(nA, nB)
signed_separation_mm = dot(oB - oA, nA)
distance_residual_mm = signed_separation_mm - target_distance_mm
```

Target A owns the signed direction exactly as in the local builder.

Reversing A and B may therefore change both signed separation and distance residual.

### Angle

```text
normal_dot      = dot(nA, nB)
angle_alignment = normal_dot - cos(target_angle_deg)
```

The existing cosine semantics remain direction-symmetric and keep the same documented `0°`/`180°` local-rank degeneracies.

### Concentric

For root-space axes A and B:

```text
direction_parallelism = cross(dA, dB)
axis_offset_mm         = cross(oB - oA, dA)
```

Equal and opposed directions are accepted. Axial separation and rotation around the common axis remain free.

### Insert

For root-space axis/seat pairs A and B:

```text
direction_parallelism       = cross(dA, dB)
axis_offset_mm               = cross(oB - oA, dA)
signed_seating_separation_mm = dot(sB - sA, nA)
```

Target A defines signed seating direction.

The residual equations are intentionally identical to the local builders. Only target identity and transform depth differ.

## Visibility and suppression boundary

Target resolution is geometric and read-only.

Visibility and suppression do not change endpoint identity or the mathematical ability to resolve existing authored geometry. This matches the separation already present in local target resolution.

Future cross-hierarchy graph and numeric-solver integration must own participation filtering:

```text
hidden -> still targetable model geometry
suppressed endpoint/path -> relationship excluded from active numeric subgroup
```

This block does not invent graph or solver suppression behavior before an occurrence-qualified graph exists.

## Persistence boundary

No JSON schema change is introduced.

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

## Failure policy

The read-only target/residual path fails closed on:

- empty occurrence ids inside endpoint paths;
- empty component ids or semantic references;
- invalid Distance/Angle value-family combinations;
- invalid project structure or hierarchy cycles;
- occurrence paths absent from the rooted hierarchy;
- selected assembly documents that no longer resolve;
- local component ids absent from the selected assembly document;
- referenced part documents that do not resolve;
- unsupported semantic target families;
- missing or wrong-type source features;
- invalid circular axis/seat producer geometry;
- local workplane resolution failures;
- target family mismatches such as using a planar face for Concentric;
- any existing local semantic target-resolution failure.

The source `Project` is never mutated.

## Focused coverage

`tests/geometry/assembly_hierarchy_constraint_equation_builder_tests.inc`, compiled through the existing geometry test target, proves:

- stable endpoint validation with the root empty-path convention;
- query value-family validation through the existing `AssemblyConstraint` contract;
- exact repeated-occurrence path identity for one shared child assembly document;
- root-space target positions differing across repeated rigid child occurrences;
- root endpoint resolution through an empty occurrence path;
- cross-document Mate residual construction;
- Distance target-order signed semantics;
- cross-document Angle residual construction;
- cross-document Concentric residual construction;
- cross-document Insert residual construction;
- all five families reusing their existing residual descriptor types;
- repeated occurrences of one child document producing different Concentric axis-offset residuals;
- missing occurrence-path rejection;
- missing local component rejection;
- target-family mismatch rejection;
- deterministic repeated construction;
- source component transforms and local constraint collections remaining unchanged.

Focused command:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy]"
```

## Explicitly deferred

- persistent cross-hierarchy geometric constraint records;
- a project-level cross-hierarchy constraint collection and JSON form;
- occurrence-qualified active-constraint graph nodes and edges;
- occurrence-qualified numeric variable identities;
- cross-hierarchy residual flattening inside the shared numeric system;
- Jacobian construction over components reached through different assembly occurrences;
- cross-hierarchy rigid-body solve/application and stale snapshots;
- cross-hierarchy remaining-DOF diagnostics;
- cross-hierarchy joints and nested motion propagation;
- whole-SubassemblyInstance rigid solve variables;
- occurrence-local internal component pose overrides;
- collision/contact response or physics.

## Next technical step

Add persistent project-level cross-hierarchy geometric constraint intent and route it through an occurrence-qualified constraint graph plus the shared numeric solve/application path.

The next block must preserve the endpoint identity defined here exactly:

```text
(occurrence_path, local ComponentInstanceId, semantic_reference)
```

Before solving, numeric variable and stale-snapshot identity must also become occurrence-qualified so repeated occurrences of one child document cannot alias merely because they share the same local `ComponentInstanceId`.

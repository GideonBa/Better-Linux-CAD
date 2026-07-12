# Cross-Hierarchy Revolute Joint and Nested Motion MVP-5

Status: implemented as Block 28 of `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

This block extends the frozen occurrence-qualified endpoint and `ComponentTransformAuthority` model from cross-hierarchy geometric constraints to persistent Project-level Revolute motion intent.

## Scope

Implemented:

- persistent Core `AssemblyHierarchyJoint` intent;
- Project-owned cross-hierarchy joint collection and Project-scoped joint ids;
- additive `cross_hierarchy_joints[]` Project JSON;
- exact occurrence-qualified target A/B identity and path-order preservation;
- Revolute state, principal limits, and authored coordinate semantics shared with local joints;
- Core-only exact endpoint path/reached-component structure validation;
- combined local/cross geometric/joint relationship-to-authority incidence;
- deterministic cross-hierarchy motion groups;
- exact root-space `.seat` target resolution through hierarchy parent chains;
- shared local/cross Revolute residual mathematics;
- deterministic authored-coordinate holding drives for every non-selected active Revolute joint in one motion group;
- one six-variable block per unique free active `ComponentTransformAuthority`;
- the existing shared central finite-difference and damped Gauss-Newton engine;
- complete authority, relationship, hierarchy-boundary, and semantic PartDocument snapshots;
- at most one direct-transform proposal per free authority;
- atomic authority-transform plus selected Project-level joint-coordinate application.

Not implemented:

- occurrence-local internal child pose overrides;
- `SubassemblyInstance` grounding or solve variables;
- whole-subassembly transform proposals;
- Prismatic/Cylindrical/Planar/Ball joint families;
- multi-turn Revolute coordinates outside the principal seed range;
- trajectory/time integration;
- swept-motion collision/contact response;
- structured STEP assembly products or component geometry instancing.

## Persistent Project-level joint intent

`AssemblyHierarchyJoint` stores:

```text
AssemblyJointId
name
AssemblyJointType::Revolute
target A: AssemblyHierarchyConstraintEndpoint
target B: AssemblyHierarchyConstraintEndpoint
AssemblyJointState
lower limit in degrees
upper limit in degrees
authored current coordinate in degrees
```

Endpoint identity remains:

```text
(occurrence_path,
 local ComponentInstanceId,
 semantic_reference)
```

The empty path addresses the explicit root occurrence.

A non-empty path is the exact ordered root-to-current sequence of local `SubassemblyInstanceId` values.

Target A/B order is persistent and directed motion identity.

## Same-authority endpoint rule

Local `AssemblyJoint` requires distinct local component ids.

A Project-level `AssemblyHierarchyJoint` instead rejects only exactly equal endpoint identities.

Therefore this is invalid:

```text
([left], component.shaft, feature.hole.seat)
([left], component.shaft, feature.hole.seat)
```

but this is valid:

```text
([left],  component.shaft, feature.hole.seat)
([right], component.shaft, feature.hole.seat)
```

Both rooted endpoints may map to:

```text
(assembly.gearbox, component.shaft)
```

while retaining different parent transform chains.

The motion graph then stores:

```text
2 endpoint mappings
1 unique joint -> authority incidence
```

No second persistent direct transform or synthetic coupling residual is introduced.

## Revolute limits and coordinate

The local Revolute validation contract is reused:

```text
-180 deg <= lower < upper <= 180 deg
lower <= authored coordinate <= upper
```

Limit and coordinate quantities must use `AngleDeg`.

A requested motion coordinate must also use degrees and lie inside the selected joint's persistent limits. Requests are rejected rather than clamped.

The Project-level id scope is independent from local document-scoped `AssemblyJointId` values.

## Project ownership and structure validation

`Project` owns:

```text
std::vector<AssemblyHierarchyJoint> cross_hierarchy_joints
```

Public model APIs include:

```text
add_cross_hierarchy_joint
cross_hierarchy_joints
cross_hierarchy_joint_count
find_cross_hierarchy_joint
set_cross_hierarchy_joint_coordinate
validate_cross_hierarchy_joints
```

Complete Project structure validation order is now:

```text
member parts
  -> component instances
  -> local geometric constraints
  -> local joints
  -> complete cycle-free assembly hierarchy
  -> Project-level cross-hierarchy geometric endpoints
  -> Project-level cross-hierarchy joint endpoints
```

Each joint endpoint path is followed exactly from the root. The addressed local component must exist in the reached assembly document.

Core validation does not parse `.seat` geometry or call OCCT.

Adding/loading/updating authored coordinate intent does not move component or subassembly transforms.

## Project JSON

Additive field:

```text
cross_hierarchy_joints[]
```

Representative record:

```json
{
  "id": "joint.cross.main",
  "name": "Root to nested shaft",
  "type": "revolute",
  "target_a": {
    "occurrence_path": [],
    "component_instance": "component.root",
    "semantic_reference": "feature.hole.seat"
  },
  "target_b": {
    "occurrence_path": ["subassembly.outer", "subassembly.inner"],
    "component_instance": "component.shaft",
    "semantic_reference": "feature.hole.seat"
  },
  "state": "active",
  "limits": {
    "lower": {"unit": "deg", "value": -90.0},
    "upper": {"unit": "deg", "value": 90.0}
  },
  "coordinate": {"unit": "deg", "value": 0.0}
}
```

Files without `cross_hierarchy_joints` load an empty collection.

Exact target A/B order and occurrence-path element order roundtrip.

Only `revolute`, `active|inactive`, and `deg` limit/coordinate units are accepted in this seed.

No motion graph, holding drive, residual, Jacobian, snapshot, proposal, or solve result is serialized.

## Combined motion relationship identity

Block 28 derives four relationship classes:

```text
1. local geometric relationship
   (assembly_document, AssemblyConstraintId)

2. local joint
   (assembly_document, AssemblyJointId)

3. Project-level cross-hierarchy geometric relationship
   Project-level AssemblyConstraintId

4. Project-level cross-hierarchy joint
   Project-level AssemblyJointId
```

The public derived variant is:

```text
AssemblyMotionRelationshipIdentity
```

Variant alternative order is canonical relationship-kind order:

```text
local geometry
local joint
Project cross geometry
Project cross joint
```

Within one kind, document/id keys sort lexicographically.

## Participation semantics

Local geometry participates when the relationship is active and both local endpoint components are active.

Local joints participate when the joint is active and both local endpoint components are active.

Project-level cross geometry and Project-level cross joints participate when:

```text
relationship/joint state == Active
AND every SubassemblyInstance on target A exact path is Active
AND target A addressed component is Active
AND every SubassemblyInstance on target B exact path is Active
AND target B addressed component is Active
```

Visibility does not filter motion participation.

Suppression is the participation boundary.

## Joint-to-transform-authority incidence

Every participating relationship maps to each unique direct component transform authority on which its residual depends:

```text
ComponentTransformAuthority =
  (assembly_document: DocumentId,
   local ComponentInstanceId)
```

Incidence is unique by:

```text
(AssemblyMotionRelationshipIdentity,
 ComponentTransformAuthority)
```

Project-level cross-joint endpoint mappings remain separate:

```text
joint id
TargetA | TargetB
complete occurrence-qualified endpoint
ComponentTransformAuthority
```

Thus same-authority endpoints retain rooted geometry context without duplicating numeric variable ownership.

## Combined motion groups

`AssemblyCrossHierarchyMotionGraph` derives connected components over relationship and authority nodes.

Connectivity closes transitively over all four active relationship classes.

Only connected components containing at least one active Project-level cross-hierarchy joint are exposed through `motion_groups()`.

Pure-local motion groups remain ordinary local `AssemblyJointMotionSolver` work.

Pure cross-hierarchy geometric groups remain Block-26/27 geometric solve work.

One selected Project-level cross joint must belong to exactly one current motion group.

## Root-space Revolute target evaluation

`AssemblyHierarchyRevoluteJointEquationBuilder` converts each persistent endpoint into the existing derived hierarchy target query and resolves:

```text
feature.<feature-id>.seat
```

through `AssemblyHierarchyConstraintTargetResolver::resolve_insert`.

Each endpoint obtains a root-space axis and oriented seating plane after exact evaluation of:

```text
[T_component, T_inner_parent, ..., T_outer_parent]
```

The semantic `.seat` producer and target geometry contract are identical to local Insert/Revolute target semantics.

Unsupported non-seat target families fail closed.

## Shared Revolute residual mathematics

Local and cross-hierarchy Revolute equation builders now call one internal:

```text
build_revolute_joint_residual
```

For directed target A/B axis/seating frames:

```text
direction_alignment          = dA - dB
axis_offset_mm               = cross(oB - oA, dA)
signed_seating_separation_mm = dot(sB - sA, nA)

reference_cosine = dot(xA, xB)
reference_sine   = dot(dA, cross(xA, xB))

twist_alignment_sine =
  reference_sine * cos(target)
  - reference_cosine * sin(target)

twist_alignment_cosine =
  reference_cosine * cos(target)
  + reference_sine * sin(target)
  - 1
```

The shared numeric flattening order is:

```text
direction_alignment.x
direction_alignment.y
direction_alignment.z
axis_offset_mm.x / length_residual_scale_mm
axis_offset_mm.y / length_residual_scale_mm
axis_offset_mm.z / length_residual_scale_mm
signed_seating_separation_mm / length_residual_scale_mm
twist_alignment_sine
twist_alignment_cosine
```

Component count: `9`.

## Holding drives

For one selected Project-level cross-hierarchy Revolute joint:

```text
selected joint
  -> requested coordinate

other active local Revolute joints in the same motion group
  -> authored local coordinate

other active Project-level cross-hierarchy Revolute joints in the same motion group
  -> authored Project-level coordinate
```

Holding drives are transient numeric residual intent and are not persisted separately.

If authored holding drives conflict with the requested selected drive or geometric relationships, the shared numeric engine reports the resulting non-converged solve state instead of silently dropping relationships.

## Authority-scoped variables and shared optimizer

The motion group authority list is canonical Block-28 authority order.

Each unique free active authority contributes exactly:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

Grounded authorities remain fixed residual context and contribute no variable block.

Repeated rooted occurrences that map to one authority contribute one six-variable block and at most one proposal.

Candidate values are applied once to one Project copy per authority. Every local/cross geometric and local/cross joint residual depending on that authority observes the same perturbation.

Block 28 reuses:

```text
read_cross_hierarchy_authority_variables
apply_cross_hierarchy_authority_variables
build_central_difference_jacobian
solve_numeric_variables
```

There is no second motion optimizer or finite-difference implementation.

## Motion result contract

`AssemblyCrossHierarchyJointMotionResult` stores:

```text
selected Project-level AssemblyJointId
source selected coordinate
requested selected coordinate
solve state
iteration count
ordered combined motion relationship identities
fixed authorities
complete authority snapshots
complete four-family relationship snapshots
complete participating hierarchy-boundary snapshots
exact semantic PartDocument model-intent snapshots
at most one transform proposal per free authority
residual summary
```

Relationship snapshots preserve complete persistent records.

Local geometric snapshots preserve complete local `AssemblyConstraint` input.

Local joint snapshots preserve:

```text
containing assembly document
id
name
type
target A/B
state
limits
coordinate
```

Project cross-geometric snapshots preserve complete `AssemblyHierarchyConstraint` input.

Project cross-joint snapshots preserve complete `AssemblyHierarchyJoint` input including exact occurrence-qualified endpoints, limits, and authored coordinate.

## Shared Block-27 freshness

Block 28 extracts and reuses common cross-hierarchy freshness helpers for:

```text
complete authority snapshot creation
exact fixed/free authority subsequences
exact one-proposal-per-free-authority validation
proposed transform validation
hierarchy path-boundary snapshot creation
hierarchy path-boundary freshness
atomic authority proposal writes on a Project copy
```

The existing Block-27 geometric applier uses the same helpers after the refactor.

Motion additionally validates:

```text
complete combined relationship snapshot count/order/uniqueness
byte-for-byte current local/cross relationship records
exact current combined motion group
selected joint snapshot presence and uniqueness
selected source coordinate equality
requested coordinate within snapshotted/current limits
exact semantic PartDocument model intent
```

A newly active relationship or joint that joins/splits the motion group makes an old result stale.

Visibility changes do not stale a result. Suppression and path-boundary transform/child-target changes do.

## Atomic application

`AssemblyCrossHierarchyJointMotionResultApplier`:

1. accepts converged results only;
2. validates complete Project structure;
3. validates authority/proposal freshness;
4. validates all four relationship snapshot families;
5. validates selected joint source/request semantics;
6. rebuilds the current motion graph and requires the exact result group;
7. validates every participating hierarchy boundary;
8. validates exact canonical PartDocument model intent;
9. copies the Project;
10. applies at most one direct local component transform per free authority;
11. updates only the selected Project-level joint coordinate to the requested value;
12. replaces the source Project only after every operation succeeds.

It never writes:

```text
composed hierarchy transforms
root-space endpoint placement
SubassemblyInstance::transform()
occurrence-local child pose overrides
non-selected joint coordinates
```

The return count remains the applied transform proposal count, matching the local motion-applier convention.

## Persistence boundary

Persisted Block-28 additions:

```text
Project::cross_hierarchy_joints
AssemblyHierarchyJoint records
cross_hierarchy_joints[] Project JSON
selected authored joint coordinate after explicit successful motion application
```

Derived and unpersisted data:

```text
AssemblyMotionRelationshipIdentity values
motion relationship-to-authority incidence
cross-joint endpoint-authority mappings
combined motion groups
holding drive selection
root-space resolved axis/seating frames
Revolute residual descriptors
scaled mixed residual vectors
finite-difference Jacobians
numeric solve state
authority snapshots
relationship snapshots
hierarchy-boundary snapshots
semantic PartDocument snapshots
transform proposals
motion results
```

## Focused coverage

Core tags:

```text
[core][assembly-cross-hierarchy-joint]
[core][assembly-cross-hierarchy-joint-json]
[core][assembly-cross-hierarchy-motion-graph]
```

Geometry tags:

```text
[geometry][assembly-cross-hierarchy-revolute]
[geometry][assembly-cross-hierarchy-motion]
```

Coverage proves persistent endpoint/limit/coordinate semantics, same-authority distinct endpoint validity, Project/local id-scope independence, additive JSON, exact path/target order, exact structure validation, suppression/visibility participation, combined four-family relationship order, unique same-authority incidence with two mappings, root/child and nested root-space Revolute equations, signed twist semantics, shared residual construction, root-to-child motion convergence, nested motion propagation through rigid parent boundaries, one proposal per shared authority, deterministic holding drives, source immutability, atomic transform+coordinate application, unchanged subassembly boundaries, and complete modeled-input freshness.

## Next technical step

Implement Block 29 only: component geometry instancing and structured STEP assembly products.

The next block must define a stable derived component/product occurrence identity for exchange, preserve repeated assembly occurrences without duplicating persistent PartDocument intent, reuse canonical posed-leaf transform chains, and emit structured STEP assembly/product relationships instead of only one flattened compound.

Do not add occurrence-local flexible pose overrides, whole-subassembly solve variables, or swept-motion contact simulation in Block 29.

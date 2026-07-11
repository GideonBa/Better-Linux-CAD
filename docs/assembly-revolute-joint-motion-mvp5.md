# Revolute Joint, Limits, and First Motion Seed MVP-5

Status: implemented the first persistent assembly motion relationship family: local Revolute joint intent with limits and authored coordinate, deterministic joint connectivity, signed drive residuals, shared numeric solve-engine reuse, complete component/joint stale snapshots, atomic transform-plus-coordinate application, and a headless motion command.

## Separation from geometric constraints

`AssemblyJoint` intent is persistent and separate from `AssemblyConstraint` intent.

The first family is:

```text
AssemblyJointType::Revolute
```

A Revolute joint stores:

```text
id
name
type
semantic target A
semantic target B
active | inactive state
lower angle limit
upper angle limit
authored current coordinate
```

Adding or loading a joint does not move components.

## Limit and coordinate contract

The current seed uses the principal degree domain:

```text
-180 deg <= lower < upper <= 180 deg
lower <= coordinate <= upper
```

Requested motion outside the persistent limits is rejected rather than clamped.

The coordinate is persistent authored motion state.

Multi-turn coordinates and wrap counts are not implemented.

## Semantic endpoint contract

Revolute endpoints are local `AssemblyConstraintTarget`-shaped semantic targets in the containing `AssemblyDocument`.

The first Revolute geometry producer is:

```text
feature.<feature-id>.seat
```

The existing seating-target resolver provides:

```text
primary axis
oriented seating plane
```

from the same exact circular feature/profile identity.

Revolute reuses this semantic geometry rather than persisting an OCCT cylinder face or raw topology id.

## Directed axis and signed coordinate

Concentric and Insert accept equal or opposed axis directions. A signed Revolute coordinate requires an oriented positive axis.

The Revolute drive therefore uses directed alignment:

```text
direction_alignment = dA - dB
```

Target A owns positive rotation direction.

The remaining seating terms are:

```text
axis_offset_mm               = cross(oB - oA, dA)
signed_seating_separation_mm = dot(sB - sA, nA)
```

## Signed twist residual

Let `xA` and `xB` be the oriented seating-plane x axes and `dA` the target-A directed axis.

```text
reference_cosine = dot(xA, xB)
reference_sine   = dot(dA, cross(xA, xB))
```

The target coordinate is enforced with a periodic sine/cosine pair:

```text
twist_alignment_sine   = sin(phi - target)
twist_alignment_cosine = cos(phi - target) - 1
```

The pair avoids an `atan2` branch cut inside the residual path.

One driven Revolute query contributes nine scalar residual components:

```text
direction_alignment.x
direction_alignment.y
direction_alignment.z
axis_offset_mm.x / scale
axis_offset_mm.y / scale
axis_offset_mm.z / scale
signed_seating_separation_mm / scale
twist_alignment_sine
twist_alignment_cosine
```

A regular one-free-body driven query is covered as a `9 x 6` finite-difference Jacobian of rank `6` because the requested coordinate temporarily constrains the joint's one free motion coordinate.

This rank is query-time drive behavior, not persistent authority that the undriven joint has zero free DOF.

## Deterministic joint graph

`AssemblyJointGraph` contains every local component as a node and every active local joint as a distinct edge.

It preserves multi-edges, sorts deterministically, and exposes local connected groups.

Inactive joints do not contribute graph edges.

The graph is derived and unpersisted.

## Combined motion relationship groups

Motion cannot partition on the joint graph alone because local geometric constraints and local joints may transitively couple the same components.

The motion path derives a combined local relationship closure over:

```text
active local geometric constraint edges
active local joint edges
```

The selected Revolute joint determines the initial group.

The selected joint receives the requested coordinate.

Every other active Revolute joint in the same active combined group receives a transient holding drive at its persisted coordinate.

Example:

```text
joint.a persisted coordinate = 0 deg
joint.z selected request      = 35 deg

numeric drives:
  joint.a -> 0 deg
  joint.z -> 35 deg
```

This prevents one motion request from silently releasing another active joint in the same coupled local mechanism.

## Suppression participation

The selected joint requires active, non-suppressed endpoints.

A selected joint touching a suppressed component is rejected.

Non-selected joint drives touching suppressed components vanish from the active numeric motion subgroup under the documented suppression policy.

Complete component snapshots still include suppressed group components for stale-result validation.

## Shared numeric solve engine

Both ordinary geometric solving and joint motion use:

```text
detail::solve_numeric_relationships
```

```text
AssemblyRigidBodySolver
  -> geometric constraint ids
  -> empty Revolute drive set

AssemblyJointMotionSolver
  -> geometric constraint ids
  -> deterministic Revolute drive set
```

The shared engine preserves:

- six direct transform variables per free active local component;
- deterministic relationship/component ordering;
- central finite differences;
- damped Gauss-Newton normal equations;
- partial-pivot Gaussian elimination;
- damping escalation and backtracking;
- existing solve states;
- source-project immutability;
- complete component snapshots;
- transform proposals for free active components.

There is no Revolute-specific optimizer.

## Motion result

`AssemblyJointMotionSolver::move` is read-only and returns:

```text
AssemblyJointMotionResult
  selected joint id
  selected source coordinate
  requested coordinate
  complete driven-joint snapshots
  embedded AssemblySolveResult
```

Every numerically driven joint snapshot stores:

```text
joint id
type
state
target A
target B
limits
source coordinate
```

Motion results and snapshots are derived and unpersisted.

## Atomic application

`AssemblyJointMotionResultApplier` rejects:

- non-converged motion results;
- duplicate joint snapshots;
- deleted driven joints;
- any driven-joint type/state/target/limit/coordinate change;
- inconsistent selected-joint identity/context;
- every component stale condition enforced by `AssemblySolveResultApplier`.

Successful application is atomic:

```text
copy Project
  -> apply embedded direct component-transform proposals
  -> replace selected persistent joint coordinate
  -> commit Project copy
```

If either step fails, the source project remains unchanged.

## JSON persistence

The optional local assembly collection is:

```text
assembly_joints[]
```

Representative record:

```json
{
  "id": "joint.revolute",
  "name": "Plate Revolute Joint",
  "type": "revolute",
  "target_a": {
    "component_instance": "component.plate.grounded",
    "semantic_reference": "feature.hole.seat"
  },
  "target_b": {
    "component_instance": "component.plate.free",
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

Files without `assembly_joints` load with an empty joint collection.

Persistent intent includes joint identity/family/endpoints/state/limits/current coordinate and direct component transforms after explicit successful application.

Derived data includes joint graph connectivity, combined groups, resolved seat geometry, Revolute residuals, transient drive sets, numeric residuals/Jacobians, solve state, motion results, joint snapshots, and unapplied proposals.

## Headless motion example

```bash
./build/dev-geometry/blcad_move_joint \
  examples/revolute_joint.blcad.project.json \
  joint.revolute \
  45 \
  build/revolute_joint_45.blcad.project.json
```

The output project persists the applied direct component transform and selected joint coordinate `45 deg`.

It can be passed directly to the posed assembly STEP exporter.

## Proven behavior

Core coverage includes:

- joint identity/endpoint validation;
- Angle quantity enforcement;
- principal-domain and ordered-limit validation;
- coordinate-in-range validation;
- immutable coordinate update;
- local `AssemblyDocument` joint endpoint validation;
- assembly/project JSON roundtrip and backward-compatible loading;
- deterministic active joint graph behavior.

Geometry coverage includes:

- zero and signed twist residuals;
- exact nine-component flattening;
- regular driven rank `6/6`;
- deterministic in-range motion solving;
- source-project immutability;
- atomic transform plus coordinate application;
- hard limit rejection;
- grounded participation;
- inactive selected-joint rejection;
- selected-endpoint suppression rejection;
- non-selected suppressed-drive filtering;
- stale component and joint snapshot rejection;
- multi-joint holding drives.

## Locality and hierarchy boundary

This joint seed remains local to one temporary solve view's root `AssemblyDocument`.

Rigid subassembly hierarchy and document-scoped flexible child solving were implemented later as separate blocks, but Revolute endpoint identity is still local and motion does not propagate through cross-hierarchy joint relationships.

Repeated occurrences of one child document share child-local joint records and component transform authority because the child document is one shared model definition.

The current cross-hierarchy geometric planning distinguishes occurrence identity from transform authority in `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`. Cross-hierarchy joints must reuse that distinction after geometric solve connectivity is stable.

## Still deferred

- prismatic, cylindrical, planar, ball, pin-slot, or richer joint families;
- multi-turn Revolute coordinates and persisted wrap counts;
- joint velocity, acceleration, time, torque, force, spring, damper, or actuator intent;
- continuous simulation or integration caches;
- motion trajectories, studies, or animation timelines;
- project-level cross-hierarchy joint intent;
- nested motion propagation across assembly-document boundaries;
- whole-subassembly motion variables;
- collision/interference checking during motion or swept-motion analysis;
- null-space motion exploration without an explicit requested coordinate.

Cross-hierarchy joint semantics are intentionally deferred until cross-hierarchy geometric blocks 23-27 are complete.

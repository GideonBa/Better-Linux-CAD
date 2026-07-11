# Revolute Joint Intent, Limits, and Motion MVP-5

Status: implemented the first persistent solver-independent joint family and the first explicit motion query/application path through the shared rigid-body numeric engine.

## Intent boundary

A joint is not an `AssemblyConstraint`. Geometric constraints express assembly relationships that the ordinary rigid-body solver satisfies. Joint records express authored motion relationship intent and an authored joint coordinate.

The persistent core model adds:

```text
AssemblyJointId
AssemblyJoint
AssemblyJointType::Revolute
AssemblyJointState::Active | Inactive
AssemblyJointLimits
AssemblyDocument::joints[]
```

The first record stores:

```text
id
name
type = Revolute
target_a
target_b
state
lower limit in degrees
upper limit in degrees
coordinate in degrees
```

Targets reuse `AssemblyConstraintTarget`. The record therefore keeps the existing component-instance id plus semantic-reference string boundary and never stores an OCCT topology id.

Adding or loading a joint never moves a component.

## Revolute seed domain and limit policy

The first Revolute family uses a single signed principal-angle coordinate.

```text
-180 deg <= lower_limit < upper_limit <= 180 deg
lower_limit <= coordinate <= upper_limit
```

The current motion API uses **rejection semantics** for limits. A requested coordinate below the lower limit or above the upper limit fails validation. It is not silently clamped.

Example:

```text
limits:     [-90 deg, +90 deg]
coordinate: 0 deg
request:    +45 deg   -> valid motion query
request:    +91 deg   -> validation error
```

The one-turn principal domain is an explicit seed boundary. Multi-turn revolute coordinates and wrap-count intent require a later model design.

## Semantic Revolute endpoints

The first motion-capable joint family uses the existing stable seating endpoint:

```text
feature.<feature-id>.seat
```

`AssemblyConstraintTargetResolver::resolve_insert` already derives from this one target:

```text
primary axis
oriented seating plane
```

The Revolute path reuses that exact source feature/profile identity and evaluates both descriptors in assembly space through `AssemblyTransformEvaluator`.

No `joint_axis`, raw cylinder-face id, OCCT label, or hidden second endpoint is persisted.

## Directed axis and signed positive rotation

Concentric and Insert accept equal or opposed axis directions because their current residual semantics are direction-symmetric.

A signed Revolute coordinate cannot use that symmetry. Target A defines the positive rotation direction, so Revolute explicitly requires aligned **directed** axes:

```text
direction_alignment = dA - dB
```

An opposed axis is therefore not a satisfied Revolute state.

The relative twist uses the oriented seating-plane x axes:

```text
reference_cosine = dot(xA, xB)
reference_sine   = dot(dA, cross(xA, xB))
```

For requested target angle `theta`:

```text
twist_alignment_sine   = sin(phi - theta)
twist_alignment_cosine = cos(phi - theta) - 1
```

The implementation evaluates these terms without an `atan2` branch cut:

```text
twist_alignment_sine
  = reference_sine * cos(theta) - reference_cosine * sin(theta)

twist_alignment_cosine
  = reference_cosine * cos(theta) + reference_sine * sin(theta) - 1
```

The sine row carries the regular local twist rank at a satisfied state. The cosine row is intentionally locally redundant there but keeps the periodic target condition explicit.

## Complete Revolute drive residual

One transient Revolute drive contributes nine scalar components in this exact order:

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

The geometric terms are:

```text
direction_alignment          = dA - dB
axis_offset_mm               = cross(oB - oA, dA)
signed_seating_separation_mm = dot(sB - sA, nA)
```

For one grounded component and one free component at a regular satisfied Revolute state, the shared central finite-difference Jacobian is proven as:

```text
residual_component_count = 9
variable_count           = 6
jacobian_rank            = 6
remaining_local_dof      = 0 for one driven coordinate query
```

The persistent Revolute joint itself represents one motion DOF. A **motion query** supplies a coordinate target and therefore temporarily constrains that DOF for pose solving.

## Joint graph

`AssemblyJointGraph` is read-only derived connectivity.

It contains:

```text
every component instance as one node
every active joint as one distinct edge
```

Inactive joints are excluded. Legal multi-edges are preserved. Nodes and joint edges are sorted lexicographically and connected groups are deterministic.

The graph is not serialized.

## Combined relationship group for motion

A motion query cannot look only at the selected joint. Other constraints and joints connected to either selected endpoint can affect the same rigid bodies.

`AssemblyJointMotionSolver` therefore derives the transitive closure over both active relationship graphs:

```text
selected Revolute endpoints
  -> active AssemblyConstraintGraph edges
  -> active AssemblyJointGraph edges
  -> repeat until no component is added
  -> lexicographically sorted combined component group
```

Suppressed components remain in the complete combined group so solve snapshots still cover suppression changes. The numeric subgroup removes suppressed components. Constraints and non-selected joints touching a suppressed component vanish from the active numeric relationship set.

The selected joint itself requires both target components to be active and non-suppressed. Driving a selected joint through a suppressed endpoint is rejected.

## Deterministic drive set

The shared numeric relationship set now contains two derived ordered families:

```text
constraint_ids[]
revolute_drives[]
```

Residual evaluation order is:

```text
1. active geometric constraints in AssemblyConstraintGraph lexicographic order
2. active Revolute drives in AssemblyJointGraph lexicographic order
```

For the selected joint:

```text
drive coordinate = requested coordinate
```

For every other active Revolute joint in the same active combined group:

```text
drive coordinate = persisted authored coordinate
```

Example:

```text
A -- joint.a(0 deg) -- B -- joint.z(0 deg) -- C

move joint.z to 35 deg

numeric drives:
  joint.a -> 0 deg
  joint.z -> 35 deg
```

This prevents a motion request from silently releasing another active joint in the same relationship group.

## Shared numeric solve engine

The former rigid-body solver iteration loop is now a private shared engine:

```text
detail::solve_numeric_relationships
```

Both paths use it:

```text
AssemblyRigidBodySolver
  -> geometric constraint ids
  -> empty Revolute drive set
  -> shared numeric solve engine

AssemblyJointMotionSolver
  -> geometric constraint ids
  -> deterministic Revolute drive set
  -> shared numeric solve engine
```

The engine preserves the established contracts:

- six direct rigid-transform variables per free active component;
- lexicographic relationship/component ordering supplied by the derived graphs;
- central finite-difference Jacobian;
- damped Gauss-Newton normal equations;
- partial-pivot Gaussian elimination;
- deterministic damping escalation and backtracking line search;
- existing solve states;
- source-project immutability during solve;
- complete component input snapshots;
- transform proposals only for free active components.

There is no Revolute-specific optimizer.

## Motion result and atomic application

`AssemblyJointMotionSolver::move` is read-only. It returns:

```text
AssemblyJointMotionResult
  selected joint id
  selected source coordinate
  requested coordinate
  complete driven-joint input snapshots
  AssemblySolveResult
```

Every numerically driven joint is snapshotted with:

```text
joint id
type
state
target A
target B
limits
source coordinate
```

`AssemblyJointMotionResultApplier` rejects:

- non-converged motion results;
- duplicate joint snapshots;
- deleted driven joints;
- any driven-joint type/state/target/limit/coordinate change;
- an inconsistent selected-joint snapshot;
- every component stale condition already enforced by `AssemblySolveResultApplier`.

Successful application is atomic:

```text
copy Project
  -> apply embedded transform proposals through AssemblySolveResultApplier
  -> replace selected persistent joint coordinate
  -> commit Project copy
```

If either transform application or coordinate update fails, the source project remains unchanged.

## Persistence

The embedded assembly JSON remains on the historical additive marker:

```text
blcad.assembly_document.mvp4
version 1
```

The new optional collection is:

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

Files without `assembly_joints` remain loadable and produce an empty joint collection.

Persisted model intent:

```text
joint identity
joint family
semantic endpoints
active/inactive state
limit range
authored current coordinate
component transforms after explicit successful application
```

Derived and unpersisted:

```text
joint graph connectivity
combined relationship groups
resolved axis/seat geometry
Revolute residual descriptors
numeric relationship sets
Revolute drive values for one query
residual vectors
Jacobians
normal equations
solver iteration state
motion results
joint input snapshots
unapplied transform proposals
```

## Headless motion example

Checked-in input:

```text
examples/revolute_joint.blcad.project.json
```

The complete current flow is:

```text
read_project_json_file
  -> AssemblyJointMotionSolver::move
  -> AssemblyJointMotionResultApplier::apply
  -> write_project_json_file
```

Example:

```bash
./build/dev-geometry/blcad_move_joint \
  examples/revolute_joint.blcad.project.json \
  joint.revolute \
  45 \
  build/revolute_joint_45.blcad.project.json
```

The output project persists the applied component transform and the selected joint coordinate `45 deg`. It can be passed directly to the posed assembly STEP exporter.

## Proven behavior

Core coverage:

- joint id/name/endpoint validation;
- angle quantity enforcement;
- principal-domain and ordered-limit validation;
- coordinate-in-range validation;
- immutable copy-style coordinate update;
- AssemblyDocument joint identity and endpoint validation;
- assembly/project JSON roundtrip;
- backward-compatible loading without `assembly_joints`;
- active-only joint graph edges, legal multi-edges, lexicographic ordering, deterministic groups, and isolated nodes.

Geometry coverage:

- zero and signed oriented twist residuals;
- exact nine-component shared flattening;
- regular shared finite-difference Jacobian rank `6/6` for one driven free body;
- deterministic repeated in-range motion solve;
- explicit source-project immutability before application;
- atomic transform plus joint-coordinate application;
- hard lower/upper limit rejection;
- grounded participation;
- inactive selected-joint rejection;
- suppressed selected-endpoint rejection;
- filtering of non-selected joints touching suppressed components while keeping full component snapshots;
- stale component-input rejection;
- stale joint-coordinate rejection;
- multi-joint motion holding other active joint coordinates.

## Still not implemented

- prismatic, cylindrical, planar, ball, pin-slot, or richer joint families;
- multi-turn Revolute coordinates or persisted wrap counts;
- joint velocity, acceleration, time, torque, force, spring, damper, or actuator intent;
- continuous simulation or integration caches;
- motion trajectories, studies, or animation timelines;
- rigid/flexible subassemblies;
- collision or interference checking during motion;
- null-space motion exploration without an explicit requested joint coordinate.

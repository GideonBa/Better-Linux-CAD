# Revolute Joint, Limits, and First Motion Seed MVP-5

Status: implemented the first persistent assembly motion relationship family: local Revolute joint intent with limits and authored coordinate, deterministic joint connectivity, signed drive residuals, shared numeric solve-engine reuse, complete component/joint/semantic-PartDocument freshness, atomic transform-plus-coordinate application, and a headless motion command.

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

Requested motion outside persistent limits is rejected rather than clamped.

The coordinate is persistent authored motion state.

Multi-turn coordinates and wrap counts are not implemented.

## Semantic endpoint contract

Revolute endpoints are local `AssemblyConstraintTarget`-shaped semantic targets in the containing `AssemblyDocument`.

The first Revolute geometry producer is:

```text
feature.<feature-id>.seat
```

The existing seating-target resolver provides one primary axis and oriented seating plane from the same exact circular feature/profile identity.

Revolute reuses this semantic geometry rather than persisting OCCT topology identity.

## Directed axis and signed coordinate

Concentric and Insert accept equal or opposed axis directions. Signed Revolute coordinates require an oriented positive axis.

The drive therefore uses:

```text
direction_alignment = dA - dB
```

Target A owns positive rotation direction.

Remaining seating terms are:

```text
axis_offset_mm               = cross(oB - oA, dA)
signed_seating_separation_mm = dot(sB - sA, nA)
```

## Signed twist residual

For oriented seating-plane x axes `xA`, `xB` and target-A directed axis `dA`:

```text
reference_cosine = dot(xA, xB)
reference_sine   = dot(dA, cross(xA, xB))
```

The requested coordinate is enforced with:

```text
twist_alignment_sine   = sin(phi - target)
twist_alignment_cosine = cos(phi - target) - 1
```

The pair avoids an `atan2` branch cut inside the residual path.

One driven Revolute query contributes nine scalar residuals:

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

A regular one-free-body driven query has a covered `9 x 6` finite-difference Jacobian of rank `6` because the query temporarily constrains the one free joint coordinate.

This is query-time drive behavior, not persistent authority that an undriven Revolute joint has zero free DOF.

## Deterministic joint graph

`AssemblyJointGraph` contains every local component as a node and every active local joint as a distinct edge.

It preserves multi-edges, sorts deterministically, and exposes local connected groups.

Inactive joints do not contribute edges.

The graph is derived and unpersisted.

## Combined motion relationship groups

Motion partitions over the combined closure of:

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

This prevents one motion request from silently releasing another active joint in the coupled local mechanism.

## Suppression participation

The selected joint requires active, non-suppressed endpoints.

A selected joint touching a suppressed component is rejected.

Non-selected joint drives touching suppressed components vanish from the active numeric subgroup.

Complete component snapshots still preserve the complete combined component group for stale-result validation.

## Shared numeric solve engine

Ordinary geometric solving and joint motion both call:

```text
detail::solve_numeric_relationships
```

The local adapter delegates to the shared absolute-variable evaluator engine used by cross-hierarchy solving as well.

The path preserves:

- six direct transform variables per free active local component;
- deterministic relationship/component ordering;
- shared residual flattening for geometric relationships;
- central finite differences;
- damped Gauss-Newton normal equations;
- partial-pivot dense solve;
- damping escalation and backtracking;
- existing solve states;
- source Project immutability;
- complete component snapshots;
- canonical semantic target PartDocument snapshots;
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

The embedded `AssemblySolveResult` additionally owns complete component and canonical semantic target PartDocument freshness context.

Motion results and snapshots are derived and unpersisted.

## Atomic application

`AssemblyJointMotionResultApplier` first validates complete driven-joint input snapshots.

It rejects:

- non-converged motion results;
- duplicate joint snapshots;
- deleted driven joints;
- driven-joint type/state/target/limit/coordinate changes;
- inconsistent selected-joint identity/context.

It then creates a candidate Project and delegates the embedded component solve result to `AssemblySolveResultApplier`.

That shared local applier additionally rejects:

- stale component referenced-part/transform/grounding/suppression inputs;
- changed canonical PartDocument model intent for participating semantic target producers;
- duplicate or inconsistent transform proposals;
- invalid proposed transforms.

Successful application is atomic:

```text
copy Project
  -> validate/apply embedded direct component transforms
  -> replace selected persistent joint coordinate
  -> commit Project copy
```

If either transform or coordinate application fails, the source Project remains unchanged.

## Semantic target PartDocument freshness

Block 27 deliberately applies the same Option-A freshness contract to ordinary local solve results used by joint motion.

For every unique PartDocument referenced by the complete combined component group, the embedded result stores the exact canonical:

```text
serialize_part_document_to_json(part)
```

payload.

At motion application the current payload must compare byte-for-byte equal.

A participating target-producing part parameter, formula, workplane, sketch/profile, reference-recovery/remap, or feature-history model-intent edit therefore invalidates the old motion result before any component transform or joint coordinate changes.

This is conservative and unpersisted. It is not a minimal target dependency closure.

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

Files without `assembly_joints` load with an empty collection.

Persistent intent includes joint identity/family/endpoints/state/limits/current coordinate and direct component transforms after explicit successful application.

Derived data includes joint graph connectivity, combined groups, resolved seat geometry, Revolute residuals, transient drives, numeric residuals/Jacobians, solve state, motion results, joint/component/semantic-PartDocument snapshots, and unapplied proposals.

## Headless motion example

```bash
./build/dev-geometry/blcad_move_joint \
  examples/revolute_joint.blcad.project.json \
  joint.revolute \
  45 \
  build/revolute_joint_45.blcad.project.json
```

The output Project persists the applied direct component transform and selected joint coordinate `45 deg`.

It can be passed directly to the posed assembly STEP exporter.

## Proven behavior

Core coverage includes joint identity/endpoint validation, Angle quantity enforcement, principal-domain/limit validation, coordinate validation/update, local joint endpoint validation, JSON roundtrip/backward compatibility, and deterministic active joint graph behavior.

Geometry coverage includes zero and signed twist residuals, exact nine-component flattening, regular driven rank `6/6`, deterministic in-range motion solving, source immutability, atomic transform plus coordinate application, hard limit rejection, grounding, inactive/suppressed selected-joint rejection, non-selected suppressed-drive filtering, stale component/joint snapshot rejection, multi-joint holding drives, and inherited semantic PartDocument freshness through the embedded local solve result.

Focused commands:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-revolute-joint]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-semantic-freshness]"
```

## Locality and hierarchy boundary

The current Revolute seed remains local to one temporary solve view's root `AssemblyDocument`.

Rigid subassembly hierarchy, document-scoped flexible child solving, and complete cross-hierarchy geometric solving/application/diagnostics are implemented separately.

Repeated occurrences of one child document share child-local joint records and direct component transform authority because the child document remains one shared model definition.

Cross-hierarchy joint intent must preserve occurrence-qualified geometric endpoints while mapping motion variables/proposals to `ComponentTransformAuthority` values.

## Next motion block

Block 28 from `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md` is now the next assembly technical step:

```text
persistent project-level cross-hierarchy joint intent
  -> exact occurrence-qualified Revolute endpoints
  -> joint-to-ComponentTransformAuthority incidence
  -> combined local geometric + local joint + cross-hierarchy geometric + cross-hierarchy joint motion closure
  -> nested root-space Revolute drive residual evaluation
  -> shared numeric engine
  -> complete Block-27-style freshness and atomic transform + coordinate application
```

Still deferred beyond Block 28:

- prismatic, cylindrical, planar, ball, pin-slot, or richer joint families;
- multi-turn Revolute coordinates and persisted wrap counts;
- velocity, acceleration, time, torque, force, spring, damper, or actuator intent;
- continuous simulation/integration caches;
- motion trajectories, studies, or animation timelines;
- occurrence-local child pose overrides;
- whole-subassembly motion variables;
- swept-motion contact analysis;
- null-space motion exploration without an explicit requested coordinate.

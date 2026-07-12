# Revolute Joint, Limits, and Local Motion Seed MVP-5

Status: implemented. Block 28 additionally reuses the same Revolute residual mathematics and numeric engine for Project-level occurrence-qualified cross-hierarchy motion.

This document is canonical for local `AssemblyJoint` Revolute intent, local joint connectivity, local motion solving, freshness, and atomic transform-plus-coordinate application.

Cross-hierarchy Revolute semantics are canonical in `docs/assembly-cross-hierarchy-revolute-motion-mvp5.md`.

## Separation from geometric constraints

Persistent local joint intent is separate from `AssemblyConstraint` intent.

The first family is:

```text
AssemblyJointType::Revolute
```

One local Revolute joint stores:

```text
AssemblyJointId
name
type
target A: local AssemblyConstraintTarget
target B: local AssemblyConstraintTarget
active | inactive state
lower angle limit
upper angle limit
authored current coordinate
```

Adding/loading a joint does not move components.

## Local target identity

A local joint endpoint is:

```text
(local ComponentInstanceId,
 semantic_reference)
```

Targets belong to one containing `AssemblyDocument`.

Local `AssemblyJoint` requires distinct target component ids.

Target A/B order is directed motion identity and is not normalized.

The first Revolute target family is:

```text
feature.<feature-id>.seat
```

`.seat` resolves one exact circular-cut axis plus one oriented seating plane from the same feature/profile.

## Limit and coordinate contract

The current seed uses the principal degree domain:

```text
-180 deg <= lower < upper <= 180 deg
lower <= coordinate <= upper
```

Limits, authored coordinate, and requested coordinate use `AngleDeg`.

Requested motion outside persistent limits is rejected rather than clamped.

Multi-turn coordinates remain deferred.

## Local joint connectivity

`AssemblyJointGraph` derives active local joint connectivity.

Joint nodes are local component ids. Active joint edges preserve joint id and target A/B component order.

The local motion solver closes over active local geometric constraints and active local joints that share components.

Suppressed endpoint components do not participate. Visibility does not remove relationships from solving.

Graph state is derived and unpersisted.

## Revolute target evaluation

`AssemblyRevoluteJointEquationBuilder` resolves both `.seat` endpoints through existing local semantic target resolution.

Each target yields assembly-space:

```text
axis origin
directed axis direction
seating-plane origin
seating-plane normal
seating-plane x axis
seating-plane y axis
```

The direct persisted component transform is evaluated through `AssemblyTransformEvaluator`.

No OCCT topology id or resolved frame becomes persistent joint intent.

## Shared Revolute residual constructor

Local and Block-28 cross-hierarchy Revolute equation builders call one internal:

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

Target A owns directed axis/seating/twist sign semantics.

The periodic sine/cosine pair avoids a discontinuous wrapped-angle subtraction in the numeric residual.

There is no separate cross-hierarchy Revolute formula.

## Shared numeric scalar order

Revolute residual flattening is shared by local and cross-hierarchy motion:

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

Component count:

```text
9
```

## Selected drive and holding drives

For one local motion request:

```text
selected active local Revolute joint
  -> requested in-range coordinate

other active local Revolute joints in the combined local relationship closure
  -> authored current coordinate
```

Holding drives are transient numeric intent. They are not serialized as separate records.

All active local geometric constraints in the same closure remain ordinary residuals.

Conflicting geometric relationships or holding drives are not silently removed. The existing numeric solve-state contract reports non-convergence or inconsistency.

## Shared numeric engine

Local motion reuses:

```text
AssemblyNumericRelationshipSet
shared residual flattening
absolute candidate variable evaluator
central finite differences
solve_numeric_variables
```

Each free active local component contributes:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

Grounded components remain fixed residual context.

The local joint motion path does not own a second optimizer.

Block 28 uses the same central finite-difference and Gauss-Newton engine with `ComponentTransformAuthority` variable ownership across assembly-document boundaries.

## Local motion result

`AssemblyJointMotionResult` protects:

```text
selected joint id
source authored coordinate
requested coordinate
complete local joint snapshots
embedded AssemblySolveResult
```

The embedded local solve result protects complete component snapshots, exact active free/fixed subsequences, exact transform proposals, and exact semantic target-producing PartDocument model-intent snapshots.

Joint snapshots preserve complete persistent joint inputs:

```text
id
name
type
target A/B
state
limits
authored coordinate
```

## Exact semantic PartDocument freshness

Local motion inherits the ordinary local solve freshness contract.

For every participating referenced part, the result stores:

```text
AssemblySemanticTargetPartSnapshot
  part_document
  canonical_model_intent_json
```

where the payload is exactly:

```text
serialize_part_document_to_json(part)
```

Application requires byte-for-byte equality with the current canonical PartDocument model intent.

This same freshness contract is reused by flexible-child solving, cross-hierarchy geometric solving, and Block-28 cross-hierarchy motion.

## Atomic local motion application

`AssemblyJointMotionResultApplier`:

1. accepts converged results only;
2. validates complete joint snapshots and selected source/request context;
3. delegates component/proposal/semantic-PartDocument freshness to `AssemblySolveResultApplier` on a Project copy;
4. applies direct local component transforms on that copy;
5. updates only the selected local joint authored coordinate;
6. replaces the source Project only after every operation succeeds.

Non-selected authored joint coordinates are not changed.

## Persistence boundary

Persistent local motion intent:

```text
AssemblyJoint records in AssemblyDocument::assembly_joints[]
local component direct transforms
selected authored local joint coordinate after successful explicit application
```

Derived and unpersisted:

```text
AssemblyJointGraph
combined local relationship closure
resolved axis/seating frames
selected/holding drive set
Revolute residual descriptors
scaled residual vectors
Jacobian
numeric solve state
component/joint/PartDocument freshness snapshots
transform proposals
motion results
```

## Block-28 cross-hierarchy follow-up

Implemented in `docs/assembly-cross-hierarchy-revolute-motion-mvp5.md`:

```text
AssemblyHierarchyJoint
Project::cross_hierarchy_joints
cross_hierarchy_joints[]
exact occurrence-qualified .seat endpoints
joint endpoint -> ComponentTransformAuthority mapping
combined local/cross geometric/joint motion graph
root-space nested Revolute equations
same shared Revolute residual constructor
same shared numeric engine
Block-27 authority/boundary/PartDocument freshness
atomic authority transforms + selected Project joint coordinate
```

Cross-hierarchy joint endpoints may be distinct rooted occurrences that map to one shared transform authority. Such endpoints retain two occurrence-qualified geometry contexts but contribute one six-variable authority block.

## Focused coverage

Local motion:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-revolute-joint]"
```

Cross-hierarchy Revolute equation and motion follow-up:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-revolute]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-motion]"
```

The combined coverage proves local joint intent/JSON/connectivity, signed periodic twist, holding-drive semantics, shared numeric-engine reuse, complete fresh atomic local application, exact root-space occurrence-qualified Revolute semantics, nested parent-chain motion, same-authority endpoint handling, and fresh atomic Project-level transform-plus-coordinate application.

## Current handoff

Block 28 is implemented. The current sequence source of truth is `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

Next is Block 29 only: freeze derived assembly/component/product occurrence identities and emit structured STEP assembly/product relationships while reusing canonical posed-leaf transform chains and one recomputed shape/cache per unique referenced `PartDocument`.

Occurrence-local child pose overrides, whole-subassembly solve variables, richer joint families, multi-turn motion, and swept-motion contact analysis remain deferred.

# Assembly Joint Target Compatibility and Oriented Frame Contract MVP-5

Status: implemented as Block 40 of `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

This document is canonical for derived joint target compatibility, the oriented `Frame` contract used by current Revolute equations, and local/cross-hierarchy parity. Its former Block-41 handoff is now implemented by `docs/assembly-joint-coordinate-model-mvp5.md`.

## Scope

Block 40 adds:

- `AssemblyJointTargetCompatibilityResolver`;
- deterministic ordered capability selection for joint endpoints;
- the first compatibility rule, `Revolute -> Frame / Frame`;
- compatibility-before-projection in both existing Revolute equation builders;
- one shared Frame-based residual construction path for local and cross-hierarchy targets;
- explicit Axis-only rejection.

Block 40 adds no joint family, coordinate slot, limit representation, JSON field, graph, solver, transform authority, finite-difference path, freshness rule, or result-application rule.

## Compatibility authority

The resolver consumes:

```text
AssemblyJointType
AssemblyResolvedGeometricTarget target A
AssemblyResolvedGeometricTarget target B
```

and returns one ordered transient projection contract:

```text
AssemblyJointTargetCompatibility
  joint_type
  target_a_capability
  target_b_capability
```

The implemented matrix is deliberately narrow:

```text
Revolute  Frame <-> Frame
```

The resolver validates both resolved targets first and fails closed when the ordered capability pair is unsupported. Compatibility results are derived query state and are never serialized.

## Oriented Frame contract

A valid `Frame` supplies:

```text
origin
unit x axis
unit y axis
unit z axis
```

The axes are finite, orthonormal, and right-handed. For Revolute execution:

```text
axis origin      = Frame.origin
axis direction   = Frame.z_axis
seat origin      = Frame.origin
seat x reference = Frame.x_axis
seat normal      = Frame.z_axis
```

The signed twist residual requires both the axis direction and the reference X direction. Therefore an `Axis` alone is not an oriented motion target.

## No implicit Axis-to-Frame synthesis

This is explicitly invalid:

```text
Axis
  -> choose an arbitrary world direction
  -> construct a hidden X reference
  -> synthesize Frame
```

Such a choice would make the meaning and sign of a joint coordinate depend on Geometry implementation accidents. Axis-only Revolute requests fail with a validation error from `geometry.assembly_joint_target_compatibility`.

A source may expose `Frame` in the future only when persistent model semantics define its orientation deterministically. Such a source can then enter the same compatibility, projection, and residual path without changing Revolute mathematics.

## Current semantic source

The existing circular feature `.seat` source exposes:

```text
Plane
Axis
Frame
```

Its Frame is derived deterministically from the authored circular feature and seating-plane orientation. Block 40 preserves current `.seat` execution and does not change endpoint spelling or persistence.

The existing `.axis` source exposes Axis/Cylinder but no Frame and is therefore intentionally incompatible with Revolute.

## Equation-space evaluation

Target identity and coordinate scope remain unchanged:

- local resolved targets remain `ComponentLocal` with local endpoint identity;
- hierarchy resolved targets remain `RootAssembly` with exact rooted occurrence identity.

After compatibility selection, the selected Frame projection is evaluated through the target's existing exact transform chain into the common equation space. Only that transient projection is transformed; target identity is not rewritten.

## Shared Revolute residual path

Both builders use the same compatibility and Frame residual constructor:

```text
AssemblyRevoluteJointEquationBuilder
AssemblyHierarchyRevoluteJointEquationBuilder
```

The existing residual remains numerically unchanged:

```text
direction_alignment          = dA - dB
axis_offset_mm               = cross(oB - oA, dA)
signed_seating_separation_mm = dot(sB - sA, nA)
twist_alignment_sine         = sin(phi - target)
twist_alignment_cosine       = cos(phi - target) - 1
```

The sine/cosine pair retains signed periodic twist without an `atan2` branch cut. No residual row, ordering rule, unit, or scaling rule changes in Block 40.

## Local and cross-hierarchy parity

Equivalent local and rooted `.seat` endpoints select the same `Frame / Frame` compatibility and reach the same residual constructor. Cross-hierarchy resolution continues to preserve exact occurrence paths and evaluate the complete inner-to-outer hierarchy transform chain.

Repeated occurrences remain distinct geometric endpoint identities even when they map to the same `ComponentTransformAuthority`.

## Persistence and file format

Block 40 introduces no persistence change:

- `AssemblyJoint` and `AssemblyHierarchyJoint` remain model-intent authority;
- existing Revolute target records and scalar coordinate/limit fields are unchanged;
- no capability, Frame descriptor, compatibility result, equation descriptor, residual, Jacobian, or solver state is serialized;
- schema and version markers remain unchanged.

## Verification

Focused tags:

```text
[geometry][assembly-joint-target-compatibility]
[geometry][assembly-cross-hierarchy-joint-target-compatibility]
```

Coverage proves:

- deterministic Revolute Frame/Frame selection;
- explicit Axis-only rejection with no hidden reference direction;
- compatibility is consumed before Frame projection;
- valid synthetic Frame-capable targets reach the shared residual constructor;
- local `.seat` targets retain `ComponentLocal` identity;
- rooted `.seat` targets retain `RootAssembly` identity;
- local and cross-hierarchy compatibility/results agree;
- existing local and cross-hierarchy Revolute numeric/motion tests remain unchanged.

## Block 41 outcome

Block 41 is implemented in `docs/assembly-joint-coordinate-model-mvp5.md`.

Both `AssemblyJoint` and `AssemblyHierarchyJoint` now own family-defined typed persistent coordinate slots with stable semantic roles, explicit Angular/Linear kinds, authored values, and optional typed limits.

Current Revolute public APIs are preserved through an explicit adaptation boundary. Block 42 JSON and Block 43 vector motion drives are implemented in their canonical joint documents. The next joint-family step is Block 44 Prismatic.

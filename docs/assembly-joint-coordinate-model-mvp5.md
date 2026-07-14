# Assembly Joint Coordinate Model MVP-5

Status: implemented as Block 41 of `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

This document is canonical for persistent family-defined joint coordinate slots, their unit boundary, local/cross-hierarchy parity, the Revolute scalar compatibility adapter, and the Block-42 persistence handoff.

## Scope

Block 41 replaces scalar coordinate authority inside `AssemblyJoint` and `AssemblyHierarchyJoint` with ordered `AssemblyJointCoordinateSlot` state. It adds no JSON field, vector drive, residual equation, target compatibility, transform authority, or joint family.

The two joint classes continue to own different endpoint scopes:

```text
AssemblyJoint          -> AssemblyConstraintTarget
AssemblyHierarchyJoint -> AssemblyHierarchyConstraintEndpoint
```

Only their coordinate model and validation rules are shared.

## Coordinate slot contract

Every slot owns:

```text
AssemblyJointCoordinateRole role
AssemblyJointCoordinateKind kind
Quantity value
optional<Quantity> lower_limit
optional<Quantity> upper_limit
```

Roles are semantic identities, not meanings inferred from a vector index. Their stable Core spellings are:

| Role | Spelling | Required kind |
|---|---|---|
| `Rotation` | `rotation` | Angular |
| `Translation` | `translation` | Linear |
| `TranslationU` | `translation_u` | Linear |
| `TranslationV` | `translation_v` | Linear |
| `RotationNormal` | `rotation_normal` | Angular |

Kinds have stable spellings `angular` and `linear`.

Slot order is family-defined and deterministic. Role identity remains explicit even when a family has only one coordinate. Duplicate, missing, extra, wrongly ordered, or wrongly typed roles are family validation failures rather than silently reinterpreted indexes.

## Unit boundary

Angular coordinates and bounds require `QuantityKind::AngleDeg`.

Linear joint coordinates require `QuantityKind::LinearDisplacementMm`, created with `Quantity::linear_displacement_mm`. This quantity is finite and signed. It is deliberately distinct from `LengthMm`, which represents positive construction dimensions, and from unitless doubles.

A slot rejects:

- a role/kind mismatch;
- a value or bound with the wrong physical quantity kind;
- lower bound greater than or equal to upper bound;
- a value outside either authored bound.

Lower and upper bounds are independently optional in the general slot model. A joint family may impose a stricter rule.

## Current Revolute family

Block 41 initially implemented only Revolute. Its exact slot signature is:

```text
0: rotation, Angular, AngleDeg value, required AngleDeg lower/upper bounds
```

The existing seed-limit restriction to the principal `[-180 deg, 180 deg]` interval remains unchanged. The authored value must remain inside both bounds.

No Prismatic, Cylindrical, Planar, or Ball/Spherical execution is introduced by this model. Their planned roles merely have stable identities available for later family blocks.

Block 44 subsequently activates Prismatic with exactly one bounded Linear `translation` slot;
its canonical execution contract is `docs/assembly-prismatic-joint-mvp5.md`.

## Compatibility adapter

Existing Revolute callers remain source-compatible:

- scalar `create(... lower_limit, upper_limit, coordinate)` constructs the canonical rotation slot;
- `with_coordinate(Quantity)` delegates to the rotation role update;
- `limits()` returns the legacy degree view derived at validated construction;
- `coordinate_deg()` reads the rotation slot value.

New callers may use the slot-vector `create` overload, `coordinate_slots()`, `find_coordinate_slot(role)`, and `with_coordinate_value(role, value)`.

Updates are immutable: the requested slot is rebuilt and the complete family signature is revalidated. A role absent from the family fails explicitly.

## Persistence boundary

Block 41 intentionally leaves assembly and Project JSON unchanged. Existing serializers continue through the Revolute compatibility accessors, so historical `limits` plus scalar `coordinate` records and schema/version markers remain stable. Existing readers still construct the same canonical rotation slot through the scalar adapter.

The in-memory slot vector is authoritative. The legacy scalar view exists only at the public/JSON compatibility boundary.

## Acceptance coverage

Focused tags:

```text
[core][assembly-joint-coordinate-model]
[core][assembly-cross-hierarchy-joint-coordinate-model]
```

Coverage proves stable role/kind spellings, signed linear units, optional bounds, range and unit failures, exact Revolute family shape, scalar compatibility views, immutable role updates, and identical local/cross-hierarchy coordinate semantics without merging endpoint identity scopes. Existing JSON roundtrip tests prove that the Block-40 scalar representation remains readable and writable.

## Block 42 outcome

Block 42 is implemented in `docs/assembly-joint-coordinate-json-mvp5.md`.

Slots now serialize additively for local and Project-level joints with canonical role spellings, typed units, deterministic family order, historical scalar Revolute loading, a dual compatibility writer, and explicit malformed-role failures.

Block 43 vector motion drives, Blocks 44–46 Prismatic/Cylindrical/Planar execution, and Block 47
passive Spherical with an empty family slot signature are implemented in their canonical documents.
Blocks 48–90 Part Construction through 3D Sketch JSON and semantic references are implemented;
Block 78 3D Sketch Geometry conversion is implemented; Block 79 connected PathCurve Core intent, JSON, and validation is implemented; Block 80 Sweep feature Core intent and JSON is implemented; Block 81 Basic Sweep Geometry is implemented; Block 82 3D path, twist, and guide-controlled Sweep is implemented; Block 83 Path-following Extrude and Extruded Cut is implemented; Block 84 ProfileSectionReference and Loft Core intent plus JSON is implemented; Block 85 Two-section Loft Geometry on arbitrary planes is implemented; Block 86 Multi-section Loft is implemented; Block 87 Guided and continuity-controlled Loft is implemented; Block 88 Surface feature Core intent and JSON is implemented; Block 89 Boundary and Fill Surface Geometry is implemented; Block 90 Trim and Extend Surface Geometry is implemented; Block 91 Stitch/Knit/Sew shell Geometry is next.

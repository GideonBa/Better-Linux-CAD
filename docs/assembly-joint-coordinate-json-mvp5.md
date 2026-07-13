# Assembly Joint Coordinate JSON MVP-5

Status: implemented as Block 42 of `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

This document is canonical for joint-coordinate slot persistence, canonical role/kind spellings and units, deterministic ordering, historical Revolute compatibility, local/cross-hierarchy parity, and the Block-43 motion handoff.

## Scope

Block 42 serializes the Block-41 `AssemblyJointCoordinateSlot` model for both:

```text
AssemblyDocument.assembly_joints[]
Project.cross_hierarchy_joints[]
```

It changes no endpoint scope, schema/version marker, motion equation, drive request, freshness rule, transform authority, or joint family.

## Canonical slot record

Every joint record may contain an ordered `coordinates` array:

```json
"coordinates": [
  {
    "role": "rotation",
    "kind": "angular",
    "value": {"unit": "deg", "value": 25.0},
    "lower_limit": {"unit": "deg", "value": -90.0},
    "upper_limit": {"unit": "deg", "value": 90.0}
  }
]
```

`lower_limit` and `upper_limit` are independently optional in the general record. Current Revolute family validation requires both.

Canonical spellings are:

| Role | Kind | Unit |
|---|---|---|
| `rotation` | `angular` | `deg` |
| `translation` | `linear` | `mm` |
| `translation_u` | `linear` | `mm` |
| `translation_v` | `linear` | `mm` |
| `rotation_normal` | `angular` | `deg` |

Angular values construct `AngleDeg`; linear values construct signed `LinearDisplacementMm`. Unit aliases, case variants, unitless numbers, and role/kind mismatches fail closed.

Array order is preserved and validated by the joint family. Role identity is never inferred from an array index.

## Writer contract

Current Revolute writers emit a dual additive record:

```text
canonical coordinates[]
+ historical limits
+ historical scalar coordinate
```

This lets new readers consume the general model while older readers can continue consuming the established Revolute fields. The slot array is emitted in family-defined order. Repeated serialization is deterministic.

Schema names and version `1` remain unchanged because the new field is additive and existing collections were already optional/additive.

## Reader compatibility matrix

Readers accept:

| Input shape | Result |
|---|---|
| `coordinates[]` only | accepted and family-validated |
| historical `limits` + scalar `coordinate` only | accepted through the Revolute adapter |
| both forms, exactly equivalent | accepted |
| both forms, conflicting | rejected |
| only one historical field | rejected |
| neither form | rejected |

Historical files therefore remain loadable. Slot-only records allow later joint families to persist without inventing scalar compatibility fields.

## Failure policy

Parsing fails before the joint enters its owning document/project for:

- non-array `coordinates`;
- unknown or duplicate roles;
- unknown kinds;
- missing, extra, wrongly ordered, or family-incompatible slots;
- wrong units or role/kind combinations;
- invalid optional-bound order or authored values outside bounds;
- incomplete or conflicting dual representations.

`AssemblyJoint::create` and `AssemblyHierarchyJoint::create` remain the final family validation authorities. Local component-scoped targets and exact rooted Project-level targets remain distinct.

## Acceptance coverage

Focused tags:

```text
[core][assembly-joint-coordinate-json]
[core][assembly-cross-hierarchy-joint-coordinate-json]
```

Coverage proves canonical writer shape, deterministic reserialization, slot-only loading, historical-only loading, exact dual agreement, malformed role/kind/unit/order failures, and identical compatibility policy across both endpoint scopes.

## Block 43 outcome

Block 43 is implemented in `docs/assembly-vector-joint-drive-mvp5.md`.

Coordinates are now driven by stable role; omitted selected and non-selected active slots hold authored values; result freshness includes complete slot and explicit drive state; and transforms plus exactly driven selected coordinates apply atomically. Blocks 44–46 Prismatic, Cylindrical, and Planar plus Block 47 passive Spherical are implemented in their canonical documents. Spherical persists the canonical empty `coordinates[]` signature. Blocks 48–54 Body identity, body-scoped recompute/inspection, and BodyBooleanFeature Core intent are implemented; Block 55 Body boolean Geometry and recompute is next.

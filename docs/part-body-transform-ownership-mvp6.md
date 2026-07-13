# BodyTransform and SketchOwnership Core Intent MVP-6

Status: implemented in Block 56.

## Scope

Block 56 adds persistent, Geometry-independent Body transform and Sketch association intent to
`PartDocument`. OCCT shapes, transformed workplane frames, and transformed construction geometry
are derived products implemented by Block 57; see `docs/part-body-transform-geometry-mvp6.md`.

## BodyTransform

`BodyTransform` has a stable `BodyTransformId`, one existing target `BodyId`, and one of three
frozen kinds:

```text
translate      translation_mm = finite Vector3
rotate         rotation_axis + finite angle_deg
uniform_scale  positive finite scale_factor + finite center
```

The first coordinate spaces are:

```text
world
body_local
sketch_local             requires an existing SketchId reference
construction_reference  requires an existing datum/construction frame reference
```

World and Body-local transforms forbid a coordinate reference. The construction-reference seed
accepts an existing datum plane/axis, construction line, or construction plane.

Rotation-axis identity is explicit and supports:

- finite explicit origin plus non-zero direction;
- existing `DatumAxisId`;
- existing `ConstructionLineId`; or
- validated `SemanticEdgeReference`.

Mirror, non-uniform scale, arbitrary authored matrices, and inferred coordinate references are not
part of Block 56.

## Stack and graph semantics

`PartDocument::body_transforms()` preserves insertion order. This is the authoritative transform
stack order, including interleaved transforms for multiple Bodies. Adding a transform advances the
target Body producer chain:

```text
previous Body producer -> BodyTransform -> body:<BodyId>
```

Coordinate references, rotation references, and applicable SketchOwnership records feed the
transform node. Cycles and graph-node identity collisions fail transactionally. Transform changes
participate in invalidation and recompute planning through
`mark_body_transform_changed(BodyTransformId)`.

## SketchOwnership

One Sketch may have at most one ownership record:

```text
SketchOwnership
  sketch
  owning_body
  association = drives_body | consumed_by_body | reference_only
```

Records are exposed in canonical lexicographic Sketch-ID order. Both referenced objects must exist.
The graph uses `sketch_ownership:<SketchId>` nodes. When a transform requests
`apply_to_owned_sketches`, matching ownership nodes feed that transform regardless of whether
ownership or transform intent was added first.

`apply_to_owned_construction_geometry` is persisted here; Block 57 derives its associated
workplane/projected-reference closure in `ShapeCache`. Body deletion fails closed while either
transform or ownership intent references it.

## JSON

Part JSON adds optional-on-read, always-emitted arrays:

```text
sketch_ownerships[]
body_transforms[]
```

Transform JSON preserves stack order and emits only the parameter fields for the selected kind.
Rotation-axis JSON preserves its selected identity family. Older files without either array load
with empty collections. Wrong containers, missing fields, invalid spellings/types, non-finite or
invalid numeric values, missing references, duplicate ownership, duplicate transform IDs, and graph
cycles fail closed through the normal validated construction APIs.

## Proof and handoff

Focused tags:

```text
[core][body-transform]
[core][sketch-ownership]
```

Block 57 now executes translation, rotation, and uniform scaling, moves explicitly requested owned
frames/geometry, re-resolves semantic/projected references, and proves that unowned Sketches remain
fixed. Its canonical Geometry contract is `docs/part-body-transform-geometry-mvp6.md`.

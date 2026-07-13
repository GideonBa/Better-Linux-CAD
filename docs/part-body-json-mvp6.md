# Part Body JSON MVP-6

Status: implemented in Block 49.

## Boundary

Block 49 persists the Block-48 Body collection as additive `PartDocument` model intent. It changes
neither Geometry execution nor the historical feature/final-shape behavior.

```json
{
  "bodies": [
    {
      "id": "body.base",
      "name": "Base",
      "kind": "solid",
      "visibility": "visible"
    },
    {
      "id": "body.skin",
      "name": "Skin",
      "kind": "surface",
      "visibility": "hidden"
    }
  ]
}
```

`id`, `name`, `kind`, and `visibility` are required strings. Supported canonical values are:

```text
kind        solid | surface
visibility  visible | hidden
```

## Determinism and compatibility

Serialization always emits `bodies`, including an empty array, in lexicographic `BodyId` order.
Deserialization accepts any authored array order and rebuilds the canonical order through the
normal `PartDocument::add_body` boundary. Re-serialization after load is deterministic.

Historical Part files without `bodies` load through the explicit zero-body compatibility path.
Neither an absent field nor an empty array creates an implicit Body from legacy features or the
single final `ShapeCache` product.

## Structural validation

Loading fails closed when:

- `bodies` is present but is not an array;
- an entry is not an object;
- a required field is absent or is not a string;
- an id or name is empty;
- kind or visibility has an unsupported spelling;
- a `BodyId` occurs more than once in the Part.

Blocks 50–51 add and persist feature output/target Body context, resolve it only against this
validated collection, and connect it to dependency/invalidation semantics.

## Proof

Focused acceptance tag:

```text
[core][part-body-json]
```

The tests cover the exact JSON values, Solid/Surface and Visible/Hidden roundtrip, canonical order,
byte-identical re-serialization, the absent-field compatibility path, zero hidden body generation,
and all structural failures listed above.

## Handoff

Blocks 50–53 feature Body-operation intent, dependencies, multi-body recompute, and checked
inspection are implemented. Block 54 now adds BodyBooleanFeature Core intent and JSON.

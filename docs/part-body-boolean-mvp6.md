# Part Body Boolean MVP-6

Status: implemented in Block 54.

## Boundary

Block 54 adds persistent, OCCT-free Boolean intent between existing Body identities:

```text
BodyBooleanFeature
  FeatureId id
  operation = Add | Subtract | Intersect
  target_body
  tool_bodies[]
  result_mode = ModifyTarget | NewBody
  optional produced_body
  keep_tool_bodies
```

Canonical spellings are `add`, `subtract`, `intersect`, `modify_target`, and `new_body`.
`keep_tool_bodies` explicitly freezes retention versus consumption intent for Block-55 Geometry.

## Validation and identity

The target and tools are semantic `BodyId` values. Tool lists must be non-empty, contain no empty
or duplicate IDs, and cannot contain the target. They are stored in lexicographic BodyId order.

`ModifyTarget` forbids `produced_body` and preserves the target identity. `NewBody` requires a
non-empty produced Body distinct from target and tools. Every referenced/effective result Body must
already be owned by the Part. Boolean Feature IDs share the existing Part-wide Feature ID scope.

## PartDocument graph behavior

For a distinct NewBody result:

```text
target body ----\
tool bodies -----+-> BodyBooleanFeature -> produced body
```

The produced Body must not already have a producer. ModifyTarget advances the target state using
the Block-51 in-place rule:

```text
previous target producer -> BodyBooleanFeature -> target body
tool bodies -------------> BodyBooleanFeature
```

Graph construction is transactional. Duplicate producers and real cycles reject the entire add.
Changes to target/tool producer chains propagate through the Boolean result. Target, tools, and
result Bodies cannot be removed while referenced. The keep/consume flag does not mutate Body
records in Core; Block 55 applies it to derived cached Geometry.

## JSON

The always-emitted, optional-on-read top-level collection is:

```json
"body_booleans": [
  {
    "id": "boolean.subtract_tool",
    "operation": "subtract",
    "target_body": "body.base",
    "tool_bodies": ["body.tool"],
    "result_mode": "modify_target",
    "keep_tool_bodies": false
  }
]
```

`produced_body` is emitted only for `new_body`. Historical documents without `body_booleans` load
an empty collection. Wrong container/field types, missing mandatory fields, unsupported spellings,
invalid combinations, missing Body references, duplicate IDs/producers, and cycles fail closed.

## Proof and deferral

Focused tag:

```text
[core][body-boolean]
```

Block 54 creates no OCCT Boolean, changes no ShapeCache entry, and consumes no Body Geometry.
Block 55 implements deterministic Add/Subtract/Intersect execution, keep/consume behavior, both
result modes, incremental recompute, and Geometry error context.

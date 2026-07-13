# Part Feature Body Operation MVP-6

Status: implemented in Block 50.

## Boundary

Block 50 adds explicit Core authority for the Body result of a Part feature without changing JSON,
dependency/invalidation graphs, Geometry execution, or historical feature behavior.

```text
FeatureBodyOperationMode
  NewBody
  Join
  Cut
  Intersect

FeatureBodyResultContext
  operation_mode
  optional target_body
  optional produced_body
```

Canonical Core spellings are `new_body`, `join`, `cut`, and `intersect`.

## Valid combinations

```text
NewBody
  target_body   = none
  produced_body = required

Join / Cut / Intersect
  target_body   = required
  produced_body = optional
```

For a modifying mode, an omitted `produced_body` explicitly preserves the target Body identity.
When present, `produced_body` names a separate documented result identity. Therefore every valid
context exposes one deterministic `effective_produced_body`.

Empty Body IDs, a NewBody target, a missing NewBody result, a missing modifying target, and unknown
operation modes fail closed during context creation.

## Feature and PartDocument integration

Existing feature constructors retain a null Body context. `Feature::with_body_result_context`
returns a new Feature value and leaves the original unchanged. When a Feature has an explicit
context, `PartDocument::add_feature` requires its target Body, when present, and its effective
produced Body to exist in the Block-49 Body collection.

Block 50 deliberately added no `body:` dependency nodes or edges. Block 51 now implements producer
ordering, consumer dependencies, invalidation, cycle rejection, and removal protection in
`docs/part-feature-body-dependency-mvp6.md`.

## Historical compatibility

Existing AdditiveExtrude and SubtractiveExtrude values retain no explicit Body result context.
Their JSON remains byte-compatible: `operation_mode`, `target_body`, and `produced_body` are not
written or read in Block 50, and historical zero-body Parts continue to load and recompute through
the existing single-final-shape path. No hidden Body identity is synthesized.

Block 51 now persists an explicit Block-50 context; historical null contexts remain fieldless.

## Proof

Focused acceptance tag:

```text
[core][feature-body-operation]
```

The tests cover all four modes and spellings, valid implicit/explicit result identity, invalid
combinations, immutable Feature attachment, PartDocument reference validation, absence of premature
Body graph nodes, and unchanged historical feature JSON.

## Handoff

Blocks 51–53 persistence, graph semantics, body-scoped Geometry execution, and checked inspection
are implemented. Block 54 now adds BodyBooleanFeature Core intent and JSON.

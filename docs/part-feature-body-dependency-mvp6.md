# Part Feature Body Dependency MVP-6

Status: implemented in Block 51.

## Boundary

Block 51 persists the Block-50 feature Body-result context and connects Body identities to the
existing dependency, invalidation, and recompute-plan models. Block 52 now executes that plan into
body-scoped ShapeCache results.

## Feature JSON

An explicit context adds fields directly to the existing Feature record:

```json
{
  "operation_mode": "new_body | join | cut | intersect",
  "target_body": "optional BodyId",
  "produced_body": "optional BodyId"
}
```

The Block-50 combination rules remain authoritative:

```text
NewBody
  target_body absent
  produced_body required

Join / Cut / Intersect
  target_body required
  produced_body absent -> preserve target identity
  produced_body present -> explicit result identity
```

Every present field is a string. Body references without `operation_mode`, unsupported spellings,
invalid combinations, missing referenced Bodies, and producer conflicts fail closed. Serialization
omits optional fields rather than writing `null`.

Historical Feature records with none of the three fields restore the exact null Body context. No
Body is inferred from Feature type, Feature order, or the historical final shape.

## Dependency graph

Body nodes use the canonical spelling:

```text
body:<BodyId>
```

For distinct input and result Bodies, the graph is:

```text
input sketch/profile/parameters -> feature
target body                     -> feature
feature                         -> produced body
produced body                   -> later consuming feature
```

One Body result identity has one current producer. A second NewBody or distinct-result producer is
rejected.

### In-place operations

Representing both `body -> modifier` and `modifier -> same body` would create an artificial cycle.
For Join/Cut/Intersect that preserves target identity, `PartDocument` advances the Body state:

```text
previous producer -> modifier -> body -> later consumer
```

Existing incoming producer edges of the Body are moved to the modifier before the modifier becomes
the current producer. The update is built on a graph copy; any real cycle rejects the Feature and
leaves Feature order, graph, and invalidation state unchanged.

## Invalidation, planning, and removal

`mark_feature_changed` propagates through produced Body state into later modifiers/consumers and
their result Bodies. `mark_body_changed` starts at `body:<BodyId>`. Dirty Body nodes participate in
the existing deterministic `RecomputePlan`; Block 52 treats them as derived result boundaries while
their producer/consumer Features update the body-scoped cache.

A Body referenced or produced by any Feature cannot be removed. Removing an unused Body removes
its graph node and invalidation entry atomically. This makes Block-48 dependent-removal behavior
effective rather than advisory.

## Proof

Focused acceptance tags:

```text
[core][feature-body-operation-json]
[core][part-body-dependency]
```

Tests cover explicit and implicit result JSON, historical defaults, malformed records, missing
references, producer chains, in-place state advancement, transitive invalidation, recompute plans,
removal cleanup/rejection, duplicate producers, and transactional cycle rejection.

## Handoff

Blocks 52–56 body-scoped recompute/inspection, Boolean execution, and BodyTransform/SketchOwnership
intent are implemented. Block 57 is the transform Geometry handoff.

# Part Body Identity MVP-6

Status: implemented in Block 48.

## Boundary

Block 48 establishes persistent body identity and `PartDocument` ownership in Core without changing
Geometry execution or the current JSON schema.

```text
BodyId

Body
  id
  name
  kind = Solid | Surface
  visibility = Visible | Hidden

PartDocument
  bodies[] in canonical lexicographic BodyId order
```

`Body` is immutable model intent. Creation requires a non-empty id and name. Visibility changes
replace the stored value through `Body::with_visibility`; they do not mutate a shared record in
place. Canonical Core spellings are `solid`, `surface`, `visible`, and `hidden`.

## Ownership and ordering

`BodyId` is unique within one `PartDocument`; equal body names are allowed. `add_body`, `find_body`,
`set_body_visibility`, and `remove_body` validate non-empty ids and fail explicitly for duplicates
or missing bodies. The collection is ordered lexicographically by `BodyId`, independent of insertion
order, so callers do not acquire accidental insertion-order authority.

Removal fails closed when the dependency graph reports direct dependents of the reserved
`body:<BodyId>` node. Block 51 now adds the actual feature/body dependency edges and makes this
removal boundary effective for both producers and consumers.

## Compatibility boundary

Existing parts keep their historical feature history and single final `ShapeCache` product. They
load and operate with zero implicit `Body` records: Block 48 neither infers a body from legacy
features nor changes recompute, Geometry, or STEP export behavior.

Body JSON was deliberately not part of Block 48. Block 49 now persists these records through the
additive `bodies[]` contract in `docs/part-body-json-mvp6.md`; historical files without that field
remain compatible and produce an empty body collection when loaded.

## Proof

Focused acceptance tag:

```text
[core][part-body]
```

The tests cover identity and enum spellings, immutable visibility updates, invalid values,
document-scoped uniqueness, deterministic ordering, lookup/removal behavior, and compatibility of
historical zero-body parts.

## Handoff

Block 49 Body JSON and structural validation is implemented. Block 50 now adds feature output-body
and operation-mode Core intent without Geometry behavior.

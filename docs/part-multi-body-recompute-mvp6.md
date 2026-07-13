# Part Multi-body Recompute MVP-6

Status: implemented in Block 52.

## Boundary

`ShapeCache` remains a derived Geometry product outside the OCCT-free Core, but now owns two
deterministic result spaces:

```text
FeatureId -> GeometryShape
BodyId    -> { source FeatureId, GeometryShape }
```

Body entries are ordered by `BodyId`, replace an existing entry without duplication, and can be
queried or removed independently. They are never serialized and no OCCT shape identity is written
back into `PartDocument`.

## Body-aware execution

An explicit `new_body` AdditiveExtrude stores its generated solid under the effective produced
Body. An explicit `cut` SubtractiveExtrude reads the target through `target_body` and writes the
result to the effective produced Body; omitted `produced_body` therefore updates the target Body in
place. Historical Features without Body context continue to use `target_feature` and the existing
feature/final-shape path.

The current narrow executor rejects other Feature/operation combinations rather than guessing
Boolean semantics. General Body-to-Body Join/Cut/Intersect is introduced by Blocks 54–55.

`execute_plan` follows the deterministic Block-51 plan, executes Feature nodes, treats Body nodes
as derived result boundaries, and changes only the Body entries produced by those Features.
Unmentioned Body entries remain intact. For explicit Body documents, `execute_plan` and
`execute_document` operate on a cache copy and commit it only after every requested Feature
succeeds, so missing target/produced Bodies, missing target shapes, or Geometry failures preserve
the caller's complete previous cache. Historical zero-Body documents retain their established
partial-result behavior on failure.

## Historical final-shape compatibility

The historical `final_shape()` query remains available for zero-Body documents and for an explicit
single Solid Body. A document with more than one Body, or a single Surface Body, clears this
compatibility result instead of selecting an arbitrary Body. Block 53 freezes the public checked
query/error and body-summary inspection contracts in `docs/part-multi-body-inspection-mvp6.md`.

## Proof

Focused acceptance tag:

```text
[geometry][multi-body-recompute]
```

Coverage proves deterministic Body ordering, replace/remove isolation, two simultaneous solid
results, selective recompute that preserves an unaffected Body, body-targeted cutting, missing
target-shape failure, and disabled final-shape compatibility for a multi-body document.

## Handoff

Blocks 53–56 inspection, Body Boolean execution, and BodyTransform/SketchOwnership intent are
implemented. Block 57 is the transform Geometry handoff.

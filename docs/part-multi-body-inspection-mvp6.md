# Part Multi-body Inspection MVP-6

Status: implemented in Block 53.

## Boundary

`BodyResultInspector` is the public headless bridge between persistent `PartDocument` Body intent
and derived Block-52 `ShapeCache` products. It exposes one deterministic inspection value:

```text
PartBodyInspection
  bodies[] ordered by BodyId
    body_id
    name
    kind
    visibility
    optional source_feature_id
    optional ShapeSummary
  solid_body_count
  surface_body_count
```

Bodies without computed Geometry remain visible in the result with absent source and summary.
Cached Body results that do not belong to the inspected Part fail structural validation. Inspection
does not mutate Core intent, invalidation state, cache values, or OCCT identity.

## Historical final-shape compatibility

Applications must use `resolve_compatible_final_shape(document, cache)` when consuming the
historical one-shape contract:

```text
zero Bodies       -> legal when the historical final shape exists
one Solid Body    -> legal when that Body has a cached shape
one Surface Body  -> error
multiple Bodies   -> ambiguity error
```

The raw `ShapeCache::final_shape()` pointer remains for source compatibility, but it is not an
application-level multi-body selection policy. CLI STEP export, Project Part export, and Assembly
Part shape-definition construction now use the checked compatibility query and therefore never
choose an arbitrary Body.

## Independent recompute proof

The headless acceptance path creates two Solid Bodies with distinct metadata and volume summaries,
changes only the width parameter of the first Body, executes the deterministic incremental plan,
and inspects again. The first volume changes while the second Body's summary remains unchanged.

Focused acceptance tag:

```text
[geometry][multi-body-inspection]
```

## Handoff

Blocks 54–56 Body Boolean intent/execution and BodyTransform/SketchOwnership intent are implemented.
Block 57 is the transform Geometry handoff.

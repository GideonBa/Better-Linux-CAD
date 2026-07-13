# ShapeCache Data Model

Status: geometry data model for computed feature/body shapes and compatibility final-shape tracking; generalized for multi-body recompute in Block 52.

This document describes the small `ShapeCache` used by the optional `blcad_geometry` target. The cache contains computed `GeometryShape` values. It remains outside `PartDocument`, which stores model intent only.

## Goal

The `ShapeCache` stores computed geometry as the result of the parametric model. It is not the source of truth. The source of truth remains the document, parameters, sketches, features, dependency graph, invalidation state, and recompute plan.

The current cache supports:

- storing computed feature shapes by `FeatureId`
- storing deterministic body results by `BodyId` with their source `FeatureId`
- replacing an existing feature shape when the same `FeatureId` is stored again
- removing a cached feature shape before dirty-feature recompute
- marking one final shape and its source feature
- clearing all cache contents
- rejecting empty IDs and empty shapes

## CMake target

The cache is built only with the geometry preset:

```bash
cmake --preset dev-geometry
cmake --build --preset dev-geometry
ctest --preset dev-geometry
```

The default preset `dev` remains core-only.

## Public interface

Header:

```text
include/blcad/geometry/shape_cache.hpp
```

Current types:

- `CachedFeatureShape`
- `CachedBodyShape`
- `ShapeCache`

`ShapeCache` uses:

- `ShapeCacheId`
- `FeatureId`
- `BodyId`
- `GeometryShape`
- `Result<T>` and `Error`

## Behavior

A cache is created with a non-empty `ShapeCacheId`:

```text
ShapeCache::create(shape_cache_id)
```

Feature shapes are stored by feature ID:

```text
store_feature_shape(feature.base_extrude, geometry_shape)
```

Body results are stored, replaced, queried, and removed independently:

```text
store_body_shape(body.base, feature.top_hole_cut, geometry_shape)
find_body_result(body.base)
remove_body_shape(body.base)
```

The `body_shapes()` view is always ordered lexicographically by `BodyId`.

The final shape is connected to the feature that currently produced it:

```text
set_final_shape(feature.top_hole_cut, geometry_shape)
```

Dirty-feature recompute can remove a stale shape before re-executing the feature:

```text
remove_feature_shape(feature.top_hole_cut)
```

If the removed feature is also the current final-shape source, the final-shape marker is cleared. This prevents stale final geometry from surviving a failed incremental recompute.

`clear()` removes all feature shapes, body shapes, and the final shape.

## Incremental recompute rule

For explicit Body documents, `GeometryRecomputeExecutor::execute_plan` works on a cache copy,
removes cached shapes for executed dirty Features there, and commits only after the complete plan
succeeds. Body entries outside the plan are preserved. Historical zero-Body documents keep the
original stale-feature removal and partial-result behavior.

This matters for validation failures:

1. A previous full recompute creates `feature.top_hole_cut`.
2. A parameter update makes the top-face hole invalid.
3. The incremental plan reaches `feature.top_hole_cut`.
4. The old cut shape is removed before recompute.
5. Bounds validation fails.
6. No stale cut shape remains in the cache.

If an earlier dirty feature such as `feature.base_extrude` was recomputed successfully, its shape can remain as the final valid shape.

## Validation

Current rules:

- cache ID must not be empty
- feature ID must not be empty for store, set-final, and remove operations
- empty `GeometryShape` handles must not be stored
- storing the same feature ID again replaces the existing shape instead of creating a duplicate entry
- removing a missing feature ID succeeds with `false`

## Test coverage

Current tests check:

- the cache starts empty with a stable ID
- empty cache IDs are rejected
- feature shapes can be stored and found again
- a stored rectangle-extrusion shape contains exactly one solid
- repeated storage of the same feature ID does not create a duplicate entry
- final shape and source feature are stored
- feature shapes can be removed
- removing the final source feature clears the final-shape marker
- removing a missing feature returns `false`
- `clear()` clears feature and final shapes
- empty feature IDs and empty shapes are rejected
- `GeometryRecomputeExecutor` removes stale dirty feature shapes before incremental recompute

## Deliberate limitation

Not included yet:

- integration into `PartDocument` (deliberately remains outside Core)
- persistent cache metadata
- partial cache invalidation records
- cache serialization
- GUI cache visualization

The cache remains a computed result container, not part of the model-intent layer.

# MVP 1 ShapeCache Data Model

Status: optional geometry data model, usable by additive recompute.

This document describes the first small `ShapeCache` for MVP 1. The cache lives in the optional target `blcad_geometry` because it contains `GeometryShape`. This keeps the core free of OCCT headers and geometry linking.

## Goal

The `ShapeCache` stores computed geometry as the result of the parametric model. It is not the source of truth. The source of truth remains the document, parameters, sketches, features, dependency graph, and recompute plan.

The current cache supports the first additive recompute step:

- store computed feature shapes by `FeatureId`
- mark one final shape and its source feature
- clear cache contents
- reject empty IDs and empty shapes

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
- `ShapeCache`

`ShapeCache` uses:

- `ShapeCacheId`
- `FeatureId`
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

The final shape is connected to the feature that currently produced it:

```text
set_final_shape(feature.base_extrude, geometry_shape)
```

`clear()` removes all feature shapes and the final shape.

## Validation

Current rules:

- cache ID must not be empty
- feature ID must not be empty
- empty `GeometryShape` handles must not be stored
- storing the same feature ID again replaces the existing shape instead of creating a duplicate entry

## Test coverage

Current tests check:

- the cache starts empty with a stable ID
- empty cache IDs are rejected
- feature shapes can be stored and found again
- a stored rectangle-extrusion shape contains exactly one solid
- repeated storage of the same feature ID does not create a duplicate entry
- final shape and source feature are stored
- `clear()` clears feature and final shapes
- empty feature IDs and empty shapes are rejected
- `GeometryRecomputeExecutor` can store an executed `AdditiveExtrude` in the cache

## Deliberate limitation

Not included yet:

- integration into `PartDocument`
- `SubtractiveExtrude` and Boolean cut
- STEP export
- GUI

## Next useful step

The next step should create the centered cut for `SubtractiveExtrude`:

1. read the target shape from the `ShapeCache`
2. resolve the circle profile and diameter parameter from `PartDocument`
3. execute the OCCT Boolean cut
4. store the result in the `ShapeCache` as the new final shape
5. continue not to build a general solver or GUI

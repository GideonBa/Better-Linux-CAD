# MVP 1 Recompute Execution: AdditiveExtrude

Status: optional geometry execution for one rectangle profile.

This document describes the first executed recompute step for MVP 1. The code lives in the optional target `blcad_geometry`, so it remains outside the pure core target.

## Goal

This step connects the existing data models for the first time:

- `PartDocument`
- `RecomputePlan`
- `RectangleProfile`
- `AdditiveExtrude`
- `RectangleExtrusionAdapter`
- `ShapeCache`

The execution should prove that a `dirty` feature node from a recompute plan can be translated into computed OCCT geometry and stored in the cache.

## CMake target

Execution is built only with the geometry preset:

```bash
cmake --preset dev-geometry
cmake --build --preset dev-geometry
ctest --preset dev-geometry
```

The default preset `dev` remains core-only.

## Public interface

Header:

```text
include/blcad/geometry/recompute_executor.hpp
```

Current types:

- `GeometryRecomputeSummary`
- `GeometryRecomputeExecutor`

Current operations:

```text
execute_additive_extrude(document, feature_id, shape_cache)
execute_plan(document, recompute_plan, shape_cache)
```

## Execution model

`execute_additive_extrude` works in this order:

1. Validate the feature ID.
2. Find the feature in the `PartDocument`.
3. The feature type must be `AdditiveExtrude`.
4. Find the input sketch.
5. The sketch must contain exactly one rectangle profile and no other profiles.
6. Resolve width, height, and length parameters.
7. Call `RectangleExtrusionAdapter`.
8. Store the result in `ShapeCache` as a feature shape and final shape.

`execute_plan` iterates over the steps of a `RecomputePlan`. Non-feature nodes, such as sketch nodes, are skipped. Feature nodes are currently executed only if they are `AdditiveExtrude`.

Before execution, the plan is checked for unsupported feature types. This prevents partially writing the `ShapeCache` if a later node in the same plan is a not-yet-implemented `SubtractiveExtrude`.

## Validation

Current error cases:

- empty feature ID
- feature does not exist in the document
- feature is not an `AdditiveExtrude`
- input sketch does not exist
- sketch does not contain exactly one rectangle profile
- width, height, or length parameter is missing
- geometry adapter reports an error
- ShapeCache rejects the result

Unsupported feature types in a plan return a geometry error. This prevents silently ignoring `SubtractiveExtrude` nodes.

## Test coverage

Current tests check:

- a single `AdditiveExtrude` creates a non-empty shape in the cache
- `execute_plan` skips sketch nodes and executes the additive feature node
- missing feature IDs are reported
- non-rectangular additive sketches are rejected
- unsupported feature types in the plan create an error without partially writing the cache

## Deliberate limitation

Not included yet:

- `SubtractiveExtrude`
- Boolean cut
- circle profiles as holes
- multiple rectangle profiles in one sketch
- ShapeCache integration directly into `PartDocument`
- automatic `mark_all_clean()` after successful recompute
- STEP export
- GUI

## Next useful step

The next technical step should be the centered cut for `SubtractiveExtrude`:

1. resolve diameter and position from a circle profile
2. read the target shape from the `ShapeCache`
3. execute the OCCT Boolean cut
4. store the new final shape in the `ShapeCache`
5. continue not to build a general solver or GUI

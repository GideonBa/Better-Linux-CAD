# MVP 1 Recompute Execution: SubtractiveExtrude

Status: optional geometry execution for a centered cut.

This document describes the second executed recompute step for MVP 1. The code lives in the optional target `blcad_geometry`, so it remains outside the pure core target.

## Goal

This step builds on additive execution and connects the Boolean cut to the same path:

- `PartDocument`
- `RecomputePlan`
- `CircleProfile`
- `SubtractiveExtrude`
- `CircularCutAdapter`
- `ShapeCache`

The execution should prove that a previously computed base body can be read from the `ShapeCache`, cut with a centered hole, and written back as the new final shape.

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

Current operations:

```text
execute_additive_extrude(document, feature_id, shape_cache)
execute_subtractive_extrude(document, feature_id, shape_cache)
execute_plan(document, recompute_plan, shape_cache)
```

## Execution model

`execute_subtractive_extrude` works in this order:

1. Validate the feature ID.
2. Find the feature in the `PartDocument`.
3. The feature type must be `SubtractiveExtrude`.
4. Find the input sketch.
5. The sketch must contain exactly one circle profile and no other profiles.
6. Resolve the diameter parameter.
7. Read the target shape from the `ShapeCache` through `target_feature`.
8. Call `CircularCutAdapter`.
9. Store the result in `ShapeCache` as a feature shape and final shape.

`execute_plan` iterates over the steps of a `RecomputePlan`. Non-feature nodes, such as sketch nodes, are skipped. Feature nodes are dispatched by their type: `AdditiveExtrude` goes to `execute_additive_extrude`, and `SubtractiveExtrude` goes to `execute_subtractive_extrude`.

The topological order of the `RecomputePlan` ensures that an `AdditiveExtrude` runs before the dependent `SubtractiveExtrude`. As a result, the cut finds its target body already present in the `ShapeCache`.

## Dependency on the target body

The cut needs the shape of its `target_feature` in the `ShapeCache`. In a partial plan that only affects `hole_diameter`, the base body is not recomputed. It must then still exist in the cache from an earlier full recompute. If the target shape is missing, the executor reports a geometry error and writes no final shape.

## Validation

Current error cases:

- empty feature ID
- feature does not exist in the document
- feature is not a `SubtractiveExtrude`
- input sketch does not exist
- sketch does not contain exactly one circle profile
- diameter parameter is missing
- target shape is missing in the `ShapeCache`
- geometry adapter reports an error
- ShapeCache rejects the result

## Test coverage

Current tests check:

- a `SubtractiveExtrude` cuts a hole into a previously cached base body and reduces the volume
- `execute_plan` from a `width` plan first executes the additive feature node and then the subtractive feature node
- a missing target shape in the cache is reported
- non-circular subtractive sketches are rejected

## Deliberate limitation

Not included yet:

- multiple holes or patterns
- limited cut depth instead of `through_all`
- ShapeCache integration directly into `PartDocument`
- automatic `mark_all_clean()` after successful recompute
- STEP export
- GUI

## Next useful step

The next technical step should be STEP export of the final shape from the `ShapeCache`:

1. read the final shape from the `ShapeCache`
2. export through a small OCCT STEP adapter
3. receive the export path from the caller
4. report export errors as error objects
5. continue not to build a general solver or GUI

# MVP 2 Incremental Recompute Through Derived Workplanes

Status: incremental recompute is verified through the derived top-face workplane dependency path.

This document describes the incremental recompute behavior for a model where a sketch is placed on a workplane derived from `feature.base_extrude.top`.

## Goal

The goal is to prove that this dependency path is not only valid during full recompute, but also during parameter-driven incremental recompute:

```text
part.width
  -> sketch.base
  -> feature.base_extrude
  -> workplane.base_top
  -> sketch.top_hole
  -> feature.top_hole_cut
```

A source-dimension change must affect both:

- the additive base feature
- the downstream cut whose sketch is attached to the derived top-face workplane

## Recompute plan behavior

After a clean full recompute, the document can be marked clean:

```text
document.mark_all_clean()
```

Then a base-dimension update such as:

```text
part.width = 140 mm
```

marks these nodes as affected:

```text
part.width
sketch.base
feature.base_extrude
workplane.base_top
sketch.top_hole
feature.top_hole_cut
```

The recompute plan includes dirty graph nodes in topological order. It includes both feature and non-feature nodes:

```text
sketch.base
feature.base_extrude
workplane.base_top
sketch.top_hole
feature.top_hole_cut
```

`GeometryRecomputeExecutor::execute_plan` skips non-feature nodes during execution, but preserves their ordering role in the plan.

The executed feature count for this update is therefore:

```text
2
```

Those two executed features are:

```text
feature.base_extrude
feature.top_hole_cut
```

## Cache behavior

Before executing a dirty feature during incremental recompute, the executor removes any existing cached shape for that feature from `ShapeCache`.

This prevents stale feature geometry from surviving a failed recompute.

Example:

1. Full recompute creates a valid base and cut.
2. The document is marked clean.
3. `part.width` is reduced.
4. The base is recomputed successfully.
5. The cut becomes invalid because the circle no longer fits inside the smaller top face.
6. The old cut shape is removed before the failed cut recompute.
7. The cache keeps the recomputed base shape as the final valid shape.
8. No stale `feature.top_hole_cut` shape remains cached.

## Bounds failure after shrinking

For the reference model, a hole at:

```text
center = (50, 0)
diameter = 20
```

is valid when the width is `120 mm` because the local X range is:

```text
[-60, 60]
```

After shrinking `part.width` to `100 mm`, the local X range becomes:

```text
[-50, 50]
```

The same circle now touches:

```text
[40, 60]
```

This exceeds the face bounds. Recompute returns:

```text
ErrorCategory::Validation
object_id = profile.top_hole
message = circle profile must lie fully inside resolved workplane bounds
```

## Test coverage

Geometry tests cover:

- full recompute before incremental updates
- `part.width` update through `PartDocument::set_parameter_value`
- affected node set including base sketch, base feature, derived workplane, dependent sketch, and cut feature
- recompute-plan ordering through the derived workplane path
- skipping non-feature plan nodes during geometry execution
- successful incremental recompute after widening the base rectangle
- correct updated base volume after widening
- correct updated final volume after widening
- failed incremental recompute after shrinking the base rectangle too far
- removal of stale cut geometry after failed recompute
- preserving the recomputed base shape as the final valid cached shape after cut failure

## Deliberate limitation

Not included yet:

- dirty-node pruning from the generic recompute plan
- persistent cache invalidation metadata
- broad topological naming
- side-face workplanes
- GUI-driven update commands

This step verifies the core architecture: derived workplanes participate in the dependency graph as non-feature nodes, while the geometry executor recomputes only affected feature nodes.

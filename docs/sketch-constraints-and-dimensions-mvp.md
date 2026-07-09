# Sketch constraints and driving dimensions seed

This document describes the first non-solver sketch constraint and driving-dimension seed.

The implemented scope is intentionally narrow. It records stable model intent, validates references, persists the records to JSON, connects dimension parameters to invalidation, and evaluates a deterministic subset for closed-profile recompute. It does not implement a full sketch constraint solver.

## Implemented model intent

The core model now has two new sketch-level record families.

`SketchGeometricConstraint` stores explicit geometric constraint intent on explicit line segments:

- `fixed`
- `horizontal`
- `vertical`
- `parallel`
- `perpendicular`
- `equal_length`

`SketchDrivingDimension` stores first driving distance dimensions backed by existing length parameters:

- `horizontal_distance`
- `vertical_distance`
- `aligned_distance`
- `point_to_point_distance`

Dimensions use `SketchDimensionId` and reference two explicit line endpoint targets. The dimension's length value is read from a `ParameterId` already stored in `PartDocument`.

## Validation behavior

The seed validates only explicit deterministic references:

- geometric constraints target explicit line segments or, for `fixed`, explicit line endpoints
- two-line constraints require two distinct explicit line segment targets
- driving dimensions require two distinct explicit line endpoint targets
- `PartDocument::add_sketch` verifies that each driving dimension parameter exists
- dimension parameters are connected to the owning sketch in the dependency graph

The seed does not diagnose over-constrained or under-constrained sketches. It also does not solve arbitrary constraint systems.

## JSON format

Sketches now serialize `geometric_constraints` and `driving_dimensions` arrays:

```json
{
  "geometric_constraints": [
    {
      "id": "constraint.bottom.horizontal",
      "kind": "horizontal",
      "first": {"kind": "line_segment", "entity": "line.bottom"}
    },
    {
      "id": "constraint.width.equal",
      "kind": "equal_length",
      "first": {"kind": "line_segment", "entity": "line.bottom"},
      "second": {"kind": "line_segment", "entity": "line.top"}
    }
  ],
  "driving_dimensions": [
    {
      "id": "dim.width.bottom",
      "kind": "horizontal_distance",
      "first": {"kind": "line_segment_start", "entity": "line.bottom"},
      "second": {"kind": "line_segment_end", "entity": "line.bottom"},
      "parameter": "part.width"
    }
  ]
}
```

These records are loaded after projected-reference constraints and before profiles.

## Dependency and invalidation behavior

Driving dimensions are parameter-backed. For each dimension, `PartDocument` adds this dependency:

```text
<dimension-parameter> -> <sketch-id>
```

The existing sketch-to-feature dependency then propagates changes to dependent features:

```text
part.width -> sketch.dimensioned -> feature.dimensioned
```

Geometric constraints currently have no parameter dependencies, so they are validated and persisted but do not add graph edges of their own.

## Geometry behavior

The optional geometry layer has `DimensionDrivenProfileResolver`.

It resolves a deterministic closed profile where line endpoints are driven by dimensions that target the start and end of the same explicit line segment. For the first seed:

- `horizontal_distance` moves the next profile vertex along local X using the original line direction sign
- `vertical_distance` moves the next profile vertex along local Y using the original line direction sign
- `aligned_distance` and `point_to_point_distance` move along the original line direction
- the resolved loop must close back to the first vertex

`GeometryRecomputeExecutor` uses this resolver for closed-profile additive extrudes and subtractive through-all cuts when the sketch has driving dimensions and does not use reference-generated helper lines.

## Test coverage

The tests cover:

- JSON roundtrip for geometric constraints and driving dimensions
- dependency propagation from a dimension parameter to sketch and feature
- deterministic local vertex resolution from dimension parameters
- recomputing an additive extrude from a dimension-driven closed profile

## Deliberate limitations

This block does not include a general constraint solver.

It does not support driven dimensions, constraint-status diagnosis, automatic constraint inference, or solver conflict reporting.

It does not automatically detect sketch regions from unordered curves.

It does not support arcs, splines, trimming, tangent constraints, GUI editing, 3D sketches, sweep, loft, or surfacing.

# Reference-generated profile helpers

This document describes the implemented first-class path for turning reference-driven sketch helper geometry into deterministic profile input.

The goal is to keep projected-reference helper geometry explicit and reproducible without adding a full sketch solver or automatic region detector yet.

## Implemented scope

The core model has `ReferenceGeneratedLine` as an explicit helper-line intent record.

A `ReferenceGeneratedLine` stores:

- a helper line entity id
- the coincident projected-point constraint that defines the start endpoint
- the coincident projected-point constraint that defines the end endpoint
- an optional projected-line direction constraint

`Sketch` now stores `reference_generated_lines` alongside explicit line segments, projected points, projected lines, constraints, and profiles.

A `ClosedProfile` can explicitly reference reference-generated helper line ids. This makes the generated helper geometry first-class model intent rather than an external geometry-resolver input.

The core validation deliberately stays narrow:

- a reference-generated line id must be unique within the sketch
- constraints may target reference-generated helper line ids
- endpoint constraints must be `coincident_to_projected_point` constraints on the helper line start and end
- optional direction constraints must be `parallel_to_projected_line` or `collinear_with_projected_line` constraints on the helper line
- a closed profile may consume reference-generated helper line ids

## JSON format

Sketch JSON stores helper-line records in a `reference_generated_lines` array:

```json
{
  "reference_generated_lines": [
    {
      "id": "helper.ab",
      "start_constraint": "constraint.ab.start",
      "end_constraint": "constraint.ab.end",
      "direction_constraint": "constraint.ab.parallel_x"
    },
    {
      "id": "helper.bc",
      "start_constraint": "constraint.bc.start",
      "end_constraint": "constraint.bc.end"
    }
  ]
}
```

`reference_generated_lines` are loaded before `constraints`. This allows constraints to target the helper-line ids during sketch validation. The endpoint and direction constraint ids are then validated when the complete sketch is added to `PartDocument`.

## Geometry resolver

The optional geometry layer has `ReferenceGeneratedProfileResolver`.

It resolves one `ReferenceGeneratedLine` by using `ReferenceDrivenSketchHelper` to evaluate the start and end projected-point constraints. If a direction constraint is present, the resolver verifies that the resolved helper line direction is consistent with the resolved projected-line constraint.

It can also resolve a `ClosedProfile` into ordered local vertices while allowing the profile line ids to be supplied by reference-generated helper-line records.

The resolver validates that the resolved profile lines are ordered and connected before returning profile vertices.

## Recompute integration

`GeometryRecomputeExecutor` now checks closed-profile sketches for reference-generated helper-line usage. If a closed profile uses those helper ids, recompute resolves the local profile vertices through `ReferenceGeneratedProfileResolver` before mapping them through the resolved sketch workplane.

This path is used by both:

- additive extrude from one reference-generated closed profile
- subtractive through-all cut from one reference-generated closed profile

## Dependency and invalidation behavior

`PartDocument` adds dependency graph nodes for helper lines using this node shape:

```text
<sketch-id>.reference_generated_line.<helper-line-id>
```

Projected construction points, projected construction lines, semantic vertices, and semantic edges used by endpoint or direction constraints point to the helper-line node. The helper-line node then points to the owning sketch, and feature dependencies continue from sketch to feature.

This gives the intended dirty path:

```text
parameter/source reference
  -> construction point/semantic reference
  -> sketch.reference_generated_line.helper
  -> sketch
  -> dependent feature
```

## Test coverage

The implemented tests cover:

- JSON roundtrip for `reference_generated_lines`
- invalidation propagation from a parameter-driven construction point through helper-line nodes to sketch and feature
- resolving deterministic helper profile vertices from projected-point endpoint constraints
- validating projected-line direction consistency
- rejecting a helper line whose endpoint-derived direction does not match the required projected-line direction
- additive extrude recompute from a reference-generated helper profile
- subtractive through-all cut recompute from a reference-generated helper profile

## Test workflow

Use the workflow presets when checking this area locally:

```bash
cmake --workflow --preset dev-build-test
cmake --workflow --preset dev-geometry-build-test
```

The geometry workflow is required for recompute and OCCT-backed tests.

## Deliberate limitations

This block does not add a full sketch constraint solver.

It does not add automatic region detection from unordered projected references.

It does not support mixed explicit-line and reference-generated-line closed profiles in the same `ClosedProfile` yet.

It does not implement arcs, splines, trim/extend, tangent constraints, driven dimensions, GUI editing, arbitrary generated topology, 3D sketches, sweep, loft, or surfacing.

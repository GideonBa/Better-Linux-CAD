# Projected Sketch Reference Geometry

This document describes the implemented seed for sketch-local projected reference geometry.

The goal is to let a sketch consume stable model-intent references to already defined construction geometry and semantic generated geometry without storing raw OCCT topology IDs and without introducing a full sketch constraint solver yet.

## Implemented scope

The core sketch model now has two reference-entity types:

- `ProjectedSketchPoint`
- `ProjectedSketchLine`

Both use `SketchEntityId` as sketch-local reference entity IDs. This keeps projected reference entities in the same sketch-local identity namespace as regular sketch entities while keeping them separate from profile-owning `LineSegment` geometry.

`ProjectedSketchPoint` can reference:

- a `ConstructionPointId`
- a `SemanticVertexReference`

`ProjectedSketchLine` can reference:

- a `ConstructionLineId`
- a `SemanticEdgeReference`

These references are model intent. They do not store raw OCCT faces, edges, vertices, or shape sub-indexes.

## JSON format

Sketch JSON now supports optional `projected_points` and `projected_lines` arrays.

A projected construction point reference has the form:

```json
{
  "id": "ref.point.top_front_mid",
  "source": "construction_point",
  "construction_point": "point.top_front_mid"
}
```

A projected semantic vertex reference has the form:

```json
{
  "id": "ref.vertex.top_front_right",
  "source": "semantic_vertex",
  "semantic_vertex": {
    "source_feature": "feature.base",
    "vertex": "top_front_right"
  }
}
```

A projected construction line reference has the form:

```json
{
  "id": "ref.line.top_front_axis",
  "source": "construction_line",
  "construction_line": "line.top_front_axis"
}
```

A projected semantic edge reference has the form:

```json
{
  "id": "ref.edge.top_front",
  "source": "semantic_edge",
  "semantic_edge": {
    "source_feature": "feature.base",
    "edge": "top_front"
  }
}
```

The checked-in example `examples/projected_sketch_references.blcad.json` shows all four reference forms in one model.

## Geometry projection

The optional geometry layer now contains `SketchReferenceProjector`.

It resolves the sketch workplane through `WorkplaneResolver`, then resolves the referenced source through the appropriate resolver:

- construction point references use `ConstructionPointResolver`
- construction line references use `ConstructionLineResolver`
- semantic vertex references use `SemanticReferenceEvaluator::resolve_vertex`
- semantic edge references use `SemanticReferenceEvaluator::resolve_edge`

A resolved 3D point is projected into the sketch-local workplane frame as a 2D point.

A resolved 3D line is projected as a local point plus a normalized 2D direction. The line is accepted only if its anchor point lies on the sketch workplane and its 3D direction is parallel to the sketch workplane.

A semantic generated edge is accepted only if its evaluated edge direction lies in the sketch workplane. Its projected local line uses the evaluated edge start point as the local line anchor and the edge vector as the local direction seed.

## Out-of-plane validation

Projection is intentionally strict. A point that is not on the sketch workplane returns a validation error:

```text
projected sketch reference must lie on the sketch workplane
```

A line that is not fully contained in the sketch workplane returns a validation error:

```text
projected sketch line must lie on the sketch workplane
```

This avoids silently dropping dimensions or projecting arbitrary 3D geometry into an unrelated 2D sketch.

## Tests

The implemented test coverage includes:

- core storage and source-type tests for projected point and line references
- JSON roundtrip tests for construction-point, construction-line, semantic-vertex, and semantic-edge projected references
- geometry projection tests for semantic generated vertices and edges on a top-face sketch
- geometry projection tests for construction points and construction lines on a top-face sketch
- out-of-plane rejection tests

## Deliberate limitations

This block does not implement a full sketch solver. Projected references are not yet automatically consumed by `ClosedProfile` or by dimension/constraint solving.

The currently implemented semantic generated edge and vertex evaluation still supports only the controlled rectangular additive extrude topology. Arbitrary generated topology remains deferred.

Projected line references are represented as local point-plus-direction helper geometry, not as bounded profile curves. Turning projected references into selectable profile edges or constraint targets is a separate later sketcher block.

GUI manipulators, live selection, associative sketch dimensions, automatic region detection, and solver-backed projected constraints remain deferred.

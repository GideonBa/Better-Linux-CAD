# Construction Geometry MVP

Status: explicit construction geometry, relation-driven construction geometry, semantic generated edge/vertex references, evaluated rectangular-extrude edge/vertex references, deterministic construction-point evaluation, deterministic construction-line evaluation, and the first chained construction relations are implemented in the headless core/geometry layers.

This document records the construction-geometry layer for BLCAD. The implementation supports explicit construction points, lines, and planes; relation-driven construction points, lines, and planes; and semantic references to generated edges and vertices. Semantic generated references are saved as model intent by source feature plus semantic enum, not as raw OCCT topology handles.

## Goal

Construction geometry gives users stable helper geometry that is part of model intent.

Implemented user paths:

```text
Explicit ConstructionPlane
  -> Sketch on ConstructionPlane
  -> Feature using that sketch
```

```text
ConstructionRelation
  -> relation-driven ConstructionPlane
  -> Sketch on ConstructionPlane
  -> Feature using that sketch
```

```text
Feature semantic edge/vertex reference
  -> evaluated rectangular-extrude edge/vertex
  -> relation-driven ConstructionPoint or ConstructionLine
```

## Implemented scope

The implementation contains:

- `ConstructionPointId`, `ConstructionLineId`, `ConstructionPlaneId`, and `ConstructionRelationId`
- `ConstructionPoint`, `ConstructionLine`, `ConstructionPlane`, and `ConstructionRelation`
- `ConstructionPointKind`, `ConstructionLineKind`, and `ConstructionPlaneKind`
- `SemanticFaceReference`, `SemanticEdgeReference`, and `SemanticVertexReference`
- explicit 3D placement for construction points, lines, and planes
- relation-driven construction points from `PointOnGeneratedVertex` and `PointOnGeneratedEdge`
- relation-driven construction planes from `PlaneOffsetFromPlane`, `PlaneThroughThreePoints`, and `PlaneParallelToPlaneThroughPoint`
- relation-driven construction lines from `LineThroughTwoPoints`, `LineParallelToLineThroughPoint`, and `LineParallelToGeneratedEdgeThroughPoint`
- model-intent relation definitions for `PointOnPlane`, `PointOnLine`, and `LineOnPlane`
- validation for generated edge/vertex references against additive-extrude source features without storing raw OCCT topology IDs
- `SemanticReferenceEvaluator` for rectangular additive extrude generated edges and vertices
- `ConstructionPointResolver` for explicit points, generated-vertex points, and generated-edge midpoint points
- `ConstructionLineResolver` for explicit lines, two-point lines, line-parallel-through-point lines, and generated-edge-parallel-through-point lines
- collinearity validation for `PlaneThroughThreePoints`
- optional `parameter_dependencies` on explicit construction geometry
- parameter dependency capture for offset-plane relations
- `PartDocument` storage and validation for construction geometry and relation references
- dependency graph nodes for construction geometry
- dependency graph edges from parameters to construction geometry
- dependency graph edges from relation references to relation-driven construction geometry
- dependency graph edges from generated-edge/generated-vertex relations to their source feature where those relations are used by relation-driven construction objects
- JSON serialization/deserialization of explicit, relation-driven, chained, and semantic-reference construction geometry
- JSON roundtrip tests for chained construction relations and semantic generated references
- JSON-backed example model `examples/generated_semantic_references.blcad.json`
- `WorkplaneResolver` support for explicit construction planes
- `WorkplaneResolver` support for offset construction planes
- `WorkplaneResolver` support for construction planes through three points
- `WorkplaneResolver` support for planes parallel to another plane through a construction point
- geometry recompute tests for closed profiles sketched on explicit and offset construction planes
- checked-in example model `examples/construction_plane_prism.blcad.json`

## Semantic generated references

Generated references are stable semantic model-intent references.

```text
SemanticEdgeReference
  source_feature = feature.base_extrude
  edge = top_front | top_back | top_right | ...
```

```text
SemanticVertexReference
  source_feature = feature.base_extrude
  vertex = top_front_right | bottom_back_left | ...
```

The reference node identity is derived from the source feature and semantic enum, for example:

```text
feature.base_extrude.edge.top_front
feature.base_extrude.vertex.bottom_back_left
```

The saved JSON does not contain OCCT face, edge, vertex, `TopoDS_Shape`, `TShape`, or transient topology identity.

## Evaluated semantic references

`SemanticReferenceEvaluator` resolves semantic edge and vertex references for the same controlled simple rectangular additive extrude topology already used by semantic face workplanes.

The evaluator requires:

- an additive extrude source feature
- a source sketch with exactly one rectangle profile
- valid width, height, and depth length parameters
- a resolvable source sketch workplane

For `feature.base` with a centered `100 mm x 60 mm` rectangle and `10 mm` depth, examples are:

```text
top_front edge:        (-50, 30, 10) -> (50, 30, 10)
bottom_back_left vertex: (-50, -30, 0)
```

Construction-point evaluation currently supports:

```text
explicit point                       -> stored position
point_on_generated_vertex            -> evaluated generated vertex position
point_on_generated_edge              -> deterministic generated-edge midpoint
```

Construction-line evaluation currently supports:

```text
explicit line                                      -> stored point and direction
line_through_two_points                            -> normalized vector between two resolved construction points
line_parallel_to_line_through_point                -> source line direction through a construction point
line_parallel_to_generated_edge_through_point      -> generated edge direction through a construction point
```

`point_on_generated_edge` deliberately resolves to the edge midpoint until the model has a richer point-on-curve parameter or constraint representation.

## Relation-driven placement

Implemented relation type strings:

```text
plane_offset_from_plane
line_through_two_points
plane_through_three_points
point_on_plane
point_on_line
point_on_generated_edge
point_on_generated_vertex
line_on_plane
plane_parallel_to_plane_through_point
line_parallel_to_line_through_point
line_parallel_to_generated_edge_through_point
```

Deterministically resolved construction planes:

- `PlaneOffsetFromPlane` resolves by resolving the source workplane, translating the origin along the source normal by the offset parameter, and preserving the frame axes.
- `PlaneThroughThreePoints` resolves by using the first point as origin, the normalized first-to-second vector as x-axis, and the normalized cross product as normal.
- `PlaneParallelToPlaneThroughPoint` resolves by resolving the source workplane and copying its frame axes to a new origin at the referenced construction point.

`PointOnPlane`, `PointOnLine`, and `LineOnPlane` are currently validated model-intent relations. They are not yet solved geometrically because that would require a richer point/curve constraint model.

## Dependency graph integration

Construction geometry contributes nodes and dependency edges to the model graph.

```text
parameter -> construction_geometry
source_plane -> construction_plane.offset
parameter.offset -> construction_plane.offset
construction_point.a -> construction_line.axis_ab
construction_point.b -> construction_line.axis_ab
construction_point.p -> construction_plane.parallel
source_plane -> construction_plane.parallel
feature.base -> construction_line.parallel_to_generated_edge
construction_point.p -> construction_line.parallel_to_generated_edge
construction_plane -> sketch -> feature
```

If a parameter listed in an offset-plane relation changes, the recompute plan includes the construction plane, dependent sketch, and dependent feature. If a generated-edge relation references a feature, the relation-driven construction object depends on that source feature.

## JSON shape

Construction geometry is serialized as model intent. Explicit geometry keeps numeric placement. Relation-driven geometry embeds a `relation` object.

```json
{
  "construction_points": [
    {
      "id": "point.top_front_right",
      "name": "TopFrontRight",
      "kind": "on_generated_vertex",
      "relation": {
        "id": "relation.point_top_front_right",
        "type": "point_on_generated_vertex",
        "point": "point.top_front_right",
        "generated_vertex": {
          "source_feature": "feature.base",
          "vertex": "top_front_right"
        }
      }
    }
  ],
  "construction_lines": [
    {
      "id": "line.top_front_axis",
      "name": "TopFrontAxis",
      "kind": "parallel_to_generated_edge_through_point",
      "relation": {
        "id": "relation.top_front_axis",
        "type": "line_parallel_to_generated_edge_through_point",
        "generated_edge": {
          "source_feature": "feature.base",
          "edge": "top_front"
        },
        "through_point": "point.top_front_right"
      }
    }
  ]
}
```

## Geometry-layer behavior

`WorkplaneResolver` resolves construction-plane workplanes. `SemanticReferenceEvaluator`, `ConstructionPointResolver`, and `ConstructionLineResolver` resolve semantic generated references and construction point/line geometry.

Construction planes are treated as infinite user reference planes. Generated edge/vertex evaluation is deliberately limited to controlled rectangular additive extrudes and should be expanded by explicit, testable topology families.

## Test coverage

Core tests cover explicit construction geometry, relation creation, relation validation, relation-driven construction objects, dependency graph edges, recompute-plan inclusion after parameter changes, semantic generated edge/vertex references, chained relation dependencies, and JSON roundtrips for semantic generated references and relation-driven construction points.

Geometry tests cover resolving explicit construction planes, offset construction planes, three-point construction planes, planes parallel to another plane through a point, semantic edge/vertex evaluation, construction-point evaluation, construction-line evaluation, mapping local sketch coordinates through construction plane frames, and recomputing closed profiles on explicit and offset construction planes.

## Not implemented yet

The current construction-geometry layer does not implement:

- evaluated expression-based explicit point/line/plane coordinates
- general generated edge/vertex evaluation for arbitrary features
- point-on-generated-edge parameterization other than deterministic midpoint
- point-on-line, point-on-plane, and line-on-plane geometric solving
- plane normal to a line
- tangent or normal references to generated surfaces
- relation collection management independent from construction objects
- GUI manipulators
- assembly-level construction geometry
- 3D sketch splines, guide curves, lofts, sweeps, boundary surfaces, or surface stitching
- exporting construction geometry as final STEP curves or surfaces by default

## Next construction-geometry step

The next construction-geometry increment should project evaluated semantic generated edges and vertices into sketch-space reference geometry so sketches can consume projected construction points and lines without storing raw OCCT topology. It should still avoid a full sketch constraint solver.

## Relationship to existing roadmap

The semantic-face workplane seed proves that sketches can be placed on generated planar faces without storing raw OCCT face IDs. The generated edge/vertex evaluator extends the same principle to edge and vertex references while keeping saved model intent independent from transient OCCT topology.

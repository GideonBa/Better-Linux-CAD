# Construction Geometry MVP

Status: explicit construction geometry, relation-driven construction geometry, semantic generated edge/vertex references, and the first chained construction relations are implemented in the headless core.

This document records the construction-geometry layer for BLCAD. The implementation supports explicit construction points, explicit construction lines, explicit construction planes, relation-driven construction lines and planes, and semantic references to generated edges and vertices. These references are saved as model intent by source feature plus semantic enum, not as raw OCCT topology handles.

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
  -> chained ConstructionRelation
  -> dependency graph edge to the source feature
```

## Implemented scope

The implementation contains:

- `ConstructionPointId`, `ConstructionLineId`, `ConstructionPlaneId`, and `ConstructionRelationId`
- `ConstructionPoint`, `ConstructionLine`, `ConstructionPlane`, and `ConstructionRelation`
- `SemanticFaceReference`, `SemanticEdgeReference`, and `SemanticVertexReference`
- explicit 3D placement for construction points, lines, and planes
- relation-driven construction planes from `PlaneOffsetFromPlane`, `PlaneThroughThreePoints`, and `PlaneParallelToPlaneThroughPoint`
- relation-driven construction lines from `LineThroughTwoPoints`, `LineParallelToLineThroughPoint`, and `LineParallelToGeneratedEdgeThroughPoint`
- model-intent relation definitions for `PointOnPlane`, `PointOnLine`, `PointOnGeneratedEdge`, `PointOnGeneratedVertex`, and `LineOnPlane`
- validation for generated edge/vertex references against additive-extrude source features without storing raw OCCT topology IDs
- collinearity validation for `PlaneThroughThreePoints`
- optional `parameter_dependencies` on explicit construction geometry
- parameter dependency capture for offset-plane relations
- `PartDocument` storage and validation for construction geometry and relation references
- dependency graph nodes for construction geometry
- dependency graph edges from parameters to construction geometry
- dependency graph edges from relation references to relation-driven construction geometry
- dependency graph edges from generated-edge/generated-vertex relations to their source feature
- dependency graph edges from construction planes to sketches
- JSON serialization/deserialization of explicit, relation-driven, chained, and semantic-reference construction geometry
- JSON roundtrip tests for chained construction relations and semantic generated references
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

The current core validates that the source feature exists and is an additive extrude before accepting generated edge/vertex relation references. The saved JSON does not contain OCCT face, edge, vertex, `TopoDS_Shape`, `TShape`, or transient topology identity.

## Relation-driven placement

Relation-driven construction geometry stores model intent as a typed relation object embedded in the construction object.

Implemented relation types:

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

`LineThroughTwoPoints`, `LineParallelToLineThroughPoint`, `LineParallelToGeneratedEdgeThroughPoint`, `PointOnPlane`, `PointOnLine`, `PointOnGeneratedEdge`, `PointOnGeneratedVertex`, and `LineOnPlane` are validated model-intent relations with dependency graph integration. They are not yet consumed by a downstream geometric line evaluator or sketch constraint solver.

## Validation rules

The implementation rejects:

- empty construction object IDs and names
- invalid explicit line or plane frames
- empty construction relation IDs
- empty relation references
- duplicate point references in `LineThroughTwoPoints`
- duplicate or collinear point references in `PlaneThroughThreePoints`
- empty or duplicate parameter dependencies on one construction object
- construction-geometry parameter dependencies that do not exist in the `PartDocument`
- relation references that do not exist in the `PartDocument`
- generated edge/vertex references whose source feature is missing
- generated edge/vertex references whose source feature is not an additive extrude
- duplicate construction plane workplane IDs
- relation-driven line/plane objects whose kind does not match the embedded relation type

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
  "construction_lines": [
    {
      "id": "line.edge_parallel",
      "name": "GeneratedEdgeParallel",
      "kind": "parallel_to_generated_edge_through_point",
      "relation": {
        "id": "relation.edge_parallel",
        "type": "line_parallel_to_generated_edge_through_point",
        "generated_edge": {
          "source_feature": "feature.base",
          "edge": "top_front"
        },
        "through_point": "point.z"
      }
    }
  ],
  "construction_planes": [
    {
      "id": "construction_plane.parallel_xy",
      "name": "ParallelXY",
      "kind": "parallel_to_plane_through_point",
      "relation": {
        "id": "relation.parallel_xy",
        "type": "plane_parallel_to_plane_through_point",
        "source_plane": "datum.xy",
        "through_point": "point.z"
      }
    }
  ]
}
```

Construction-plane deserialization resolves construction-plane dependencies iteratively. Construction-line deserialization runs after sketches and features so generated-edge references can validate against their source features.

## Geometry-layer behavior

`WorkplaneResolver` resolves:

```text
datum.xy
feature.base_extrude.top/bottom/right/left/front/back
explicit construction_plane.<id>
offset_from_plane construction_plane.<id>
through_three_points construction_plane.<id>
parallel_to_plane_through_point construction_plane.<id>
```

A construction plane resolves into a `ResolvedWorkplane` frame with origin, x-axis, y-axis, and normal. Construction-plane bounds are currently disabled because these planes are treated as infinite user reference planes.

## Test coverage

Core tests cover explicit construction geometry, relation creation, relation validation, relation-driven construction objects, dependency graph edges, recompute-plan inclusion after parameter changes, semantic generated edge/vertex references, chained relation dependencies, and JSON roundtrips for semantic generated references.

Geometry tests cover resolving explicit construction planes, offset construction planes, three-point construction planes, planes parallel to another plane through a point, mapping local sketch coordinates through construction plane frames, and recomputing closed profiles on explicit and offset construction planes.

## Not implemented yet

The current construction-geometry layer does not implement:

- evaluated expression-based explicit point/line/plane coordinates
- geometric evaluation of generated edges or generated vertices into exact points/curves
- a general construction-line resolver
- point-on-line, point-on-plane, and line-on-plane geometric solving
- plane normal to a line
- tangent or normal references to generated surfaces
- relation collection management independent from construction objects
- GUI manipulators
- assembly-level construction geometry
- 3D sketch splines, guide curves, lofts, sweeps, boundary surfaces, or surface stitching
- exporting construction geometry as final STEP curves or surfaces by default

## Next construction-geometry step

The next construction-geometry increment should evaluate semantic generated edge and vertex references for the limited rectangular-additive-extrude topology already used by semantic face workplanes, then consume those evaluated references in deterministic construction-point and construction-line evaluators. It should not add a general constraint solver yet.

## Relationship to existing roadmap

The semantic-face workplane seed proves that sketches can be placed on generated planar faces without storing raw OCCT face IDs. The chained-relation step extends the same principle to generated edge and vertex references while still keeping saved model intent independent from transient OCCT topology.

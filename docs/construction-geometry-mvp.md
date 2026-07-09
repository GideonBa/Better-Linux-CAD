# Construction Geometry MVP

Status: explicit construction geometry plus the first relation-driven construction-geometry seed implemented.

This document records the construction-geometry layer for BLCAD. The implementation supports explicit construction points, explicit construction lines, explicit construction planes, and the first relation-driven construction objects. Sketches can reference user-created construction planes as workplanes.

The current relation path is deliberately small. It supports `PlaneOffsetFromPlane`, `LineThroughTwoPoints`, and `PlaneThroughThreePoints`. It does not yet implement parallel/orthogonal/angle relations, generated semantic edge/vertex references, analytic generated-surface references, GUI manipulators, or expression-based coordinate solving for arbitrary point placement.

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

Implemented reference geometry:

```text
ConstructionPoint
ConstructionLine / ConstructionAxis
ConstructionPlane
ConstructionRelation
```

These objects are model-intent objects, not temporary UI artifacts and not OCCT topology handles.

## Implemented scope

The implementation contains:

- `ConstructionPointId`
- `ConstructionLineId`
- `ConstructionPlaneId`
- `ConstructionRelationId`
- `ConstructionPoint`
- `ConstructionLine`
- `ConstructionPlane`
- `ConstructionRelation`
- explicit 3D placement for construction points
- explicit 3D placement for construction lines
- explicit 3D placement for construction planes
- `PlaneOffsetFromPlane` relation-driven construction planes
- `LineThroughTwoPoints` relation-driven construction lines
- `PlaneThroughThreePoints` relation-driven construction planes
- collinearity validation for `PlaneThroughThreePoints`
- optional `parameter_dependencies` on explicit construction geometry
- parameter dependency capture for offset-plane relations
- `PartDocument` storage for construction geometry
- `PartDocument` validation for missing construction-geometry parameter dependencies
- `PartDocument` validation for missing relation references
- dependency graph nodes for construction geometry
- dependency graph edges from parameters to construction geometry
- dependency graph edges from relation references to relation-driven construction geometry
- dependency graph edges from construction planes to sketches
- sketches can reference construction planes through their workplane ID
- JSON serialization/deserialization of explicit and relation-driven construction geometry
- JSON roundtrip tests for explicit and relation-driven construction geometry
- `WorkplaneResolver` support for explicit construction planes
- `WorkplaneResolver` support for offset construction planes
- `WorkplaneResolver` support for construction planes through three points
- geometry recompute tests for closed profiles sketched on explicit and offset construction planes
- checked-in example model `examples/construction_plane_prism.blcad.json`

## Explicit placement

The explicit path allows construction geometry to be placed directly by numeric coordinates and unit vectors.

```text
ConstructionPoint
  id
  name
  position = (x, y, z)
  parameter_dependencies = [...]
```

```text
ConstructionLine
  id
  name
  kind = explicit
  point = (x, y, z)
  direction = unit vector
  parameter_dependencies = [...]
```

```text
ConstructionPlane
  id
  name
  kind = explicit
  origin = (x, y, z)
  x_axis = unit vector
  y_axis = unit vector
  normal = unit vector
  parameter_dependencies = [...]
```

The numeric placement itself is explicit. The `parameter_dependencies` field is an invalidation hook: it records that the construction object depends on one or more parameters, so changing such a parameter marks the construction object, dependent sketches, and dependent features dirty. It does not yet evaluate expressions into new explicit coordinates.

## Relation-driven placement

Relation-driven construction geometry stores model intent as a typed relation object.

```text
ConstructionRelation
  id
  type = plane_offset_from_plane
  source_plane = datum.xy | construction_plane.* | derived workplane id
  offset_parameter = part.offset
```

```text
ConstructionRelation
  id
  type = line_through_two_points
  first_point = construction_point.a
  second_point = construction_point.b
```

```text
ConstructionRelation
  id
  type = plane_through_three_points
  first_point = construction_point.a
  second_point = construction_point.b
  third_point = construction_point.c
```

The first implementation embeds the relation object directly in the relation-driven construction line or construction plane. There is no separate relation collection in `PartDocument` yet. This keeps the API and JSON shape smaller while still making the relation explicit in the model.

`PlaneOffsetFromPlane` resolves by resolving its source workplane, translating the source origin along the source normal by the offset parameter value, and preserving the source x-axis, y-axis, and normal.

`PlaneThroughThreePoints` resolves by using the first point as origin, the normalized vector from the first point to the second point as the x-axis, and the normalized cross product of the first-to-second and first-to-third vectors as the normal. The y-axis is derived from `normal cross x_axis`.

`LineThroughTwoPoints` is currently a validated model-intent relation with dependency graph integration. It is not yet consumed by a downstream geometry feature.

## Validation rules

The implementation rejects:

- empty construction point IDs
- empty construction point names
- empty construction line IDs
- empty construction line names
- zero-length explicit construction line directions
- non-unit explicit construction line directions
- empty construction plane IDs
- empty construction plane names
- zero-length explicit construction plane axes or normals
- non-unit explicit construction plane axes or normals
- non-orthogonal explicit construction plane axes and normals
- explicit construction plane normals that do not match `x_axis cross y_axis`
- empty construction relation IDs
- empty relation references
- duplicate point references in `LineThroughTwoPoints`
- duplicate point references in `PlaneThroughThreePoints`
- collinear point references in `PlaneThroughThreePoints`
- empty parameter dependencies
- duplicate parameter dependencies on the same construction object
- construction-geometry parameter dependencies that do not exist in the `PartDocument`
- relation references that do not exist in the `PartDocument`
- duplicate construction plane workplane IDs

## Dependency graph integration

Construction geometry contributes nodes and dependency edges to the model graph.

Explicit parameter hook:

```text
parameter -> construction_geometry
```

Offset plane relation:

```text
source_plane -> construction_plane.offset
parameter.offset -> construction_plane.offset
```

Line through two points:

```text
construction_point.a -> construction_line.axis_ab
construction_point.b -> construction_line.axis_ab
```

Plane through three points:

```text
construction_point.a -> construction_plane.abc
construction_point.b -> construction_plane.abc
construction_point.c -> construction_plane.abc
```

Sketch integration:

```text
construction_plane -> sketch -> feature
```

If a parameter listed in an offset-plane relation changes, the recompute plan includes the construction plane, dependent sketch, and dependent feature.

## JSON shape

Construction geometry is serialized as model intent. Explicit geometry keeps the original shape:

```json
{
  "construction_planes": [
    {
      "id": "construction_plane.explicit_xy",
      "name": "ExplicitXY",
      "kind": "explicit",
      "origin": {"x": 0.0, "y": 0.0, "z": 25.0},
      "x_axis": {"x": 1.0, "y": 0.0, "z": 0.0},
      "y_axis": {"x": 0.0, "y": 1.0, "z": 0.0},
      "normal": {"x": 0.0, "y": 0.0, "z": 1.0},
      "parameter_dependencies": ["part.plane_offset"]
    }
  ]
}
```

Relation-driven geometry stores an embedded `relation` object:

```json
{
  "construction_lines": [
    {
      "id": "construction_line.axis_ab",
      "name": "AxisAB",
      "kind": "through_two_points",
      "relation": {
        "id": "relation.axis_ab",
        "type": "line_through_two_points",
        "first_point": "construction_point.a",
        "second_point": "construction_point.b"
      }
    }
  ],
  "construction_planes": [
    {
      "id": "construction_plane.offset_xy",
      "name": "OffsetXY",
      "kind": "offset_from_plane",
      "relation": {
        "id": "relation.offset_xy",
        "type": "plane_offset_from_plane",
        "source_plane": "datum.xy",
        "offset_parameter": "part.plane_offset"
      }
    },
    {
      "id": "construction_plane.abc",
      "name": "PlaneABC",
      "kind": "through_three_points",
      "relation": {
        "id": "relation.plane_abc",
        "type": "plane_through_three_points",
        "first_point": "construction_point.a",
        "second_point": "construction_point.b",
        "third_point": "construction_point.c"
      }
    }
  ]
}
```

Construction-plane deserialization resolves construction-plane dependencies iteratively, so an offset construction plane can reference an earlier construction plane in the same file.

## Geometry-layer behavior

`WorkplaneResolver` now resolves:

```text
datum.xy
feature.base_extrude.top/bottom/right/left/front/back
explicit construction_plane.<id>
offset_from_plane construction_plane.<id>
through_three_points construction_plane.<id>
```

A construction plane resolves into a `ResolvedWorkplane` frame with origin, x-axis, y-axis, and normal. Construction-plane bounds are currently disabled because these planes are treated as infinite user reference planes.

## Example

The repository contains:

```text
examples/construction_plane_prism.blcad.json
```

This model defines a construction plane, places a closed-profile sketch on it, recomputes the profile through the geometry layer, and can be exported through the normal headless STEP workflow.

## Test coverage

Core tests cover:

- explicit construction point creation
- construction relation creation
- relation validation for duplicate point references
- explicit construction line creation
- zero-length construction line rejection
- non-unit construction line rejection
- line-through-two-points construction line creation
- explicit construction plane creation
- relation-driven construction plane creation
- zero-length plane frame rejection
- non-orthogonal plane frame rejection
- wrong normal orientation rejection
- `PartDocument` storage for points, lines, and planes
- construction planes acting as sketch workplanes
- dependency graph edges from parameters to construction planes
- dependency graph edges from construction points to relation-driven lines and planes
- dependency graph edges from source workplanes and offset parameters to offset construction planes
- dependency graph edges from construction planes to sketches
- recompute-plan inclusion after an offset parameter changes
- missing parameter-dependency rejection
- missing relation-reference rejection
- collinear three-point plane rejection
- JSON roundtrip for explicit construction geometry
- JSON roundtrip for relation-driven construction geometry

Geometry tests cover:

- resolving an explicit construction plane into a `ResolvedWorkplane`
- resolving an offset construction plane relation into a `ResolvedWorkplane`
- resolving a three-point construction plane relation into a `ResolvedWorkplane`
- mapping local sketch coordinates through explicit and relation-driven construction plane frames
- recomputing a closed profile sketched on an explicit construction plane
- recomputing a closed profile sketched on an offset construction plane relation

## Not implemented yet

The current construction-geometry seed does not implement:

- evaluated expression-based explicit point/line/plane coordinates
- plane parallel to another plane through a point
- line parallel to another line through a point
- plane normal to a line
- point-on-line relation
- point-on-plane relation
- line-on-plane relation
- parallel, orthogonal, or angle relations
- references to generated semantic edges or vertices
- tangent or normal references to generated surfaces
- relation collection management independent from construction objects
- GUI manipulators
- assembly-level construction geometry
- 3D sketch splines, guide curves, lofts, sweeps, boundary surfaces, or surface stitching
- exporting construction geometry as final STEP curves or surfaces by default

## Next construction-geometry step

The next construction-geometry increment should stabilize construction relations into a more general reference system instead of adding GUI editing immediately.

Recommended next sequence:

1. Add semantic generated edge and vertex reference IDs similar to `SemanticFaceReference`.
2. Add relation validation for generated edge/vertex references without storing raw OCCT topology IDs.
3. Add `PointOnPlane`, `PointOnLine`, and `LineOnPlane` relation definitions.
4. Add `PlaneParallelToPlaneThroughPoint` and `LineParallelToLineThroughPoint`.
5. Add relation dependency tests covering chained relations.
6. Add JSON roundtrip tests for chained relations and semantic references.
7. Add resolver support only where a relation can be evaluated deterministically from existing model intent.
8. Keep GUI manipulators, constraint solving, and 3D sketch splines out of this step.

## Relationship to existing roadmap

The current MVP 2 semantic-face workplane seed proves that sketches can be placed on generated planar faces without storing raw OCCT face IDs.

The line-based closed-profile block proves that a planar sketch can define a non-rectangular area from explicit line loops.

This construction-geometry block generalizes workplane placement from fixed datum planes and controlled generated faces to user-defined and relation-driven reference planes.

```text
Construction geometry answers: where can a sketch or curve be placed?
General closed sketch profiles answer: what planar shape can a sketch describe?
Inventor-like sketcher roadmap answers: how should sketch entities, constraints, dimensions, and profile detection eventually behave?
Advanced surfacing answers: how can spatial curves, multiple sketches, guide curves, and surfaces form freeform geometry?
```

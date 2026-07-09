# Construction Geometry MVP

Status: first explicit construction-geometry MVP implemented.

This document records the construction-geometry layer for BLCAD. The first implementation supports explicit construction points, explicit construction lines, and explicit construction planes. It also lets sketches reference user-created construction planes as workplanes.

The current implementation is intentionally narrower than the full relation-driven datum system. It does not yet implement offset-plane relations, line-through-two-points relations, plane-through-three-points relations, parallel/orthogonal/angle relations, generated edge/vertex references, or GUI manipulators.

## Goal

Construction geometry gives users stable helper geometry that is part of model intent.

Implemented user path:

```text
ConstructionPlane
  -> Sketch on ConstructionPlane
  -> Feature using that sketch
```

Implemented reference geometry:

```text
ConstructionPoint
ConstructionLine / ConstructionAxis
ConstructionPlane
```

These objects are model-intent objects, not temporary UI artifacts and not OCCT topology handles.

## Implemented scope

The first implementation adds:

- `ConstructionPointId`
- `ConstructionLineId`
- `ConstructionPlaneId`
- `ConstructionPoint`
- `ConstructionLine`
- `ConstructionPlane`
- explicit 3D placement for construction points
- explicit 3D placement for construction lines
- explicit 3D placement for construction planes
- optional `parameter_dependencies` on construction geometry
- `PartDocument` storage for construction geometry
- `PartDocument` validation for missing construction-geometry parameter dependencies
- dependency graph nodes for construction geometry
- dependency graph edges from parameters to construction geometry
- dependency graph edges from construction planes to sketches
- sketches can reference construction planes through their workplane ID
- JSON serialization/deserialization of construction points, lines, and planes
- JSON roundtrip tests for explicit construction geometry
- `WorkplaneResolver` support for explicit construction planes
- geometry recompute test for a closed profile sketched on a construction plane
- checked-in example model `examples/construction_plane_prism.blcad.json`

## Explicit placement

The first useful version allows construction geometry to be placed directly by numeric coordinates and unit vectors.

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
  point = (x, y, z)
  direction = unit vector
  parameter_dependencies = [...]
```

```text
ConstructionPlane
  id
  name
  origin = (x, y, z)
  x_axis = unit vector
  y_axis = unit vector
  normal = unit vector
  parameter_dependencies = [...]
```

The numeric placement itself is currently explicit. The `parameter_dependencies` field is an invalidation hook: it records that the construction object depends on one or more parameters, so changing such a parameter marks the construction object, dependent sketches, and dependent features dirty. It does not yet evaluate expressions into new coordinates.

## Validation rules

The current implementation rejects:

- empty construction point IDs
- empty construction point names
- empty construction line IDs
- empty construction line names
- zero-length construction line directions
- non-unit construction line directions
- empty construction plane IDs
- empty construction plane names
- zero-length construction plane axes or normals
- non-unit construction plane axes or normals
- non-orthogonal construction plane axes and normals
- construction plane normals that do not match `x_axis cross y_axis`
- empty parameter dependencies
- duplicate parameter dependencies on the same construction object
- construction-geometry parameter dependencies that do not exist in the `PartDocument`
- duplicate construction plane workplane IDs

## Sketch integration

A sketch can reference a construction plane as its workplane:

```text
construction_plane.offset_xy
  -> sketch.closed_rectangle_on_plane
  -> feature.closed_rectangle_prism
```

The dependency graph stores this explicitly:

```text
parameter -> construction_plane -> sketch -> feature
```

If a parameter listed in a construction plane's `parameter_dependencies` changes, the recompute plan includes the construction plane, the sketch, and the dependent feature.

## JSON shape

Construction geometry is serialized as model intent:

```json
{
  "construction_points": [
    {
      "id": "construction_point.anchor",
      "name": "Anchor",
      "kind": "explicit",
      "position": {"x": 0.0, "y": 0.0, "z": 25.0},
      "parameter_dependencies": ["part.plane_offset"]
    }
  ],
  "construction_lines": [
    {
      "id": "construction_line.axis_z",
      "name": "AxisZ",
      "kind": "explicit",
      "point": {"x": 0.0, "y": 0.0, "z": 0.0},
      "direction": {"x": 0.0, "y": 0.0, "z": 1.0},
      "parameter_dependencies": []
    }
  ],
  "construction_planes": [
    {
      "id": "construction_plane.offset_xy",
      "name": "OffsetXY",
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

## Geometry-layer behavior

`WorkplaneResolver` now resolves:

```text
datum.xy
feature.base_extrude.top/bottom/right/left/front/back
construction_plane.<id>
```

A construction plane resolves directly into a `ResolvedWorkplane` frame with origin, x-axis, y-axis, and normal. Construction-plane bounds are currently disabled because the plane is an infinite user reference plane in the first MVP.

## Example

The repository contains:

```text
examples/construction_plane_prism.blcad.json
```

This model defines an explicit construction plane, places a closed-profile sketch on it, recomputes the profile through the geometry layer, and can be exported through the normal headless STEP workflow.

## Test coverage

Core tests cover:

- explicit construction point creation
- explicit construction line creation
- zero-length construction line rejection
- non-unit construction line rejection
- explicit construction plane creation
- zero-length plane frame rejection
- non-orthogonal plane frame rejection
- wrong normal orientation rejection
- `PartDocument` storage for points, lines, and planes
- construction planes acting as sketch workplanes
- dependency graph edges from parameters to construction planes
- dependency graph edges from construction planes to sketches
- recompute-plan inclusion after a construction dependency parameter changes
- missing parameter-dependency rejection
- JSON roundtrip for points, lines, planes, dependencies, sketches, and features

Geometry tests cover:

- resolving an explicit construction plane into a `ResolvedWorkplane`
- mapping local sketch coordinates through a construction plane frame
- recomputing a closed profile sketched on a construction plane

## Not implemented yet

The first construction-geometry MVP does not implement:

- evaluated expression-based placement
- offset plane from another plane
- line through two points
- plane through three points
- plane parallel to another plane through a point
- line parallel to another line through a point
- plane normal to a line
- point-on-line relation
- point-on-plane relation
- line-on-plane relation
- parallel, orthogonal, or angle relations
- references to generated semantic edges or vertices
- tangent or normal references to generated surfaces
- GUI manipulators
- assembly-level construction geometry
- 3D sketch splines, guide curves, lofts, sweeps, boundary surfaces, or surface stitching
- exporting construction geometry as final STEP curves or surfaces by default

## Next construction-geometry step

The next construction-geometry increment should add relation-driven construction definitions while keeping the explicit placement path stable.

Recommended next sequence:

1. Add a `ConstructionRelation` model.
2. Add `PlaneOffsetFromPlane` as the first relation-driven plane definition.
3. Add `LineThroughTwoPoints` as the first relation-driven line definition.
4. Add `PlaneThroughThreePoints` with collinearity validation.
5. Add dependency graph edges from referenced construction objects to relation-driven construction objects.
6. Add JSON roundtrip tests for each relation type.
7. Add `WorkplaneResolver` support for relation-driven construction planes.
8. Add invalidation tests for point/line/plane reference changes.
9. Only after that, add parallel, orthogonal, angle, and semantic face/edge/vertex references.

## Relationship to existing roadmap

The current MVP 2 semantic-face workplane seed proves that sketches can be placed on generated planar faces without storing raw OCCT face IDs.

The line-based closed-profile block proves that a planar sketch can define a non-rectangular area from explicit line loops.

This construction-geometry block generalizes workplane placement from fixed datum planes and controlled generated faces to user-defined reference planes.

```text
Construction geometry answers: where can a sketch or curve be placed?
General closed sketch profiles answer: what planar shape can a sketch describe?
Inventor-like sketcher roadmap answers: how should sketch entities, constraints, dimensions, and profile detection eventually behave?
Advanced surfacing answers: how can spatial curves, multiple sketches, guide curves, and surfaces form freeform geometry?
```

# Workplane Resolver

Status: geometry-layer resolver for standard datum planes, derived top/bottom/right/left/front/back workplanes of a simple additive extrude, explicit construction planes, offset construction planes, three-point construction planes, and planes parallel to another plane through a construction point.

The core stores semantic model intent only. The geometry layer resolves supported intent into concrete frames for recompute. Raw OCCT topology IDs are not stored in `PartDocument` JSON.

## Goal

The resolver makes paths such as these geometrically meaningful:

```text
feature.base_extrude.back
  -> workplane.base_back
  -> sketch.back_hole local point
  -> global cut center
```

```text
construction_plane.offset_xy
  -> sketch.closed_rectangle_on_plane local point
  -> global profile vertex
```

```text
construction_plane.parallel_xy
  -> source workplane datum.xy
  -> construction point through_point
  -> resolved parallel plane frame
```

## Supported workplanes

For `datum.xy`, the resolver returns the stored datum-plane frame and leaves bounds disabled.

For an explicit construction plane, the resolver returns the construction plane frame directly and leaves bounds disabled.

For an offset construction-plane relation, the resolver first resolves the source workplane, then translates the source origin along the source normal by the offset parameter value:

```text
origin = source.origin + offset_mm * source.normal
x_axis = source.x_axis
y_axis = source.y_axis
normal = source.normal
bounds.enabled = false
```

For a three-point construction-plane relation, the resolver builds a right-handed frame from the referenced construction points:

```text
origin = first_point.position
x_axis = normalize(second_point.position - first_point.position)
normal = normalize(cross(second - first, third - first))
y_axis = normalize(cross(normal, x_axis))
bounds.enabled = false
```

For a plane-parallel-through-point relation, the resolver resolves the source workplane and moves the frame origin to the referenced construction point:

```text
origin = through_point.position
x_axis = source.x_axis
y_axis = source.y_axis
normal = source.normal
bounds.enabled = false
```

For `workplane.base_top -> feature.base_extrude.top`, the frame is:

```text
origin = (rectangle_center.x, rectangle_center.y, thickness)
x_axis = (1, 0, 0)
y_axis = (0, 1, 0)
normal = (0, 0, 1)
```

For `workplane.base_bottom -> feature.base_extrude.bottom`, the frame is:

```text
origin = (rectangle_center.x, rectangle_center.y, 0)
x_axis = (1, 0, 0)
y_axis = (0, 1, 0)
normal = (0, 0, -1)
```

For `workplane.base_right -> feature.base_extrude.right`, the frame is:

```text
origin = (rectangle_center.x + width / 2, rectangle_center.y, thickness / 2)
x_axis = (0, 1, 0)
y_axis = (0, 0, 1)
normal = (1, 0, 0)
```

For `workplane.base_left -> feature.base_extrude.left`, the frame is:

```text
origin = (rectangle_center.x - width / 2, rectangle_center.y, thickness / 2)
x_axis = (0, -1, 0)
y_axis = (0, 0, 1)
normal = (-1, 0, 0)
```

For `workplane.base_front -> feature.base_extrude.front`, the frame is:

```text
origin = (rectangle_center.x, rectangle_center.y + height / 2, thickness / 2)
x_axis = (-1, 0, 0)
y_axis = (0, 0, 1)
normal = (0, 1, 0)
```

For `workplane.base_back -> feature.base_extrude.back`, the frame is:

```text
origin = (rectangle_center.x, rectangle_center.y - height / 2, thickness / 2)
x_axis = (1, 0, 0)
y_axis = (0, 0, 1)
normal = (0, -1, 0)
```

Side faces and construction planes use right-handed local frames:

```text
x_axis cross y_axis = normal
```

## Rectangular bounds

Generated-face derived workplanes keep rectangular bounds for top, bottom, right, left, front, and back faces. Construction planes remain unbounded in the current implementation because they represent infinite user reference planes.

## Recompute integration

`GeometryRecomputeExecutor` resolves the input sketch workplane before executing circular cuts and closed-profile extrudes/cuts. The mapping formula is:

```text
global = origin + local.x * x_axis + local.y * y_axis
```

For `construction_plane.offset_xy` with resolved origin `(0, 0, 25)` and local `(2, 3)`, this becomes:

```text
global = (2, 3, 25)
```

For `construction_plane.parallel_xy` through point `(5, 6, 7)` and local `(2, 3)`, this becomes:

```text
global = (7, 9, 7)
```

`GeometryRecomputeExecutor` passes the resolved workplane normal to circular cuts and closed-profile extrudes/cuts. Top and bottom use Z-axis cuts. Right and left use X-axis cuts. Front and back use Y-axis cuts. Construction planes use their explicit or relation-resolved normal.

## Relation scope

The resolver intentionally supports only relations that can be evaluated from current headless model intent:

- explicit construction planes
- `PlaneOffsetFromPlane`
- `PlaneThroughThreePoints`
- `PlaneParallelToPlaneThroughPoint`

Semantic generated edge and vertex references are validated and serialized in the core, but they are not yet evaluated into exact points or curves by the geometry layer.

## Example models

```text
examples/top_face_cut.blcad.json
examples/bottom_face_cut.blcad.json
examples/right_face_cut.blcad.json
examples/left_face_cut.blcad.json
examples/front_face_cut.blcad.json
examples/back_face_cut.blcad.json
examples/construction_plane_prism.blcad.json
```

## Test coverage

Geometry tests cover:

- resolving `datum.xy`
- resolving `workplane.base_top`
- resolving `workplane.base_bottom`
- resolving `workplane.base_right`
- resolving `workplane.base_left`
- resolving `workplane.base_front`
- resolving `workplane.base_back`
- resolving explicit construction planes
- resolving offset construction-plane relations
- resolving three-point construction-plane relations
- resolving planes parallel to another plane through a point
- checking top/bottom/side origins and normals
- checking construction plane origins, axes, normals, and unbounded status
- checking rectangular bounds for resolved generated-face workplanes
- mapping local sketch points through resolved frames
- rejecting missing workplanes
- recomputing circular cuts and closed-profile operations from sketches on resolved workplanes

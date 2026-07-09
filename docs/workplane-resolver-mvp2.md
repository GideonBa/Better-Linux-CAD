# Workplane Resolver

Status: geometry-layer resolver for standard datum planes, derived top/bottom/right/left/front/back workplanes of a simple additive extrude, explicit construction planes, and the first relation-driven construction planes.

This document describes the resolver after introducing `SemanticFaceReference`, `DerivedWorkplane`, `ConstructionPlane`, and `ConstructionRelation` in the core model. The core stores semantic model intent only. The geometry layer resolves that intent into concrete frames for recompute.

## Goal

The resolver makes paths such as these geometrically meaningful:

```text
feature.base_extrude.back
  -> workplane.base_back
  -> sketch.back_hole local point
  -> global cut center
```

and:

```text
construction_plane.offset_xy
  -> sketch.closed_rectangle_on_plane local point
  -> global profile vertex
```

The resolver proves that sketches on generated planar faces and user-created construction planes can be evaluated without storing raw OCCT face IDs in `PartDocument`.

## Supported workplanes

For `datum.xy`, the resolver returns the stored datum-plane frame and leaves bounds disabled.

For an explicit construction plane, the resolver returns the construction plane frame directly and leaves bounds disabled:

```text
origin = construction_plane.origin
x_axis = construction_plane.x_axis
y_axis = construction_plane.y_axis
normal = construction_plane.normal
bounds.enabled = false
```

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

Top and bottom derived workplanes use source rectangle width and height:

```text
bounds.width_mm = source rectangle width
bounds.height_mm = source rectangle height
```

Right and left derived workplanes use source rectangle height and extrude thickness:

```text
bounds.width_mm = source rectangle height
bounds.height_mm = extrude thickness
```

Front and back use source rectangle width and extrude thickness:

```text
bounds.width_mm = source rectangle width
bounds.height_mm = extrude thickness
```

Construction planes are unbounded in the current implementation:

```text
bounds.enabled = false
```

## Recompute integration

`GeometryRecomputeExecutor::execute_subtractive_extrude` resolves the input sketch workplane before executing the circular cut. Before mapping the point into global coordinates, the executor validates the circle radius against rectangular bounds when bounds are enabled.

Line-based closed-profile geometry also resolves the input sketch workplane before converting local vertices into global points. If the workplane has bounds, closed-profile vertices are validated against those bounds. If the workplane is a construction plane, bounds are disabled and the profile is mapped through the construction plane frame.

The mapping formula is:

```text
global = origin + local.x * x_axis + local.y * y_axis
```

For `workplane.base_back` and local `(-12, 1.5)`, this becomes:

```text
global = (-12, -40, 5.5)
```

For `construction_plane.offset_xy` with resolved origin `(0, 0, 25)` and local `(2, 3)`, this becomes:

```text
global = (2, 3, 25)
```

`GeometryRecomputeExecutor` passes the resolved workplane normal to circular cuts and closed-profile extrudes/cuts. Top and bottom use Z-axis cuts. Right and left use X-axis cuts. Front and back use Y-axis cuts. Construction planes use their stored or relation-resolved normal.

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

Export commands:

```bash
./build/dev-geometry/blcad_export_step examples/top_face_cut.blcad.json build/top_face_cut.step
./build/dev-geometry/blcad_export_step examples/bottom_face_cut.blcad.json build/bottom_face_cut.step
./build/dev-geometry/blcad_export_step examples/right_face_cut.blcad.json build/right_face_cut.step
./build/dev-geometry/blcad_export_step examples/left_face_cut.blcad.json build/left_face_cut.step
./build/dev-geometry/blcad_export_step examples/front_face_cut.blcad.json build/front_face_cut.step
./build/dev-geometry/blcad_export_step examples/back_face_cut.blcad.json build/back_face_cut.step
./build/dev-geometry/blcad_export_step examples/construction_plane_prism.blcad.json build/construction_plane_prism.step
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
- checking top/bottom/side origins and normals
- checking construction plane origins, axes, normals, and unbounded status
- checking rectangular bounds for resolved generated-face workplanes
- mapping local sketch points through resolved frames
- rejecting missing workplanes
- recomputing circular cuts from sketches on top, bottom, right, left, front, and back derived workplanes
- recomputing a closed profile from a sketch on an explicit construction plane
- recomputing a closed profile from a sketch on an offset construction-plane relation
- accepting valid circles inside bounds
- rejecting out-of-bounds circles before executing the cut
- verifying that final volumes match expected values within tolerance

## Deliberate limitation

Not included yet:

- arbitrary generated planar faces
- generated semantic edge or vertex references
- face orientation derived from OCCT topology
- non-rectangular source faces for derived generated-face workplanes
- storing or matching raw OCCT face IDs
- GUI face selection
- 3D sketch workplanes or spatial curve frames
- parallel, orthogonal, angle, tangent, or normal construction relations

The resolver is intentionally limited to selected semantic faces of a simple additive rectangle extrusion and deterministic construction-plane relations. This keeps the workplane system incremental and avoids prematurely building a full topological-naming system or construction-geometry solver.

# MVP 2 Workplane Resolver

Status: geometry-layer resolver for derived top, bottom, right, left, front, and back workplanes of a simple additive extrude, including rectangular bounds.

This document describes the resolver after introducing `SemanticFaceReference` and `DerivedWorkplane` in the core model. The core still stores semantic model intent only. The geometry layer resolves that semantic workplane into a concrete frame for recompute.

## Goal

The resolver makes paths such as these geometrically meaningful:

```text
feature.base_extrude.back
  -> workplane.base_back
  -> sketch.back_hole local point
  -> global cut center
```

The resolver proves that sketches on generated planar faces can be evaluated without storing raw OCCT face IDs in `PartDocument`.

## Supported workplanes

For `datum.xy`, the resolver returns the stored datum-plane frame and leaves bounds disabled.

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

Side faces use right-handed local frames:

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

## Recompute integration

`GeometryRecomputeExecutor::execute_subtractive_extrude` resolves the input sketch workplane before executing the circular cut. Before mapping the point into global coordinates, the executor validates the circle radius against rectangular bounds when bounds are enabled.

Then the point is mapped into global coordinates:

```text
global = origin + local.x * x_axis + local.y * y_axis
```

For `workplane.base_back` and local `(-12, 1.5)`, this becomes:

```text
global = (-12, -40, 5.5)
```

`GeometryRecomputeExecutor` passes the resolved workplane normal to the circular cut adapter. Top and bottom use Z-axis cuts. Right and left use X-axis cuts. Front and back use Y-axis cuts.

## Example models

```text
examples/top_face_cut.blcad.json
examples/bottom_face_cut.blcad.json
examples/right_face_cut.blcad.json
examples/left_face_cut.blcad.json
examples/front_face_cut.blcad.json
examples/back_face_cut.blcad.json
```

Export commands:

```bash
./build/dev-geometry/blcad_export_step examples/top_face_cut.blcad.json build/top_face_cut.step
./build/dev-geometry/blcad_export_step examples/bottom_face_cut.blcad.json build/bottom_face_cut.step
./build/dev-geometry/blcad_export_step examples/right_face_cut.blcad.json build/right_face_cut.step
./build/dev-geometry/blcad_export_step examples/left_face_cut.blcad.json build/left_face_cut.step
./build/dev-geometry/blcad_export_step examples/front_face_cut.blcad.json build/front_face_cut.step
./build/dev-geometry/blcad_export_step examples/back_face_cut.blcad.json build/back_face_cut.step
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
- checking top/bottom/side origins and normals
- checking rectangular bounds for resolved generated-face workplanes
- mapping local sketch points through resolved frames
- rejecting missing workplanes
- recomputing circular cuts from sketches on top, bottom, right, left, front, and back derived workplanes
- accepting valid circles inside bounds
- rejecting out-of-bounds circles before executing the cut
- verifying that the final volume matches the expected removed cylinder volume within tolerance

## Deliberate limitation

Not included yet:

- arbitrary planar faces
- face orientation derived from OCCT topology
- non-rectangular source faces
- storing or matching raw OCCT face IDs
- GUI face selection
- general closed sketch profiles

The resolver is intentionally limited to selected semantic faces of a simple additive rectangle extrusion. This keeps MVP 2 incremental and avoids prematurely building a full topological-naming system.

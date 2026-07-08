# MVP 2 Front-Face Derived Workplane

Status: minimal front-face semantic reference and geometry resolution for the `+Y` face of a simple additive rectangle extrude.

This document describes the first Y-side face after top, bottom, right, and left derived workplanes. It remains intentionally narrow and does not introduce arbitrary topology.

## Goal

The supported front-face model path is:

```text
feature.base_extrude.front
  -> workplane.base_front
  -> sketch.front_hole
  -> feature.front_hole_cut
```

The implementation still avoids raw OCCT face IDs in `PartDocument`.

## Core model

`SemanticFace` now supports:

```text
top
bottom
right
left
front
```

A front-face reference is represented as:

```text
SemanticFaceReference
  source_feature = feature.base_extrude
  face = front
```

A derived workplane can expose that front face:

```text
DerivedWorkplane
  id = workplane.base_front
  name = BaseFrontFace
  kind = feature_face
  source_feature = feature.base_extrude
  face = front
```

## Geometry resolution

`WorkplaneResolver` resolves `feature.base_extrude.front` as:

```text
origin = (rectangle_center.x, rectangle_center.y + height / 2, thickness / 2)
x_axis = (-1, 0, 0)
y_axis = (0, 0, 1)
normal = (0, 1, 0)
```

The `x_axis` points to `-X` so the local frame is right-handed:

```text
x_axis cross y_axis = normal
```

The local side-face support region is:

```text
center = (0, 0)
width = source rectangle width
height = extrude thickness
```

For the reference model:

```text
local x range = [-60, 60]
local y range = [-4, 4]
```

Because the current plate is only 8 mm thick, the front-face example uses a 4 mm diameter side hole.

## Axis-directed circular cut

`GeometryRecomputeExecutor` passes the resolved workplane normal to:

```text
cut_circular_hole_along_axis(target, diameter, center, axis_direction)
```

For the front face, the axis is:

```text
(0, 1, 0)
```

This produces a through-all Y-axis side cut from the front side.

## Example model

The checked-in example is:

```text
examples/front_face_cut.blcad.json
```

It places a circular through-all cut sketch on:

```text
workplane.base_front
```

with local center:

```text
(-12, 0)
```

Export command:

```bash
./build/dev-geometry/blcad_export_step examples/front_face_cut.blcad.json build/front_face_cut.step
```

## Test coverage

Core tests cover:

- `SemanticFace::Front`
- `to_string(SemanticFace::Front) == "front"`
- adding a derived front-face workplane to `PartDocument`
- adding a sketch on that front workplane
- dependency graph edges from base feature to front workplane, front workplane to sketch, and sketch to cut
- JSON roundtrip for front-face workplanes

Geometry tests cover:

- resolving `workplane.base_front`
- checking front origin at `y = height / 2` and `z = thickness / 2`
- checking front normal `(0, 1, 0)`
- checking local axes `local x -> -X` and `local y -> +Z`
- checking rectangular bounds `width x thickness`
- mapping a local front-face sketch point into global coordinates
- recomputing a through-all Y-axis circular cut from a sketch on the front face
- rejecting an out-of-bounds side-face circle before cutting

## Deliberate limitation

Not included yet:

- back face
- arbitrary planar faces
- face orientation derived from OCCT topology
- full topological naming
- GUI face selection

The front-face path adds one controlled Y-side case before completing the opposite `back` side.

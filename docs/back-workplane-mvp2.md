# MVP 2 Back-Face Derived Workplane

Status: minimal back-face semantic reference and geometry resolution for the `-Y` face of a simple additive rectangle extrude.

This document completes the controlled six-face semantic workplane seed for simple rectangle extrudes: top, bottom, right, left, front, and back. It remains intentionally narrow and does not introduce arbitrary topology.

## Goal

The supported back-face model path is:

```text
feature.base_extrude.back
  -> workplane.base_back
  -> sketch.back_hole
  -> feature.back_hole_cut
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
back
```

A back-face reference is represented as:

```text
SemanticFaceReference
  source_feature = feature.base_extrude
  face = back
```

A derived workplane can expose that back face:

```text
DerivedWorkplane
  id = workplane.base_back
  name = BaseBackFace
  kind = feature_face
  source_feature = feature.base_extrude
  face = back
```

## Geometry resolution

`WorkplaneResolver` resolves `feature.base_extrude.back` as:

```text
origin = (rectangle_center.x, rectangle_center.y - height / 2, thickness / 2)
x_axis = (1, 0, 0)
y_axis = (0, 0, 1)
normal = (0, -1, 0)
```

The local frame is right-handed:

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

Because the current plate is only 8 mm thick, the back-face example uses a 4 mm diameter side hole.

## Axis-directed circular cut

`GeometryRecomputeExecutor` passes the resolved workplane normal to:

```text
cut_circular_hole_along_axis(target, diameter, center, axis_direction)
```

For the back face, the axis is:

```text
(0, -1, 0)
```

This produces a through-all Y-axis side cut from the back side.

## Example model

The checked-in example is:

```text
examples/back_face_cut.blcad.json
```

It places a circular through-all cut sketch on:

```text
workplane.base_back
```

with local center:

```text
(-12, 0)
```

Export command:

```bash
./build/dev-geometry/blcad_export_step examples/back_face_cut.blcad.json build/back_face_cut.step
```

## Test coverage

Core tests cover:

- `SemanticFace::Back`
- `to_string(SemanticFace::Back) == "back"`
- adding a derived back-face workplane to `PartDocument`
- adding a sketch on that back workplane
- dependency graph edges from base feature to back workplane, back workplane to sketch, and sketch to cut
- JSON roundtrip for back-face workplanes

Geometry tests cover:

- resolving `workplane.base_back`
- checking back origin at `y = -height / 2` and `z = thickness / 2`
- checking back normal `(0, -1, 0)`
- checking local axes `local x -> +X` and `local y -> +Z`
- checking rectangular bounds `width x thickness`
- mapping a local back-face sketch point into global coordinates
- recomputing a through-all Y-axis circular cut from a sketch on the back face
- rejecting an out-of-bounds side-face circle before cutting

## Deliberate limitation

Not included yet:

- arbitrary planar faces
- face orientation derived from OCCT topology
- full topological naming
- GUI face selection
- general closed sketch profiles

The back-face path completes the controlled semantic-face seed for one simple additive rectangle extrude. Broader generated-face support and arbitrary sketch profiles remain separate future blocks.

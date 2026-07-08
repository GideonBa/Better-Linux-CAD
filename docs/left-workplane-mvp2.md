# MVP 2 Left-Face Derived Workplane

Status: minimal left-face semantic reference and geometry resolution for the `-X` face of a simple additive rectangle extrude.

This document describes the matching opposite side-face step after the right-face derived workplane. It remains intentionally narrow and does not introduce arbitrary topology.

## Goal

The supported left-face model path is:

```text
feature.base_extrude.left
  -> workplane.base_left
  -> sketch.left_hole
  -> feature.left_hole_cut
```

The implementation still avoids raw OCCT face IDs in `PartDocument`.

## Core model

`SemanticFace` now supports:

```text
top
bottom
right
left
```

A left-face reference is represented as:

```text
SemanticFaceReference
  source_feature = feature.base_extrude
  face = left
```

A derived workplane can expose that left face:

```text
DerivedWorkplane
  id = workplane.base_left
  name = BaseLeftFace
  kind = feature_face
  source_feature = feature.base_extrude
  face = left
```

## Geometry resolution

`WorkplaneResolver` resolves `feature.base_extrude.left` as:

```text
origin = (rectangle_center.x - width / 2, rectangle_center.y, thickness / 2)
x_axis = (0, -1, 0)
y_axis = (0, 0, 1)
normal = (-1, 0, 0)
```

This keeps the left side frame right-handed:

```text
x_axis cross y_axis = normal
```

The local side-face support region is the same size as the right face:

```text
center = (0, 0)
width = source rectangle height
height = extrude thickness
```

For the reference model:

```text
local x range = [-40, 40]
local y range = [-4, 4]
```

Because the current plate is only 8 mm thick, the left-face example uses a 4 mm diameter side hole.

## Axis-directed circular cut

`GeometryRecomputeExecutor` passes the resolved workplane normal to:

```text
cut_circular_hole_along_axis(target, diameter, center, axis_direction)
```

For the left face, the axis is:

```text
(-1, 0, 0)
```

This produces a through-all X-axis side cut from the left side.

## Example model

The checked-in example is:

```text
examples/left_face_cut.blcad.json
```

It places a circular through-all cut sketch on:

```text
workplane.base_left
```

with local center:

```text
(-12, 0)
```

Export command:

```bash
./build/dev-geometry/blcad_export_step examples/left_face_cut.blcad.json build/left_face_cut.step
```

## Test coverage

Core tests cover:

- `SemanticFace::Left`
- `to_string(SemanticFace::Left) == "left"`
- adding a derived left-face workplane to `PartDocument`
- adding a sketch on that left workplane
- dependency graph edges from base feature to left workplane, left workplane to sketch, and sketch to cut
- JSON roundtrip for left-face workplanes

Geometry tests cover:

- resolving `workplane.base_left`
- checking left origin at `x = -width / 2` and `z = thickness / 2`
- checking left normal `(-1, 0, 0)`
- checking local axes `local x -> -Y` and `local y -> +Z`
- checking rectangular bounds `height x thickness`
- mapping a local left-face sketch point into global coordinates
- recomputing a through-all X-axis circular cut from a sketch on the left face
- rejecting an out-of-bounds side-face circle before cutting

## Deliberate limitation

Not included yet:

- front face
- back face
- arbitrary planar faces
- face orientation derived from OCCT topology
- full topological naming
- GUI face selection

The left-face path completes the current X-side pair before broadening to the remaining Y-side faces.

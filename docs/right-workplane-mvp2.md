# MVP 2 Right-Face Derived Workplane

Status: minimal right-face semantic reference and geometry resolution for the `+X` face of a simple additive rectangle extrude.

This document describes the first side-face step after supporting top and bottom derived workplanes. It is intentionally narrow and does not introduce arbitrary topology.

## Goal

The supported right-face model path is:

```text
feature.base_extrude.right
  -> workplane.base_right
  -> sketch.right_hole
  -> feature.right_hole_cut
```

The implementation still avoids raw OCCT face IDs in `PartDocument`.

## Core model

`SemanticFace` supports the current controlled face set:

```text
top
bottom
right
left
```

A right-face reference is represented as:

```text
SemanticFaceReference
  source_feature = feature.base_extrude
  face = right
```

A derived workplane can expose that right face:

```text
DerivedWorkplane
  id = workplane.base_right
  name = BaseRightFace
  kind = feature_face
  source_feature = feature.base_extrude
  face = right
```

## Geometry resolution

`WorkplaneResolver` resolves `feature.base_extrude.right` as:

```text
origin = (rectangle_center.x + width / 2, rectangle_center.y, thickness / 2)
x_axis = (0, 1, 0)
y_axis = (0, 0, 1)
normal = (1, 0, 0)
```

This means local side-face coordinates are:

```text
local x -> global Y
local y -> global Z
```

The rectangular local support region is:

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

Because the current plate is only 8 mm thick, the right-face example uses a 4 mm diameter side hole.

## Axis-directed circular cut

The original `CircularCutAdapter::cut_circular_hole` performed only vertical Z-axis through-all cuts. Right-face sketches need a through-all cut along the X axis.

The adapter now also exposes:

```text
cut_circular_hole_along_axis(target, diameter, center, axis_direction)
```

`GeometryRecomputeExecutor` uses the resolved workplane normal as the cut axis. For the right face, this produces a through-all X-axis cylinder.

## Example model

The checked-in example is:

```text
examples/right_face_cut.blcad.json
```

It places a circular through-all cut sketch on:

```text
workplane.base_right
```

with local center:

```text
(-12, 0)
```

Export command:

```bash
./build/dev-geometry/blcad_export_step examples/right_face_cut.blcad.json build/right_face_cut.step
```

## Test coverage

Core tests cover:

- `SemanticFace::Right`
- `to_string(SemanticFace::Right) == "right"`
- adding a derived right-face workplane to `PartDocument`
- adding a sketch on that right workplane
- dependency graph edges from base feature to right workplane, right workplane to sketch, and sketch to cut
- JSON roundtrip for right-face workplanes

Geometry tests cover:

- resolving `workplane.base_right`
- checking right origin at `x = width / 2` and `z = thickness / 2`
- checking right normal `(1, 0, 0)`
- checking local axes `local x -> Y` and `local y -> Z`
- checking rectangular bounds `height x thickness`
- mapping a local right-face sketch point into global coordinates
- recomputing a through-all X-axis circular cut from a sketch on the right face
- rejecting an out-of-bounds side-face circle before cutting

## Deliberate limitation

Not included yet:

- front/back faces
- arbitrary planar faces
- face orientation derived from OCCT topology
- full topological naming
- GUI face selection

The right-face path proved the first controlled side-face case. The matching left-face case is documented separately in `docs/left-workplane-mvp2.md`.

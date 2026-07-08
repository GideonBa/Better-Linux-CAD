# MVP 2 Bottom-Face Derived Workplane

Status: minimal bottom-face semantic reference and geometry resolution for a simple additive rectangle extrude.

This document describes the first second-face extension after the original top-face derived workplane path. It is intentionally still narrow: only top and bottom faces of a simple `AdditiveExtrude` are supported.

## Goal

The goal is to support this model path:

```text
feature.base_extrude.bottom
  -> workplane.base_bottom
  -> sketch.bottom_hole
  -> feature.bottom_hole_cut
```

The implementation still avoids raw OCCT face IDs in `PartDocument`.

## Core model

`SemanticFace` now supports:

```text
top
bottom
```

A bottom-face reference is represented as:

```text
SemanticFaceReference
  source_feature = feature.base_extrude
  face = bottom
```

A derived workplane can expose that bottom face:

```text
DerivedWorkplane
  id = workplane.base_bottom
  name = BaseBottomFace
  kind = feature_face
  source_feature = feature.base_extrude
  face = bottom
```

`PartDocument::add_derived_workplane` accepts both top and bottom semantic faces for simple additive extrudes.

## JSON format

The JSON representation is:

```json
{
  "id": "workplane.base_bottom",
  "name": "BaseBottomFace",
  "kind": "feature_face",
  "source_feature": "feature.base_extrude",
  "face": "bottom"
}
```

The serializer writes `bottom`, and the deserializer restores `SemanticFace::Bottom` through the same validated construction path as top-face workplanes.

## Geometry resolution

`WorkplaneResolver` resolves `feature.base_extrude.bottom` as:

```text
origin = (rectangle_center.x, rectangle_center.y, 0)
x_axis = (1, 0, 0)
y_axis = (0, 1, 0)
normal = (0, 0, -1)
```

The rectangular bounds are the same local support region as the top face:

```text
center = (0, 0)
width = source rectangle width
height = source rectangle height
```

For the current reference dimensions:

```text
x range = [-60, 60]
y range = [-40, 40]
```

## Example model

The checked-in example is:

```text
examples/bottom_face_cut.blcad.json
```

It places a circular through-all cut sketch on:

```text
workplane.base_bottom
```

with local center:

```text
(-20, 10)
```

Export command:

```bash
./build/dev-geometry/blcad_export_step examples/bottom_face_cut.blcad.json build/bottom_face_cut.step
```

## Test coverage

Core tests cover:

- `SemanticFace::Bottom`
- `to_string(SemanticFace::Bottom) == "bottom"`
- adding a derived bottom-face workplane to `PartDocument`
- adding a sketch on that bottom workplane
- dependency graph edges from base feature to bottom workplane, bottom workplane to sketch, and sketch to cut
- JSON roundtrip for bottom-face workplanes

Geometry tests cover:

- resolving `workplane.base_bottom`
- checking bottom origin at `z = 0`
- checking bottom normal `(0, 0, -1)`
- checking rectangular bounds
- mapping a local bottom-face sketch point into global coordinates
- recomputing a through-all circular cut from a sketch on the bottom face

## Deliberate limitation

Not included yet:

- side faces
- arbitrary planar faces
- face orientation derived from OCCT topology
- reversed cutter direction based on face normal
- full topological naming
- GUI face selection

The bottom-face path is only the second controlled semantic face case. It exists to prove that the semantic-face approach generalizes beyond a single hard-coded top face before side faces are introduced.

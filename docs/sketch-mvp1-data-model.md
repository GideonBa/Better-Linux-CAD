# MVP 1 Sketch Data Model

Status: pure data model with `PartDocument` integration, no geometry generation.

This document describes the current state of `DatumPlane`, `Sketch`, `RectangleProfile`, and `CircleProfile`.

The current scope is deliberately limited:

- no features
- no extrusion
- no cut
- no recompute
- no OCCT
- no GUI

## Goal

The sketch data model captures the design intent for the first reference plate:

- a fixed XY workplane
- a centered rectangle for the base plate
- a centered circle for the later hole
- references to parameters, but no evaluation into OCCT geometry yet
- document validation for workplane and parameter references

## Types

### `Point2`

A simple 2D point for sketch coordinates.

```text
Point2
  x
  y
```

MVP-1 profiles currently use `center = (0, 0)`.

### `Point3`

A simple 3D point for datum-plane origins.

```text
Point3
  x
  y
  z
```

### `Vector3`

A simple 3D vector for axes and normals.

```text
Vector3
  x
  y
  z
```

## `DatumPlane`

MVP 1 implements only the standard `XY` plane.

```text
DatumPlane XY
  id = "datum.xy"
  name = "XY"
  origin = (0, 0, 0)
  x_axis = (1, 0, 0)
  y_axis = (0, 1, 0)
  normal = (0, 0, 1)
```

Validation:

- datum-plane ID must not be empty
- name must not be empty

Not included yet:

- freely defined planes
- offset planes
- planes derived from face references
- validation of orthogonality or normal length

## `RectangleProfile`

The rectangle profile describes the later base plate.

```text
RectangleProfile
  id = "profile.base_rectangle"
  center = (0, 0)
  width_parameter = "part.width"
  height_parameter = "part.height"
```

Validation:

- profile ID must not be empty
- width parameter ID must not be empty
- height parameter ID must not be empty

The profile itself does not check whether the referenced parameters exist in the `PartDocument`. This check happens when a sketch is added to `PartDocument`.

## `CircleProfile`

The circle profile describes the later centered hole.

```text
CircleProfile
  id = "profile.center_hole"
  center = (0, 0)
  diameter_parameter = "part.hole_diameter"
```

Validation:

- profile ID must not be empty
- diameter parameter ID must not be empty

The profile itself does not check whether the diameter parameter exists in `PartDocument`. This check happens when a sketch is added to `PartDocument`.

The size validation `hole_diameter < min(width, height)` requires access to parameter values and follows later.

## `Sketch`

A sketch stores ID, name, workplane reference, and profiles.

```text
Sketch
  id = "sketch.base"
  name = "Sketch_BaseRectangle"
  workplane = "datum.xy"
  rectangle_profiles
  circle_profiles
```

Validation:

- sketch ID must not be empty
- name must not be empty
- workplane ID must not be empty
- profile IDs must be unique within a sketch
- profile IDs must also be unique across rectangle and circle profiles

Not included yet:

- general sketch constraints
- lines, arcs, or freely editable entities
- automatic profile derivation
- parameter value evaluation
- OCCT conversion

## Integration into `PartDocument`

`PartDocument` now stores:

- parameters
- datum planes
- sketches

When a sketch is added, the document validates that:

- the sketch `workplane` exists as a datum plane in the document
- every `RectangleProfile.width_parameter` exists in the document
- every `RectangleProfile.height_parameter` exists in the document
- every `CircleProfile.diameter_parameter` exists in the document
- sketch IDs are unique in the document

This check ensures that a sketch does not point to non-existing document objects.

## Test coverage

Current tests check:

- XY plane with origin, X axis, Y axis, and normal direction
- datum-plane validation for ID and name
- rectangle profile with width and height parameters
- circle profile with diameter parameter
- profile validation for missing IDs and parameter references
- sketch validation for ID, name, and workplane
- adding rectangle and circle profiles
- unique profile IDs within a sketch
- datum planes in `PartDocument`
- sketches in `PartDocument`
- missing workplanes when adding sketches
- missing width, height, and diameter parameters when adding sketches

## Next integration step

The next integration steps are already done: features have been added as pure data models, and `PartDocument` creates dependency graph nodes and edges from parameter, sketch, and feature references. The invalidation state marks affected sketches and features after a parameter change. The recompute plan lists affected `dirty` nodes in topological order.

The next useful step now sits above this sketch layer:

- evaluate the circle profile for `SubtractiveExtrude`
- read the target shape from the existing geometry `ShapeCache`
- create the centered cut as the next geometry step
- keep OCCT behind the adapter boundary
- do not build a GUI yet

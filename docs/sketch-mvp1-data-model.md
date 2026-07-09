# Sketch Data Model

Status: pure core data model with `PartDocument` integration, primitive profiles, and the first line-based closed-profile sketch path.

This document describes the current state of `DatumPlane`, `Sketch`, `LineSegment`, `RectangleProfile`, `CircleProfile`, and `ClosedProfile`.

The current scope is still deliberately limited:

- no full sketch constraint solver
- no arcs
- no splines
- no automatic profile-region detection
- no multiple contours or inner holes in one profile
- no GUI sketch editor

## Goal

The sketch data model captures design intent for planar sketches:

- a workplane reference
- primitive profiles for fast-path rectangle and circle cases
- explicit line segments for the first general closed-profile path
- ordered closed profiles that reference line-segment IDs
- references to parameters where size is parameterized
- document validation for workplane and parameter references

## Types

### `Point2`

A simple 2D point for sketch coordinates.

```text
Point2
  x
  y
```

### `Point3`

A simple 3D point for datum-plane origins and resolved geometry points.

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

MVP 1 implements only the standard `XY` plane as a fixed datum plane.

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

User-defined construction planes are tracked separately in `docs/construction-geometry-mvp.md`.

## `LineSegment`

A line segment is the first explicit sketch entity.

```text
LineSegment
  id = "line.a"
  start = (0, 0)
  end = (20, 0)
```

Validation:

- line segment ID must not be empty
- start and end must not be identical within tolerance
- line segment IDs must be unique inside one sketch

Line segments are model intent. They are not OCCT edges in the core.

## `RectangleProfile`

The rectangle profile is still supported as a fast-path primitive.

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

The circle profile is still supported as a fast-path primitive.

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

## `ClosedProfile`

A closed profile references ordered line-segment IDs.

```text
ClosedProfile
  id = "profile.triangle"
  line_segments = ["line.a", "line.b", "line.c"]
```

Validation:

- profile ID must not be empty
- at least three line segments are required
- line segment IDs must not be empty
- line segment IDs must be unique inside the closed profile
- referenced line segments must exist in the owning sketch
- line segments must be ordered and connected
- the last segment must close back to the first segment
- non-adjacent line segments must not self-intersect

`ClosedProfile` does not yet support arcs, splines, multiple contours, inner holes, or automatic region detection.

## `Sketch`

A sketch stores ID, name, workplane reference, sketch entities, and profiles.

```text
Sketch
  id = "sketch.triangle"
  name = "Sketch_Triangle"
  workplane = "datum.xy"
  line_segments
  rectangle_profiles
  circle_profiles
  closed_profiles
```

Validation:

- sketch ID must not be empty
- name must not be empty
- workplane ID must not be empty
- line segment IDs must be unique within a sketch
- profile IDs must be unique within a sketch across rectangle, circle, and closed profile types
- closed profiles must reference existing line segments in ordered connected loops

## Integration into `PartDocument`

`PartDocument` stores:

- parameters
- datum planes
- derived workplanes
- sketches
- features

When a sketch is added, the document validates that:

- the sketch `workplane` exists as a datum plane or derived workplane in the document
- every `RectangleProfile.width_parameter` exists in the document
- every `RectangleProfile.height_parameter` exists in the document
- every `CircleProfile.diameter_parameter` exists in the document
- sketch IDs are unique in the document

Line-based closed profiles do not introduce parameter dependencies yet because their current coordinates are explicit sketch-local values.

## Test coverage

Current tests check:

- XY plane with origin, X axis, Y axis, and normal direction
- datum-plane validation for ID and name
- line segment endpoint storage
- line segment validation for missing IDs and zero length
- rectangle profile with width and height parameters
- circle profile with diameter parameter
- closed profile ordered line references
- closed profile validation for too few segments and duplicate references
- sketch validation for ID, name, and workplane
- adding rectangle, circle, and closed profiles
- unique profile IDs within a sketch
- ordered closed-loop validation
- disconnected closed-loop rejection
- self-intersecting closed-loop rejection
- JSON roundtrip for line segments and closed profiles
- geometry recompute for triangle prism and triangle through-all cut

## Next integration step

The next useful sketch-adjacent step is not more line-loop scope. The first line-based closed-profile path is implemented. The next foundational block is construction geometry: user-created construction points, construction lines, and construction planes, so sketches can be placed on explicit user-defined planes instead of only fixed datum planes or semantic generated faces.

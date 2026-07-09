# MVP 2 Bounded Workplane Validation

Status: minimal bounds validation for circle profiles on derived top, bottom, right, left, front, and back workplanes of a simple additive rectangle extrude.

This document describes the validation step after resolving semantic generated faces into concrete workplane frames. The geometry layer knows the rectangular support region of the resolved workplane and validates circle profiles before executing the cut.

## Goal

The goal is to reject invalid cuts before OCCT is asked to create geometry.

For the current MVP-2 seed, the supported bounded faces are:

```text
feature.base_extrude.top
feature.base_extrude.bottom
feature.base_extrude.right
feature.base_extrude.left
feature.base_extrude.front
feature.base_extrude.back
```

The resolved workplane carries a rectangular support region in local workplane coordinates. A circle profile on that workplane is valid only if the complete circle lies inside the rectangle.

## Bounds model

`ResolvedWorkplane` contains:

```text
id
origin
x_axis
y_axis
normal
bounds
```

The current bounds representation is:

```text
RectangularWorkplaneBounds
  enabled
  center
  width_mm
  height_mm
```

Standard datum planes are unbounded:

```text
enabled = false
```

Derived generated-face workplanes are bounded:

```text
enabled = true
center = (0, 0)
```

Top and bottom use source rectangle width and height:

```text
width_mm = source rectangle width
height_mm = source rectangle height
```

Right and left use source rectangle height and extrude thickness:

```text
width_mm = source rectangle height
height_mm = extrude thickness
```

Front and back use source rectangle width and extrude thickness:

```text
width_mm = source rectangle width
height_mm = extrude thickness
```

## Validation rule

For a circle with local center `(cx, cy)` and radius `r`, the circle is valid iff:

```text
cx - r >= min_x
cx + r <= max_x
cy - r >= min_y
cy + r <= max_y
```

A small numeric tolerance is used for equality at the boundary.

For side faces on the current 120 x 80 x 8 mm reference plate, the local thickness range is only `[-4, 4]`. Therefore the side-face examples use 4 mm holes so that the circle can fit inside the 8 mm thickness.

## Recompute integration

`GeometryRecomputeExecutor::execute_subtractive_extrude` performs this sequence:

1. Resolve the sketch workplane.
2. Read the circle profile diameter.
3. If the workplane has rectangular bounds, validate the circle center and radius against those bounds.
4. Map the local circle center into a global center.
5. Execute `CircularCutAdapter` along the resolved workplane normal.

If validation fails, recompute returns:

```text
ErrorCategory::Validation
object_id = profile id
message = circle profile must lie fully inside resolved workplane bounds
```

The already computed base shape remains in the `ShapeCache`, but no new cut shape is stored. During incremental recompute, stale cut shapes are removed before the dirty cut feature is executed.

## Test coverage

Geometry tests cover:

- `datum.xy` has no rectangular bounds
- top and bottom workplanes have rectangular bounds matching source width and height
- right and left workplanes have rectangular bounds matching source height and thickness
- front and back workplanes have rectangular bounds matching source width and thickness
- centered and off-center holes recompute
- valid near-edge holes are accepted
- out-of-bounds holes are rejected
- after rejection, the base feature shape remains cached and the cut feature shape is not cached

## Deliberate limitation

Not included yet:

- arbitrary planar faces
- non-rectangular faces
- partial-overlap clipping
- automatic sketch repair
- full topological naming
- GUI face selection
- general closed sketch profiles

This step keeps validation in the geometry layer, because the core stores semantic model intent and does not own geometric face extents.

# MVP 2 Bounded Workplane Validation

Status: minimal bounds validation for circle profiles on derived top and bottom workplanes of a simple additive rectangle extrude.

This document describes the validation step after resolving semantic generated faces into concrete workplane frames. The geometry layer knows the rectangular support region of the resolved workplane and validates circle profiles before executing the cut.

## Goal

The goal is to reject invalid cuts before OCCT is asked to create geometry.

For the current MVP-2 seed, the supported bounded faces are:

```text
feature.base_extrude.top
feature.base_extrude.bottom
```

The resolved workplane carries a rectangular support region:

```text
center = (0, 0)
width  = part.width
height = part.height
```

A circle profile on that workplane is valid only if the complete circle lies inside the rectangle.

## Data model

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

Derived top and bottom workplanes are bounded:

```text
enabled = true
center = (0, 0)
width_mm = source rectangle width
height_mm = source rectangle height
```

The bounds are expressed in local workplane coordinates. This means that for the reference plate:

```text
x range = [-60, 60]
y range = [-40, 40]
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

Example valid near-edge circle:

```text
center = (50, 30)
radius = 10
x range touched: 40..60
y range touched: 20..40
```

Example invalid circle:

```text
center = (55, 0)
radius = 10
x range touched: 45..65
```

The second circle exceeds the right face boundary by 5 mm.

## Recompute integration

`GeometryRecomputeExecutor::execute_subtractive_extrude` performs this sequence:

1. Resolve the sketch workplane.
2. Read the circle profile diameter.
3. If the workplane has rectangular bounds, validate the circle center and radius against those bounds.
4. Map the local circle center into a global center.
5. Execute `CircularCutAdapter`.

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
- `workplane.base_top` has rectangular bounds matching source width and height
- `workplane.base_bottom` has rectangular bounds matching source width and height
- a centered hole still recomputes
- off-center top and bottom holes recompute
- a near-edge hole at `(50, 30)` with diameter `20` is accepted
- an out-of-bounds hole at `(55, 0)` with diameter `20` is rejected
- after rejection, the base feature shape remains cached and the cut feature shape is not cached

## Deliberate limitation

Not included yet:

- side-face bounds
- arbitrary planar faces
- non-rectangular faces
- partial-overlap clipping
- automatic sketch repair
- full topological naming
- GUI face selection

This step keeps validation in the geometry layer, because the core stores semantic model intent and does not own geometric face extents.

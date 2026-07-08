# MVP 2 Workplane Resolver

Status: geometry-layer resolver for derived top and bottom workplanes of a simple additive extrude, including rectangular bounds.

This document describes the resolver after introducing `SemanticFaceReference` and `DerivedWorkplane` in the core model. The core still stores semantic model intent only. The geometry layer resolves that semantic workplane into a concrete frame for recompute.

## Goal

The goal is to make these paths geometrically meaningful:

```text
feature.base_extrude.top
  -> workplane.base_top
  -> sketch.top_hole local point
  -> global cut center
```

```text
feature.base_extrude.bottom
  -> workplane.base_bottom
  -> sketch.bottom_hole local point
  -> global cut center
```

The resolver proves that sketches on generated planar faces can be evaluated without storing raw OCCT face IDs in `PartDocument`.

## Public interface

Header:

```text
include/blcad/geometry/workplane_resolver.hpp
```

Main types:

```text
ResolvedWorkplane
RectangularWorkplaneBounds
WorkplaneResolver
```

A resolved workplane contains:

```text
id
origin
x_axis
y_axis
normal
bounds
```

The resolver exposes:

```text
resolve(document, workplane_id)
evaluate_point(workplane, local_point)
```

## Supported workplanes

### Standard datum plane

For `datum.xy`, the resolver returns the stored datum-plane frame:

```text
origin = (0, 0, 0)
x_axis = (1, 0, 0)
y_axis = (0, 1, 0)
normal = (0, 0, 1)
bounds.enabled = false
```

### Derived top-face workplane

For:

```text
workplane.base_top -> feature.base_extrude.top
```

the resolver validates that:

- the source feature exists
- the source feature is an `AdditiveExtrude`
- the source sketch exists
- the source sketch has exactly one rectangle profile
- the rectangle width and height parameters exist
- the additive extrude thickness parameter exists

Then it returns the top-face frame:

```text
origin = (rectangle_center.x, rectangle_center.y, thickness)
x_axis = (1, 0, 0)
y_axis = (0, 1, 0)
normal = (0, 0, 1)
```

### Derived bottom-face workplane

For:

```text
workplane.base_bottom -> feature.base_extrude.bottom
```

the resolver uses the same source validation and returns the bottom-face frame:

```text
origin = (rectangle_center.x, rectangle_center.y, 0)
x_axis = (1, 0, 0)
y_axis = (0, 1, 0)
normal = (0, 0, -1)
```

The negative normal records that this is the bottom face. The current through-all cut still uses the global X/Y center and remains vertical.

## Rectangular bounds

Top and bottom derived workplanes both contain a rectangular bounds object:

```text
bounds.enabled = true
bounds.center = (0, 0)
bounds.width_mm = source rectangle width
bounds.height_mm = source rectangle height
```

For the current reference dimensions:

```text
bounds.width_mm = 120
bounds.height_mm = 80
```

## Recompute integration

`GeometryRecomputeExecutor::execute_subtractive_extrude` resolves the input sketch workplane before executing the circular cut.

Before mapping the point into global coordinates, the executor validates the circle radius against rectangular bounds when bounds are enabled.

Then the point is mapped into global coordinates:

```text
global = origin + local.x * x_axis + local.y * y_axis
```

For `workplane.base_top` and local `(25, -10)`, this becomes:

```text
global = (25, -10, 8)
```

For `workplane.base_bottom` and local `(-20, 10)`, this becomes:

```text
global = (-20, 10, 0)
```

The current circular cut adapter still performs a through-all vertical cut. It uses the resolved global X/Y center. The resolved Z value proves workplane placement, even though the current through-all cutter spans the target's full Z bounds.

## Bounds validation

Bounds validation is documented in:

```text
docs/bounded-workplane-validation-mvp2.md
```

The current validation rejects a circle profile if its local center and radius do not fit fully inside the rectangular bounds.

## Example models

Top-face example:

```text
examples/top_face_cut.blcad.json
```

Bottom-face example:

```text
examples/bottom_face_cut.blcad.json
```

Export commands:

```bash
./build/dev-geometry/blcad_export_step examples/top_face_cut.blcad.json build/top_face_cut.step
./build/dev-geometry/blcad_export_step examples/bottom_face_cut.blcad.json build/bottom_face_cut.step
```

## Test coverage

Geometry tests cover:

- resolving `datum.xy`
- resolving `workplane.base_top`
- resolving `workplane.base_bottom`
- checking that the top-face origin is at `z = thickness`
- checking that the bottom-face origin is at `z = 0`
- checking top normal `+Z` and bottom normal `-Z`
- checking that `datum.xy` is unbounded
- checking that top and bottom derived workplanes have rectangular bounds
- mapping top and bottom local sketch points through the resolved frames
- rejecting missing workplanes
- recomputing circular cuts from sketches on top and bottom derived workplanes
- accepting a near-edge circle that still lies inside bounds
- rejecting an out-of-bounds circle before executing the cut
- verifying that the final volume matches the expected removed cylinder volume within tolerance

## Deliberate limitation

Not included yet:

- right/left/front/back side faces
- arbitrary planar faces
- side-face coordinate conventions
- cutter direction derived from face normal
- non-rectangular source faces
- storing or matching raw OCCT face IDs
- GUI face selection

The resolver is intentionally limited to the top and bottom faces of a simple additive rectangle extrusion. This keeps MVP 2 incremental and avoids prematurely building a full topological-naming system.

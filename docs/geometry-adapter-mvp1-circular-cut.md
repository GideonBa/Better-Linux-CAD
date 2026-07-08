# Geometry Adapter: Circular Cut

Status: optional OCCT adapter for vertical and principal-axis through-all circular cuts.

This document describes the circular-cut geometry path. The adapter cuts a through-hole from an already computed solid body. It corresponds to the current `SubtractiveExtrude` with `through_all`.

## Goal

This step places the Boolean cut behind the same core/OCCT boundary as rectangle extrusion:

- `blcad_core` remains free of OCCT headers and OCCT linking.
- `blcad_geometry` encapsulates the concrete OCCT Boolean cut.
- The adapter works on a `GeometryShape` and returns a `GeometryShape`.
- The adapter does not read a `PartDocument` or a `ShapeCache`; that is handled by `GeometryRecomputeExecutor`.

## Public interface

The public header is:

```text
include/blcad/geometry/circular_cut_adapter.hpp
```

Current operations:

```text
cut_circular_hole(target, diameter, center)
cut_circular_hole_along_axis(target, diameter, center, axis_direction)
```

`cut_circular_hole` is the original convenience method for vertical Z-axis cuts. `cut_circular_hole_along_axis` supports principal-axis through-all cuts and is used by derived workplanes such as the right `+X` face.

## Operation

Rules:

- the target body `target` must be a non-empty `GeometryShape`
- the diameter arrives as a validated `Quantity` and must be greater than `0`
- the cut passes through the entire body (`through_all`)
- the cutting geometry is an OCCT cylinder
- axis-directed cuts currently accept only principal axes: X, Y, or Z
- the result must be a non-empty OCCT shape
- expected OCCT errors are reported as `ErrorCategory::Geometry`

## Through-all model

The adapter reads the target body's OCCT bounding box (`BRepBndLib`). The cutting cylinder is extended beyond the relevant axis bounds by a fixed overhang. This keeps the cut cleanly `through_all`, even with numerical tolerances at face boundaries.

For the current MVP-2 right-face workplane, `GeometryRecomputeExecutor` passes the resolved workplane normal `(1, 0, 0)`, producing an X-axis through-all side cut.

## Shape diagnostics

`ShapeSummary` and `RectangleExtrusionAdapter::summarize` provide `volume_mm3`. The volume is computed through `BRepGProp::VolumeProperties`. This allows tests to verify that the cut reduces the volume compared with the full body without a direct OCCT dependency.

## Test coverage

Current geometry tests check:

- a centered vertical cut creates a non-empty shape with exactly one solid
- the volume after the cut is smaller than the volume of the full body
- an empty target body is rejected as a geometry error
- a right-face derived workplane produces an X-axis through-all cut

## Deliberate limitation

Not included yet:

- arbitrary non-principal cut axes
- limited cut depth instead of `through_all`
- bolt circles or patterns
- GUI

The adapter remains intentionally small. It supports the principal-axis face cases needed by the current semantic workplane MVP without becoming a full feature engine.

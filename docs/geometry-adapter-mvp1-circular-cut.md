# MVP 1 Geometry Adapter: Centered Cut

Status: optional OCCT adapter for the centered hole cut.

This document describes the second geometry path for MVP 1. The adapter cuts a centered through-hole from an already computed solid body. It corresponds to the MVP-1 `SubtractiveExtrude` with `through_all`.

## Goal

This step places the Boolean cut behind the same core/OCCT boundary as rectangle extrusion:

- `blcad_core` remains free of OCCT headers and OCCT linking.
- `blcad_geometry` encapsulates the concrete OCCT Boolean cut.
- The adapter works on a `GeometryShape` and returns a `GeometryShape`.
- The adapter does not read a `PartDocument` or a `ShapeCache`; that is handled by `GeometryRecomputeExecutor`.

## CMake target

The adapter lives in the optional target `blcad_geometry` and is built only with the geometry preset:

```bash
cmake --preset dev-geometry
cmake --build --preset dev-geometry
ctest --preset dev-geometry
```

The Boolean cut additionally requires the OCCT toolkits `TKBO` and `TKBool`.

## Public interface

The public header is:

```text
include/blcad/geometry/circular_cut_adapter.hpp
```

Current type:

- `CircularCutAdapter`: OCCT adapter for the centered cut

Current operation:

```text
cut_circular_hole(target, diameter, center)
```

`GeometryShape` remains an opaque handle. Its internal OCCT backing is shared with the adapters only through the non-public header `src/geometry/geometry_shape_internal.hpp`. `CircularCutAdapter`, like `RectangleExtrusionAdapter`, is declared as a `friend` of `GeometryShape`.

## Operation

Currently implemented:

```text
cut_circular_hole(target, diameter, center)
```

Rules:

- the target body `target` must be a non-empty `GeometryShape`
- the diameter arrives as a validated `Quantity` and must be greater than `0`
- the circle is centered around `(0, 0)` in the XY plane by default
- the cut passes through the entire body (`through_all`)
- the cutting geometry is a cylinder in `+Z` direction
- the result must be a non-empty OCCT shape
- expected OCCT errors are reported as `ErrorCategory::Geometry`

## Through-all model

The cut does not directly know the target body's thickness. Instead of passing a thickness value, the adapter reads the target body's Z extent from its OCCT bounding box (`BRepBndLib`). The cutting cylinder is extended beyond these bounds by a fixed overhang. This keeps the cut cleanly `through_all`, even with numerical tolerances at the top and bottom faces.

## Shape diagnostics

`ShapeSummary` and `RectangleExtrusionAdapter::summarize` were extended with `volume_mm3`. The volume is computed through `BRepGProp::VolumeProperties`. This allows tests to verify that the cut reduces the volume compared with the full body without a direct OCCT dependency.

## Test coverage

Current geometry tests check:

- a centered cut creates a non-empty shape with exactly one solid
- the volume after the cut is smaller than the volume of the full body
- an empty target body is rejected as a geometry error

## Deliberate limitation

Not included yet:

- non-centered or multiple holes
- limited cut depth instead of `through_all`
- cut direction other than `+Z`
- bolt circles or patterns
- STEP export
- GUI

## Next useful step

After additive and subtractive execution, only STEP export of the final shape from the `ShapeCache` is missing for the first vertical MVP-1 line. The `PartDocument` remains the source of model intent, not the OCCT shape.

# MVP 1 Geometry Adapter: Rectangle Extrusion

Status: optional OCCT adapter, not integrated into recompute yet.

This document describes the first small geometry path for MVP 1. The adapter creates an OCCT solid for a centered rectangular plate from three already validated `Quantity` values.

## Goal

This step tests the technical boundary between the core and OCCT:

- `blcad_core` remains free of OCCT headers and OCCT linking.
- `blcad_geometry` encapsulates concrete OCCT generation.
- Core tests continue to run without the geometry target.
- Geometry tests are built only with `BLCAD_BUILD_GEOMETRY=ON`.

The adapter is not feature recompute yet. It does not read a `PartDocument` and does not execute a `RecomputePlan`. A separate `ShapeCache` in the same geometry target can store computed shapes.

## CMake target

The target is named:

```text
blcad_geometry
```

It is built only when this CMake option is active:

```text
BLCAD_BUILD_GEOMETRY=ON
```

The default preset `dev` leaves this option disabled. The geometry path has its own preset:

```bash
cmake --preset dev-geometry
cmake --build --preset dev-geometry
ctest --preset dev-geometry
```

## Public interface

The public header is:

```text
include/blcad/geometry/rectangle_extrusion_adapter.hpp
```

Current types:

- `GeometryShape`: opaque handle for computed geometry
- `ShapeSummary`: small test and diagnostic summary
- `RectangleExtrusionAdapter`: first OCCT adapter

`GeometryShape` contains no public OCCT types. This allows `ShapeCache` to hold geometry without mixing the core with OCCT.

## Operation

Currently implemented:

```text
make_extruded_rectangle(width, height, thickness)
```

Rules:

- width, height, and thickness arrive as validated `Quantity` values.
- the rectangle is centered around `(0, 0)` in the XY plane.
- extrusion direction is fixed to `+Z`.
- the result must be a non-empty OCCT shape.
- expected OCCT errors are reported as `ErrorCategory::Geometry`.

## Test coverage

Current geometry tests check:

- positive lengths create a non-empty shape
- rectangle extrusion creates exactly one solid
- a default-constructed `GeometryShape` is empty

## Deliberate limitation

Not included yet:

- execution of `AdditiveExtrude`
- execution of `SubtractiveExtrude`
- centered hole cut
- `SubtractiveExtrude` cut
- STEP export
- GUI

## Next useful step

After this adapter, the first `ShapeCache`, and additive execution, the next step should prepare the centered cut for `SubtractiveExtrude`. The `PartDocument` remains the source of model intent, not the OCCT shape.

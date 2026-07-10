# Sketch-plane extrude direction seed

This document records the first cleanup of feature direction semantics.

The important rule is that feature direction is model intent relative to the owning sketch workplane, not a raw global world axis.

## Implemented scope

`ExtrudeDirection` now has explicit sketch-plane semantics:

```text
sketch_normal
opposite_sketch_normal
```

The previous `+Z` JSON spelling is accepted as a legacy alias for `sketch_normal`.

## Recompute behavior

`GeometryRecomputeExecutor` resolves the owning sketch workplane first and then maps the feature direction to a 3D vector:

- `sketch_normal` uses `ResolvedWorkplane::normal`
- `opposite_sketch_normal` uses the negative resolved workplane normal

This direction is used consistently for supported additive and subtractive profile operations.

Rectangle-profile additive extrudes no longer bypass the sketch workplane through the old global rectangle adapter path. They are converted into local rectangle vertices, mapped through the resolved workplane frame, and extruded along the resolved feature direction.

## Arbitrary construction-plane orientation

The first seed supports arbitrary valid construction-plane frames for supported profile operations. This includes explicit construction planes and relation-driven planes such as three-point planes, provided the `WorkplaneResolver` can resolve them to an origin, x axis, y axis, and normal.

Through-all cut prisms and circular cut cylinders no longer require a principal X/Y/Z axis. The through-all extent is computed by projecting the target bounding box onto the normalized cut axis and adding a small margin.

## Deliberate limitations

This does not implement arbitrary generated topology. Derived face workplanes are still limited to the current rectangular-additive-extrude semantic face seed.

This also does not add draft angles, two-sided extrudes, asymmetric distances, start offsets, up-to-next/up-to-face end conditions, revolved features, sweep, loft, or 3D sketch features.

## Test coverage

The geometry tests cover:

- additive closed-profile extrude on a slanted explicit construction plane
- additive rectangle-profile extrude through the sketch-aware workplane path
- subtractive spline/curve cuts using the resolved sketch normal path

The core tests cover:

- default feature direction as `sketch_normal`
- explicit `opposite_sketch_normal` storage
- JSON support for `sketch_normal`, `opposite_sketch_normal`, and legacy `+Z`

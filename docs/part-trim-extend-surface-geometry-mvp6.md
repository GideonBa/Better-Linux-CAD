# Trim and Extend Surface Geometry MVP-6

Status: implemented in Block 90.

## Scope

Block 90 executes the persistent `TrimSurfaceFeature` and `ExtendSurfaceFeature` contracts from
Block 88. Targets may be `BodyKind::Surface` results or supported semantic planar faces. Boundary
and profile references remain model authority; OCCT face and edge identities remain cache-only.

## Trim contract

Trim resolves exactly one target face and exactly one closed planar trimming contour. The contour
may come from a closed PathCurve or a supported sketch `ProfileRegionReference` (rectangle,
circle, line-based closed profile, or arc/spline closed profile). The deterministic result is the
single connected intersection inside that contour.

There is intentionally no inferred keep-side choice. Open contours, non-planar contours, missing
targets, zero/multiple target faces, and empty, disconnected, or multi-face intersections fail
closed as ambiguous.

## Extend contract

Extend resolves one linear PathCurve or semantic-edge boundary that uniquely matches an outer edge
of a single geometrically planar Surface face. The positive Length parameter moves that edge
outward in the face plane and rebuilds one polygonal face. The current Block-90 execution rejects
curved boundaries, inner loops, multi-face targets, and non-planar targets rather than inventing
topology or an extension direction.

## Recompute and recovery

Semantic planar-face targets are recovered from current producer geometry on every execution.
PathCurve and profile inputs are likewise re-resolved after parameter changes. Target resolution,
trim/extend execution, validation, and feature/Surface-Body cache publication are transactional.
Successful results contain faces and no solid and never become the legacy single-solid
`final_shape`.

## Persistence

Block 90 adds no JSON fields. Existing `surface_features`, semantic Surface/trimming references,
PathCurves, profile regions, distance parameters, and result Body ids contain all persistent
intent. Generated intersection, boundary, and output faces are never serialized.

## Focused proof

```bash
./build/blcad_geometry_tests "[geometry][surface-trim-extend]"
```

The proof covers semantic-face trimming and recovery after an upstream dimension change,
parameter-driven boundary recovery for Extend, Surface-Body publication, and closed failure for an
ambiguous multi-face target.

## Handoff

Blocks 48–93 are implemented. Stitch/Knit/Sew shell and Closed-shell-to-solid execution are
canonical in `docs/part-surface-stitch-geometry-mvp6.md` and
`docs/part-closed-shell-to-solid-geometry-mvp6.md`. Block 93 multi-body STEP export and deterministic body naming is implemented; Block 94
integrated Part Construction MVP acceptance is the current next technical step.

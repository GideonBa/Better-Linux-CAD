# Boundary and Fill Surface Geometry MVP-6

Status: implemented in Block 89.

## Scope

Block 89 executes the persistent `BoundarySurfaceFeature` and `FillSurfaceFeature` contracts from
Block 88. The Core model and JSON schema are unchanged: OCCT edges, wires, faces, and topology
identity remain transient Geometry/cache products.

Supported boundary sources are:

- planar sketch lines, arcs, splines, and bounded reference-generated lines;
- 3D-sketch lines, polylines, arcs, splines, and helices;
- bounded construction lines;
- semantic generated edges that resolve to the currently supported linear edge capability.

An unbounded projected line is rejected. PathCurve ordering, reversal, closure, and connection
tolerance remain the persistent authority; the Geometry layer resolves them deterministically.

## Geometry contract

`BoundarySurfaceFeature` accepts its frozen two-to-four-boundary contract. Two independent open
boundaries produce an open lofted surface. Three or four ordered boundaries must form one closed
connected contour and are passed to OCCT filling. `FillSurfaceFeature` requires all ordered input
boundaries together to form one closed connected contour.

Every successful operation must produce at least one valid face and no solid. The result is
published under both the Surface feature and its `BodyKind::Surface` result Body. Surface results
never become the legacy single-solid `final_shape`.

Boundary resolution, OCCT execution, validation, and cache publication are transactional. Missing,
unbounded, disconnected, open, invalid, ambiguous, or unsupported inputs fail without publishing a
partial feature or Body shape. Trim, Extend, Stitch, and closed-shell-to-Solid execution remain
deferred to Blocks 90–92.

## Persistence

Block 89 adds no JSON fields. The existing `surface_features`, `path_curves`, semantic references,
and result Body ids contain all persistent intent. Generated wires and faces are recomputed and are
never serialized.

## Focused proof

```bash
./build/blcad_geometry_tests "[geometry][surface-boundary-fill]"
```

The proof covers open two-boundary and non-planar four-boundary surfaces, a supported closed planar
fill, Surface-Body cache publication, the absence of a solid/final-shape result, and transactional
rejection of an open contour.

## Handoff

Blocks 48–93 are implemented. Trim/Extend, Stitch, and Closed-shell-to-solid execution are canonical
in `docs/part-trim-extend-surface-geometry-mvp6.md`,
`docs/part-surface-stitch-geometry-mvp6.md`, and
`docs/part-closed-shell-to-solid-geometry-mvp6.md`. Block 93 multi-body STEP export and deterministic body naming is implemented; Block 94
integrated Part Construction MVP acceptance is the current next technical step.

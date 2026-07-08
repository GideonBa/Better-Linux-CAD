# MVP Plan

## MVP 1: Single-part modeling

Goal: one single parametric part.

Detailed document: `docs/mvp-1-specification.md`

- part document
- parameters with units
- simple 2D sketch on a standard plane
- rectangle, circle, line
- simple dimensions
- extrude
- cut
- STEP export
- JSON serialization of model intent

Example: rectangular plate with width, height, thickness, and a centered hole.

Current state: MVP 1 now has the required core data models, dependency graph, invalidation state, recompute plan, numeric parameter updates, JSON serialization of model intent, optional OCCT geometry execution, centered cut, full document recompute, and STEP export. A document restored from JSON can be recomputed into a fresh `ShapeCache` and exported again.

Immediate next step before MVP 2: add filesystem-level `.blcad.json` read/write helpers, a checked-in reference model under `examples/`, and a small non-GUI command-line example that loads the JSON model and exports STEP.

## MVP 2: Sketch on planar face

Goal: place sketches on existing planar faces.

- select face
- create workplane from face
- start sketch on workplane
- cut through existing body

## MVP 3: Parametric bolt circle

Goal: first meaningful parametric feature test.

- bolt circle radius as parameter
- hole count as parameter
- hole diameter as parameter
- automatic recompute on change

## MVP 4: Simple assembly parameters

Goal: cross-part parametrization.

- assembly document
- two part documents in one assembly
- shared assembly parameter
- both parts use the same bolt-circle parameter

## MVP 5: Simple assembly constraints

Goal: place parts through constraints.

- component instances
- axis-to-axis
- face-to-face
- simple screw placement

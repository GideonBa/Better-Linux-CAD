# MVP Plan

## MVP 1: Single-part modeling

Goal: one single parametric part with a headless file-based export path.

Detailed document: `docs/mvp-1-specification.md`

Implemented scope:

- part document
- parameters with units
- simple 2D sketch on a standard plane
- rectangle and circle profiles
- simple dimensions through parameters
- additive extrude
- subtractive through-all cut
- dependency graph
- invalidation state
- recompute plan
- numeric parameter update
- optional OCCT geometry execution
- ShapeCache as computed result
- STEP export
- JSON serialization of model intent
- `.blcad.json` file read/write helpers
- checked-in reference model under `examples/`
- headless JSON-to-STEP command-line example

Example: rectangular plate with width, height, thickness, and a centered hole.

Current state: MVP 1 has the required core data models, dependency graph, invalidation state, recompute plan, numeric parameter updates, JSON serialization of model intent, `.blcad.json` file workflow, optional OCCT geometry execution, centered cut, full document recompute, and STEP export. The checked-in `examples/reference_plate.blcad.json` model can be loaded by `blcad_export_step`, recomputed into a fresh `ShapeCache`, and exported as STEP.

## MVP 2: Sketch on planar face

Goal: place sketches on existing generated planar faces.

Implemented seed:

- semantic reference for the top face of `feature.base_extrude`
- semantic reference for the bottom face of `feature.base_extrude`
- `DerivedWorkplane` from those semantic faces
- sketches can reference derived workplanes
- dependency graph includes paths such as `feature.base_extrude -> workplane.base_top -> sketch.top_hole`
- JSON serialization supports `derived_workplanes` with `top` and `bottom` faces
- checked-in `examples/top_face_cut.blcad.json`
- checked-in `examples/bottom_face_cut.blcad.json`
- geometry-layer `WorkplaneResolver`
- top-face frame resolved from source rectangle sketch and thickness parameter
- bottom-face frame resolved from source rectangle sketch at `z = 0` with negative normal
- subtractive recompute evaluates circle centers through the resolved workplane
- rectangular bounds on resolved top and bottom workplanes
- near-edge valid hole test and out-of-bounds invalid hole test
- incremental recompute through derived-workplane dependencies
- stale dirty feature shapes are removed before incremental recompute
- shrinking the source rectangle can invalidate an existing top-face hole

Next narrow step:

- add one side-face enum value, preferably `SemanticFace::Right`, for the `+X` face
- allow `DerivedWorkplane` to reference `feature.base_extrude.right`
- resolve the right face with a clear frame and rectangular bounds
- define local side-face coordinates consistently
- add JSON roundtrip coverage for the right-face workplane
- add geometry recompute coverage for a through-all circular cut from the right face
- keep support limited to top, bottom, and right faces of simple additive extrudes
- do not add all side faces yet
- do not build a full topological naming system yet
- do not build a GUI yet

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

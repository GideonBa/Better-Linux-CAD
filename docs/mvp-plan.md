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

Current state: MVP 1 now has the required core data models, dependency graph, invalidation state, recompute plan, numeric parameter updates, JSON serialization of model intent, `.blcad.json` file workflow, optional OCCT geometry execution, centered cut, full document recompute, and STEP export. The checked-in `examples/reference_plate.blcad.json` model can be loaded by `blcad_export_step`, recomputed into a fresh `ShapeCache`, and exported as STEP.

## MVP 2: Sketch on planar face

Goal: place sketches on existing generated planar faces.

The first implementation should stay narrow:

- expose a minimal semantic reference for the top face of `feature.base_extrude`
- create a derived workplane from that semantic face
- allow a sketch to reference the derived workplane
- execute a cut from that sketch through the existing body
- keep the first implementation limited to simple extrude-generated planar faces
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

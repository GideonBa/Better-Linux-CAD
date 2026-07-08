# MVP Plan

Source: `zielarchitektur-parametrisches-cad-system.tex`.

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

Example: rectangular plate with width, height, thickness, and a centered hole.

Current state: the core contains the required data models up to the recompute plan. An optional `blcad_geometry` target already creates a centered rectangle extrusion as an OCCT solid and provides a small `ShapeCache`. `AdditiveExtrude` can already be executed for one rectangle profile from a recompute plan. Centered cut, full document recompute, STEP export, and numeric parameter updates are also implemented in the optional geometry path.

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

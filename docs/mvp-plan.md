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

Goal: place sketches on existing generated planar faces while preserving semantic model intent instead of storing raw OCCT face IDs.

Implemented seed:

- semantic reference for the top face of `feature.base_extrude`
- semantic reference for the bottom face of `feature.base_extrude`
- semantic reference for the right `+X` face of `feature.base_extrude`
- semantic reference for the left `-X` face of `feature.base_extrude`
- semantic reference for the front `+Y` face of `feature.base_extrude`
- semantic reference for the back `-Y` face of `feature.base_extrude`
- `DerivedWorkplane` from those semantic faces
- sketches can reference derived workplanes
- dependency graph includes paths such as `feature.base_extrude -> workplane.base_back -> sketch.back_hole`
- JSON serialization supports `derived_workplanes` with `top`, `bottom`, `right`, `left`, `front`, and `back` faces
- checked-in `examples/top_face_cut.blcad.json`
- checked-in `examples/bottom_face_cut.blcad.json`
- checked-in `examples/right_face_cut.blcad.json`
- checked-in `examples/left_face_cut.blcad.json`
- checked-in `examples/front_face_cut.blcad.json`
- checked-in `examples/back_face_cut.blcad.json`
- geometry-layer `WorkplaneResolver`
- top-face frame resolved from source rectangle sketch and thickness parameter
- bottom-face frame resolved from source rectangle sketch at `z = 0` with negative normal
- right-face frame resolved at `x = width / 2` with normal `+X`
- left-face frame resolved at `x = -width / 2` with normal `-X`
- front-face frame resolved at `y = height / 2` with normal `+Y`
- back-face frame resolved at `y = -height / 2` with normal `-Y`
- subtractive recompute evaluates circle centers through the resolved workplane
- axis-directed through-all circular cuts
- rectangular bounds on resolved top, bottom, right, left, front, and back workplanes
- near-edge valid hole test and out-of-bounds invalid hole test
- incremental recompute through derived-workplane dependencies
- stale dirty feature shapes are removed before incremental recompute
- shrinking the source rectangle can invalidate an existing top-face hole

Next narrow step after MVP 2 has been completed: move to the line-based closed-profile sketch block below while keeping the controlled six-face seed stable.

## Implemented block: Line-based general closed sketch profiles

Goal: support a first general closed-profile path instead of only rectangle and circle profile primitives.

Detailed document: `docs/general-closed-sketch-profile-mvp.md`

Implemented scope:

- `SketchEntityId`
- `LineSegment`
- `ClosedProfile`
- ordered line-segment references
- validation of closed and connected line loops
- rejection of duplicate line references inside one profile
- rejection of self-intersecting loops
- JSON serialization and roundtrip tests for `line_segments` and `closed_profiles`
- OCCT wire and face creation from ordered line vertices
- additive extrude from one closed profile
- subtractive through-all extrude from one closed profile
- geometry tests for a non-rectangular triangle prism
- geometry tests for a non-circular triangle cut
- checked-in `examples/triangle_prism.blcad.json`
- checked-in `examples/triangle_cut_plate.blcad.json`

Still not implemented in this block:

- arcs
- splines
- trimmed curves
- automatic region detection from unordered curves
- multiple contours in one feature
- inner holes in the same profile
- full sketch constraint solver
- GUI sketch editing

This block proves what shape a single planar sketch can describe. It does not yet solve arbitrary sketch placement; that is the construction-geometry block below.

## Implemented block: Explicit construction geometry

Goal: support user-created construction planes, construction lines/axes, and construction points that can be placed explicitly in 3D and used as stable sketch/reference geometry.

Detailed document: `docs/construction-geometry-mvp.md`

Implemented scope:

- `ConstructionPointId`, `ConstructionLineId`, and `ConstructionPlaneId`
- `ConstructionPoint`, `ConstructionLine`, and `ConstructionPlane` core model types
- explicit placement definitions for point, line, and plane
- `PartDocument` storage, validation, and dependency graph nodes
- optional `parameter_dependencies` from construction geometry to parameters
- sketches can reference construction planes as workplanes
- JSON serialization and roundtrip tests for explicit construction geometry
- `WorkplaneResolver` resolves construction planes into `ResolvedWorkplane` frames
- geometry test for a feature driven by a closed-profile sketch on a construction plane
- validation for invalid construction geometry such as zero-length lines and degenerate or non-orthonormal plane frames
- invalidation tests proving parameter changes can mark construction geometry, dependent sketches, and dependent features dirty
- checked-in `examples/construction_plane_prism.blcad.json`

This block remains the stable fallback path for relation-driven construction geometry.

## Implemented block: Relation-driven construction geometry

Goal: extend the explicit construction-geometry path with stable relation-driven definitions.

Detailed document: `docs/construction-geometry-mvp.md`

Implemented scope:

- `ConstructionRelationId`
- `ConstructionRelation`
- `ConstructionRelationType`
- `ConstructionLineKind`
- `ConstructionPlaneKind`
- `PlaneOffsetFromPlane` relation-driven construction plane
- `LineThroughTwoPoints` relation-driven construction line
- `PlaneThroughThreePoints` relation-driven construction plane
- collinearity validation for `PlaneThroughThreePoints`
- dependency graph edges from referenced construction objects to relation-driven construction objects
- dependency graph edges from source workplanes and offset parameters to offset construction planes
- JSON serialization and roundtrip tests for relation-driven construction geometry
- `WorkplaneResolver` support for offset construction planes
- `WorkplaneResolver` support for three-point construction planes
- geometry tests for closed profiles sketched on offset construction planes
- invalidation tests proving parameter changes can mark relation-driven construction planes and dependent sketches dirty
- explicit construction geometry kept as the stable fallback path

Still not implemented in this block:

- generated semantic edge and vertex references
- point-on-line, point-on-plane, and line-on-plane relations
- plane-parallel-through-point and line-parallel-through-point relations
- parallel, orthogonal, tangent, angle, or normal construction relations
- standalone relation collections independent from construction objects
- GUI manipulators
- full construction-geometry solver
- 3D sketch splines, sweep, loft, surface stitching, or closed-shell-to-solid conversion

This block continues to answer where a sketch can live. The line-based closed sketch profile block answers what planar shape a sketch can describe. They remain separate, testable increments.

## Next MVP: Semantic references and chained construction relations

Goal: generalize the construction-relation seed into a semantic reference system for generated edges, generated vertices, and chained construction relations while keeping raw OCCT topology out of saved model intent.

Proposed first implementation sequence:

- add semantic generated edge and vertex reference IDs similar to `SemanticFaceReference`
- add relation validation for generated edge/vertex references without storing raw OCCT topology IDs
- add `PointOnPlane`, `PointOnLine`, and `LineOnPlane` relation definitions
- add `PlaneParallelToPlaneThroughPoint` and `LineParallelToLineThroughPoint` relation definitions
- add dependency graph tests for chained construction relations
- add JSON roundtrip tests for chained relations and semantic generated references
- extend `WorkplaneResolver` only for relations that can be evaluated deterministically from existing model intent
- keep explicit construction geometry and the current relation-driven path as stable fallback paths
- defer GUI manipulators, sketch constraint solving, 3D sketching, and advanced surfacing

## Future roadmap: Inventor-like sketcher and sketch-driven features

Goal: eventually provide an Inventor-class sketch environment and sketch-driven feature set while keeping BLCAD's own core model and Linux-native architecture.

Detailed document: `docs/inventor-like-sketcher-and-feature-roadmap.md`

Planned scope includes:

- 2D sketch points, lines, construction lines, centerlines, rectangles, circles, arcs, ellipses, splines, polygons, slots, sketch fillets, sketch chamfers, sketch text, and projected geometry
- sketch editing tools such as trim, extend, split, offset, mirror, rectangular/circular sketch pattern, move, copy, rotate, scale, stretch, and construction toggle
- geometric constraints such as coincident, collinear, concentric, parallel, perpendicular, tangent, horizontal, vertical, equal, symmetric, midpoint, fix/ground, smooth, and point-on-curve
- driving and driven/reference dimensions such as linear, aligned, horizontal, vertical, angular, radius, diameter, arc length, point-to-line, and point-to-point dimensions
- automatic profile and region detection from unordered curves
- multiple profile regions, inner loops, islands, and stable region selection
- 3D sketch points, lines, polylines, arcs, splines, helices, projected curves, intersection curves, and guide curves
- sketch-driven feature families such as richer extrude/cut, revolve, revolve cut, sweep, sweep cut, loft, loft cut, hole, thread, emboss, engrave, rib, web, shell, draft, patterns, and mirrors
- surfacing features such as boundary surface, fill surface, network surface, patch surface, trim/extend surface, stitch/knit surfaces, and closed-shell-to-solid conversion

This is a long-term parity target, not a single MVP. It should be implemented after construction geometry and in staged increments: richer 2D entities, profile detection, constraints/dimensions, revolve, richer extrude/cut, 3D sketches, sweep/loft, surfacing, and finally GUI usability.

## Future MVP: Advanced surfacing and 3D sketching

Goal: support spatial curves, guide splines, sweep, loft, boundary surfaces, surface stitching, and conversion of closed stitched surfaces into solids.

Detailed document: `docs/advanced-surfacing-and-3d-sketch-mvp.md`

Explicitly not implemented yet:

- 3D sketch points, 3D lines, 3D polylines, and 3D splines
- splines connecting points from different sketches on different planes
- guide curves for loft and sweep features
- sweep along lines, polylines, arcs, or splines
- loft between two or more sketches
- smooth multi-section lofts through arbitrary many parallel sketches
- continuity modes such as C0, G1, and G2
- using a middle sketch as a smooth transition control without an unwanted hard edge
- boundary surfaces from spatial curves
- surfaces from arbitrary curves across multiple sketches
- surface stitching / knitting
- conversion from a closed stitched shell into a solid body

Target use cases:

- turbine blades
- propeller blades
- wings and airfoils
- smooth duct transitions
- pipe-to-pipe transitions
- organic fairings

Proposed first implementation sequence:

- add explicit 3D points and 3D line segments
- add 3D spline curves with control or interpolation points
- allow 3D splines to reference points from sketches on different planes
- add JSON roundtrip tests for 3D sketch curves
- convert 3D curves into OCCT edge/wire representations
- add simple sweep along a straight construction line
- add sweep along a 3D polyline or spline path
- add simple loft between two closed planar profile sketches
- add multi-section loft through three or more profiles
- add explicit continuity settings for smooth intermediate sections
- add guide-curve lofts using one or more 3D splines
- add boundary surfaces from spatial curves
- add surface stitching and closed-shell-to-solid conversion

This block depends on construction geometry and closed sketch profiles. It answers how spatial curves, multiple profiles, and surfaces can form freeform geometry.

## MVP 3: Parametric bolt circle

Goal: first meaningful parametric feature test.

Detailed documents: `docs/feature-system.md`, `docs/hole-wizard.md`, `docs/pattern-and-mirror-features.md`

- bolt circle radius as parameter
- hole count as parameter
- hole diameter as parameter
- automatic recompute on change

## MVP 4: Simple assembly parameters

Goal: cross-part parametrization.

Detailed documents: `docs/parameter-model.md`, `docs/file-format.md`

- assembly document
- two part documents in one assembly
- shared assembly parameter
- both parts use the same bolt-circle parameter

## MVP 5: Simple assembly constraints

Goal: place parts through constraints.

Detailed document: `docs/assembly-system.md`

- component instances
- axis-to-axis
- face-to-face
- simple screw placement

## Later target-architecture blocks

These are documented as incremental, testable blocks and are sequenced after the numbered MVPs above:

- `docs/inventor-like-sketcher-and-feature-roadmap.md`: Inventor-like sketcher, constraints, dimensions, region detection, revolve, sweep, loft, and sketch-driven feature parity
- `docs/fillet-chamfer-features.md`: fillets and chamfers as parametric features
- `docs/pattern-and-mirror-features.md`: linear/circular patterns and mirror
- `docs/hole-wizard.md`: semantic hole features and the standards database
- `docs/shaft-wizard.md`: shaft assistant (calculation plus geometry generation)
- `docs/assembly-system.md`: full constraint solver, joints, motion, subassemblies
- `docs/engineering-modules.md`: bolt, bearing, gear, material, and standard-parts modules
- `docs/advanced-surfacing-and-3d-sketch-mvp.md`: 3D sketching, guide curves, sweep, loft, boundary surfaces, surface stitching, and closed-shell-to-solid conversion
- `docs/user-interface.md`: UI over the core

The cross-cutting rules they all rely on are in `docs/semantic-references.md` and `docs/parameter-model.md`.

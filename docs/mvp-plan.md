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

## MVP 2: Sketch on planar face

Goal: place sketches on existing generated planar faces while preserving semantic model intent instead of storing raw OCCT face IDs.

Implemented seed:

- semantic references for top, bottom, right, left, front, and back faces of a simple additive extrude
- `DerivedWorkplane` from those semantic faces
- sketches can reference derived workplanes
- dependency graph includes paths such as `feature.base_extrude -> workplane.base_back -> sketch.back_hole`
- JSON serialization supports `derived_workplanes` with `top`, `bottom`, `right`, `left`, `front`, and `back` faces
- geometry-layer `WorkplaneResolver`
- rectangular bounds on resolved generated-face workplanes
- circular cut and closed-profile recompute through resolved workplanes
- checked-in top/bottom/right/left/front/back example files

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

## Implemented block: Construction geometry, chained relations, and evaluated semantic references

Goal: support user-created and relation-driven construction planes, construction lines/axes, and construction points as stable sketch/reference geometry without storing raw OCCT topology.

Detailed document: `docs/construction-geometry-mvp.md`

Implemented scope:

- `ConstructionPointId`, `ConstructionLineId`, `ConstructionPlaneId`, and `ConstructionRelationId`
- `ConstructionPoint`, `ConstructionLine`, `ConstructionPlane`, and `ConstructionRelation`
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
- `PlaneOffsetFromPlane` relation-driven construction plane
- `LineThroughTwoPoints` relation-driven construction line
- `PlaneThroughThreePoints` relation-driven construction plane
- `PlaneParallelToPlaneThroughPoint` relation-driven construction plane
- `LineParallelToLineThroughPoint` relation-driven construction line
- `LineParallelToGeneratedEdgeThroughPoint` relation-driven construction line
- relation-driven construction points on generated vertices and generated edge midpoints
- semantic generated edge and vertex references similar to `SemanticFaceReference`
- `SemanticReferenceEvaluator` for rectangular-additive-extrude generated edges and vertices
- `ConstructionPointResolver` for explicit points, generated vertex points, and generated edge midpoint points
- `ConstructionLineResolver` for explicit, two-point, line-parallel, and generated-edge-parallel construction lines
- model-intent relation definitions for `PointOnPlane`, `PointOnLine`, `PointOnGeneratedEdge`, `PointOnGeneratedVertex`, and `LineOnPlane`
- relation validation for generated edge/vertex references without storing raw OCCT topology IDs
- dependency graph tests for chained construction relations
- JSON roundtrip tests for chained relations and semantic generated references
- JSON-backed `examples/generated_semantic_references.blcad.json`
- `WorkplaneResolver` support for offset construction planes
- `WorkplaneResolver` support for three-point construction planes
- `WorkplaneResolver` support for planes parallel to another plane through a point
- explicit construction geometry kept as the stable fallback path

Still not implemented in this block:

- generated semantic edge/vertex evaluation for arbitrary non-rectangular or non-extrude topology
- point-on-generated-edge parameterization beyond deterministic midpoint
- point-on-line, point-on-plane, and line-on-plane geometric solving
- parallel, orthogonal, tangent, angle, or normal construction relations beyond the implemented seed
- standalone relation collections independent from construction objects
- GUI manipulators
- full construction-geometry solver
- 3D sketch splines, sweep, loft, surface stitching, or closed-shell-to-solid conversion

## Implemented block: Projected sketch reference geometry

Goal: project evaluated semantic generated edges, generated vertices, construction points, and construction lines into sketch-local reference geometry so sketches can consume projected references without storing raw OCCT topology and without adding a full sketch constraint solver yet.

Detailed document: `docs/projected-sketch-reference-geometry.md`

Implemented scope:

- `ProjectedSketchPoint` and `ProjectedSketchLine` as sketch-local reference entities
- sketch-level projected reference IDs using `SketchEntityId`
- model-intent references from projected point references to `ConstructionPointId` and `SemanticVertexReference`
- model-intent references from projected line references to `ConstructionLineId` and `SemanticEdgeReference`
- `projected_points` and `projected_lines` JSON persistence on sketches
- JSON roundtrip tests for construction-point, construction-line, semantic-vertex, and semantic-edge projected references
- `SketchReferenceProjector` in the optional geometry layer
- projection of resolved construction points and semantic vertices into sketch-local `Point2` coordinates
- projection of resolved construction lines and semantic edges into sketch-local point-plus-direction line references
- explicit out-of-plane validation errors for projected points and projected lines
- geometry tests for projected semantic generated vertices and edges on generated top-face workplanes
- geometry tests for projected generated-reference construction points and generated-edge-parallel construction lines
- checked-in `examples/projected_sketch_references.blcad.json`

Still not implemented in this block:

- automatic use of projected references as closed-profile segments
- solver-backed sketch constraints against projected references
- dependency graph edges from projected references to the owning sketch and downstream features
- projected reference selection or display in a GUI
- arbitrary generated topology beyond the current rectangular additive extrude seed

## Next MVP: Reference-driven sketch constraints and profile consumption

Goal: make projected sketch references usable as associative sketch targets instead of passive projected helper geometry.

Proposed first implementation sequence:

- add dependency graph edges from projected sketch references to their source construction or semantic nodes, and from the sketch to downstream features
- add sketch constraint/reference target handles for projected points and projected lines
- implement deterministic reference-driven helper constraints such as coincident-to-projected-point and parallel-or-collinear-to-projected-line without introducing the full solver yet
- allow deterministic profile helper generation from projected references only where endpoints and directions are fully resolved
- add incremental invalidation tests proving source feature or construction-relation changes dirty projected-reference sketches and dependent features
- add JSON roundtrip tests for the first projected-reference constraint target records
- keep GUI manipulators, automatic region detection, full sketch solving, 3D sketches, sweep, loft, and surfacing deferred

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

This is a long-term parity target, not a single MVP. It should be implemented after construction geometry and in staged increments.

## Future MVP: Advanced surfacing and 3D sketching

Goal: support spatial curves, guide splines, sweep, loft, boundary surfaces, surface stitching, and conversion of closed stitched surfaces into solids.

Detailed document: `docs/advanced-surfacing-and-3d-sketch-mvp.md`

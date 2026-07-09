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

## Implemented block: Projected sketch references and first reference-driven constraints

Goal: project evaluated semantic generated edges, generated vertices, construction points, and construction lines into sketch-local reference geometry, then make those projected references usable as first associative sketch targets without storing raw OCCT topology and without adding a full sketch constraint solver yet.

Detailed document: `docs/projected-sketch-reference-geometry.md`

Implemented scope:

- `ProjectedSketchPoint` and `ProjectedSketchLine` as sketch-local reference entities
- sketch-level projected reference IDs using `SketchEntityId`
- model-intent references from projected point references to `ConstructionPointId` and `SemanticVertexReference`
- model-intent references from projected line references to `ConstructionLineId` and `SemanticEdgeReference`
- `SketchReferenceTarget` handles for line segments, line endpoints, projected points, and projected lines
- `SketchConstraintId` and first `SketchConstraint` records
- `coincident_to_projected_point`, `parallel_to_projected_line`, and `collinear_with_projected_line` constraint kinds
- `projected_points`, `projected_lines`, and `constraints` JSON persistence on sketches
- JSON roundtrip tests for construction-point, construction-line, semantic-vertex, semantic-edge, and projected-reference constraint records
- `SketchReferenceProjector` in the optional geometry layer
- projection of resolved construction points and semantic vertices into sketch-local `Point2` coordinates
- projection of resolved construction lines and semantic edges into sketch-local point-plus-direction line references
- `ReferenceDrivenSketchHelper` for deterministic evaluation of the first projected-reference constraints
- deterministic profile-helper line generation from two fully resolved projected-point endpoint constraints
- explicit out-of-plane validation errors for projected points and projected lines
- dependency graph edges from projected construction and semantic references to owning sketches
- invalidation tests proving source feature and construction-relation changes dirty projected-reference sketches and dependent features
- geometry tests for projected semantic generated vertices and edges on generated top-face workplanes
- geometry tests for projected generated-reference construction points and generated-edge-parallel construction lines
- geometry tests for the first reference-driven helper constraints
- checked-in `examples/projected_sketch_references.blcad.json`

Still not implemented in this block:

- full sketch constraint solving
- automatic region detection from projected references
- automatic consumption of projected references as profile topology without explicit helper constraints
- projected reference selection or display in a GUI
- arbitrary generated topology beyond the current rectangular additive extrude seed

## Implemented block: Robust reference remapping and sketch placement recovery

Goal: make sketch placement and projected references robust when a source face, edge, or vertex changes or disappears after feature edits.

Detailed document: `docs/reference-recovery-mvp.md`

Implemented scope:

- `ReferenceStatusId` and `ReferenceRemapId`
- `SemanticReferenceTarget` as a uniform target for semantic faces, edges, and vertices
- `ReferenceStatusRecord` with `resolved` and `lost` states
- `ReferenceRemapRecord` for explicit same-kind semantic remap intent
- `SketchOriginOverrideRecord` for optional sketch-local origin overrides
- `ReferenceRecoveryEvaluator` for conservative resolved/lost checks against the current rectangular additive-extrude semantic topology seed
- `PartDocument` storage, finders, counts, and dependency graph nodes for reference status, remap, and origin-override records
- JSON persistence for `reference_statuses`, `reference_remaps`, and `sketch_origin_overrides`
- `WorkplaneResolver::resolve_for_sketch` applying sketch-origin overrides to resolved frames and bounds
- `SketchReferenceProjector` and `GeometryRecomputeExecutor` now use sketch-aware workplane resolution
- checked-in recovery metadata in `examples/projected_sketch_references.blcad.json`
- core tests for resolved/lost reference status and remap/origin storage
- JSON roundtrip tests for reference recovery metadata
- geometry tests for generated-face sketch movement under source dimension changes
- geometry tests for lost-reference reporting instead of silent reassignment
- geometry tests for sketch-origin override frame shifts

Still not implemented in this block:

- GUI repair/remapping dialogs
- automatic topology matching after arbitrary edits
- automatic application of remap records to rewrite sketches or projected references
- arbitrary generated topology beyond the current rectangular additive extrude seed
- full sketch constraint solving or automatic region detection

## Next MVP: Deterministic profile consumption from reference-driven sketch helpers

Goal: turn the first reference-driven helper geometry into reproducible profile input for feature operations without adding a full sketch solver or automatic region detector yet.

Proposed first implementation sequence:

- add model-intent records for reference-generated helper line entities derived from projected endpoint constraints
- validate helper-generated line endpoints and projected-line direction consistency before profile construction
- allow a `ClosedProfile` to explicitly consume deterministic reference-generated helper line IDs where all helpers are fully resolved
- add JSON roundtrip tests for reference-generated helper entity records
- add geometry tests for an additive extrude and a subtractive cut whose closed profile uses deterministic projected-reference helper lines
- add invalidation tests proving projected source changes dirty helper-generated profiles and dependent features
- keep full sketch solving, automatic region detection, GUI editing, arbitrary generated topology, 3D sketches, sweep, loft, and surfacing deferred

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

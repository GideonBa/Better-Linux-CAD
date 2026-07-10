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
- dependency graph paths from source feature to workplane, sketch, and dependent feature
- JSON serialization for `derived_workplanes`
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
- full sketch constraint solver
- GUI sketch editing

## Implemented block: Construction geometry, chained relations, and evaluated semantic references

Goal: support user-created and relation-driven construction planes, construction lines/axes, and construction points as stable sketch/reference geometry without storing raw OCCT topology.

Detailed document: `docs/construction-geometry-mvp.md`

Implemented scope:

- `ConstructionPointId`, `ConstructionLineId`, `ConstructionPlaneId`, and `ConstructionRelationId`
- `ConstructionPoint`, `ConstructionLine`, `ConstructionPlane`, and `ConstructionRelation`
- explicit placement definitions for point, line, and plane
- optional `parameter_dependencies` from construction geometry to parameters
- sketches can reference construction planes as workplanes
- JSON serialization and roundtrip tests for explicit and relation-driven construction geometry
- `WorkplaneResolver` resolves explicit, offset, three-point, and parallel-through-point construction planes
- relation-driven construction points on generated vertices and generated edge midpoints
- semantic generated edge and vertex references similar to `SemanticFaceReference`
- `SemanticReferenceEvaluator` for rectangular-additive-extrude generated edges and vertices
- `ConstructionPointResolver` and `ConstructionLineResolver`
- chained construction-relation dependency and invalidation tests
- JSON-backed `examples/generated_semantic_references.blcad.json`

Still not implemented in this block:

- generated semantic edge/vertex evaluation for arbitrary non-rectangular or non-extrude topology
- point-on-generated-edge parameterization beyond deterministic midpoint
- point-on-line, point-on-plane, and line-on-plane geometric solving
- richer construction relations beyond the implemented seed
- standalone relation collections independent from construction objects
- GUI manipulators
- full construction-geometry solver
- 3D sketch splines, sweep, loft, surface stitching, or closed-shell-to-solid conversion

## Implemented block: Projected sketch references and first reference-driven constraints

Goal: project evaluated semantic generated edges, generated vertices, construction points, and construction lines into sketch-local reference geometry, then make those projected references usable as first associative sketch targets without storing raw OCCT topology and without adding a full sketch constraint solver yet.

Detailed document: `docs/projected-sketch-reference-geometry.md`

Implemented scope:

- `ProjectedSketchPoint` and `ProjectedSketchLine` as sketch-local reference entities
- model-intent references from projected point references to `ConstructionPointId` and `SemanticVertexReference`
- model-intent references from projected line references to `ConstructionLineId` and `SemanticEdgeReference`
- `SketchReferenceTarget` handles for line segments, line endpoints, projected points, and projected lines
- `SketchConstraintId` and first `SketchConstraint` records
- `coincident_to_projected_point`, `parallel_to_projected_line`, and `collinear_with_projected_line` constraint kinds
- `projected_points`, `projected_lines`, and `constraints` JSON persistence on sketches
- `SketchReferenceProjector` in the optional geometry layer
- `ReferenceDrivenSketchHelper` for deterministic evaluation of the first projected-reference constraints
- deterministic helper-line generation from two fully resolved projected-point endpoint constraints
- explicit out-of-plane validation errors
- dependency graph edges from projected construction and semantic references to owning sketches
- invalidation and geometry tests for projected references and first helper constraints
- checked-in `examples/projected_sketch_references.blcad.json`

Still not implemented in this block:

- full sketch constraint solving
- automatic region detection from projected references
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
- `SketchReferenceProjector` and `GeometryRecomputeExecutor` use sketch-aware workplane resolution
- checked-in recovery metadata in `examples/projected_sketch_references.blcad.json`
- core, JSON, invalidation, and geometry tests for recovery behavior

Still not implemented in this block:

- GUI repair/remapping dialogs
- automatic topology matching after arbitrary edits
- automatic application of remap records to rewrite sketches or projected references
- arbitrary generated topology beyond the current rectangular additive extrude seed
- full sketch constraint solving or automatic region detection

## Implemented block: Deterministic profile consumption from reference-driven sketch helpers

Goal: turn the first reference-driven helper geometry into reproducible profile input for feature operations without adding a full sketch solver or automatic region detector yet.

Detailed document: `docs/reference-generated-profile-helpers-mvp.md`

Implemented scope:

- `ReferenceGeneratedLine` as first-class sketch helper-line model intent
- `Sketch` storage for `reference_generated_lines`
- JSON persistence for `reference_generated_lines`
- sketch validation allowing constraints and closed profiles to target reference-generated helper-line IDs
- `PartDocument` validation of endpoint constraints and optional projected-line direction constraints for helper lines
- dependency graph nodes using `<sketch-id>.reference_generated_line.<helper-line-id>`
- dependency edges from projected construction/semantic sources through helper-line nodes to sketches and dependent features
- `ReferenceGeneratedProfileResolver` resolving helper lines from projected endpoint constraints
- direction consistency validation against optional projected-line constraints
- `GeometryRecomputeExecutor` consumption of reference-generated helper-line closed profiles
- additive extrude recompute from one reference-generated helper profile
- subtractive through-all cut recompute from one reference-generated helper profile
- JSON roundtrip tests for reference-generated helper lines
- invalidation tests for helper-line dirty propagation
- geometry tests for resolver behavior, additive recompute, and subtractive recompute

Still not implemented in this block:

- mixed explicit-line and reference-generated-line profiles in one closed profile
- full sketch constraint solving
- automatic region detection from unordered projected references
- GUI editing of reference-generated helper entities
- arbitrary generated topology beyond the current rectangular additive extrude seed

## Implemented block: Sketch constraints and dimensions seed

Goal: add the next minimal non-solver sketch intent layer: explicit geometric constraints and first driving dimensions on line-segment geometry, while keeping deterministic validation and avoiding a full constraint solver.

Detailed document: `docs/sketch-constraints-and-dimensions-mvp.md`

Implemented scope:

- `SketchGeometricConstraint` records for fixed, horizontal, vertical, parallel, perpendicular, and equal-length constraints
- `SketchDrivingDimension` records for horizontal, vertical, aligned, and point-to-point distances
- `SketchDimensionId`
- `geometric_constraints` and `driving_dimensions` JSON persistence
- validation of geometric constraints against explicit line segments and endpoints
- validation of driving dimensions against explicit line endpoints and existing length parameters
- dependency graph edges from driving dimension parameters to sketches and dependent features
- `DimensionDrivenProfileResolver` for deterministic dimension-driven closed-profile vertices
- `GeometryRecomputeExecutor` integration for dimension-driven closed-profile additive extrudes and subtractive cuts
- JSON roundtrip and invalidation tests
- geometry tests proving dimension parameter changes alter resolved profile vertices

Still not implemented in this block:

- full sketch constraint solving
- over- or under-constrained diagnosis
- driven dimensions
- automatic constraint inference
- GUI constraint display or editing
- arcs, splines, 3D sketches, sweep, loft, and surfacing

## Implemented block: Automatic profile region detection seed

Goal: detect deterministic closed regions from explicit sketch lines and resolved helper lines without requiring users or JSON to predeclare every `ClosedProfile` manually.

Detailed document: `docs/automatic-profile-region-detection-mvp.md`

Implemented scope:

- `SketchRegionFinder` in the optional geometry layer
- deterministic single-region detection from unordered explicit sketch lines
- local-line collection from explicit lines, dimension-resolved explicit lines, and resolved reference-generated helper lines
- validation errors for duplicate edges, degenerate edges, gaps, branches, ambiguous continuations, non-closing loops, self-intersections, and multiple/simple-contour violations
- stable generated `ClosedProfile` candidate IDs of the form `generated.region.<sketch-id>.0`
- conversion of a generated candidate into a persistable `ClosedProfile`
- `GeometryRecomputeExecutor` fallback to detected regions when a sketch has no rectangle, circle, or explicit closed profile
- additive extrude recompute from one detected region
- subtractive through-all cut recompute from one detected region
- geometry tests for deterministic detection, branch rejection, additive recompute, and subtractive recompute

Still not implemented in this block:

- multiple regions
- nested contours
- trim/extend
- arcs, splines, 3D sketches, sweep, loft, and surfacing
- full sketch solving or GUI region selection

## Implemented block: Multi-contour profiles and holes seed

Goal: allow one profile operation to consume an outer contour plus one or more inner contours for holes, while keeping contour selection explicit and deterministic.

Detailed document: `docs/composite-closed-profile-holes-mvp.md`

Implemented scope:

- `CompositeClosedProfile` model-intent record with one ordered outer contour and ordered inner contours
- core validation for non-empty ids, at least one inner contour, contour sizes, duplicate line ids, and shared line ids across contours
- sketch-level validation that each contour is an ordered, connected, non-self-intersecting closed loop
- geometry validation that inner contours lie strictly inside the outer contour
- geometry validation that inner contours do not intersect the outer contour or each other
- `ClosedProfileAdapter` support for OCCT faces with inner wires
- additive extrude recompute from one composite closed profile with a hole
- subtractive through-all cut recompute from one composite closed profile with a hole
- core tests for composite contour intent and validation
- geometry tests for additive and subtractive recompute from composite profiles

Still not implemented in this block:

- automatic multi-region selection
- trim/extend
- arcs and splines
- GUI profile picking
- full sketch constraint solving
- 3D sketches, sweep, loft, and surfacing

## Next MVP: Arc and trim/extend sketch profile seed

Goal: support first non-linear 2D sketch contours by adding circular arcs and explicit trim/extend endpoint intent.

Proposed first implementation sequence:

- add a sketch arc entity with stable IDs, center/radius or three-point construction, and deterministic JSON intent
- add contour validation that supports ordered line/arc loops without converting arcs to raw tessellation as model intent
- extend the geometry adapter to build OCCT wires containing line and circular-arc edges
- add trim/extend metadata for line/arc endpoints as explicit model intent, not as GUI-only state
- add additive extrude and subtractive cut tests for one profile containing a circular arc
- keep splines, tangent constraint solving, automatic fillets, full GUI editing, 3D sketches, sweep, loft, surface stitching, and closed-shell-to-solid conversion deferred

## Future roadmap: Inventor-like sketcher and sketch-driven features

Goal: eventually provide an Inventor-class sketch environment and sketch-driven feature set while keeping BLCAD's own core model and Linux-native architecture.

Detailed document: `docs/inventor-like-sketcher-and-feature-roadmap.md`

Planned scope includes 2D sketch primitives, sketch editing tools, geometric constraints, driving and driven dimensions, automatic profile and region detection, 3D sketches, richer sketch-driven feature families, and surfacing features.

This is a long-term parity target, not a single MVP. It should be implemented after construction geometry and in staged increments.

## Future MVP: Advanced surfacing and 3D sketching

Goal: support spatial curves, guide splines, sweep, loft, boundary surfaces, surface stitching, and conversion of closed stitched surfaces into solids.

Detailed document: `docs/advanced-surfacing-and-3d-sketch-mvp.md`

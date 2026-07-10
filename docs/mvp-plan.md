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
- JSON serialization of model intent for the currently JSON-backed records
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
- geometry tests for non-rectangular and non-circular profile operations
- checked-in closed-profile examples under `examples/`

Still not implemented in this block:

- splines
- trimmed curves
- full sketch constraint solver
- GUI sketch editing

## Implemented block: Construction geometry, chained relations, and evaluated semantic references

Goal: support user-created and relation-driven construction planes, construction lines/axes, and construction points as stable sketch/reference geometry without storing raw OCCT topology.

Detailed document: `docs/construction-geometry-mvp.md`

Implemented scope:

- explicit and relation-driven construction points, lines, and planes
- construction relation IDs and relation intent records
- optional construction-geometry parameter dependencies
- construction planes as sketch workplanes
- semantic generated edge and vertex references
- `SemanticReferenceEvaluator` for the rectangular-additive-extrude seed
- `ConstructionPointResolver` and `ConstructionLineResolver`
- chained construction-relation dependency and invalidation tests
- JSON-backed generated semantic reference example

Still not implemented in this block:

- generated semantic edge/vertex evaluation for arbitrary non-rectangular or non-extrude topology
- point-on-generated-edge parameterization beyond deterministic midpoint
- full construction-geometry solver
- GUI manipulators
- 3D sketch splines, sweep, loft, surface stitching, or closed-shell-to-solid conversion

## Implemented block: Projected sketch references and first reference-driven constraints

Goal: project evaluated semantic generated edges, generated vertices, construction points, and construction lines into sketch-local reference geometry, then make those projected references usable as first associative sketch targets without storing raw OCCT topology.

Detailed document: `docs/projected-sketch-reference-geometry.md`

Implemented scope:

- `ProjectedSketchPoint` and `ProjectedSketchLine`
- model-intent references to construction geometry and semantic generated references
- `SketchReferenceTarget` handles for line segments, line endpoints, projected points, and projected lines
- first `SketchConstraint` records
- `coincident_to_projected_point`, `parallel_to_projected_line`, and `collinear_with_projected_line`
- projected-reference JSON persistence
- `SketchReferenceProjector`
- `ReferenceDrivenSketchHelper`
- deterministic helper-line generation from projected references
- out-of-plane validation errors
- dependency graph edges from projected references to owning sketches

Still not implemented in this block:

- full sketch constraint solving
- projected reference selection or display in a GUI
- arbitrary generated topology beyond the current rectangular additive extrude seed

## Implemented block: Robust reference remapping and sketch placement recovery

Goal: make sketch placement and projected references robust when a source face, edge, or vertex changes or disappears after feature edits.

Detailed document: `docs/reference-recovery-mvp.md`

Implemented scope:

- `ReferenceStatusRecord` with `resolved` and `lost` states
- `ReferenceRemapRecord` for explicit same-kind semantic remap intent
- `SketchOriginOverrideRecord`
- `ReferenceRecoveryEvaluator`
- JSON persistence for recovery metadata
- sketch-aware workplane resolution using origin overrides
- recovery tests across core, JSON, invalidation, and geometry paths

Still not implemented in this block:

- GUI repair/remapping dialogs
- automatic topology matching after arbitrary edits
- automatic application of remap records to rewrite sketches or projected references

## Implemented block: Deterministic profile consumption from reference-driven sketch helpers

Goal: turn the first reference-driven helper geometry into reproducible profile input for feature operations without adding a full sketch solver or automatic region detector yet.

Detailed document: `docs/reference-generated-profile-helpers-mvp.md`

Implemented scope:

- `ReferenceGeneratedLine`
- JSON persistence for `reference_generated_lines`
- validation of endpoint and optional projected-line direction constraints
- helper-line dependency graph nodes
- `ReferenceGeneratedProfileResolver`
- additive and subtractive recompute from reference-generated helper profiles
- core, invalidation, JSON, and geometry tests

Still not implemented in this block:

- mixed explicit-line and reference-generated-line profiles in one closed profile
- full sketch constraint solving
- GUI editing of reference-generated helper entities

## Implemented block: Sketch constraints and dimensions seed

Goal: add the next minimal non-solver sketch intent layer: explicit geometric constraints and first driving dimensions on line-segment geometry.

Detailed document: `docs/sketch-constraints-and-dimensions-mvp.md`

Implemented scope:

- `SketchGeometricConstraint` records for fixed, horizontal, vertical, parallel, perpendicular, and equal-length constraints
- `SketchDrivingDimension` records for horizontal, vertical, aligned, and point-to-point distances
- `SketchDimensionId`
- JSON persistence for geometric constraints and driving dimensions
- dependency graph edges from driving dimension parameters to sketches and dependent features
- `DimensionDrivenProfileResolver`
- additive and subtractive recompute from deterministic dimension-driven profiles

Still not implemented in this block:

- full sketch constraint solving
- over- or under-constrained diagnosis
- driven dimensions
- automatic constraint inference
- GUI constraint display or editing

## Implemented block: Automatic profile region detection seed

Goal: detect deterministic closed regions from explicit sketch lines and resolved helper lines without requiring users or JSON to predeclare every `ClosedProfile` manually.

Detailed document: `docs/automatic-profile-region-detection-mvp.md`

Implemented scope:

- `SketchRegionFinder`
- deterministic single-region detection from unordered explicit sketch lines
- local-line collection from explicit lines, dimension-resolved explicit lines, and resolved reference-generated helper lines
- validation errors for duplicate edges, degenerate edges, gaps, branches, ambiguous continuations, non-closing loops, self-intersections, and multiple/simple-contour violations
- stable generated `ClosedProfile` candidate IDs
- conversion of a generated candidate into a persistable `ClosedProfile`
- additive and subtractive recompute from one detected region

Still not implemented in this block:

- multiple regions
- nested contours
- trim/extend solving
- full sketch solving or GUI region selection

## Implemented block: Multi-contour profiles and holes seed

Goal: allow one profile operation to consume an outer contour plus one or more inner contours for holes, while keeping contour selection explicit and deterministic.

Detailed document: `docs/composite-closed-profile-holes-mvp.md`

Implemented scope:

- `CompositeClosedProfile` with one ordered outer contour and ordered inner contours
- core validation for contour sizes, duplicate line ids, and shared line ids across contours
- sketch-level validation for ordered, connected, non-self-intersecting contours
- geometry validation that inner contours lie strictly inside the outer contour and do not overlap
- `ClosedProfileAdapter` support for OCCT faces with inner wires
- additive and subtractive recompute from composite profiles with holes
- core, JSON, and geometry tests

Still not implemented in this block:

- automatic multi-region selection
- trim/extend solving
- arcs and splines inside composite contours
- GUI profile picking
- 3D sketches, sweep, loft, and surfacing

## Implemented block: Arc and trim/extend sketch profile seed

Goal: support first non-linear 2D sketch contours by adding circular arcs and explicit trim/extend endpoint intent.

Detailed document: `docs/arc-and-trim-extend-sketch-profile-mvp.md`

Implemented scope:

- `ArcSegment` as a three-point circular-arc sketch entity
- `ArcClosedProfile` as an ordered line/arc contour profile record
- `SketchTrimOperationId`
- `SketchTrimExtendOperation` records for explicit trim/extend endpoint metadata
- core validation for non-empty arc IDs, distinct arc points, non-collinear arc definitions, ordered line/arc contour closure, and trim/extend target existence
- `ClosedProfileAdapter` curve APIs for OCCT wires with line and circular-arc edges
- additive extrude recompute from one arc closed profile
- subtractive through-all cut recompute from one arc closed profile
- core tests for arc validation, trim metadata, and line/arc profile validation
- geometry tests for additive and subtractive recompute from one arc profile

Still not implemented in this block:

- source-level JSON roundtrip for arc profile records
- exact curved self-intersection validation
- trim/extend solving or geometry rewriting
- splines
- tangent constraint solving
- automatic fillets
- GUI curve editing
- 3D sketches, sweep, loft, and surfacing

## Next MVP: Arc JSON roundtrip and curved-contour validation seed

Goal: make arc/trim profile intent fully persistent and add the first exact curved-contour validation pass.

Proposed first implementation sequence:

- extend `part_document_json.cpp` to serialize and deserialize `arc_segments`, `trim_extend_operations`, and `arc_closed_profiles`
- add JSON roundtrip tests for one line/arc profile and one trim/extend metadata record
- add an exact curved self-intersection check for simple line/arc profiles
- add a checked-in `.blcad.json` example for an arc-profile additive extrude
- keep splines, tangent constraint solving, automatic fillets, GUI trim/extend solving, 3D sketches, sweep, loft, surface stitching, and closed-shell-to-solid conversion deferred

## Future roadmap: Inventor-like sketcher and sketch-driven features

Goal: eventually provide an Inventor-class sketch environment and sketch-driven feature set while keeping BLCAD's own core model and Linux-native architecture.

Detailed document: `docs/inventor-like-sketcher-and-feature-roadmap.md`

Planned scope includes 2D sketch primitives, sketch editing tools, geometric constraints, driving and driven dimensions, automatic profile and region detection, 3D sketches, richer sketch-driven feature families, and surfacing features.

This is a long-term parity target, not a single MVP. It should be implemented in staged increments.

## Future MVP: Advanced surfacing and 3D sketching

Goal: support spatial curves, guide splines, sweep, loft, boundary surfaces, surface stitching, and conversion of closed stitched surfaces into solids.

Detailed document: `docs/advanced-surfacing-and-3d-sketch-mvp.md`

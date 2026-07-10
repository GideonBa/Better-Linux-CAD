# MVP Plan

## MVP 1: Single-part modeling

Goal: one single parametric part with a headless file-based export path.

Detailed document: `docs/mvp-1-specification.md`

Implemented scope:

- part document, parameters with units, datum planes, sketches, and feature-intent records
- rectangle and circle profile seeds
- additive extrude and subtractive through-all cut seeds
- dependency graph, invalidation state, recompute plan, and numeric parameter update
- optional OCCT geometry execution through `ShapeCache`
- STEP export and headless JSON-to-STEP example
- JSON serialization of the currently JSON-backed model-intent records

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

## Implemented block: Line-based general closed sketch profiles

Detailed document: `docs/general-closed-sketch-profile-mvp.md`

Implemented scope:

- `SketchEntityId`, `LineSegment`, and `ClosedProfile`
- ordered line-segment references
- validation of closed, connected, non-self-intersecting line loops
- JSON serialization and roundtrip tests for `line_segments` and `closed_profiles`
- OCCT wire and face creation from ordered line vertices
- additive and subtractive recompute from one closed profile

## Implemented block: Construction geometry, chained relations, and evaluated semantic references

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

## Implemented block: Projected sketch references and first reference-driven constraints

Detailed document: `docs/projected-sketch-reference-geometry.md`

Implemented scope:

- `ProjectedSketchPoint` and `ProjectedSketchLine`
- model-intent references to construction geometry and semantic generated references
- `SketchReferenceTarget` handles for line segments, line endpoints, projected points, and projected lines
- first `SketchConstraint` records
- `coincident_to_projected_point`, `parallel_to_projected_line`, and `collinear_with_projected_line`
- projected-reference JSON persistence
- `SketchReferenceProjector`, `ReferenceDrivenSketchHelper`, and deterministic helper-line generation
- dependency graph edges from projected references to owning sketches

## Implemented block: Robust reference remapping and sketch placement recovery

Detailed document: `docs/reference-recovery-mvp.md`

Implemented scope:

- `ReferenceStatusRecord` with `resolved` and `lost` states
- `ReferenceRemapRecord` for explicit same-kind semantic remap intent
- `SketchOriginOverrideRecord`
- `ReferenceRecoveryEvaluator`
- JSON persistence for recovery metadata
- sketch-aware workplane resolution using origin overrides

## Implemented block: Deterministic profile consumption from reference-driven sketch helpers

Detailed document: `docs/reference-generated-profile-helpers-mvp.md`

Implemented scope:

- `ReferenceGeneratedLine`
- JSON persistence for `reference_generated_lines`
- validation of endpoint and optional projected-line direction constraints
- helper-line dependency graph nodes
- `ReferenceGeneratedProfileResolver`
- additive and subtractive recompute from reference-generated helper profiles

## Implemented block: Sketch constraints and dimensions seed

Detailed document: `docs/sketch-constraints-and-dimensions-mvp.md`

Implemented scope:

- `SketchGeometricConstraint` records for fixed, horizontal, vertical, parallel, perpendicular, and equal-length constraints
- `SketchDrivingDimension` records for horizontal, vertical, aligned, and point-to-point distances
- JSON persistence for geometric constraints and driving dimensions
- dependency graph edges from driving dimension parameters to sketches and dependent features
- `DimensionDrivenProfileResolver`
- additive and subtractive recompute from deterministic dimension-driven profiles

## Implemented block: Automatic profile region detection seed

Detailed document: `docs/automatic-profile-region-detection-mvp.md`

Implemented scope:

- `SketchRegionFinder`
- deterministic single-region detection from unordered explicit sketch lines
- local-line collection from explicit lines, dimension-resolved explicit lines, and resolved reference-generated helper lines
- stable generated `ClosedProfile` candidate IDs
- additive and subtractive recompute from one detected region

## Implemented block: Multi-contour profiles and holes seed

Detailed document: `docs/composite-closed-profile-holes-mvp.md`

Implemented scope:

- `CompositeClosedProfile` with one ordered outer contour and ordered inner contours
- core validation for contour sizes, duplicate line ids, and shared line ids across contours
- sketch-level validation for ordered, connected, non-self-intersecting contours
- geometry validation that inner contours lie strictly inside the outer contour and do not overlap
- `ClosedProfileAdapter` support for OCCT faces with inner wires
- additive and subtractive recompute from composite profiles with holes

## Implemented block: Arc and trim/extend sketch profile seed

Detailed document: `docs/arc-and-trim-extend-sketch-profile-mvp.md`

Implemented scope:

- `ArcSegment` as a three-point circular-arc sketch entity
- `ArcClosedProfile` as an ordered line/arc contour profile record
- `SketchTrimExtendOperation` records for explicit trim/extend endpoint metadata
- first curved-contour self-intersection validation for line/line, line/arc, and arc/arc pairs
- JSON roundtrip for `arc_segments`, `trim_extend_operations`, and `arc_closed_profiles`
- `ClosedProfileAdapter` curve APIs for OCCT wires with line and circular-arc edges
- additive and subtractive recompute from one arc closed profile

## Implemented block: Sketch-plane extrude direction seed

Detailed document: `docs/sketch-plane-extrude-direction-mvp.md`

Implemented scope:

- `ExtrudeDirection::SketchNormal`
- `ExtrudeDirection::OppositeSketchNormal`
- legacy `+Z` JSON loading as `sketch_normal`
- rectangle additive extrudes mapped through the resolved sketch workplane instead of the old global rectangle path
- arbitrary-axis through-all placement for circular and profile cuts using target bounding-box projection
- tests for slanted construction-plane additive extrude and opposite sketch-normal storage

## Implemented block: Spline and tangent-continuity sketch profile seed

Detailed document: `docs/spline-and-tangent-continuity-mvp.md`

Implemented scope:

- `SplineSegment` as an explicit cubic-Bezier sketch entity
- `SketchTangentContinuity` tangent metadata between two explicit sketch curve entities
- ordered line/arc/spline profile support through the existing `ArcClosedProfile` record
- JSON persistence for `spline_segments` and `tangent_continuities`
- OCCT Bezier-edge construction from four spline poles
- additive and subtractive recompute from one line/spline closed profile

## Implemented block: Sketch solver diagnostics seed

Detailed document: `docs/sketch-solver-diagnostics-mvp.md`

Implemented scope:

- `SketchDiagnosticSeverity`, `SketchDiagnosticKind`, `SketchConstraintDiagnostic`, and `SketchDiagnosticReport`
- `SketchConstraintDiagnostics::analyze` as a pure report generator that does not mutate model intent
- warnings for unconstrained endpoints, free spline handles, and profile sketches without driving dimensions
- errors for horizontal/vertical conflicts, duplicate fixed endpoint constraints, and duplicate driving dimensions
- debug JSON output through `serialize_sketch_diagnostic_report_to_json`

## Implemented block: Sketch repair suggestion seed

Detailed document: `docs/sketch-repair-suggestions-mvp.md`

Implemented scope:

- `SketchRepairSuggestionAction`, `SketchRepairSuggestion`, and `SketchRepairSuggestionReport`
- `SketchRepairSuggester::suggest` as a pure report generator that consumes `SketchDiagnosticReport`
- deterministic suggestion mapping for unconstrained endpoints, horizontal/vertical conflicts, duplicate fixed endpoints, duplicate driving dimensions, and undimensioned profile sketches
- links from each suggestion to the originating diagnostic kind and target
- debug JSON output through `serialize_sketch_repair_suggestion_report_to_json`

## Implemented block: Sketch repair application command seed

Detailed document: `docs/sketch-repair-commands-mvp.md`

Implemented scope:

- `SketchRepairCommandStatus`, `SketchRepairCommand`, and `SketchRepairCommandResult`
- `SketchRepairCommandExecutor::apply` for explicitly selected repair suggestions
- narrow mutation hooks on `Sketch` for removing one geometric constraint or one driving dimension by ID
- safe apply subset: add deterministic fixed endpoint constraint, remove duplicate fixed endpoint constraints, and remove duplicate driving dimensions
- skipped structured results for unsupported actions such as add-driving-dimension and choose-a-side orientation-conflict removal
- core tests proving selected safe commands mutate only intended sketch records

## Implemented block: Sketch repair transaction and undo seed

Detailed document: `docs/sketch-repair-transactions-mvp.md`

Implemented scope:

- `SketchRepairTransactionStatus`, `SketchRepairTransaction`, and `SketchRepairTransactionUndoResult`
- `SketchRepairTransactionExecutor::apply` wrapping one explicit repair command application
- minimal affected-record capture instead of full sketch snapshots
- undo for deterministic fixed endpoint constraints added by repair commands
- undo for removed duplicate fixed endpoint constraints by restoring captured `SketchGeometricConstraint` records
- undo for removed duplicate driving dimensions by restoring captured `SketchDrivingDimension` records
- skipped non-undoable transactions for unsupported command results
- core tests proving apply plus undo restores exact affected records for the safe subset

## Implemented block: Sketch repair undo stack seed

Detailed document: `docs/sketch-repair-undo-stack-mvp.md`

Implemented scope:

- `SketchRepairUndoStackStatus`, `SketchRepairUndoStackResult`, and `SketchRepairUndoStack`
- in-memory storage of applied undoable `SketchRepairTransaction` records
- rejection of non-undoable transactions without changing stack depth
- strict LIFO undo by delegating to `SketchRepairTransactionExecutor::undo`
- structured stack results with transaction status, remaining stack size, and affected record IDs
- core tests proving multiple safe repair transactions undo in reverse application order
- empty-stack undo returning a non-mutating `Empty` result

## Implemented block: Sketch repair undo stack summary seed

Detailed document: `docs/sketch-repair-undo-stack-summary-mvp.md`

Implemented scope:

- `SketchRepairUndoStackSummaryEntry`, `SketchRepairUndoStackSummary`, and `SketchRepairUndoStackSummarizer`
- read-only stack inspection ordered from oldest transaction to newest transaction
- latest-transaction marker for the entry that `undo_latest` would undo next
- exposure of transaction status, action, target, undoable flag, and affected record IDs
- debug JSON output through `serialize_sketch_repair_undo_stack_summary_to_json`
- core tests for empty summaries, stable ordering, latest marker, affected IDs, and debug JSON

## Implemented block: Sketch repair command label seed

Detailed document: `docs/sketch-repair-command-labels-mvp.md`

Implemented scope:

- `SketchRepairCommandLabel` and `SketchRepairCommandLabeler`
- deterministic action labels for all current safe and unsupported repair actions
- undo-oriented labels for `SketchRepairUndoStackSummaryEntry` records
- presentation-only label data kept separate from model intent and `.blcad.json`
- title and description fields in undo-stack summary debug JSON
- core tests for deterministic action labels and summary-entry label JSON

## Implemented block: Sketch repair presentation metadata seed

Detailed document: `docs/sketch-repair-presentation-metadata-mvp.md`

Implemented scope:

- `SketchRepairDisplayCategory` with `safe_apply`, `requires_user_choice`, `requires_parameter_value`, and `undo_entry`
- `SketchRepairDisplayPriority` with `low`, `normal`, and `high`
- `SketchRepairAffectedCounts` for added constraints, removed constraints, removed dimensions, and total count
- `SketchRepairPresentationMetadata` and `SketchRepairPresentationMetadataProvider`
- stable machine-readable `label_id` values for repair actions and undo-stack entries
- concise `affected_summary` generation for undo-stack summary entries
- presentation metadata fields in undo-stack summary debug JSON
- core tests for categories, priorities, stable ids, affected counts, summaries, and JSON output

Still not implemented in this block:

- localization
- icons or GUI widgets
- timestamps
- presentation snapshot rows
- redo metadata or persistent command history
- multi-sketch stack coordination
- parameter-creating repairs
- full solve iteration or exact DOF counting

## Next MVP: Sketch repair presentation snapshot seed

Goal: provide one combined read-only row model for CLI and future GUI code so callers do not need to manually merge summary entries, labels, and metadata.

Proposed first implementation sequence:

- add `SketchRepairPresentationSnapshotEntry` and `SketchRepairPresentationSnapshot` records
- add a `SketchRepairPresentationSnapshotBuilder` that consumes `SketchRepairUndoStackSummary`
- include summary data, title, description, label id, category, priority, affected counts, and affected summary in each snapshot entry
- preserve stack ordering and latest-entry semantics from `SketchRepairUndoStackSummary`
- add a distinct debug JSON schema for presentation snapshots
- keep lower-level summary JSON unchanged for debugging
- add core tests for empty snapshots, multi-entry snapshots, latest-entry metadata, affected-count propagation, and JSON output
- keep localization, icons, GUI widgets, redo, persistent journals, timestamps, multi-sketch grouping, parameter-creating repairs, full solve iteration, exact DOF counting, and arbitrary model rewriting deferred

## Future roadmap: Multi-body transforms and path features

Detailed document: `docs/multi-body-transform-and-path-features-roadmap.md`

Planned scope includes `BodyId`, `Body`, `BodyTransform`, `BodyBooleanFeature`, `SketchOwnership`, `PathCurve`, `ProfileSectionReference`, path-following extrude/cut, two-section loft, multi-section loft, path/guide-curve loft, and associated JSON/project-file records.

## Future roadmap: Inventor-like sketcher and sketch-driven features

Detailed document: `docs/inventor-like-sketcher-and-feature-roadmap.md`

Planned scope includes 2D sketch primitives, sketch editing tools, geometric constraints, driving and driven dimensions, automatic profile and region detection, 3D sketches, richer sketch-driven feature families, and surfacing features.

## Future MVP: Advanced surfacing and 3D sketching

Detailed document: `docs/advanced-surfacing-and-3d-sketch-mvp.md`

Goal: support spatial curves, guide splines, sweep, loft, boundary surfaces, surface stitching, and conversion of closed stitched surfaces into solids.

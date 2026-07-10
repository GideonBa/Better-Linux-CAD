# MVP Plan

This document tracks the current implementation sequence. Detailed technical scope lives in the referenced documents.

## MVP 1: Single-part modeling

Goal: one single parametric part with a headless file-based export path.

Detailed document: `docs/mvp-1-specification.md`

Implemented scope:

- `PartDocument`, parameters with units, datum planes, sketches, and feature-intent records
- rectangle and circle profile seeds
- additive extrude and subtractive through-all cut seeds
- dependency graph, invalidation state, recompute plan, and numeric parameter updates
- optional OCCT geometry execution through `ShapeCache`
- STEP export and headless JSON-to-STEP example
- JSON serialization of the currently JSON-backed model-intent records

## MVP 2: Sketch on planar face and richer sketch-driven profiles

Goal: preserve semantic model intent while sketches, workplanes, references, and profiles become richer.

Implemented feature documents:

- `docs/derived-workplane-mvp2-seed.md`
- `docs/workplane-resolver-mvp2.md`
- `docs/general-closed-sketch-profile-mvp.md`
- `docs/construction-geometry-mvp.md`
- `docs/projected-sketch-reference-geometry.md`
- `docs/reference-recovery-mvp.md`
- `docs/reference-generated-profile-helpers-mvp.md`
- `docs/sketch-constraints-and-dimensions-mvp.md`
- `docs/automatic-profile-region-detection-mvp.md`
- `docs/composite-closed-profile-holes-mvp.md`
- `docs/arc-and-trim-extend-sketch-profile-mvp.md`
- `docs/sketch-plane-extrude-direction-mvp.md`
- `docs/spline-and-tangent-continuity-mvp.md`

Implemented scope summary:

- semantic face-derived workplanes and sketch placement recovery
- reference-driven projected sketch geometry
- construction geometry and chained relations
- line, arc, spline, composite, and automatically detected profile regions
- first sketch constraints and driving dimensions
- sketch-plane-relative extrude direction and geometry recompute support

## Implemented block: Sketch diagnostics, repair, and presentation helpers

Detailed documents:

- `docs/sketch-solver-diagnostics-mvp.md`
- `docs/sketch-repair-suggestions-mvp.md`
- `docs/sketch-repair-commands-mvp.md`
- `docs/sketch-repair-transactions-mvp.md`
- `docs/sketch-repair-undo-stack-mvp.md`
- `docs/sketch-repair-undo-stack-summary-mvp.md`
- `docs/sketch-repair-command-labels-mvp.md`
- `docs/sketch-repair-presentation-metadata-mvp.md`
- `docs/sketch-repair-presentation-snapshot-mvp.md`
- `docs/sketch-repair-presentation-snapshot-query-mvp.md`

Implemented scope summary:

- read-only sketch diagnostics and deterministic repair suggestions
- explicit safe repair commands for selected suggestions
- transaction capture and undo for the safe subset
- in-memory undo stack and stack summaries
- read-only labels, presentation metadata, snapshots, and snapshot queries for future CLI/GUI consumers

## Frozen block: Sketch repair presentation helpers

The sketch-repair presentation chain is complete enough for the current headless stage. It has no consumers outside its own tests yet, and `docs/user-interface.md` keeps GUI work out of scope until the core model path is reliable.

Decision: no further presentation-layer increments such as grouping, sorting, localization, icons, or widgets until a first GUI or CLI consumer exists. Development follows the numbered core-CAD MVP sequence.

## Implemented block: MVP 3 parametric bolt circle pattern

Detailed document: `docs/bolt-circle-pattern-mvp3.md`

Implemented scope:

- `QuantityKind::Count`, `ParameterType::Count`, and count JSON support
- `CircularHolePattern` sketch record with radius, count, and hole-diameter parameters
- dependency edges from pattern parameters to sketches and dependent features
- subtractive geometry recompute that expands the pattern into per-hole through-all cuts
- JSON roundtrip and checked-in `examples/bolt_circle_plate.blcad.json`
- incremental recompute tests for pattern count and radius changes

Still deferred:

- hole wizard semantics such as clearance class, threads, and countersinks
- partial-angle and skipped-instance patterns
- seed-feature patterns that repeat arbitrary features

## Implemented block: MVP 4 seed, assembly parameters shared across parts

Detailed document: `docs/assembly-parameters-mvp4.md`

Implemented scope:

- `AssemblyDocument` with assembly-scoped parameters
- member part registration by `DocumentId`
- `ParameterBinding` records so part parameters can follow assembly parameters
- binding validation for membership, existing assembly parameter, and one binding per part parameter
- `apply_bindings_to(PartDocument&)` using normal `PartDocument::set_parameter_value`
- length/count type agreement at application time
- JSON schema `blcad.assembly_document.mvp4`
- checked-in `examples/flange_assembly.blcad.json`
- geometry end-to-end test for one assembly parameter driving two member plates

## Implemented block: MVP 4 project container for assembly and member parts

Detailed document: `docs/project-container-mvp4.md`

Implemented scope:

- `Project` model owning one `AssemblyDocument` and embedded `PartDocument`s
- validation that every assembly member id resolves to an owned project part document
- automatic binding propagation to affected member parts after a project-level assembly parameter update
- `ProjectPartUpdate` and `ProjectUpdateResult` with per-part recompute plans
- JSON schema `blcad.project.mvp4` using embedded assembly and part documents
- file helpers for reading and writing project JSON
- headless `blcad_export_project` example that loads a project, updates one assembly parameter, recomputes owned parts, and exports each part to STEP
- tests for membership validation, automatic propagation, per-part invalidation, JSON roundtrip, and missing-member rejection

Still deferred:

- component instances, transforms, and assembly constraints
- assembly-level STEP export
- manifest-based project files and external part references
- lazy part loading and dirty-file tracking

## Next MVP: First component instance seed

Goal: start MVP 5 by introducing assembly component instances that reference owned project part documents without duplicating part geometry.

Proposed first implementation sequence:

- add a `ComponentInstance` record owned by `AssemblyDocument` or a closely related assembly-structure model
- store stable instance id, display name, referenced project part document id, visibility, suppression state, and grounded/fixed state
- add an initial rigid transform record for free placement, but do not solve constraints yet
- validate that every component instance references an assembly member part and an owned project part
- add JSON persistence for component instances inside the assembly/project structure
- add project-level tests proving two instances can reference the same part document without duplicating part geometry
- add a minimal headless inspection/export hook that lists component instances and their referenced part documents
- keep mate/concentric/distance constraints, solver, DOF display, collision checks, subassemblies, and assembly-level STEP export deferred

## Future roadmap: Multi-body transforms and path features

Detailed document: `docs/multi-body-transform-and-path-features-roadmap.md`

Planned scope includes `BodyId`, `Body`, `BodyTransform`, `BodyBooleanFeature`, `SketchOwnership`, `PathCurve`, `ProfileSectionReference`, path-following extrude/cut, two-section loft, multi-section loft, path/guide-curve loft, and associated JSON/project-file records.

## Future roadmap: Inventor-like sketcher and sketch-driven features

Detailed document: `docs/inventor-like-sketcher-and-feature-roadmap.md`

Planned scope includes 2D sketch primitives, sketch editing tools, geometric constraints, driving and driven dimensions, automatic profile and region detection, 3D sketches, richer sketch-driven feature families, and surfacing features.

## Future MVP: Advanced surfacing and 3D sketching

Detailed document: `docs/advanced-surfacing-and-3d-sketch-mvp.md`

Goal: support spatial curves, guide splines, sweep, loft, boundary surfaces, surface stitching, and conversion of closed stitched surfaces into solids.

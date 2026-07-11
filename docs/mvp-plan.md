# MVP Plan

## MVP 1: Single-part modeling

Goal: one single parametric part with a headless file-based export path.

Detailed document: `docs/mvp-1-specification.md`

Implemented scope summary:

- part document, parameters with units, datum planes, sketches, and feature-intent records
- rectangle and circle profile seeds
- additive extrude and subtractive through-all cut seeds
- dependency graph, invalidation state, recompute plan, and numeric parameter update
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

- assembly-level STEP export
- manifest-based project files and external part references
- lazy part loading and dirty-file tracking

## Implemented block: MVP 5 component instances and explicit placement/state updates

Detailed document: `docs/component-instance-mvp5.md`

Implemented scope:

- `ComponentInstanceId` typed id
- `ComponentInstance` owned by `AssemblyDocument`
- stable instance id, display name, and referenced project part document id
- visibility, suppression state, and grounding state records
- `RigidTransform` with finite translation values in millimeters and finite rotation values in degrees for free placement
- assembly-level validation that component instances reference member parts
- project-level validation that component instances resolve to owned project parts
- copy-style `ComponentInstance::with_*` operations that preserve identity and referenced part intent
- explicit `AssemblyDocument` updates for transform, visibility, suppression state, and grounding state
- direct transform edits even while an instance is marked grounded, preserving the deliberate no-solver boundary
- assembly/project JSON roundtrip for initial and updated component instance state
- core tests proving two updated component instances can reference one owned `PartDocument` without duplicating the owned part model intent
- checked-in `examples/component_instances.blcad.project.json`
- headless `blcad_inspect_project_components` inspection tool exposing persisted placement/state values

## Implemented block: MVP 5 assembly constraint model-intent seed

Detailed document: `docs/assembly-constraint-model-intent-mvp5.md`

Implemented scope:

- typed `AssemblyConstraintId`
- `AssemblyConstraintType` limited to Mate, Concentric, and Distance
- `AssemblyConstraintTarget` combining a `ComponentInstanceId` with a non-empty persistent semantic reference token
- `AssemblyConstraint` records with stable id, name, type, target A, target B, active/inactive state, and a Distance-only length value
- type-specific validation so Distance requires a length and Mate/Concentric reject distance values
- `AssemblyDocument` ownership with unique constraint-id and existing component-target validation
- constraint creation and loading that never mutate component free-placement transforms
- optional `assembly_constraints` JSON loading with backward compatibility for older files without the field
- assembly/project JSON roundtrip for Mate, Concentric, Distance, semantic targets, state, and distance values
- project structure validation of assembly constraint component targets
- shared owned `PartDocument` intent across multiple constrained component instances
- headless constraint inspection through `blcad_inspect_project_components`
- Mate/Concentric/Distance records in `examples/component_instances.blcad.project.json`

Still deferred:

- semantic target geometry resolution
- constraint graph construction and rigid-body solving
- remaining DOF computation and assembly constraint-state analysis
- enforced grounding and suppression behavior in future assembly consumers
- Insert, Angle, Tangent, Flush, Coincident, and Lock constraints
- collision checks, subassemblies, and assembly-level STEP export

## Next MVP: Read-only assembly constraint graph seed

Goal: derive deterministic component connectivity from the stable persistent assembly constraint records before semantic geometry resolution or rigid-body solving is introduced.

Proposed first implementation sequence:

- add a read-only `AssemblyConstraintGraph` whose nodes are existing `ComponentInstanceId` values
- add graph edges for active `AssemblyConstraintId` records only; inactive constraints must not participate
- preserve each edge's constraint id and two component endpoints without copying semantic geometry or OCCT topology into the graph
- validate graph construction against an `AssemblyDocument` and reject dangling component endpoints if invalid model state is ever observed
- provide deterministic adjacency queries per component instance
- provide deterministic connected-component/group queries so independent assembly groups can be identified
- keep duplicate component pairs legal because several different constraints may relate the same two component instances
- keep graph construction read-only with respect to `RigidTransform`, grounding, constraint state, and part model intent
- add tests for active/inactive edges, isolated component nodes, multiple constraints between one component pair, deterministic adjacency, connected groups, and unchanged component transforms
- optionally extend the headless inspector with a compact graph/group summary after the core graph API is stable
- keep semantic target resolution, rigid-body solving, constraint equations, remaining DOF, enforced grounding, overconstraint detection, and solved transform updates deferred

## Future roadmap: Multi-body transforms and path features

Detailed document: `docs/multi-body-transform-and-path-features-roadmap.md`

Planned scope includes `BodyId`, `Body`, `BodyTransform`, `BodyBooleanFeature`, `SketchOwnership`, `PathCurve`, `ProfileSectionReference`, path-following extrude/cut, two-section loft, multi-section loft, path/guide-curve loft, and associated JSON/project-file records.

## Future roadmap: Inventor-like sketcher and sketch-driven features

Detailed document: `docs/inventor-like-sketcher-and-feature-roadmap.md`

Planned scope includes 2D sketch primitives, sketch editing tools, geometric constraints, driving and driven dimensions, automatic profile and region detection, 3D sketches, richer sketch-driven feature families, and surfacing features.

## Future MVP: Advanced surfacing and 3D sketching

Detailed document: `docs/advanced-surfacing-and-3d-sketch-mvp.md`

Goal: support spatial curves, guide splines, sweep, loft, boundary surfaces, surface stitching, and conversion of closed stitched surfaces into solids.

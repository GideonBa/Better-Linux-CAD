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
- headless `blcad_export_project` parameter-update, recompute, and per-part STEP export path
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
- finite `RigidTransform` translation values in millimeters and rotation values in degrees
- assembly/project reference validation
- copy-style `ComponentInstance::with_*` operations preserving identity and referenced part intent
- explicit transform, visibility, suppression, and grounding updates
- direct transform edits while grounded, preserving the deliberate storage-layer boundary
- assembly/project JSON roundtrip for component placement/state
- shared owned `PartDocument` intent across repeated component occurrences
- checked-in `examples/component_instances.blcad.project.json`
- headless component inspection

## Implemented block: MVP 5 assembly constraint model-intent seed

Detailed document: `docs/assembly-constraint-model-intent-mvp5.md`

Implemented scope:

- typed `AssemblyConstraintId`
- `AssemblyConstraintType` limited to Mate, Concentric, and Distance
- persistent semantic `AssemblyConstraintTarget` records
- stable constraint id/name/type, target A/B, active/inactive state, and Distance-only length value
- type-specific distance validation
- `AssemblyDocument` ownership with unique id and existing component-target validation
- constraint creation/loading without transform mutation
- optional backward-compatible `assembly_constraints` JSON
- assembly/project JSON roundtrip
- project structure validation of constraint component targets
- shared part model intent across constrained occurrences
- headless constraint inspection

Persistent record-layer boundary:

- semantic geometry interpretation remains outside the record layer
- transform evaluation remains outside the record layer
- residual construction remains outside the record layer
- solving and solved transform updates remain outside the record layer

## Implemented block: MVP 5 read-only assembly constraint graph seed

Detailed document: `docs/assembly-constraint-graph-mvp5.md`

Implemented scope:

- read-only `AssemblyConstraintGraph` derived from `AssemblyDocument`
- every component represented as a node, including isolated components
- active constraints represented as distinct edges
- inactive constraints excluded from connectivity
- endpoint identity preserved without copying semantic geometry
- defensive active-edge endpoint validation
- lexicographically deterministic node/edge ordering
- deterministic adjacency and connected-group queries
- legal multi-edges between the same component pair
- no transform, grounding, constraint-state, or part-intent mutation
- headless connected-group summary
- no graph JSON field because connectivity is regenerable

## Implemented block: MVP 5 read-only semantic assembly target resolution

Detailed document: `docs/assembly-constraint-target-resolution-mvp5.md`

Implemented scope:

- read-only `AssemblyConstraintTargetResolver` in the optional geometry layer
- component resolution through the project assembly
- component part resolution to a project-owned `PartDocument`
- parsing and validation of the generated-face semantic token family
- `feature.<feature-id>.{top,bottom,right,left,front,back}` support
- direct reuse of `WorkplaneResolver::resolve_generated_face`
- component-local origin/x-axis/y-axis/normal descriptors
- separate preservation of persisted `RigidTransform`
- explicit malformed/unsupported-reference failures
- semantic axes and generated edge/vertex target families remain unsupported
- no transform, constraint, part-intent, or geometry-cache mutation
- no resolved-target JSON field
- geometry tests for all six faces, failure paths, determinism, and unchanged model intent

## Implemented block: MVP 5 explicit assembly rigid-transform evaluation

Detailed document: `docs/assembly-rigid-transform-evaluation-mvp5.md`

Implemented scope:

- read-only `AssemblyTransformEvaluator`
- distinct `AssemblySpacePlanarDescriptor`
- translation in millimeters and rotation in degrees
- active right-handed fixed-axis X-then-Y-then-Z rotation convention
- column-vector composition `Rz * Ry * Rx`
- points rotated then translated
- vectors, axes, and normals rotated only
- planar-frame evaluation with translation applied only to the origin
- direct compatibility with resolved target local planes and component transforms
- vector magnitude and frame orthogonality preservation within numeric tolerance
- deterministic repeated evaluation
- no input/model mutation
- no transform-evaluation JSON/cache field
- focused identity, translation, single-axis, combined-axis, target-frame, and read-only tests

## Implemented block: MVP 5 read-only planar Mate/Distance equation construction

Detailed document: `docs/assembly-planar-constraint-equations-mvp5.md`

Implemented scope:

- read-only `AssemblyConstraintEquationBuilder` in the optional geometry layer
- `AssemblySpaceConstraintTargetDescriptor` preserving component identity, semantic token, and evaluated assembly-space plane
- `AssemblyConstraintEquationDescriptor` preserving constraint id/type and target A/B order
- active Mate and Distance support for the implemented generated planar face family
- direct reuse of `AssemblyConstraintTargetResolver`
- direct reuse of `AssemblyTransformEvaluator`
- canonical Mate residuals:
  - `normal_opposition = nA + nB`
  - `signed_separation_mm = dot(oB - oA, nA)`
- canonical planar Distance residuals:
  - `normal_parallelism = cross(nA, nB)`
  - `signed_separation_mm = dot(oB - oA, nA)`
  - `distance_residual_mm = signed_separation_mm - target_distance_mm`
- explicit target-order dependence for signed Distance separation
- inactive-constraint rejection before geometry resolution
- explicit Concentric rejection until semantic axis targets exist
- deterministic target-A-then-target-B resolution and error precedence
- residual descriptors remain regenerable and unpersisted
- no transform, constraint, or part-intent mutation
- focused tests for satisfied/unsatisfied Mate, transformed target geometry, satisfied/unsatisfied Distance, target-order sign, unsupported families, determinism, and read-only behavior

## Implemented block: MVP 5 first rigid-body assembly solver seed

Detailed document: `docs/assembly-rigid-body-solver-mvp5.md`

Implemented scope:

- `AssemblyRigidBodySolver` over one exact deterministic `AssemblyConstraintGraph` connected group
- at least one grounded component required as an absolute fixed reference
- every grounded component treated as fixed
- multiple grounded components allowed
- satisfied all-grounded groups return `Converged`
- inconsistent all-grounded groups return `FixedGeometryInconsistent`
- suppressed group components rejected explicitly in the first seed
- visibility excluded from solver participation policy
- direct six-variable `RigidTransform` representation per free component: `tx,ty,tz,rx_deg,ry_deg,rz_deg`
- lexicographic free-component variable ordering
- lexicographic `AssemblyConstraintId` residual ordering inherited from the graph
- four flattened components per supported Mate/Distance residual
- explicit `1 mm` default length-residual scale before mixing length and orientation residuals
- central finite-difference Jacobian with separate translation/rotation perturbation steps
- damped Gauss-Newton normal equations
- partial-pivot Gaussian elimination
- deterministic backtracking line search and damping escalation
- explicit convergence RMS and maximum-iteration policy
- `Converged`, `MaximumIterationsReached`, `FixedGeometryInconsistent`, and `NumericalFailure` solve states
- source-project immutability during every solve outcome
- `AssemblySolveResult` with residual summary, component snapshots, and proposed free-component transforms
- separate `AssemblySolveResultApplier`
- stale solve-input detection including changed grounded anchors
- successful transform application through a private project copy followed by atomic project replacement
- no solver-result, Jacobian, iteration, damping, proposal, or residual-summary JSON fields
- focused Mate, Distance, orientation, multi-constraint chain, deterministic ordering, fixed-group, validation, failure, stale-result, and application tests

Still deferred after the first solver seed:

- remaining rigid-body DOF computation
- Jacobian rank diagnostics
- underconstrained, locally fully constrained, and overconstrained classification
- persistent solve/DOF cache data
- semantic axis references and Concentric residual/solve support
- component geometry instancing and assembly-level STEP export

## Next MVP: Read-only assembly solve diagnostics and remaining-DOF analysis

Goal: classify the local constraint state of a solved or evaluable connected group without introducing persistent DOF cache state.

Proposed first implementation sequence:

- reuse the solver's deterministic connected-group, constraint, residual-component, and variable ordering
- expose or reuse one read-only numeric Jacobian evaluation path at a selected transform state
- define a documented Jacobian-rank tolerance
- compute variable count, local Jacobian rank, constrained DOF, and remaining DOF
- distinguish underconstrained and locally fully constrained groups
- carry forward `FixedGeometryInconsistent` and non-converged solver diagnostics explicitly
- define an initial overconstraint/inconsistency diagnostic boundary without pretending redundant equations always imply semantic overconstraint
- return regenerable diagnostic descriptors without mutating transforms or persisting DOF state
- test single-Mate remaining freedom, planar Distance freedom, a chained group, fully fixed groups, deterministic rank results, and numeric tolerance behavior
- keep Concentric and richer constraint families outside this block until their semantic references and residuals exist

## Future roadmap: Multi-body transforms and path features

Detailed document: `docs/multi-body-transform-and-path-features-roadmap.md`

Planned scope includes `BodyId`, `Body`, `BodyTransform`, `BodyTransformStack`, `BodyBooleanFeature`, `SketchOwnership`, `PathCurve`, `ProfileSectionReference`, path-following extrude/cut, two-section loft, multi-section loft, path/guide-curve loft, and associated JSON/project-file records.

## Future roadmap: Inventor-like sketcher and sketch-driven features

Detailed document: `docs/inventor-like-sketcher-and-feature-roadmap.md`

Planned scope includes 2D sketch primitives, sketch editing tools, geometric constraints, driving and driven dimensions, automatic profile and region detection, 3D sketches, richer sketch-driven feature families, and surfacing features.

## Future MVP: Advanced surfacing and 3D sketching

Detailed document: `docs/advanced-surfacing-and-3d-sketch-mvp.md`

Goal: support spatial curves, guide splines, sweep, loft, boundary surfaces, surface stitching, and conversion of closed stitched surfaces into solids.

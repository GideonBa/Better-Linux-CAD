# MVP Plan

This document is the implementation-sequence source of truth. Feature-specific documents are canonical for detailed contracts.

## MVP 1: Single-part modeling

Goal: one parametric part with a headless file-based export path.

Canonical document: `docs/mvp-1-specification.md`.

Implemented:

- part documents, typed parameters with units, datum planes, sketches, and feature-intent records
- rectangle and circle profile seeds
- additive extrude and subtractive through-all cut seeds
- dependency graph, invalidation state, recompute plan, and parameter updates
- optional OCCT geometry execution through `ShapeCache`
- STEP export and headless JSON-to-STEP examples
- JSON serialization of current JSON-backed model-intent records

## MVP 2: Semantic references and richer sketch-driven profiles

Implemented feature documents include:

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

Implemented scope includes semantic face-derived workplanes, reference-driven projected geometry, construction geometry and chained relations, line/arc/spline/composite/detected profile regions, first sketch constraints and dimensions, and sketch-plane-relative extrude direction.

## Implemented sketch diagnostics and repair helpers

Canonical documents:

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

The presentation-helper chain is frozen until a first GUI or CLI consumer exists. Core CAD work follows the numbered MVP sequence.

## MVP 3: Parametric bolt circle pattern

Canonical document: `docs/bolt-circle-pattern-mvp3.md`.

Implemented:

- count quantities and count parameters
- `CircularHolePattern` with radius, count, hole-diameter, center, and angle-offset intent
- parameter-to-sketch dependency edges
- subtractive recompute expansion into per-hole through-all cuts
- incremental count/radius recompute
- JSON roundtrip and checked-in headless example

Still deferred: hole-wizard semantics, partial/skipped patterns, and arbitrary seed-feature patterns.

## MVP 4: Assembly parameters and project container

Canonical documents:

- `docs/assembly-parameters-mvp4.md`
- `docs/project-container-mvp4.md`

Implemented:

- assembly-scoped parameters
- member-part registration and `ParameterBinding`
- binding propagation through normal part invalidation
- one `Project` owning an assembly and embedded part documents
- project-level assembly-parameter updates with per-part recompute plans
- embedded assembly/project JSON
- headless project parameter-update, recompute, and per-part STEP export

Assembly-level STEP export and manifest/external-file project containers remain deferred.

## MVP 5: Assembly relationship and rigid-body pipeline

### 1. Component instances and explicit placement/state updates

Canonical document: `docs/component-instance-mvp5.md`.

Implemented:

- `ComponentInstanceId` and `ComponentInstance`
- repeated occurrences referencing one project-owned `PartDocument`
- visibility, suppression, grounding, and finite `RigidTransform`
- explicit placement/state update APIs
- direct storage-level transform edits while grounded
- assembly/project JSON roundtrip
- headless component inspection

### 2. Solver-independent assembly constraint intent

Canonical document: `docs/assembly-constraint-model-intent-mvp5.md`.

Implemented:

- `AssemblyConstraintId`
- Mate, Concentric, and Distance record types
- semantic `AssemblyConstraintTarget` strings
- active/inactive state
- Distance-only positive length value
- assembly ownership and component-target validation
- assembly/project JSON roundtrip
- no transform mutation during constraint creation/loading

### 3. Deterministic active-constraint graph

Canonical document: `docs/assembly-constraint-graph-mvp5.md`.

Implemented:

- every component as a node, including isolated occurrences
- active constraints as distinct multi-edges
- inactive constraints excluded from connectivity
- lexicographic node, edge, adjacency, and connected-group order
- read-only graph construction
- exact connected groups as solver partition boundaries
- no graph JSON/cache field

### 4. Generated planar-face target resolution

Canonical document: `docs/assembly-constraint-target-resolution-mvp5.md`.

Implemented planar family:

```text
feature.<feature-id>.top
feature.<feature-id>.bottom
feature.<feature-id>.right
feature.<feature-id>.left
feature.<feature-id>.front
feature.<feature-id>.back
```

Implemented:

- component and project-owned part lookup
- generated-face token parsing
- supported additive-extrude validation
- reuse of `WorkplaneResolver::resolve_generated_face`
- component-local planar descriptors
- separate component-transform preservation
- explicit failure paths and read-only behavior

The same resolver also owns the separate generated-axis family described in step 9.

### 5. Explicit rigid-transform evaluation

Canonical document: `docs/assembly-rigid-transform-evaluation-mvp5.md`.

Canonical convention:

```text
translation_mm in millimeters
rotation_deg in degrees
active right-handed fixed-axis rotations
apply X, then Y, then Z
R = Rz * Ry * Rx for column vectors
```

Implemented:

- point evaluation: rotate then translate
- vector evaluation: rotate only
- planar-frame evaluation
- generated-axis evaluation
- separate component-local and assembly-space descriptor types
- deterministic read-only behavior
- no evaluated-geometry persistence

### 6. Planar Mate/Distance residual construction

Canonical document: `docs/assembly-planar-constraint-equations-mvp5.md`.

Implemented residuals:

```text
Mate:
  normal_opposition     = nA + nB
  signed_separation_mm  = dot(oB - oA, nA)

Distance:
  normal_parallelism    = cross(nA, nB)
  signed_separation_mm  = dot(oB - oA, nA)
  distance_residual_mm  = signed_separation_mm - target_distance_mm
```

Target A/B order is preserved because signed Distance semantics are order-dependent.

The planar builder remains Mate/Distance-specific. Concentric uses the dedicated builder introduced in step 9.

### 7. Deterministic rigid-body solver and explicit result application

Canonical document: `docs/assembly-rigid-body-solver-mvp5.md`.

Implemented:

- one exact deterministic graph connected group as solver input
- at least one grounded component required
- every grounded component fixed; multiple grounded components supported
- suppressed group components rejected; visibility ignored for participation
- six direct variables per free component: `tx,ty,tz,rx_deg,ry_deg,rz_deg`
- lexicographic variable and constraint ordering
- shared Mate/Distance/Concentric numeric residual evaluation
- explicit default `1 mm` residual length scale
- central finite-difference Jacobian
- damped Gauss-Newton normal equations
- partial-pivot Gaussian elimination
- damping escalation and deterministic backtracking line search
- explicit solve states
- source-project immutability during solve
- proposed transforms and complete component snapshots
- separate `AssemblySolveResultApplier`
- stale-input detection including grounded anchors
- atomic successful application through a project copy

Concentric reaches the solver only through the shared numeric-system path described in step 10. No Concentric-specific optimizer branch exists.

### 8. Read-only Jacobian-rank and remaining-DOF diagnostics

Canonical document: `docs/assembly-solve-diagnostics-mvp5.md`.

Implemented:

- one shared private numeric-system path used by solver and diagnostics
- identical residual flattening, variable ordering, finite-difference steps, and Jacobian construction for both consumers
- Mate, Distance, and Concentric Jacobian evaluation
- `AssemblySolveDiagnosticsAnalyzer`
- configurable absolute/relative rank tolerances
- deterministic row-echelon rank computation with maximum-magnitude row pivot selection
- pivot threshold:

```text
max(rank_absolute_tolerance,
    rank_relative_tolerance * maximum_abs_jacobian_entry)
```

- `variable_count = 6 * free_component_count`
- `constrained_dof = rank(J)`
- `remaining_dof = variable_count - rank(J)`
- `NoVariableDof`, `Underconstrained`, and `LocallyFullyConstrained`
- separate consistency classification
- residual-row rank structure without equating redundant rows with semantic overconstraint
- rank evaluation only at a converged private solver state
- no source-project mutation or diagnostic JSON cache

The result is local and linearized in the solver's direct `RigidTransform` coordinates.

### 9. Semantic generated-axis references and read-only Concentric residuals

Canonical document: `docs/assembly-semantic-axis-concentric-residuals-mvp5.md`.

Implemented semantic axis family:

```text
feature.<feature-id>.axis
```

The first supported producer is one `SubtractiveExtrude` whose input sketch contains exactly one `CircleProfile` and exactly one total profile.

Implemented:

- `SemanticAxis::Primary`
- `SemanticAxisReference` with stable `feature.<feature-id>.axis` node identity
- `ComponentLocalAxisDescriptor`
- `ResolvedAssemblyAxisConstraintTarget`
- `AssemblyConstraintTargetResolver::resolve_axis`
- exact source feature and source circle-profile identity preservation
- axis origin from `CircleProfile.center` mapped through the resolved source-sketch workplane
- axis direction from the same workplane normal and the feature's `ExtrudeDirection`
- explicit exclusion of ambiguous multi-axis `CircularHolePattern` tokens
- `AssemblySpaceAxisDescriptor`
- `AssemblyTransformEvaluator::evaluate_axis`
- dedicated `AssemblyConcentricConstraintEquationBuilder`
- deterministic target-A-then-target-B resolution
- canonical Concentric residuals:

```text
direction_parallelism = cross(dA, dB)
axis_offset_mm         = cross(oB - oA, dA)
```

- equal and opposed axis directions accepted
- axial translation intentionally absent from the residual
- target-order-sensitive raw offset-vector convention
- read-only, regenerable local/assembly axis and residual descriptors
- no new project/assembly JSON field

### 10. Concentric numeric-system, solver, and DOF integration

Canonical document: `docs/assembly-concentric-numeric-solver-dof-mvp5.md`.

Implemented exact Concentric flattening:

```text
direction_parallelism.x
direction_parallelism.y
direction_parallelism.z
axis_offset_mm.x / length_residual_scale_mm
axis_offset_mm.y / length_residual_scale_mm
axis_offset_mm.z / length_residual_scale_mm
```

Implemented:

- constraint-type selection inside the one shared assembly numeric residual evaluator
- unchanged Mate/Distance flattening
- six Concentric scalar residual components
- reuse of lexicographic graph constraint ordering
- reuse of persistent target A/B order
- reuse of six-variable free-component ordering
- reuse of central finite-difference Jacobian construction
- reuse of existing damped Gauss-Newton, damping, and line-search policy
- lateral axis-offset solving
- axis-tilt solving
- equal and opposed valid axis directions
- preserved axial translation and rotation-about-axis freedom in the regular centered-axis case
- source-project immutability
- complete solve-input snapshots
- stale-result detection
- explicit atomic successful result application
- all-grounded Concentric inconsistency
- explicit non-converged Concentric boundaries
- unchanged semantic target failure propagation
- regular one-free-body Concentric Jacobian rank:

```text
variable_count           = 6
residual_component_count = 6
jacobian_rank            = 4
constrained_dof          = 4
remaining_dof            = 2
```

- mixed Distance/Concentric deterministic residual order and dimension `10`
- regular mixed Distance/Concentric rank `5/6` with one remaining rotation-about-axis DOF
- no persistent residual/Jacobian/solve/DOF cache

## Next MVP: Stable Insert intent and read-only composite residual semantics

Goal: define Insert as explicit assembly model intent over stable semantic circular-feature geometry before allowing it to participate in the numeric solver.

Insert must not be treated as an undocumented Concentric relationship plus an arbitrary axial offset.

Proposed implementation sequence:

1. Define stable semantic axial-seating plane references for the first supported circular-cut feature family without raw OCCT topology ids.
2. Preserve exact source feature/profile identity and deterministic component-local seating geometry.
3. Evaluate seating planes in assembly space through the existing `AssemblyTransformEvaluator` convention.
4. Introduce `AssemblyConstraintType::Insert` with explicit record and JSON compatibility semantics.
5. Define the target contract required to derive both one semantic axis and one seating plane per Insert endpoint.
6. Define canonical axis alignment semantics, including whether equal/opposed directions are accepted.
7. Define canonical seating-plane normal orientation and signed axial seating in persistent target A/B order.
8. Construct a dedicated read-only Insert residual descriptor from semantic axes plus seating planes.
9. Keep the Insert residual path derived and unpersisted.
10. Prove the regular Insert residual Jacobian has local rank five over six rigid-body variables and one remaining rotation-about-axis DOF.
11. Test target order, semantic geometry failures, deterministic construction, read-only behavior, and JSON roundtrip of Insert model intent.
12. Keep Insert numeric-system/solver/application integration as the following block after target and residual semantics are stable.

No persistent solved transform, residual, Jacobian, or DOF cache is required.

## Proposed assembly implementation sequence

1. Component instances and grounding. Implemented.
2. Explicit placement/state updates. Implemented.
3. Mate/Concentric/Distance records and semantic targets. Implemented.
4. Persistence and inspection without transform solving. Implemented.
5. Read-only active-constraint graph. Implemented.
6. Generated-face planar target resolution. Implemented.
7. Explicit rigid-transform evaluation. Implemented.
8. Planar Mate/Distance residual construction. Implemented.
9. First rigid-body solver and explicit successful result application. Implemented.
10. Jacobian-rank and remaining-DOF diagnostics. Implemented.
11. Semantic generated-axis references and read-only Concentric residual construction. Implemented.
12. Concentric numeric-system, solver, and DOF integration. Implemented.
13. Add solved-state or DOF cache records only if a later consumer requires data that cannot be safely regenerated. No current requirement.
14. Stable Insert intent and read-only composite residual semantics. Next.
15. Insert numeric-system, solver, and DOF integration.
16. Add richer constraint families.
17. Add joints and limits, then motion through the solver.
18. Add rigid subassemblies first, flexible subassemblies later, collision/interference checks, component geometry instancing, and assembly-level STEP export.

## Future roadmaps

- Multi-body and path features: `docs/multi-body-transform-and-path-features-roadmap.md`
- Inventor-like sketcher and feature parity: `docs/inventor-like-sketcher-and-feature-roadmap.md`
- Advanced surfacing and 3D sketches: `docs/advanced-surfacing-and-3d-sketch-mvp.md`

Later assembly work includes Insert solver integration, richer constraint families, per-component/null-space DOF presentation, joints and limits, motion, rigid then flexible subassemblies, collision/interference checks, component geometry instancing, and assembly-level STEP export.

## Persistence rule

Persist model intent. Regenerate graph connectivity, resolved planar/axis targets, assembly-space planes/axes, residual descriptors, numeric Jacobians, solve results, rank summaries, and remaining-DOF diagnostics.

Only explicitly applied successful transform proposals change the existing persisted component `RigidTransform` model intent.

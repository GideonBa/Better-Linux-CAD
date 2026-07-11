# MVP Plan

This document is the maintained implementation sequence for BLCAD. Feature-specific documents remain canonical for exact APIs, formulas, validation, and tests.

## MVP 1: Single-part parametric core

Goal: one parametric part with a headless file-based export path.

Canonical document: `docs/mvp-1-specification.md`.

Implemented scope:

- `PartDocument`, typed parameters and units
- datum planes and simple sketches
- rectangle and circle profile seeds
- additive extrude and subtractive through-all cut seeds
- dependency graph, invalidation state, and recompute planning
- numeric parameter updates
- optional OCCT geometry execution through `ShapeCache`
- STEP export
- JSON model-intent serialization and file helpers
- headless JSON-to-STEP workflow

## MVP 2: Semantic sketch/workplane/profile path

Goal: preserve design intent while sketches, references, workplanes, and profile geometry become richer.

Implemented documents include:

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

Implemented scope includes semantic face-derived workplanes, construction geometry, projected references, reference recovery, line/arc/spline profile paths, composite profiles with holes, automatic region detection, first sketch constraints/dimensions, and sketch-plane-relative feature direction.

## Implemented sketch diagnostics and repair chain

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

The current chain provides read-only diagnostics, deterministic repair suggestions, an explicit safe command subset, transactions, undo, stack summaries, and read-only presentation helpers.

This presentation chain is frozen until a real CLI or GUI consumer exists. Localization, icons, richer grouping, and widget work do not belong ahead of the core CAD sequence.

## MVP 3: Parametric bolt circle

Canonical document: `docs/bolt-circle-pattern-mvp3.md`.

Implemented scope:

- `QuantityKind::Count` and count parameters
- `CircularHolePattern` intent
- radius/count/hole-diameter dependencies
- subtractive recompute expanding the pattern into through-all circular cuts
- JSON roundtrip
- `examples/bolt_circle_plate.blcad.json`
- incremental recompute coverage for radius and count changes

Still deferred: hole-wizard semantics, skipped/partial patterns, and arbitrary seed-feature patterns.

## MVP 4: Assembly parameters and project container

Canonical documents:

- `docs/assembly-parameters-mvp4.md`
- `docs/project-container-mvp4.md`

Implemented scope:

- assembly-scoped parameters
- member-part registration
- explicit `ParameterBinding` records
- part-parameter propagation through normal `PartDocument::set_parameter_value`
- length/count agreement validation
- `Project` ownership of one `AssemblyDocument` and embedded `PartDocument` values
- project assembly-structure validation
- automatic binding propagation after project-level assembly parameter updates
- per-part recompute plans
- project JSON and file helpers
- headless project parameter-update/recompute/per-part STEP export

Assembly-level STEP export, external/manifest part references, lazy part loading, and dirty-file tracking remain deferred.

## MVP 5 assembly sequence

### 1. Component instances and explicit placement/state

Canonical document: `docs/component-instance-mvp5.md`.

Implemented:

- `ComponentInstanceId`
- component occurrence identity independent from part identity
- references to project-owned member parts
- visibility, suppression, and grounding state
- finite `RigidTransform` translation in millimeters and rotation in degrees
- explicit placement/state updates
- repeated occurrences sharing one owned `PartDocument`
- assembly/project JSON roundtrip
- headless component inspection

### 2. Assembly constraint model intent

Canonical document: `docs/assembly-constraint-model-intent-mvp5.md`.

Implemented:

- `AssemblyConstraintId`
- persistent `AssemblyConstraintTarget`
- Mate, Concentric, and Distance record types
- target A/B identity and order
- active/inactive state
- Distance-only length quantity
- assembly ownership and target validation
- additive backward-compatible `assembly_constraints` JSON
- project structure validation

The record layer remains independent from semantic geometry resolution, transform evaluation, residual construction, solving, and DOF diagnostics.

### 3. Deterministic active-constraint graph

Canonical document: `docs/assembly-constraint-graph-mvp5.md`.

Implemented:

- every component as a node, including isolated occurrences
- active constraints as distinct edges
- inactive constraints excluded from connectivity
- deterministic lexicographic node/edge/adjacency/group order
- legal multi-edges
- deterministic connected-component partitioning
- read-only derived graph with no JSON cache field

### 4. Generated planar face target resolution

Canonical document: `docs/assembly-constraint-target-resolution-mvp5.md`.

Implemented:

- read-only `AssemblyConstraintTargetResolver`
- project component and referenced-part resolution
- supported `feature.<feature-id>.{top,bottom,right,left,front,back}` tokens
- shared generated-face geometry through `WorkplaneResolver::resolve_generated_face`
- component-local planar descriptors
- separate preservation of component placement intent
- explicit unsupported/malformed target diagnostics

Semantic axes and generated edge/vertex assembly target families remain unsupported.

### 5. Explicit rigid-transform evaluation

Canonical document: `docs/assembly-rigid-transform-evaluation-mvp5.md`.

Implemented:

- `AssemblyTransformEvaluator`
- `AssemblySpacePlanarDescriptor`
- translation in millimeters
- rotation in degrees
- active right-handed fixed-axis X-then-Y-then-Z convention
- column-vector composition `Rz * Ry * Rx`
- points rotated then translated
- vectors/axes/normals rotated only
- deterministic read-only evaluation

### 6. Planar Mate/Distance residual construction

Canonical document: `docs/assembly-planar-constraint-equations-mvp5.md`.

Implemented canonical residuals:

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

### 7. First deterministic rigid-body solver

Canonical document: `docs/assembly-rigid-body-solver-mvp5.md`.

Implemented:

- one exact deterministic graph connected group as solver input
- at least one grounded component required
- every grounded component fixed
- multiple grounded components supported
- suppressed group components rejected explicitly
- visibility ignored by solver participation
- six direct variables per free component: `tx,ty,tz,rx_deg,ry_deg,rz_deg`
- lexicographic variable and constraint ordering
- four flattened scalar residual components per supported planar constraint
- explicit default `1 mm` residual length scale
- central finite-difference Jacobian
- deterministic damped Gauss-Newton normal equations
- partial-pivot Gaussian elimination
- damping escalation and backtracking line search
- explicit solve states
- source-project immutability during solve
- proposed transforms in `AssemblySolveResult`
- source component snapshots
- separate `AssemblySolveResultApplier`
- stale-input detection including grounded anchors
- atomic successful application through a project copy

### 8. Read-only Jacobian-rank and remaining-DOF diagnostics

Canonical document: `docs/assembly-solve-diagnostics-mvp5.md`.

Implemented:

- one shared private assembly numeric-system path used by solver and diagnostics
- identical residual flattening, free-component variable order, finite-difference steps, and central Jacobian construction for both consumers
- `AssemblySolveDiagnosticsAnalyzer`
- configurable finite non-negative absolute/relative rank tolerances that may not both be zero
- deterministic row-echelon rank computation with maximum-magnitude row pivot selection
- pivot threshold:

```text
max(rank_absolute_tolerance,
    rank_relative_tolerance * maximum_abs_jacobian_entry)
```

- `variable_count = 6 * free_component_count`
- `constrained_dof = rank(J)`
- `remaining_dof = variable_count - rank(J)`
- `NoVariableDof`, `Underconstrained`, and `LocallyFullyConstrained` classifications
- separate `LocallyConsistent`, `FixedGeometryInconsistent`, and `SolverDidNotConverge` consistency classification
- explicit residual-row rank structure without claiming semantic overconstraint from redundant equations alone
- rank evaluation only at a converged private solver state
- no source-project mutation
- no Jacobian/rank/DOF/diagnostic JSON cache

This is a local linearized DOF result in the solver's direct `RigidTransform` coordinates. It is not a global configuration-space dimension or singularity analysis.

## Next MVP: Semantic generated-axis references and Concentric residuals

Goal: remove the current semantic-reference blocker for Concentric without prematurely coupling solver logic to raw OCCT topology.

Proposed implementation sequence:

1. Define a stable semantic generated-axis token family produced by supported feature intent.
2. Introduce a component-local axis descriptor with point/origin and unit direction.
3. Resolve axis targets through component occurrence -> project-owned `PartDocument` -> supported source feature intent.
4. Reuse `AssemblyTransformEvaluator` semantics: transform axis points with rotation+translation and axis directions with rotation only.
5. Introduce a distinct assembly-space axis descriptor.
6. Extend the target/equation path with explicit planar-target versus axis-target typing rather than overloading planar descriptors.
7. Define canonical Concentric residuals for axis direction parallelism and shortest line-to-line offset.
8. Preserve deterministic target A/B handling and failure precedence.
9. Keep the new target/residual path read-only and unpersisted.
10. Add focused semantic-axis resolution, transform, Concentric residual, determinism, unsupported-family, and read-only tests.
11. Only after target/residual semantics are stable, integrate Concentric into the rigid-body solver and DOF diagnostics.
12. Keep Insert downstream because it combines Concentric axis alignment with axial seating semantics.

## Future roadmap

Multi-body and path features: `docs/multi-body-transform-and-path-features-roadmap.md`.

Inventor-like sketcher and feature parity: `docs/inventor-like-sketcher-and-feature-roadmap.md`.

Advanced surfacing and 3D sketches: `docs/advanced-surfacing-and-3d-sketch-mvp.md`.

Later assembly work includes Concentric solver integration, Insert and richer constraint families, per-component/null-space DOF presentation, joints and limits, motion, rigid then flexible subassemblies, collision/interference checks, component geometry instancing, and assembly-level STEP export.

## Persistence rule

Persist model intent. Regenerate graph connectivity, resolved targets, assembly-space frames, residual descriptors, numeric Jacobians, solve results, rank summaries, and remaining-DOF diagnostics.

Only explicitly applied successful transform proposals change the existing persisted component `RigidTransform` model intent.

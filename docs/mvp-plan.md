# MVP Plan

This document is the implementation-sequence source of truth. Feature-specific documents are canonical for exact contracts.

## MVP 1: Single-part modeling

Canonical document: `docs/mvp-1-specification.md`.

Implemented:

- part documents and typed quantities/parameters
- datum planes and sketches
- rectangle/circle profile seeds
- additive extrude and subtractive through-all cut intent
- dependency graph, invalidation, recompute planning, and parameter updates
- optional OCCT execution through `ShapeCache`
- STEP export and headless JSON-to-STEP examples
- JSON serialization of current model-intent records

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

Implemented scope includes semantic face-derived workplanes, projected/reference-driven geometry, construction geometry and chained relations, line/arc/spline/composite/detected profile regions, first sketch constraints/dimensions, and sketch-plane-relative extrude direction.

### Sketch diagnostics and repair helpers

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
- `CircularHolePattern` radius/count/hole-diameter/center/angle-offset intent
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
- one `Project` owning an assembly and embedded parts
- project-level assembly-parameter updates with per-part recompute plans
- embedded assembly/project JSON
- headless project parameter update, recompute, and per-part STEP export

Assembly-level STEP export and manifest/external-file project containers remain deferred.

## MVP 5: Assembly relationship and rigid-body pipeline

### 1. Component instances and explicit placement/state

Canonical document: `docs/component-instance-mvp5.md`.

Implemented component occurrence identity, repeated part reuse, visibility, suppression, grounding, finite `RigidTransform`, explicit placement/state updates, JSON roundtrip, and headless inspection.

### 2. Solver-independent constraint intent

Canonical document: `docs/assembly-constraint-model-intent-mvp5.md`.

Implemented persistent relationship families:

```text
Mate
Concentric
Distance
Insert
```

All use persistent target A/B semantic strings and active/inactive state. Distance alone stores a positive length quantity. Constraint creation/loading never moves components.

### 3. Deterministic active-constraint graph

Canonical document: `docs/assembly-constraint-graph-mvp5.md`.

Implemented every component as a node, active constraints as distinct multi-edges, inactive filtering, lexicographic ordering, deterministic connected groups, exact solver partition boundaries, and no graph cache persistence.

### 4. Semantic target resolution

Canonical document: `docs/assembly-constraint-target-resolution-mvp5.md`.

Implemented planar generated-face family:

```text
feature.<feature-id>.top
feature.<feature-id>.bottom
feature.<feature-id>.right
feature.<feature-id>.left
feature.<feature-id>.front
feature.<feature-id>.back
```

Implemented primary circular-feature axis family:

```text
feature.<feature-id>.axis
```

Implemented primary circular-feature seating family:

```text
feature.<feature-id>.seat
```

The first axis/seat producer is one `SubtractiveExtrude` whose source sketch contains exactly one `CircleProfile` and one total profile. Axis and seat resolution preserve exact feature/profile identity and remain component-local with separate component placement.

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

Points rotate then translate. Vectors rotate only. Planar frames and axis lines are evaluated read-only into assembly space and are never persisted.

### 6. Planar Mate/Distance residuals

Canonical document: `docs/assembly-planar-constraint-equations-mvp5.md`.

```text
Mate:
  normal_opposition    = nA + nB
  signed_separation_mm = dot(oB - oA, nA)

Distance:
  normal_parallelism   = cross(nA, nB)
  signed_separation_mm = dot(oB - oA, nA)
  distance_residual_mm = signed_separation_mm - target_distance_mm
```

Target order remains persistent intent.

### 7. Deterministic rigid-body solver and explicit application

Canonical document: `docs/assembly-rigid-body-solver-mvp5.md`.

Implemented:

- exactly one deterministic connected group per solve
- at least one grounded component
- every grounded component fixed
- suppressed selected components rejected; visibility ignored
- six direct variables per free component: `tx,ty,tz,rx_deg,ry_deg,rz_deg`
- lexicographic component and constraint ordering
- shared Mate/Distance/Concentric residual evaluation
- central finite-difference Jacobian
- damped Gauss-Newton normal equations
- partial-pivot Gaussian elimination
- deterministic backtracking line search and damping escalation
- explicit solve states
- source-project immutability during solve
- complete solve-input snapshots and transform proposals
- stale-result detection including moved grounded anchors
- explicit atomic successful application through `AssemblySolveResultApplier`

Insert is not yet a solver residual family.

### 8. Local Jacobian-rank and remaining-DOF diagnostics

Canonical document: `docs/assembly-solve-diagnostics-mvp5.md`.

Solver and diagnostics share the same private residual/variable/finite-difference Jacobian path for Mate, Distance, and Concentric.

```text
variable_count = 6 * free_component_count
constrained_dof = rank(J)
remaining_dof = variable_count - rank(J)
```

Rank uses the documented absolute/relative pivot threshold. DOF, consistency, and residual-row rank structure remain separate classifications. The analysis is local in direct persisted `RigidTransform` coordinates and is unpersisted.

### 9. Semantic axes and Concentric residuals

Canonical document: `docs/assembly-semantic-axis-concentric-residuals-mvp5.md`.

```text
direction_parallelism = cross(dA, dB)
axis_offset_mm         = cross(oB - oA, dA)
```

Equal and opposed directions are accepted. Axial translation and rotation about the common axis remain free. The target and residual path is deterministic and read-only.

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

Concentric reuses the same graph order, free-component variable order, finite differences, damped Gauss-Newton path, solve states, snapshots, stale-result checks, and explicit application boundary as planar constraints.

Proven regular one-free-body result:

```text
variable_count           = 6
residual_component_count = 6
jacobian_rank            = 4
constrained_dof          = 4
remaining_dof            = 2
```

Mixed Distance/Concentric is also covered as rank `5/6` with one remaining rotation-about-axis DOF.

### 11. Stable Insert intent and read-only composite residual semantics

Canonical document: `docs/assembly-insert-intent-composite-residuals-mvp5.md`.

Implemented:

- `AssemblyConstraintType::Insert`
- string/JSON spelling `insert`
- distance-free Insert record semantics
- stable `SemanticSeatingPlane::Primary`
- stable `SemanticSeatingPlaneReference`
- `feature.<feature-id>.seat` node identity
- first `.seat` producer on the same single-circle `SubtractiveExtrude` family as `.axis`
- `ResolvedAssemblyInsertConstraintTarget`
- exact source feature/profile identity preservation
- component-local primary axis plus oriented seating plane from one persistent seat target
- seat origin at the mapped `CircleProfile.center`
- seat normal canonically aligned with the semantic axis direction
- right-handed frame preservation for `OppositeSketchNormal`
- assembly-space axis/seat evaluation through the existing transform evaluator
- dedicated `AssemblyInsertConstraintEquationBuilder`
- deterministic target-A-then-target-B resolution
- canonical composite residual:

```text
direction_parallelism       = cross(dA, dB)
axis_offset_mm               = cross(oB - oA, dA)
signed_seating_separation_mm = dot(sB - sA, nA)
```

- equal and opposed axis directions accepted
- target-A-defined signed seating direction
- no duplicate seating-normal residual
- seven scalar residual components
- direct central finite-difference `7 x 6` Jacobian proof
- regular Jacobian rank `5`
- one remaining rotation-about-axis DOF
- read-only target/residual construction
- assembly JSON roundtrip without schema-shape change
- explicit solver rejection until the numeric integration block below
- no persistent target/residual/Jacobian/DOF cache

### 12. Insert numeric-system, solver, and DOF integration

Canonical document: `docs/assembly-insert-numeric-solver-dof-mvp5.md`.

Implemented exact Insert flattening:

```text
direction_parallelism.x
direction_parallelism.y
direction_parallelism.z
axis_offset_mm.x / length_residual_scale_mm
axis_offset_mm.y / length_residual_scale_mm
axis_offset_mm.z / length_residual_scale_mm
signed_seating_separation_mm / length_residual_scale_mm
```

Insert reuses the same graph order, free-component variable order, finite differences, damped Gauss-Newton path, solve states, snapshots, stale-result checks, and explicit application boundary as planar and Concentric constraints. Lateral offset, axis tilt, and axial seating are solved; equal and opposed aligned axis directions stay accepted; rotation about the common axis stays free.

Proven regular one-free-body result:

```text
variable_count           = 6
residual_component_count = 7
jacobian_rank            = 5
constrained_dof          = 5
remaining_dof            = 1
```

Mixed Concentric/Insert on one axis is also covered: rank `5`, thirteen residual components, redundancy `8`, insertion-order independent.

### 13. Planar Angle constraint family

Canonical document: `docs/assembly-angle-constraint-mvp5.md`.

Implemented in one block: `QuantityKind::AngleDeg` (`unit "deg"`), `AssemblyConstraintType::Angle` with a required angle value (JSON spelling `angle`), generated-face planar target resolution, and the dedicated dimensionless residual:

```text
angle_alignment = dot(nA, nB) - cos(target_angle_deg)
```

Angle flattens as one unscaled component in the shared lexicographic order and reuses the whole numeric/solver/application/diagnostics path unchanged.

Proven regular one-free-body result:

```text
variable_count           = 6
residual_component_count = 1
jacobian_rank            = 1
constrained_dof          = 1
remaining_dof            = 5
residual_row_redundancy  = 0
```

Known seed semantics: the cosine form is direction-symmetric, and extremal targets (`0°`, `180°`) contribute no rank at the solved state (reported as redundancy). Oriented signed angles are a later extension.

## Next MVP: Suppressed components in solved groups

Goal: replace the current hard rejection of suppressed components inside a connected group with the documented suppression policy, so a partially suppressed assembly still solves deterministically.

Required implementation sequence:

1. Exclude suppressed components from the variable and fixed sets instead of erroring on them.
2. Exclude every constraint that touches a suppressed component from the collected constraint set.
3. Keep suppressed components in the solve snapshots so stale-result detection still covers them.
4. Reject application when a component's suppression state changed after solving (already snapshotted).
5. Require at least one grounded non-suppressed component in the remaining subgroup.
6. Classify a group whose constraints all vanish through suppression as converged with zero residual components.
7. Keep diagnostics honest: rank/DOF are computed only over non-suppressed free components.
8. Cover: suppressed leaf component, suppressed grounded component, fully suppressed group, unchanged determinism, stale-result rejection after unsuppressing.

## Proposed assembly implementation sequence

1. Component instances and grounding. Implemented.
2. Explicit placement/state updates. Implemented.
3. Mate/Concentric/Distance relationship intent. Implemented.
4. Persistence and inspection without transform solving. Implemented.
5. Read-only active-constraint graph. Implemented.
6. Generated-face planar target resolution. Implemented.
7. Explicit rigid-transform evaluation. Implemented.
8. Planar Mate/Distance residual construction. Implemented.
9. First rigid-body solver and explicit successful result application. Implemented.
10. Jacobian-rank and remaining-DOF diagnostics. Implemented.
11. Semantic generated-axis references and read-only Concentric residual construction. Implemented.
12. Concentric numeric-system, solver, and DOF integration. Implemented.
13. Stable Insert intent, semantic seating targets, and read-only composite residual semantics. Implemented.
14. Insert numeric-system, solver, and DOF integration. Implemented.
15. Add solved-state or DOF cache records only if a later consumer requires non-regenerable data. No current requirement.
16. Add richer constraint families. First family (planar Angle) implemented; further families follow as needed.
17. Suppressed components in solved groups. Next.
18. Add joints and limits, then motion through the solver.
19. Add rigid subassemblies first, flexible subassemblies later, collision/interference checks, component geometry instancing, and assembly-level STEP export.

## Future roadmaps

- Multi-body and path features: `docs/multi-body-transform-and-path-features-roadmap.md`
- Inventor-like sketch/feature parity: `docs/inventor-like-sketcher-and-feature-roadmap.md`
- Advanced surfacing and 3D sketches: `docs/advanced-surfacing-and-3d-sketch-mvp.md`

Later assembly work includes richer constraint families, per-component/null-space DOF presentation, joints and limits, motion, rigid then flexible subassemblies, collision/interference checks, component geometry instancing, and assembly-level STEP export.

## Persistence rule

Persist model intent. Regenerate graph connectivity, resolved plane/axis/seat targets, assembly-space geometry, residual descriptors, numeric Jacobians, solve results, rank summaries, and remaining-DOF diagnostics.

Only explicitly applied successful transform proposals change the existing persisted component `RigidTransform` model intent.

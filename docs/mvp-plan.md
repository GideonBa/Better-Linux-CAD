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

Assembly-level STEP product structure and manifest/external-file project containers remain deferred.

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
- at least one grounded component while constraints survive
- every grounded active component fixed
- suppressed selected components excluded from the numeric subgroup; visibility ignored
- six direct variables per free active component: `tx,ty,tz,rx_deg,ry_deg,rz_deg`
- lexicographic component and constraint ordering
- shared Mate/Distance/Concentric/Insert/Angle residual evaluation
- central finite-difference Jacobian
- damped Gauss-Newton normal equations
- partial-pivot Gaussian elimination
- deterministic backtracking line search and damping escalation
- explicit solve states
- source-project immutability during solve
- complete solve-input snapshots and transform proposals
- stale-result detection including moved grounded anchors and suppression changes
- explicit atomic successful application through `AssemblySolveResultApplier`

### 8. Local Jacobian-rank and remaining-DOF diagnostics

Canonical document: `docs/assembly-solve-diagnostics-mvp5.md`.

Solver and diagnostics share the same private residual/variable/finite-difference Jacobian path for every currently integrated numeric constraint family.

```text
variable_count = 6 * free_active_component_count
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

### 14. Suppressed components in solved groups

Canonical document: `docs/assembly-suppressed-component-solving-mvp5.md`.

Implemented: suppressed components contribute no solve variables, every constraint touching them vanishes from the collected constraint set, snapshots still cover them for stale-result detection, proposals must match free **active** snapshots, the remaining subgroup requires a grounded non-suppressed component only while constraints survive, fully vanished constraint sets are trivially converged with zero residual components, and diagnostics compute rank/DOF over the active subgroup only.

### 15. Posed assembly STEP export seed

Canonical document: `docs/assembly-posed-step-export-mvp5.md`.

Implemented the first end-to-end assembly geometry deliverable:

1. Validate project/member/component structure before geometry export.
2. Collect referenced part documents deterministically and recompute each one exactly once into one per-export `ShapeCache`, reused across repeated component occurrences.
3. Apply each visible, non-suppressed component's persisted `RigidTransform` to a part-shape copy using explicit X, then Y, then Z degree rotations followed by millimeter translation, matching `AssemblyTransformEvaluator` semantics exactly.
4. Compose posed occurrence shapes into one OCCT compound and export through the existing `StepExporter`.
5. Fail closed on unresolved project members, recompute failures, missing final part shapes, transform failures, empty visible-active assemblies, or STEP writer failures.
6. Keep recompute caches, transformed shapes, the compound, and export summaries derived and unpersisted.
7. Provide `blcad_export_posed_assembly`, which loads project JSON, builds the active graph, solves one connected multi-component group, explicitly applies the converged result, and exports the posed assembly.
8. Cover repeated instances of one part, bounding-box and volume transform checks, hidden/suppressed exclusion, geometrically equivalent repeated STEP exports after re-import, unresolved members, and missing recompute end shapes.

## Next MVP: Joint/limit model intent and first motion seed

Goal: add the first persistent solver-independent motion relationship layer without turning solved numeric state into model intent.

Required implementation sequence:

1. Define stable joint identity, active/inactive state, semantic target endpoints, and explicit limit ranges as persistent records separate from geometric assembly constraints.
2. Start with one minimal motion-capable joint family whose unconstrained motion is already expressible by the current rigid-body coordinate system; a revolute joint is the preferred seed.
3. Validate joint targets against existing component instances and reuse semantic face/axis/seat resolution instead of storing OCCT topology ids.
4. Derive active joint graph participation without persisting connectivity or numeric state.
5. Map the seed joint and limits into the shared residual/Jacobian path while preserving deterministic component/relationship ordering, finite differences, damping, solve states, snapshots, stale-result checks, and atomic application.
6. Expose an explicit motion input/query boundary that changes only a requested joint coordinate and returns transform proposals; no continuous simulation cache yet.
7. Cover free in-range motion, lower/upper limit clamping or rejection semantics, grounded participation, suppression filtering, stale application, and deterministic repeated solves.
8. Keep joint coordinates, limit intent, and relationship identity persistent only when they are true user model intent; regenerate Jacobians, null spaces, and solve state.

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
17. Suppressed components in solved groups. Implemented.
18. Posed assembly STEP export seed. Implemented.
19. Add joints and limits, then motion through the solver. Next.
20. Add rigid subassemblies first, flexible subassemblies later, collision/interference checks, and component geometry instancing.

## Future roadmaps

- Multi-body and path features: `docs/multi-body-transform-and-path-features-roadmap.md`
- Inventor-like sketch/feature parity: `docs/inventor-like-sketcher-and-feature-roadmap.md`
- Advanced surfacing and 3D sketches: `docs/advanced-surfacing-and-3d-sketch-mvp.md`

Later assembly work includes richer constraint families, per-component/null-space DOF presentation, joints and limits, motion, rigid then flexible subassemblies, collision/interference checks, component geometry instancing, and structured STEP assembly product export beyond the current geometric compound seed.

## Persistence rule

Persist model intent. Regenerate graph connectivity, resolved plane/axis/seat targets, assembly-space geometry, residual descriptors, numeric Jacobians, solve results, rank summaries, remaining-DOF diagnostics, per-export shape caches, posed component shapes, and assembly STEP compounds.

Only explicitly applied successful transform proposals change the existing persisted component `RigidTransform` model intent. Future joint coordinates and limits may be persistent only when introduced as explicit user-authored model intent.

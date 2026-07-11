# MVP Plan

This document is the implementation-sequence source of truth. Feature-specific documents are canonical for exact contracts.

## MVP 1: Single-part modeling

Canonical document: `docs/mvp-1-specification.md`.

Implemented:

- part documents and typed quantities/parameters;
- datum planes and sketches;
- rectangle/circle profile seeds;
- additive extrude and subtractive through-all cut intent;
- dependency graph, invalidation, recompute planning, and parameter updates;
- optional OCCT execution through `ShapeCache`;
- STEP export and headless JSON-to-STEP examples;
- JSON serialization of current model-intent records.

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

- count quantities and count parameters;
- `CircularHolePattern` radius/count/hole-diameter/center/angle-offset intent;
- parameter-to-sketch dependency edges;
- subtractive recompute expansion into per-hole through-all cuts;
- incremental count/radius recompute;
- JSON roundtrip and checked-in headless example.

Still deferred: hole-wizard semantics, partial/skipped patterns, and arbitrary seed-feature patterns.

## MVP 4: Assembly parameters and project container

Canonical documents:

- `docs/assembly-parameters-mvp4.md`
- `docs/project-container-mvp4.md`

Implemented:

- assembly-scoped parameters;
- member-part registration and `ParameterBinding`;
- binding propagation through normal part invalidation;
- one `Project` owning one root assembly and embedded part documents;
- project-level assembly-parameter updates with per-part recompute plans;
- embedded assembly/project JSON;
- headless project parameter update, recompute, and per-part STEP export.

Structured multi-assembly ownership and manifest/external-file project containers remain deferred.

## MVP 5: Assembly relationship and rigid-body pipeline

### 1. Component instances and explicit placement/state

Canonical document: `docs/component-instance-mvp5.md`.

Implemented component occurrence identity, repeated part reuse, visibility, suppression, grounding, finite `RigidTransform`, explicit placement/state updates, JSON roundtrip, and headless inspection.

### 2. Solver-independent geometric constraint intent

Canonical document: `docs/assembly-constraint-model-intent-mvp5.md`.

Implemented persistent relationship families:

```text
Mate
Concentric
Distance
Insert
Angle
```

All preserve persistent target A/B semantic strings and active/inactive state. Distance stores a positive length quantity. Angle stores a finite degree quantity. Constraint creation/loading never moves components.

### 3. Deterministic active-constraint graph

Canonical document: `docs/assembly-constraint-graph-mvp5.md`.

Implemented every component as a node, active constraints as distinct multi-edges, inactive filtering, lexicographic ordering, deterministic connected groups, exact ordinary-solver partition boundaries, and no graph cache persistence.

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

Implemented primary circular-feature axis and seating families:

```text
feature.<feature-id>.axis
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

- exactly one deterministic connected constraint group per ordinary solve;
- at least one grounded component while numeric relationships survive;
- every grounded active component fixed;
- suppressed selected components excluded from the numeric subgroup;
- six direct variables per free active component: `tx,ty,tz,rx_deg,ry_deg,rz_deg`;
- lexicographic component and relationship ordering;
- shared Mate/Distance/Concentric/Insert/Angle residual evaluation;
- central finite-difference Jacobian;
- damped Gauss-Newton normal equations;
- partial-pivot Gaussian elimination;
- deterministic backtracking line search and damping escalation;
- explicit solve states;
- source-project immutability during solve;
- complete component solve-input snapshots and transform proposals;
- stale-result detection including moved grounded anchors and suppression changes;
- explicit atomic successful application through `AssemblySolveResultApplier`.

### 8. Local Jacobian-rank and remaining-DOF diagnostics

Canonical document: `docs/assembly-solve-diagnostics-mvp5.md`.

Solver and diagnostics share the same private residual/variable/finite-difference Jacobian path for every currently integrated geometric constraint family.

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

Proven regular one-free-body result:

```text
variable_count           = 6
residual_component_count = 6
jacobian_rank            = 4
constrained_dof          = 4
remaining_dof            = 2
```

Mixed Distance/Concentric is also covered as rank `5/6` with one remaining rotation-about-axis DOF.

### 11. Stable Insert intent and composite residual semantics

Canonical document: `docs/assembly-insert-intent-composite-residuals-mvp5.md`.

Implemented `AssemblyConstraintType::Insert`, JSON spelling `insert`, stable `feature.<feature-id>.seat` identity, exact source feature/profile preservation, one local primary axis plus one oriented seating plane, deterministic target-A-then-target-B resolution, and the composite residual:

```text
direction_parallelism       = cross(dA, dB)
axis_offset_mm               = cross(oB - oA, dA)
signed_seating_separation_mm = dot(sB - sA, nA)
```

Equal and opposed axis directions are accepted. Target A defines signed seating direction. No duplicate seating-normal residual is added.

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

Insert reuses the same graph order, free-component variable order, finite differences, damped Gauss-Newton path, solve states, snapshots, stale-result checks, and explicit application boundary as the other geometric constraint families.

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

Implemented `QuantityKind::AngleDeg`, `AssemblyConstraintType::Angle`, JSON spelling `angle`, generated-face planar target resolution, and:

```text
angle_alignment = dot(nA, nB) - cos(target_angle_deg)
```

Angle flattens as one unscaled component and reuses the whole shared numeric/solver/application/diagnostics path.

Proven regular non-extremal one-free-body result:

```text
variable_count           = 6
residual_component_count = 1
jacobian_rank            = 1
constrained_dof          = 1
remaining_dof            = 5
residual_row_redundancy  = 0
```

Known seed semantics: the cosine form is direction-symmetric and extremal targets (`0°`, `180°`) contribute no local rank at the solved state. Oriented signed angles are deferred.

### 14. Suppressed components in solved groups

Canonical document: `docs/assembly-suppressed-component-solving-mvp5.md`.

Implemented: suppressed components contribute no solve variables, every geometric constraint touching them vanishes from the collected constraint set, snapshots still cover them for stale-result detection, proposals must match free active snapshots, the remaining subgroup requires a grounded non-suppressed component only while numeric relationships survive, fully vanished relationship sets are trivially converged with zero residual components, and diagnostics compute rank/DOF over the active subgroup only.

### 15. Posed assembly STEP export seed

Canonical document: `docs/assembly-posed-step-export-mvp5.md`.

Implemented the first end-to-end assembly geometry deliverable:

1. Validate project/member/component structure before geometry export.
2. Collect referenced part documents deterministically and recompute each one exactly once into one per-export `ShapeCache`, reused across repeated component occurrences.
3. Apply each visible, non-suppressed component's persisted `RigidTransform` to a part-shape copy using explicit X, then Y, then Z degree rotations followed by millimeter translation, matching `AssemblyTransformEvaluator` semantics exactly.
4. Compose posed occurrence shapes into one OCCT compound and export through the existing `StepExporter`.
5. Fail closed on unresolved project members, recompute failures, missing final part shapes, transform failures, empty visible-active assemblies, or STEP writer failures.
6. Keep recompute caches, transformed shapes, the compound, and export summaries derived and unpersisted.
7. Provide `blcad_export_posed_assembly`, which loads project JSON, builds the active graph, solves one connected multi-component group when present, explicitly applies the converged result, and exports the posed assembly.
8. Cover repeated instances of one part, bounding-box and volume transform checks, hidden/suppressed exclusion, geometrically equivalent repeated STEP exports after re-import, unresolved members, and missing recompute end shapes.

### 16. Revolute joint/limit intent and first motion seed

Canonical document: `docs/assembly-revolute-joint-motion-mvp5.md`.

Implemented the first persistent solver-independent motion relationship layer:

1. Add stable `AssemblyJointId`, separate `AssemblyJoint` records, active/inactive state, semantic target A/B endpoints, explicit degree limits, and a persistent authored coordinate.
2. Implement `AssemblyJointType::Revolute` as the first motion-capable family.
3. Constrain the seed to an explicit principal range `[-180°, 180°]`, require ordered lower/upper limits, and reject coordinates outside the authored range.
4. Persist joints additively as optional `assembly_joints[]` records while keeping the historical assembly schema marker and loading older files as empty-joint assemblies.
5. Reuse `feature.<feature-id>.seat` resolution so one endpoint derives the exact existing semantic primary axis and oriented seating frame without OCCT topology ids.
6. Add `AssemblyJointGraph`: every component is a node, every active joint is a lexicographically sorted distinct edge, inactive joints are excluded, and connectivity is regenerated.
7. Define directed Revolute axis alignment `dA - dB` because target A owns signed positive rotation; unlike Concentric/Insert, opposed directions are not equivalent.
8. Define a periodic signed twist target through sine/cosine residuals without an `atan2` branch cut.
9. Flatten one transient Revolute drive as nine scalar residual components and prove the regular one-free-body shared finite-difference Jacobian as rank `6/6`.
10. Extract the old rigid-body Gauss-Newton loop into one private `solve_numeric_relationships` engine used unchanged by ordinary geometric solving and motion drives.
11. Derive one transitive combined constraint/joint relationship group for a motion query, remove suppressed components from the numeric subgroup, and keep complete component snapshots for stale suppression checks.
12. Drive the selected Revolute joint to the requested coordinate while holding every other active Revolute joint in the same active combined group at its persisted coordinate, in lexicographic joint order.
13. Reject out-of-range motion requests rather than silently clamping them; reject driving the selected joint through a suppressed endpoint.
14. Return ordinary transform proposals plus complete input snapshots for every numerically driven joint.
15. Apply a converged motion result atomically on a project copy: validate all joint snapshots, apply component proposals through `AssemblySolveResultApplier`, update the selected persistent joint coordinate, then commit the copy.
16. Provide `blcad_move_joint` and a checked-in `examples/revolute_joint.blcad.project.json` flow that writes an updated project directly consumable by posed assembly STEP export.

Proven focused behavior includes signed twist orientation, exact nine-component flattening, local rank `6/6`, deterministic repeated motion solves, source immutability, atomic application, hard limit rejection, grounding, inactive-joint rejection, selected-endpoint suppression rejection, filtering of related suppressed joints, component stale rejection, joint stale rejection, and holding other active joint coordinates in a chain.

## Next MVP: Rigid subassembly instance and nested posed-export seed

Goal: add the first hierarchical assembly structure without introducing flexible-subassembly solver variables. A child assembly occurrence is rigid at the parent boundary; its already-authored internal component placements are flattened recursively for geometry export.

Required implementation sequence:

1. Extend `Project` from one root assembly plus parts to one root assembly plus project-owned child `AssemblyDocument` records, preserving a single explicit root assembly identity.
2. Define stable `SubassemblyInstanceId` and persistent rigid subassembly occurrence records with referenced child assembly id, visibility, suppression, and finite `RigidTransform` placement intent.
3. Allow `AssemblyDocument` to store subassembly occurrences separately from part component occurrences; adding/loading an occurrence must never move child components.
4. Validate child assembly references against project-owned assembly documents and reject self-reference, duplicate occurrence ids, missing child assemblies, and every direct or indirect assembly-reference cycle.
5. Derive a deterministic read-only assembly hierarchy traversal ordered by occurrence id. Do not persist parent links, traversal order, composed transforms, or flattened leaves.
6. Add hierarchical rigid-transform evaluation for leaf points/vectors/shapes. A leaf component is evaluated in its child assembly, then through each parent subassembly occurrence from inner to outer, preserving the existing active X-then-Y-then-Z component transform convention at every authored level.
7. Extend posed assembly export to recursively flatten visible, non-suppressed leaf component occurrences from the root assembly, recompute each referenced part document once per export, pose every leaf through the complete rigid hierarchy, compose one OCCT compound, and delegate to the existing STEP writer.
8. Hidden or suppressed subassembly occurrences exclude their complete subtree. Child component visibility/suppression remains effective inside an otherwise active subassembly occurrence.
9. Fail closed on missing child assemblies, hierarchy cycles, unresolved leaf part documents, failed leaf part recomputes, or an empty flattened visible-active assembly.
10. Cover repeated rigid occurrences of one child assembly, two-level nested transforms, transform-order correctness through bounding boxes, hidden/suppressed subtree exclusion, deterministic traversal independent of insertion order, cycle rejection, missing-child rejection, and repeated STEP geometric equivalence after re-import.
11. Keep rigid subassembly hierarchy, occurrence transforms, and states as model intent. Regenerate traversal, composed transforms, flattened leaf occurrence descriptors, and export geometry.
12. Keep geometric constraints/joints and flexible motion across a subassembly boundary deferred for this seed. Child internal placements are treated as rigid authored pose at the parent boundary.

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
16. Add richer geometric constraint families. First family (planar Angle) implemented; further families follow as needed.
17. Suppressed components in solved groups. Implemented.
18. Posed assembly STEP export seed. Implemented.
19. Add joints and limits, then motion through the solver. First Revolute family implemented.
20. Rigid subassembly instance and nested posed-export seed. Next.
21. Add flexible subassemblies, collision/interference checks, and component geometry instancing in later dedicated blocks.

## Future roadmaps

- Multi-body and path features: `docs/multi-body-transform-and-path-features-roadmap.md`
- Inventor-like sketch/feature parity: `docs/inventor-like-sketcher-and-feature-roadmap.md`
- Advanced surfacing and 3D sketches: `docs/advanced-surfacing-and-3d-sketch-mvp.md`

Later assembly work includes richer constraint and joint families, per-component/null-space DOF presentation, multi-turn and time-based motion, rigid then flexible subassemblies, cross-hierarchy relationship semantics, collision/interference checks, component geometry instancing, and structured STEP assembly product export beyond the current geometric compound seed.

## Persistence rule

Persist model intent. Regenerate constraint/joint graph connectivity, combined motion groups, resolved plane/axis/seat targets, assembly-space geometry, residual descriptors, transient drive sets, numeric Jacobians, solve/motion results, input snapshots, rank summaries, remaining-DOF diagnostics, per-export shape caches, posed component shapes, and assembly STEP compounds.

Only explicit application of a fresh converged solve or motion result changes persisted component `RigidTransform` model intent. A successful motion application additionally changes the selected persistent joint coordinate because that coordinate is explicit user-authored motion state.

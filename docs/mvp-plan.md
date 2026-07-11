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

Implemented count quantities/parameters, `CircularHolePattern` intent, dependency edges, per-hole through-all recompute expansion, incremental count/radius recompute, JSON roundtrip, and a checked-in headless example.

Still deferred: hole-wizard semantics, partial/skipped patterns, and arbitrary seed-feature patterns.

## MVP 4: Assembly parameters and project container

Canonical documents:

- `docs/assembly-parameters-mvp4.md`
- `docs/project-container-mvp4.md`

Implemented assembly-scoped parameters, member-part registration, `ParameterBinding`, binding propagation through normal part invalidation, the initial explicit root assembly/project container, project-level root assembly parameter updates with per-part recompute plans, embedded assembly/project JSON, and headless parameter-update/recompute/STEP flows.

The original one-root-plus-parts project seed has since been extended additively by MVP-5 rigid child assembly ownership. Manifest/external-file containers remain deferred.

## MVP 5: Assembly relationship, motion, hierarchy, and posed-geometry pipeline

### 1. Component instances and explicit placement/state

Canonical document: `docs/component-instance-mvp5.md`.

Implemented component occurrence identity, repeated part reuse, visibility, suppression, grounding, finite `RigidTransform`, explicit placement/state updates, JSON roundtrip, and headless inspection.

### 2. Solver-independent geometric constraint intent

Canonical document: `docs/assembly-constraint-model-intent-mvp5.md`.

Persistent relationship families are:

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

Implemented planar generated-face targets:

```text
feature.<feature-id>.top|bottom|right|left|front|back
```

Implemented circular feature targets:

```text
feature.<feature-id>.axis
feature.<feature-id>.seat
```

The first axis/seat producer is one `SubtractiveExtrude` whose source sketch contains exactly one `CircleProfile` and one total profile. Resolved targets preserve source feature/profile identity and remain derived.

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

Points rotate then translate. Vectors rotate only.

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

Implemented one deterministic connected group per ordinary solve, grounded participation, six direct persisted-transform variables per free active component, lexicographic component/relationship ordering, shared residual evaluation, central finite differences, damped Gauss-Newton normal equations, partial-pivot Gaussian elimination, deterministic damping escalation/backtracking, explicit solve states, source immutability, complete component snapshots, stale-result detection, proposals, and atomic converged-result application.

### 8. Local Jacobian-rank and remaining-DOF diagnostics

Canonical document: `docs/assembly-solve-diagnostics-mvp5.md`.

```text
variable_count = 6 * free_active_component_count
constrained_dof = rank(J)
remaining_dof = variable_count - rank(J)
```

Rank uses the documented pivot threshold. DOF, consistency, and residual-row rank structure remain separate classifications. Diagnostics are local and unpersisted.

### 9. Semantic axes and Concentric residuals

Canonical document: `docs/assembly-semantic-axis-concentric-residuals-mvp5.md`.

```text
direction_parallelism = cross(dA, dB)
axis_offset_mm         = cross(oB - oA, dA)
```

Equal and opposed directions are accepted. Axial translation and rotation around the common axis remain free.

### 10. Concentric numeric-system, solver, and DOF integration

Canonical document: `docs/assembly-concentric-numeric-solver-dof-mvp5.md`.

Concentric flattens to six scalar residual components. The proven regular one-free-body result is rank `4/6` with two remaining DOF. Mixed Distance/Concentric is covered as rank `5/6`.

### 11. Stable Insert intent and composite residual semantics

Canonical document: `docs/assembly-insert-intent-composite-residuals-mvp5.md`.

```text
direction_parallelism       = cross(dA, dB)
axis_offset_mm               = cross(oB - oA, dA)
signed_seating_separation_mm = dot(sB - sA, nA)
```

Insert uses stable `.seat` identity and derives one axis plus one oriented seating plane from the same exact circular feature/profile intent. Target A defines signed seating direction.

### 12. Insert numeric-system, solver, and DOF integration

Canonical document: `docs/assembly-insert-numeric-solver-dof-mvp5.md`.

Insert flattens to seven scalar residual components and reuses the complete shared numeric/solver/application/diagnostics path. The proven regular one-free-body result is rank `5/6` with one remaining rotation-about-axis DOF.

### 13. Planar Angle constraint family

Canonical document: `docs/assembly-angle-constraint-mvp5.md`.

```text
angle_alignment = dot(nA, nB) - cos(target_angle_deg)
```

Angle flattens as one unscaled component and reuses the shared numeric path. Away from `0°` and `180°`, the regular one-free-body local rank is `1/6`. The current cosine seed is direction-symmetric; oriented signed planar angles remain deferred.

### 14. Suppressed components in solved groups

Canonical document: `docs/assembly-suppressed-component-solving-mvp5.md`.

Suppressed components contribute no solve variables, geometric constraints touching them vanish from the active numeric subgroup, complete snapshots still protect stale application, fully vanished relationship sets are trivially converged, and diagnostics use the active subgroup only.

### 15. Posed assembly STEP export seed

Canonical document: `docs/assembly-posed-step-export-mvp5.md`.

Implemented the initial end-to-end derived geometry consumer: validate project structure, deterministically collect referenced parts, recompute one `ShapeCache` per referenced part, pose visible active occurrences with exact persisted transform semantics, compose one OCCT compound, and delegate to the existing `StepExporter`.

`blcad_export_posed_assembly` loads project JSON, optionally solves/applies one root geometric constraint group when present, and exports the current posed assembly.

### 16. Revolute joint/limit intent and first motion seed

Canonical document: `docs/assembly-revolute-joint-motion-mvp5.md`.

Implemented the first persistent solver-independent motion relationship layer:

1. stable `AssemblyJointId` and separate persistent `AssemblyJoint` records;
2. `AssemblyJointType::Revolute`, active/inactive state, semantic endpoints, principal-angle limits, and authored coordinate;
3. additive optional `assembly_joints[]` JSON;
4. deterministic active `AssemblyJointGraph`;
5. reuse of `.seat` axis/oriented-frame target semantics;
6. directed axis alignment `dA - dB` for signed positive rotation owned by target A;
7. periodic signed sine/cosine twist residuals without an `atan2` branch cut;
8. nine scalar drive residual components and a proven regular `9 x 6` finite-difference Jacobian of rank `6`;
9. one private `solve_numeric_relationships` engine shared with ordinary geometric solving;
10. transitive combined constraint/joint motion groups;
11. selected-joint drive plus persisted-coordinate holding drives for all other active joints in the active group;
12. hard out-of-range rejection and selected-endpoint suppression rejection;
13. complete driven-joint input snapshots;
14. atomic transform plus selected persistent coordinate application;
15. `blcad_move_joint` and a checked-in project flow directly consumable by posed export.

### 17. Rigid subassembly hierarchy and nested posed export

Canonical document: `docs/assembly-rigid-subassembly-nested-export-mvp5.md`.

Implemented the complete rigid hierarchy seed:

1. Extend `Project` to one explicit root assembly, project-owned child `AssemblyDocument` records, and owned parts while preserving `Project::assembly()` as the root API.
2. Add stable `SubassemblyInstanceId` and persistent rigid child occurrences containing referenced child assembly id, visibility, suppression, and finite `RigidTransform` intent.
3. Store `subassembly_instances[]` separately from `component_instances[]`; adding/loading an occurrence never mutates child component transforms.
4. Persist child assemblies additively in optional project `assemblies[]` and rigid occurrences additively in optional assembly `subassembly_instances[]` while retaining historical schema markers and backward compatibility.
5. Validate project-owned child references, unique assembly ids, duplicate occurrence ids, direct self-reference, child-to-root references, missing children, and direct/indirect cycles across all owned assembly documents.
6. Add deterministic `AssemblyHierarchyTraversal`: root-first pre-order DFS, child occurrences ordered lexicographically by `SubassemblyInstanceId`, repeated child document occurrences preserved as separate descriptors.
7. Keep parent links, occurrence paths, accumulated visibility/suppression path state, and parent transform chains derived and unpersisted.
8. Add `AssemblyHierarchyTransformEvaluator` for exact authored transform-chain evaluation in inner-to-outer order. Every level retains the existing active X-then-Y-then-Z then translation convention; no composed rotation is converted back to Euler angles.
9. Add `AssemblyLeafOccurrenceResolver` as the canonical visible-active flattening boundary. It returns assembly identity, subassembly path, component identity, referenced part identity, and exact `[component, inner parent, ..., outer parent]` transform chains.
10. Hidden or suppressed subassembly occurrences exclude their complete subtree; local child component visibility/suppression remains effective inside otherwise active paths.
11. Extend `AssemblyStepExporter` to consume flattened leaves, collect referenced leaf part ids deterministically, recompute each part exactly once per export, reuse caches across repeated subassembly occurrences, apply complete transform chains to independent shape copies, and compose the same single OCCT compound/STEP output.
12. Fail closed on invalid hierarchy, missing children, cycles, unresolved leaf parts, failed recomputes, missing final shapes, transform failures, and empty visible-active flattened output.
13. Cover insertion-order-independent traversal and leaf order, repeated child occurrences, two hierarchy levels, exact path/transform-chain semantics, transform non-commutativity, hidden/suppressed subtree filtering, local component filtering, exact STEP solid/volume/bounding-box behavior, and repeated STEP geometric equivalence after re-import.
14. Provide `examples/nested_subassembly.blcad.project.json` as a direct `blcad_export_posed_assembly` consumer.
15. Keep flexible subassembly solve variables, grounding of whole subassembly occurrences, and geometric constraints/joints across assembly-document boundaries deferred.

### 18. Posed assembly interference-analysis seed

Canonical document: `docs/assembly-interference-analysis-mvp5.md`.

Implemented: a read-only `AssemblyInterferenceAnalyzer` over the shared posed-leaf boundary (`AssemblyPosedLeafShapeBuilder`, now also backing posed STEP export), deterministic lexicographic pair evaluation with stable non-topological leaf identities, OCCT boolean-common positive-volume overlap above an explicit finite tolerance, fail-closed error propagation, and fully derived/unpersisted analysis products. Contact without positive volume is not interference. Posing composes each authored transform chain into one rigid OCCT transform per leaf occurrence.

## Next MVP: Posed assembly clearance-analysis seed

Goal: extend the same posed-leaf boundary with deterministic minimum-distance records between non-interfering leaves, so clearance requirements become checkable before joints and motion sweeps.

Required implementation sequence:

1. Add a read-only `AssemblyClearanceAnalyzer` (or extend the interference analyzer with a combined analysis) consuming `const Project&` through the shared `AssemblyPosedLeafShapeBuilder`.
2. Reuse the deterministic lexicographic pair order and the stable leaf identity contract unchanged.
3. Use OCCT extrema (`BRepExtrema_DistShapeShape`) to derive the minimum distance in `mm` for each pair without positive-volume interference.
4. Report pairs below an explicit finite positive clearance threshold; report interfering pairs separately (distance zero with overlap volume), never as clearance records.
5. Fail closed on extrema failures and non-finite distances.
6. Keep witness points optional and derived; keep everything unpersisted.
7. Cover: pair above threshold (no record), pair below threshold with known exact distance, touching pair (distance zero, no volume), interfering pair excluded from clearance records, nested occurrences, determinism, and threshold validation.

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
20. Rigid subassembly instance and nested posed-export seed. Implemented.
21. First posed assembly interference-analysis seed. Implemented.
22. Posed assembly clearance-analysis seed. Next.
23. Add flexible subassemblies, cross-hierarchy relationship semantics, component geometry instancing, and richer collision/contact analysis in later dedicated blocks.

## Future roadmaps

- Multi-body and path features: `docs/multi-body-transform-and-path-features-roadmap.md`
- Inventor-like sketch/feature parity: `docs/inventor-like-sketcher-and-feature-roadmap.md`
- Advanced surfacing and 3D sketches: `docs/advanced-surfacing-and-3d-sketch-mvp.md`

Later assembly work includes richer constraint and joint families, per-component/null-space DOF presentation, multi-turn and time-based motion, flexible subassemblies, cross-hierarchy relationship semantics, richer collision/contact and swept-motion analysis, component geometry instancing, and structured STEP assembly product export beyond the current geometric compound seed.

## Persistence rule

Persist model intent. Current persistent assembly intent includes component placement/state, geometric constraints, joint/limit/coordinate records, project-owned child assembly documents, and rigid subassembly occurrence placement/state.

Regenerate constraint/joint graph connectivity, combined motion groups, assembly hierarchy traversal, occurrence paths, parent transform chains, flattened leaf occurrence descriptors, resolved semantic geometry, residual descriptors, transient drive sets, numeric Jacobians, solve/motion results, input snapshots, rank summaries, remaining-DOF diagnostics, per-consumer shape caches, posed leaf shapes, pair candidates, future interference records, and assembly STEP compounds.

Only explicit application of a fresh converged solve or motion result changes persisted component `RigidTransform` intent. A successful motion application additionally changes the selected persistent joint coordinate because that coordinate is explicit user-authored motion state. Rigid subassembly placement/state changes only through explicit occurrence edits.

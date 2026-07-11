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

The original one-root-plus-parts project seed has since been extended additively by MVP-5 child assembly ownership. Manifest/external-file containers remain deferred.

## MVP 5: Assembly relationship, motion, hierarchy, and posed-geometry pipeline

### 1. Component instances and explicit placement/state

Canonical document: `docs/component-instance-mvp5.md`.

Implemented occurrence identity, repeated part reuse, visibility, suppression, grounding, finite `RigidTransform`, explicit placement/state edits, JSON roundtrip, and headless inspection.

### 2. Solver-independent geometric constraint intent

Canonical document: `docs/assembly-constraint-model-intent-mvp5.md`.

Persistent local relationship families are:

```text
Mate
Concentric
Distance
Insert
Angle
```

All preserve target A/B semantic strings and active/inactive state. Distance stores a length quantity. Angle stores a degree quantity. Constraint creation/loading never moves components.

### 3. Deterministic active-constraint graph

Canonical document: `docs/assembly-constraint-graph-mvp5.md`.

Implemented every local component as a node, active constraints as distinct multi-edges, inactive filtering, lexicographic ordering, deterministic connected groups, and no graph-cache persistence.

### 4. Semantic target resolution

Canonical document: `docs/assembly-constraint-target-resolution-mvp5.md`.

Implemented generated planar targets:

```text
feature.<feature-id>.top|bottom|right|left|front|back
```

Implemented circular feature targets:

```text
feature.<feature-id>.axis
feature.<feature-id>.seat
```

The first axis/seat producer is one `SubtractiveExtrude` whose source sketch contains exactly one `CircleProfile` and one total profile. Resolved geometry remains derived.

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

Implemented one deterministic connected group per local solve, grounded participation, six direct persisted-transform variables per free active component, lexicographic component/relationship ordering, central finite differences, damped Gauss-Newton normal equations, deterministic damping/backtracking, explicit solve states, source immutability, complete snapshots, stale-result detection, proposals, and atomic converged-result application.

### 8. Local Jacobian-rank and remaining-DOF diagnostics

Canonical document: `docs/assembly-solve-diagnostics-mvp5.md`.

```text
variable_count = 6 * free_active_component_count
constrained_dof = rank(J)
remaining_dof = variable_count - rank(J)
```

DOF, consistency, and residual-row rank structure remain separate classifications. Diagnostics are local and unpersisted.

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

Insert uses stable `.seat` identity and derives one axis plus one oriented seating plane from the same circular feature/profile intent.

### 12. Insert numeric-system, solver, and DOF integration

Canonical document: `docs/assembly-insert-numeric-solver-dof-mvp5.md`.

Insert flattens to seven scalar residual components and reuses the shared numeric/solver/application/diagnostics path. The proven regular one-free-body result is rank `5/6` with one remaining rotation-about-axis DOF.

### 13. Planar Angle constraint family

Canonical document: `docs/assembly-angle-constraint-mvp5.md`.

```text
angle_alignment = dot(nA, nB) - cos(target_angle_deg)
```

Angle flattens as one unscaled component and reuses the shared numeric path. Away from `0°` and `180°`, the regular one-free-body local rank is `1/6`.

### 14. Suppressed components in solved groups

Canonical document: `docs/assembly-suppressed-component-solving-mvp5.md`.

Suppressed components contribute no solve variables; geometric constraints touching them vanish from the active numeric subgroup; complete snapshots still protect stale application; fully vanished relationship sets are trivially converged.

### 15. Posed assembly STEP export seed

Canonical document: `docs/assembly-posed-step-export-mvp5.md`.

Implemented project validation, deterministic referenced-part collection, one recompute cache per referenced part, visible-active occurrence posing, one OCCT compound, and existing STEP writer delegation.

`blcad_export_posed_assembly` loads project JSON, optionally solves/applies one root local geometric constraint group, and exports the current posed assembly.

### 16. Revolute joint/limit intent and first motion seed

Canonical document: `docs/assembly-revolute-joint-motion-mvp5.md`.

Implemented stable `AssemblyJointId`, persistent Revolute state/limits/coordinate intent, active joint graph, `.seat` axis/frame reuse, directed axis alignment, periodic signed twist residuals, nine scalar drive residuals, shared numeric solve-engine reuse, combined constraint/joint motion groups, held non-selected joint coordinates, hard limit rejection, complete joint snapshots, atomic transform plus selected-coordinate application, `blcad_move_joint`, and a checked-in example.

### 17. Rigid subassembly hierarchy and nested posed export

Canonical document: `docs/assembly-rigid-subassembly-nested-export-mvp5.md`.

Implemented one explicit root assembly plus project-owned child `AssemblyDocument` records, persistent rigid `SubassemblyInstance` occurrences, project-level hierarchy/cycle validation, deterministic root-first pre-order traversal, repeated child document occurrences as distinct paths, exact inner-to-outer authored transform-chain evaluation, visible-active leaf flattening, and nested posed STEP export.

`AssemblyLeafOccurrenceResolver` exposes:

```text
assembly_document
subassembly_path
component_instance
referenced_part_document
transforms_inner_to_outer
```

Repeated child occurrences share part recompute caches while keeping distinct occurrence paths and transform chains.

### 18. Posed assembly interference-analysis seed

Canonical document: `docs/assembly-interference-analysis-mvp5.md`.

Implemented `AssemblyInterferenceAnalyzer` over the shared posed-leaf boundary with deterministic occurrence-key pair ordering, OCCT boolean common, positive finite common-volume classification above explicit tolerance, fail-closed behavior, and fully derived analysis products. Contact without positive volume is not interference.

### 19. Posed assembly clearance-analysis seed

Canonical document: `docs/assembly-interference-analysis-mvp5.md`.

Implemented `AssemblyClearanceAnalyzer` on the same leaf/pair identity contract. Interfering pairs remain interference records; remaining pairs derive OCCT minimum distance and become clearance violations below a finite positive threshold. Touching without positive common volume is a zero-distance clearance violation.

### 20. Headless assembly analysis report

Canonical document: `docs/assembly-interference-analysis-mvp5.md`.

Implemented `blcad_analyze_assembly <project.json> [clearance-threshold-mm]` with deterministic leaf/pair/part counts, record output, and non-zero fail-closed errors.

### 21. Document-scoped flexible subassembly local solving

Canonical document: `docs/assembly-flexible-subassembly-solving-mvp5.md`.

Implemented exact active non-root occurrence-path selection, the referenced child `AssemblyDocument` isolated as a temporary local solve root, unchanged reuse of the existing local constraint graph, numeric residual/Jacobian path, `AssemblyRigidBodySolver`, and `AssemblySolveResultApplier`, plus atomic application of direct component proposals back to the project-owned child document.

The `SubassemblyInstance` boundary transform remains rigid and untouched. Repeated occurrences of one child document share one internal solved component pose while preserving independent boundary transforms. No JSON schema change or second internal-pose source exists.

### 22. Cross-hierarchy relationship target and residual semantics

Canonical document: `docs/assembly-cross-hierarchy-relationship-semantics-mvp5.md`.

Implemented stable occurrence-qualified read-only endpoint identity:

```text
(occurrence_path, local ComponentInstanceId, semantic_reference)
```

`AssemblyHierarchyConstraintTargetResolver` resolves the exact rooted occurrence, reuses `AssemblyConstraintTargetResolver` for local semantic feature geometry, and evaluates the exact `[component, inner parent, ..., outer parent]` chain into root-assembly space.

`AssemblyHierarchyConstraintEquationBuilder` reuses the canonical Mate, Distance, Angle, Concentric, and Insert residual descriptor types and equations. Target A/B order remains exact. Repeated occurrences of one shared child document remain distinct because the occurrence path is part of identity.

The query, resolved targets, transform chains, and residuals are derived and unpersisted. No cross-hierarchy graph or solver participation is introduced in this block.

## Parameter expression seed

Canonical document: `docs/parameter-expression-mvp.md`.

Implemented unit-aware formula parameters on part documents (`+ - * /`, parentheses, unary minus, `mm` literals, name references), length-power unit tracking, dependency edges from every referenced parameter, topological re-evaluation after input changes, cycle rejection, direct-write rejection, JSON formula roundtrip with re-derived edges, geometry-driven incremental recompute, and transactional formula editing with stale input-edge replacement.

Still deferred: plain/expression conversion, assembly-scope/cross-part expressions, functions, and further units.

## Next MVP: Cross-hierarchy geometric constraint solving

The next numbered assembly block is persistent project-level cross-hierarchy geometric constraint intent plus occurrence-qualified graph and numeric solve/application integration.

The endpoint identity from step 22 is frozen for that block:

```text
(occurrence_path, local ComponentInstanceId, semantic_reference)
```

The next implementation must:

1. add a project-level persistent cross-hierarchy geometric constraint collection and additive JSON form;
2. keep local `AssemblyDocument::constraints()` unchanged for local relationships;
3. derive occurrence-qualified graph-node identity so repeated child occurrences cannot alias by local `ComponentInstanceId`;
4. define active/suppressed path participation before numeric flattening;
5. map each occurrence-qualified free component to six direct local transform variables without treating composed hierarchy transforms as persisted variables;
6. route the existing five residual families through the shared numeric residual/Jacobian semantics;
7. return complete occurrence-qualified snapshots and proposals;
8. atomically apply fresh converged proposals to the correct project-owned assembly/component targets;
9. preserve rigid `SubassemblyInstance` boundary transforms unless a later dedicated whole-subassembly solve-variable block explicitly changes that contract;
10. keep cross-hierarchy joints and nested motion propagation deferred.

## Broad assembly implementation sequence

This broad sequence groups the interference, clearance, and headless analysis sub-blocks that are listed separately as detailed MVP-5 sections 18-20 above. The detailed section numbers above are canonical for the split implementation blocks.

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
16. Add richer geometric constraint families. First family (planar Angle) implemented.
17. Suppressed components in solved groups. Implemented.
18. Posed assembly STEP export seed. Implemented.
19. Add joints and limits, then motion through the solver. First Revolute family implemented.
20. Rigid subassembly instance and nested posed-export seed. Implemented.
21. Posed interference, clearance, and headless analysis report. Implemented as detailed sections 18-20 above.
22. Document-scoped flexible child-assembly solving. Implemented as detailed section 21 above.
23. Stable cross-hierarchy endpoint identity and read-only target/residual semantics. Implemented as detailed section 22 above.
24. Persistent cross-hierarchy geometric constraint intent, occurrence-qualified graph, and shared numeric solve/application integration. Next.
25. Cross-hierarchy joint semantics and nested motion propagation. Deferred.
26. Component geometry instancing and structured STEP assembly products. Deferred.
27. Richer collision/contact and swept-motion analysis. Deferred.

## Future roadmaps

- Multi-body and path features: `docs/multi-body-transform-and-path-features-roadmap.md`
- Inventor-like sketch/feature parity: `docs/inventor-like-sketcher-and-feature-roadmap.md`
- Advanced surfacing and 3D sketches: `docs/advanced-surfacing-and-3d-sketch-mvp.md`

Later assembly work includes richer constraint and joint families, per-component/null-space DOF presentation, multi-turn and time-based motion, occurrence-local flexible pose overrides, whole-subassembly variables, cross-hierarchy joints, richer collision/contact and swept-motion analysis, component geometry instancing, and structured STEP assembly product export beyond the current geometric compound seed.

## Persistence rule

Persist model intent. Current persistent assembly intent includes component placement/state, local geometric constraints, joint/limit/coordinate records, project-owned child assembly documents, and rigid subassembly occurrence placement/state.

Regenerate local constraint/joint graph connectivity, combined motion groups, assembly hierarchy traversal, occurrence paths, parent transform chains, flattened leaf occurrence descriptors, occurrence-qualified cross-hierarchy query endpoints, resolved root-space hierarchy target geometry, residual descriptors, transient drive sets, numeric Jacobians, solve/motion results, snapshots, rank summaries, remaining-DOF diagnostics, per-consumer shape caches, posed leaf shapes, pair candidates, analysis records, and assembly STEP compounds.

Only explicit application of a fresh converged solve or motion result changes persisted component `RigidTransform` intent. A successful motion application additionally changes the selected persistent joint coordinate. Rigid subassembly placement/state changes only through explicit occurrence edits until a later dedicated whole-subassembly solve-variable contract is implemented.

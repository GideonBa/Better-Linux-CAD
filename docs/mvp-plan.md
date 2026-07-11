# MVP Plan

This document is the implementation-sequence source of truth. Feature-specific documents are canonical for exact contracts, formulas, persistence details, and test proofs.

## Planning discipline

BLCAD grows through narrow headless vertical slices. A numbered block should cross one primary authority boundary at a time.

Preferred order when a feature spans multiple layers:

```text
core model intent
  -> serialization / compatibility
  -> derived graph or query semantics
  -> numeric or geometry execution
  -> stale-result validation / atomic application
  -> diagnostics or presentation consumers
```

A block is ready to implement only when its canonical document states:

1. the persistent authority and what remains derived;
2. stable identity and ordering rules;
3. explicit failure behavior;
4. focused acceptance tests and a test tag;
5. intentionally deferred adjacent work;
6. one precise next technical step.

Do not combine persistence, graph semantics, numeric solving, and application into one block merely because they belong to one long-term feature. Do not persist a type owned by the Geometry layer. Do not create a second source of transform or geometry truth when an existing model-intent authority already owns the state.

## MVP 1: Single-part modeling

Canonical document: `docs/mvp-1-specification.md`.

Implemented:

- part documents and typed parameters/quantities;
- datum planes and sketches;
- rectangle/circle profile seeds;
- additive extrude and subtractive through-all cut intent;
- dependency graph, invalidation, recompute planning, and parameter updates;
- optional OCCT execution through `ShapeCache`;
- STEP export and JSON model-intent serialization.

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

Implemented scope includes semantic face-derived workplanes, projected/reference-driven geometry, construction geometry and relations, line/arc/spline/composite/detected profile regions, first sketch constraints/dimensions, and sketch-plane-relative extrude direction.

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

The presentation-helper chain remains frozen until a first GUI or CLI consumer requires it.

## MVP 3: Parametric bolt circle pattern

Canonical document: `docs/bolt-circle-pattern-mvp3.md`.

Implemented count quantities/parameters, `CircularHolePattern` intent, dependency edges, per-hole cut expansion, incremental count/radius recompute, JSON roundtrip, and a checked-in headless example.

Still deferred: hole-wizard semantics, partial/skipped patterns, and arbitrary seed-feature patterns.

## MVP 4: Assembly parameters and project container

Canonical documents:

- `docs/assembly-parameters-mvp4.md`
- `docs/project-container-mvp4.md`

Implemented assembly-scoped parameters, member-part registration, `ParameterBinding`, binding propagation, one explicit root assembly, project-owned parts, project-level parameter updates with per-part recompute plans, embedded project JSON, and headless update/recompute/export flows.

MVP-5 subsequently extended the container with project-owned child assembly documents. Manifest/external-file containers remain deferred.

## MVP 5: Assembly relationship, motion, hierarchy, and posed-geometry pipeline

Detailed implemented blocks are listed below. Their feature documents are canonical for exact contracts.

### 1. Component instances and placement/state

`docs/component-instance-mvp5.md`

Implemented occurrence identity, repeated part reuse, visibility, suppression, grounding, finite `RigidTransform`, explicit placement/state edits, JSON roundtrip, and headless inspection.

### 2. Local geometric constraint intent

`docs/assembly-constraint-model-intent-mvp5.md`

Implemented persistent local Mate, Concentric, Distance, Insert, and Angle intent with semantic target A/B identity and active/inactive state.

### 3. Deterministic local active-constraint graph

`docs/assembly-constraint-graph-mvp5.md`

Implemented local component nodes, active relationship multi-edges, deterministic ordering, connected groups, and no graph-cache persistence.

### 4. Semantic target resolution

`docs/assembly-constraint-target-resolution-mvp5.md`

Implemented generated planar face targets plus circular-cut `.axis` and `.seat` targets. Resolved geometry remains derived.

### 5. Rigid-transform evaluation

`docs/assembly-rigid-transform-evaluation-mvp5.md`

Implemented finite millimeter/degree transforms with active right-handed fixed-axis X-then-Y-then-Z rotation and `R = Rz * Ry * Rx` for column vectors.

### 6. Planar Mate/Distance residuals

`docs/assembly-planar-constraint-equations-mvp5.md`

Implemented deterministic read-only planar target and residual construction with target-order-sensitive signed separation.

### 7. Deterministic local rigid-body solver and application

`docs/assembly-rigid-body-solver-mvp5.md`

Implemented direct persisted-transform variables, shared residual evaluation, central finite differences, damped Gauss-Newton solving, deterministic damping/backtracking, solve states, complete snapshots, stale-result validation, proposals, and atomic converged-result application.

### 8. Local rank and remaining-DOF diagnostics

`docs/assembly-solve-diagnostics-mvp5.md`

Implemented local Jacobian-rank diagnostics with `remaining_dof = variable_count - rank(J)`.

### 9-12. Concentric and Insert semantic/numeric integration

Canonical documents:

- `docs/assembly-semantic-axis-concentric-residuals-mvp5.md`
- `docs/assembly-concentric-numeric-solver-dof-mvp5.md`
- `docs/assembly-insert-intent-composite-residuals-mvp5.md`
- `docs/assembly-insert-numeric-solver-dof-mvp5.md`

Implemented semantic axes/seating planes, canonical Concentric and Insert residuals, numeric flattening, shared solver integration, and regular one-free-body rank proofs.

### 13. Planar Angle constraint

`docs/assembly-angle-constraint-mvp5.md`

Implemented the scalar cosine alignment seed and shared numeric integration. Oriented signed planar angles remain deferred.

### 14. Suppressed components in solved groups

`docs/assembly-suppressed-component-solving-mvp5.md`

Implemented active-subgroup filtering while retaining complete component snapshots for stale-result validation.

### 15. Posed assembly STEP export

`docs/assembly-posed-step-export-mvp5.md`

Implemented visible-active component posing, deterministic referenced-part recompute, one cache per referenced part, one derived OCCT compound, and STEP export.

### 16. Revolute joint/limit intent and motion seed

`docs/assembly-revolute-joint-motion-mvp5.md`

Implemented persistent Revolute state/limits/coordinate intent, joint graph, signed drive residuals, shared numeric solver reuse, combined local relationship groups, joint snapshots, atomic transform plus selected-coordinate application, and `blcad_move_joint`.

### 17. Rigid subassembly hierarchy and nested posed export

`docs/assembly-rigid-subassembly-nested-export-mvp5.md`

Implemented project-owned child assemblies, persistent rigid `SubassemblyInstance` occurrences, hierarchy/cycle validation, deterministic rooted traversal, exact inner-to-outer transform chains, visible-active leaf flattening, repeated child occurrences, and nested posed STEP export.

### 18-20. Posed interference, clearance, and headless analysis

`docs/assembly-interference-analysis-mvp5.md`

Implemented shared posed-leaf shape construction, deterministic unordered leaf pairs, positive-volume interference, minimum-distance clearance violations, and `blcad_analyze_assembly`.

### 21. Document-scoped flexible child solving

`docs/assembly-flexible-subassembly-solving-mvp5.md`

Implemented exact active non-root occurrence selection, child-as-root local solve views, unchanged reuse of the local solver/application contracts, and atomic application to the referenced child `AssemblyDocument`.

Repeated occurrences of one child document share one internal component pose because the child document remains the sole persistent transform authority. Their rigid `SubassemblyInstance` boundary transforms stay independent and unchanged.

### 22. Cross-hierarchy relationship target and residual semantics

`docs/assembly-cross-hierarchy-relationship-semantics-mvp5.md`

Implemented the read-only geometric endpoint identity:

```text
(occurrence_path, local ComponentInstanceId, semantic_reference)
```

Implemented exact rooted occurrence resolution, local semantic target-resolver reuse, exact hierarchy transform-chain evaluation into root-assembly space, and read-only Mate/Distance/Angle/Concentric/Insert residual semantics.

No project-level persistent cross-hierarchy constraint, graph edge, numeric variable, solve result, or application path exists yet.

## Parameter expression seed

Canonical document: `docs/parameter-expression-mvp.md`.

Implemented unit-aware formulas on part documents, dependency edges from referenced parameters, topological re-evaluation, cycle rejection, direct-write rejection, JSON roundtrip with re-derived edges, incremental geometry recompute, and transactional formula editing with input-edge replacement.

Still deferred: plain/expression conversion, assembly-scope/cross-part expressions, functions, and further units.

## Next MVP sequence: Cross-hierarchy geometric constraint solving

Canonical planning document: `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

The previous plan incorrectly grouped persistent intent, JSON, graph semantics, numeric solving, snapshots, application, and diagnostics into one block. It also treated occurrence-qualified component identity as if it were automatically numeric-variable identity. That is incompatible with the implemented document-scoped flexible-subassembly persistence contract.

The corrected identity split is:

```text
geometric endpoint
  = (occurrence_path, local ComponentInstanceId, semantic_reference)

relationship node
  = (occurrence_path, local ComponentInstanceId)

persisted transform authority
  = (AssemblyDocumentId, local ComponentInstanceId)
```

Repeated occurrences of one shared child document are distinct geometric occurrences but may map to one shared persisted transform authority. Until occurrence-local pose overrides exist, they must not become independent six-variable transform blocks.

### 23. Core endpoint contract and project-level constraint intent — Next

Extract the frozen endpoint value contract into the Core layer and add persistent project-owned cross-hierarchy Mate/Distance/Angle/Concentric/Insert relationship records.

Do not add JSON, graph connectivity, numeric solving, snapshots, or result application in this block.

### 24. Additive cross-hierarchy constraint JSON and structure validation

Add backward-compatible `cross_hierarchy_constraints[]` project JSON, exact occurrence-path roundtrip, endpoint structural validation, and fail-closed loading.

Do not add graph or solver behavior in this block.

### 25. Occurrence relationship graph and transform-authority solve connectivity

Lift local active constraints once per rooted assembly occurrence, add project-level cross-hierarchy edges, derive path-sensitive suppression participation, map occurrence nodes to `(AssemblyDocumentId, ComponentInstanceId)` transform authorities, and derive solve groups coupled by both relationship adjacency and shared transform-authority identity.

Authority coupling contributes no residual row.

### 26. Shared numeric residual/Jacobian and solve-result integration

Use six variables per unique free active transform authority, not per occurrence node. Evaluate every relationship endpoint through its own parent transform chain while reading the candidate local transform of its shared authority. Reuse the existing five geometric residual families, finite-difference Jacobian semantics, and numeric solve engine.

Return derived solve results with complete transform-authority snapshots and at most one proposal per transform authority. Do not apply results in this block.

### 27. Atomic application and cross-hierarchy diagnostics

Validate current hierarchy/relationship context plus complete transform-authority snapshots, reject stale results, and atomically apply at most one direct local component transform per authority on a `Project` copy.

Rank/DOF diagnostics must use:

```text
variable_count = 6 * unique_free_active_transform_authority_count
```

not the number of free occurrence nodes.

### 28. Cross-hierarchy joints and nested motion propagation

Deferred until blocks 23-27 freeze geometric relationship persistence, solve connectivity, authority mapping, and application semantics.

### 29. Component geometry instancing and structured STEP assembly products

Deferred.

### 30. Richer collision/contact and swept-motion analysis

Deferred.

## Future roadmaps

- Multi-body and path features: `docs/multi-body-transform-and-path-features-roadmap.md`
- Inventor-like sketch/feature parity: `docs/inventor-like-sketcher-and-feature-roadmap.md`
- Advanced surfacing and 3D sketches: `docs/advanced-surfacing-and-3d-sketch-mvp.md`

Later assembly work also includes richer constraint/joint families, per-component/null-space DOF presentation, multi-turn motion, occurrence-local flexible pose overrides, whole-subassembly variables, engineering contact semantics, and structured exchange.

## Persistence rule

Persist model intent. Current persistent assembly intent includes component placement/state, local geometric constraints, local joint/limit/coordinate records, project-owned child assembly documents, and rigid subassembly occurrence placement/state.

Regenerate local graph connectivity, combined motion groups, hierarchy traversal, occurrence paths, parent transform chains, flattened leaves, cross-hierarchy read-only query values, resolved root-space geometry, residual descriptors, numeric residuals/Jacobians, solve/motion results, snapshots, diagnostics, shape caches, posed shapes, pair candidates, analysis products, and STEP compounds.

Only explicit application of a fresh converged solve or motion result changes persisted component `RigidTransform` intent. A successful motion application may additionally change a selected authored joint coordinate. Rigid subassembly placement/state changes only through explicit occurrence edits until a dedicated whole-subassembly solve-variable contract is implemented.

## Next technical step

Implement block 23 from `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`: Core-owned occurrence-qualified endpoint value intent plus persistent project-level cross-hierarchy geometric constraint records. No JSON, graph, solver, snapshot, or application integration belongs in the same block.

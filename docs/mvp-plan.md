# MVP Plan

This document is the implementation-sequence source of truth. Feature-specific documents are canonical for exact contracts, formulas, persistence details, failure policies, and focused proofs.

## Planning discipline

BLCAD grows through narrow headless vertical slices. A numbered block should cross one primary authority boundary at a time.

Preferred order when a feature spans multiple layers:

```text
Core model intent
  -> serialization / compatibility
  -> derived graph or query semantics
  -> numeric or geometry execution
  -> stale-result validation / atomic application
  -> diagnostics or presentation consumers
```

A block is ready to implement only when its canonical document states:

1. persistent authority and what remains derived;
2. stable identity and ordering rules;
3. explicit failure behavior;
4. focused acceptance tests and a test tag;
5. intentionally deferred adjacent work;
6. one precise next technical step.

Do not combine persistence, graph semantics, numeric solving, and application into one block merely because they belong to one long-term feature. Do not persist a Geometry-layer execution/query type. Do not create a second source of transform or geometry truth when existing model intent already owns the state.

## MVP 1: Single-part modeling

Canonical document: `docs/mvp-1-specification.md`.

Implemented seeds include part documents, typed parameters/quantities, datum planes, sketches, rectangle/circle profiles, additive extrude, subtractive through-all cut intent, dependency/invalidation/recompute planning, optional OCCT execution through `ShapeCache`, STEP export, and JSON model-intent serialization.

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

Sketch diagnostics/repair helper documents include:

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

## MVP 4: Assembly parameters and project container

Canonical documents:

- `docs/assembly-parameters-mvp4.md`
- `docs/project-container-mvp4.md`

Implemented assembly-scoped parameters, member-part registration, `ParameterBinding`, binding propagation, one explicit root assembly, project-owned parts, project-level parameter updates with per-part recompute plans, embedded project JSON, and headless update/recompute/export flows.

MVP-5 subsequently extended the container with project-owned child assembly documents and project-level cross-hierarchy geometric relationship intent.

## MVP 5: Assembly relationship, motion, hierarchy, and posed-geometry pipeline

Feature documents remain canonical for exact implementation details.

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

Implemented semantic axes/seating planes, canonical Concentric and Insert residuals, shared numeric integration, and regular one-free-body rank proofs.

### 13. Planar Angle constraint

`docs/assembly-angle-constraint-mvp5.md`

Implemented the scalar cosine alignment seed and shared numeric integration.

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

Implemented exact active non-root occurrence selection, child-as-root local solve views, unchanged reuse of local solver/application contracts, and atomic application to the referenced child `AssemblyDocument`.

Repeated occurrences of one child document share one internal component pose because the child document remains the sole persistent transform authority. Their rigid `SubassemblyInstance` boundary transforms remain independent and unchanged.

### 22. Cross-hierarchy relationship target and residual semantics

`docs/assembly-cross-hierarchy-relationship-semantics-mvp5.md`

Implemented read-only occurrence-qualified endpoint identity, exact rooted occurrence resolution, reuse of local semantic target resolution, exact hierarchy transform-chain evaluation into root-assembly space, and Mate/Distance/Angle/Concentric/Insert residual semantics.

### 23. Core endpoint contract and project-level cross-hierarchy intent

`docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`

Implemented Core-owned `AssemblyHierarchyConstraintEndpoint`, persistent Project-owned `AssemblyHierarchyConstraint`, all five established geometric relationship families, project-level collection/id scope, transform immutability, and direct bridging into the read-only Geometry query layer.

### 24. Additive cross-hierarchy JSON and structure validation

`docs/assembly-cross-hierarchy-constraint-json-mvp5.md`

Implemented additive `cross_hierarchy_constraints[]` project JSON, exact target/path ordering, Distance/Angle quantity roundtrip, absent-field backward compatibility, duplicate-id rejection, and Core-only exact occurrence-path/reached-component validation after ordinary hierarchy validation.

Semantic feature geometry remains unresolved during loading and Core structure validation.

### 25. Relationship-to-authority incidence and active solve groups

`docs/assembly-cross-hierarchy-incidence-groups-mvp5.md`

Implemented derived `ComponentTransformAuthority` identity, active local/project-level relationship collection, path/component suppression participation, exact endpoint-to-authority mapping, unique relationship-to-authority incidence, and deterministic connected cross-hierarchy solve groups.

Local constraints are collected once per containing `AssemblyDocument`. Repeated endpoint occurrences can retain distinct paths while mapping to one shared authority. A same-authority cross relationship therefore keeps two endpoint mappings but one unique authority incidence edge.

Pure-local connected components remain ordinary local solver work and are not returned as cross-hierarchy solve groups.

Focused tag:

```text
[core][assembly-cross-hierarchy-graph]
```

## Parameter expression seed

Canonical document: `docs/parameter-expression-mvp.md`.

Implemented unit-aware formulas on part documents, dependency edges from referenced parameters, topological re-evaluation, cycle rejection, direct-write rejection, JSON roundtrip with re-derived edges, incremental geometry recompute, and transactional formula editing with input-edge replacement.

Still deferred: plain/expression conversion, assembly-scope/cross-part expressions, functions, and further units.

## Next MVP sequence: Cross-hierarchy geometric constraint solving

Canonical planning document: `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

The frozen identity split is:

```text
geometric endpoint
  = (occurrence_path, local ComponentInstanceId, semantic_reference)

geometric component occurrence
  = (occurrence_path, local ComponentInstanceId)

component transform authority
  = (assembly_document: DocumentId, local ComponentInstanceId)
```

Repeated occurrences of one shared child document are distinct geometric occurrences but may map to one shared persistent transform authority. They do not become independent transform variable blocks until occurrence-local pose overrides exist.

Local constraints remain model-definition intent of one containing `AssemblyDocument` and are not duplicated per rooted occurrence.

### 26. Shared numeric residual/Jacobian and solve-result integration — Next

Solve one exact `AssemblyCrossHierarchySolveGroup` without applying the result.

Use six variables per unique free active `ComponentTransformAuthority`, ordered by the Block-25 authority order after free/active filtering:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

Grounded authorities remain residual dependencies but contribute no variable block.

Evaluate local relationships once in the containing assembly document's local space. Evaluate project-level cross-hierarchy relationships through each endpoint's exact parent transform chain in root-assembly space. Two endpoints may read one candidate authority transform while following different parent chains.

Concatenate residuals in exact Block-25 relationship order, preserve existing family scalar order and length scaling, and use central finite differences over authority-scoped variables.

Reuse the existing numeric solve engine. Refactor the evaluator boundary if necessary; do not copy the optimizer.

Return selected relationship identities, complete participating authority input snapshots, at most one proposal per free authority, solve state, iteration count, and residual summary. Do not apply proposals in this block.

Focused tag:

```text
[geometry][assembly-cross-hierarchy-solver]
```

### 27. Atomic application and cross-hierarchy diagnostics

Validate current authority inputs, persistent relationship intent, and participating hierarchy boundary inputs. Reject stale results and atomically apply at most one direct local component transform per authority on a `Project` copy.

The block must explicitly document the semantic target-producing part-model freshness boundary rather than silently claiming geometry-complete stale-result detection.

Rank/DOF diagnostics use:

```text
variable_count = 6 * unique_free_active_transform_authority_count
```

not the number of free geometric occurrences.

### 28. Cross-hierarchy joints and nested motion propagation

Deferred until Blocks 23-27 freeze geometric relationship persistence, solve connectivity, authority mapping, and application semantics.

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

Persist model intent. Current persistent assembly intent includes component placement/state, local geometric constraints, local joint/limit/coordinate records, project-owned child assembly documents, rigid subassembly occurrence placement/state, and project-owned cross-hierarchy geometric constraints with exact occurrence-qualified endpoints.

Project JSON roundtrips cross-hierarchy relationship intent through `cross_hierarchy_constraints[]`.

Regenerate local graph connectivity, combined motion groups, hierarchy traversal, parent transform chains, flattened leaves, cross-hierarchy target-resolution values, root-space geometry, residual descriptors, `ComponentTransformAuthority` identities, relationship-to-authority incidence, endpoint mappings, cross-hierarchy solve groups, numeric residuals/Jacobians, solve/motion results, snapshots, diagnostics, shape caches, posed shapes, pair candidates, analysis products, and STEP compounds.

Only explicit application of a fresh converged solve or motion result changes persisted component `RigidTransform` intent. A successful motion application may additionally change a selected authored joint coordinate. Rigid subassembly placement/state changes only through explicit occurrence edits until a dedicated whole-subassembly solve-variable contract exists.

## Next technical step

Implement Block 26 from `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`: authority-scoped mixed local/root-space residual evaluation, central finite-difference Jacobian construction, and shared numeric solve-result integration for one `AssemblyCrossHierarchySolveGroup`.

Do not add result application or cross-hierarchy diagnostics in the same block.

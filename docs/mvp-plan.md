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

A block is ready to implement only when its canonical document states persistent authority versus derived state, stable identity/order, failure behavior, focused acceptance tests/tags, deferred adjacent work, and one precise next technical step.

Do not combine persistence, graph semantics, numeric solving, and application merely because they belong to one long-term feature. Do not persist Geometry execution/query types or create a second transform/geometry authority.

## MVP 1: Single-part modeling

Canonical document: `docs/mvp-1-specification.md`.

Implemented part documents, typed parameters/quantities, datum planes, sketches, rectangle/circle profiles, additive extrude, subtractive through-all cut intent, dependency/invalidation/recompute planning, optional OCCT execution through `ShapeCache`, STEP export, and JSON model-intent serialization.

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

Implemented assembly-scoped parameters, member-part registration, `ParameterBinding`, propagation, one explicit root assembly, project-owned parts, project-level parameter updates with per-part recompute plans, embedded Project JSON, and headless update/recompute/export flows.

MVP-5 extended the container with Project-owned child assembly documents and Project-level cross-hierarchy relationship intent.

## MVP 5: Assembly relationship, motion, hierarchy, and posed-geometry pipeline

Feature documents remain canonical for exact details.

### 1. Component instances and placement/state

`docs/component-instance-mvp5.md`

Implemented occurrence identity, repeated part reuse, visibility, suppression, grounding, finite `RigidTransform`, placement/state edits, JSON roundtrip, and headless inspection.

### 2. Local geometric constraint intent

`docs/assembly-constraint-model-intent-mvp5.md`

Implemented persistent local Mate, Concentric, Distance, Insert, and Angle intent.

### 3. Deterministic local active-constraint graph

`docs/assembly-constraint-graph-mvp5.md`

Implemented local component nodes, active relationship multi-edges, deterministic ordering, connected groups, and derived graph state.

### 4. Semantic target resolution

`docs/assembly-constraint-target-resolution-mvp5.md`

Implemented generated planar face targets plus circular-cut `.axis` and `.seat` targets.

### 5. Rigid-transform evaluation

`docs/assembly-rigid-transform-evaluation-mvp5.md`

Implemented finite millimeter/degree transforms with active right-handed fixed-axis X-then-Y-then-Z rotation and `R = Rz * Ry * Rx`.

### 6. Planar Mate/Distance residuals

`docs/assembly-planar-constraint-equations-mvp5.md`

Implemented deterministic read-only planar target/residual construction with target-order-sensitive signed separation.

### 7. Deterministic local rigid-body solver and application

`docs/assembly-rigid-body-solver-mvp5.md`

Implemented direct transform variables, shared residual evaluation, central finite differences, damped Gauss-Newton solving, solve states, complete component snapshots, exact canonical semantic target PartDocument snapshots, stale-result validation, proposals, and atomic application.

Block 27 extends the local freshness contract to byte-for-byte canonical PartDocument model-intent comparison for Parts referenced by the complete solved component group. Flexible-child and local Revolute motion application inherit this same local applier boundary.

### 8. Local rank and remaining-DOF diagnostics

`docs/assembly-solve-diagnostics-mvp5.md`

Implemented local Jacobian-rank diagnostics with `remaining_dof = variable_count - rank(J)`. Matrix-rank elimination is now shared with Block-27 cross-hierarchy diagnostics.

### 9-12. Concentric and Insert semantic/numeric integration

Canonical documents:

- `docs/assembly-semantic-axis-concentric-residuals-mvp5.md`
- `docs/assembly-concentric-numeric-solver-dof-mvp5.md`
- `docs/assembly-insert-intent-composite-residuals-mvp5.md`
- `docs/assembly-insert-numeric-solver-dof-mvp5.md`

Implemented semantic axes/seating planes, canonical Concentric/Insert residuals, shared numeric integration, and regular one-free-body rank proofs.

### 13. Planar Angle constraint

`docs/assembly-angle-constraint-mvp5.md`

Implemented the scalar cosine alignment seed and shared numeric integration.

### 14. Suppressed components in solved groups

`docs/assembly-suppressed-component-solving-mvp5.md`

Implemented active-subgroup filtering while retaining complete component freshness context.

### 15. Posed assembly STEP export

`docs/assembly-posed-step-export-mvp5.md`

Implemented visible-active component posing, deterministic referenced-part recompute, one cache per referenced part, one derived OCCT compound, and STEP export.

### 16. Revolute joint/limit intent and motion seed

`docs/assembly-revolute-joint-motion-mvp5.md`

Implemented persistent local Revolute state/limits/coordinate intent, joint graph, signed drive residuals, shared numeric solver reuse, combined local relationship groups, joint snapshots, exact inherited semantic PartDocument freshness, atomic transform plus selected-coordinate application, and `blcad_move_joint`.

### 17. Rigid subassembly hierarchy and nested posed export

`docs/assembly-rigid-subassembly-nested-export-mvp5.md`

Implemented Project-owned child assemblies, rigid `SubassemblyInstance` occurrences, hierarchy/cycle validation, deterministic rooted traversal, exact inner-to-outer transform chains, visible-active leaf flattening, repeated child occurrences, and nested posed STEP export.

### 18-20. Posed interference, clearance, and headless analysis

`docs/assembly-interference-analysis-mvp5.md`

Implemented shared posed-leaf shape construction, deterministic unordered leaf pairs, positive-volume interference, minimum-distance clearance violations, and `blcad_analyze_assembly`.

### 21. Document-scoped flexible child solving

`docs/assembly-flexible-subassembly-solving-mvp5.md`

Implemented exact active non-root occurrence selection, child-as-root local solve views, ordinary local solver/application reuse, inherited semantic PartDocument freshness, and atomic application to the referenced child `AssemblyDocument`.

Repeated child occurrences share one internal component pose because the child document remains the sole persistent transform authority. Rigid `SubassemblyInstance` boundary transforms remain independent and unchanged.

### 22. Cross-hierarchy relationship target and residual semantics

`docs/assembly-cross-hierarchy-relationship-semantics-mvp5.md`

Implemented occurrence-qualified endpoint identity, exact rooted occurrence resolution, local semantic target reuse, exact hierarchy transform-chain evaluation into root space, and Mate/Distance/Angle/Concentric/Insert residual semantics.

### 23. Core endpoint contract and Project-level cross-hierarchy intent

`docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`

Implemented Core-owned `AssemblyHierarchyConstraintEndpoint`, persistent Project-owned `AssemblyHierarchyConstraint`, all five established geometric families, Project-level collection/id scope, and Geometry query bridging.

### 24. Additive cross-hierarchy JSON and structure validation

`docs/assembly-cross-hierarchy-constraint-json-mvp5.md`

Implemented additive `cross_hierarchy_constraints[]`, exact target/path order, quantity roundtrip, absent-field compatibility, duplicate-id rejection, and exact occurrence-path/reached-component validation after ordinary hierarchy validation.

### 25. Relationship-to-authority incidence and active solve groups

`docs/assembly-cross-hierarchy-incidence-groups-mvp5.md`

Implemented derived `ComponentTransformAuthority`, active local/Project-level relationship collection, suppression participation, endpoint-to-authority mapping, unique incidence, and deterministic connected cross-hierarchy solve groups.

Local constraints are collected once per containing `AssemblyDocument`. Repeated endpoints can retain distinct paths while mapping to one shared authority.

Focused tag:

```text
[core][assembly-cross-hierarchy-graph]
```

### 26. Shared numeric residual/Jacobian and solve-result integration

`docs/assembly-cross-hierarchy-numeric-solver-mvp5.md`

Implemented one exact current solve-group execution through unique free authority variable blocks, document-local local-relationship evaluation, root-space cross-relationship evaluation, shared five-family residual flattening, one central finite-difference implementation, and the existing damped Gauss-Newton engine.

Each free active authority contributes:

```text
tx_mm ty_mm tz_mm rx_deg ry_deg rz_deg
```

in canonical Block-25 authority order.

Repeated child occurrences sharing one authority observe one candidate direct transform and yield one variable block/proposal authority.

Focused tag:

```text
[geometry][assembly-cross-hierarchy-solver]
```

### 27. Complete modeled-input freshness, atomic authority application, and diagnostics

`docs/assembly-cross-hierarchy-application-diagnostics-mvp5.md`

Implemented complete authority snapshots with referenced-part identity; complete local/cross relationship record snapshots; exact active solve-group revalidation; exact hierarchy path-boundary snapshots; visibility-independent solve freshness; exact canonical PartDocument model-intent snapshots; atomic authority-qualified application; shared matrix-rank elimination; and cross-hierarchy rank/remaining-DOF diagnostics over the exact Block-26 free-authority order.

Semantic target-producing model freshness deliberately uses Option A:

```text
AssemblySemanticTargetPartSnapshot
  part_document
  exact serialize_part_document_to_json(part) payload
```

No hash and no mutable revision counter are introduced. Any serialized model-intent change in a participating referenced PartDocument invalidates the result conservatively.

The same helper extends ordinary local solve results and is inherited by flexible-child and local Revolute motion application.

Cross-hierarchy application writes only direct local component transforms addressed by `ComponentTransformAuthority` values on a Project copy and commits only after all freshness checks/writes succeed.

Diagnostics use:

```text
variable_count = 6 * unique_free_active_transform_authority_count
constrained_dof = rank(J)
remaining_dof = variable_count - rank(J)
```

Repeated rooted occurrences sharing one free child authority contribute six variables, not twelve.

Focused tags:

```text
[geometry][assembly-cross-hierarchy-application]
[geometry][assembly-cross-hierarchy-diagnostics]
[geometry][assembly-semantic-freshness]
```

## Parameter expression seed

Canonical document: `docs/parameter-expression-mvp.md`.

Implemented unit-aware part formulas, dependency edges, topological re-evaluation, cycle rejection, direct-write rejection, JSON roundtrip with re-derived edges, incremental geometry recompute, and transactional formula editing.

Still deferred: plain/expression conversion, assembly-scope/cross-part expressions, functions, and further units.

## Next MVP sequence: Cross-hierarchy motion relationships

Canonical planning document: `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

The frozen identity split remains:

```text
geometric endpoint
  = (occurrence_path, local ComponentInstanceId, semantic_reference)

geometric component occurrence
  = (occurrence_path, local ComponentInstanceId)

component transform authority
  = (assembly_document: DocumentId, local ComponentInstanceId)
```

Blocks 23-27 complete the cross-hierarchy geometric persistence/connectivity/numeric/fresh-application/diagnostics chain.

### 28. Cross-hierarchy Revolute joint intent and nested motion propagation — Next

Implement persistent Project-level occurrence-qualified Revolute joint intent with exact hierarchy endpoints and additive JSON.

Map joint endpoints to `ComponentTransformAuthority` while retaining exact endpoint occurrence context separately.

Derive deterministic motion connectivity over:

```text
active local geometric relationships
active local Revolute joints
active Project-level cross-hierarchy geometric relationships
active Project-level cross-hierarchy Revolute joints
```

The selected Project-level joint determines the combined motion component. Other active Revolute joints in that component receive deterministic holding drives at authored coordinates.

Evaluate cross-hierarchy Revolute `.seat` target geometry through exact parent chains in root space while reusing local directed-axis, seating, and signed-twist mathematics.

Deduplicate six-variable blocks by transform authority and reuse the Block-26 mixed numeric evaluator boundary, shared central finite differences, and shared Gauss-Newton engine.

Cross-hierarchy motion results must inherit/generalize Block-27 freshness for authority inputs, complete geometric/joint records, hierarchy path boundaries, exact canonical PartDocument model intent, and selected source/requested coordinate context.

Apply authority transforms plus the selected Project-level joint coordinate atomically on a Project copy.

Recommended focused tags:

```text
[core][assembly-cross-hierarchy-joint]
[core][assembly-cross-hierarchy-joint-json]
[core][assembly-cross-hierarchy-motion-graph]
[geometry][assembly-cross-hierarchy-revolute]
[geometry][assembly-cross-hierarchy-motion]
```

### 29. Component geometry instancing and structured STEP assembly products

Deferred.

### 30. Richer collision/contact and swept-motion analysis

Deferred.

## Future roadmaps

- Multi-body and path features: `docs/multi-body-transform-and-path-features-roadmap.md`
- Inventor-like sketch/feature parity: `docs/inventor-like-sketcher-and-feature-roadmap.md`
- Advanced surfacing and 3D sketches: `docs/advanced-surfacing-and-3d-sketch-mvp.md`

Later assembly work also includes richer joint families, per-authority/null-space DOF presentation, multi-turn motion, occurrence-local flexible pose overrides, whole-subassembly variables, engineering contact semantics, and structured exchange.

## Persistence rule

Persist model intent.

Current persistent assembly intent includes component placement/state, local geometric constraints, local Revolute joint/limit/coordinate records, Project-owned child assemblies, rigid subassembly occurrence placement/state, and Project-owned cross-hierarchy geometric constraints with exact occurrence-qualified endpoints.

Project JSON roundtrips cross-hierarchy geometric intent through `cross_hierarchy_constraints[]`.

Regenerate graph connectivity, combined motion groups, hierarchy traversal, parent chains, flattened leaves, target-resolution values, root-space geometry, residuals, transform authorities, incidence/mappings, solve groups, numeric residuals/Jacobians, solve/motion results, semantic/authority/relationship/boundary snapshots, proposals, rank products, diagnostics, shape caches, posed shapes, analysis products, and STEP compounds.

Only explicit fresh converged solve/motion application changes persisted component `RigidTransform` intent. Successful motion application may also change the selected authored joint coordinate. Rigid subassembly placement changes only through explicit occurrence edits until a dedicated whole-subassembly variable contract exists.

## Next technical step

Implement Block 28 from `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`: persistent occurrence-qualified Project-level cross-hierarchy Revolute joint intent, additive JSON, joint-to-transform-authority incidence, combined geometric/joint motion connectivity across assembly documents, root-space nested Revolute drive evaluation, shared authority-scoped numeric solving, and complete Block-27-style fresh atomic transform-plus-coordinate application.

Do not add occurrence-local child pose overrides, whole-subassembly transform variables, component geometry instancing, or swept-motion contact analysis in the same block.

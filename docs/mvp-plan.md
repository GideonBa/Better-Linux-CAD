# MVP Plan

This document is the implementation-sequence source of truth. Feature-specific documents are canonical for exact contracts, formulas, persistence details, failure policies, and focused proofs.

## Planning discipline

BLCAD grows through narrow headless vertical slices. A numbered block should cross one primary authority boundary at a time.

Preferred order for a feature spanning multiple layers:

```text
Core model intent
  -> serialization / compatibility
  -> derived graph or query semantics
  -> numeric or geometry execution
  -> stale-result validation / atomic application
  -> diagnostics or presentation consumers
```

A block is ready to implement only when its canonical document states:

1. persistent authority and derived data;
2. stable identity and ordering rules;
3. explicit failure behavior;
4. focused acceptance tests and a test tag;
5. intentionally deferred adjacent work;
6. one precise next technical step.

Do not combine persistence, graph semantics, numeric solving, and application merely because they belong to one long-term feature. Do not persist a Geometry-owned query type. Do not create a second source of transform or geometry truth when an existing model-intent authority already owns the state.

## MVP 1: Single-part modeling

Canonical document: `docs/mvp-1-specification.md`.

Implemented typed parameters/quantities, datum planes, sketches, rectangle/circle profile seeds, additive extrude and subtractive through-all cut intent, dependency/invalidation/recompute planning, optional OCCT execution through `ShapeCache`, STEP export, and JSON model-intent serialization.

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

The sketch diagnostics/repair presentation-helper chain remains frozen until a first GUI or CLI consumer requires it.

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

Implemented finite millimeter/degree transforms with active right-handed fixed-axis X-then-Y-then-Z rotation.

### 6. Planar Mate/Distance residuals

`docs/assembly-planar-constraint-equations-mvp5.md`

Implemented deterministic read-only planar residual construction with target-order-sensitive signed separation.

### 7. Deterministic local rigid-body solver and application

`docs/assembly-rigid-body-solver-mvp5.md`

Implemented six direct component-transform variables per free active local component, shared residual evaluation, central finite differences, damped Gauss-Newton solving, deterministic damping/backtracking, solve states, complete component snapshots, stale-result validation, proposals, and atomic converged-result application.

### 8. Local rank and remaining-DOF diagnostics

`docs/assembly-solve-diagnostics-mvp5.md`

Implemented Jacobian-rank diagnostics with:

```text
remaining_dof = variable_count - rank(J)
```

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

Implemented persistent Revolute state/limits/coordinate intent, local joint graph, signed drive residuals, shared numeric solver reuse, combined local relationship groups, joint snapshots, atomic transform plus selected-coordinate application, and `blcad_move_joint`.

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

### 22. Cross-hierarchy read-only target and residual semantics

`docs/assembly-cross-hierarchy-relationship-semantics-mvp5.md`

Implemented exact occurrence-qualified geometric target identity, rooted occurrence resolution, local semantic target-resolver reuse, exact hierarchy transform-chain evaluation into root-assembly space, and read-only Mate/Distance/Angle/Concentric/Insert residual semantics.

### 23. Core cross-hierarchy endpoint and persistent project-level constraint intent — Implemented

Canonical document: `docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`.

Implemented Core-owned endpoint identity:

```text
AssemblyHierarchyConstraintEndpoint =
  (occurrence_path,
   local ComponentInstanceId,
   semantic_reference)
```

Implemented persistent `AssemblyHierarchyConstraint` records for the existing five geometric families and a Project-owned cross-hierarchy constraint collection.

The project-level collection has its own id scope. Local constraint ids remain scoped by containing `AssemblyDocument`.

Record creation and addition validate value-local intent and project-level cross-hierarchy id uniqueness only. They do not resolve hierarchy paths, component structure, semantic geometry, or mutate transforms.

The existing read-only Geometry query layer accepts Core endpoints and complete persistent records through an explicit query bridge.

Focused tag:

```text
[core][assembly-cross-hierarchy-intent]
```

## Parameter expression seed

Canonical document: `docs/parameter-expression-mvp.md`.

Implemented unit-aware part-local formulas, dependency edges from referenced parameters, topological re-evaluation, cycle rejection, direct-write rejection, JSON roundtrip with re-derived edges, incremental geometry recompute, and transactional formula editing with input-edge replacement.

Still deferred: plain/expression conversion, assembly-scope/cross-part expressions, functions, and further units.

## Cross-hierarchy geometric constraint solve sequence

Canonical planning document: `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

The frozen identity split remains:

```text
geometric endpoint
  = (occurrence_path, local ComponentInstanceId, semantic_reference)

geometric component occurrence
  = (occurrence_path, local ComponentInstanceId)

persisted transform authority
  = (assembly_document: DocumentId, local ComponentInstanceId)
```

Repeated occurrences of one child document may be distinct geometric occurrences while mapping to one shared persisted transform authority. They must not become independent six-variable transform blocks until occurrence-local pose overrides exist.

Local constraints remain one model-definition relationship per containing `AssemblyDocument`; they are not duplicated per rooted occurrence.

### 24. Additive cross-hierarchy constraint JSON and structure validation — Next

Add backward-compatible `cross_hierarchy_constraints[]` project JSON, exact occurrence-path and target-order roundtrip, Distance/Angle quantity roundtrip, absent-field compatibility, and fail-closed occurrence-path/reached-component structure validation.

`Project::validate_assembly_structure()` must include cross-hierarchy endpoint structure only after the underlying assembly hierarchy has been validated.

Do not add connectivity or solver behavior in this block.

### 25. Relationship-to-transform-authority incidence and active solve groups

Collect local active constraints once per owned `AssemblyDocument`, collect active project-level cross-hierarchy relationships, map each participating relationship to the unique component transform authorities affecting its residual, and derive deterministic connected incidence groups.

Cross-hierarchy path suppression is endpoint-path-sensitive. Visibility does not filter geometric solve participation.

### 26. Shared numeric residual/Jacobian and solve-result integration

Use six variables per unique free active transform authority, not per geometric occurrence.

Evaluate local relationships once in containing-document local assembly space. Evaluate project-level cross-hierarchy relationships through exact endpoint parent chains in root-assembly space.

Reuse the five existing geometric residual families, length scaling, finite-difference Jacobian semantics, and numeric solve engine. Return complete transform-authority snapshots and at most one proposal per transform authority. Do not apply results in this block.

### 27. Atomic application and cross-hierarchy diagnostics

Validate current transform-authority inputs, persistent relationship intent, and participating hierarchy boundary inputs. Reject stale results and atomically apply at most one direct local component transform per authority on a `Project` copy.

The block must explicitly choose a semantic target-producing part-model freshness contract before claiming geometry-complete stale detection.

Diagnostics use:

```text
variable_count = 6 * unique_free_active_transform_authority_count
```

not the number of free geometric occurrences.

### 28. Cross-hierarchy joints and nested motion propagation

Deferred until Blocks 23-27 freeze geometric relationship persistence, structure, solve connectivity, authority mapping, and application semantics.

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

Persist model intent. Current persistent assembly intent includes component placement/state, local geometric constraints, local joint/limit/coordinate records, project-owned child assembly documents, rigid subassembly occurrence placement/state, and project-owned cross-hierarchy geometric constraint records.

The new cross-hierarchy records are currently persistent in the Core model but are not yet serialized by project JSON. Block 24 owns that compatibility boundary.

Regenerate local graphs, combined motion groups, hierarchy traversal, occurrence paths, parent transform chains, flattened leaves, Geometry query values, resolved root-space geometry, residual descriptors, incidence connectivity, numeric residuals/Jacobians, solve/motion results, snapshots, diagnostics, shape caches, posed shapes, pair candidates, analysis products, and STEP compounds.

Only explicit application of a fresh converged solve or motion result changes persisted component `RigidTransform` intent. A successful motion application may additionally change a selected authored joint coordinate. Rigid subassembly placement/state changes only through explicit occurrence edits until a dedicated whole-subassembly solve-variable contract is implemented.

## Next technical step

Implement **Block 24 only** from `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`:

```text
additive cross_hierarchy_constraints[] project JSON
  -> exact endpoint path and target-order roundtrip
  -> backward-compatible absent-field loading
  -> fail-closed occurrence-path and reached-component structure validation
```

Do not add incidence graphs, numeric variables, solving, snapshots, diagnostics, or application in the same block.

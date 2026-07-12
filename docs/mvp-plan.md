# MVP Plan

This document is the implementation-sequence source of truth. Feature-specific documents are canonical for exact contracts, formulas, persistence details, failure policies, ordering, and focused proofs.

## Planning discipline

BLCAD grows through narrow headless vertical slices. A numbered block should cross one primary authority boundary at a time.

Preferred order:

```text
Core model intent
  -> serialization / compatibility
  -> derived graph or query semantics
  -> numeric or geometry execution
  -> stale-result validation / atomic application
  -> diagnostics or presentation/exchange consumers
```

A block is ready only when its canonical document states persistent authority versus derived state, stable identity/order, failure behavior, focused tests/tags, deferred adjacent work, and one precise next technical step.

Do not persist Geometry execution/query types or introduce a second transform/geometry authority.

## MVP 1: Single-part modeling

Canonical document: `docs/mvp-1-specification.md`.

Implemented part documents, typed quantities/parameters, datum planes, sketches, basic profiles, additive/subtractive extrude intent, dependency/invalidation/recompute planning, optional OCCT execution through `ShapeCache`, STEP export, and JSON model-intent serialization.

## MVP 2: Semantic references and richer sketch-driven profiles

Implemented workplane resolution, general/composite profiles, construction geometry, projected/reference-driven sketch geometry, semantic references and recovery, sketch constraints/dimensions/diagnostics/repair helpers, automatic profile regions, arcs/trim/extend, extrude direction, and spline/tangent-continuity seeds.

The presentation-helper chain remains frozen until a GUI or CLI consumer requires it.

## MVP 3: Parametric bolt circle pattern

Canonical document: `docs/bolt-circle-pattern-mvp3.md`.

Implemented count quantities/parameters, `CircularHolePattern` intent, dependency edges, per-hole cut expansion, incremental recompute, JSON roundtrip, and a headless example.

## MVP 4: Assembly parameters and project container

Canonical documents:

- `docs/assembly-parameters-mvp4.md`
- `docs/project-container-mvp4.md`

Implemented assembly-scoped parameters, member-part registration, bindings, propagation, one explicit root assembly, Project-owned parts, parameter updates with per-part recompute plans, embedded Project JSON, and headless update/recompute/export flows.

MVP-5 extended the Project with child assembly documents and Project-level occurrence-qualified geometric and motion intent.

## MVP 5: Assembly relationship, motion, hierarchy, and posed-geometry pipeline

### 1-8. Local placement, geometric solving, application, and diagnostics

Canonical documents include:

- `docs/component-instance-mvp5.md`
- `docs/assembly-constraint-model-intent-mvp5.md`
- `docs/assembly-constraint-graph-mvp5.md`
- `docs/assembly-constraint-target-resolution-mvp5.md`
- `docs/assembly-rigid-transform-evaluation-mvp5.md`
- `docs/assembly-planar-constraint-equations-mvp5.md`
- `docs/assembly-rigid-body-solver-mvp5.md`
- `docs/assembly-solve-diagnostics-mvp5.md`

Implemented component placement/state, local geometric intent/connectivity, semantic target resolution, canonical transforms/residuals, authority-preserving direct transform solving, exact PartDocument model-intent freshness, atomic application, and local Jacobian-rank/nullity diagnostics.

### 9-13. Concentric, Insert, and Angle integration

Canonical documents:

- `docs/assembly-semantic-axis-concentric-residuals-mvp5.md`
- `docs/assembly-concentric-numeric-solver-dof-mvp5.md`
- `docs/assembly-insert-intent-composite-residuals-mvp5.md`
- `docs/assembly-insert-numeric-solver-dof-mvp5.md`
- `docs/assembly-angle-constraint-mvp5.md`

Implemented `.axis`/`.seat` semantics, Concentric and Insert residuals, full shared numeric integration, and planar Angle cosine alignment.

### 14. Suppressed components in solved groups

`docs/assembly-suppressed-component-solving-mvp5.md`

Implemented active-subgroup filtering while retaining complete freshness context.

### 15. Posed assembly STEP export

`docs/assembly-posed-step-export-mvp5.md`

Implemented visible-active component posing, deterministic referenced-part recompute, one cache per referenced part, one derived OCCT compound, and STEP export.

### 16. Local Revolute joint/limit intent and motion

`docs/assembly-revolute-joint-motion-mvp5.md`

Implemented persistent local Revolute state/limits/coordinate intent, local joint graph, directed axis/seating/signed-twist drives, combined local geometric/joint closure, shared numeric engine, inherited exact PartDocument freshness, atomic transform plus selected-coordinate application, and `blcad_move_joint`.

### 17. Rigid subassembly hierarchy and nested posed export

`docs/assembly-rigid-subassembly-nested-export-mvp5.md`

Implemented Project-owned child assemblies, rigid `SubassemblyInstance` occurrences, cycle-free hierarchy validation, deterministic rooted traversal, exact inner-to-outer transform chains, visible-active leaf flattening, repeated child occurrences, and nested posed STEP export.

### 18-20. Posed interference, clearance, and headless analysis

`docs/assembly-interference-analysis-mvp5.md`

Implemented shared posed-leaf shape construction, deterministic unordered leaf pairs, positive-volume interference, minimum-distance clearance violations, and `blcad_analyze_assembly`.

### 21. Document-scoped flexible child solving

`docs/assembly-flexible-subassembly-solving-mvp5.md`

Implemented exact active child occurrence selection, child-as-local-root solving, ordinary local solver/application reuse, inherited semantic PartDocument freshness, and atomic application to the referenced child `AssemblyDocument`.

Repeated child occurrences share one child-document internal pose. Rigid boundary transforms remain independent and unchanged.

### 22. Cross-hierarchy target and residual semantics

`docs/assembly-cross-hierarchy-relationship-semantics-mvp5.md`

Implemented occurrence-qualified endpoint identity, exact rooted occurrence resolution, local semantic target reuse, exact root-space transform-chain evaluation, and all five geometric residual families.

### 23. Core endpoint and Project-level geometric intent

`docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`

Implemented Core-owned `AssemblyHierarchyConstraintEndpoint`, persistent Project-owned `AssemblyHierarchyConstraint`, five geometric families, independent Project-level id scope, and Geometry query bridging.

### 24. Cross-hierarchy geometric JSON and structure validation

`docs/assembly-cross-hierarchy-constraint-json-mvp5.md`

Implemented additive `cross_hierarchy_constraints[]`, exact target/path order, quantity roundtrip, absent-field compatibility, duplicate rejection, and exact path/reached-component Core validation.

### 25. Relationship-to-authority incidence and solve groups

`docs/assembly-cross-hierarchy-incidence-groups-mvp5.md`

Implemented derived `ComponentTransformAuthority`, active local/Project cross relationship participation, suppression semantics, endpoint-to-authority mapping, unique incidence, and deterministic connected geometric solve groups.

Repeated rooted endpoints may retain distinct paths while sharing one authority.

### 26. Shared cross-hierarchy numeric solving

`docs/assembly-cross-hierarchy-numeric-solver-mvp5.md`

Implemented one exact current solve-group execution through unique free authority variable blocks, document-local local relationship evaluation, root-space Project relationship evaluation, shared residual flattening, central finite differences, and the existing damped Gauss-Newton engine.

Each free authority contributes:

```text
tx_mm ty_mm tz_mm rx_deg ry_deg rz_deg
```

### 27. Complete freshness, atomic authority application, and diagnostics

`docs/assembly-cross-hierarchy-application-diagnostics-mvp5.md`

Implemented complete authority/relationship/path-boundary snapshots, exact current group revalidation, exact canonical PartDocument model-intent snapshots, atomic authority application, shared matrix-rank elimination, and cross-hierarchy rank/remaining-DOF diagnostics.

Semantic target freshness uses:

```text
AssemblySemanticTargetPartSnapshot
  part_document
  exact serialize_part_document_to_json(part) payload
```

Diagnostics use:

```text
variable_count = 6 * unique_free_active_transform_authority_count
constrained_dof = rank(J)
remaining_dof = variable_count - rank(J)
```

### 28. Project-level occurrence-qualified Revolute motion — Implemented

`docs/assembly-cross-hierarchy-revolute-motion-mvp5.md`

Implemented persistent `AssemblyHierarchyJoint` Revolute intent and Project-owned `cross_hierarchy_joints[]` with exact occurrence-qualified target A/B identity, Project-level joint id scope, principal limits, and authored coordinate.

Implemented `AssemblyCrossHierarchyMotionGraph` connectivity over:

```text
local geometry
local joints
Project-level cross-hierarchy geometry
Project-level cross-hierarchy joints
```

Canonical relationship-kind order is the order above. Participation is state/suppression based and ignores visibility.

Cross-joint endpoints map to `ComponentTransformAuthority`; endpoint mappings remain separate from unique incidence. Two different rooted endpoints may map to one shared authority and therefore one six-variable block.

`AssemblyHierarchyRevoluteJointEquationBuilder` resolves exact `.seat` endpoints through hierarchy parent chains into root space and reuses the same directed axis/seating/signed-twist residual constructor as the local Revolute path.

The selected Project-level joint receives the requested in-range drive. Every other active local or Project-level Revolute joint in the combined motion group receives a transient holding drive at its authored coordinate. All geometric relationships remain active residuals.

Motion reuses the authority variable order, central finite differences, and `solve_numeric_variables`. It returns complete authority, four-family relationship, hierarchy-boundary, and exact semantic PartDocument snapshots plus at most one proposal per free authority.

Shared Block-27 freshness helpers now protect both geometric solve application and cross-hierarchy motion application. Fresh motion application atomically writes direct authority transforms and only the selected Project-level joint coordinate on a Project copy.

Focused tags:

```text
[core][assembly-cross-hierarchy-joint]
[core][assembly-cross-hierarchy-joint-json]
[core][assembly-cross-hierarchy-motion-graph]
[geometry][assembly-cross-hierarchy-revolute]
[geometry][assembly-cross-hierarchy-motion]
```

## Parameter expression seed

Canonical document: `docs/parameter-expression-mvp.md`.

Implemented unit-aware part formulas, dependency edges, topological re-evaluation, cycle rejection, JSON roundtrip with re-derived edges, incremental recompute, and transactional formula editing.

## Next MVP sequence: structured assembly exchange

Canonical planning document: `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

### 29. Component geometry instancing and structured STEP assembly products — Next

Define stable derived exchange identities for assembly occurrences and part-product/component occurrences.

Requirements:

```text
reuse explicit root + exact rooted SubassemblyInstance paths
reuse AssemblyLeafOccurrenceResolver transform chains
reuse one recomputed shape/cache per unique referenced PartDocument
preserve repeated child assembly occurrences as distinct exchange occurrences
preserve repeated part occurrences as distinct component/product instances
avoid duplicating persistent PartDocument intent
emit structured STEP assembly/product relationships
retain existing flattened compound export as an explicit compatibility consumer if useful
```

The exact product/occurrence naming and deterministic ordering must be canonical before writing STEP structure.

Focused tests must prove repeated child assemblies, repeated parts, nested hierarchy, hidden/suppressed filtering policy, deterministic product order/names, and unchanged source Project/model intent.

Do not add occurrence-local flexible pose overrides or whole-subassembly solve variables in Block 29.

### 30. Richer collision/contact and swept-motion analysis

Deferred until structured assembly exchange identity is frozen.

## Future roadmaps

- Multi-body and path features: `docs/multi-body-transform-and-path-features-roadmap.md`
- Inventor-like sketch/feature parity: `docs/inventor-like-sketcher-and-feature-roadmap.md`
- Advanced surfacing and 3D sketches: `docs/advanced-surfacing-and-3d-sketch-mvp.md`

Later assembly work includes richer joint families, null-space basis presentation, multi-turn motion, occurrence-local flexible pose overrides, whole-subassembly variables, engineering contact semantics, and swept-motion analysis.

## Persistence rule

Persist model intent.

Current persistent assembly intent includes component placement/state, local geometric constraints, local Revolute joint/limit/coordinate records, Project-owned child assemblies, rigid subassembly occurrence placement/state, Project-owned cross-hierarchy geometric constraints, and Project-owned occurrence-qualified cross-hierarchy Revolute joint records.

Project JSON roundtrips:

```text
cross_hierarchy_constraints[]
cross_hierarchy_joints[]
```

Regenerate graph connectivity, hierarchy traversal, parent chains, flattened leaves, target-resolution values, root-space geometry, transform authorities, incidence/mappings, solve/motion groups, holding drives, residuals/Jacobians, solve/motion results, freshness snapshots, proposals, rank products, diagnostics, shape caches, posed shapes, analysis products, and STEP exchange products.

Only explicit fresh converged solve/motion application changes persisted component `RigidTransform` intent. Successful motion application may additionally change the selected authored local or Project-level joint coordinate. Rigid subassembly placement changes only through explicit occurrence edits until a dedicated whole-subassembly variable contract exists.

## Next technical step

Implement Block 29 only from `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`: component geometry instancing and structured STEP assembly products with stable derived exchange occurrence/product identity, canonical deterministic ordering/naming, reuse of posed-leaf transform chains, and preservation of repeated hierarchy occurrences as distinct STEP assembly instances.

Do not add occurrence-local child pose overrides, whole-subassembly transform variables, or swept-motion contact simulation in the same block.

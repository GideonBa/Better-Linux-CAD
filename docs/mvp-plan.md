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
  -> diagnostics, analysis, or presentation/exchange consumers
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

## MVP 5: Assembly relationship, motion, hierarchy, posed geometry, and exchange

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

Implemented `.axis`/`.seat` semantics, Concentric and Insert residuals, full shared numeric integration, and planar Angle cosine alignment.

Canonical details are in the assembly semantic/numeric MVP-5 documents.

### 14. Suppressed components in solved groups

`docs/assembly-suppressed-component-solving-mvp5.md`

Implemented active-subgroup filtering while retaining complete freshness context.

### 15. Flattened posed assembly STEP export

`docs/assembly-posed-step-export-mvp5.md`

Implemented visible-active component posing, deterministic unique referenced-part recompute, exact leaf transform chains, one derived OCCT compound, and generic STEP export.

Block 29 later extracts the shared part-definition and OCCT transform helpers while retaining this flattened compatibility path.

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

### 23-27. Cross-hierarchy geometric persistence, solving, freshness, and diagnostics

Canonical documents:

- `docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`
- `docs/assembly-cross-hierarchy-constraint-json-mvp5.md`
- `docs/assembly-cross-hierarchy-incidence-groups-mvp5.md`
- `docs/assembly-cross-hierarchy-numeric-solver-mvp5.md`
- `docs/assembly-cross-hierarchy-application-diagnostics-mvp5.md`

Implemented:

```text
Core-owned occurrence-qualified endpoint intent
cross_hierarchy_constraints[]
exact hierarchy path/reached-component validation
ComponentTransformAuthority incidence
one six-variable block per unique free authority
document-local local residual evaluation
root-space Project-level residual evaluation
shared finite differences and Gauss-Newton engine
complete authority/relationship/path/PartDocument freshness
atomic direct authority application
cross-hierarchy Jacobian-rank/remaining-DOF diagnostics
```

The transform authority remains:

```text
(assembly_document: DocumentId,
 local ComponentInstanceId)
```

Repeated rooted endpoints can remain distinct geometric contexts while sharing one variable/proposal authority.

### 28. Project-level occurrence-qualified Revolute motion

`docs/assembly-cross-hierarchy-revolute-motion-mvp5.md`

Implemented persistent `AssemblyHierarchyJoint` Revolute intent and additive `cross_hierarchy_joints[]` with exact occurrence-qualified target A/B identity, Project-level id scope, limits, and authored coordinate.

`AssemblyCrossHierarchyMotionGraph` closes transitively over:

```text
local geometry
local joints
Project-level cross-hierarchy geometry
Project-level cross-hierarchy joints
```

The hierarchy Revolute builder resolves exact `.seat` endpoints into root space and reuses the local directed axis/seating/signed-twist residual constructor.

Selected Project-level joints receive requested in-range drives; other active local or Project-level Revolute joints in the same group receive holding drives at authored coordinates. Motion reuses authority variables, finite differences, numeric solve machinery, Block-27 freshness helpers, and atomic application.

### 29. Structured assembly exchange and STEP product relationships — Implemented

`docs/assembly-structured-step-products-mvp5.md`

Implemented three derived exchange identities:

```text
assembly occurrence
  = exact rooted SubassemblyInstance occurrence path

part component occurrence
  = (containing assembly occurrence path,
     local ComponentInstanceId)

part product definition
  = referenced PartDocumentId
```

`AssemblyExchangeGraph` derives the explicit root plus every rooted assembly path prefix required by one canonical visible-active leaf.

Ordering is deterministic:

```text
assembly occurrences: lexicographic path sequence
component occurrences: path, then ComponentInstanceId
part definitions: PartDocumentId
```

Generated exchange names percent-encode all UTF-8 bytes outside `A-Z a-z 0-9 . _ -`; path separators and the explicit root sentinel therefore cannot collide with authored ids.

`AssemblyPartShapeDefinitionBuilder` sorts/deduplicates referenced part ids and performs exactly one recompute plus one private `ShapeCache` per unique exported `PartDocumentId`.

The flattened posed-leaf builder and structured STEP exporter reuse this same part-definition boundary and one shared OCCT rigid-transform conversion.

`AssemblyStructuredStepExporter` creates:

```text
one XDE part definition label per PartDocumentId
one assembly label per rooted assembly occurrence
part component references to shared definitions
parent -> child assembly occurrence references
```

The explicit root label is transferred through `STEPCAFControl_Writer` with names enabled.

The existing `AssemblyStepExporter` remains the flattened compound compatibility consumer.

Headless consumer:

```text
blcad_export_structured_assembly <input.blcad.project.json> <output.step>
```

The structured command exports current authored/persisted pose and does not implicitly run a solver.

Focused tags:

```text
[core][assembly-exchange-graph]
[geometry][assembly-structured-step-export]
```

Block 29 adds no persistent record and no JSON field.

## Parameter expression seed

Canonical document: `docs/parameter-expression-mvp.md`.

Implemented unit-aware part formulas, dependency edges, topological re-evaluation, cycle rejection, JSON roundtrip with re-derived edges, incremental recompute, and transactional formula editing.

## Next MVP sequence: richer contact and swept-motion analysis

Canonical planning document: `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

### 30. Richer collision/contact and swept-motion analysis — Next

Freeze a richer posed contact result over exact rooted component occurrence pairs.

The first static classification should explicitly define and test:

```text
Separated
Touching
Interfering
```

Any touching/separation tolerance must be an explicit validated input or frozen documented default.

A first swept-motion seed should remain bounded and deterministic:

```text
selected supported Revolute joint
start coordinate
end coordinate
explicit/validated sample resolution
```

The implementation must state honestly that sampled sweep analysis is not continuous collision detection.

Reuse:

```text
AssemblyLeafOccurrenceResolver
shared part shape definition builder
existing posed geometry semantics
exact rooted exchange/component occurrence identity
current Revolute motion solve/application boundaries
```

Do not introduce a second pose model, mutate the source Project during queries, or build a general physics engine.

## Future roadmaps

- Multi-body and path features: `docs/multi-body-transform-and-path-features-roadmap.md`
- Inventor-like sketch/feature parity: `docs/inventor-like-sketcher-and-feature-roadmap.md`
- Advanced surfacing and 3D sketches: `docs/advanced-surfacing-and-3d-sketch-mvp.md`

Later assembly work includes richer joint families, null-space basis presentation, multi-turn motion, occurrence-local flexible pose overrides, whole-subassembly variables, and general dynamic/contact response.

## Persistence rule

Persist model intent.

Current persistent assembly intent includes component placement/state, local geometric constraints, local Revolute joint/limit/coordinate records, Project-owned child assemblies, rigid subassembly occurrence placement/state, Project-owned cross-hierarchy geometric constraints, and Project-owned occurrence-qualified cross-hierarchy Revolute joint records.

Project JSON roundtrips:

```text
cross_hierarchy_constraints[]
cross_hierarchy_joints[]
```

Regenerate graph connectivity, hierarchy traversal, parent chains, flattened leaves, target geometry, transform authorities, incidence/mappings, solve/motion groups, holding drives, residuals/Jacobians, solve/motion results, freshness snapshots, proposals, diagnostics, exchange identities/graphs/names, part shape definitions, XDE labels/references, STEP entities, posed shapes, and analysis products.

Only explicit fresh converged solve/motion application changes persisted component `RigidTransform` intent. Successful motion application may additionally change the selected authored local or Project-level joint coordinate. Rigid subassembly placement changes only through explicit occurrence edits until a dedicated whole-subassembly variable contract exists.

## Next technical step

Implement Block 30 only from `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`: richer posed contact classification and bounded deterministic swept-Revolute analysis over exact rooted component occurrence identities.

Do not add occurrence-local child pose overrides, whole-subassembly transform variables, richer joint families, or a general physics engine in the same block.

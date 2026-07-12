# MVP Plan

This document is the implementation-sequence source of truth. Feature-specific documents are canonical for exact contracts, formulas, persistence details, failure policies, ordering, and focused proofs.

## Planning discipline

BLCAD grows through narrow headless vertical slices. A numbered block should cross one primary authority boundary at a time.

Preferred order:

```text
Core model intent
  -> serialization / compatibility
  -> stable semantic identity
  -> derived graph or query semantics
  -> geometry / numeric execution
  -> freshness / atomic application when mutation is required
  -> diagnostics, analysis, or exchange consumers
```

A block is ready only when its canonical document states persistent authority versus derived state, stable identity/order, failure behavior, focused tests/tags, deferred adjacent work, and one precise next technical step.

Do not persist Geometry execution/query products or introduce a second transform, hierarchy, pose, or numeric authority.

## MVP 1: Single-part modeling

Canonical document: `docs/mvp-1-specification.md`.

Implemented part documents, typed quantities/parameters, datum planes, sketches, basic profiles, additive/subtractive extrude intent, dependency/invalidation/recompute planning, optional OCCT execution through `ShapeCache`, STEP export, and JSON model-intent serialization.

## MVP 2: Semantic references and richer sketch-driven profiles

Implemented workplane resolution, general/composite profiles, construction geometry, projected/reference-driven sketch geometry, semantic references and recovery, sketch constraints/dimensions/diagnostics/repair helpers, automatic profile regions, arcs/trim/extend, extrude direction, and spline/tangent-continuity seeds.

## MVP 3: Parametric bolt circle pattern

Canonical document: `docs/bolt-circle-pattern-mvp3.md`.

Implemented count quantities/parameters, `CircularHolePattern` intent, dependency edges, per-hole cut expansion, incremental recompute, JSON roundtrip, and a headless example.

## MVP 4: Assembly parameters and project container

Canonical documents:

- `docs/assembly-parameters-mvp4.md`
- `docs/project-container-mvp4.md`

Implemented assembly-scoped parameters, member-part registration, bindings, propagation, one explicit root assembly, Project-owned parts, parameter updates with per-part recompute plans, embedded Project JSON, and headless update/recompute/export flows.

## MVP 5: Assembly relationships, motion, hierarchy, posed geometry, analysis, and exchange

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

### 14. Suppressed components in solved groups

Canonical document: `docs/assembly-suppressed-component-solving-mvp5.md`.

Implemented active-subgroup filtering while retaining complete freshness context.

### 15. Flattened posed assembly STEP export

Canonical document: `docs/assembly-posed-step-export-mvp5.md`.

Implemented visible-active component posing, deterministic unique referenced-part recompute, exact leaf transform chains, one derived OCCT compound, and generic STEP export. Block 29 later extracted shared part-definition and OCCT transform helpers while retaining this compatibility path.

### 16. Local Revolute joint/limit intent and motion

Canonical document: `docs/assembly-revolute-joint-motion-mvp5.md`.

Implemented persistent local Revolute state/limits/coordinate intent, local joint graph, directed axis/seating/signed-twist drives, combined local geometric/joint closure, shared numeric engine, exact PartDocument freshness, atomic transform plus selected-coordinate application, and `blcad_move_joint`.

### 17. Rigid subassembly hierarchy and nested posed export

Canonical document: `docs/assembly-rigid-subassembly-nested-export-mvp5.md`.

Implemented Project-owned child assemblies, rigid `SubassemblyInstance` occurrences, cycle-free hierarchy validation, deterministic rooted traversal, exact inner-to-outer transform chains, visible-active leaf flattening, repeated child occurrences, and nested posed STEP export.

### 18-20. Posed interference, clearance, and headless analysis

Canonical document: `docs/assembly-interference-analysis-mvp5.md`.

Implemented shared posed-leaf shape construction, deterministic unordered leaf pairs, positive-volume interference, minimum-distance clearance violations, and `blcad_analyze_assembly`.

### 21. Document-scoped flexible child solving

Canonical document: `docs/assembly-flexible-subassembly-solving-mvp5.md`.

Implemented exact active child occurrence selection, child-as-local-root solving, ordinary local solver/application reuse, inherited semantic PartDocument freshness, and atomic application to the referenced child `AssemblyDocument`.

Repeated child occurrences share one child-document internal pose. Rigid boundary transforms remain independent and unchanged.

### 22. Cross-hierarchy target and residual semantics

Canonical document: `docs/assembly-cross-hierarchy-relationship-semantics-mvp5.md`.

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

Transform authority remains:

```text
(assembly_document: DocumentId,
 local ComponentInstanceId)
```

Repeated rooted endpoints can remain distinct geometric contexts while sharing one variable/proposal authority.

### 28. Project-level occurrence-qualified Revolute motion

Canonical document: `docs/assembly-cross-hierarchy-revolute-motion-mvp5.md`.

Implemented persistent `AssemblyHierarchyJoint` Revolute intent and additive `cross_hierarchy_joints[]` with exact occurrence-qualified target identity, Project-level id scope, limits, and authored coordinate.

`AssemblyCrossHierarchyMotionGraph` closes transitively over local geometry, local joints, Project-level cross geometry, and Project-level cross joints. Exact `.seat` endpoints evaluate in root space and reuse the local directed axis/seating/signed-twist residual constructor.

Selected Project-level joints receive requested drives; other active Revolute joints in the same group receive authored-coordinate holding drives. Motion reuses authority variables, finite differences, numeric solve machinery, complete freshness, and atomic application.

### 29. Structured assembly exchange and STEP product relationships

Canonical document: `docs/assembly-structured-step-products-mvp5.md`.

Implemented derived identities:

```text
assembly occurrence
  = exact rooted SubassemblyInstance path

component occurrence
  = (containing rooted assembly path,
     local ComponentInstanceId)

part product definition
  = referenced PartDocumentId
```

`AssemblyExchangeGraph` derives the explicit root plus every rooted assembly path prefix required by one canonical visible-active leaf. Assembly/component/part order is deterministic.

`AssemblyPartShapeDefinitionBuilder` recomputes each unique exported `PartDocumentId` once. Flattened posed geometry and structured STEP reuse the same part-definition and OCCT transform boundaries.

`AssemblyStructuredStepExporter` creates XDE part definitions, rooted assembly labels, shared part component references, and exact parent-child assembly references. `AssemblyStepExporter` remains the flattened compatibility consumer.

Block 29 adds no persistent record and no JSON field.

### 30. Rooted contact classification and bounded sampled Revolute sweep — Implemented

Canonical document: `docs/assembly-contact-swept-motion-mvp5.md`.

Implemented exact typed rooted pair identity:

```text
AssemblyComponentOccurrencePairIdentity =
  canonical ordered pair of
  AssemblyExchangeComponentOccurrenceIdentity values
```

`AssemblyContactAnalyzer` reuses canonical posed leaves and classifies every visible-active unordered pair once:

```text
overlap_volume_mm3 > minimum_overlap_volume_mm3
  -> Interfering

otherwise minimum_distance_mm <= touching_tolerance_mm
  -> Touching

otherwise
  -> Separated
```

Defaults:

```text
touching_tolerance_mm = 1.0e-6
minimum_overlap_volume_mm3 = 1.0e-6
```

`AssemblyContactAnalysis::records` is a complete classification set rather than a violation-only list. The historical interference and clearance APIs remain unchanged compatibility contracts.

`AssemblyRevoluteSweepAnalyzer` supports exactly:

```text
RootAssemblyLocal
ProjectCrossHierarchy
```

The selected joint must be active Revolute intent in that scope. Start/end values are `AngleDeg` inside authored limits. Sample count is bounded:

```text
2 <= sample_count <= 1001
```

Samples are linear, inclusive, and preserve caller direction. Every sample starts from a fresh copy of the same source Project, delegates to the existing scope-specific motion solver and atomic applier, then runs complete contact classification.

No sample accumulates the preceding sample's numeric state. Source transforms and authored joint coordinates remain unchanged.

Block 30 is deterministic discrete sampled analysis, not continuous collision detection. Events existing only between requested sample coordinates may be missed.

Focused tags:

```text
[geometry][assembly-contact]
[geometry][assembly-revolute-sweep]
```

Block 30 adds no persistent record, no JSON field, and no analysis CLI.

## Parameter expression seed

Canonical document: `docs/parameter-expression-mvp.md`.

Implemented unit-aware part formulas, dependency edges, topological re-evaluation, cycle rejection, JSON roundtrip with re-derived edges, incremental recompute, and transactional formula editing.

## Blocks 31-47: general assembly targets, relationships, and richer joints

Canonical roadmap: `docs/assembly-general-geometric-target-roadmap.md`.

Mandatory order:

```text
31 typed geometric target taxonomy and capability projection
32 assembly-selectable reference geometry Core intent
33 reference geometry serialization and structure validation
34 datum/axis/line/point target resolution
35 stable semantic generated topology identity/recovery
36 generated face/edge/vertex target resolution
37 explicit target compatibility matrix
38 generic geometric relationship Core intent + JSON
39 generic relationship equations + shared solve integration
40 joint target compatibility + oriented Frame contract
41 general joint coordinate/limit Core model
42 general joint coordinate JSON/backward compatibility
43 vector joint drives + holding/freshness/atomic application
44 Prismatic joint
45 Cylindrical joint
46 Planar joint
47 Ball/Spherical joint
```

Do not merge these blocks into one feature block.

### 31. Typed geometric target taxonomy and capability projection — Next

Primary boundary: Geometry-derived query semantics.

Separate selected semantic source kind from solver geometric capability.

Planned source kinds:

```text
GeneratedPlanarFace
GeneratedCylindricalFace
GeneratedLinearEdge
GeneratedCircularEdge
GeneratedVertex
DatumPlane
DatumAxis
ConstructionLine
ConstructionPoint
CircularFeatureSeat
```

Planned capabilities:

```text
Plane
Axis
Line
Point
Circle
Cylinder
Frame
```

Representative projections:

```text
GeneratedCylindricalFace -> Cylinder + Axis
GeneratedCircularEdge -> Circle + Axis + Point(center)
DatumPlane -> Plane
DatumAxis -> Axis + Line
CircularFeatureSeat -> Frame + Axis + Plane
```

Existing `.top/.bottom/.right/.left/.front/.back`, `.axis`, and `.seat` behavior must migrate without changing persistent target strings or numeric semantics.

Block 31 adds no source grammar, compatibility matrix, relationship family, joint family, DatumAxis persistence, or JSON shape.

### 32-47. Planned continuation

Blocks 32-36 establish assembly-selectable reference geometry, serialization, reference target resolution, stable producer-driven generated topology identity/recovery, and generated face/edge/vertex resolution.

Blocks 37-40 add one deterministic compatibility layer, persistent `Coincident`/`Parallel`/`Perpendicular` intent and JSON, their equations through existing local/cross solver paths, and an oriented `Frame` contract for joint compatibility.

Blocks 41-43 generalize family-defined typed joint coordinate/limit slots, serialize them with historical scalar Revolute compatibility, and add vector-drive/holding/freshness/atomic-application infrastructure.

Blocks 44-47 add exactly one richer joint family per block: Prismatic, Cylindrical, Planar, then Ball/Spherical.

Exact future target, compatibility, relationship, and joint semantics are canonical only in `docs/assembly-general-geometric-target-roadmap.md` until the corresponding block is implemented.

## Persistence rule

Persist model intent.

Current persistent assembly intent includes component placement/state, local geometric constraints, local Revolute joint/limit/coordinate records, Project-owned child assemblies, rigid subassembly occurrence placement/state, Project-owned cross-hierarchy geometric constraints, and Project-owned occurrence-qualified cross-hierarchy Revolute joint records.

Project JSON roundtrips:

```text
cross_hierarchy_constraints[]
cross_hierarchy_joints[]
```

Regenerate graph connectivity, hierarchy traversal, parent chains, flattened leaves, target geometry, transform authorities, incidence/mappings, solve/motion groups, holding drives, residuals/Jacobians, solve/motion results, freshness snapshots, proposals, diagnostics, exchange identities/graphs/names, part shape definitions, XDE labels/references, STEP entities, posed shapes, contact classifications, sample Project copies, and sampled sweep products.

## Next technical step

Implement Block 31 only from `docs/assembly-general-geometric-target-roadmap.md`: typed geometric target taxonomy and capability projection.

Preserve all current semantic target strings and Mate/Distance/Angle/Concentric/Insert/Revolute numeric behavior. Do not add DatumAxis persistence, reference-geometry JSON, generic relationship families, or richer joint families in Block 31.

# MVP Plan

This document is the implementation-sequence source of truth. Feature-specific documents remain canonical for exact contracts, formulas, persistence details, failure policies, ordering, and focused proofs.

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

Do not persist Geometry query/execution products or introduce a second transform, hierarchy, pose, or numeric authority.

## MVP 1 — Single-part modeling

Canonical document: `docs/mvp-1-specification.md`.

Implemented PartDocument model intent, typed parameters/quantities, datum planes, sketches/profiles, additive/subtractive extrudes, dependency/invalidation/recompute planning, optional OCCT execution, STEP export, and JSON serialization.

## MVP 2 — Semantic references and richer sketch workflows

Implemented workplane resolution, construction geometry, composite/automatic profile regions, projected/reference-driven sketch geometry, semantic references/recovery, sketch constraints/dimensions/diagnostics/repair helpers, arcs/trim/extend, extrude direction, splines, and tangent-continuity seeds.

## MVP 3 — Parametric bolt circle pattern

Canonical document: `docs/bolt-circle-pattern-mvp3.md`.

Implemented count parameters, `CircularHolePattern` intent, dependency edges, per-hole cut expansion, incremental recompute, JSON roundtrip, and headless execution.

## MVP 4 — Assembly parameters and Project container

Canonical documents:

- `docs/assembly-parameters-mvp4.md`
- `docs/project-container-mvp4.md`

Implemented assembly-scoped parameters/bindings, one explicit root assembly, Project-owned parts, parameter propagation, per-part recompute planning, embedded Project JSON, and headless update/export flows.

## MVP 5 — Assembly relationships, motion, hierarchy, analysis, exchange, and general target architecture

### Blocks 1–8 — Local placement, solving, application, diagnostics

Implemented persistent component placement/state, local geometric relationship intent/connectivity, semantic target resolution, canonical rigid transforms/residuals, direct component-transform variable ownership, shared numeric solving, exact PartDocument freshness, atomic application, and Jacobian-rank/nullity diagnostics.

Canonical documents include:

- `docs/component-instance-mvp5.md`
- `docs/assembly-constraint-model-intent-mvp5.md`
- `docs/assembly-constraint-graph-mvp5.md`
- `docs/assembly-constraint-target-resolution-mvp5.md`
- `docs/assembly-rigid-transform-evaluation-mvp5.md`
- `docs/assembly-planar-constraint-equations-mvp5.md`
- `docs/assembly-rigid-body-solver-mvp5.md`
- `docs/assembly-solve-diagnostics-mvp5.md`

### Blocks 9–13 — Concentric, Insert, and Angle

Implemented current `.axis` and `.seat` semantic sources, Concentric/Insert residuals, shared numeric integration, and planar Angle cosine alignment.

### Block 14 — Suppression-aware solving

Canonical document: `docs/assembly-suppressed-component-solving-mvp5.md`.

Implemented active-subgroup filtering while retaining complete stale-result context.

### Block 15 — Flattened posed STEP

Canonical document: `docs/assembly-posed-step-export-mvp5.md`.

Implemented visible-active component posing, deterministic unique referenced-part recompute, exact transform chains, derived OCCT compound export, and later shared part-definition/transform helper reuse.

### Block 16 — Local Revolute motion

Canonical document: `docs/assembly-revolute-joint-motion-mvp5.md`.

Implemented persistent local Revolute state/limits/coordinate intent, local joint graph, directed axis/seating/signed-twist drives, combined geometric/joint closure, shared numeric engine, exact PartDocument freshness, and atomic transform plus selected-coordinate application.

### Block 17 — Rigid subassembly hierarchy

Canonical document: `docs/assembly-rigid-subassembly-nested-export-mvp5.md`.

Implemented Project-owned child assemblies, rigid `SubassemblyInstance` occurrences, cycle-free hierarchy validation, deterministic rooted traversal, exact inner-to-outer transform chains, visible-active leaf flattening, repeated child occurrences, and nested posed STEP export.

### Blocks 18–20 — Interference, clearance, headless analysis

Canonical document: `docs/assembly-interference-analysis-mvp5.md`.

Implemented shared posed-leaf shape construction, deterministic unordered leaf pairs, positive-volume interference, minimum-distance clearance violations, and `blcad_analyze_assembly`.

### Block 21 — Flexible child solving

Canonical document: `docs/assembly-flexible-subassembly-solving-mvp5.md`.

Implemented exact active child occurrence selection, child-as-local-root solving, local solver/application reuse, inherited semantic PartDocument freshness, and atomic application to the shared Project-owned child `AssemblyDocument`.

### Block 22 — Cross-hierarchy target/residual semantics

Canonical document: `docs/assembly-cross-hierarchy-relationship-semantics-mvp5.md`.

Implemented occurrence-qualified endpoint identity, exact rooted occurrence resolution, local semantic target reuse, root-space transform-chain evaluation, and all five geometric residual families.

### Blocks 23–27 — Cross-hierarchy persistent intent, solve chain, freshness, diagnostics

Canonical documents:

- `docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`
- `docs/assembly-cross-hierarchy-constraint-json-mvp5.md`
- `docs/assembly-cross-hierarchy-incidence-groups-mvp5.md`
- `docs/assembly-cross-hierarchy-numeric-solver-mvp5.md`
- `docs/assembly-cross-hierarchy-application-diagnostics-mvp5.md`

Implemented:

```text
Core-owned occurrence-qualified endpoint intent
cross_hierarchy_constraints[] JSON
exact hierarchy path/reached-component validation
ComponentTransformAuthority incidence
one six-variable block per unique free authority
document-local local residuals
root-space Project-level residuals
shared central finite differences + Gauss-Newton
complete authority/relationship/path/PartDocument freshness
atomic direct-authority application
cross-hierarchy Jacobian-rank / remaining-DOF diagnostics
```

Transform authority remains:

```text
(assembly_document: DocumentId,
 local ComponentInstanceId)
```

Repeated rooted endpoints may retain different geometric context while sharing one variable/proposal authority.

### Block 28 — Project-level occurrence-qualified Revolute motion

Canonical document: `docs/assembly-cross-hierarchy-revolute-motion-mvp5.md`.

Implemented persistent `AssemblyHierarchyJoint` Revolute intent, `cross_hierarchy_joints[]`, exact rooted `.seat` endpoints, combined local/cross geometry+joint motion closure, authored-coordinate holding drives, shared numeric execution, complete freshness, and atomic authority-plus-selected-coordinate application.

### Block 29 — Structured assembly exchange

Canonical document: `docs/assembly-structured-step-products-mvp5.md`.

Implemented derived assembly occurrence, component occurrence, and part product definition identities. `AssemblyExchangeGraph` derives the root plus every rooted path prefix needed by one visible-active leaf. Structured XDE/STEP export reuses shared part definitions and exact direct hierarchy/component placements.

Block 29 adds no JSON field.

### Block 30 — Rooted contact classification and sampled Revolute sweep

Canonical document: `docs/assembly-contact-swept-motion-mvp5.md`.

Implemented exact rooted unordered component-pair identity, complete `Separated` / `Touching` / `Interfering` classification, explicit touching/overlap thresholds, and bounded inclusive sampled Revolute sweeps over existing root-local and Project cross-hierarchy motion contracts.

Every sweep sample starts from a fresh source Project copy. Block 30 is deterministic discrete sampling, not continuous collision detection.

Block 30 adds no persistent record or JSON field.

### Block 31 — Typed geometric target taxonomy and capability projection — Implemented

Canonical document: `docs/assembly-geometric-target-taxonomy-mvp5.md`.

Implemented derived source kinds:

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

Implemented geometric capabilities in canonical order:

```text
Plane
Axis
Line
Point
Circle
Cylinder
Frame
```

`AssemblyResolvedGeometricTarget` retains exact local or occurrence-qualified endpoint identity, source classification, derived source metadata, one typed descriptor variant, canonical capabilities, coordinate space, and exact transform context.

Implemented projection boundary:

```text
project_plane
project_axis
project_line
project_point
project_circle
project_cylinder
project_frame
```

Current legacy source migration remains:

```text
.top/.bottom/.right/.left/.front/.back -> GeneratedPlanarFace -> Plane
.axis -> GeneratedCylindricalFace -> Axis + Cylinder
.seat -> CircularFeatureSeat -> Plane + Axis + Frame
```

Local and hierarchy legacy resolver APIs adapt from the typed projection boundary. Existing Mate/Distance/Angle/Concentric/Insert/Revolute equation consumers retain their descriptor shapes and numeric semantics.

Focused tag:

```text
[geometry][assembly-geometric-target-taxonomy]
```

### Block 32 — Assembly-selectable reference geometry Core intent — Implemented

Canonical document: `docs/assembly-reference-geometry-intent-mvp5.md`.

Implemented first-class `DatumAxis` PartDocument intent with:

```text
Explicit               finite origin + unit direction + parameter dependencies
FromConstructionLine   owned ConstructionLineId identity only
```

Implemented canonical reference semantic-source grammar:

```text
ref:datum_plane:<encoded-id>
ref:datum_axis:<encoded-id>
ref:construction_line:<encoded-id>
ref:construction_point:<encoded-id>
```

Every id byte outside `[A-Za-z0-9_-]` is escaped as uppercase `%HH`. Parsing is canonical and fail closed. Assembly endpoints continue to persist component/occurrence identity plus the semantic-reference string.

Focused tags:

```text
[core][datum-axis]
[core][assembly-reference-target-intent]
```

### Block 33 — Reference geometry serialization and structure validation — Implemented

Canonical document: `docs/assembly-reference-geometry-intent-mvp5.md`. Save-format authority: `docs/file-format.md`.

Implemented the additive optional `datum_axes` part-document JSON array for both frozen families. Historical files without it load empty collections. Local and cross-hierarchy endpoint JSON shapes are unchanged and `ref:` semantic-reference strings roundtrip byte-for-byte. Loading resolves no reference geometry.

Focused tags:

```text
[core][datum-axis-json]
[core][assembly-reference-target-json]
```

### Block 34 — Datum/axis/line/point target resolution — Implemented

Canonical roadmap: `docs/assembly-general-geometric-target-roadmap.md`.

Implemented Geometry resolution:

```text
DatumPlane        -> Plane
DatumAxis         -> Axis + Line
ConstructionLine  -> Line
ConstructionPoint -> Point
```

The resolver reuses existing workplane, construction-line, and construction-point execution. Local targets remain component-local; hierarchy targets use the exact component-plus-parent transform chain. Canonical PartDocument snapshots remain freshness authority.

Focused tag:

```text
[geometry][assembly-reference-target-resolution]
```

### Block 35 — Stable semantic generated topology identity and recovery — Implemented

Canonical document: `docs/assembly-generated-topology-reference-mvp5.md`.

Block 35 implements Core producer-driven semantic identities for:

```text
GeneratedCylindricalFace
GeneratedLinearEdge
GeneratedCircularEdge
GeneratedVertex
```

The canonical `topo:` grammar stores semantic producer identity rather than kernel topology identity:

```text
topo:cylindrical_face:<encoded-feature-id>:<encoded-profile-id>:wall
topo:linear_edge:<encoded-feature-id>:<role>
topo:circular_edge:<encoded-feature-id>:<encoded-profile-id>:<source_rim|opposite_rim>
topo:vertex:<encoded-feature-id>:<role>
```

Supported producer matrices are deliberately finite:

```text
RectangularAdditiveExtrude
  -> 12 named linear edges, cardinality 1 each
  -> 8 named vertices, cardinality 1 each

SingleCircleSubtractiveExtrude
  -> wall, cardinality 1
  -> source_rim, cardinality 1
  -> opposite_rim, cardinality 1
```

The single-circle producer retains the exact source `CircleProfileId`. Unsupported producers, mixed/multiple-circle ambiguity, wrong profile identity, and family/role mismatches fail closed. `CircularHolePattern` subelements remain unavailable until stable per-instance semantic identity exists.

`ReferenceRecoveryEvaluator` performs read-only generated-topology recovery from current PartDocument producer intent. It reports `Resolved` or `Lost` and never writes raw OCCT ids into model intent.

Focused tags:

```text
[core][semantic-generated-topology-reference]
[core][semantic-generated-topology-recovery]
```

Block 35 adds no generated-topology Geometry resolver branch. Geometry target resolution remains Block 36.

## Blocks 36–47 — Planned general target, relationship, and joint continuation

Canonical roadmap: `docs/assembly-general-geometric-target-roadmap.md`.

Mandatory order:

```text
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

Do not merge these blocks.

## MVP 6 — Planned Part Construction MVP after Block 47

Canonical sequence: `docs/part-construction-sequence-mvp6.md`.

Block 48 starts only after the Assembly MVP reaches Block 47.

Mandatory Part Construction phase order:

```text
48-57   multi-body/result/transform authority
58-60   shared Part feature inputs and richer Extrude/Cut
61-62   Revolve / RevolveCut
63-65   general Linear/Circular Pattern
66-67   Mirror Feature
68-70   Fillet and Chamfer
71-74   Shell and Draft
75-78   3D Sketch
79      PathCurve
80-83   Sweep and path-following Extrude/Cut
84-87   Loft
88-92   Surface Features and surface-to-solid
93      multi-body STEP exchange
94      integrated Part Construction MVP acceptance
```

The sequence consolidates existing planning from:

- `docs/multi-body-transform-and-path-features-roadmap.md`
- `docs/inventor-like-sketcher-and-feature-roadmap.md`
- `docs/advanced-surfacing-and-3d-sketch-mvp.md`

After Block 94 the first Part Construction MVP is considered complete. That means a serious headless parametric Part kernel with multiple solid/surface bodies, explicit body operations, Revolve, general patterns, Mirror, Fillet, Chamfer, Shell, Draft, 3D Sketches, Sweep, Loft, first Surface Features, surface-to-solid conversion, and multi-body STEP exchange.

It does not mean SolidWorks/Inventor Part product parity. Production GUI modeling, Class-A surfacing, arbitrary NURBS control cages, variable-radius fillets, advanced topology healing, direct modeling, sheet metal, weldments, and specialized manufacturing feature systems remain later work.

## Current next technical step — Block 36

Primary boundary: Geometry semantic target resolution.

Block 36 must resolve validated Block-35 semantic generated topology identities into the Block-31 target taxonomy:

```text
GeneratedPlanarFace      -> Plane
GeneratedCylindricalFace -> Cylinder + Axis
GeneratedLinearEdge      -> Line
GeneratedCircularEdge    -> Circle + Axis + Point(center)
GeneratedVertex          -> Point
```

The resolver must parse canonical `topo:` spellings before legacy feature-role parsing, validate the producer-role identity, identify the exact current generated subelement, prove topology type and expected cardinality, and preserve component-local versus exact rooted transform semantics.

Focused tag:

```text
[geometry][assembly-generated-topology-target-resolution]
```

Target compatibility remains Block 37.

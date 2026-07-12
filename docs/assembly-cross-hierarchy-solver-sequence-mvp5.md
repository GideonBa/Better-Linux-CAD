# Cross-Hierarchy Assembly Relationship Sequence MVP-5

Status: Blocks 23-29 are implemented. Block 30 is the current next technical step.

This document is the canonical implementation sequence for assembly relationships, motion, hierarchy-aware exchange, and the next posed-analysis step after the read-only hierarchy target/residual seed.

Implemented contracts are canonical in:

- `docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`
- `docs/assembly-cross-hierarchy-constraint-json-mvp5.md`
- `docs/assembly-cross-hierarchy-incidence-groups-mvp5.md`
- `docs/assembly-cross-hierarchy-numeric-solver-mvp5.md`
- `docs/assembly-cross-hierarchy-application-diagnostics-mvp5.md`
- `docs/assembly-cross-hierarchy-revolute-motion-mvp5.md`
- `docs/assembly-structured-step-products-mvp5.md`
- `docs/file-format.md`

## Sequencing rule

Cross-document assembly work crosses separate authority boundaries:

```text
Core persistent model intent
  -> serialization and structure compatibility
  -> derived occurrence/authority connectivity
  -> root-space semantic target/equation evaluation
  -> shared numeric residual/Jacobian execution
  -> derived solve/motion results and proposals
  -> complete modeled-input freshness
  -> atomic authority-qualified application
  -> deterministic occurrence-aware exchange
  -> posed analysis consumers
```

Do not:

- persist Geometry query/execution types;
- duplicate one shared child component transform into independent variables per rooted occurrence;
- duplicate local model-definition relationships per rooted occurrence;
- add residual rows merely to encode shared transform authority;
- persist graph, residual, Jacobian, freshness snapshot, proposal, diagnostic, XDE label, or STEP entity caches;
- mutate `SubassemblyInstance::transform()` through component authority variables;
- introduce a second optimizer, finite-difference contract, Revolute residual formula, hierarchy traversal, or export transform convention.

## Frozen solve/motion identities

### Occurrence-qualified semantic endpoint

```text
AssemblyHierarchyConstraintEndpoint =
  (occurrence_path,
   local ComponentInstanceId,
   semantic_reference)
```

The empty path addresses the explicit root occurrence. Non-empty paths are exact ordered root-to-current `SubassemblyInstanceId` sequences.

### Geometric component occurrence

```text
HierarchyComponentOccurrence =
  (occurrence_path,
   local ComponentInstanceId)
```

### Direct component transform authority

```text
ComponentTransformAuthority =
  (assembly_document: DocumentId,
   local ComponentInstanceId)
```

Repeated rooted occurrences can remain different geometric/motion contexts while mapping to one shared child-document transform authority.

## Frozen Block-29 exchange identities

### Assembly occurrence

```text
AssemblyExchangeAssemblyOccurrenceIdentity =
  exact rooted SubassemblyInstance occurrence path
```

### Part component occurrence

```text
AssemblyExchangeComponentOccurrenceIdentity =
  (containing assembly occurrence path,
   local ComponentInstanceId)
```

### Part product definition

```text
AssemblyExchangePartProductDefinitionIdentity =
  referenced PartDocumentId
```

These exchange identities are derived and unpersisted. They are not aliases for OCCT topology ids, TDF label tags, or STEP entity ids.

## Blocks 23-27: cross-hierarchy geometric solve chain — implemented

### 23. Core endpoint contract and persistent Project-level intent

Canonical document: `docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`.

Implemented Core-owned occurrence-qualified endpoint identity and persistent Project-owned Mate/Concentric/Distance/Insert/Angle relationships.

### 24. Additive JSON and exact structure validation

Canonical document: `docs/assembly-cross-hierarchy-constraint-json-mvp5.md`.

Implemented additive:

```text
cross_hierarchy_constraints[]
```

with exact target/path order and exact root-to-reached-component structure validation after ordinary hierarchy validation.

### 25. Relationship-to-authority incidence and solve groups

Canonical document: `docs/assembly-cross-hierarchy-incidence-groups-mvp5.md`.

Implemented deterministic active local/Project relationship incidence over unique `ComponentTransformAuthority` values. Local relationships are collected once per containing `AssemblyDocument`; repeated rooted endpoints retain exact occurrence context while mapping to shared authorities.

### 26. Shared numeric residual/Jacobian integration

Canonical document: `docs/assembly-cross-hierarchy-numeric-solver-mvp5.md`.

Implemented one six-variable block per unique free active authority:

```text
tx_mm ty_mm tz_mm rx_deg ry_deg rz_deg
```

Local relationships evaluate once in document-local space. Project-level relationships evaluate through exact rooted parent chains in root-assembly space. Existing residual flattening, central finite differences, damping/backtracking, and Gauss-Newton machinery are reused.

### 27. Complete freshness, atomic application, and diagnostics

Canonical document: `docs/assembly-cross-hierarchy-application-diagnostics-mvp5.md`.

Implemented complete authority/relationship/path-boundary/semantic-PartDocument freshness, exact solve-group revalidation, atomic direct-authority transform application, and Jacobian-rank/remaining-DOF diagnostics.

Semantic target PartDocument freshness uses exact canonical:

```text
serialize_part_document_to_json(part)
```

payload comparison.

Focused tags:

```text
[geometry][assembly-cross-hierarchy-solver]
[geometry][assembly-cross-hierarchy-application]
[geometry][assembly-cross-hierarchy-diagnostics]
[geometry][assembly-semantic-freshness]
```

## Block 28: occurrence-qualified Revolute motion — implemented

Canonical document: `docs/assembly-cross-hierarchy-revolute-motion-mvp5.md`.

Implemented:

- persistent Project-level `AssemblyHierarchyJoint` Revolute intent;
- additive `cross_hierarchy_joints[]` JSON;
- exact occurrence-qualified `.seat` endpoints;
- combined motion closure over local geometry, local joints, Project cross geometry, and Project cross joints;
- exact endpoint-to-transform-authority mapping;
- two endpoint mappings but one unique incidence for different rooted endpoints sharing one authority;
- one shared local/cross Revolute residual constructor;
- exact root-space parent-chain target evaluation;
- authored-coordinate holding drives for non-selected active Revolute joints;
- shared authority variable order, finite differences, and numeric engine;
- complete four-family relationship snapshots;
- shared Block-27 authority/boundary/PartDocument freshness helpers;
- atomic authority transforms plus selected Project-level joint coordinate application.

Focused tags:

```text
[core][assembly-cross-hierarchy-joint]
[core][assembly-cross-hierarchy-joint-json]
[core][assembly-cross-hierarchy-motion-graph]
[geometry][assembly-cross-hierarchy-revolute]
[geometry][assembly-cross-hierarchy-motion]
```

## Block 29: component geometry instancing and structured STEP assembly products — implemented

Canonical document: `docs/assembly-structured-step-products-mvp5.md`.

### Persistent authority versus derived exchange state

Block 29 adds no persistent record and no JSON field.

Persistent authority remains existing Project/AssemblyDocument/PartDocument model intent.

Derived/unpersisted:

```text
AssemblyExchangeGraph
assembly exchange occurrence records
component exchange occurrence records
part product definition records
generated exchange names
part shape definition build products
XDE document and labels
XCAF component references
STEP assembly/product entities
export summaries
```

### Deterministic exchange graph

`AssemblyExchangeGraph` reuses:

```text
AssemblyHierarchyTraversal
AssemblyLeafOccurrenceResolver
```

The export set is:

```text
explicit root occurrence
+
every rooted assembly path prefix required by one visible-active leaf
```

This inherits the canonical posed-leaf visibility/suppression policy rather than implementing a second filter.

Ordering:

```text
assembly occurrence:
  lexicographic exact SubassemblyInstanceId path sequence

component occurrence:
  containing path
  then local ComponentInstanceId

part product definition:
  PartDocumentId
```

Component occurrences retain the exact canonical:

```text
transforms_inner_to_outer
```

leaf chain.

Each non-root assembly occurrence retains the exact direct boundary:

```text
transform_from_parent =
  hierarchy occurrence parent_transforms_inner_to_outer.front()
```

### Stable generated names

Generated exchange names are deterministic metadata, not identity authority.

Ordinary ASCII id bytes:

```text
A-Z a-z 0-9 . _ -
```

remain readable. All other UTF-8 bytes are uppercase `%HH` encoded.

Literal `/` and `%` inside authored ids cannot collide with path separators or escapes.

The explicit root path uses:

```text
root
```

A non-root one-segment path whose encoded text is literally `root` uses reserved spelling:

```text
%72oot
```

Representative names:

```text
blcad:assembly-occurrence:root
blcad:assembly-occurrence:subassembly.left/subassembly.inner
blcad:component-occurrence:subassembly.left/component.shaft
blcad:part-definition:part.plate
```

### Shared part definition/recompute authority

`AssemblyPartShapeDefinitionBuilder`:

```text
sort/deduplicate PartDocumentId
-> one private ShapeCache per unique part
-> one GeometryRecomputeExecutor call per unique part
-> require final shape
-> one unposed TopoDS_Shape definition per PartDocumentId
```

`AssemblyPosedLeafShapeBuilder` now reuses this helper.

Structured STEP reuses the same helper.

### Shared OCCT transform conversion

Block 29 extracts one conversion contract:

```text
to_occt_rigid_transform
to_occt_location
```

with existing BLCAD semantics:

```text
translation * Rz * Ry * Rx
```

The flattened posed-leaf path composes its exact leaf chain through this helper.

Structured STEP uses only direct local placements:

```text
part component location
  = leaf transforms_inner_to_outer.front()

child assembly location
  = exchange transform_from_parent
```

It does not compose a separate root-space chain.

### Structured XDE/STEP transfer

`AssemblyStructuredStepExporter` builds:

```text
one XDE part definition label per PartDocumentId
one XDE assembly label per rooted assembly occurrence
part component references to shared definitions
parent -> child assembly occurrence references
```

The explicit root assembly label is transferred through `STEPCAFControl_Writer` with names enabled and AP214 schema selection.

The existing `AssemblyStepExporter` remains a flattened compound compatibility consumer.

Headless consumer:

```text
blcad_export_structured_assembly <input.blcad.project.json> <output.step>
```

The structured command exports the current authored/persisted pose and does not implicitly invoke solving or motion.

Focused tags:

```text
[core][assembly-exchange-graph]
[geometry][assembly-structured-step-export]
```

Required proofs are implemented for repeated root parts, repeated child assembly occurrences, nested children, shared part definitions, deterministic order, exact transform-chain reuse, hidden/suppressed filtering, collision-free generated names, Project immutability, structured STEP assembly usage relationships, STEP re-import, and geometric equivalence with the flattened compatibility exporter.

## Block 30: richer collision/contact and swept-motion analysis — Next

Goal: extend the existing posed leaf/interference/clearance analysis boundary using the now-frozen rooted occurrence identities and current motion result/application contracts.

Do not implement a general physics engine.

Required planning boundary before implementation:

1. define persistent authority versus derived contact/sweep products;
2. define stable unordered or directed occurrence-pair identity using exact rooted component occurrence identity;
3. freeze static contact classification semantics beyond current positive-volume interference and scalar minimum clearance;
4. define a deterministic swept-motion query input over one selected supported Revolute motion interval;
5. state whether the sweep samples authored/current Project copies or transient solver results and how source immutability is guaranteed;
6. freeze deterministic sample ordering and failure behavior;
7. reuse `AssemblyLeafOccurrenceResolver`, shared part definitions/posed geometry, and current motion solver/application boundaries instead of introducing a second pose model;
8. define focused acceptance tags and one precise next handoff.

Likely static classifications to evaluate and freeze explicitly:

```text
Separated
Touching
Interfering
```

Any tolerance used to distinguish touching from separated must be explicit and validated.

A first swept-motion seed should remain deterministic and bounded. It may sample one requested Revolute coordinate interval at explicit caller-provided or validated fixed resolution; it must not claim continuous collision detection unless an actual continuous algorithm is implemented.

The result should identify exact rooted component occurrence pairs and the motion coordinate/sample at which a classification or clearance event was observed.

Deferred after Block 30:

- occurrence-local internal child pose overrides;
- whole-`SubassemblyInstance` grounding or solve variables;
- richer joint families;
- multi-turn joint coordinates;
- general dynamic simulation or contact response;
- arbitrary continuous collision detection.

## Persistence rule

Persist model intent.

Current persistent assembly intent includes:

```text
component placement/state
local geometric constraints
local Revolute joint/limit/coordinate records
Project-owned child assembly documents
rigid SubassemblyInstance placement/state
Project-level cross-hierarchy geometric constraints
Project-level occurrence-qualified cross-hierarchy Revolute joints
```

Project JSON fields include:

```text
cross_hierarchy_constraints[]
cross_hierarchy_joints[]
```

Regenerate connectivity, hierarchy traversal, parent chains, leaves, target geometry, transform authorities, solve/motion groups, holding drives, residuals/Jacobians, solve/motion results, freshness snapshots, proposals, diagnostics, exchange identities/graphs/names, part shape definitions, XDE labels/component references, STEP entities, posed shapes, and analysis products.

## Next technical step

Implement Block 30 only: freeze richer posed contact classification and a bounded deterministic swept-Revolute analysis contract over exact rooted component occurrence identities.

Do not add occurrence-local flexible pose overrides, whole-subassembly solve variables, richer joint families, or a general physics engine in Block 30.

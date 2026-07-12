# Cross-Hierarchy Assembly Relationship Sequence MVP-5

Status: Blocks 23-28 are implemented. Block 29 is the current next technical step.

This document is the canonical implementation sequence for assembly relationships and motion that cross `AssemblyDocument` boundaries after the read-only hierarchy target/residual seed.

Implemented contracts are canonical in:

- `docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`
- `docs/assembly-cross-hierarchy-constraint-json-mvp5.md`
- `docs/assembly-cross-hierarchy-incidence-groups-mvp5.md`
- `docs/assembly-cross-hierarchy-numeric-solver-mvp5.md`
- `docs/assembly-cross-hierarchy-application-diagnostics-mvp5.md`
- `docs/assembly-cross-hierarchy-revolute-motion-mvp5.md`
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
  -> diagnostics or exchange consumers
```

Do not:

- persist Geometry query/execution types;
- duplicate one shared child component transform into independent variables per rooted occurrence;
- duplicate local model-definition relationships per rooted occurrence;
- add residual rows merely to encode shared transform authority;
- persist graph, residual, Jacobian, freshness snapshot, proposal, or diagnostic caches;
- mutate `SubassemblyInstance::transform()` through component authority variables;
- introduce a second optimizer, finite-difference contract, or Revolute residual formula.

## Frozen identities

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

The authority identity is derived. It addresses persistent:

```text
ComponentInstance::transform()
ComponentInstance::grounding_state()
ComponentInstance::suppression_state()
ComponentInstance::referenced_part_document()
```

Repeated rooted occurrences may map to one transform authority.

## Block 23: Core endpoint and Project-level geometric intent — Implemented

Canonical document: `docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`.

Implemented Core-owned occurrence-qualified endpoints and persistent Project-owned `AssemblyHierarchyConstraint` Mate/Concentric/Distance/Insert/Angle intent.

Project-level geometric ids are independent from local document-scoped constraint ids.

## Block 24: Additive geometric JSON and structure validation — Implemented

Canonical document: `docs/assembly-cross-hierarchy-constraint-json-mvp5.md`.

Implemented:

```text
cross_hierarchy_constraints[]
```

with absent-field compatibility, exact target/path order, `mm`/`deg` quantity rules, Project-level id uniqueness, and exact path/reached-component Core validation after complete hierarchy validation.

Semantic target geometry remains unresolved during loading.

## Block 25: Relationship-to-authority incidence and geometric solve groups — Implemented

Canonical document: `docs/assembly-cross-hierarchy-incidence-groups-mvp5.md`.

Implemented derived bipartite connectivity:

```text
local geometric relationship
or Project cross geometric relationship
          |
          v
ComponentTransformAuthority
```

Local constraints are collected once per containing `AssemblyDocument`.

Cross endpoints retain complete occurrence paths in endpoint mappings even when both map to one authority.

Participation uses active relationship state and suppression. Visibility is ignored.

Only connected components containing a Project-level cross geometric relationship are exposed as cross-hierarchy geometric solve groups.

## Block 26: Shared authority-scoped numeric solving — Implemented

Canonical document: `docs/assembly-cross-hierarchy-numeric-solver-mvp5.md`.

Each unique free active authority contributes exactly:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

Local relationships are evaluated once in containing-document local space. Project cross geometric relationships are evaluated through exact occurrence paths in root space.

All five geometric families reuse the existing scalar flattening/length scaling.

The shared evaluator boundary is:

```text
absolute candidate variable vector
  -> deterministic scaled residual vector
```

One central finite-difference implementation and one `solve_numeric_variables` Gauss-Newton engine are used by local and cross-hierarchy solving.

Results remain unapplied and authority-qualified.

## Block 27: Complete freshness, atomic authority application, and diagnostics — Implemented

Canonical document: `docs/assembly-cross-hierarchy-application-diagnostics-mvp5.md`.

Implemented complete modeled-input freshness for:

```text
transform authorities
complete local/cross geometric relationship records
exact current active solve-group participation
all persistent hierarchy boundaries on participating cross paths
exact canonical model intent of participating referenced PartDocuments
```

Semantic target Part freshness uses byte-for-byte:

```text
serialize_part_document_to_json(part)
```

as a derived unpersisted snapshot payload.

Atomic application validates all inputs first, writes direct local component transforms on one Project copy, and commits only after every write succeeds.

Cross-hierarchy diagnostics reuse the exact Block-26 free-authority order, mixed residual evaluator, central finite differences, and shared matrix-rank implementation:

```text
variable_count = 6 * unique_free_active_transform_authority_count
constrained_dof = rank(J)
remaining_dof = variable_count - rank(J)
```

## Block 28: Project-level cross-hierarchy Revolute motion — Implemented

Canonical document: `docs/assembly-cross-hierarchy-revolute-motion-mvp5.md`.

### 28A. Persistent occurrence-qualified joint intent

Implemented:

```text
AssemblyHierarchyJoint
Project::cross_hierarchy_joints
```

The first family remains `AssemblyJointType::Revolute` and reuses local state/principal-limit/authored-coordinate semantics.

Cross-joint target identity is the same occurrence-qualified endpoint contract.

Project-level cross joint ids are independent from local document-scoped joint ids.

Exactly equal endpoint identities are rejected. Different rooted endpoints that map to one shared authority remain valid.

### 28B. Additive Project JSON

Implemented:

```text
cross_hierarchy_joints[]
```

with exact target A/B and occurrence-path order, `revolute`, `active|inactive`, `deg` limits/coordinate, duplicate-id rejection, absent-field compatibility, and complete exact path/reached-component structure validation.

No derived motion product is serialized.

### 28C. Combined motion connectivity

`AssemblyCrossHierarchyMotionGraph` closes transitively over:

```text
1. active local geometric relationships
2. active local joints
3. active Project-level cross geometric relationships
4. active Project-level cross joints
```

The list above is canonical relationship-kind order.

Within each kind, assembly-document/id or Project-level id keys sort lexicographically.

Every relationship maps to each unique `ComponentTransformAuthority` on which its residual depends.

Cross-joint TargetA/TargetB endpoint mappings remain separate from unique incidence.

Only connected components containing at least one Project-level cross joint are exposed as motion groups.

### 28D. Participation semantics

Local geometric relationships and local joints participate when active and both local endpoint components are active.

Project-level cross relationships/joints participate when active, every boundary on both exact endpoint paths is active, and both addressed components are active.

Visibility does not filter motion participation.

### 28E. Root-space Revolute equations

`AssemblyHierarchyRevoluteJointEquationBuilder` resolves exact occurrence-qualified `.seat` endpoints through the existing hierarchy target resolver.

Each endpoint evaluates:

```text
[T_component, T_inner_parent, ..., T_outer_parent]
```

into root space.

Local and cross-hierarchy Revolute builders call one shared residual constructor:

```text
direction_alignment          = dA - dB
axis_offset_mm               = cross(oB - oA, dA)
signed_seating_separation_mm = dot(sB - sA, nA)

reference_cosine = dot(xA, xB)
reference_sine   = dot(dA, cross(xA, xB))

twist_sine   = reference_sine*cos(target) - reference_cosine*sin(target)
twist_cosine = reference_cosine*cos(target) + reference_sine*sin(target) - 1
```

Shared scalar order is nine residual components:

```text
direction x/y/z
axis offset x/y/z scaled by length scale
seating separation scaled by length scale
twist sine
twist cosine
```

### 28F. Holding drives and numeric execution

The selected Project-level Revolute joint receives the requested in-range coordinate.

Every other active local or Project-level Revolute joint in the same motion group receives a transient holding drive at its authored coordinate.

All active geometric relationships remain residuals.

Authority variable ownership remains deduplicated. One shared free authority contributes one six-variable block even when multiple rooted endpoints expose it.

Motion reuses:

```text
read_cross_hierarchy_authority_variables
apply_cross_hierarchy_authority_variables
shared residual flattening
shared central finite differences
solve_numeric_variables
```

### 28G. Fresh result and atomic motion application

`AssemblyCrossHierarchyJointMotionResult` protects:

```text
selected joint id
source selected coordinate
requested selected coordinate
complete ordered combined relationship identities
complete authority inputs
complete local geometric records
complete local joint records
complete Project cross geometric records
complete Project cross joint records
all participating hierarchy path boundaries
exact canonical PartDocument model intent
one proposal per free authority
```

Block 27 authority/proposal and hierarchy-boundary freshness helpers are extracted and reused by geometric application and motion application.

Motion application validates the exact current motion group, selected source coordinate, requested limit validity, all snapshots, and semantic Part freshness before mutation.

It then applies authority transforms and only the selected Project-level joint coordinate on one Project copy and commits atomically.

`SubassemblyInstance::transform()` and non-selected authored joint coordinates are never written by the motion applier.

Focused tags:

```text
[core][assembly-cross-hierarchy-joint]
[core][assembly-cross-hierarchy-joint-json]
[core][assembly-cross-hierarchy-motion-graph]
[geometry][assembly-cross-hierarchy-revolute]
[geometry][assembly-cross-hierarchy-motion]
```

## Block 29: Component geometry instancing and structured STEP assembly products — Next

Goal: replace flattened-compound-only exchange semantics with a deterministic derived assembly/product occurrence model and a structured STEP assembly export consumer.

### 29A. Freeze exchange occurrence identities

Define stable derived identities for at least:

```text
assembly occurrence
  = exact rooted SubassemblyInstance occurrence path

part component occurrence
  = (containing assembly occurrence path,
     local ComponentInstanceId)

part product definition
  = referenced PartDocumentId
```

Repeated occurrences of one child assembly remain distinct assembly occurrences.

Repeated component occurrences may reference one shared part product definition without duplicating persistent `PartDocument` intent.

These identities are exchange/presentation products and are not persisted as new model authority.

### 29B. Derive deterministic structured exchange graph

Build a read-only derived structure from the validated Project hierarchy.

Requirements:

1. explicit root product/assembly occurrence;
2. exact parent-child assembly occurrence relationships;
3. exact local component occurrences per containing assembly occurrence;
4. one reusable part product definition per referenced `PartDocumentId`;
5. exact canonical posed transform chain per component occurrence;
6. deterministic lexicographic occurrence/product order and stable generated names;
7. explicit visibility/suppression export policy consistent with posed-leaf consumers;
8. no OCCT product/label object becomes persistent model intent.

### 29C. Reuse part recompute and pose authority

Reuse:

```text
AssemblyLeafOccurrenceResolver
exact inner-to-outer transform chains
one ShapeCache per unique referenced PartDocument
one recompute per unique referenced PartDocument
```

Do not reimplement hierarchy transform composition inside the STEP exporter.

### 29D. Structured STEP assembly export

Emit STEP assembly/product relationships so consumers can distinguish:

```text
root assembly product
nested assembly occurrence products
repeated child assembly occurrences
repeated component occurrences
shared part product definitions
```

The current flattened OCCT compound export may remain as a compatibility consumer, but structured export must not derive identity from unstable OCCT topology ids.

### 29E. Acceptance proofs

Required focused coverage:

- one root with two repeated part occurrences;
- repeated child assembly occurrences referencing one child document;
- nested child hierarchy;
- shared `PartDocument` definition reused by multiple component occurrences;
- exact deterministic assembly/component occurrence identity and order;
- exact posed transform chains reused from canonical leaf semantics;
- hidden/suppressed export filtering policy;
- source Project/model-intent immutability;
- STEP transfer succeeds and contains structured assembly/product relationships rather than only one anonymous flattened compound.

Choose one focused tag before implementation, for example:

```text
[geometry][assembly-structured-step-export]
```

## Block 30: Richer collision/contact and swept-motion analysis

Deferred until Block 29 freezes structured assembly occurrence/product identity.

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

Regenerate connectivity, hierarchy traversal, parent transform chains, flattened leaves, target-resolution values, root-space geometry, transform authorities, incidence/mappings, geometric solve groups, combined motion groups, holding drives, residuals/Jacobians, solve/motion results, freshness snapshots, proposals, diagnostics, shape caches, posed shapes, analysis products, and future structured exchange products.

## Next technical step

Implement Block 29 only: freeze derived exchange occurrence/product identity, derive deterministic structured assembly/component occurrence relationships from the canonical hierarchy, reuse posed-leaf transform/recompute authority, and emit structured STEP assembly/product relationships.

Do not add occurrence-local flexible pose overrides, whole-subassembly solve variables, or swept-motion contact simulation in Block 29.

# Cross-Hierarchy Geometric Constraint Solver Sequence MVP-5

Status: Blocks 23-27 are implemented. Block 28 is the current next technical step.

This document is the canonical implementation sequence for assembly relationships that cross `AssemblyDocument` boundaries after the read-only target/residual seed in `docs/assembly-cross-hierarchy-relationship-semantics-mvp5.md`.

Implemented contracts are canonical in:

- `docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`
- `docs/assembly-cross-hierarchy-constraint-json-mvp5.md`
- `docs/assembly-cross-hierarchy-incidence-groups-mvp5.md`
- `docs/assembly-cross-hierarchy-numeric-solver-mvp5.md`
- `docs/assembly-cross-hierarchy-application-diagnostics-mvp5.md`
- `docs/file-format.md`

## Sequencing rule

Cross-document assembly solving crosses separate authority boundaries:

```text
Core persistent model intent
  -> serialization and structure compatibility
  -> derived relationship-to-transform-authority connectivity
  -> mixed numeric residual/Jacobian execution
  -> derived solve results and proposals
  -> complete modeled-input freshness
  -> atomic authority-qualified application
  -> rank and remaining-DOF diagnostics
  -> cross-hierarchy motion relationships
```

Do not:

- persist Geometry query/execution types;
- duplicate one shared child component transform into independent variables per rooted occurrence;
- duplicate one local child constraint per rooted occurrence;
- add residual rows merely to encode shared transform authority;
- persist graph, solve-group, residual, Jacobian, snapshot, proposal, or diagnostic caches;
- mutate `SubassemblyInstance::transform()` through component authority variables;
- introduce a second numeric optimizer or second finite-difference contract.

## Frozen identity split

### Geometric endpoint

```text
AssemblyHierarchyConstraintEndpoint =
  (occurrence_path,
   local ComponentInstanceId,
   semantic_reference)
```

The empty path addresses the explicit root assembly occurrence.

Exact path order is identity.

### Geometric component occurrence

```text
HierarchyComponentOccurrence =
  (occurrence_path,
   local ComponentInstanceId)
```

Two repeated child occurrences are distinct geometric contexts.

### Direct component transform authority

```text
ComponentTransformAuthority =
  (assembly_document: DocumentId,
   local ComponentInstanceId)
```

The authority addresses persistent:

```text
ComponentInstance::referenced_part_document()
ComponentInstance::transform()
ComponentInstance::grounding_state()
ComponentInstance::suppression_state()
```

Repeated geometric occurrences can map to one authority.

Until occurrence-local internal pose overrides exist, both occurrences read, perturb, snapshot, and propose one shared direct component transform.

## Relationship identity

Local geometric relationship:

```text
LocalRelationship =
  (assembly_document: DocumentId,
   AssemblyConstraintId)
```

A local constraint exists once in its containing document and is evaluated once in that document's local assembly space.

Project-level cross-hierarchy geometric relationship:

```text
CrossHierarchyRelationship =
  Project-level AssemblyConstraintId
```

It preserves both exact occurrence-qualified endpoints and is evaluated in root-assembly space.

## Block 23: Core endpoint and Project-level constraint intent — Implemented

Canonical document: `docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`.

Implemented Core endpoint identity, persistent Project-owned cross-hierarchy Mate/Concentric/Distance/Insert/Angle intent, established quantity validation, separate Project-level id scope, and the Core-record to read-only Geometry-query bridge.

Focused tag:

```text
[core][assembly-cross-hierarchy-intent]
```

## Block 24: Additive JSON and Core structure validation — Implemented

Canonical document: `docs/assembly-cross-hierarchy-constraint-json-mvp5.md`.

Implemented additive `cross_hierarchy_constraints[]`, exact target/path order roundtrip, absent-field compatibility, hierarchy-first structure validation, exact endpoint path traversal from the root, and reached-component validation without semantic geometry execution.

Focused tag:

```text
[core][assembly-cross-hierarchy-json]
```

## Block 25: Relationship-to-authority incidence and active solve groups — Implemented

Canonical document: `docs/assembly-cross-hierarchy-incidence-groups-mvp5.md`.

The derived bipartite graph is:

```text
relationship node
    |
    | residual depends on
    v
ComponentTransformAuthority node
```

Implemented active local/project relationship participation, suppression-based filtering, visibility-independent solve participation, exact endpoint-to-authority mapping, unique `(relationship, authority)` incidence, separate target-A/target-B mappings, deterministic connected cross-hierarchy solve groups, and pure-local work exclusion.

Canonical order remains local relationships before cross relationships, then lexicographic relationship and authority keys.

Focused tag:

```text
[core][assembly-cross-hierarchy-graph]
```

## Block 26: Shared mixed numeric solving and unapplied proposals — Implemented

Canonical document: `docs/assembly-cross-hierarchy-numeric-solver-mvp5.md`.

Implemented exact current solve-group validation at solve start, six absolute direct-transform variables per unique free active authority, grounded fixed context, document-local local relationship evaluation, exact-path root-space cross-hierarchy evaluation, shared five-family residual flattening, one central finite-difference implementation, one damped Gauss-Newton optimizer, source immutability, and authority-qualified proposals.

Variable order is:

```text
solve_group.authorities
  -> filter to Free
  -> for each authority:
       tx_mm ty_mm tz_mm rx_deg ry_deg rz_deg
```

Repeated rooted occurrences mapping to one authority produce one six-variable block and at most one generated proposal.

Focused tag:

```text
[geometry][assembly-cross-hierarchy-solver]
```

## Block 27: Complete freshness, atomic application, and diagnostics — Implemented

Canonical document: `docs/assembly-cross-hierarchy-application-diagnostics-mvp5.md`.

### 27A. Authority freshness — Implemented

Every participating authority snapshot stores:

```text
ComponentTransformAuthority
referenced PartDocument identity
grounding
suppression
source direct RigidTransform
```

Application rejects duplicate snapshots, missing authority targets, part retargeting, grounding/suppression/transform changes, inactive snapshots, and inconsistent fixed-authority order.

Visibility remains outside solve input freshness.

Every free active authority must have exactly one proposal in exact free-authority order.

### 27B. Complete relationship freshness — Implemented

Local relationship snapshots preserve containing assembly, id, name, type, target A/B, state, and Distance/Angle value.

Project-level snapshots preserve the same complete relationship intent with exact occurrence-qualified endpoints.

Application rejects relationship removal/replacement or any name/type/state/target/value change, incomplete snapshot sets, duplicate relationship identity, and order mismatch.

The current Block-25 graph is rebuilt and the exact result relationship/authority group must still exist. New active relationships that merge into the numeric problem therefore stale the old result.

### 27C. Hierarchy-boundary freshness — Implemented

Every persistent boundary on participating cross endpoint paths is uniquely snapshotted by:

```text
(containing AssemblyDocumentId,
 SubassemblyInstanceId)
```

The snapshot protects referenced child assembly, suppression, and boundary `RigidTransform`.

At application exact endpoint paths are followed again and the canonical boundary snapshot set is rebuilt.

Boundary removal, retargeting, suppression change, transform change, or missing/extra/tampered snapshots makes the result stale.

Visibility does not invalidate a solve result.

### 27D. Semantic target-producing PartDocument freshness — Option A implemented

Every ordinary local and cross-hierarchy solve result stores exact derived:

```text
AssemblySemanticTargetPartSnapshot
  part_document
  canonical_model_intent_json
```

The payload is the exact output of:

```text
serialize_part_document_to_json(part)
```

No hash and no mutable revision counter is introduced.

Application serializes the current PartDocument again and requires byte-for-byte equality.

This conservatively protects every serialized PartDocument model-intent edit for Parts referenced by participating components/authorities.

The contract is intentionally broader than a minimal semantic-target dependency closure.

The same helper is used by ordinary local solve results and therefore by flexible-child and local Revolute motion application.

### 27E. Atomic authority-qualified application — Implemented

Public boundary:

```text
AssemblyCrossHierarchySolveResultApplier
```

Application order:

```text
Converged result only
  -> validate Project structure
  -> authority + exact free proposal freshness
  -> complete relationship freshness
  -> exact current solve-group participation
  -> exact hierarchy path-boundary freshness
  -> exact canonical PartDocument model intent
  -> copy Project
  -> write one direct local component transform per proposal authority
  -> replace source Project only after all writes succeed
```

The applier never writes composed transforms, root-space endpoint placements, occurrence-local overrides, or `SubassemblyInstance::transform()` values.

### 27F. Cross-hierarchy rank and remaining-DOF diagnostics — Implemented

Public analyzer:

```text
AssemblyCrossHierarchySolveDiagnosticsAnalyzer
```

Local and cross diagnostics now share:

```text
compute_assembly_matrix_rank
```

Cross diagnostics use exactly the Block-26 free-authority proposal order and the same authority read/apply, mixed residual evaluation, length scaling, and central finite-difference implementation as the solver.

```text
variable_count = 6 * unique_free_active_transform_authority_count
constrained_dof = rank(J)
remaining_dof = variable_count - rank(J)
```

The count is not based on geometric occurrence count.

For two repeated rooted occurrences sharing one free child authority:

```text
variable_count = 6
```

not `12`.

Focused tags:

```text
[geometry][assembly-cross-hierarchy-application]
[geometry][assembly-cross-hierarchy-diagnostics]
[geometry][assembly-semantic-freshness]
```

## Block 28: Cross-hierarchy joints and nested motion propagation — Next

Goal: extend the frozen hierarchy identity and transform-authority model from geometric constraints to persistent motion-joint intent.

The first cross-hierarchy motion family remains Revolute.

### 28A. Persistent occurrence-qualified joint endpoints

Add a Core-owned Project-level cross-hierarchy joint record whose endpoints use exact:

```text
(occurrence_path,
 local ComponentInstanceId,
 semantic_reference)
```

Reuse existing local Revolute type/state/limits/current-coordinate semantics.

Define Project-level joint id scope independently from local document-scoped joint ids.

Additive Project JSON must preserve exact target A/B and occurrence-path order.

### 28B. Joint-to-transform-authority incidence

Map each exact cross-hierarchy joint endpoint to:

```text
ComponentTransformAuthority =
  (reached assembly document,
   local ComponentInstanceId)
```

Retain complete occurrence-qualified endpoint mappings separately from unique authority incidence.

A same-authority joint can therefore have two endpoint mappings and one unique authority dependency.

### 28C. Combined geometric and motion connectivity

Motion grouping must close transitively over:

```text
active local geometric relationships
active local joints
active Project-level cross-hierarchy geometric relationships
active Project-level cross-hierarchy joints
```

The selected Project-level joint determines the initial motion component.

Every other active Revolute joint in that combined component must receive a deterministic transient holding drive at its authored coordinate unless a later motion-query contract states otherwise.

### 28D. Root-space Revolute drive evaluation

Reuse exact hierarchy endpoint resolution and parent transform chains.

Cross-hierarchy Revolute target geometry must reuse `.seat` axis/oriented-seating semantics.

Directed axis, seating, and signed twist residuals remain mathematically identical to the local Revolute seed; only endpoint depth and authority mapping differ.

Two endpoints sharing one component transform authority observe one candidate direct transform through distinct parent chains.

### 28E. Shared numeric engine and proposals

Deduplicate six-variable blocks by `ComponentTransformAuthority`.

Reuse Block-26 mixed residual evaluation, central finite differences, and `solve_numeric_variables`.

Do not introduce `SubassemblyInstance` variables.

Return at most one direct-transform proposal per free authority.

### 28F. Fresh motion result and atomic application

Cross-hierarchy motion results must protect:

```text
complete transform authority inputs
complete geometric relationship inputs in the combined motion group
complete local joint inputs in the combined group
complete Project-level cross-hierarchy joint inputs
all participating hierarchy path boundaries
exact semantic target PartDocument model intent
selected source coordinate and requested-coordinate/limit validity
```

Application must use a Project copy and commit authority transforms plus the selected Project-level joint coordinate atomically.

Block-27 freshness helpers should be generalized/reused rather than duplicated family-by-family.

Recommended focused tags:

```text
[core][assembly-cross-hierarchy-joint]
[core][assembly-cross-hierarchy-joint-json]
[core][assembly-cross-hierarchy-motion-graph]
[geometry][assembly-cross-hierarchy-revolute]
[geometry][assembly-cross-hierarchy-motion]
```

## Blocks 29+

### 29. Component geometry instancing and structured STEP assembly products

Deferred.

### 30. Richer collision/contact and swept-motion analysis

Deferred.

## Persistence rule

Persist model intent.

Current persistent assembly intent includes component placement/state, local geometric constraints, local Revolute joint/limit/coordinate records, project-owned child assemblies, rigid subassembly occurrence placement/state, and Project-owned cross-hierarchy geometric constraints with exact occurrence-qualified endpoints.

Regenerate graph connectivity, combined motion groups, hierarchy traversal, parent transform chains, flattened leaves, resolved target geometry, residual descriptors, transform authorities, incidence/mappings, solve groups, mixed residuals/Jacobians, solve/motion results, freshness snapshots, proposals, rank products, diagnostics, shape caches, posed shapes, analysis products, and STEP compounds.

Block 28 may add persistent Project-level cross-hierarchy joint intent and its authored coordinate. It must not persist derived motion connectivity or solver products.

## Next technical step

Implement Block 28 only: persistent occurrence-qualified Project-level cross-hierarchy Revolute joint intent, additive JSON, joint-to-transform-authority incidence, combined geometric/joint motion connectivity across assembly documents, root-space nested Revolute drive evaluation, shared authority-scoped numeric solving, and complete Block-27-style fresh atomic transform-plus-coordinate application.

Do not add occurrence-local child pose overrides, whole-subassembly transform variables, component geometry instancing, or swept-motion contact analysis in Block 28.

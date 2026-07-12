# Cross-Hierarchy Fresh-Result Application and Diagnostics MVP-5

Status: implemented as Block 27 of `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

This document is canonical for complete modeled-input freshness of assembly solve results, atomic application of Block-26 cross-hierarchy transform-authority proposals, and Jacobian-rank/remaining-DOF diagnostics over the exact Block-26 free-authority variable order.

## Scope

Implemented:

- complete participating `ComponentTransformAuthority` freshness;
- referenced-part identity protection on authority/component snapshots;
- complete local relationship record snapshots;
- complete Project-level cross-hierarchy relationship record snapshots;
- exact active solve-group revalidation before cross-hierarchy application;
- exact participating hierarchy path-boundary snapshots;
- visibility-independent solve freshness;
- an explicit semantic target-producing PartDocument freshness contract;
- the same semantic PartDocument freshness for ordinary local solve results;
- atomic authority-qualified cross-hierarchy direct-transform application;
- one shared matrix-rank implementation for local and cross-hierarchy diagnostics;
- cross-hierarchy diagnostics over the exact Block-26 free-authority order;
- exact reuse of Block-26 authority candidate and mixed residual evaluation semantics.

Still deferred:

- project-level cross-hierarchy joint intent;
- nested motion propagation;
- whole-`SubassemblyInstance` transform variables or grounding;
- occurrence-local internal child pose overrides;
- null-space basis vectors or per-authority free-motion directions;
- sparse rank/solve machinery;
- component geometry instancing and structured STEP product hierarchy;
- swept-motion contact analysis.

## Semantic target-producing model freshness: Option A

Block 27 deliberately chooses Option A from the sequence plan.

Every solve result now protects the model intent of every referenced `PartDocument` whose participating component/authority can supply semantic target geometry.

The snapshot is:

```text
AssemblySemanticTargetPartSnapshot
  part_document: DocumentId
  canonical_model_intent_json: string
```

The payload is the exact result of:

```text
serialize_part_document_to_json(part)
```

No hash is stored. There is therefore no hash-collision boundary.

No mutable revision counter is added to `PartDocument`.

The existing canonical PartDocument JSON serializer already covers persistent part-model intent including parameters/formulas, datum/workplane definitions, construction geometry, sketches and profile producers, semantic-reference recovery/remap intent, and feature history.

At solve-result application the current PartDocument is serialized again and compared byte-for-byte with the stored canonical payload.

Any serialized PartDocument model-intent change makes the result stale.

This is intentionally conservative. The snapshot is not a minimal semantic-target dependency closure. A serialized edit elsewhere in the same participating PartDocument can invalidate the result even when the selected target geometry would be unchanged.

The conservative contract is preferred to claiming geometry-complete freshness without a target-dependency revision model.

The snapshots are derived and unpersisted.

## Shared local solve-result freshness extension

`AssemblySolveComponentSnapshot` now also stores:

```text
referenced_part_document
```

`AssemblySolveResult` now also stores canonical semantic target PartDocument snapshots.

The ordinary `AssemblySolveResultApplier` validates:

```text
component identity
referenced PartDocument identity
grounding
suppression
source direct transform
exact participating PartDocument canonical model intent
```

before proposal validation or mutation.

Because flexible-child solving delegates to `AssemblySolveResultApplier`, the same PartDocument freshness contract automatically applies to document-scoped flexible child results.

Because `AssemblyJointMotionResultApplier` applies its embedded `AssemblySolveResult` through the same local applier, local Revolute motion also inherits the same semantic target-producing PartDocument freshness.

There is one local semantic model freshness boundary rather than separate family-specific checks.

## Cross-hierarchy authority snapshots

Every participating Block-25 authority has one snapshot:

```text
AssemblyCrossHierarchyTransformAuthoritySnapshot
  authority:
    assembly_document
    local ComponentInstanceId
  referenced_part_document
  grounding_state
  suppression_state
  source_transform
```

Application rejects:

- duplicate authority snapshots;
- missing assembly documents;
- missing local components;
- referenced-part retargeting;
- grounding changes;
- suppression changes;
- direct component-transform changes;
- inactive authority snapshots;
- a `fixed_authorities` vector that does not exactly equal the grounded snapshot subsequence.

Visibility is intentionally absent because Block 25 and Block 26 do not use visibility for solve participation or residual evaluation.

## Exact free-authority proposal coverage

A fresh converged result must contain exactly one proposal for every free active authority snapshot, in the exact free-authority order derived from the canonical authority snapshot order.

For each proposal:

```text
ComponentTransformAuthority
source direct RigidTransform
proposed direct RigidTransform
```

Application rejects:

- duplicate proposal authorities;
- proposals for missing snapshots;
- proposals for grounded or suppressed authorities;
- source-transform mismatch;
- missing free-authority proposals;
- extra proposals;
- reordered proposal-authority identity;
- proposed transforms rejected by `ComponentInstance::with_transform`.

This strengthens the Block-26 generation guarantee into a fail-closed application contract.

## Complete relationship snapshots

### Local relationship snapshot

One local snapshot stores:

```text
assembly_document
AssemblyConstraintId
name
type
target A
target B
state
optional distance_mm
optional angle_deg
```

The identity is:

```text
(assembly_document, AssemblyConstraintId)
```

The current exact local `AssemblyConstraint` record must reproduce the snapshot.

### Project-level cross-hierarchy relationship snapshot

One Project-level snapshot stores:

```text
AssemblyConstraintId
name
type
exact occurrence-qualified target A
exact occurrence-qualified target B
state
optional distance_mm
optional angle_deg
```

The current exact persistent `AssemblyHierarchyConstraint` must reproduce the snapshot.

Distance and Angle quantities are compared through their canonical family values in millimeters/degrees because `Quantity` is validated family intent rather than a public equality value type.

Relationship snapshots are stored in exact `AssemblyCrossHierarchySolveResult::relationships` order.

Application rejects:

- incomplete or extra relationship snapshots;
- duplicate relationship identities;
- snapshot identity/order mismatch;
- relationship removal;
- relationship replacement;
- name changes;
- type changes;
- state changes;
- target A/B changes;
- endpoint occurrence-path changes;
- semantic-reference changes;
- Distance value changes;
- Angle value changes.

## Exact active solve-group freshness

Record equality alone is not enough.

An additional active relationship can connect to an authority after a result is produced. That changes the numeric problem even if every original relationship record remains unchanged.

Block 27 therefore rebuilds:

```text
AssemblyCrossHierarchyConstraintGraph::build(current_project)
```

and reconstructs the result's expected group from:

```text
result.relationships
result.authority_snapshots[].authority
```

That exact `AssemblyCrossHierarchySolveGroup` must still exist in the current deterministic `solve_groups()` collection.

This rejects participation changes caused by:

- new active relationships joining the group;
- relationships becoming inactive;
- component suppression changes;
- cross-hierarchy path suppression changes;
- connectivity changes that merge or split the group.

## Hierarchy path-boundary snapshots

Every `SubassemblyInstance` boundary occurring on target A or target B of a participating Project-level cross-hierarchy relationship is snapshotted.

Boundary authority identity is:

```text
(containing AssemblyDocumentId,
 local SubassemblyInstanceId)
```

The snapshot stores:

```text
containing_assembly_document
subassembly_instance
referenced_assembly_document
suppression_state
source boundary RigidTransform
```

A boundary can appear on multiple endpoints or through repeated rooted contexts while still being one persistent direct boundary record. Snapshots are therefore unique by persistent boundary authority identity and canonically ordered by:

```text
containing assembly document id
then SubassemblyInstanceId
```

At application the exact endpoint paths from the relationship snapshots are followed again from the explicit root and the canonical boundary snapshot set is rebuilt.

Application rejects:

- missing path boundaries;
- boundary retargeting to another child document;
- suppression changes;
- boundary transform changes;
- missing snapshots;
- extra snapshots;
- duplicate/tampered snapshot identity.

Visibility is not stored and does not invalidate a solve result.

No boundary transform is ever a component solve proposal.

## Atomic cross-hierarchy application

Public mutation boundary:

```text
AssemblyCrossHierarchySolveResultApplier
```

Application order is:

```text
require Converged
  -> validate current Project structure
  -> validate authority + proposal freshness
  -> validate complete relationship freshness
  -> validate exact current active solve group
  -> validate exact hierarchy boundary freshness
  -> validate exact semantic target PartDocument model intent
  -> copy Project
  -> write each proposed direct component transform to its authority
  -> replace source Project only after every write succeeds
```

A proposal writes only:

```text
project.find_assembly_document(authority.assembly_document)
  -> set_component_instance_transform(authority.component_instance, proposed_transform)
```

The applier never writes:

```text
composed hierarchy transforms
root-space endpoint placements
SubassemblyInstance::transform()
occurrence-local pose overrides
```

The source Project remains unchanged on every validation or write failure.

## Shared matrix-rank implementation

The previous local diagnostics matrix-rank elimination has been moved into one internal utility:

```text
compute_assembly_matrix_rank
```

It retains the established deterministic partial-pivot elimination semantics.

For Jacobian `J`:

```text
maximum_abs_entry = max(abs(J[i,j]))
pivot_threshold = max(rank_absolute_tolerance,
                      rank_relative_tolerance * maximum_abs_entry)
```

Entries must be finite and every row width must match the expected variable count.

Local and cross-hierarchy diagnostics use the same rank implementation.

## Cross-hierarchy diagnostics variable order

Public read-only analyzer:

```text
AssemblyCrossHierarchySolveDiagnosticsAnalyzer
```

The analyzer first calls the Block-26 solver on one exact current solve group.

Free authority order is taken directly from the solve result proposal order.

Because Block 26 creates proposals in canonical `variable_authorities` order, diagnostics use exactly:

```text
solve_group.authorities
  -> filter Grounded
  -> retain Free in original authority order
```

Each free authority contributes:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

Thus:

```text
variable_count = 6 * unique_free_active_transform_authority_count
```

The count is never based on rooted geometric occurrence count.

For repeated occurrences:

```text
([left],  component.child)
([right], component.child)

both -> (assembly.child, component.child)
```

one free shared authority means:

```text
variable_count = 6
```

not `12`.

## Diagnostics residual and Jacobian identity

For a converged result the analyzer:

1. copies the source Project;
2. applies the fresh Block-26 result to that copy through `AssemblyCrossHierarchySolveResultApplier`;
3. reads the solved free-authority vector through `read_cross_hierarchy_authority_variables`;
4. evaluates the exact mixed residual vector through `evaluate_cross_hierarchy_group_residuals`;
5. constructs the finite-difference evaluator by applying candidate authority vectors through `apply_cross_hierarchy_authority_variables` to Project copies;
6. evaluates the same mixed residual function;
7. calls the shared central finite-difference Jacobian builder;
8. computes rank through the shared matrix-rank utility.

The solver and diagnostics therefore share:

```text
authority resolution
six-variable direct-transform layout
authority candidate application
local relationship local-space evaluation
cross-hierarchy root-space evaluation
relationship ordering
residual family flattening
length scaling
central finite differences
```

Diagnostics do not rebuild a second residual/Jacobian contract.

## Rank and DOF semantics

For converged results:

```text
constrained_dof = rank(J)
remaining_dof = variable_count - rank(J)
residual_row_redundancy = residual_component_count - rank(J)
```

The existing classifications are reused:

```text
NoVariableDof
Underconstrained
LocallyFullyConstrained
```

`LocallyFullyConstrained` means full column rank in the local Jacobian linearization around the solved pose. It does not mean the relationships belong to one local AssemblyDocument.

Residual-rank classification remains:

```text
FullRowRank
RedundantResidualComponents
```

A regular one-free-authority planar Distance example has:

```text
residual components = 4
variables = 6
rank(J) = 3
constrained DOF = 3
remaining DOF = 3
residual row redundancy = 1
```

The rank is computed, not family-hard-coded.

## Fixed and non-converged diagnostics

For `FixedGeometryInconsistent`:

```text
consistency = FixedGeometryInconsistent
DOF classification = NotEvaluated
rank_evaluated = false
```

For another non-converged solve state:

```text
consistency = SolverDidNotConverge
DOF classification = NotEvaluated
rank_evaluated = false
```

For a converged all-grounded satisfied group:

```text
variable_count = 0
rank = 0
remaining_dof = 0
DOF classification = NoVariableDof
```

## Focused coverage

Application/freshness:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-application]"
```

Diagnostics:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-diagnostics]"
```

Shared semantic PartDocument freshness:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-semantic-freshness]"
```

The suites prove:

- fresh cross-hierarchy result application;
- exactly one direct child authority write;
- unchanged rigid subassembly boundary transforms;
- visibility changes do not stale a result;
- authority transform, grounding, suppression, and referenced-part identity are protected;
- complete relationship records are protected;
- new active relationship participation stales an old result;
- hierarchy boundary transform and suppression are protected;
- exact canonical PartDocument model-intent edits stale cross-hierarchy results;
- ordinary local solve results use the same semantic PartDocument freshness;
- duplicate/missing/tampered authority, relationship, boundary, PartDocument, and proposal result data fails closed;
- invalid proposed transforms fail before source mutation;
- one planar Distance authority has six variables and rank three;
- repeated rooted occurrences sharing one authority still have six variables, not twelve;
- mixed local/cross relationship order is retained in diagnostics;
- all-grounded consistent and inconsistent states preserve established diagnostics semantics;
- rank option validation is shared.

## Persistence boundary

Block 27 adds no JSON field.

Persistent model intent remains unchanged.

After explicit successful application the only cross-hierarchy solve mutation is an already-existing direct:

```text
ComponentInstance::transform()
```

The following remain derived and unpersisted:

```text
AssemblySemanticTargetPartSnapshot
canonical PartDocument freshness payloads
cross-hierarchy authority snapshots
local/cross relationship input snapshots
hierarchy boundary snapshots
cross-hierarchy proposals
freshness validation products
mixed residual vectors
finite-difference Jacobians
matrix-rank products
cross-hierarchy diagnostics
```

## Next technical step

Implement Block 28 only:

```text
persistent project-level cross-hierarchy joint intent
  -> exact occurrence-qualified joint endpoints
  -> joint-to-ComponentTransformAuthority incidence
  -> combined geometric + joint motion connectivity across assembly documents
  -> nested Revolute motion propagation through exact parent transform chains
  -> shared numeric engine and authority-scoped proposals
  -> complete fresh-result snapshots and atomic application
```

Reuse the frozen endpoint/occurrence/transform-authority identity split and the Block-27 freshness/application boundaries.

Do not add occurrence-local child pose overrides, whole-subassembly transform variables, component geometry instancing, or swept-motion contact analysis in Block 28.

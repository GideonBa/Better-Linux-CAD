# Cross-Hierarchy Fresh-Result Application and Diagnostics MVP-5

Status: implemented as Block 27. Block 28 now reuses the same authority/proposal, hierarchy-boundary, and semantic PartDocument freshness boundaries for Project-level cross-hierarchy Revolute motion.

This document is canonical for modeled-input freshness, atomic authority-qualified geometric solve-result application, and cross-hierarchy Jacobian-rank/remaining-DOF diagnostics. Block 39 reuses these boundaries unchanged for Coincident, Parallel, and Perpendicular solve results.

Cross-hierarchy motion follow-up is canonical in `docs/assembly-cross-hierarchy-revolute-motion-mvp5.md`.

## Scope

Implemented for cross-hierarchy geometric solve results:

```text
complete ComponentTransformAuthority freshness
referenced PartDocument identity protection
complete local relationship record snapshots
complete Project-level cross geometric relationship snapshots
exact current active geometric solve-group revalidation
all persistent hierarchy boundaries on participating cross paths
visibility-independent solve freshness
exact semantic target-producing PartDocument model-intent freshness
one proposal per free authority
atomic direct-transform authority application
shared matrix-rank diagnostics
```

The same exact semantic PartDocument freshness is also used by ordinary local solve results, flexible-child application, and local Revolute motion.

Block 28 extracts the common authority/proposal and hierarchy-boundary freshness helpers so cross-hierarchy geometric application and cross-hierarchy motion application use one implementation.

## Authority freshness

One `AssemblyCrossHierarchyTransformAuthoritySnapshot` protects:

```text
ComponentTransformAuthority
referenced PartDocumentId
grounding state
component suppression state
source direct RigidTransform
```

`ComponentTransformAuthority` remains:

```text
(assembly_document: DocumentId,
 local ComponentInstanceId)
```

Application rejects duplicate authority snapshots.

Every snapshotted assembly document and component must still exist. Referenced part, grounding, suppression, and direct transform must match exactly.

Participating authority snapshots must remain active.

The exact grounded authority subsequence must equal `fixed_authorities`.

The exact free authority subsequence defines required proposal order.

## Proposal freshness

One proposal stores:

```text
ComponentTransformAuthority
source direct transform
proposed direct transform
```

Application requires:

```text
one proposal per free active authority snapshot
no proposal for grounded authorities
no duplicate authority proposals
proposal authority order == exact free authority subsequence
proposal source transform == matching snapshot source transform
proposed transform valid through ComponentInstance::with_transform
```

The count is authority-based, not rooted-occurrence-based.

Repeated rooted endpoints that map to one authority yield at most one proposal.

## Geometric relationship freshness

Local geometric relationship identity:

```text
(assembly_document: DocumentId,
 AssemblyConstraintId)
```

The complete local `AssemblyConstraint` input is snapshotted:

```text
id
name
type
target A
target B
state
Distance value
Angle value
```

Project-level cross geometric snapshots protect complete `AssemblyHierarchyConstraint` records, including both exact occurrence-qualified endpoints.

Result relationship snapshots must exactly match the selected relationship identity count and order and contain no duplicate relationship identity.

At application each current persistent relationship record is reconstructed into the same snapshot value and compared exactly.

Removal, replacement, state, type, target, occurrence path, semantic reference, explicit quantity, or name changes stale the result.

## Exact active solve-group freshness

After authority and relationship freshness, application rebuilds:

```text
AssemblyCrossHierarchyConstraintGraph::build(project)
```

It reconstructs:

```text
AssemblyCrossHierarchySolveGroup{
  result.relationships,
  current_authorities
}
```

and requires that exact group to remain in current `solve_groups()`.

Therefore graph participation is solve input. Block 39 enables Coincident/Parallel/Perpendicular graph participation, so adding, removing, activating, or deactivating those relationships affects freshness through this same exact-group rule.

Examples that stale an old result:

```text
new active relationship joins one authority
suppression removes one endpoint path
participation merges two groups
participation splits one group
relationship becomes inactive
```

Visibility is not solve participation and does not stale the result.

## Hierarchy-boundary freshness

Every persistent `SubassemblyInstance` boundary on a participating Project-level cross endpoint path is protected.

Boundary identity:

```text
(containing AssemblyDocumentId,
 local SubassemblyInstanceId)
```

Snapshot:

```text
containing assembly document
subassembly instance id
referenced child assembly document
suppression state
source direct boundary RigidTransform
```

Snapshots are deduplicated by boundary identity and sorted by containing assembly document id, then local occurrence id.

Current boundary snapshots are regenerated from the exact persistent endpoints and compared exactly.

Boundary removal, child retargeting, suppression, or direct transform changes stale the result.

Visibility is deliberately absent from the boundary freshness snapshot.

## Exact semantic target-producing PartDocument freshness

The implemented freshness contract deliberately chose the strong exact-model-intent option.

One derived snapshot stores:

```text
AssemblySemanticTargetPartSnapshot
  part_document: DocumentId
  canonical_model_intent_json: string
```

The payload is exactly:

```text
serialize_part_document_to_json(part)
```

At application the current PartDocument is serialized again and compared byte-for-byte.

This protects the complete currently persisted PartDocument model intent, including parameters/formulas, datum/workplane intent, construction geometry, sketches/profiles, semantic-reference recovery/remap intent, and feature history.

The contract is conservative: a serialized edit in a participating referenced part may stale a result even when one specific semantic target would not change.

This avoids a false geometry-complete freshness claim without a general target dependency/revision graph.

No hash and no mutable revision counter are used.

The payload remains derived and is never serialized as additional Project state.

## Shared cross-hierarchy freshness helpers

Block 28 extracts the following common internal boundary:

```text
make_cross_hierarchy_authority_snapshots
semantic_part_documents_from_authority_snapshots
make_cross_hierarchy_boundary_snapshots
validate_cross_hierarchy_authority_and_proposal_freshness
validate_cross_hierarchy_boundary_freshness
apply_cross_hierarchy_authority_proposals
```

The Block-27 geometric applier and Block-28 cross-hierarchy motion applier both use these functions.

This freezes one implementation for:

```text
complete authority snapshots
exact fixed/free authority subsequences
one proposal per free authority
proposed transform validation
hierarchy path-boundary snapshot identity/order
hierarchy boundary freshness
atomic authority proposal writes on a supplied Project copy
```

Motion adds its own complete local/cross geometric/joint relationship snapshots and selected-coordinate semantics, but does not duplicate transform/boundary freshness.

## Atomic geometric application

`AssemblyCrossHierarchySolveResultApplier` performs:

```text
require Converged
  -> validate complete Project structure
  -> validate authority/proposal freshness
  -> validate complete geometric relationship freshness
  -> validate exact current geometric solve group
  -> validate hierarchy path boundaries
  -> validate exact semantic PartDocument model intent
  -> copy Project
  -> apply direct authority transform proposals
  -> replace source Project
```

The applier never writes:

```text
composed hierarchy transforms
root-space endpoint placements
SubassemblyInstance::transform()
occurrence-local pose overrides
```

The source Project remains unchanged on every validation or candidate-write failure.

## Shared matrix-rank implementation

Local and cross-hierarchy diagnostics, including Block-39 generic relationship diagnostics, use:

```text
compute_assembly_matrix_rank
```

For finite Jacobian `J`:

```text
maximum_abs_entry = max(abs(J[i,j]))
pivot_threshold = max(
  rank_absolute_tolerance,
  rank_relative_tolerance * maximum_abs_entry
)
```

Deterministic partial-pivot row-echelon elimination computes rank.

Solver damping is excluded from rank because damping is numerical stabilization, not a geometric constrained direction.

## Cross-hierarchy diagnostics variable order

`AssemblyCrossHierarchySolveDiagnosticsAnalyzer` first obtains one Block-26 solve result.

The free-authority order is the proposal order, which is exactly the canonical solve-group authority list filtered to `Free`:

```text
solve_group.authorities
  -> retain free active authorities in original order
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

Therefore:

```text
variable_count = 6 * unique_free_active_transform_authority_count
```

Repeated rooted occurrences that share one authority contribute six variables, not twelve.

## Diagnostics residual and Jacobian identity

For a converged result the analyzer:

1. copies the source Project;
2. applies the fresh Block-26 result to that copy;
3. reads the solved free-authority vector through the shared authority helper;
4. evaluates the exact mixed local/root-space geometric residual vector;
5. applies candidate authority vectors to Project copies;
6. calls the shared central finite-difference Jacobian builder;
7. computes rank through the shared matrix-rank utility.

Solver and diagnostics share authority resolution, six-variable layout, candidate application, local relationship evaluation, Project cross root-space evaluation, relationship order, scalar flattening, length scaling, and central finite differences.

Diagnostics do not own a second residual/Jacobian contract.

## Rank and DOF semantics

For converged results:

```text
constrained_dof = rank(J)
remaining_dof = variable_count - rank(J)
residual_row_redundancy = residual_component_count - rank(J)
```

DOF classifications remain:

```text
NoVariableDof
Underconstrained
LocallyFullyConstrained
```

Residual rank classifications remain:

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

Rank is computed from the actual shared Jacobian and is not hard-coded by relationship family.

For `FixedGeometryInconsistent` or another non-converged solve state, rank is not evaluated and DOF classification is `NotEvaluated`.

A converged all-grounded satisfied group has zero variables, rank zero, remaining DOF zero, and `NoVariableDof`.

## Block-28 cross-hierarchy motion reuse

Implemented follow-up:

```text
AssemblyHierarchyJoint
cross_hierarchy_joints[]
AssemblyCrossHierarchyMotionGraph
AssemblyHierarchyRevoluteJointEquationBuilder
AssemblyCrossHierarchyJointMotionSolver
AssemblyCrossHierarchyJointMotionResultApplier
```

The Block-28 motion result protects complete four-family relationship records:

```text
local geometry
local joints
Project cross geometry
Project cross joints
```

It also protects exact current combined motion-group participation and selected source/request coordinate semantics.

Authority/proposal freshness, hierarchy path-boundary freshness, semantic PartDocument freshness, and atomic direct-authority writes reuse Block-27 boundaries exactly.

Motion application then updates only the selected Project-level joint authored coordinate after authority proposals succeed on the same Project copy.

## Focused coverage

Block-27 application/freshness:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-application]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-semantic-freshness]"
```

Block-27 diagnostics:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-diagnostics]"
```

Block-28 shared-freshness follow-up:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-motion]"
```

Coverage proves fresh atomic application, unchanged rigid boundaries, visibility-independent freshness, complete authority/relationship/path/PartDocument protection, current-group participation freshness, invalid proposal rejection, authority-based variable count/rank/nullity, repeated-occurrence shared-authority semantics, and Block-28 reuse of the same authority/boundary/semantic freshness boundary.

## Persistence boundary

Block 27 adds no persistent field.

After explicit geometric solve application the only persistent mutation is an existing direct:

```text
ComponentInstance::transform()
```

Derived products include semantic PartDocument snapshots, authority snapshots, local/cross geometric relationship snapshots, hierarchy-boundary snapshots, proposals, residual/Jacobian values, rank products, and diagnostics.

Block 28 adds persistent Project-level Revolute joint intent separately; its snapshots and motion results remain derived.

## Current handoff

Block 28 is implemented and reuses the Block-27 freshness/application boundary.

Next is Block 29 only from `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`: freeze derived assembly/component/product occurrence identities and emit deterministic structured STEP assembly/product relationships while reusing canonical posed-leaf transform chains and one recomputed shape/cache per unique referenced PartDocument.

Occurrence-local child pose overrides, whole-subassembly solve variables, and swept-motion contact simulation remain deferred.

# Cross-Hierarchy Relationship-to-Authority Incidence and Solve Groups MVP-5

Status: implemented as Block 25. Block 26 numeric solving is implemented; Block 27 is next.

This document is canonical for the Core-derived active relationship-to-`ComponentTransformAuthority` incidence model and deterministic cross-hierarchy solve groups.

## Purpose

A pure occurrence graph preserves rooted geometry identity but loses persistent mutation authority.

A pure transform-authority graph preserves mutation ownership but loses occurrence-specific endpoint geometry.

Block 25 therefore derives a bipartite graph:

```text
relationship node
    |
    | residual depends on
    v
ComponentTransformAuthority node
```

The graph is connectivity and participation semantics only. It does not resolve semantic target geometry or execute numeric solving.

## Transform-authority identity

The derived authority identity is:

```text
ComponentTransformAuthority =
  (assembly_document: DocumentId,
   local ComponentInstanceId)
```

It identifies the persistent component record whose direct transform, grounding state, and suppression state are solve inputs.

The authority identity itself is not a separately persisted model record.

Repeated rooted occurrences can map to one authority:

```text
([left],  component.shaft)
([right], component.shaft)

both ->
(assembly.gearbox, component.shaft)
```

## Relationship identities

Local relationship identity is:

```text
AssemblyLocalRelationshipIdentity =
  (assembly_document: DocumentId,
   AssemblyConstraintId)
```

A local relationship is collected once from its containing `AssemblyDocument`.

If the child document is instantiated multiple times, the local relationship is not duplicated per rooted occurrence.

Project-level cross-hierarchy relationship identity is:

```text
AssemblyProjectCrossHierarchyRelationshipIdentity =
  Project-level AssemblyConstraintId
```

The public `AssemblyRelationshipIdentity` is a variant of these two relationship identities.

## Participation rules

Complete Project structure validates before graph derivation.

### Local relationship participation

A local relationship participates when:

```text
constraint state == Active
AND target A component suppression == Active
AND target B component suppression == Active
```

Visibility does not filter participation.

### Cross-hierarchy relationship participation

A Project-level cross-hierarchy relationship participates when:

```text
constraint state == Active
AND every SubassemblyInstance on target A occurrence path is Active
AND target A addressed component is Active
AND every SubassemblyInstance on target B occurrence path is Active
AND target B addressed component is Active
```

Visibility does not filter participation.

Suppression is the solve-participation boundary.

## Endpoint-to-authority mapping

Each participating cross-hierarchy endpoint is resolved through its exact occurrence path to the assembly document containing its addressed local component.

The endpoint then maps to:

```text
(reached assembly document id,
 local ComponentInstanceId)
```

The complete occurrence-qualified endpoint remains available in:

```text
AssemblyCrossHierarchyEndpointAuthorityMapping
```

A mapping stores:

```text
Project-level cross-hierarchy constraint id
TargetA | TargetB
complete AssemblyHierarchyConstraintEndpoint
ComponentTransformAuthority
```

## Unique incidence versus endpoint context

Incidence is unique per:

```text
(AssemblyRelationshipIdentity,
 ComponentTransformAuthority)
```

Suppose:

```text
Target A = ([left],  component.shaft, feature.left.axis)
Target B = ([right], component.shaft, feature.right.axis)
```

and both endpoints map to:

```text
(assembly.gearbox, component.shaft)
```

The relationship has:

```text
1 unique relationship-to-authority incidence
2 endpoint-to-authority mappings
```

This avoids a fake duplicate graph edge while preserving both rooted geometry contexts.

No synthetic residual row is introduced to express shared authority.

## Local relationship collection

All owned assembly documents are considered in deterministic assembly-document-id order.

For each active local constraint with active endpoint components:

1. derive one `AssemblyLocalRelationshipIdentity`;
2. derive target A and target B transform authorities from the containing assembly document and local component ids;
3. add each unique authority;
4. add one unique incidence per relationship/authority dependency.

A local child constraint exists once even if its child document has multiple rooted occurrences.

## Cross-hierarchy relationship collection

Project-level cross-hierarchy constraints are considered through persistent Project intent.

For each participating relationship:

1. resolve target A exact occurrence path;
2. resolve target B exact occurrence path;
3. preserve both endpoint identities;
4. map each endpoint to its reached document/component authority;
5. add the project-level relationship identity;
6. add each unique authority;
7. add one unique incidence per relationship/authority dependency;
8. retain both endpoint mappings separately.

## Deterministic ordering

Canonical order is:

```text
relationship kind:
  local before cross-hierarchy

local relationship:
  assembly document id
  AssemblyConstraintId

cross-hierarchy relationship:
  Project-level AssemblyConstraintId

authority:
  assembly document id
  ComponentInstanceId

incidence:
  relationship key
  authority key

endpoint mapping:
  cross-hierarchy constraint id
  TargetA before TargetB
```

All textual keys sort lexicographically ascending.

Ordering is independent of persistent insertion order.

## Connected solve groups

Connected components are derived over both relationship and authority nodes.

One connected component may contain:

```text
local relationship
  -> shared child authority
  -> Project-level cross relationship
  -> root authority
```

Such local and cross relationships belong to one numeric solve group because their residuals depend on overlapping transform authorities.

`AssemblyCrossHierarchySolveGroup` stores:

```text
ordered AssemblyRelationshipIdentity values
ordered unique ComponentTransformAuthority values
```

Only connected components containing at least one active Project-level cross-hierarchy relationship are returned through `solve_groups()`.

Pure-local connected components remain ordinary local solver work.

Solve-group order is determined by the first canonical relationship key, with the first canonical authority key as fallback.

## Persistence boundary

Persistent inputs remain:

```text
AssemblyDocument local AssemblyConstraint records
Project-owned AssemblyHierarchyConstraint records
ComponentInstance transform/state
SubassemblyInstance path boundary placement/state
```

Derived and unpersisted Block-25 data is:

```text
ComponentTransformAuthority values
AssemblyRelationshipIdentity values
relationship-to-authority incidences
cross-hierarchy endpoint-to-authority mappings
connected cross-hierarchy solve groups
```

None is emitted to Project JSON.

## Failure policy

Graph construction fails closed when complete Project structure is invalid.

After successful structure validation, derivation also fails if an expected local target component, cross-hierarchy occurrence path, reached assembly document, or addressed endpoint component cannot be reproduced.

Unsupported semantic feature strings are not a Block-25 graph error because this layer does not evaluate semantic geometry.

## Focused coverage

Test file:

```text
tests/core/assembly_cross_hierarchy_constraint_graph_tests.cpp
```

Focused tag:

```text
[core][assembly-cross-hierarchy-graph]
```

The suite proves root/child and nested authority mapping, repeated occurrence path preservation, shared authority deduplication, same-authority one-incidence/two-mapping semantics, one child-local relationship despite repeated child occurrences, mixed local/cross connectivity through shared authority, path and component suppression, visibility independence, inactive relationship exclusion, pure-local group filtering, insertion-order independence, source transform immutability, and fail-closed invalid Project structure.

Focused command:

```bash
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-graph]"
```

## Implemented numeric follow-up

Block 26 is canonical in `docs/assembly-cross-hierarchy-numeric-solver-mvp5.md`.

`AssemblyCrossHierarchyConstraintSolver` now consumes one exact current solve group and reuses:

```text
unique authority ordering
relationship ordering
local relationship identity
Project-level cross relationship identity
exact endpoint occurrence paths
```

It adds authority-scoped variables, mixed document-local/root-space residual evaluation, shared residual flattening, shared central finite differences, and the existing Gauss-Newton engine.

A repeated rooted occurrence does not add a second variable block when it maps to an authority already present in the group.

Block-26 solve results remain derived and unapplied.

## Explicitly deferred from the connectivity layer

This Block-25 contract does not itself implement:

- solve-result freshness validation;
- atomic result application;
- cross-hierarchy rank/remaining-DOF diagnostics;
- semantic target-producing PartDocument revision tracking;
- cross-hierarchy joints or nested motion propagation;
- occurrence-local internal pose overrides;
- whole-subassembly variables.

## Current handoff

The sequence source of truth is `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

Next is Block 27 only:

```text
Block-26 authority snapshots + proposals
  -> complete authority freshness
  -> complete local/cross relationship freshness
  -> participating hierarchy-boundary freshness
  -> explicit semantic target-geometry freshness contract
  -> atomic authority-qualified direct-transform application
  -> exact-authority-order rank / remaining-DOF diagnostics
```

Do not add cross-hierarchy joints, nested motion propagation, occurrence-local internal pose overrides, or whole-subassembly solve variables in Block 27.

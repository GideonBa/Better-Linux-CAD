# Cross-Hierarchy Relationship-to-Authority Incidence and Solve Groups MVP-5

Status: implemented as Block 25. Blocks 26 and 27 are implemented follow-ups; Block 28 is next.

This document is canonical for the Core-derived active relationship-to-`ComponentTransformAuthority` incidence model and deterministic cross-hierarchy geometric solve groups.

## Purpose

A pure occurrence graph preserves rooted geometry identity but loses persistent mutation authority.

A pure transform-authority graph preserves mutation ownership but loses occurrence-specific endpoint geometry.

Block 25 therefore derives:

```text
relationship node
    |
    | residual depends on
    v
ComponentTransformAuthority node
```

This layer owns connectivity and participation only. It does not resolve semantic target geometry or execute numeric solving.

## Transform-authority identity

```text
ComponentTransformAuthority =
  (assembly_document: DocumentId,
   local ComponentInstanceId)
```

It identifies the persistent component record whose direct transform, grounding, suppression, and referenced-part identity are solve inputs.

The authority identity itself is derived and unpersisted.

Repeated rooted occurrences can map to one authority:

```text
([left],  component.shaft)
([right], component.shaft)

both ->
(assembly.gearbox, component.shaft)
```

## Relationship identities

Local relationship:

```text
AssemblyLocalRelationshipIdentity =
  (assembly_document: DocumentId,
   AssemblyConstraintId)
```

A local relationship is collected once from its containing `AssemblyDocument`, not once per rooted occurrence.

Project-level cross-hierarchy relationship:

```text
AssemblyProjectCrossHierarchyRelationshipIdentity =
  Project-level AssemblyConstraintId
```

`AssemblyRelationshipIdentity` is the variant of these two identities.

## Participation rules

Complete Project structure validates before graph derivation.

A local relationship participates when its constraint is Active and both local target components are Active.

A Project-level cross relationship participates when the relationship is Active, every `SubassemblyInstance` on both exact endpoint paths is Active, and both addressed components are Active.

Visibility does not filter solve participation.

Suppression is the participation boundary.

## Endpoint-to-authority mapping

Each participating cross endpoint is resolved through its exact occurrence path to the assembly document containing the addressed local component.

It maps to:

```text
(reached assembly document id,
 local ComponentInstanceId)
```

The complete occurrence-qualified endpoint remains in:

```text
AssemblyCrossHierarchyEndpointAuthorityMapping
```

A mapping stores constraint id, TargetA/TargetB role, complete endpoint identity, and transform authority.

## Unique incidence versus endpoint context

Incidence is unique per:

```text
(AssemblyRelationshipIdentity,
 ComponentTransformAuthority)
```

For:

```text
Target A = ([left],  component.shaft, feature.left.axis)
Target B = ([right], component.shaft, feature.right.axis)
```

when both endpoints map to:

```text
(assembly.gearbox, component.shaft)
```

the relationship has one unique authority incidence but two endpoint mappings.

No synthetic residual row represents shared authority.

## Deterministic ordering

Canonical order:

```text
relationship kind:
  local before cross-hierarchy

local relationship:
  assembly document id
  constraint id

cross relationship:
  Project-level constraint id

authority:
  assembly document id
  component id

incidence:
  relationship key
  authority key

endpoint mapping:
  cross constraint id
  TargetA before TargetB
```

All textual keys sort lexicographically ascending. Persistent insertion order does not affect derived products.

## Connected solve groups

Connected components are derived across relationship and authority nodes.

A component may contain:

```text
child-local relationship
  -> shared child authority
  -> Project-level cross relationship
  -> root authority
```

These relationships belong to one numeric solve group because their residuals depend on overlapping transform authorities.

`AssemblyCrossHierarchySolveGroup` stores ordered relationship identities and ordered unique authorities.

Only connected components containing at least one active Project-level cross-hierarchy relationship appear in `solve_groups()`.

Pure-local components remain ordinary local solver work.

## Persistence boundary

Persistent inputs remain local `AssemblyConstraint` records, Project-owned `AssemblyHierarchyConstraint` records, component model state, and `SubassemblyInstance` path-boundary model state.

Derived and unpersisted Block-25 products are:

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

After successful structure validation it also fails when an expected local target component, cross-hierarchy occurrence path, reached assembly document, or addressed endpoint component cannot be reproduced.

Unsupported semantic feature strings are not graph errors because this layer does not evaluate semantic geometry.

## Focused coverage

```bash
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-graph]"
```

Coverage proves root/child/nested authority mapping, repeated path preservation, shared authority deduplication, same-authority one-incidence/two-mapping semantics, one child-local relationship despite repeated child occurrences, mixed local/cross connectivity, suppression and visibility policy, inactive relationship exclusion, pure-local filtering, insertion-order independence, source immutability, and fail-closed invalid Project structure.

## Implemented numeric and application follow-ups

Block 26 (`docs/assembly-cross-hierarchy-numeric-solver-mvp5.md`) consumes one exact current solve group, filters canonical authority order to free authorities, allocates six direct-transform variables per unique free authority, and evaluates mixed document-local/root-space residuals through the shared numeric engine.

Block 27 (`docs/assembly-cross-hierarchy-application-diagnostics-mvp5.md`) rebuilds this graph during result application and requires the exact relationship/authority group encoded by the result snapshots to still exist.

This means graph participation itself is now a freshness input. A newly active relationship joining the group, suppression change, or connectivity merge/split invalidates an old solve result even when the original relationship records are unchanged.

Cross-hierarchy diagnostics also use the canonical Block-25 authority order filtered to free authorities. Repeated rooted occurrences mapping to one authority therefore contribute one six-variable diagnostics block.

## Explicitly deferred from this connectivity layer

- cross-hierarchy joint-to-authority incidence;
- combined local/cross geometric/joint motion connectivity;
- nested motion propagation;
- occurrence-local child pose overrides;
- whole-subassembly variables.

## Current handoff

Next is Block 28 only.

Block 28 must extend the relationship/authority incidence discipline to persistent Project-level occurrence-qualified Revolute joints and derive combined motion connectivity across local geometric relationships, local joints, Project-level cross geometric relationships, and Project-level cross-hierarchy joints.

The existing geometric `AssemblyCrossHierarchyConstraintGraph` should remain authoritative for geometric solve groups unless a deliberately generalized shared relationship/authority graph abstraction is introduced without changing established ordering.

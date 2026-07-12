# Cross-Hierarchy Relationship-to-Authority Incidence and Solve Groups MVP-5

Status: implemented as Block 25. Blocks 26-28 are implemented follow-ups; Block 29 is next.

This document is canonical for the Core-derived active geometric relationship-to-`ComponentTransformAuthority` incidence model and deterministic cross-hierarchy geometric solve groups.

Block 28 adds a separate combined motion graph; it does not replace or change the geometric graph contract defined here.

## Purpose

A pure occurrence graph preserves rooted geometry identity but loses persistent mutation authority.

A pure transform-authority graph preserves mutation ownership but loses occurrence-specific endpoint geometry.

Block 25 therefore derives:

```text
geometric relationship node
    |
    | residual depends on
    v
ComponentTransformAuthority node
```

This layer owns active geometric connectivity only. It does not resolve semantic target geometry or execute numeric solving.

## Transform-authority identity

```text
ComponentTransformAuthority =
  (assembly_document: DocumentId,
   local ComponentInstanceId)
```

It identifies the persistent component record whose direct transform, grounding, suppression, and referenced-part identity are solve inputs.

The authority value itself is derived and unpersisted.

Repeated rooted occurrences can map to one authority:

```text
([left],  component.shaft)
([right], component.shaft)

both ->
(assembly.gearbox, component.shaft)
```

## Geometric relationship identities

Local relationship:

```text
AssemblyLocalRelationshipIdentity =
  (assembly_document: DocumentId,
   AssemblyConstraintId)
```

A local relationship is collected once from its containing `AssemblyDocument`, not once per rooted occurrence.

Project-level cross relationship:

```text
AssemblyProjectCrossHierarchyRelationshipIdentity =
  Project-level AssemblyConstraintId
```

`AssemblyRelationshipIdentity` is the variant of these two geometric relationship identities.

## Participation rules

Complete Project structure validates before graph derivation.

A local relationship participates when:

```text
constraint state == Active
AND target A local component suppression == Active
AND target B local component suppression == Active
```

A Project-level cross relationship participates when:

```text
constraint state == Active
AND every SubassemblyInstance on target A exact path is Active
AND target A addressed component is Active
AND every SubassemblyInstance on target B exact path is Active
AND target B addressed component is Active
```

Visibility does not filter geometric solve participation.

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

A mapping stores Project cross constraint id, TargetA/TargetB role, complete endpoint identity, and transform authority.

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

when both map to:

```text
(assembly.gearbox, component.shaft)
```

the relationship has:

```text
1 unique relationship-to-authority incidence
2 endpoint-to-authority mappings
```

No duplicate graph edge or synthetic residual row represents shared authority.

## Deterministic ordering

Canonical geometric graph order:

```text
relationship kind:
  local before Project cross geometry

local relationship:
  assembly document id
  constraint id

Project cross relationship:
  Project-level constraint id

authority:
  assembly document id
  component id

incidence:
  relationship key
  authority key

endpoint mapping:
  Project cross constraint id
  TargetA before TargetB
```

All textual keys sort lexicographically ascending. Persistent insertion order does not affect derived products.

## Connected geometric solve groups

Connected components are derived across geometric relationship and authority nodes.

A component may contain:

```text
child-local relationship
  -> shared child authority
  -> Project cross geometric relationship
  -> root authority
```

`AssemblyCrossHierarchySolveGroup` stores ordered geometric relationship identities and ordered unique authorities.

Only connected components containing at least one Project-level cross geometric relationship appear in `solve_groups()`.

Pure-local geometric components remain ordinary local solver work.

## Block-28 motion graph follow-up

`AssemblyCrossHierarchyMotionGraph` is a separate derived graph because motion closes over four relationship classes:

```text
1. local geometry
2. local joint
3. Project cross geometry
4. Project cross joint
```

The established `AssemblyCrossHierarchyConstraintGraph` remains authoritative for geometric solve groups and its ordering is unchanged.

The motion graph reuses the same `ComponentTransformAuthority` identity and same suppression/visibility policy.

Project cross-joint TargetA/TargetB endpoint mappings use the same one-incidence/two-mapping rule when two rooted endpoints map to one authority.

Only connected motion components containing a Project-level cross joint become Block-28 motion groups.

## Block-26/27 numeric and application follow-ups

Block 26 consumes one exact current geometric solve group, filters canonical authority order to free authorities, allocates six direct-transform variables per unique free authority, and evaluates mixed document-local/root-space geometric residuals through the shared numeric engine.

Block 27 rebuilds this geometric graph during application and requires the exact result relationship/authority group to still exist.

A newly active geometric relationship joining the group, suppression change, or connectivity merge/split therefore invalidates an old result.

Geometric diagnostics use the canonical authority order filtered to free authorities. Repeated rooted occurrences sharing one authority contribute one six-variable block.

Block 28 applies the same authority ownership principle to combined motion groups and reuses shared authority/proposal and boundary freshness helpers.

## Persistence boundary

Persistent inputs remain local `AssemblyConstraint` records, Project-owned `AssemblyHierarchyConstraint` records, component model state, and `SubassemblyInstance` path-boundary state.

Derived and unpersisted Block-25 products:

```text
ComponentTransformAuthority values
AssemblyRelationshipIdentity values
geometric relationship-to-authority incidences
cross-geometric endpoint-to-authority mappings
connected geometric solve groups
```

Block-28 motion graph identities/incidences/mappings/groups are also derived and are not emitted to Project JSON.

## Failure policy

Graph construction fails closed when complete Project structure is invalid.

After successful structure validation it also fails when an expected local target component, exact cross-hierarchy occurrence path, reached assembly document, or addressed endpoint component cannot be reproduced.

Unsupported semantic feature strings are not graph errors because graph derivation does not evaluate semantic geometry.

## Focused coverage

Geometric graph:

```bash
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-graph]"
```

Motion graph follow-up:

```bash
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-motion-graph]"
```

Coverage proves exact authority mapping, repeated path preservation, shared authority deduplication, same-authority one-incidence/two-mapping semantics, one child-local relationship despite repeated child occurrences, mixed local/Project connectivity, suppression and visibility policy, pure-local filtering, deterministic ordering, and Block-28 four-class motion closure.

## Current handoff

The geometric incidence contract remains frozen and is reused by Blocks 26-27. Block 28 is implemented with a separate motion-specific graph that reuses transform-authority identity without changing geometric solve groups.

Next is Block 29 only: derive stable structured exchange assembly/component/product occurrence identities from the canonical hierarchy and posed-leaf boundaries, then emit structured STEP assembly/product relationships.

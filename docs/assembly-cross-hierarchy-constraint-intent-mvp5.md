# Cross-Hierarchy Constraint Intent MVP-5

Status: implemented Block 23 of the persistent cross-hierarchy geometric constraint sequence. Block 24 JSON and structural validation are also implemented in `docs/assembly-cross-hierarchy-constraint-json-mvp5.md`.

This document is canonical for the Core-owned endpoint and project-level relationship intent contract.

## Implemented scope

```text
frozen occurrence-qualified endpoint identity
  -> Core-owned AssemblyHierarchyConstraintEndpoint
  -> project-level AssemblyHierarchyConstraint relationship intent
  -> Project-owned cross_hierarchy_constraints collection
  -> direct bridge into the existing read-only Geometry query layer
```

Record creation and collection insertion are model-intent operations. They do not resolve hierarchy geometry and do not move components or subassemblies.

## Core endpoint identity

`AssemblyHierarchyConstraintEndpoint` stores:

```text
occurrence_path
local ComponentInstanceId
semantic_reference
```

Exact identity is:

```text
(occurrence_path,
 local ComponentInstanceId,
 semantic_reference)
```

The empty path addresses the explicit root assembly occurrence:

```text
([], component.root, feature.base.top)
```

A nested endpoint may be:

```text
([subassembly.outer, subassembly.inner],
 component.shaft,
 feature.bore.axis)
```

Occurrence-path element order is identity. The Core record stores the path verbatim and does not sort, normalize, or resolve it during construction.

Endpoint construction rejects empty path ids, an empty component id, and an empty semantic-reference string.

## Why the endpoint belongs in Core

The earlier read-only Geometry seed introduced a query target with the same geometric identity shape.

Persistent relationship intent cannot use a `blcad::geometry` type as save-format authority. Block 23 therefore introduced the frozen endpoint value contract in Core.

Current ownership is:

```text
Core:
  AssemblyHierarchyConstraintEndpoint
    -> persistent model identity

Geometry:
  AssemblyHierarchyConstraintTarget
    -> derived target-resolution query seed
```

The Geometry query layer accepts the Core endpoint directly and converts it into its derived query target.

There is one persistent endpoint authority.

## Persistent project-level relationship intent

`AssemblyHierarchyConstraint` stores:

```text
id
name
type
target_a
target_b
state
optional distance
optional angle
```

Supported types are the existing geometric families:

```text
Mate
Concentric
Distance
Insert
Angle
```

State uses the existing:

```text
Active
Inactive
```

Target A/B order is preserved exactly.

The record reuses the same value-family rules as local `AssemblyConstraint`:

```text
Mate        -> no distance, no angle
Concentric  -> no distance, no angle
Insert      -> no distance, no angle
Distance    -> LengthMm distance required, no angle
Angle       -> AngleDeg angle required, no distance
```

Block 23 delegates this validation to the established local relationship contract instead of maintaining a second family rule set.

## Project ownership and id scope

`Project` owns:

```text
cross_hierarchy_constraints[]
```

Core collection APIs are:

```text
add_cross_hierarchy_constraint
cross_hierarchy_constraints
cross_hierarchy_constraint_count
find_cross_hierarchy_constraint
```

Project-level cross-hierarchy constraint ids are unique within this collection.

Local geometric constraint ids remain scoped by containing `AssemblyDocument`.

Therefore this is legal:

```text
assembly.root / constraint.shared
Project cross_hierarchy_constraints / constraint.shared
```

They are different relationship scopes.

## Creation does not resolve structure or geometry

`AssemblyHierarchyConstraintEndpoint::create` and `Project::add_cross_hierarchy_constraint` intentionally do not ask:

```text
does occurrence_path currently exist?
does the reached AssemblyDocument exist?
does the local ComponentInstanceId exist there?
does semantic_reference resolve to a supported feature target?
```

This allows Core model-intent construction to stay independent from hierarchy traversal and Geometry execution.

Block 24 subsequently added exact path/reached-component Project structure validation and JSON loading. Semantic feature geometry remains a later Geometry concern.

## Repeated child occurrence identity

Example:

```text
root
  subassembly.left  -> assembly.gearbox
  subassembly.right -> assembly.gearbox

assembly.gearbox
  component.shaft
```

These endpoints are distinct:

```text
([subassembly.left],  component.shaft, feature.bore.axis)
([subassembly.right], component.shaft, feature.bore.axis)
```

But both currently map to one persisted child-component transform authority:

```text
(assembly.gearbox, component.shaft)
```

The Core endpoint contract therefore preserves occurrence-specific geometric identity without claiming occurrence-local transform ownership.

## Identity split

The current cross-hierarchy architecture freezes three roles:

```text
geometric endpoint
  = (occurrence_path, local ComponentInstanceId, semantic_reference)

geometric component occurrence
  = (occurrence_path, local ComponentInstanceId)

persisted transform authority
  = (assembly_document: DocumentId, local ComponentInstanceId)
```

Future graph and solver layers must not collapse these roles.

## Geometry query bridge

`AssemblyHierarchyConstraintQuery::create` accepts:

```text
AssemblyHierarchyConstraintEndpoint
```

or a complete:

```text
AssemblyHierarchyConstraint
```

The query path converts Core endpoints into the existing derived Geometry target type and reuses the implemented root-space target/residual semantics.

This bridge does not make the Geometry target type persistent.

## Transform immutability

Adding project-level cross-hierarchy intent never changes:

```text
ComponentInstance::transform()
SubassemblyInstance::transform()
```

Relationship creation is not solving.

Grounding, suppression, visibility, and joint coordinates are also not changed by record insertion.

## Focused coverage

`tests/core/assembly_cross_hierarchy_constraint_tests.cpp` under:

```text
[core][assembly-cross-hierarchy-intent]
```

proves:

- root empty-path endpoint identity;
- exact nested occurrence-path ordering;
- empty occurrence/component/semantic identity rejection;
- shared five-family value rules;
- inactive state preservation;
- Distance and Angle quantity preservation;
- target A/B endpoint preservation;
- project-owned collection access and lookup;
- project-level id uniqueness;
- separate local and project-level relationship id scopes;
- root-to-child, repeated-left-to-right, and nested-to-root intent construction without path/component/geometry resolution;
- component and subassembly transforms remaining unchanged when intent is added.

Focused command:

```bash
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-intent]"
```

Block-24 persistence/structure coverage is separate:

```bash
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-json]"
```

## Implemented follow-up

Block 24 is implemented:

```text
cross_hierarchy_constraints[] project JSON
exact endpoint/target-order roundtrip
absent-field backward compatibility
exact occurrence-path structure validation
reached local component validation
```

See `docs/assembly-cross-hierarchy-constraint-json-mvp5.md`.

## Explicitly deferred

- active/suppressed relationship participation graph semantics;
- relationship-to-transform-authority incidence;
- deterministic cross-hierarchy solve groups;
- numeric variable ordering;
- persistent cross-hierarchy residual/Jacobian execution;
- solve results and freshness snapshots;
- atomic result application;
- cross-hierarchy rank/DOF diagnostics;
- cross-hierarchy joints and nested motion.

## Next technical step

Implement Block 25 only:

```text
active local and project-level cross-hierarchy relationships
  -> relationship-to-ComponentTransformAuthority incidence
  -> deterministic connected cross-hierarchy solve groups
```

Do not add numeric residual/Jacobian execution, solving, snapshots, proposals, diagnostics, or application in the same block.

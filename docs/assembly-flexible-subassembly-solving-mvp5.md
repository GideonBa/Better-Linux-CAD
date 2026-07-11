# Document-Scoped Flexible Subassembly Local Solving MVP-5

Status: implemented.

This document is canonical for the first flexible-child solving seed. The implementation deliberately solves the shared child `AssemblyDocument` in its own local assembly space; it does not introduce occurrence-local internal pose overrides.

## Scope

The public adapter is:

```text
AssemblyFlexibleSubassemblySolver
AssemblyFlexibleSubassemblySolveResultApplier
```

A caller selects one exact active non-root subassembly occurrence path.

The selected path identifies the referenced project-owned child `AssemblyDocument` whose internal component transforms are the persistent solve authority.

## Exact occurrence selection

The solve API accepts:

```text
occurrence_path = [SubassemblyInstanceId, ...]
```

The path is an exact root-to-current rooted occurrence sequence.

The empty root path is rejected. Missing paths and suppressed occurrence paths are rejected.

Visibility is not the flexible-solve participation boundary; suppression is.

The selected `SubassemblyInstance` boundary is used to identify the child document. Its rigid boundary transform is not a numeric variable in this seed.

## Child-as-local-root solve view

After exact occurrence selection, the solver creates a temporary Project view with:

```text
selected child AssemblyDocument -> temporary root assembly
all project-owned PartDocument records copied into the temporary Project
```

The existing local `AssemblyRigidBodySolver` then solves the requested connected local component group unchanged.

This reuses:

```text
AssemblyConstraintGraph
AssemblyConstraintTargetResolver
local equation builders
AssemblyConstraintNumericSystem
AssemblyNumericSolveEngine
AssemblySolveResult snapshots/proposals
```

There is no second optimizer and no hierarchy-specific local residual implementation.

## Result contract

`AssemblyFlexibleSubassemblySolveResult` stores:

```text
selected occurrence_path
selected child assembly_document identity
ordinary local AssemblySolveResult
```

`converged()` delegates to the local result state.

The wrapper preserves enough hierarchy identity to reject application if the selected occurrence boundary no longer references the same child document.

## Explicit application

`AssemblyFlexibleSubassemblySolveResultApplier`:

1. re-resolves the exact occurrence path in the current Project;
2. requires the same child `AssemblyDocument` identity;
3. rebuilds a fresh child-as-local-root Project view;
4. delegates ordinary local stale-result validation to `AssemblySolveResultApplier`;
5. applies the already-validated proposals to a Project copy;
6. writes resulting direct local component transforms into the selected project-owned child `AssemblyDocument`;
7. replaces the source Project only after every write succeeds.

The selected and ancestor `SubassemblyInstance::transform()` values remain unchanged.

No composed hierarchy transform is written back.

## Repeated child occurrence semantics

Suppose:

```text
subassembly.left  -> assembly.gearbox
subassembly.right -> assembly.gearbox
```

and the child document contains:

```text
component.shaft
```

The current model has one persistent direct child-component transform authority:

```text
(assembly.gearbox, component.shaft)
```

A successful flexible-child solve updates that child component transform once.

Both rooted occurrences immediately observe the new internal pose through their own independent rigid boundary transforms:

```text
left:
  [T_shaft_updated, T_left]

right:
  [T_shaft_updated, T_right]
```

This is document-scoped flexible solving.

It is not:

```text
([left],  component.shaft) -> independent internal transform
([right], component.shaft) -> independent internal transform
```

Occurrence-local internal pose overrides remain deferred.

## Leaf flattening and posed consumers

`AssemblyLeafOccurrenceResolver` continues to derive exact rooted leaf occurrences.

After successful application, repeated child occurrences retain separate occurrence paths and parent transform chains while reading the updated shared child component transform.

Posed STEP export and posed analysis consumers therefore observe the new child internal pose without any hierarchy cache persistence.

## Persistence boundary

The flexible solve introduces no new JSON field.

Persistent changes after explicit successful application are direct child `ComponentInstance::transform()` values already owned by the child `AssemblyDocument`.

Derived and unpersisted data includes:

```text
selected occurrence resolution
child-as-local-root Project view
ordinary local numeric solve state
solve snapshots and proposals
wrapper result context
```

No persistent flexible/rigid mode flag, occurrence-local transform override, composed transform cache, or solver coordinate is introduced.

## Failure policy

The solve/apply path fails closed on:

- invalid Project structure;
- empty/root occurrence path;
- missing occurrence path;
- suppressed occurrence path;
- selected child identity mismatch at application;
- any ordinary local solver failure;
- non-converged results;
- stale local component transform, grounding, or suppression snapshots;
- duplicate/invalid ordinary proposals;
- invalid proposed direct transforms.

The source Project remains unchanged on failed application.

## Focused coverage

Test tag:

```text
[geometry][assembly-flexible-subassembly]
```

Focused command:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-flexible-subassembly]"
```

The suite proves deterministic child-local Mate solving, source immutability before apply, atomic child-transform application, unchanged subassembly boundary transforms, immediate leaf-resolver visibility of the updated child pose, repeated child occurrences sharing one solved local pose while retaining distinct paths, invalid path rejection, suppressed path rejection, and stale child-transform rejection.

## Cross-hierarchy follow-up status

The previously deferred hierarchy relationship sequence has advanced:

```text
Block 22
  read-only occurrence-qualified target/residual semantics

Block 23
  Core endpoint + persistent project-level constraint intent

Block 24
  additive Project JSON + exact endpoint structure validation

Block 25
  relationship-to-ComponentTransformAuthority incidence
  + deterministic connected cross-hierarchy solve groups
```

The Block-25 authority identity formalizes the same shared-child transform authority already used by flexible-child solving:

```text
ComponentTransformAuthority =
  (assembly_document, local ComponentInstanceId)
```

Repeated child occurrences remain distinct geometric endpoint contexts but can map to this one shared transform authority.

## Explicitly deferred

- occurrence-local internal child pose overrides;
- whole-subassembly grounding and solve variables;
- `SubassemblyInstance` transform proposals;
- cross-hierarchy joint intent and nested motion propagation;
- component geometry instancing;
- structured STEP product hierarchy;
- collision/contact response or physics.

## Next technical step

Implement Block 26 from `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`: solve one `AssemblyCrossHierarchySolveGroup` through unique free active `ComponentTransformAuthority` variables, mixed local/root-space residual evaluation, the shared finite-difference Jacobian path, and the existing numeric solve engine.

Do not apply proposals or add cross-hierarchy diagnostics in Block 26.

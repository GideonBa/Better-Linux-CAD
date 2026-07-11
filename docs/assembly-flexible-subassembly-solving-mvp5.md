# Document-Scoped Flexible Subassembly Local Solving MVP-5

Status: implemented.

This document is canonical for the first flexible-child solving seed. The implementation solves a shared child `AssemblyDocument` in its own local assembly space; it does not introduce occurrence-local internal pose overrides.

## Scope

Public adapters:

```text
AssemblyFlexibleSubassemblySolver
AssemblyFlexibleSubassemblySolveResultApplier
```

A caller selects one exact active non-root subassembly occurrence path.

The selected path identifies the referenced Project-owned child `AssemblyDocument` whose internal component transforms are the persistent solve authority.

## Exact occurrence selection

The solve API accepts:

```text
occurrence_path = [SubassemblyInstanceId, ...]
```

The path is an exact root-to-current rooted occurrence sequence.

The empty root path is rejected. Missing paths and suppressed occurrence paths are rejected.

Visibility is not the flexible-solve participation boundary; suppression is.

The selected `SubassemblyInstance` boundary identifies the child document. Its rigid boundary transform is not a numeric variable.

## Child-as-local-root solve view

After exact occurrence selection, the solver creates a temporary Project view with:

```text
selected child AssemblyDocument -> temporary root assembly
all Project-owned PartDocument records copied into the temporary Project
```

The existing local `AssemblyRigidBodySolver` solves the requested connected local component group.

This reuses:

```text
AssemblyConstraintGraph
AssemblyConstraintTargetResolver
local equation builders
AssemblyConstraintNumericSystem
AssemblyNumericSolveEngine
AssemblySolveResult snapshots/proposals
```

There is no flexible-child-specific optimizer or hierarchy-specific local residual implementation.

The later Block-26 numeric refactor preserves this contract: the local rigid-body solver still uses the same public API and delegates internally to the shared numeric variable evaluator boundary.

## Result contract

`AssemblyFlexibleSubassemblySolveResult` stores:

```text
selected occurrence_path
selected child assembly_document identity
ordinary local AssemblySolveResult
```

`converged()` delegates to the ordinary local result state.

The wrapper preserves hierarchy identity needed to reject application if the selected occurrence boundary no longer references the same child document.

## Explicit application

`AssemblyFlexibleSubassemblySolveResultApplier`:

1. re-resolves the exact occurrence path in the current Project;
2. requires the same child `AssemblyDocument` identity;
3. rebuilds a fresh child-as-local-root Project view;
4. delegates local stale-result validation to `AssemblySolveResultApplier`;
5. applies the validated proposals to a Project copy;
6. writes resulting direct local component transforms into the selected Project-owned child `AssemblyDocument`;
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

Both rooted occurrences immediately observe the new internal pose through their independent rigid boundary transforms:

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

## Relationship to cross-hierarchy transform authority

Blocks 25 and 26 formalize the same shared-child ownership rule through:

```text
ComponentTransformAuthority =
  (assembly_document,
   local ComponentInstanceId)
```

For repeated child occurrences:

```text
([left],  component.shaft)
([right], component.shaft)
```

both geometric occurrences can map to:

```text
(assembly.gearbox, component.shaft)
```

Block 25 uses this authority for solve connectivity.

Block 26 uses it for numeric variable ownership and proposal identity.

Therefore repeated child endpoints can retain distinct occurrence paths and parent chains while sharing one candidate direct transform, one six-variable block, and at most one transform proposal.

This is consistent with the earlier flexible-child application contract.

## Leaf flattening and posed consumers

`AssemblyLeafOccurrenceResolver` remains the canonical hierarchy-to-leaf boundary for posed geometry consumers.

After successful flexible-child application, repeated child occurrences retain separate occurrence paths and parent transform chains while reading the updated shared child component transform.

Posed STEP export and posed analysis consumers therefore observe the new child internal pose without hierarchy cache persistence.

## Persistence boundary

Flexible-child solving introduces no JSON field.

Persistent changes after explicit successful application are direct child `ComponentInstance::transform()` values already owned by the child `AssemblyDocument`.

Derived and unpersisted flexible-child data includes:

```text
selected occurrence resolution
child-as-local-root Project view
ordinary local numeric solve state
local solve snapshots and proposals
wrapper result context
```

Cross-hierarchy graph and numeric products are also derived and unpersisted.

No persistent flexible/rigid mode flag, occurrence-local transform override, composed transform cache, or solver coordinate is introduced.

## Failure policy

The flexible solve/apply path fails closed on:

- invalid Project structure;
- empty/root occurrence path;
- missing occurrence path;
- suppressed occurrence path;
- selected child identity mismatch at application;
- ordinary local solver failures;
- non-converged results;
- stale local component transform, grounding, or suppression snapshots;
- duplicate or invalid ordinary proposals;
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

The hierarchy relationship sequence has advanced:

```text
Block 22
  read-only occurrence-qualified target/residual semantics

Block 23
  Core endpoint + persistent Project-level relationship intent

Block 24
  additive Project JSON + exact endpoint structure validation

Block 25
  relationship-to-ComponentTransformAuthority incidence
  + deterministic connected cross-hierarchy solve groups

Block 26
  authority-scoped mixed local/root-space numeric solving
  + shared finite differences and Gauss-Newton engine
  + complete authority snapshots and unapplied proposals
```

The Block-25/26 authority identity formalizes the same shared child-component transform authority already used by flexible-child solving.

## Explicitly deferred

- occurrence-local internal child pose overrides;
- whole-subassembly grounding and solve variables;
- `SubassemblyInstance` transform proposals;
- cross-hierarchy joint intent and nested motion propagation;
- component geometry instancing;
- structured STEP product hierarchy;
- collision/contact response or physics.

## Current handoff

The current cross-hierarchy sequence is canonical in `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

Next is Block 27 only: complete Block-26 result freshness validation, atomic authority-qualified direct-transform application, an explicit semantic target-producing geometry freshness contract, and cross-hierarchy rank/remaining-DOF diagnostics over the exact Block-26 free-authority variable ordering.

The flexible-child contract remains unchanged by Block 27 unless a deliberate shared freshness-revision mechanism is introduced for both local and cross-hierarchy solving.

# Flexible Subassembly Local Solving MVP-5

Status: implemented the first flexible-subassembly solver-variable seed. An exact active rooted child occurrence selects one project-owned child `AssemblyDocument`; that document's direct component variables and local geometric constraints are solved through the existing local rigid-body solver. The `SubassemblyInstance` boundary transform remains rigid and outside the numeric system.

## Scope

```text
exact active non-root occurrence path
  -> referenced child AssemblyDocument
  -> child document as temporary local Project root
  -> existing local AssemblyConstraintGraph
  -> existing local target/residual semantics
  -> shared numeric residual/Jacobian system
  -> AssemblyRigidBodySolver
  -> wrapped child-local solve result
  -> ordinary stale-result/application validation on a fresh local view
  -> atomic direct component-transform writes to the child document
```

The boundary transform remains:

```text
SubassemblyInstance::transform()
```

It is not a solve variable and is never changed by this block.

## Occurrence selection

`AssemblyFlexibleSubassemblySolver::solve` receives:

```text
const Project&
occurrence_path: [SubassemblyInstanceId, ...]
connected_group: [ComponentInstanceId, ...]
AssemblyRigidBodySolverOptions
```

The occurrence path is the exact root-to-current path defined by `AssemblyHierarchyTraversal`.

The path must:

- be non-empty;
- resolve exactly in the rooted hierarchy;
- be on an active hierarchy path;
- identify a project-owned non-root child `AssemblyDocument` through its current parent occurrence boundary.

The path is query/context identity. It is not persisted solver state.

## Child-as-root solve view

The selected child `AssemblyDocument` is copied into a temporary `Project` as the temporary root assembly. Project-owned part documents are copied into the view so existing semantic target resolution continues to work normally.

The local view deliberately preserves the existing local solver contract:

```text
AssemblyConstraintGraph::build(child)
exact deterministic local connected component
active-component suppression filtering
six direct local transform variables per free active component
existing semantic target resolution
existing residual flattening
existing finite-difference Jacobian
existing damped Gauss-Newton engine
existing grounding rules
existing solve states
existing component snapshots and proposals
```

No second optimizer, hierarchy-specific residual family, composed transform variable, or duplicate application validator is introduced.

## Result contract

`AssemblyFlexibleSubassemblySolveResult` stores derived context:

```text
occurrence_path
assembly_document
local_result: AssemblySolveResult
```

The embedded local result keeps its existing meaning inside the referenced child document.

`converged()` delegates to `local_result.converged()`.

The wrapper is derived and unpersisted.

## Atomic application

Application:

1. rebuilds the current hierarchy;
2. resolves the stored exact occurrence path;
3. requires the path to remain active and to reference the same child assembly document id;
4. reconstructs a fresh child-as-root solve view;
5. applies `local_result` through the existing `AssemblySolveResultApplier` on that local view;
6. therefore reuses ordinary converged-result, snapshot, grounding, suppression, source-transform, proposal/snapshot consistency, and transform validation;
7. copies the source `Project`;
8. writes only successfully applied direct child component transforms into the selected child document of the project copy;
9. replaces the source project only after all writes succeed.

A stale or invalid result changes no project state.

The selected `SubassemblyInstance::transform()` remains unchanged.

## Shared child-document transform authority

Project-owned child assembly documents are shared model definitions. One child document may occur repeatedly through different `SubassemblyInstance` records.

Example:

```text
root
  subassembly.left  -> assembly.gearbox boundary x = +100 mm
  subassembly.right -> assembly.gearbox boundary x = -100 mm

assembly.gearbox
  component.fixed
  component.shaft local z = 20 mm
```

A child-local Mate solve selected through `subassembly.left` may propose:

```text
component.shaft local z: 20 mm -> 8 mm
```

After application, the persistent authority that changed is:

```text
(assembly.gearbox, component.shaft)
```

Both rooted gearbox occurrences then observe the shared local `z = 8 mm` pose while preserving their independent `+100 mm` and `-100 mm` boundary transforms.

In the refined cross-hierarchy planning terminology:

```text
ComponentTransformAuthority =
  (AssemblyDocumentId, local ComponentInstanceId)
```

The two rooted component occurrences are geometrically distinct but map to one transform authority.

This is document-scoped flexible solving, not occurrence-local flexible instancing.

## Interaction with hierarchy target semantics

`AssemblyLeafOccurrenceResolver` and `AssemblyHierarchyConstraintTargetResolver` regenerate geometry from current model intent.

After successful application:

```text
updated child ComponentInstance::transform()
  -> leaf resolver reads the updated direct local transform
  -> hierarchy target resolver reads the same updated direct local transform
  -> each rooted occurrence applies its own parent transform chain
  -> repeated occurrences obtain different root-space geometry from one shared local transform authority
```

This is why future cross-hierarchy numeric variables must not be keyed by occurrence path alone while occurrence-local pose overrides remain deferred.

No posed shape, composed transform, leaf descriptor, hierarchy target descriptor, or analysis result is persisted by the flexible solver.

## Persistence boundary

No JSON schema change was introduced by this block.

Already-persisted intent that may change after explicit successful application:

```text
ComponentInstance::transform()
inside one project-owned child AssemblyDocument
```

Derived and unpersisted:

```text
occurrence-path solve selection
child-as-root Project view
AssemblyFlexibleSubassemblySolveResult
ordinary local AssemblySolveResult
component snapshots
transform proposals
numeric variables
residual vectors
Jacobians
hierarchy descriptors
flattened leaves
root-space hierarchy target descriptors
posed shapes
```

Rigid `SubassemblyInstance` boundary placement remains persistent model intent and changes only through explicit occurrence-edit APIs.

## Failure policy

The flexible child solve path fails closed on:

- empty/root occurrence paths;
- missing occurrence paths;
- suppressed occurrence paths or suppressed ancestors;
- invalid project structure or hierarchy;
- changed occurrence boundary identity;
- a selected target that is not a project-owned child assembly;
- invalid local connected groups;
- all existing target-resolution, residual, numeric, and solver failures;
- non-converged application attempts;
- stale child component transform, grounding, or suppression input;
- duplicate or inconsistent ordinary snapshots/proposals;
- occurrence target identity changing before application;
- failure while writing any proposed child component transform.

Application is atomic through a project copy.

## Focused coverage

`tests/geometry/assembly_flexible_subassembly_solver_tests.inc` proves:

- deterministic child-local Mate solving through an exact rooted occurrence path;
- source-project immutability before explicit application;
- ordinary local proposal semantics inside the child document;
- atomic child component-transform application;
- unchanged rigid subassembly boundary placement;
- leaf flattening observing the updated child component transform;
- two rigid occurrences of one child document sharing the solved internal pose;
- rejection of empty/root, missing, and suppressed occurrence paths;
- stale child component input rejection without partial mutation.

Focused command:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-flexible-subassembly]"
```

## Explicitly deferred

- occurrence-local internal component pose overrides;
- a persistent rigid/flexible occurrence mode;
- grounding or solving a complete `SubassemblyInstance` boundary as one rigid variable;
- cross-hierarchy joints and nested motion propagation;
- collision/contact response or physics;
- component geometry instancing.

Cross-hierarchy geometric endpoint semantics are now implemented separately in `docs/assembly-cross-hierarchy-relationship-semantics-mvp5.md`.

## Current handoff

The corrected cross-hierarchy solver sequence is documented in `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

The next implementation block is block 23 only: Core-owned frozen endpoint value intent plus persistent project-owned cross-hierarchy geometric constraint records.

Repeated child occurrences must remain distinct geometric occurrence identities while sharing child-document transform authority until occurrence-local internal pose overrides are deliberately introduced.

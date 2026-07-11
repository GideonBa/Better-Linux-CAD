# Flexible Subassembly Local Solving MVP-5

Status: implemented the first flexible-subassembly solver-variable seed. An exact active rooted child occurrence selects one project-owned child `AssemblyDocument`; that document's direct component variables and local geometric constraints are solved through the existing rigid-body solver. The `SubassemblyInstance` boundary transform remains rigid and outside the numeric system.

## Scope

This block adds document-scoped flexible child solving. It does not introduce occurrence-local component pose overrides, whole-subassembly variables, or cross-hierarchy solver participation.

```text
root occurrence path
  -> exact active child AssemblyDocument occurrence
  -> child document as temporary local solve root
  -> existing local AssemblyConstraintGraph
  -> existing shared residual/Jacobian system
  -> existing AssemblyRigidBodySolver
  -> wrapped child-local solve result
  -> existing stale-result/application validation on a fresh local view
  -> atomic component-transform application to the project-owned child document
```

The child occurrence boundary remains:

```text
SubassemblyInstance::transform()
```

It is not a solve variable in this block and is never changed by flexible child solving.

## Occurrence selection and identity

`AssemblyFlexibleSubassemblySolver::solve` receives:

```text
const Project&
occurrence_path: [SubassemblyInstanceId, ...]
connected_group: [ComponentInstanceId, ...]
AssemblyRigidBodySolverOptions
```

The occurrence path is the exact root-to-current path defined by `AssemblyHierarchyTraversal`.

The path must:

- be non-empty, so the ordinary root solver remains a separate API;
- resolve exactly to one rooted hierarchy occurrence;
- remain on an active, unsuppressed hierarchy path;
- identify a non-root project-owned child `AssemblyDocument` through its current parent `SubassemblyInstance` boundary.

The path is selection and stale-context identity. It does not become a new persistent solver record.

## Child-as-root solve view

The source `Project` is validated through `AssemblyHierarchyTraversal::build`, preserving the existing complete project structure and hierarchy validation boundary.

The selected child `AssemblyDocument` is copied into a temporary `Project` as that temporary project's root assembly. Project-owned `PartDocument` records are copied into the view so existing semantic target resolution continues to resolve referenced part models normally.

The local view deliberately exposes only the selected child document as solver root. The existing solver therefore keeps its exact semantics:

```text
AssemblyConstraintGraph::build(local child)
exact deterministic connected component
active-component suppression filtering
local geometric constraint collection
six persisted transform variables per free active component
existing target resolution
existing residual flattening
existing central finite-difference Jacobian
existing damped Gauss-Newton engine
existing grounding rules
existing solve states
existing component snapshots and transform proposals
```

No second numeric solver, hierarchy-specific residual family, composed transform variable, or duplicated application validator is introduced.

## Result contract

`AssemblyFlexibleSubassemblySolveResult` stores derived solve context:

```text
occurrence_path
assembly_document
local_result: AssemblySolveResult
```

`local_result` is the ordinary solver result produced from the child-as-root view. Its component ids, fixed-component set, snapshots, proposals, iteration count, state, and residual summary keep their existing meanings inside the referenced child document.

`converged()` delegates directly to `local_result.converged()`.

The wrapper remains derived and unpersisted.

## Atomic application and stale-result validation

`AssemblyFlexibleSubassemblySolveResultApplier` does not trust a stored child pointer or cached hierarchy descriptor.

Application:

1. rebuilds the current hierarchy and resolves the exact stored occurrence path;
2. requires the path to remain active and to resolve to the same child `AssemblyDocument` id;
3. reconstructs a fresh child-as-root solve view;
4. passes `local_result` to the existing `AssemblySolveResultApplier`;
5. reuses converged-result, duplicate snapshot/proposal, source-transform, grounding, suppression, proposal/snapshot consistency, and transform-validation checks;
6. copies the source `Project`;
7. writes only proposed direct child component transforms from the successfully applied local view into the selected child document of the project copy;
8. replaces the source project only after every write succeeds.

A stale or invalid result changes no project state.

The selected `SubassemblyInstance::transform()` remains unchanged throughout solve and application.

## Repeated child occurrences

Project-owned child assembly documents are shared model definitions. The rigid hierarchy allows one child `AssemblyDocument` to occur more than once through separate `SubassemblyInstance` records.

Example:

```text
root
  subassembly.left  -> assembly.gearbox  boundary x = +100 mm
  subassembly.right -> assembly.gearbox  boundary x = -100 mm

assembly.gearbox
  component.fixed
  component.shaft   local z = 20 mm
```

A child-local Mate solve selected through `subassembly.left` may propose:

```text
component.shaft local z: 20 mm -> 8 mm
```

After application, `assembly.gearbox/component.shaft` has local `z = 8 mm`. Both flattened gearbox occurrences observe the same internal `z = 8 mm` pose, while their independent `+100 mm` and `-100 mm` boundary transforms remain unchanged.

This is document-scoped flexible solving, not occurrence-local flexible instancing.

## Interaction with leaf flattening and posed geometry

No leaf-occurrence format changes are required.

`AssemblyLeafOccurrenceResolver` reads each component's current authored transform from the referenced child document and prepends it to the exact inner-to-outer parent transform chain.

After successful flexible child solve application:

```text
updated child ComponentInstance::transform()
  -> normal AssemblyLeafOccurrenceResolver regeneration
  -> updated first transform level for every occurrence of that child document
  -> unchanged parent SubassemblyInstance transform levels
  -> existing posed STEP export and posed analysis consumers
```

No shape, posed leaf, composed transform, export cache, or analysis record is persisted by the solver.

## Interaction with cross-hierarchy relationship semantics

The later read-only target/residual block is implemented in `docs/assembly-cross-hierarchy-relationship-semantics-mvp5.md`.

It reuses the same rooted occurrence-path identity but adds a local component and semantic feature reference:

```text
(occurrence_path, local ComponentInstanceId, semantic_reference)
```

That query layer can observe the child component transforms produced by this flexible solve path and evaluate them through the complete parent transform chain into root-assembly space.

The read-only hierarchy relationship layer does not change this solver's ownership contract: repeated occurrences of one child document still share one internal component pose. Persistent occurrence-qualified graph and solver integration remain a separate next block.

## Persistence boundary

No JSON schema change is introduced.

Already-persisted intent that may change after explicit successful application:

```text
ComponentInstance::transform() inside a project-owned child AssemblyDocument
```

Existing project/assembly JSON persists the solved internal child pose through normal child assembly document serialization.

Derived and unpersisted:

```text
occurrence-path selection context
child-as-root Project solve view
AssemblyFlexibleSubassemblySolveResult
ordinary local AssemblySolveResult
component snapshots
transform proposals
numeric variables
residual vectors
Jacobians
hierarchy descriptors
flattened leaves
posed shapes
```

The rigid `SubassemblyInstance` boundary transform remains persistent model intent and changes only through its existing explicit occurrence-edit API.

## Failure policy

The flexible child solve path fails closed on:

- empty/root occurrence paths;
- occurrence paths absent from the current rooted hierarchy;
- suppressed occurrence paths or suppressed ancestors;
- invalid project hierarchy or project structure;
- a path that no longer matches its parent occurrence boundary;
- a selected target that is not a project-owned child assembly;
- invalid ordinary connected groups;
- all existing target-resolution, residual, numerical, and solver failures;
- non-converged application attempts;
- stale child component transform, grounding, or suppression input;
- duplicate or inconsistent ordinary solve snapshots/proposals;
- occurrence target identity changing before application;
- failure while writing any proposed child component transform.

Application is atomic through a project copy.

## Focused coverage

`tests/geometry/assembly_flexible_subassembly_solver_tests.inc`, compiled through the existing geometry test target, proves:

- deterministic child-local Mate solving through an exact rooted occurrence path;
- source-project immutability before explicit application;
- ordinary solver proposal semantics inside the child document;
- atomic application of one child component transform;
- unchanged rigid subassembly boundary transform;
- leaf flattening immediately observing the updated child component transform;
- two rigid occurrences of one child document sharing the same solved internal pose while keeping distinct occurrence paths;
- rejection of empty/root, missing, and suppressed occurrence paths;
- stale child component input rejection without partial mutation.

Focused command:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-flexible-subassembly]"
```

## Explicitly deferred

- occurrence-local internal component pose overrides for repeated child documents;
- a persistent rigid/flexible occurrence mode flag, until a consumer requires one;
- grounding or solving a complete `SubassemblyInstance` boundary as one rigid variable;
- persistent project-level cross-hierarchy geometric constraint records;
- occurrence-qualified cross-hierarchy graph and numeric solver integration;
- cross-hierarchy joint records and nested motion propagation;
- cross-hierarchy Jacobian/remaining-DOF diagnostics;
- collision/contact response or physics;
- component geometry instancing.

## Next technical step

Read-only occurrence-qualified cross-hierarchy endpoint and residual semantics are now implemented in `docs/assembly-cross-hierarchy-relationship-semantics-mvp5.md`.

The next block is persistent project-level cross-hierarchy geometric constraint intent plus occurrence-qualified active-constraint graph and shared numeric solve/application integration. Numeric variable, snapshot, and proposal identity must include occurrence path so repeated occurrences of one child document never alias by local `ComponentInstanceId` alone.

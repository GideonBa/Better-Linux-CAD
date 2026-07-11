# Suppressed Components in Solved Groups MVP-5

Status: implemented the suppression policy inside the shared local rigid-body solve path, replacing the earlier hard rejection of suppressed components.

## Policy

A suppressed local component remains persistent model intent but leaves the active numeric subgroup:

- suppressed components contribute no solve variables and are neither fixed nor variable;
- every local geometric constraint touching a suppressed component vanishes from the collected numeric relationship set;
- suppressed components remain in complete solve snapshots, so suppression changes make old results stale;
- proposals must match free active component snapshots;
- the surviving constrained subgroup still requires grounded active participation under the existing local solver rules;
- a group whose relationships all vanish through suppression is trivially converged with zero residual components and zero iterations;
- diagnostics use the same filtered subgroup and surviving relationships.

This is a derived solve-participation policy. The persistent `AssemblyConstraint` records are not auto-suppressed or rewritten.

## Proven behavior

`tests/geometry/assembly_suppressed_component_solver_tests.cpp` proves:

- suppressing the middle component of an `a - b - c` Mate chain removes both numeric relationships while retaining complete three-component snapshots;
- suppressing one grounded component drops only its touching relationship while another grounded/free pair solves normally;
- a surviving constrained subgroup without an active grounded component is rejected under the established grounding rule;
- changing suppression after solving makes the result stale and unapplable;
- diagnostics use only active free components and surviving relationships;
- repeated evaluation is deterministic.

The corresponding rigid-body solver regression asserts vanishing-relationship behavior rather than hard rejection.

## Persistence boundary

Persistent authority remains:

```text
ComponentInstance::suppression_state
AssemblyConstraint intent
```

Derived and unpersisted:

```text
active component subgroup
filtered numeric relationship set
numeric variables
residual vectors
Jacobians
solve results
snapshots
proposals
rank / remaining-DOF diagnostics
```

Suppression filtering never rewrites constraint state.

## Hierarchy interaction

Later hierarchy work adds a second suppression dimension: `SubassemblyInstance` path state.

For posed leaf consumers, a suppressed subassembly occurrence removes its complete subtree.

For future persistent cross-hierarchy solving, path-sensitive participation must be evaluated before repeated occurrence nodes are mapped to shared transform authority. One suppressed occurrence path must not automatically suppress another active occurrence of the same child document.

That future contract is specified in `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

## Current boundaries

Still deferred:

- automatic persistent suppression cascades or rewriting relationship state;
- configuration/model-state systems that author groups of suppression changes;
- occurrence-local internal component pose overrides.

Implemented later in separate blocks:

- posed assembly STEP export;
- Revolute joint motion with selected-endpoint suppression rejection and non-selected drive filtering;
- rigid subassembly hierarchy;
- posed interference/clearance analysis;
- document-scoped flexible child solving;
- read-only cross-hierarchy target/residual semantics.

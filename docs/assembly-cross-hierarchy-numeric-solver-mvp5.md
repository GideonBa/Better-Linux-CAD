# Cross-Hierarchy Numeric Solver MVP-5

Status: implemented as Block 26 of `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`. Block 27 freshness/application/diagnostics is implemented in `docs/assembly-cross-hierarchy-application-diagnostics-mvp5.md`.

This document is canonical for authority-scoped mixed local/cross-hierarchy numeric execution and derived transform proposals. Block 27 owns complete result freshness, atomic application, and rank/remaining-DOF diagnostics.

## Scope

Block 26 implements:

- exact current `AssemblyCrossHierarchySolveGroup` validation at solve start;
- one six-variable direct-transform block per unique free active `ComponentTransformAuthority`;
- grounded authority residual context without variables;
- local relationship evaluation once in containing-document local space;
- Project-level cross-hierarchy relationship evaluation through exact occurrence paths in root-assembly space;
- deterministic mixed relationship residual concatenation;
- shared Mate, Distance, Angle, Concentric, and Insert scalar flattening;
- shared central finite differences;
- the existing damped Gauss-Newton optimizer;
- source Project immutability;
- derived authority-qualified transform proposals.

Block 27 extends the result with complete modeled-input snapshots and provides explicit application. Block 26 itself remains a read-only solve boundary.

## Input group authority

The solver accepts exactly one current:

```text
AssemblyCrossHierarchySolveGroup
```

At solve start it rebuilds:

```text
AssemblyCrossHierarchyConstraintGraph::build(project)
```

and requires the supplied group to equal one deterministic `solve_groups()` entry.

Participation changes before solve therefore reject an old group descriptor.

Post-solve freshness is separately enforced by Block 27.

## Numeric variable identity

Variable ownership is:

```text
ComponentTransformAuthority =
  (assembly_document: DocumentId,
   local ComponentInstanceId)
```

The identity is derived. It addresses persistent:

```text
AssemblyDocument::ComponentInstance::transform()
```

Each unique free active authority contributes exactly:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

Variable blocks follow canonical Block-25 authority order after filtering to free authorities.

Grounded authorities remain residual dependencies and fixed context but contribute no variable block.

The current numeric contract requires at least one grounded authority when relationship residuals exist.

## Repeated child occurrences

For:

```text
root
  left  -> assembly.child
  right -> assembly.child

assembly.child
  component.child
```

geometric occurrences:

```text
([left],  component.child)
([right], component.child)
```

are distinct, but both may map to:

```text
(assembly.child, component.child)
```

Block 26 allocates one six-variable block for that shared authority.

During residual evaluation the candidate direct transform is written once to the child component in a Project copy. Both endpoint resolvers read that same candidate transform, then each follows its own exact parent chain.

Thus:

```text
same candidate direct transform
+ different occurrence path
+ different parent transform chain
= potentially different root-space geometry
```

No occurrence-local composed transform is perturbed or proposed.

## Local relationship evaluation

Local relationship identity is:

```text
(assembly_document: DocumentId,
 AssemblyConstraintId)
```

For each local relationship in exact solve-group order:

1. resolve the containing `AssemblyDocument`;
2. copy it as the root of a temporary local Project view;
3. copy Project-owned PartDocuments into the view;
4. resolve the exact local constraint;
5. evaluate through the existing local numeric relationship path.

The existing local target and equation builders remain authoritative.

A child-local relationship is evaluated once in child-document local assembly space. It is not duplicated per rooted child occurrence.

## Cross-hierarchy relationship evaluation

For each Project-level cross-hierarchy relationship:

1. resolve the persistent `AssemblyHierarchyConstraint`;
2. create the derived hierarchy query;
3. preserve target A/B and exact occurrence paths;
4. read candidate direct transforms from authority-owned components;
5. evaluate each endpoint through its component-plus-parent chain into root space;
6. build the canonical hierarchy equation descriptor;
7. flatten the residual through the shared scalar implementation.

`AssemblyHierarchyConstraintEquationBuilder` remains authoritative for root-space hierarchy target/equation semantics.

## Shared residual flattening

Canonical scalar counts remain:

```text
Mate        4
Distance    4
Angle       1
Concentric  6
Insert      7
```

Length residuals retain the established `length_residual_scale_mm` scaling.

Complete relationship blocks concatenate in exact Block-25 `solve_group.relationships` order.

No scalar row is introduced for shared transform authority.

## Shared finite-difference and solve engine

The numeric system owns one evaluator contract:

```text
absolute candidate variable vector
  -> deterministic scaled residual vector
```

Central finite differences are shared:

```text
J[:, i] = (r(x + h_i e_i) - r(x - h_i e_i)) / (2 h_i)
```

Translation and rotation variables use the existing millimeter/degree perturbation options.

`solve_numeric_variables` remains the only optimizer and owns option validation, initial residual evaluation, grounded-reference policy, fixed inconsistency classification, finite-difference Jacobians, damped Gauss-Newton normal equations, partial-pivot dense solve, damping escalation, backtracking, and solve-state classification.

Ordinary local solving and joint motion use the local `solve_numeric_relationships` adapter.

Cross-hierarchy solving adapts `ComponentTransformAuthority` values to the same engine.

## Result contract after Block 27 extension

`AssemblyCrossHierarchySolveResult` now stores:

```text
state
iterations
selected relationship identities
fixed transform authorities
complete participating authority snapshots
complete local/cross relationship input snapshots
complete participating hierarchy boundary snapshots
canonical semantic target PartDocument snapshots
unapplied free-authority transform proposals
residual summary
```

Authority snapshots are qualified by:

```text
(assembly_document, ComponentInstanceId)
```

and include referenced PartDocument identity, grounding, suppression, and source direct transform.

Block 27 documents the exact freshness semantics of the additional snapshot collections.

There is at most one generated proposal per free authority. Block-27 application strengthens this into exact free-authority proposal coverage.

No proposal addresses a `SubassemblyInstance` boundary transform.

## Source immutability

Initial residual evaluation, finite differences, line search, accepted iterations, solved-project reconstruction, and result construction operate on Project copies.

The source Project remains unchanged by solving.

Only the explicit Block-27 applier may commit fresh converged proposals.

## Focused coverage

Block-26 numeric coverage:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-solver]"
```

The suite proves all five residual families, root-to-child convergence, nested exact parent chains, repeated child occurrences sharing one variable block/proposal, same-authority endpoints through different parent chains, mixed local/cross residuals, canonical relationship order, insertion-order independence, grounded context, no-grounded failure, solve-start group freshness, and source/boundary immutability.

Block-27 follow-up coverage:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-application]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-diagnostics]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-semantic-freshness]"
```

## Persistence boundary

No numeric solve, freshness snapshot, proposal, Jacobian, rank product, or diagnostics product is serialized.

Persistent model intent remains direct component state/placement, rigid subassembly occurrence state/placement, local geometric constraints, and Project-level occurrence-qualified cross-hierarchy geometric constraints.

Derived and unpersisted products include transform authorities, incidence, endpoint mappings, solve groups, mixed residual vectors, finite-difference Jacobians, solve state, authority/relationship/boundary/semantic-PartDocument snapshots, proposals, freshness products, and diagnostics.

## Implemented follow-up

Block 27 is canonical in:

```text
docs/assembly-cross-hierarchy-application-diagnostics-mvp5.md
```

It implements complete authority/relationship/group/hierarchy-boundary freshness, exact canonical PartDocument model-intent freshness, atomic authority-qualified application, shared matrix-rank calculation, and cross-hierarchy remaining-DOF diagnostics over the exact Block-26 free-authority order.

## Next technical step

Implement Block 28 only: persistent occurrence-qualified cross-hierarchy Revolute joint intent, joint-to-`ComponentTransformAuthority` incidence, combined geometric/joint motion connectivity across assembly documents, and nested Revolute motion propagation through the shared numeric and Block-27 fresh-application boundaries.

Do not add occurrence-local child pose overrides, whole-subassembly transform variables, component geometry instancing, or swept-motion contact analysis in Block 28.

# Cross-Hierarchy Numeric Solver MVP-5

Status: implemented as Block 26 of `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

This block connects the deterministic Core solve groups from `AssemblyCrossHierarchyConstraintGraph` to the existing assembly numeric machinery. It solves one exact cross-hierarchy solve group on Project copies and returns derived, unapplied transform-authority proposals.

## Scope

Implemented:

- authority-scoped direct component-transform variables;
- free/grounded authority partitioning;
- local relationship evaluation in containing-document local assembly space;
- project-level cross-hierarchy relationship evaluation in root-assembly space;
- deterministic mixed local/cross residual concatenation;
- the existing Mate, Distance, Angle, Concentric, and Insert scalar flattening order and length scaling;
- one shared central finite-difference Jacobian implementation;
- one shared damped Gauss-Newton solve engine;
- complete participating transform-authority snapshots;
- at most one unapplied transform proposal per free authority;
- source Project immutability;
- exact current solve-group validation before numeric work.

Not implemented here:

- result application;
- complete stale-result validation after a result has been returned;
- local/cross relationship snapshots for application freshness;
- hierarchy-boundary snapshots for application freshness;
- semantic target-producing part revision tracking;
- cross-hierarchy rank/remaining-DOF diagnostics;
- cross-hierarchy joints or nested motion propagation;
- occurrence-local internal pose overrides;
- `SubassemblyInstance` transform variables.

## Input group authority

The solver accepts one exact:

```text
AssemblyCrossHierarchySolveGroup
```

Before numeric work it rebuilds:

```text
AssemblyCrossHierarchyConstraintGraph::build(project)
```

and requires the supplied group to equal one current deterministic `solve_groups()` entry.

This protects Block-26 execution from stale participation selection. For example, if an endpoint path becomes suppressed after a group was selected, the old group is rejected because it is no longer a current Block-25 solve group.

This is not the complete Block-27 stale-result contract. It only validates the solve query at solve start.

## Numeric variable identity

Variable ownership is the Block-25 transform authority:

```text
ComponentTransformAuthority =
  (assembly_document: DocumentId,
   local ComponentInstanceId)
```

The authority identity itself is derived. The persistent state it addresses remains:

```text
AssemblyDocument::ComponentInstance::transform()
```

Each unique free active authority contributes exactly six absolute direct-transform variables in this order:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

Variable blocks follow the canonical Block-25 authority order after filtering to `ComponentGroundingState::Free`.

Grounded authorities remain residual dependencies and are returned in `fixed_authorities`, but contribute no variable block.

The existing assembly solve contract still requires at least one grounded authority when the selected group has relationship residuals.

## Repeated child occurrences

Consider:

```text
root
  left  -> assembly.child
  right -> assembly.child

assembly.child
  component.child
```

These geometric component occurrences are distinct:

```text
([left],  component.child)
([right], component.child)
```

but both map to one authority:

```text
(assembly.child, component.child)
```

Block 26 therefore allocates one six-variable block and returns at most one proposal for that authority.

During one residual evaluation the candidate direct transform is written once to the child component in a Project copy. Both endpoint resolvers read that same candidate transform. Each endpoint then follows its own exact parent transform chain.

Thus two endpoints can observe:

```text
same candidate component transform
+ different occurrence path
+ different parent transform chain
= different root-space geometry
```

No occurrence-local composed transform is perturbed or proposed.

## Local relationship evaluation

A local relationship identity is:

```text
(assembly_document: DocumentId,
 AssemblyConstraintId)
```

For each local relationship in the exact solve-group relationship order:

1. resolve the containing `AssemblyDocument` by id;
2. copy that document as the root of a temporary local `Project` view;
3. copy the source Project's owned `PartDocument` records into the view;
4. resolve the exact local `AssemblyConstraint` by id;
5. evaluate it through the existing local numeric relationship path.

The existing local path continues to own:

```text
AssemblyConstraintTargetResolver
AssemblyConstraintEquationBuilder
AssemblyConcentricConstraintEquationBuilder
AssemblyInsertConstraintEquationBuilder
```

A child document local constraint is therefore evaluated once in that document's local assembly space. Block 26 never selects an arbitrary rooted occurrence of the child document.

## Cross-hierarchy relationship evaluation

A project-level cross-hierarchy relationship identity is:

```text
project-level AssemblyConstraintId
```

For each cross-hierarchy relationship in the exact solve-group relationship order:

1. resolve the persistent `AssemblyHierarchyConstraint` from the Project;
2. create the derived `AssemblyHierarchyConstraintQuery`;
3. resolve target A and target B through their exact stored occurrence paths;
4. read each addressed component's current candidate direct transform from its authority in the candidate Project copy;
5. evaluate each endpoint through its own component-plus-parent transform chain into root-assembly space;
6. build the canonical hierarchy equation descriptor;
7. flatten the resulting residual descriptor through the shared residual-flattening implementation.

The existing:

```text
AssemblyHierarchyConstraintEquationBuilder
```

continues to own root-space cross-hierarchy target and equation semantics.

## Shared residual flattening

The local numeric system now exposes one shared internal residual-flattening boundary used by both local and cross-hierarchy evaluation.

Canonical scalar order remains:

### Mate

```text
normal_opposition.x
normal_opposition.y
normal_opposition.z
signed_separation_mm / length_residual_scale_mm
```

Component count: `4`.

### Distance

```text
normal_parallelism.x
normal_parallelism.y
normal_parallelism.z
distance_residual_mm / length_residual_scale_mm
```

Component count: `4`.

### Angle

```text
angle_alignment
```

Component count: `1`.

### Concentric

```text
direction_parallelism.x
direction_parallelism.y
direction_parallelism.z
axis_offset_mm.x / length_residual_scale_mm
axis_offset_mm.y / length_residual_scale_mm
axis_offset_mm.z / length_residual_scale_mm
```

Component count: `6`.

### Insert

```text
direction_parallelism.x
direction_parallelism.y
direction_parallelism.z
axis_offset_mm.x / length_residual_scale_mm
axis_offset_mm.y / length_residual_scale_mm
axis_offset_mm.z / length_residual_scale_mm
signed_seating_separation_mm / length_residual_scale_mm
```

Component count: `7`.

Mixed residual vectors concatenate complete relationship residual blocks in the exact canonical Block-25 `solve_group.relationships` order.

No additional scalar row is introduced for shared transform authority.

## Shared finite-difference boundary

The numeric system now defines an internal evaluator contract:

```text
absolute candidate variable vector
  -> deterministically scaled residual vector
```

Both ordinary local solving and cross-hierarchy solving adapt their model-specific transform ownership to this boundary.

The central finite-difference implementation is shared:

```text
J[:, i] = (r(x + h_i e_i) - r(x - h_i e_i)) / (2 h_i)
```

with the existing perturbation semantics:

```text
translation variable -> finite_difference_translation_step_mm
rotation variable    -> finite_difference_rotation_step_deg
```

Because one authority candidate transform is applied once to a complete candidate Project copy, every local and cross-hierarchy residual depending on that authority observes the same perturbation.

## Shared solve engine

The existing numeric optimizer is not copied.

`solve_numeric_variables` now owns the common absolute-variable solve loop:

```text
option validation
  -> initial residual evaluation
  -> grounded-reference requirement
  -> fixed-geometry consistency state
  -> central finite-difference Jacobian
  -> damped Gauss-Newton normal equations
  -> partial-pivot dense solve
  -> damping escalation
  -> backtracking line search
  -> convergence / maximum-iteration / numerical-failure state
```

The ordinary local solver and joint-motion path continue through `solve_numeric_relationships`, which adapts local root component ids to the shared engine.

The cross-hierarchy solver adapts `ComponentTransformAuthority` values to the same engine.

There is one optimizer and one central finite-difference implementation.

## Result contract

`AssemblyCrossHierarchySolveResult` stores:

```text
state
iterations
selected relationship identities
fixed transform authorities
complete participating authority snapshots
unapplied free-authority transform proposals
residual summary
```

A snapshot stores:

```text
ComponentTransformAuthority
grounding state
component suppression state
source direct RigidTransform
```

A proposal stores:

```text
ComponentTransformAuthority
source direct RigidTransform
proposed direct RigidTransform
```

There is at most one snapshot per participating authority because Block-25 group authorities are unique.

There is at most one proposal per free authority because numeric variable blocks are authority-scoped.

No proposal can address a `SubassemblyInstance` boundary transform.

## Source immutability

Every candidate residual evaluation starts from a Project copy and applies candidate direct component transforms only to that copy.

The source Project is not mutated by:

```text
initial residual evaluation
finite differences
line search
accepted iterations
result construction
```

Block 26 returns proposals only.

## Focused coverage

`tests/geometry/assembly_cross_hierarchy_constraint_solver_tests.cpp` uses:

```text
[geometry][assembly-cross-hierarchy-solver]
```

The suite proves:

- all five cross-hierarchy residual families use the established scalar counts and satisfied semantics;
- root-to-child Distance solving converges through one child authority proposal;
- a nested endpoint uses the exact two-boundary parent chain;
- repeated child occurrences share one variable block and one proposal;
- two endpoints of one authority observe the same candidate transform through different parent chains;
- a local child Mate and project-level cross Mate contribute to one mixed residual vector;
- local-before-cross relationship order is preserved from Block 25;
- reversed persistent relationship insertion yields the same solve result;
- grounded authorities remain snapshots/fixed context and produce no proposal;
- a group with relationship residuals and no grounded authority fails closed;
- a previously selected group is rejected after suppression changes current participation;
- a structurally modified group descriptor is rejected;
- the source component transform and `SubassemblyInstance` boundary transform remain unchanged.

Focused command:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-solver]"
```

## Persistence boundary

No Block-26 solve data is serialized.

Persisted model intent remains:

```text
component direct transform/state
rigid SubassemblyInstance placement/state
local AssemblyConstraint records
project-level AssemblyHierarchyConstraint records
```

Derived and unpersisted data now also includes:

```text
ComponentTransformAuthority identities
relationship-to-authority incidence
endpoint-authority mappings
cross-hierarchy solve groups
mixed scaled residual vectors
finite-difference Jacobians
numeric solve state
transform-authority snapshots
transform-authority proposals
```

## Next technical step

Implement Block 27 only:

```text
Block-26 solve result
  -> complete relationship + hierarchy-boundary freshness validation
  -> explicit semantic-geometry freshness contract
  -> atomic authority-qualified direct-transform application
  -> rank / constrained-DOF / remaining-DOF diagnostics
     over the exact Block-26 free-authority variable order
```

Do not introduce cross-hierarchy joint motion, occurrence-local internal pose overrides, or whole-subassembly transform variables in Block 27.

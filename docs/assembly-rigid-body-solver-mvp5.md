# Assembly Rigid-Body Solver MVP-5

Status: implemented deterministic local rigid-body solving for Mate, Distance, Concentric, Insert, and planar Angle relationships, including suppression filtering, complete component and semantic-target PartDocument freshness snapshots, explicit solve states, atomic result application, and shared numeric-engine reuse by Revolute motion.

## Scope and locality

`AssemblyRigidBodySolver` solves exactly one deterministic local connected component of the temporary solve view's root `AssemblyDocument`.

The ordinary public path receives:

```text
const Project&
connected_group: [ComponentInstanceId, ...]
AssemblyRigidBodySolverOptions
```

For a normal project, the local relationship graph is `project.assembly()`.

`AssemblyFlexibleSubassemblySolver` reuses the same solver by copying one selected child `AssemblyDocument` into a temporary `Project` as the local root. The rigid-body solver itself remains local; it does not traverse cross-hierarchy endpoint paths.

## Supported local geometric families

The shared local numeric path supports:

```text
Mate
Distance
Concentric
Insert
Angle
```

Canonical residual semantics live in their feature documents. Semantic feature-target inference remains owned by the existing target/equation builders.

## Deterministic input group

`AssemblyConstraintGraph::build(project.assembly())` derives local active-constraint connectivity.

The requested `connected_group` must exactly equal one deterministic graph connected component.

Component and relationship ordering is lexicographic according to the existing local graph/numeric contracts.

The solver does not silently expand or shrink the requested graph component except for documented suppression filtering before numeric participation.

## Suppression participation

Complete group component identity is preserved for snapshots.

For numeric participation:

```text
active_subgroup = non-suppressed group components
```

Local constraints survive only when both endpoints are in the active subgroup.

Suppressed components:

- contribute no numeric variables;
- are not fixed bodies;
- receive no proposals;
- remain in complete component and referenced-part freshness context.

If all relationships vanish through suppression, the group is trivially converged with zero residual components.

## Grounding and variables

Grounded active components participate in target/residual evaluation but contribute no numeric variables.

Free active components contribute six direct persisted-transform variables:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

The variables map directly to `ComponentInstance::transform()`.

The local solver does not optimize a matrix, quaternion, composed hierarchy transform, or shape placement cache.

When surviving relationship residuals exist, the current solve contract requires at least one grounded active reference.

Grounded components are never moved to repair inconsistency.

## Shared numeric relationship system

The private numeric relationship set contains:

```text
constraint_ids[]
revolute_drives[]
```

Ordinary rigid-body solving supplies geometric constraint ids and an empty Revolute drive set.

`AssemblyJointMotionSolver` supplies geometric constraint ids plus deterministic transient Revolute drives.

Both paths call:

```text
detail::solve_numeric_relationships
```

which adapts local component variables to the shared:

```text
solve_numeric_variables
```

absolute-variable-vector/residual-evaluator engine.

There is one central finite-difference implementation and one damped Gauss-Newton optimizer for ordinary local, joint-motion, and later cross-hierarchy solving.

## Residual flattening

Length residual components are divided by `length_residual_scale_mm`.

Canonical scalar order remains:

```text
Mate:
  normal_opposition.x
  normal_opposition.y
  normal_opposition.z
  signed_separation_mm / scale

Distance:
  normal_parallelism.x
  normal_parallelism.y
  normal_parallelism.z
  distance_residual_mm / scale

Concentric:
  direction_parallelism.x
  direction_parallelism.y
  direction_parallelism.z
  axis_offset_mm.x / scale
  axis_offset_mm.y / scale
  axis_offset_mm.z / scale

Insert:
  direction_parallelism.x
  direction_parallelism.y
  direction_parallelism.z
  axis_offset_mm.x / scale
  axis_offset_mm.y / scale
  axis_offset_mm.z / scale
  signed_seating_separation_mm / scale

Angle:
  angle_alignment
```

Residual order follows deterministic relationship order and family-specific scalar order.

## Finite-difference Jacobian

The solver uses central finite differences over direct component variables:

```text
J[:, j] = (r(x + h_j e_j) - r(x - h_j e_j)) / (2 h_j)
```

Translation and degree-rotation perturbation sizes are explicit finite positive solver options.

Every residual evaluation occurs on a private candidate `Project` copy.

Analytic Jacobians are not implemented.

## Damped Gauss-Newton engine

For residual vector `r` and Jacobian `J`:

```text
(J^T J + lambda I) delta = -J^T r
```

The shared engine uses deterministic dense matrices, partial-pivot Gaussian elimination, damping escalation, backtracking line search, RMS convergence, explicit iteration limits, and explicit numerical-failure classification.

There is no sparse solve or trust-region path yet.

## Solve states

### Converged

```text
residual_rms <= convergence_rms
```

Only a converged result may be applied.

### MaximumIterationsReached

The iteration limit was reached above tolerance. Current best proposals are diagnostic and unapplable.

### FixedGeometryInconsistent

The all-grounded/fixed active system remains above tolerance. Grounded components are not moved.

### NumericalFailure

Free variables exist but no configured damping/line-search attempt produces a decreasing RMS step.

Validation and target-resolution failures remain `Result<T>` errors.

## Read-only solve result

`AssemblyRigidBodySolver::solve` never mutates the caller's `Project`.

`AssemblySolveResult` stores:

```text
solve state
iteration count
residual summary
complete group component snapshots
canonical semantic target PartDocument snapshots
fixed component identities
free-active component transform proposals
```

Solver output is derived and unpersisted.

## Complete component snapshots

Every group component snapshot stores:

```text
component id
referenced PartDocument id
grounding state
suppression state
source direct transform
```

Snapshots include grounded and suppressed group components.

Application is therefore stale when a snapshotted component is removed, retargeted to another part, moved, regrounded, or resuppressed.

Visibility remains outside local numeric solve input freshness.

## Exact semantic target PartDocument freshness

Block 27 closes the previous semantic-target model freshness gap for local solving as well as cross-hierarchy solving.

For every unique PartDocument referenced by a component in the complete connected group, the result stores:

```text
AssemblySemanticTargetPartSnapshot
  part_document
  canonical_model_intent_json
```

The payload is the exact output of:

```text
serialize_part_document_to_json(part)
```

At application the current PartDocument is serialized again and compared byte-for-byte.

No hash or mutable revision counter is used.

This is a conservative exact model-intent contract rather than a minimal semantic-target dependency closure. Any serialized model-intent edit in a participating referenced part invalidates the result.

The same helper is used by cross-hierarchy solving.

## Explicit atomic application

`AssemblySolveResultApplier::apply` accepts only converged results.

Before mutation it validates:

- duplicate component snapshots;
- every snapshotted component still exists;
- referenced PartDocument identity still matches;
- source transform, grounding, and suppression still match;
- the exact canonical PartDocument snapshot set is complete and unchanged;
- duplicate proposals;
- every proposal matches a free active component snapshot;
- proposal source transforms match snapshot source transforms;
- every proposed transform satisfies the normal component transform contract.

Application then performs:

```text
copy Project
  -> write proposed direct component transforms
  -> replace source Project only after every write succeeds
```

The caller Project remains unchanged on failure.

This one local application boundary is reused by every implemented local geometric family and inside flexible-child and joint-motion application.

## Flexible-child and Revolute inheritance

`AssemblyFlexibleSubassemblySolveResultApplier` rebuilds the selected child-as-local-root view and delegates its embedded ordinary local result to `AssemblySolveResultApplier`.

Therefore a participating PartDocument edit invalidates a flexible-child result before child transforms are written back.

`AssemblyJointMotionResultApplier` validates joint snapshots and then delegates the embedded ordinary local result to `AssemblySolveResultApplier` on a candidate Project copy.

Therefore local Revolute motion results also protect the exact participating PartDocument model intent before transforms or the selected joint coordinate change.

## Local diagnostics

`AssemblySolveDiagnosticsAnalyzer` evaluates the same local finite-difference Jacobian used by ordinary solving.

The matrix-rank implementation is now shared with cross-hierarchy diagnostics through:

```text
compute_assembly_matrix_rank
```

```text
variable_count  = 6 * free_active_component_count
constrained_dof = rank(J)
remaining_dof   = variable_count - rank(J)
```

Covered one-free-body results include:

```text
Concentric: rank 4/6, remaining DOF 2
Insert:     rank 5/6, remaining DOF 1
Angle:      rank 1/6, remaining DOF 5 away from extremal targets
```

A planar Mate or Distance has rank `3/6` and leaves three local rigid-body DOF.

Rank is derived from the numeric Jacobian and is not family-hard-coded.

## Error propagation

Semantic target and equation-builder failures propagate through the shared numeric path without being collapsed into generic solver errors.

The solver does not reinterpret semantic tokens.

## Persistence boundary

Persistent authority used by the local solver remains:

```text
AssemblyDocument local AssemblyConstraint records
ComponentInstance referenced part / grounding / suppression / transform
PartDocument model intent
```

Derived and unpersisted products include:

```text
local active graph connectivity
active numeric subgroup
resolved target geometry
residual descriptors and scaled residual vectors
numeric variable ordering
finite-difference Jacobians
normal equations and damping attempts
solver iteration state
AssemblySolveResult
component snapshots
AssemblySemanticTargetPartSnapshot values
canonical PartDocument freshness payloads
transform proposals
rank and remaining-DOF diagnostics
```

No solve snapshot or freshness payload is serialized.

Only explicit successful result application changes persistent component transforms.

## Focused tests

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-insert]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-diagnostics]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-semantic-freshness]"
```

Coverage includes deterministic groups/variables, all five geometric families, grounding, suppression filtering, option validation, source-project immutability, component and referenced-part freshness, exact PartDocument model-intent freshness, atomic application, fixed inconsistency, non-convergence, semantic target failure propagation, and local rank/DOF proofs.

## Locality and current limitations

The ordinary rigid-body solver remains local to one temporary solve view's root `AssemblyDocument`.

Cross-hierarchy geometric solving, freshness/application, and diagnostics are implemented by their separate authority-qualified adapters.

Still deferred from this local solver contract:

- cross-hierarchy joint motion and nested motion propagation;
- whole-`SubassemblyInstance` solve variables or grounding;
- occurrence-local child component pose overrides;
- analytic Jacobians;
- sparse matrix storage/solve;
- trust-region methods;
- minimal semantic-target dependency revision closure.

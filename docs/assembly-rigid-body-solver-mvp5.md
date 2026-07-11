# Assembly Rigid-Body Solver MVP-5

Status: implemented deterministic local rigid-body solving for Mate, Distance, Concentric, Insert, and planar Angle relationships, including suppression filtering, complete stale snapshots, explicit solve states, atomic result application, and shared numeric-engine reuse by Revolute motion.

## Scope and locality

`AssemblyRigidBodySolver` solves exactly one deterministic local connected component of the temporary solve view's root `AssemblyDocument`.

The ordinary public path receives:

```text
const Project&
connected_group: [ComponentInstanceId, ...]
AssemblyRigidBodySolverOptions
```

For a normal project, the local relationship graph is `project.assembly()`.

`AssemblyFlexibleSubassemblySolver` later reuses the same solver by copying a selected child `AssemblyDocument` into a temporary `Project` as the local root. The rigid-body solver itself therefore remains local; it does not traverse or solve cross-hierarchy endpoints.

## Supported local geometric families

The shared local numeric path supports:

```text
Mate
Distance
Concentric
Insert
Angle
```

Canonical residual semantics live in their feature documents. The solver does not duplicate semantic feature-target inference.

## Deterministic input group

`AssemblyConstraintGraph::build(project.assembly())` derives local active-constraint connectivity.

The requested `connected_group` must exactly equal one deterministic graph connected component.

Component and relationship ordering is lexicographic according to the existing local graph/numeric contracts.

The solver does not silently expand or shrink the requested graph component except for the documented derived suppression filtering before numeric participation.

## Suppression participation

Complete group component identity is preserved for snapshots.

For numeric participation:

```text
active_subgroup = non-suppressed group components
```

Local geometric constraints survive only when both endpoints are in the active subgroup.

Suppressed components:

- contribute no numeric variables;
- are not treated as fixed bodies;
- receive no proposals;
- remain in complete input snapshots so later suppression changes make the result stale.

If all relationships vanish through suppression, the group is trivially converged with zero residual components.

## Grounding policy

Grounded active components participate in target and residual evaluation but contribute no numeric variables.

Free active components contribute six direct persisted-transform variables:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

The variables correspond directly to `ComponentInstance::transform()` fields. The local solver does not optimize a matrix, quaternion, composed hierarchy transform, or shape placement cache.

When surviving constrained relationships exist, the current local seed requires grounded active participation under the established solver validation contract.

Grounded components are never moved to repair inconsistency.

## Shared numeric relationship system

The private numeric relationship set contains:

```text
constraint_ids[]
revolute_drives[]
```

Ordinary `AssemblyRigidBodySolver` supplies geometric constraint ids and an empty Revolute drive set.

`AssemblyJointMotionSolver` later supplies geometric constraint ids plus deterministic transient Revolute drives.

Both paths call the private shared engine:

```text
detail::solve_numeric_relationships
```

There is no joint-specific or Insert-specific optimizer.

## Residual flattening

Length residual components are divided by `length_residual_scale_mm`.

Canonical local flattening includes:

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

The solver uses central finite differences over direct component variables.

For variable `x_j` and step `h_j`:

```text
J[:, j] = (r(x + h_j) - r(x - h_j)) / (2 h_j)
```

Translation and degree-rotation step sizes are explicit solver options and validated as finite positive values.

Every residual evaluation occurs on a private candidate `Project` copy.

The solver does not implement analytic Jacobians.

## Damped Gauss-Newton engine

For residual vector `r` and Jacobian `J`, the engine forms damped normal equations:

```text
(J^T J + lambda I) delta = -J^T r
```

The current implementation uses deterministic dense matrices and partial-pivot Gaussian elimination.

The iteration policy includes:

- finite option validation;
- deterministic damping escalation;
- backtracking line search;
- RMS residual convergence;
- maximum iteration limits;
- explicit numerical-failure classification.

There is no sparse solve path or trust-region implementation yet.

## Solve states

### Converged

```text
residual_rms <= convergence_rms
```

Only a converged result may be applied.

### MaximumIterationsReached

The configured iteration limit was reached above tolerance. Current best proposals remain diagnostic and unapplable.

### FixedGeometryInconsistent

The supported all-grounded/fixed active system remains above tolerance. Grounded components are not moved.

### NumericalFailure

Free variables exist but no configured damping/line-search attempt produces a decreasing RMS step.

Validation and target-resolution failures remain `Result<T>` errors rather than solve states.

## Read-only solve boundary

`AssemblyRigidBodySolver::solve` never mutates the caller's `Project`.

The engine changes only private project copies and returns `AssemblySolveResult`.

The result stores:

```text
solve state
iteration count
residual summary
complete group component snapshots
fixed component identity
free-active component transform proposals
```

Solver output is derived and unpersisted.

## Complete component snapshots

Every group component snapshot stores:

```text
component id
grounding state
suppression state
source transform
```

Snapshots include grounded and suppressed group components, not only numeric variables.

This protects application from stale solve inputs such as:

- a free component moved after solve;
- a grounded anchor moved;
- grounding changed;
- suppression changed.

## Explicit atomic application

`AssemblySolveResultApplier::apply` accepts only converged results.

Before mutation it validates:

- duplicate snapshots;
- duplicate proposals;
- every snapshotted component still exists;
- source transform, grounding, and suppression still match;
- every proposal matches a free active snapshot;
- proposal source transforms match snapshot source transforms;
- every proposed transform satisfies the normal component transform contract.

Application then:

```text
copy Project
  -> write proposed direct component transforms
  -> commit only after every write succeeds
```

The caller project is unchanged on any failure.

This one application boundary is shared by every implemented local geometric family and is also reused inside later joint-motion and flexible-child application paths.

## Local diagnostics

`AssemblySolveDiagnosticsAnalyzer` evaluates the same local geometric finite-difference Jacobian used by ordinary solving.

```text
variable_count  = 6 * free_active_component_count
constrained_dof = rank(J)
remaining_dof   = variable_count - rank(J)
```

Regular covered one-free-body results include:

```text
Concentric: rank 4/6, remaining DOF 2
Insert:     rank 5/6, remaining DOF 1
Angle:      rank 1/6, remaining DOF 5 away from extremal targets
```

Rank is derived from the generic numeric Jacobian. Family rank is not hard-coded.

## Error propagation

Semantic target and equation-builder failures propagate through the shared numeric path without being collapsed into generic solver errors.

Examples include unsupported semantic reference families, invalid `.axis` producers, and invalid `.seat` producers.

The solver does not reinterpret semantic tokens.

## Persistence boundary

Persistent authority used by the local solver:

```text
AssemblyDocument local AssemblyConstraint records
ComponentInstance grounding/suppression/transform
```

Derived and unpersisted:

```text
local active constraint graph connectivity
active numeric subgroup
resolved target geometry
residual descriptors
flattened residual vectors
numeric variable ordering
finite-difference Jacobians
normal equations and damping attempts
solver iteration state
AssemblySolveResult
component snapshots
transform proposals
rank and remaining-DOF diagnostics
```

Only explicit successful result application changes persistent component transforms.

## Focused tests

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-insert]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-diagnostics]"
```

Related Angle and suppression suites extend the same shared path.

Coverage across the local solver chain includes deterministic groups/variables, all five geometric families, grounding, suppression filtering, option validation, source-project immutability, stale-result detection, atomic application, fixed inconsistency, non-convergence, semantic target failure propagation, and local rank/DOF proofs.

## Current limitations

The ordinary rigid-body solver remains local to the temporary solve view's root `AssemblyDocument`.

Still not implemented in this local solver contract:

- persistent project-level cross-hierarchy constraints;
- cross-hierarchy relationship/solve connectivity;
- transform-authority deduplication across repeated child occurrences;
- cross-hierarchy numeric variables, snapshots, proposals, and application;
- whole-`SubassemblyInstance` solve variables or grounding;
- occurrence-local child component pose overrides;
- analytic Jacobians;
- sparse matrix storage/solve;
- trust-region methods;
- per-constraint weights;
- general floating-group gauge fixing;
- per-component null-space labels or drag projection.

The next cross-hierarchy solver work is deliberately split into blocks 23-27 in `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

## Current handoff

Do not extend this local solver by making occurrence paths direct numeric variable ids.

Block 23 first moves/extracts the frozen occurrence-qualified endpoint value contract into the Core layer and adds persistent project-owned cross-hierarchy geometric constraint intent. JSON, graph connectivity, numeric solving, and application follow in separate blocks.

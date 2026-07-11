# Assembly System with Constraints

Status: component instances, explicit placement/state updates, Mate/Concentric/Distance intent records, deterministic active-constraint connectivity, generated planar face target resolution, explicit rigid-transform evaluation, planar Mate/Distance residual construction, deterministic rigid-body solving, explicit atomic result application, and read-only local Jacobian-rank/remaining-DOF diagnostics are implemented.

Semantic generated-axis targets and Concentric residual construction are the next assembly block.

## Core model

A project owns one assembly and project-owned part documents. An assembly contains component occurrences referencing those parts.

```text
PartDocument Screw_M6x25
AssemblyDocument HousingAssembly
  ComponentInstance Screw_01 -> Screw_M6x25
  ComponentInstance Screw_02 -> Screw_M6x25
```

One part model may therefore be reused by several component occurrences.

Persistent records store model intent. Graphs, resolved geometry, residuals, Jacobians, solve results, and DOF diagnostics remain derived unless an explicit successful solve proposal is applied to the persisted component transform.

## Current assembly pipeline

```text
persistent component and constraint intent
  -> AssemblyConstraintGraph
  -> semantic target resolution
  -> component-local target descriptors
  -> explicit rigid-transform evaluation
  -> assembly-space target descriptors
  -> planar Mate/Distance residual construction
  -> shared deterministic numeric residual/Jacobian system
  -> rigid-body solve on a private Project copy
  -> proposed component transforms
  -> explicit atomic converged-result application
  -> read-only local Jacobian-rank and remaining-DOF diagnostics
```

The layers are intentionally separate.

## Component instances and placement

Canonical document: `docs/component-instance-mvp5.md`.

`AssemblyDocument` owns `ComponentInstance` values. Each occurrence carries:

```text
ComponentInstanceId
name
referenced_part_document
visibility
suppression_state
grounding_state
RigidTransform
  translation_mm
  rotation_deg
```

Direct placement/state edits are explicit storage operations. They do not infer constraints or run a solver.

## Assembly constraint intent

Canonical document: `docs/assembly-constraint-model-intent-mvp5.md`.

```text
AssemblyConstraintTarget
  component_instance
  semantic_reference

AssemblyConstraint
  id
  name
  type = mate | concentric | distance
  target_a
  target_b
  state = active | inactive
  distance = optional positive length
```

The record layer validates identity and type-specific fields but keeps semantic target tokens opaque. It does not resolve geometry, evaluate transforms, construct residuals, solve, or compute DOF.

Target A/B order is persistent relationship intent.

## Active-constraint graph

Canonical document: `docs/assembly-constraint-graph-mvp5.md`.

`AssemblyConstraintGraph` is read-only derived connectivity:

- every component is a node
- active constraints create distinct edges
- inactive constraints do not affect connectivity
- multi-edges are legal
- nodes are ordered lexicographically by component id
- edges and adjacency are ordered lexicographically by constraint id
- connected groups and their members are deterministic

The exact connected groups are the current solver partition boundary.

## Generated planar face target resolution

Canonical document: `docs/assembly-constraint-target-resolution-mvp5.md`.

The currently supported target family is:

```text
feature.<feature-id>.top
feature.<feature-id>.bottom
feature.<feature-id>.right
feature.<feature-id>.left
feature.<feature-id>.front
feature.<feature-id>.back
```

`AssemblyConstraintTargetResolver` resolves:

```text
component occurrence
  -> referenced project-owned PartDocument
  -> supported generated face
  -> ComponentLocalPlanarDescriptor
  + separate persisted RigidTransform
```

Supported generated faces reuse `WorkplaneResolver::resolve_generated_face`.

Semantic axis targets are not implemented yet.

## Rigid-transform evaluation

Canonical document: `docs/assembly-rigid-transform-evaluation-mvp5.md`.

The persisted transform convention is:

```text
translation: millimeters
rotation: degrees
positive rotation: right-hand rule
rotation type: active fixed-axis
application order: X -> Y -> Z
column-vector composition: Rz * Ry * Rx
```

Points are rotated then translated. Vectors, basis axes, normals, and later axis directions are rotated only.

`AssemblyTransformEvaluator` maps component-local planar descriptors to distinct `AssemblySpacePlanarDescriptor` values.

## Planar Mate and Distance residuals

Canonical document: `docs/assembly-planar-constraint-equations-mvp5.md`.

For assembly-space plane origins/normals `oA,nA` and `oB,nB`:

Mate:

```text
normal_opposition    = nA + nB
signed_separation_mm = dot(oB - oA, nA)
```

Distance:

```text
normal_parallelism   = cross(nA, nB)
signed_separation_mm = dot(oB - oA, nA)
distance_residual_mm = signed_separation_mm - target_distance_mm
```

Signed Distance measures target B from target A along target A's normal. Swapping targets may change the sign and therefore the residual.

`AssemblyConstraintEquationBuilder` supports active planar Mate and Distance only. Concentric is rejected before target resolution until semantic axis targets exist.

## Shared numeric system

The first solver and the DOF diagnostics now use one private geometry-layer numeric path:

```text
src/geometry/assembly_constraint_numeric_system.hpp
src/geometry/assembly_constraint_numeric_system.cpp
```

This path owns:

- deterministic constraint-id collection from the graph
- supported residual flattening
- residual RMS/max-absolute calculations
- free-component variable extraction
- application of variable vectors to project copies
- central finite-difference Jacobian construction

Variable order per free component:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

Free components follow deterministic connected-group order.

Each supported constraint contributes four scalar residuals in graph constraint-id order.

Mate:

```text
normal_opposition.x
normal_opposition.y
normal_opposition.z
signed_separation_mm / length_residual_scale_mm
```

Distance:

```text
normal_parallelism.x
normal_parallelism.y
normal_parallelism.z
distance_residual_mm / length_residual_scale_mm
```

Default length scale: `1.0 mm`.

The shared numeric path is important: DOF diagnostics measure the same local system that the solver optimizes.

## Rigid-body solver

Canonical document: `docs/assembly-rigid-body-solver-mvp5.md`.

### Participation

```text
Grounded  -> fixed participant
Free      -> six solver variables
Suppressed -> rejected by the first solver
Hidden/Visible -> no participation effect
```

At least one grounded component is required. The first solver does not invent a floating-group gauge condition.

Multiple grounded components are allowed.

An all-grounded group returns:

- `Converged` if residual RMS is within tolerance
- `FixedGeometryInconsistent` otherwise

### Numeric method

The first solver uses deterministic damped Gauss-Newton:

```text
evaluate r
  -> central finite-difference J
  -> J^T J + lambda I
  -> -J^T r
  -> partial-pivot Gaussian elimination
  -> deterministic backtracking line search
  -> damping escalation if no decreasing step is accepted
```

Default numeric policy:

```text
length residual scale                 1.0 mm
convergence RMS                       1.0e-8
translation finite-difference step    1.0e-4 mm
rotation finite-difference step       1.0e-4 deg
initial damping                        1.0e-6
maximum iterations                     100
maximum damping attempts               8
maximum line-search steps              12
```

Solve states:

```text
Converged
MaximumIterationsReached
FixedGeometryInconsistent
NumericalFailure
```

### Mutation boundary

`AssemblyRigidBodySolver::solve` mutates only private project copies.

`AssemblySolveResult` contains:

- solve state
- iteration count
- deterministic group/fixed component identity
- source component snapshots
- proposed transforms for free components
- residual summary

`AssemblySolveResultApplier` is the explicit mutation boundary. It accepts only converged results, rejects stale component input including moved grounded anchors, validates proposals, applies to another project copy, and replaces the caller project only after every update succeeds.

## Local Jacobian-rank and remaining-DOF diagnostics

Canonical document: `docs/assembly-solve-diagnostics-mvp5.md`.

The analyzer operates on one exact deterministic connected group and first invokes the existing solver.

### Solver-state boundary

```text
FixedGeometryInconsistent
  -> preserve explicit inconsistency
  -> no DOF rank classification

MaximumIterationsReached / NumericalFailure
  -> preserve non-convergence
  -> no DOF rank classification

Converged
  -> apply result to a private Project copy
  -> evaluate shared numeric residual/Jacobian system
  -> compute rank and local remaining DOF
```

The input project remains unchanged.

### Rank tolerance

```text
pivot_threshold = max(
  rank_absolute_tolerance,
  rank_relative_tolerance * maximum_abs_jacobian_entry
)
```

Defaults:

```text
rank_absolute_tolerance = 1.0e-12
rank_relative_tolerance = 1.0e-8
```

Both values must be finite and non-negative; they may not both be zero.

Rank is computed deterministically by row-echelon elimination:

- scan columns left-to-right
- select the maximum-magnitude remaining row pivot in the current column
- accept a pivot only when its magnitude is strictly above the threshold
- use row swaps and forward elimination

Solver damping does not participate in rank analysis. Damping is a solve stabilizer and must not manufacture constrained directions.

### DOF count

For the free solver variables:

```text
variable_count  = 6 * free_component_count
constrained_dof = rank(J)
remaining_dof   = variable_count - rank(J)
```

This is Jacobian nullity in the current direct `RigidTransform` coordinates.

It is a local linearized result, not a global configuration-space dimension.

### Classification

DOF classification:

```text
NotEvaluated
NoVariableDof
Underconstrained
LocallyFullyConstrained
```

Consistency classification:

```text
LocallyConsistent
FixedGeometryInconsistent
SolverDidNotConverge
```

Residual rank structure:

```text
NotEvaluated
FullRowRank
RedundantResidualComponents
```

The three classifications are deliberately separate.

A residual vector may have dependent scalar rows without the assembly being semantically overconstrained. For example, the three components of a unit-normal alignment residual can represent only two independent local rotational directions. Duplicate and overlapping constraints can also create row dependence.

Therefore:

```text
residual_component_count > rank(J)
```

does not imply semantic overconstraint.

The current explicit inconsistency boundary is `FixedGeometryInconsistent`. Arbitrary conflicting free-body constraint diagnosis requires a stronger model and remains future work.

## Persistence

Persisted assembly intent includes:

- member parts and parameter bindings
- component occurrences and component state
- component `RigidTransform`
- assembly constraint records and target A/B order

The following remain unpersisted derived data:

- active-constraint graph connectivity
- resolved target descriptors
- assembly-space frames
- Mate/Distance residual descriptors
- flattened residual vectors
- numeric Jacobians and normal equations
- solver damping/iteration state
- solve results and unapplied proposals
- residual summaries
- Jacobian rank summaries
- constrained/remaining DOF
- local DOF/consistency/rank-structure classification

Only explicit application of a fresh converged solve result changes the persisted component transform.

## Grounding, suppression, and visibility

Grounding is persisted model intent but is enforced only as solver participation policy. Direct storage-level transform updates remain possible on grounded components.

Visibility does not affect the current solver.

Suppression does not alter graph connectivity or residual construction yet. The first solver rejects a selected connected group containing a suppressed component rather than silently changing the graph/constraint system.

A future suppression-aware solve path must define connectivity and constraint participation together.

## Next assembly block: semantic axes and Concentric residuals

The next implementation block is a stable generated-axis semantic reference family and a read-only Concentric target/residual pipeline.

Required sequence:

```text
feature-produced stable axis intent
  -> semantic axis token
  -> component-local axis descriptor
  -> project/component/part resolution
  -> assembly-space axis evaluation
  -> canonical Concentric residual descriptor
```

Axis persistence must remain semantic. Raw OCCT edge, cylinder, or topology ids are not acceptable model references.

The transform convention already defines how the future axis descriptor must evaluate:

- axis point/origin: rotate then translate
- axis direction: rotate only

Concentric residual semantics must be stabilized before solver and DOF-diagnostic integration.

Insert remains downstream because it combines Concentric axis alignment with axial seating.

## Later assembly work

Later work includes:

- Concentric solver and DOF-diagnostic integration
- semantic overconstraint/conflict diagnostics for movable groups
- per-component DOF labels and null-space basis extraction
- drag projection into remaining DOF
- Insert, Angle, Tangent, Flush, Coincident, Lock, and richer constraint families
- joints and limits
- motion
- rigid then flexible subassemblies
- collision/interference analysis
- component geometry instancing
- assembly-level STEP export

The goal remains traceable semantic relationships and deterministic solve logic, not visually stacked BRep bodies.

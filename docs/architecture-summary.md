# Architecture Summary

This document condenses the implemented architecture and the current target direction. Feature-specific documents remain canonical for exact contracts.

## Goal

BLCAD is an independent parametric CAD system for Linux. The model stores design intent rather than only final BRep geometry: parameters, sketches, features, dependencies, semantic references, construction geometry, assembly structure, and later multi-body, path, surfacing, and engineering intent.

The long-term goal is recorded in `docs/project-goal.md`.

## Fundamental decisions

- Do not fork FreeCAD.
- Use OCCT as the geometry kernel.
- Keep a custom CAD core above OCCT.
- Treat OCCT shapes as computed cache data, not primary model intent.
- Persist semantic model references, not raw OCCT topology ids.
- Keep GUI code above the CAD core; UI state must not become geometry or solver authority.

## Layers

```text
user interface
command/application layer
parametric core
sketch/constraint layer
construction geometry and datum relations
multi-body part layer
3D sketch and surfacing layer
assembly layer
engineering modules
OCCT geometry kernel
file and exchange formats
```

## Current object families

Implemented and planned object families include:

- `PartDocument`, `AssemblyDocument`, `Project`
- `Parameter` and future `Expression`
- sketches, sketch entities, constraints, dimensions, and profile-region records
- `DatumPlane`, `DerivedWorkplane`, construction geometry and construction relations
- semantic face, edge, and vertex references
- implemented extrude/cut paths and future richer feature families
- `DependencyGraph`, invalidation/recompute state, and `ShapeCache`
- `ComponentInstance`
- `AssemblyConstraintTarget`, `AssemblyConstraint`
- `AssemblyConstraintGraph`
- `ComponentLocalPlanarDescriptor`, `ResolvedAssemblyConstraintTarget`
- `AssemblyConstraintTargetResolver`
- `AssemblySpacePlanarDescriptor`, `AssemblyTransformEvaluator`
- `AssemblySpaceConstraintTargetDescriptor`
- `PlanarMateResidualDescriptor`, `PlanarDistanceResidualDescriptor`
- `AssemblyConstraintEquationDescriptor`, `AssemblyConstraintEquationBuilder`
- `AssemblySolveState`, `AssemblySolveResult`, `AssemblyRigidBodySolver`
- `AssemblySolveResultApplier`
- `AssemblyDofClassification`
- `AssemblyConstraintConsistencyClassification`
- `AssemblyResidualRankStructure`
- `AssemblyJacobianRankSummary`
- `AssemblySolveDiagnostics`
- `AssemblySolveDiagnosticsAnalyzer`
- future semantic axis descriptors, Concentric residuals, joints, and motion
- future `BodyId`, body transform/boolean records, path records, 3D sketch and surfacing records

## Implemented part-model path

### MVP-1 single-part core

The first vertical slice provides:

- typed parameters and quantities
- datum planes
- simple sketches and rectangle/circle profiles
- additive and subtractive extrude intent
- dependency graph, invalidation, and recompute planning
- numeric parameter updates
- optional OCCT geometry execution
- `ShapeCache`
- STEP export
- JSON model-intent serialization
- headless JSON-to-STEP workflows

### Semantic workplanes, references, and richer profiles

The implemented path includes:

- semantic generated-face references
- derived workplanes and construction workplanes
- `WorkplaneResolver`
- projected/reference-driven sketch geometry
- construction points, lines, planes, and chained relations
- reference recovery helpers
- stable sketch entity ids
- line, arc, spline, composite, and automatically detected profile regions
- first sketch constraints and dimensions
- sketch-plane-relative feature direction
- diagnostics, deterministic repair suggestions, safe repair commands, transactions, undo, and read-only presentation snapshots

Persistent model intent remains distinct from raw kernel topology.

### MVP-3 parametric bolt circle

`docs/bolt-circle-pattern-mvp3.md` documents:

- count quantities and parameters
- circular hole pattern intent
- parameter dependencies
- subtractive recompute expansion into through-all cuts
- incremental recompute
- JSON persistence
- checked-in headless example geometry

## Implemented project and assembly container path

### Assembly parameters and project ownership

`docs/assembly-parameters-mvp4.md` and `docs/project-container-mvp4.md` define:

- assembly-scoped parameters
- member-part registration
- explicit `ParameterBinding` records
- binding propagation through normal part invalidation
- one `Project` owning an assembly and embedded parts
- project assembly-structure validation
- project-level parameter updates and per-part recompute plans
- project JSON and file helpers
- headless project parameter update/recompute/per-part STEP export

### Component instances and placement

`docs/component-instance-mvp5.md` is canonical.

A `ComponentInstance` is an occurrence with identity independent from part-document identity. Repeated occurrences may reference one project-owned `PartDocument`.

Persisted occurrence state includes:

```text
visibility
suppression_state
grounding_state
RigidTransform
  translation_mm
  rotation_deg
```

Storage-level placement/state edits are explicit. They do not infer constraints or run the solver.

## Implemented assembly constraint pipeline

The current assembly path is deliberately layered:

```text
persistent component/constraint intent
  -> AssemblyConstraintGraph
  -> AssemblyConstraintTargetResolver
  -> component-local target frame
  -> AssemblyTransformEvaluator
  -> assembly-space target frame
  -> AssemblyConstraintEquationBuilder
  -> planar Mate/Distance residual descriptor
  -> shared deterministic numeric residual/Jacobian system
  -> AssemblyRigidBodySolver
  -> AssemblySolveResult
  -> explicit AssemblySolveResultApplier
  -> AssemblySolveDiagnosticsAnalyzer
  -> local Jacobian rank and remaining DOF
```

Each layer has one responsibility.

### Persistent constraint intent

`docs/assembly-constraint-model-intent-mvp5.md` is canonical.

Implemented records support Mate, Concentric, and Distance intent with semantic target A/B references, active/inactive state, and a Distance-only positive length value.

The record layer does not interpret geometry, solve placement, or persist derived solver state.

### Deterministic active-constraint graph

`docs/assembly-constraint-graph-mvp5.md` is canonical.

`AssemblyConstraintGraph`:

- contains every component as a node
- contains active constraints as distinct edges
- excludes inactive constraints from connectivity
- preserves legal multi-edges
- sorts nodes and edges lexicographically
- provides deterministic adjacency and connected groups
- is read-only and unpersisted

Connected groups are the current solver partition boundary.

### Generated planar face target resolution

`docs/assembly-constraint-target-resolution-mvp5.md` is canonical.

Supported assembly target tokens currently use:

```text
feature.<feature-id>.top
feature.<feature-id>.bottom
feature.<feature-id>.right
feature.<feature-id>.left
feature.<feature-id>.front
feature.<feature-id>.back
```

For supported additive-extrude source features, `AssemblyConstraintTargetResolver` resolves project/component/part identity and reuses `WorkplaneResolver::resolve_generated_face` to produce a component-local planar descriptor.

Semantic axis targets are not implemented yet.

### Explicit rigid-transform evaluation

`docs/assembly-rigid-transform-evaluation-mvp5.md` is canonical.

The persisted `RigidTransform` convention is:

```text
translation: millimeters
rotation: degrees
positive rotation: right-hand rule
rotation type: active fixed-axis
application order: X, then Y, then Z
column-vector composition: Rz * Ry * Rx
```

Points are rotated then translated. Vectors, axes, and normals are rotated only.

### Planar Mate and Distance residuals

`docs/assembly-planar-constraint-equations-mvp5.md` is canonical.

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

Signed Distance semantics are target-order dependent. Target A/B order must remain stable through graph, numeric, solver, and diagnostic layers.

## Implemented shared assembly numeric system

The solver and DOF diagnostics share one private geometry-layer implementation:

```text
src/geometry/assembly_constraint_numeric_system.hpp
src/geometry/assembly_constraint_numeric_system.cpp
```

It owns the common numeric interpretation of the supported assembly equations:

- deterministic constraint-id collection
- Mate/Distance residual flattening
- residual RMS and maximum-absolute value
- free-component variable extraction
- numeric variable application to project copies
- central finite-difference Jacobian construction

Each free component contributes variables in exact order:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

Each supported planar constraint contributes four scalar residuals: three orientation components followed by the length residual divided by the configured millimeter scale.

The default length scale is `1.0 mm`.

This shared path prevents solver and diagnostics from interpreting the same constraint system differently.

## Implemented rigid-body solver

`docs/assembly-rigid-body-solver-mvp5.md` is canonical.

Participation policy:

- solve exactly one deterministic graph connected group
- require at least one grounded component
- treat every grounded component as fixed
- allow multiple grounded components
- reject suppressed group components explicitly
- ignore visibility for solver participation

The first numeric method is deterministic damped Gauss-Newton:

```text
r(x)
  -> central finite-difference J
  -> J^T J + lambda I
  -> -J^T r
  -> partial-pivot Gaussian elimination
  -> backtracking line search
  -> damping escalation when needed
```

Solve states:

```text
Converged
MaximumIterationsReached
FixedGeometryInconsistent
NumericalFailure
```

The solver mutates only private `Project` copies and returns proposed transforms.

`AssemblySolveResultApplier` is the explicit mutation boundary. It accepts only converged results, rejects stale component snapshots including moved grounded anchors, validates every proposal, applies to another project copy, and replaces the caller project only after all updates succeed.

## Implemented local Jacobian-rank and remaining-DOF diagnostics

`docs/assembly-solve-diagnostics-mvp5.md` is canonical.

`AssemblySolveDiagnosticsAnalyzer` runs the existing solver first and preserves its state.

For a converged group, proposals are applied only to a private project copy. The analyzer then evaluates the shared numeric residual/Jacobian system at that converged state.

Rank threshold:

```text
pivot_threshold = max(
  rank_absolute_tolerance,
  rank_relative_tolerance * maximum_abs_jacobian_entry
)
```

Default tolerances:

```text
rank_absolute_tolerance = 1.0e-12
rank_relative_tolerance = 1.0e-8
```

The deterministic rank routine scans columns left-to-right, selects the maximum-magnitude row pivot in the current column, accepts only pivots strictly above the threshold, and performs forward elimination.

For free solver variables:

```text
variable_count  = 6 * free_component_count
constrained_dof = rank(J)
remaining_dof   = variable_count - rank(J)
```

DOF classification:

```text
NotEvaluated
NoVariableDof
Underconstrained
LocallyFullyConstrained
```

Consistency classification is separate:

```text
LocallyConsistent
FixedGeometryInconsistent
SolverDidNotConverge
```

Residual row-rank structure is also separate:

```text
NotEvaluated
FullRowRank
RedundantResidualComponents
```

`RedundantResidualComponents` is a local linear-algebra statement. It is not automatically semantic overconstraint. A normal vector residual already contains three scalar rows while only two independent local rotational directions may be constrained; duplicate or overlapping equations are also possible.

The diagnostics therefore do not expose an `Overconstrained` state from `residual_count > rank(J)` alone.

The current DOF result is local and linearized in direct `RigidTransform` variables. It is not a global configuration-space dimension, singularity analysis, or null-space basis.

## Persistence boundary

Persisted model intent currently includes part/assembly/project records, component state and transforms, and assembly constraints.

The following remain regenerable derived data and are not serialized:

- constraint graph connectivity
- resolved target descriptors
- evaluated assembly-space frames
- Mate/Distance residual descriptors
- flattened numeric residual vectors
- numeric Jacobians and normal equations
- solve iterations, damping state, and solve results
- proposed transforms before application
- residual summaries
- Jacobian rank summaries
- constrained/remaining DOF counts
- local DOF/consistency/rank-structure classifications

Only explicitly applied successful transform proposals update the existing persisted component `RigidTransform` intent.

## Next assembly block

The next core CAD block is a stable semantic generated-axis reference family and a read-only Concentric target/residual pipeline.

Target sequence:

```text
supported feature intent
  -> stable semantic axis token
  -> component-local axis descriptor
  -> component occurrence / project-owned part resolution
  -> AssemblyTransformEvaluator-compatible point + direction evaluation
  -> assembly-space axis descriptor
  -> canonical Concentric residual descriptor
```

This block must not persist raw OCCT edge/cylinder ids or hide axis interpretation inside the solver.

Only after semantic axis and Concentric residual semantics are stable should `AssemblyRigidBodySolver` and `AssemblySolveDiagnosticsAnalyzer` consume Concentric constraints.

Insert remains downstream because it combines axis alignment with axial seating.

## Future architecture

Multi-body part modeling, body transform/boolean intent, path features, lofts, 3D sketches, and surfacing are documented in:

- `docs/multi-body-transform-and-path-features-roadmap.md`
- `docs/inventor-like-sketcher-and-feature-roadmap.md`
- `docs/advanced-surfacing-and-3d-sketch-mvp.md`

Later assembly work includes Concentric solver integration, Insert and richer constraint families, per-component/null-space DOF descriptions, joints and limits, drag projection, motion, rigid then flexible subassemblies, collision/interference checks, component geometry instancing, and assembly-level STEP export.

## Critical architecture rules

- Parameters and feature records are model intent.
- Recompute follows explicit dependency relations.
- Semantic references, not raw topology ids, cross persistent model boundaries.
- Construction geometry is user model intent, not temporary UI state.
- OCCT shapes are computed geometry/cache data.
- `PartDocument` remains OCCT-free; optional geometry execution lives in `blcad_geometry`.
- Assembly graph, target, transform-evaluation, residual, numeric-system, solver-result, and DOF-diagnostic layers have separate responsibilities.
- Target A/B order is semantically observable for signed Distance.
- Solver grounding/suppression participation is policy, not storage semantics.
- Jacobian damping is a solve stabilizer and must not create artificial constrained directions in DOF rank analysis.
- Local residual row redundancy is not equivalent to semantic overconstraint.
- GUI code must operate the core and must not own CAD logic.

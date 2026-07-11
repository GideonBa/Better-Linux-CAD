# Architecture Summary

This document condenses the implemented architecture and current target direction. Feature-specific documents remain canonical for exact contracts.

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

Current assembly-relevant types include:

- `PartDocument`, `AssemblyDocument`, `Project`
- `ComponentInstance`
- `AssemblyConstraintTarget`, `AssemblyConstraint`
- `AssemblyConstraintGraph`
- `SemanticFaceReference`, `SemanticAxisReference`
- `ComponentLocalPlanarDescriptor`, `ComponentLocalAxisDescriptor`
- `ResolvedAssemblyConstraintTarget`, `ResolvedAssemblyAxisConstraintTarget`
- `AssemblyConstraintTargetResolver`
- `AssemblySpacePlanarDescriptor`, `AssemblySpaceAxisDescriptor`
- `AssemblyTransformEvaluator`
- `PlanarMateResidualDescriptor`, `PlanarDistanceResidualDescriptor`
- `AssemblyConstraintEquationBuilder`
- `ConcentricResidualDescriptor`
- `AssemblyConcentricConstraintEquationDescriptor`
- `AssemblyConcentricConstraintEquationBuilder`
- `AssemblyRigidBodySolver`, `AssemblySolveResult`, `AssemblySolveResultApplier`
- `AssemblySolveDiagnosticsAnalyzer` and rank/DOF descriptors

Planned object families include bodies and body transforms/booleans, path records, 3D sketches, surfacing, richer assembly constraints, joints, and motion.

## Implemented part-model path

The implemented part path includes:

- typed parameters and quantities
- datum planes
- sketches and multiple profile families
- additive and subtractive extrude intent
- dependency graph, invalidation, and recompute planning
- semantic generated faces, edges, and vertices where implemented
- derived workplanes and construction geometry
- projected/reference-driven sketch geometry
- reference recovery helpers
- sketch constraints, dimensions, diagnostics, and repair helpers
- optional OCCT geometry execution through `ShapeCache`
- STEP export
- JSON model-intent serialization

`CircularHolePattern` is also implemented as parametric model intent and is expanded into per-hole cuts during geometry recompute.

## Project and assembly container

`docs/assembly-parameters-mvp4.md` and `docs/project-container-mvp4.md` define:

- assembly-scoped parameters
- member-part registration
- explicit `ParameterBinding` records
- one `Project` owning an assembly and embedded parts
- binding propagation through normal part invalidation
- project-level parameter updates and per-part recompute plans
- project JSON and file helpers

A `ComponentInstance` is an occurrence with identity independent from part-document identity. Repeated occurrences may reference the same project-owned `PartDocument`.

Persisted component state includes visibility, suppression, grounding, and:

```text
RigidTransform
  translation_mm
  rotation_deg
```

Storage-level placement edits are explicit and do not infer constraints or run the solver.

## Implemented assembly relationship architecture

The current pipeline has two derived geometry branches after persistent constraint intent.

Planar Mate/Distance path:

```text
AssemblyConstraint
  -> AssemblyConstraintGraph
  -> AssemblyConstraintTargetResolver::resolve
  -> ComponentLocalPlanarDescriptor
  -> AssemblyTransformEvaluator::evaluate_plane
  -> AssemblySpacePlanarDescriptor
  -> AssemblyConstraintEquationBuilder
  -> Planar Mate/Distance residual descriptor
  -> shared numeric residual/Jacobian system
  -> AssemblyRigidBodySolver
  -> AssemblySolveResult
  -> explicit AssemblySolveResultApplier
  -> AssemblySolveDiagnosticsAnalyzer
```

Concentric semantic path:

```text
AssemblyConstraint target feature.<feature-id>.axis
  -> AssemblyConstraintTargetResolver::resolve_axis
  -> ComponentLocalAxisDescriptor
  -> AssemblyTransformEvaluator::evaluate_axis
  -> AssemblySpaceAxisDescriptor
  -> AssemblyConcentricConstraintEquationBuilder
  -> ConcentricResidualDescriptor
```

The Concentric branch is currently read-only derived geometry/residual interpretation. It is not yet connected to the shared numeric system, rigid-body solver, or DOF diagnostics.

## Persistent constraint intent

`docs/assembly-constraint-model-intent-mvp5.md` is canonical.

Persistent records support Mate, Concentric, and Distance intent with semantic target A/B references, active/inactive state, and a Distance-only positive length value.

The record layer does not interpret geometry, solve placement, or persist derived solver state.

## Deterministic active-constraint graph

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

## Semantic generated planar faces

Supported generated-face assembly target tokens are:

```text
feature.<feature-id>.top
feature.<feature-id>.bottom
feature.<feature-id>.right
feature.<feature-id>.left
feature.<feature-id>.front
feature.<feature-id>.back
```

For supported additive-extrude source features, `AssemblyConstraintTargetResolver::resolve` reuses `WorkplaneResolver::resolve_generated_face` and returns a component-local planar descriptor plus the separate persisted component transform.

## Semantic generated axes

`docs/assembly-semantic-axis-concentric-residuals-mvp5.md` is canonical.

The first stable generated-axis token is:

```text
feature.<feature-id>.axis
```

The first supported producer is a `SubtractiveExtrude` whose input sketch contains exactly one `CircleProfile` and exactly one total profile.

This is aligned with the existing circular-cut recompute path. The local axis is derived from:

```text
origin    = CircleProfile.center mapped through the resolved sketch workplane
direction = source workplane normal, reversed for OppositeSketchNormal
```

The resolver preserves component, part, feature, and source-profile identity.

`CircularHolePattern` is deliberately not mapped to one `.axis` token because it produces several distinct hole axes. Pattern-axis instance identity requires a separate stable semantic token family.

## Explicit rigid-transform evaluation

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

Points are rotated then translated. Vectors, normals, and axis directions are rotated only.

The evaluator exposes distinct assembly-space plane and axis descriptor types so coordinate space and geometry family remain explicit.

## Planar Mate and Distance residuals

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

Signed Distance semantics are target-order dependent.

The planar builder remains Mate/Distance-specific and is the builder used by the current numeric system.

## Concentric residual semantics

The dedicated `AssemblyConcentricConstraintEquationBuilder` consumes active Concentric records and resolves both targets through the semantic-axis path.

For axis lines `(oA,dA)` and `(oB,dB)`:

```text
direction_parallelism = cross(dA, dB)
axis_offset_mm         = cross(oB - oA, dA)
```

The first residual aligns axis directions while accepting equal or opposed orientation.

The second removes axial origin separation and measures lateral line offset in millimeter-valued vector form.

A regular Concentric relationship intentionally leaves translation along the common axis and rotation about the common axis free.

Raw offset vectors are target-order/direction dependent; target A/B order remains explicit model intent.

## Shared assembly numeric system

The solver and DOF diagnostics share:

```text
src/geometry/assembly_constraint_numeric_system.hpp
src/geometry/assembly_constraint_numeric_system.cpp
```

The current shared numeric system owns:

- deterministic constraint-id collection
- Mate/Distance residual flattening
- residual RMS and maximum absolute value
- free-component variable extraction
- numeric variable application to project copies
- central finite-difference Jacobian construction

Each free component contributes:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

Each current planar constraint contributes three orientation components and one scaled length component.

Concentric residual descriptors are implemented but are not yet flattened by this shared numeric system.

## Rigid-body solver

`docs/assembly-rigid-body-solver-mvp5.md` is canonical.

Participation policy:

- solve exactly one deterministic graph connected group
- require at least one grounded component
- treat every grounded component as fixed
- allow multiple grounded components
- reject suppressed group components
- ignore visibility for solve participation

The first numeric method is deterministic damped Gauss-Newton using a central finite-difference Jacobian, damped normal equations, partial-pivot Gaussian elimination, backtracking line search, and damping escalation.

The solver mutates only private `Project` copies and returns proposed transforms.

`AssemblySolveResultApplier` is the only explicit successful-result transform-application boundary. It checks complete source snapshots, rejects stale results including moved grounded anchors, validates all proposals, and applies atomically through a project copy.

The current solver supports Mate and Distance. Concentric solver integration is not implemented yet.

## Local Jacobian-rank and remaining-DOF diagnostics

`docs/assembly-solve-diagnostics-mvp5.md` is canonical.

For a converged group, diagnostics evaluate the exact shared numeric system on a private project copy.

The pivot threshold is:

```text
max(rank_absolute_tolerance,
    rank_relative_tolerance * maximum_abs_jacobian_entry)
```

For free solver variables:

```text
variable_count  = 6 * free_component_count
constrained_dof = rank(J)
remaining_dof   = variable_count - rank(J)
```

DOF, consistency, and residual-rank structure are separate classifications.

Redundant residual rows are not automatically semantic overconstraint.

The result is local and linearized in direct `RigidTransform` coordinates; it is not a global configuration-space dimension or singularity analysis.

Because the shared numeric system is still planar-only, Concentric rank and remaining-DOF diagnostics are not yet evaluated. The expected regular one-free-body Concentric rank is four over six variables and must be proved by the next integration block.

## Persistence boundary

Persisted model intent includes part/assembly/project records, component state/transforms, and assembly constraints with semantic target strings.

The following remain regenerable derived data and are not serialized:

- constraint graph connectivity
- resolved planar and axis target descriptors
- evaluated assembly-space planes and axes
- Mate, Distance, and Concentric residual descriptors
- flattened numeric residual vectors
- numeric Jacobians and normal equations
- solve iterations, damping state, and solve results
- proposed transforms before application
- residual summaries
- rank summaries and remaining-DOF diagnostics

`feature.hole.axis` uses the already-existing semantic target string field. No new JSON shape is required for this block.

Only explicitly applied successful transform proposals update persisted component `RigidTransform` intent.

## Next assembly block

The next core CAD block is Concentric integration into the shared numeric residual/Jacobian system, rigid-body solver, and DOF diagnostics.

The integration must:

- select the dedicated Concentric builder for Concentric records
- preserve current Mate/Distance flattening
- flatten Concentric direction residuals before three millimeter-scaled axis-offset residuals
- reuse current variable ordering and finite-difference Jacobian construction
- preserve all grounding, suppression, solve-state, snapshot, stale-result, and atomic-application contracts
- solve lateral offset and tilt
- preserve axial translation and rotation-about-axis freedom
- prove local Jacobian rank four and two remaining DOF for a regular one-free-body Concentric relationship

Insert remains downstream because it adds axial seating semantics to Concentric alignment.

## Broader future direction

Multi-body transforms/booleans and path features are documented in `docs/multi-body-transform-and-path-features-roadmap.md`.

Inventor-like sketch/feature parity is documented in `docs/inventor-like-sketcher-and-feature-roadmap.md`.

3D sketching and surfacing are documented in `docs/advanced-surfacing-and-3d-sketch-mvp.md`.

Future assembly work includes Insert and richer constraints, null-space/per-component DOF presentation, joints and limits, motion, subassemblies, collision/interference checks, component geometry instancing, and assembly-level STEP export.

## Critical architecture rules

- Parameters and features are first-class model intent.
- Recompute runs through dependency relationships.
- Persistent geometry references are semantic rather than raw OCCT topology identifiers.
- User-created construction geometry is model intent.
- OCCT shapes are cache data.
- The optional OCCT path lives in `blcad_geometry`; `PartDocument` remains OCCT-free.
- Component instances reference shared part intent without duplicating owned parts.
- Component-local and assembly-space descriptors use distinct types.
- Planar and axis target resolution are explicit APIs.
- `rotation_deg` has fixed right-handed X-then-Y-then-Z semantics.
- Target A/B order is preserved when residual conventions depend on it.
- Solver and diagnostics share one numeric interpretation.
- Solving is read-only until explicit successful result application.
- Rank diagnostics are local linear algebra, not semantic overconstraint inference.
- Derived geometry, residuals, Jacobians, solver products, and DOF diagnostics remain unpersisted while safely regenerable.
- GUI code must not own CAD logic.

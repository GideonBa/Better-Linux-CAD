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

## Current assembly object families

Implemented assembly-relevant types include:

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
- private shared assembly numeric residual/Jacobian system
- `AssemblyRigidBodySolver`, `AssemblySolveResult`, `AssemblySolveResultApplier`
- `AssemblySolveDiagnosticsAnalyzer` and rank/DOF descriptors

Planned object families include stable Insert intent/residual types, richer constraints, bodies and body transforms/booleans, path records, 3D sketches, surfacing, joints, and motion.

## Implemented part-model path

The implemented part path includes:

- typed parameters and quantities
- datum planes
- sketches and multiple profile families
- additive and subtractive extrude intent
- dependency graph, invalidation, and recompute planning
- semantic generated faces, edges, vertices, and the first generated axis family
- derived workplanes and construction geometry
- projected/reference-driven sketch geometry
- reference recovery helpers
- sketch constraints, dimensions, diagnostics, and repair helpers
- optional OCCT geometry execution through `ShapeCache`
- STEP export
- JSON model-intent serialization

`CircularHolePattern` is implemented as parametric model intent and expanded into per-hole cuts during geometry recompute.

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

Persistent intent branches into geometry-family-specific residual builders and then rejoins one shared numeric path.

```text
AssemblyConstraint + ComponentInstance intent
  -> AssemblyConstraintGraph
  -> graph-ordered active AssemblyConstraintId records

planar target
  -> AssemblyConstraintTargetResolver::resolve
  -> ComponentLocalPlanarDescriptor
  -> AssemblyTransformEvaluator::evaluate_plane
  -> AssemblySpacePlanarDescriptor
  -> AssemblyConstraintEquationBuilder
  -> Mate or Distance residual descriptor

axis target
  -> AssemblyConstraintTargetResolver::resolve_axis
  -> ComponentLocalAxisDescriptor
  -> AssemblyTransformEvaluator::evaluate_axis
  -> AssemblySpaceAxisDescriptor
  -> AssemblyConcentricConstraintEquationBuilder
  -> ConcentricResidualDescriptor

all supported residual families
  -> shared assembly numeric residual/Jacobian system
  -> AssemblyRigidBodySolver
  -> AssemblySolveResult
  -> explicit AssemblySolveResultApplier
  -> AssemblySolveDiagnosticsAnalyzer
```

The geometry branches remain explicit. The numeric, solver, application, and diagnostic layers are shared.

## Persistent constraint intent

`docs/assembly-constraint-model-intent-mvp5.md` is canonical.

Persistent records currently support Mate, Concentric, and Distance intent with semantic target A/B strings, active/inactive state, and a Distance-only positive length value.

The record layer does not interpret geometry, solve placement, or persist derived solver state.

Target A/B order is persistent intent.

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

Exact connected groups are the rigid-body solver partition boundary.

The graph does not interpret planar versus axis geometry.

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

Canonical document: `docs/assembly-semantic-axis-concentric-residuals-mvp5.md`.

The first stable generated-axis token is:

```text
feature.<feature-id>.axis
```

The first supported producer is a `SubtractiveExtrude` whose input sketch contains exactly one `CircleProfile` and exactly one total profile.

The local axis is derived from:

```text
origin    = CircleProfile.center mapped through the resolved sketch workplane
direction = source workplane normal, reversed for OppositeSketchNormal
```

The resolver preserves component, part, feature, and source-profile identity.

`CircularHolePattern` is not mapped to one `.axis` token because it produces several distinct hole axes. Pattern-axis identity requires a stable per-instance semantic reference family.

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

Points and geometry origins are rotated then translated. Vectors, normals, and axis directions are rotated only.

Distinct component-local and assembly-space descriptor types keep coordinate space explicit.

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

The planar builder remains Mate/Distance-specific.

## Concentric residual semantics

Canonical document: `docs/assembly-semantic-axis-concentric-residuals-mvp5.md`.

For axis lines `(oA,dA)` and `(oB,dB)`:

```text
direction_parallelism = cross(dA, dB)
axis_offset_mm         = cross(oB - oA, dA)
```

Equal and opposed axis directions are accepted.

The offset residual removes axial origin separation and measures only lateral line offset.

A regular Concentric relationship intentionally leaves translation along the common axis and rotation about the common axis free.

Raw offset vectors are target-order/direction dependent.

## Shared assembly numeric residual/Jacobian system

Canonical integration document: `docs/assembly-concentric-numeric-solver-dof-mvp5.md`.

The private geometry-layer numeric implementation remains:

```text
src/geometry/assembly_constraint_numeric_system.hpp
src/geometry/assembly_constraint_numeric_system.cpp
```

It owns the single numeric interpretation used by solver and diagnostics:

- deterministic graph constraint-id collection
- constraint-type residual-builder selection
- Mate/Distance/Concentric scalar flattening
- residual RMS and maximum absolute value
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

Constraint records are consumed in lexicographic `AssemblyConstraintId` order. Persistent target A/B order remains unchanged inside each constraint.

Exact residual flattening:

```text
Mate:
  normal_opposition.x
  normal_opposition.y
  normal_opposition.z
  signed_separation_mm / length_residual_scale_mm

Distance:
  normal_parallelism.x
  normal_parallelism.y
  normal_parallelism.z
  distance_residual_mm / length_residual_scale_mm

Concentric:
  direction_parallelism.x
  direction_parallelism.y
  direction_parallelism.z
  axis_offset_mm.x / length_residual_scale_mm
  axis_offset_mm.y / length_residual_scale_mm
  axis_offset_mm.z / length_residual_scale_mm
```

Mate and Distance contribute four scalar rows. Concentric contributes six.

Default length scale remains `1.0 mm`.

The central finite-difference path re-resolves semantic geometry and rebuilds residuals for every plus/minus perturbation.

## Rigid-body solver

`docs/assembly-rigid-body-solver-mvp5.md` is canonical.

Participation policy:

- solve exactly one deterministic graph connected group
- require at least one grounded component
- treat every grounded component as fixed
- allow multiple grounded components
- reject suppressed group components
- ignore visibility for solve participation

The solver uses deterministic damped Gauss-Newton with a central finite-difference Jacobian, damped normal equations, partial-pivot Gaussian elimination, backtracking line search, and damping escalation.

The solver mutates only private `Project` copies and returns proposed transforms.

`AssemblySolveResultApplier` is the explicit successful-result transform-application boundary. It validates complete source snapshots, rejects stale results including moved grounded anchors, validates all proposals, and applies atomically through a project copy.

Mate, Distance, and Concentric all participate through the same numeric path. The solver has no Concentric-specific optimization branch.

## Concentric solve behavior

The shared solver now corrects:

```text
lateral axis offset
axis tilt
```

The residual model intentionally does not constrain:

```text
translation along the common axis
rotation about the common axis
```

Equal and opposed coincident axis directions are both valid initial Concentric states.

All-grounded inconsistent Concentric geometry produces `FixedGeometryInconsistent`.

Non-converged Concentric results remain read-only and cannot be applied.

## Local Jacobian-rank and remaining-DOF diagnostics

`docs/assembly-solve-diagnostics-mvp5.md` is canonical.

For a converged group, diagnostics evaluate the exact same shared numeric system and central finite-difference Jacobian on a private project copy.

Pivot threshold:

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

Regular one-free-body Concentric result:

```text
residual_component_count = 6
variable_count           = 6
jacobian_rank            = 4
constrained_dof          = 4
remaining_dof            = 2
residual_row_redundancy  = 2
```

Regular Distance plus Concentric result when the planar normal follows the common axis:

```text
residual_component_count = 10
variable_count           = 6
jacobian_rank            = 5
remaining_dof            = 1
```

These values are measured from the shared finite-difference Jacobian rather than inferred from constraint type names.

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

`feature.hole.axis` uses the existing semantic target string field.

Only explicit application of a fresh converged solve result updates persisted component `RigidTransform` intent.

## Next assembly block

The next core CAD block is stable Insert constraint intent and a read-only composite Insert residual model.

Insert must first define semantic axial-seating geometry for supported circular features. The target contract must provide both stable axis alignment intent and stable seating-plane intent without raw OCCT topology ids.

The first Insert residual model should preserve Concentric's four independent regular axis-line directions and add one independent signed axial seating direction. The expected regular local rank is therefore five over six rigid-body variables, leaving only rotation about the common axis free.

Insert numeric-system and solver participation should remain a separate following block after those target and residual semantics are stable.

## Broader future direction

Multi-body transforms/booleans and path features are documented in `docs/multi-body-transform-and-path-features-roadmap.md`.

Inventor-like sketch/feature parity is documented in `docs/inventor-like-sketcher-and-feature-roadmap.md`.

3D sketching and surfacing are documented in `docs/advanced-surfacing-and-3d-sketch-mvp.md`.

Future assembly work includes Insert solver integration, richer constraints, null-space/per-component DOF presentation, joints and limits, motion, subassemblies, collision/interference checks, component geometry instancing, and assembly-level STEP export.

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
- Geometry-family residual builders remain explicit.
- Solver and diagnostics share one numeric residual/Jacobian interpretation.
- Solving is read-only until explicit successful result application.
- Rank diagnostics are local linear algebra, not semantic overconstraint inference.
- Derived geometry, residuals, Jacobians, solver products, and DOF diagnostics remain unpersisted while safely regenerable.
- GUI code must not own CAD logic.

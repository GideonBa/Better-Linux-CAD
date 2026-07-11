# Architecture Summary

This document condenses the implemented architecture and current target direction. Feature-specific documents remain canonical for exact contracts.

## Goal

BLCAD is an independent parametric CAD system for Linux. The model stores design intent rather than only final BRep geometry: parameters, sketches, features, dependencies, semantic references, construction geometry, assembly structure, and later multi-body, path, surfacing, and engineering intent.

The long-term goal is recorded in `docs/project-goal.md`.

## Fundamental decisions

- do not fork FreeCAD
- use OCCT as the geometry kernel
- keep a custom CAD core above OCCT
- treat OCCT shapes as computed cache data, not primary model intent
- persist semantic model references, not raw OCCT topology ids
- keep GUI code above the CAD core; UI state must not become geometry or solver authority

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
- `SemanticFaceReference`, `SemanticAxisReference`, `SemanticSeatingPlaneReference`
- `ComponentLocalPlanarDescriptor`, `ComponentLocalAxisDescriptor`
- `ResolvedAssemblyConstraintTarget`, `ResolvedAssemblyAxisConstraintTarget`
- `ResolvedAssemblyInsertConstraintTarget`
- `AssemblyConstraintTargetResolver`
- `AssemblySpacePlanarDescriptor`, `AssemblySpaceAxisDescriptor`
- `AssemblyTransformEvaluator`
- `PlanarMateResidualDescriptor`, `PlanarDistanceResidualDescriptor`
- `AssemblyConstraintEquationBuilder`
- `ConcentricResidualDescriptor`, `AssemblyConcentricConstraintEquationBuilder`
- `InsertResidualDescriptor`, `AssemblyInsertConstraintEquationBuilder`
- private shared assembly numeric residual/Jacobian system
- `AssemblyRigidBodySolver`, `AssemblySolveResult`, `AssemblySolveResultApplier`
- `AssemblySolveDiagnosticsAnalyzer` and rank/DOF descriptors

Planned object families include richer constraints, bodies and body transforms/booleans, path records, 3D sketches, surfacing, joints, and motion.

## Implemented part-model path

The implemented part path includes typed quantities/parameters, datum planes, sketches and multiple profile families, additive/subtractive extrude intent, dependency/invalidation/recompute planning, semantic references, derived workplanes, construction geometry, projected/reference-driven sketch geometry, reference recovery, sketch constraints/diagnostics/repair helpers, optional OCCT execution through `ShapeCache`, STEP export, and JSON model-intent serialization.

`CircularHolePattern` is parametric model intent and expands into per-hole cuts during geometry recompute.

## Project and assembly container

`Project` owns one assembly and project-owned part documents. A `ComponentInstance` is an occurrence with identity independent from part-document identity. Repeated occurrences may reference the same `PartDocument` while retaining independent placement/state.

Persisted component state includes:

```text
visibility
suppression
 grounding
RigidTransform
  translation_mm
  rotation_deg
```

Storage-level placement edits are explicit and solver-independent.

## Persistent constraint intent

Canonical document: `docs/assembly-constraint-model-intent-mvp5.md`.

Persistent relationship families are:

```text
Mate
Concentric
Distance
Insert
```

Every record preserves target A/B semantic strings and active/inactive state. Distance alone carries a positive length quantity.

The record layer does not resolve geometry, evaluate transforms, construct residuals, solve placement, or persist derived solver data.

## Deterministic relationship graph

`AssemblyConstraintGraph` contains every component as a node and every active constraint as one distinct edge. It excludes inactive constraints, preserves multi-edges, sorts deterministically, and exposes exact connected groups as current solve partition boundaries.

The graph is geometry-family agnostic. Insert uses the same relationship edge form as Mate, Distance, and Concentric.

## Semantic target families

Generated planar-face targets:

```text
feature.<feature-id>.top
feature.<feature-id>.bottom
feature.<feature-id>.right
feature.<feature-id>.left
feature.<feature-id>.front
feature.<feature-id>.back
```

Primary circular-feature axis target:

```text
feature.<feature-id>.axis
```

Primary circular-feature seating target:

```text
feature.<feature-id>.seat
```

The first `.axis`/`.seat` producer is one `SubtractiveExtrude` whose source sketch contains exactly one `CircleProfile` and exactly one total profile.

The resolver preserves component, referenced part, source feature, and source profile identity. No raw OCCT topology id is persistent target identity.

## Composite Insert endpoint

Canonical document: `docs/assembly-insert-intent-composite-residuals-mvp5.md`.

One persisted Insert endpoint stores one `.seat` target string. `AssemblyConstraintTargetResolver::resolve_insert` derives a composite endpoint from that exact circular feature/profile:

```text
primary axis
  origin = mapped CircleProfile.center
  direction = source workplane normal adjusted by ExtrudeDirection

seating plane
  origin = primary axis origin
  normal = primary axis direction
  right-handed basis preserved for OppositeSketchNormal
```

The local axis, local seating plane, and component transform remain distinct derived values.

There is no hidden second target and no four-target Insert record.

## Explicit rigid-transform evaluation

Persisted `RigidTransform` semantics are:

```text
translation: millimeters
rotation: degrees
positive rotation: right-hand rule
rotation type: active fixed-axis
application order: X, then Y, then Z
column-vector composition: Rz * Ry * Rx
```

Points/origins rotate then translate. Vectors, normals, and axis directions rotate only.

Insert reuses the existing evaluator twice:

```text
local axis -> evaluate_axis -> assembly-space axis
local seat -> evaluate_plane -> assembly-space seating plane
```

No Insert-specific transform convention exists.

## Geometry-family residual branches

Persistent constraints branch into geometry-specific builders before supported numeric families rejoin one shared numeric path.

```text
planar Mate/Distance target
  -> resolve
  -> evaluate_plane
  -> AssemblyConstraintEquationBuilder

Concentric axis target
  -> resolve_axis
  -> evaluate_axis
  -> AssemblyConcentricConstraintEquationBuilder

Insert seat target
  -> resolve_insert
  -> evaluate_axis + evaluate_plane
  -> AssemblyInsertConstraintEquationBuilder
```

### Mate

```text
normal_opposition    = nA + nB
signed_separation_mm = dot(oB - oA, nA)
```

### Distance

```text
normal_parallelism   = cross(nA, nB)
signed_separation_mm = dot(oB - oA, nA)
distance_residual_mm = signed_separation_mm - target_distance_mm
```

### Concentric

```text
direction_parallelism = cross(dA, dB)
axis_offset_mm         = cross(oB - oA, dA)
```

### Insert

```text
direction_parallelism       = cross(dA, dB)
axis_offset_mm               = cross(oB - oA, dA)
signed_seating_separation_mm = dot(sB - sA, nA)
```

For Insert, `nA` is canonically aligned with target A's semantic axis direction. Target A defines the signed seating direction.

No separate seating-normal residual is added because axis-direction parallelism already owns the two regular tilt constraints.

## Shared numeric residual/Jacobian system

Canonical Concentric integration document: `docs/assembly-concentric-numeric-solver-dof-mvp5.md`.

The private shared numeric system currently consumes:

```text
Mate
Distance
Concentric
```

It owns deterministic graph constraint ordering, constraint-family builder selection, scalar residual flattening, residual summaries, free-component variable extraction/application on project copies, and central finite-difference Jacobian construction.

Each free component contributes:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

Current exact flattening:

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

Insert residual construction is implemented but Insert is deliberately not flattened by the shared numeric system yet.

## Rigid-body solver and application

`AssemblyRigidBodySolver` solves exactly one deterministic connected group, requires at least one grounded component, fixes every grounded component, rejects selected suppressed components, and ignores visibility for participation.

It uses deterministic damped Gauss-Newton with central finite differences, damped normal equations, partial-pivot Gaussian elimination, backtracking line search, and damping escalation.

Mate, Distance, and Concentric use this one path. No Concentric-specific optimizer exists.

The solver mutates private `Project` copies only and returns explicit transform proposals plus complete solve-input snapshots.

`AssemblySolveResultApplier` accepts only converged results, rejects stale snapshots including moved grounded anchors, validates proposals, and applies atomically through another project copy.

Insert currently reaches an explicit unsupported numeric boundary:

```text
Insert equation construction requires dedicated composite target support
```

The source project remains unchanged.

## Local Jacobian rank and remaining DOF

`AssemblySolveDiagnosticsAnalyzer` evaluates the exact shared numeric Jacobian at a converged private solve state.

```text
variable_count  = 6 * free_component_count
constrained_dof = rank(J)
remaining_dof   = variable_count - rank(J)
```

DOF, consistency, and residual-row rank structure are separate classifications. Redundant scalar residual rows are not automatically semantic overconstraint.

Proven Concentric regular result:

```text
residual_component_count = 6
variable_count           = 6
jacobian_rank            = 4
remaining_dof            = 2
```

The implemented read-only Insert test independently central-finite-differences its seven scalar residuals over all six direct transform variables and proves:

```text
residual_component_count = 7
variable_count           = 6
jacobian_rank            = 5
remaining_local_dof      = 1
```

The remaining regular Insert freedom is rotation about the common axis.

This Insert rank proof is not yet produced by `AssemblySolveDiagnosticsAnalyzer`; shared numeric integration is the next block.

## Persistence boundary

Persisted model intent includes project/part/assembly records, component placement/state, and assembly constraints with semantic target strings.

Derived and unpersisted data includes:

- constraint graph connectivity
- resolved local plane/axis/seat descriptors
- assembly-space plane/axis/seat geometry
- Mate/Distance/Concentric/Insert residual descriptors
- flattened numeric residual vectors
- numeric Jacobians and normal equations
- solver iteration state and solve results
- unapplied transform proposals
- residual summaries
- rank summaries and remaining-DOF diagnostics

`feature.hole.seat` uses the existing semantic-reference string field. `Insert` uses the existing constraint type string field. No new JSON field is required.

Only explicit application of a fresh converged solve result changes persisted component `RigidTransform` intent.

## Next assembly block

The next core assembly block is **Insert numeric-system, rigid-body solver, explicit application, and DOF-diagnostics integration**.

The shared numeric evaluator should route Insert to `AssemblyInsertConstraintEquationBuilder` and flatten exactly:

```text
direction_parallelism.x
direction_parallelism.y
direction_parallelism.z
axis_offset_mm.x / length_residual_scale_mm
axis_offset_mm.y / length_residual_scale_mm
axis_offset_mm.z / length_residual_scale_mm
signed_seating_separation_mm / length_residual_scale_mm
```

All existing ordering, finite-difference, damping, grounding, suppression, solve-state, snapshot, stale-result, and atomic-application contracts must remain unchanged. The regular integrated Insert system must be proven as rank `5/6` with one remaining rotation-about-axis DOF through the shared diagnostics path.

## Broader future direction

Multi-body transforms/booleans and path features are documented in `docs/multi-body-transform-and-path-features-roadmap.md`.

Inventor-like sketch/feature parity is documented in `docs/inventor-like-sketcher-and-feature-roadmap.md`.

3D sketching and surfacing are documented in `docs/advanced-surfacing-and-3d-sketch-mvp.md`.

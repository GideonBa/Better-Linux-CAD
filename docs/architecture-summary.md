# Architecture Summary

This document condenses the implemented architecture and current target direction. Feature-specific documents remain canonical for exact contracts.

## Goal

BLCAD is an independent parametric CAD system for Linux. The model stores design intent rather than only final BRep geometry: parameters, sketches, features, dependencies, semantic references, construction geometry, assembly structure, and later multi-body, path, surfacing, joint, motion, and engineering intent.

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
- `ComponentInstance`, `RigidTransform`
- `AssemblyConstraintTarget`, `AssemblyConstraint`
- `AssemblyConstraintGraph`
- `SemanticFaceReference`, `SemanticAxisReference`, `SemanticSeatingPlaneReference`
- `ComponentLocalPlanarDescriptor`, `ComponentLocalAxisDescriptor`
- `ResolvedAssemblyConstraintTarget`, `ResolvedAssemblyAxisConstraintTarget`
- `ResolvedAssemblyInsertConstraintTarget`
- `AssemblyConstraintTargetResolver`
- `AssemblySpacePlanarDescriptor`, `AssemblySpaceAxisDescriptor`
- `AssemblyTransformEvaluator`
- `PlanarMateResidualDescriptor`, `PlanarDistanceResidualDescriptor`, `PlanarAngleResidualDescriptor`
- `ConcentricResidualDescriptor`, `InsertResidualDescriptor`
- `AssemblyConstraintEquationBuilder`
- `AssemblyConcentricConstraintEquationBuilder`
- `AssemblyInsertConstraintEquationBuilder`
- private shared assembly numeric residual/Jacobian system
- `AssemblyRigidBodySolver`, `AssemblySolveResult`, `AssemblySolveResultApplier`
- `AssemblySolveDiagnosticsAnalyzer` and rank/DOF descriptors
- `AssemblyStepExporter`, `AssemblyStepExportSummary`

Planned object families include joints and limits, motion inputs, bodies and body transforms/booleans, path records, 3D sketches, surfacing, subassemblies, and collision/interference analysis.

## Part and project model

The implemented part path includes typed quantities/parameters, datum planes, sketches and multiple profile families, additive/subtractive extrude intent, dependency/invalidation/recompute planning, semantic references, derived workplanes, construction geometry, projected/reference-driven sketch geometry, reference recovery, sketch constraints/diagnostics/repair helpers, optional OCCT execution through `ShapeCache`, STEP export, and JSON model-intent serialization.

`CircularHolePattern` is parametric model intent and expands into per-hole cuts during geometry recompute.

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
Angle
```

Every record preserves target A/B semantic strings and active/inactive state. Distance carries a positive length quantity. Angle carries a finite degree quantity. Mate, Concentric, and Insert are value-free relationship records.

The record layer does not resolve geometry, evaluate transforms, construct residuals, solve placement, or persist derived solver data.

## Deterministic relationship graph

`AssemblyConstraintGraph` contains every component as a node and every active assembly constraint as one distinct edge. It excludes inactive constraints, preserves multi-edges, sorts deterministically, and exposes exact connected groups as current solve partition boundaries.

The graph is geometry-family agnostic. Mate, Distance, Concentric, Insert, and Angle use the same relationship edge form.

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

The local axis, local seating plane, and component transform remain distinct derived values. There is no hidden second target and no four-target Insert record.

## Rigid-transform evaluation

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

The exact same convention is reused by posed assembly export. `AssemblyStepExporter` applies explicit OCCT X, Y, and Z rotations in that order, then translation. There is no export-specific placement convention.

## Geometry-family residual branches

Persistent constraints branch into geometry-specific builders before all currently supported numeric families rejoin one shared numeric path.

```text
planar Mate/Distance/Angle target
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

For Insert, `nA` is canonically aligned with target A's semantic axis direction. Target A defines the signed seating direction. No separate seating-normal residual is added because axis-direction parallelism already owns the two regular tilt constraints.

### Angle

```text
angle_alignment = dot(nA, nB) - cos(target_angle_deg)
```

The current cosine seed is direction-symmetric. At `0°` and `180°`, the derivative vanishes at the solved state and local rank is reported accordingly. Oriented signed angles are deferred.

## Shared numeric residual/Jacobian system

The private shared numeric system currently consumes:

```text
Mate
Distance
Concentric
Insert
Angle
```

It owns deterministic graph constraint ordering, constraint-family builder selection, scalar residual flattening, residual summaries, free-active-component variable extraction/application on project copies, and central finite-difference Jacobian construction.

Each free active component contributes:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

Exact current flattening:

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

Insert:
  direction_parallelism.x
  direction_parallelism.y
  direction_parallelism.z
  axis_offset_mm.x / length_residual_scale_mm
  axis_offset_mm.y / length_residual_scale_mm
  axis_offset_mm.z / length_residual_scale_mm
  signed_seating_separation_mm / length_residual_scale_mm

Angle:
  angle_alignment
```

## Rigid-body solver, suppression, and application

`AssemblyRigidBodySolver` solves exactly one deterministic connected group. The active numeric subgroup excludes suppressed components and every constraint touching them. Visibility does not affect solving.

If constraints survive suppression, at least one active component must be grounded. Every grounded active component is fixed. Every free active component contributes six direct persisted-transform coordinates.

A group whose constraints all vanish through suppression is trivially converged with zero residual components. Complete component snapshots still include suppressed members, so suppressing or unsuppressing after solve makes the result stale.

The solver uses deterministic damped Gauss-Newton with central finite differences, damped normal equations, partial-pivot Gaussian elimination, backtracking line search, and damping escalation.

The solver mutates private `Project` copies only and returns explicit transform proposals plus complete solve-input snapshots.

`AssemblySolveResultApplier` accepts only converged results, rejects stale snapshots including moved grounded anchors and changed suppression states, validates proposals against free active snapshots, and applies atomically through another project copy.

## Local Jacobian rank and remaining DOF

`AssemblySolveDiagnosticsAnalyzer` evaluates the exact shared numeric Jacobian at a converged private solve state over the same active subgroup as the solver.

```text
variable_count  = 6 * free_active_component_count
constrained_dof = rank(J)
remaining_dof   = variable_count - rank(J)
```

DOF, consistency, and residual-row rank structure are separate classifications. Redundant scalar residual rows are not automatically semantic overconstraint.

Proven regular one-free-body results include:

```text
Concentric:
  residual_component_count = 6
  variable_count           = 6
  jacobian_rank            = 4
  remaining_dof            = 2

Insert:
  residual_component_count = 7
  variable_count           = 6
  jacobian_rank            = 5
  remaining_dof            = 1

Angle away from extremal targets:
  residual_component_count = 1
  variable_count           = 6
  jacobian_rank            = 1
  remaining_dof            = 5
```

The remaining regular Insert freedom is rotation about the common axis.

## Posed assembly STEP export

Canonical document: `docs/assembly-posed-step-export-mvp5.md`.

`AssemblyStepExporter` is the first derived end-to-end assembly geometry consumer.

```text
const Project
  -> validate assembly structure
  -> collect referenced parts in lexicographic id order
  -> recompute each referenced part once into one per-export ShapeCache
  -> reuse that cache for repeated occurrences
  -> collect visible active components in lexicographic id order
  -> copy and pose each final part shape with persisted RigidTransform semantics
  -> compose one OCCT compound
  -> existing StepExporter
  -> STEP file
```

Hidden and suppressed components are omitted from the compound. Missing project members, failed recomputes, missing final shapes, transform failures, empty visible-active output, and STEP writer failures stop the export.

The current seed exports one geometric compound. Structured STEP assembly product hierarchy, occurrence naming in STEP, and geometry instancing are deferred.

`blcad_export_posed_assembly` proves the current end-to-end path by loading project JSON, deriving the active constraint graph, solving one connected group, explicitly applying the converged result, and exporting the posed compound.

## Persistence boundary

Persisted model intent includes project/part/assembly records, component placement/state, assembly constraints with semantic target strings, and their explicit quantity values where applicable.

Derived and unpersisted data includes:

- constraint graph connectivity
- resolved local plane/axis/seat descriptors
- assembly-space plane/axis/seat geometry
- Mate/Distance/Concentric/Insert/Angle residual descriptors
- flattened numeric residual vectors
- numeric Jacobians and normal equations
- solver iteration state and solve results
- unapplied transform proposals
- residual summaries
- rank summaries and remaining-DOF diagnostics
- per-export part `ShapeCache` instances
- transformed component BRep copies
- posed assembly OCCT compounds
- assembly STEP export summaries

Only explicit application of a fresh converged solve result changes persisted component `RigidTransform` intent.

## Next assembly block

The next core assembly block is **joint/limit model intent and the first motion-capable solve seed**.

The first implementation should define persistent solver-independent joint identity, semantic endpoints, active state, and explicit limit ranges. A minimal revolute joint is the preferred seed because its free coordinate aligns naturally with the already-proven rotation-about-axis freedom of Concentric/Insert relationships.

Joint connectivity, resolved endpoints, Jacobians, null-space information, and solve state must remain derived. The motion boundary should accept an explicit requested joint coordinate, evaluate limits with documented clamp-or-reject semantics, reuse deterministic solver/application contracts, and return transform proposals rather than silently mutating a source project.

## Broader future direction

Multi-body transforms/booleans and path features are documented in `docs/multi-body-transform-and-path-features-roadmap.md`.

Inventor-like sketch/feature parity is documented in `docs/inventor-like-sketcher-and-feature-roadmap.md`.

3D sketching and surfacing are documented in `docs/advanced-surfacing-and-3d-sketch-mvp.md`.

# Architecture Summary

This document condenses the implemented architecture and current target direction. Feature-specific documents remain canonical for exact contracts.

## Goal

BLCAD is an independent parametric CAD system for Linux. The model stores design intent rather than only final BRep geometry: parameters, sketches, features, dependencies, semantic references, construction geometry, assembly structure, joint/limit intent, and later multi-body, path, surfacing, hierarchical assembly, motion-study, and engineering intent.

The long-term goal is recorded in `docs/project-goal.md`.

## Fundamental decisions

- do not fork FreeCAD;
- use OCCT as the geometry kernel;
- keep a custom CAD core above OCCT;
- treat OCCT shapes as computed cache data, not primary model intent;
- persist semantic model references, not raw OCCT topology ids;
- keep geometric assembly constraints separate from motion-joint intent;
- keep GUI code above the CAD core; UI state must not become geometry or solver authority.

## Layers

```text
user interface
command/application layer
parametric core
sketch/constraint layer
construction geometry and datum relations
multi-body part layer
3D sketch and surfacing layer
assembly relationship and motion layer
engineering modules
OCCT geometry kernel
file and exchange formats
```

## Current core/project model

The implemented part path includes typed quantities/parameters, datum planes, multiple sketch/profile families, additive/subtractive extrude intent, dependency/invalidation/recompute planning, semantic references, derived workplanes, construction geometry, projected/reference-driven sketch geometry, reference recovery, sketch constraints/diagnostics/repair helpers, optional OCCT execution through `ShapeCache`, STEP export, and JSON model-intent serialization.

`CircularHolePattern` is parametric model intent and expands into per-hole cuts during geometry recompute.

`Project` currently owns one root `AssemblyDocument` and project-owned `PartDocument` records. A `ComponentInstance` is an occurrence with identity independent from part-document identity. Repeated occurrences may reference one part while retaining independent placement/state.

Persisted component occurrence state is:

```text
visibility
suppression
grounding
RigidTransform
  translation_mm
  rotation_deg
```

Storage-level placement edits are explicit and solver-independent.

## Current assembly object families

Implemented assembly-relevant types include:

- `PartDocument`, `AssemblyDocument`, `Project`;
- `ComponentInstance`, `RigidTransform`;
- `AssemblyConstraintTarget`, `AssemblyConstraint`;
- `AssemblyConstraintGraph`;
- `AssemblyJoint`, `AssemblyJointLimits`, `AssemblyJointId`;
- `AssemblyJointGraph`;
- `SemanticFaceReference`, `SemanticAxisReference`, `SemanticSeatingPlaneReference`;
- `ComponentLocalPlanarDescriptor`, `ComponentLocalAxisDescriptor`;
- `ResolvedAssemblyConstraintTarget`, `ResolvedAssemblyAxisConstraintTarget`;
- `ResolvedAssemblyInsertConstraintTarget`;
- `AssemblyConstraintTargetResolver`;
- `AssemblySpacePlanarDescriptor`, `AssemblySpaceAxisDescriptor`;
- `AssemblyTransformEvaluator`;
- planar Mate/Distance/Angle residual descriptors;
- Concentric and Insert residual descriptors;
- `RevoluteJointResidualDescriptor`;
- geometric constraint equation builders;
- `AssemblyRevoluteJointEquationBuilder`;
- private shared assembly numeric relationship/Jacobian system;
- private shared numeric solve engine;
- `AssemblyRigidBodySolver`, `AssemblySolveResult`, `AssemblySolveResultApplier`;
- `AssemblyJointMotionSolver`, `AssemblyJointMotionResult`, `AssemblyJointMotionResultApplier`;
- `AssemblySolveDiagnosticsAnalyzer` and rank/DOF descriptors;
- `AssemblyStepExporter`, `AssemblyStepExportSummary`.

Planned object families include rigid then flexible subassembly occurrences, richer joints, motion studies, bodies and body transforms/booleans, path records, 3D sketches, surfacing, and collision/interference analysis.

## Persistent geometric constraint intent

Canonical document: `docs/assembly-constraint-model-intent-mvp5.md`.

Persistent geometric relationship families are:

```text
Mate
Concentric
Distance
Insert
Angle
```

Every record preserves target A/B semantic strings and active/inactive state. Distance carries a positive length quantity. Angle carries a degree quantity. Mate, Concentric, and Insert are value-free relationship records.

The record layer does not resolve geometry, evaluate transforms, construct residuals, solve placement, or persist derived solver data.

`AssemblyConstraintGraph` contains every component as a node and every active geometric constraint as one distinct edge. It excludes inactive constraints, preserves multi-edges, sorts lexicographically, and exposes deterministic connected groups for ordinary geometric solving.

## Persistent joint and limit intent

Canonical document: `docs/assembly-revolute-joint-motion-mvp5.md`.

Joint records are separate from `AssemblyConstraint` records.

The first family is:

```text
AssemblyJointType::Revolute
```

One persistent Revolute record stores:

```text
joint identity
name
target A
target B
active/inactive state
lower degree limit
upper degree limit
authored current degree coordinate
```

The current seed uses the signed principal range `[-180°, 180°]`, requires `lower < upper`, and requires the authored coordinate to stay inside the limits. Motion requests outside the limits are rejected rather than clamped.

`AssemblyJointGraph` contains every component as a node and every active joint as a distinct lexicographically sorted edge. Inactive joints do not participate. Joint connectivity is derived and unpersisted.

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

One `.seat` endpoint derives:

```text
primary axis
  origin = mapped CircleProfile.center
  direction = source workplane normal adjusted by ExtrudeDirection

oriented seating plane
  origin = primary axis origin
  normal = primary axis direction
  right-handed basis preserved for OppositeSketchNormal
```

Insert and Revolute reuse this same semantic producer. There is no joint-specific hidden topology target.

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

The exact same convention is reused by constraint target evaluation and posed assembly export. There is no solver-specific or exporter-specific placement convention.

## Geometric residual families

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

Equal and opposed axis directions are accepted.

### Insert

```text
direction_parallelism       = cross(dA, dB)
axis_offset_mm               = cross(oB - oA, dA)
signed_seating_separation_mm = dot(sB - sA, nA)
```

Target A defines signed seating direction. Axis-direction parallelism already owns the regular tilt constraints, so no duplicate seating-normal row is added.

### Angle

```text
angle_alignment = dot(nA, nB) - cos(target_angle_deg)
```

The current cosine seed is direction-symmetric. At `0°` and `180°`, the derivative vanishes at the solved state. Oriented signed planar angles are deferred.

## Revolute motion-drive residual

A signed Revolute coordinate requires a directed axis because target A defines positive rotation. Unlike Concentric/Insert, opposed directions are not equivalent:

```text
direction_alignment = dA - dB
```

The remaining geometric rows are:

```text
axis_offset_mm               = cross(oB - oA, dA)
signed_seating_separation_mm = dot(sB - sA, nA)
```

Signed twist uses target A's directed axis and the two oriented seating-plane x axes:

```text
reference_cosine = dot(xA, xB)
reference_sine   = dot(dA, cross(xA, xB))

twist_alignment_sine   = sin(phi - target)
twist_alignment_cosine = cos(phi - target) - 1
```

The implementation expands the last two values directly from `reference_sine`, `reference_cosine`, `sin(target)`, and `cos(target)`, avoiding an `atan2` branch cut.

One transient Revolute drive flattens as:

```text
direction_alignment.x
direction_alignment.y
direction_alignment.z
axis_offset_mm.x / length_residual_scale_mm
axis_offset_mm.y / length_residual_scale_mm
axis_offset_mm.z / length_residual_scale_mm
signed_seating_separation_mm / length_residual_scale_mm
twist_alignment_sine
twist_alignment_cosine
```

At a regular satisfied one-free-body drive state, the shared central finite-difference Jacobian is `9 x 6` with rank `6`. The persistent joint represents one motion DOF; the transient requested coordinate temporarily constrains that DOF for pose solving.

## Shared numeric relationship and solve path

The private numeric system consumes a derived relationship set:

```text
constraint_ids[]
revolute_drives[]
```

Residual order is deterministic:

```text
1. geometric constraints in AssemblyConstraintGraph order
2. Revolute drives in AssemblyJointGraph order
```

Every free active component contributes:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

The numeric layer owns scalar flattening, length scaling, variable extraction/application on project copies, residual summaries, and central finite-difference Jacobian construction.

The former rigid-body iteration loop is now one private shared engine:

```text
detail::solve_numeric_relationships
```

It implements:

- damped Gauss-Newton normal equations;
- partial-pivot Gaussian elimination;
- damping escalation;
- backtracking line search;
- established solve states;
- complete component snapshots;
- free-active-component transform proposals.

`AssemblyRigidBodySolver` supplies only geometric constraint ids and an empty Revolute drive list.

`AssemblyJointMotionSolver` supplies geometric constraint ids plus transient Revolute drives. There is no separate motion optimizer.

## Suppression and combined motion groups

Ordinary geometric solving uses one deterministic `AssemblyConstraintGraph` group. Suppressed components leave the numeric subgroup, and every geometric constraint touching them vanishes. Complete component snapshots still include suppressed members for stale-result detection.

Motion derives a transitive component closure over both active graph families:

```text
selected joint endpoints
  -> active geometric constraint edges
  -> active joint edges
  -> repeat until stable
```

The full combined group is snapshotted. The active numeric subgroup excludes suppressed components. Non-selected joint drives and constraints touching suppressed components vanish.

The selected joint itself requires active, non-suppressed target components.

For a motion query:

```text
selected joint drive = requested coordinate
other active joint drives in same active group = persisted coordinates
```

This keeps other active joints from silently becoming free during a selected-joint motion.

## Solve and motion application boundaries

`AssemblyRigidBodySolver` mutates only private `Project` copies and returns `AssemblySolveResult`. `AssemblySolveResultApplier` accepts only converged results, validates complete component solve-input snapshots and free-active proposals, then applies transforms atomically through another project copy.

`AssemblyJointMotionSolver::move` is also read-only. It returns:

```text
selected joint id
selected source coordinate
requested coordinate
complete snapshots for every driven joint
embedded AssemblySolveResult
```

Each driven-joint snapshot contains id, type, state, targets, limits, and source coordinate.

`AssemblyJointMotionResultApplier` first validates every driven-joint snapshot. It then applies the embedded component proposals through `AssemblySolveResultApplier`, replaces the selected authored joint coordinate, and commits the project copy only if both operations succeed.

Thus component placement and selected joint coordinate change atomically.

## Local Jacobian rank and remaining DOF

`AssemblySolveDiagnosticsAnalyzer` evaluates the exact shared geometric numeric Jacobian at a converged private solve state over the same active subgroup as the ordinary solver.

```text
variable_count  = 6 * free_active_component_count
constrained_dof = rank(J)
remaining_dof   = variable_count - rank(J)
```

Proven regular one-free-body geometric results include:

```text
Concentric: rank 4/6, remaining DOF 2
Insert:     rank 5/6, remaining DOF 1
Angle:      rank 1/6, remaining DOF 5 away from extremal targets
```

A driven Revolute query is independently covered through the same central finite-difference numeric path as rank `6/6`. Persistent-joint DOF presentation and null-space motion exploration without an explicit coordinate request remain deferred.

## Posed assembly STEP export

Canonical document: `docs/assembly-posed-step-export-mvp5.md`.

`AssemblyStepExporter` is the first derived end-to-end assembly geometry consumer:

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

Hidden and suppressed components are omitted. Missing members, failed recomputes, missing final shapes, transform failures, empty visible-active output, and STEP writer failures stop the export.

The current seed exports one geometric compound. Structured STEP product hierarchy and component geometry instancing are deferred.

`blcad_export_posed_assembly` proves JSON -> optional geometric solve/apply -> posed compound export. `blcad_move_joint` proves JSON -> Revolute motion query -> atomic apply -> updated project JSON; that result can be passed directly into posed export.

## Persistence boundary

Persisted model intent includes:

- project, part, and root assembly records;
- parameters and bindings;
- component occurrence identity, state, grounding, and `RigidTransform`;
- geometric assembly constraint identity, target strings, state, and explicit values;
- joint identity, family, semantic targets, state, limits, and authored coordinate.

Derived and unpersisted data includes:

- constraint graph connectivity;
- joint graph connectivity;
- combined motion relationship groups;
- resolved local plane/axis/seat descriptors;
- assembly-space plane/axis/seat geometry;
- geometric and Revolute residual descriptors;
- transient Revolute drive sets;
- flattened numeric residual vectors;
- numeric Jacobians and normal equations;
- solver iteration state;
- solve and motion results;
- component and joint input snapshots;
- unapplied transform proposals;
- residual summaries;
- rank and remaining-DOF diagnostics;
- per-export shape caches;
- transformed component shape copies;
- posed OCCT compounds.

Only explicit successful application changes persisted component transforms. A successful motion application additionally changes the selected joint coordinate because that coordinate is explicit authored motion state.

## Next assembly block

The next block is the rigid subassembly instance and nested posed-export seed.

The project container must gain project-owned child assembly documents and the assembly model must gain rigid subassembly occurrences that reference them. Validation must reject direct and indirect assembly-reference cycles. Hierarchy traversal, composed transforms, and flattened leaf descriptors remain derived.

The first consumer is recursive posed export: visible active child-assembly occurrences are traversed deterministically, child component shapes are evaluated through every parent rigid transform, and leaf geometry is flattened into the existing OCCT compound export. Hidden or suppressed subassembly occurrences exclude complete subtrees.

Flexible subassembly solver variables and cross-hierarchy geometric constraint/joint semantics remain deferred until the rigid hierarchy boundary is stable.

## Broader future direction

Multi-body transforms/booleans and path features are documented in `docs/multi-body-transform-and-path-features-roadmap.md`.

Inventor-like sketch/feature parity is documented in `docs/inventor-like-sketcher-and-feature-roadmap.md`.

3D sketching and surfacing are documented in `docs/advanced-surfacing-and-3d-sketch-mvp.md`.

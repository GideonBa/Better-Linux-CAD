# Assembly System with Constraints

Status: architecture overview with historical pipeline descriptions. Assembly MVP-5 through Block 47,
the accepted interactive workflows and structured STEP import are implemented; `mvp-plan.md` and
the feature-specific Assembly contracts own current status and exact behavior. Old “next” statements
below preserve their original phase context and are not a new execution queue.

[Blocks 189–199](inventor-parity-sequence.md) consolidate model states, independent flexible
occurrence poses, directed adaptive cross-document references, coordinated Assembly holes and
broader motion/contact analysis. Existing shared-child local solving and Assembly parameter bindings
remain supported; they are extended rather than replaced by viewer transforms or a second solver.

## Directed design references and adaptive candidates — 194–195

`ProjectDesignReference` owns one directed cross-Part link. Its source stores an exact rooted
component occurrence, expected Part identity, explicit model state and either a semantic datum/
face/axis/point spelling or a source PathCurve ID. Its target stores one exact driving occurrence,
expected editable Part and a ConstructionPoint/Line/Plane or a generated Sketch3D/PathCurve pair.
A shared adaptive Part has one driving occurrence across all links; every occurrence uses the
resulting definition. Repeated source occurrences remain distinct frame contexts. Reference-mode
Parts may supply sources; they cannot receive adaptive definition edits.

The Core dependency authority validates identities, selected states, active source/driver paths,
existing target constructions, unique target ownership and deterministic cross-Part ordering.
Part-level cycles reject conservatively. Additional directed Part dependencies may join that same
ordering authority. Linked targets store local derived construction values alongside their Project
source identity. `detach_design_reference` removes the directed link and leaves the last checked
local construction as an independent snapshot. This does not invent an imported feature tree or
persist OCCT topology, sampled world geometry or composed world transforms as source identity.

`AdaptiveProjectEvaluator::bind/evaluate/detach` works on disposable Projects. The exact source
occurrence chain maps source-local geometry into root space, then through the checked inverse of
the target driver chain into target-local coordinates. `project_occurrence_transform` and
`project_occurrence_mapping` expose that same mapping to coordinated Project consumers.

Evaluation solves independent Assembly placement/joint contexts, applies links in directed order,
recomputes affected Part histories and validates complete posed/physical Assembly results. A mixed
graph includes shape inputs, connected free pose authorities and every consumed source/driver
pose. Coordinated Hole layouts contribute cross-Part shape edges and both consumed occurrence poses
to that same guard, including Projects without adaptive references. The source layout Hole retained
in its own Part adds no artificial self-edge. Any recursive shape/pose dependency rejects; there is no iterative feedback solver. A final
solve must retain the consumed frame within the explicit numeric boundary. Geometry, healing,
boolean, relationship, source-freshness or context failure discards the complete candidate, retaining
all committed Part definitions, resources, sidecars and root history. The existing GUI root Project
transaction owns publication and one Undo boundary; durable multi-asset Save keeps its existing
atomic-publication contract.

Analytic semantic inputs preserve their checked capabilities. Native path derivation currently
copies explicit Sketch3D Line/Polyline/Arc/Spline definitions with solved source control/reference
points and unchanged degree/representation. Rigid mapping preserves their native curve family;
there is no chord sampling. Target segment identities, source topology and execution policy must
remain compatible. Fixed-up frames, Helix/equation paths and other source families require an
explicit future capability and currently reject. General curve/face derivation, richer topology
repair and every deferred requirement remain in [the full sequence](inventor-parity-sequence.md)
and [A02 acceptance](inventor-parity-test-plan.md#a02-directed-adaptive-cover-and-duct).

Unchanged canonical target content retains its executable generation. Repeated normalization can
therefore reuse certified results without changing authored snapshots. The tests are
[Core design references](../tests/core/project_design_reference_tests.cpp) and
[physical adaptive Projects](../tests/geometry/adaptive_project_tests.cpp), tagged
`[design-reference]` and `[adaptive-project]`. They cover exact rotated/translated cover support,
repeated source ownership, sparse source-state intent, native spline consumers, actual Sweep,
Loft/Thicken volumes, source updates, detached snapshots, exact reopening, independent STEP
volume/centroid, no-op generations and whole-Project failure. Numbered acceptance and native
interaction remain governed by [the MVP plan](mvp-plan.md); these test links do not close a block.

## Positions, design views and native Assembly tasks — 198

`AssemblyPosition` stores explicit root component/subassembly local placements, root and
cross-hierarchy joint coordinates and the independent rooted occurrence-pose catalog. Capture reads
the checked effective Assembly; publication replaces a catalog only on the authoritative
Primary/Master/Default Project. `Master` is the authored Project and cannot be renamed or deleted.
A named evaluation is disposable, preserves the shared child definitions and carries an explicit
runtime position marker. Switching an already projected position to another position requires the
authoritative root; resetting the marker cannot restore authored numeric placements.

`AssemblyDesignView` stores rooted component/subassembly visibility with expected definition
identities. `Default` retains authored visibility. Views do not store suppression, parameters,
materials, manufacturing state or joint values. Presentation traversal applies view visibility;
physical participation still includes every hidden active occurrence. Both catalogs reject duplicate
identities/names, orphan paths and retargeted definitions. Model states, positions and display views
are independently selected session contexts and compose from the immutable authored root in that
order. Their active choices are never saved; Open starts at Primary/Master/Default. Complete source
freshness includes each selected context, including equal numeric results with different identities.

Common and adaptive Project validation request physical-active posed definitions, including hidden
active occurrences and every available Body. Hiding all components in a design view or all Bodies
for display therefore preserves valid manufacturing geometry and physical mass. Suppression and
source/recipe validity retain their physical meaning. Scene and STEP keep visible-active scope and
their empty/no-visible diagnostics; contact, clearance and interference retain their existing
visibility scope. Physical validation does not use the export-only no-visible-geometry gate.
Geometry `[project-physical-visibility]` checks these boundaries for adaptive references and
coordinated machining.

The existing Assembly ribbon owns Flexible Occurrence, Adaptive Reference, Assembly Hole, Positions
and Design Views, and Edit Part in Context. Each uses the common root Project transaction and
complete candidate validation. Preview retains invalid inputs for correction; Apply publishes one
named root Undo. Cancellation and mode changes release task previews and restore temporary filters.
Flexible controls select an exact rooted child, explicit rigidity and a local joint coordinate.
Adaptive selectors show the source, one visible driving occurrence and the directed dependency chain.
Assembly Hole selects an authored shared layout/pattern and explicit existing participant recipes,
states, roles and shared-definition policy. Ordinary Hole authoring inside the Part context creates
those recipes. The [coordinated machining contract](hole-wizard.md#coordinated-project-machining--196197)
owns participant execution and common Project normalization for geometry consumers.

Positions can be stored, recalled, renamed and deleted; named display views have an independent
visibility table, Preview/Apply, Recall and Delete. The browser exposes both catalogs and the title
shows selected names. Generic modeling requires Master; dedicated representation edits start from
Primary/Master/Default and restore the selected checked view afterward. In-context Part editing also
requires that explicit root scope. Its breadcrumb distinguishes Part coordinates and the shared
Primary definition; Finish and Cancel return through the existing root lifecycle and child Sketch
catalog authority described in [the context contract](parameter-model.md#in-context-part-editing-and-root-publication--198).

Proof sources are [Core representations](../tests/core/assembly_representation_tests.cpp),
[session/native Assembly controls](../tests/gui/gui_advanced_assembly_tests.cpp) and
[context lifecycle](../tests/gui/gui_project_part_context_tests.cpp). Native tests use actual
keyboard/mouse controls and independent material-point, volume, mass, Measure and reopened STEP
checks. Test registration describes intended proof; executed results, reviewed captures and numbered
acceptance remain in [development setup](development-setup.md) and [the MVP plan](mvp-plan.md).

## Core model

A project owns one assembly and project-owned part documents. An assembly contains component occurrences referencing those parts.

```text
PartDocument Screw_M6x25
AssemblyDocument HousingAssembly
  ComponentInstance Screw_01 -> Screw_M6x25
  ComponentInstance Screw_02 -> Screw_M6x25
```

One part model may be reused by several component occurrences.

Persistent records store model intent. Graphs, resolved geometry, residuals, Jacobians, solve results, and DOF diagnostics remain derived unless a fresh converged transform proposal is explicitly applied.

## Current assembly pipeline

```text
persistent ComponentInstance + AssemblyConstraint intent
  -> AssemblyConstraintGraph
  -> deterministic graph-ordered active constraint ids

planar face target
  -> AssemblyConstraintTargetResolver::resolve
  -> component-local plane
  -> AssemblyTransformEvaluator::evaluate_plane
  -> AssemblyConstraintEquationBuilder
  -> Mate / Distance residual

axis target
  -> AssemblyConstraintTargetResolver::resolve_axis
  -> component-local axis
  -> AssemblyTransformEvaluator::evaluate_axis
  -> AssemblyConcentricConstraintEquationBuilder
  -> Concentric residual

circular seat target
  -> AssemblyConstraintTargetResolver::resolve_insert
  -> component-local primary axis + oriented seating plane
  -> evaluate_axis + evaluate_plane
  -> AssemblyInsertConstraintEquationBuilder
  -> composite Insert residual

Mate / Distance / Concentric residual
  -> shared numeric residual/Jacobian system
  -> AssemblyRigidBodySolver on Project copies
  -> AssemblySolveResult
  -> explicit AssemblySolveResultApplier
  -> AssemblySolveDiagnosticsAnalyzer

Insert residual
  -> next: shared numeric / solver / application / diagnostics integration
```

Geometry-family interpretation is explicit. The current numeric solver remains one common path for every integrated family.

## Component instances and placement

Canonical document: `docs/component-instance-mvp5.md`.

Each `ComponentInstance` stores occurrence identity, referenced part, visibility, suppression, grounding, and:

```text
RigidTransform
  translation_mm
  rotation_deg
```

Direct placement/state edits are explicit storage operations. They do not infer constraints or run the solver.

## Assembly constraint intent

Canonical document: `docs/assembly-constraint-model-intent-mvp5.md`.

```text
AssemblyConstraintTarget
  component_instance
  semantic_reference

AssemblyConstraint
  id
  name
  type = mate | concentric | distance | insert
  target_a
  target_b
  state = active | inactive
  distance = optional positive length
```

Distance is the only type that may carry `distance`.

Mate, Concentric, and Insert are distance-free persistent relationship intent.

The record layer keeps target semantic strings opaque. It does not resolve geometry or solve placement.

Target A/B order is persistent intent and is never normalized away.

## Active-constraint connectivity

`AssemblyConstraintGraph` is a deterministic read-only relationship graph:

```text
nodes = every ComponentInstanceId
edges = active AssemblyConstraintId records
```

Inactive constraints do not affect connectivity. Multi-edges are legal. Node, edge, adjacency, and connected-group order is lexicographic.

The graph does not distinguish planar, axis, or composite seat geometry. An active Insert record is the same relationship-edge form as any other active constraint.

Exact connected groups are current rigid-body solve partition boundaries.

## Semantic planar face targets

Implemented family:

```text
feature.<feature-id>.top
feature.<feature-id>.bottom
feature.<feature-id>.right
feature.<feature-id>.left
feature.<feature-id>.front
feature.<feature-id>.back
```

For supported `AdditiveExtrude` features, `resolve` reuses `WorkplaneResolver::resolve_generated_face` and returns a component-local plane plus separate persisted component transform.

## Semantic generated-axis targets

Canonical document: `docs/assembly-semantic-axis-concentric-residuals-mvp5.md`.

First axis token:

```text
feature.<feature-id>.axis
```

The first producer is one `SubtractiveExtrude` whose source sketch contains exactly one `CircleProfile` and exactly one total profile.

The axis is derived from constructive intent:

```text
origin = mapped CircleProfile.center
direction = source workplane normal adjusted by ExtrudeDirection
```

Exact source feature/profile identity is preserved.

## Semantic seating targets and composite Insert endpoints

Canonical document: `docs/assembly-insert-intent-composite-residuals-mvp5.md`.

First seating token:

```text
feature.<feature-id>.seat
```

The first producer uses the same intentionally narrow single-circle `SubtractiveExtrude` family as `.axis`.

`AssemblyConstraintTargetResolver::resolve_insert` derives:

```text
ResolvedAssemblyInsertConstraintTarget
  component_instance
  referenced_part_document
  source_feature
  source_profile
  axis = Primary
  seating_plane = Primary
  local_axis
  local_seating_plane
  component_transform
```

The local axis and seat come from the same exact feature/profile identity.

```text
axis.origin = mapped CircleProfile.center
axis.direction = extrude direction
seat.origin = axis.origin
seat.normal = axis.direction
```

For `OppositeSketchNormal`, the seat Y basis and normal are both reversed so the local seat frame remains right-handed.

`CircularHolePattern` is not assigned one ambiguous `.axis` or `.seat` token because it produces several distinct holes. Stable per-instance pattern identity remains future work.

## Rigid-transform evaluation

`RigidTransform` uses:

```text
translation_mm: millimeters
rotation_deg: degrees
active right-handed fixed-axis rotations
apply X, then Y, then Z
R = Rz * Ry * Rx for column vectors
```

Evaluation rules:

```text
point/origin   -> rotate, then translate
vector         -> rotate only
normal         -> rotate only
axis direction -> rotate only
```

Insert reuses `evaluate_axis` for its primary axis and `evaluate_plane` for its seating plane.

A focused test proves that local seat/axis origin `(4,-6,0)` under translation `(10,20,30)` and X rotation `90 deg` evaluates to `(14,20,24)`, while local +Z axis/seat normal evaluates to `(0,-1,0)`.

## Mate residual

```text
normal_opposition    = nA + nB
signed_separation_mm = dot(oB - oA, nA)
```

Tangential plane-origin separation is intentionally absent.

## Distance residual

```text
normal_parallelism   = cross(nA, nB)
signed_separation_mm = dot(oB - oA, nA)
distance_residual_mm = signed_separation_mm - target_distance_mm
```

Equal or opposed parallel normals are accepted. Target A defines signed separation.

## Concentric residual

```text
direction_parallelism = cross(dA, dB)
axis_offset_mm         = cross(oB - oA, dA)
```

Equal and opposed directions are accepted. Axial origin separation is absent, so Concentric leaves axial translation and rotation about the common axis free.

Regular one-free-body Concentric rank is `4/6` with two remaining local DOF.

## Composite Insert residual

The dedicated read-only builder is:

```text
AssemblyInsertConstraintEquationBuilder
```

One `.seat` endpoint is resolved and evaluated to:

```text
AssemblySpaceInsertConstraintTargetDescriptor
  component_instance
  semantic_reference
  source_feature
  source_profile
  axis
  seating_plane
```

For target axis lines `(oA,dA)`, `(oB,dB)` and seating planes with origins `sA`, `sB` and target-A normal `nA`:

```text
direction_parallelism       = cross(dA, dB)
axis_offset_mm               = cross(oB - oA, dA)
signed_seating_separation_mm = dot(sB - sA, nA)
```

The seat normal is canonically aligned with the same endpoint's semantic axis direction.

The first two fields reuse stable Concentric axis-line semantics. The third removes Concentric's axial freedom by seating the target planes.

Target A defines the signed seating direction.

No separate seat-normal vector is included because axis-direction parallelism already constrains the two regular tilt directions.

Insert therefore exposes seven scalar residual components:

```text
3 direction components
3 axis-offset components
1 seating-separation component
```

A direct central finite-difference `7 x 6` Jacobian test proves regular rank `5`, leaving only rotation about the common axis free.

The rank is measured from residual sensitivity, not hard-coded from the Insert type.

## Shared numeric residual/Jacobian system

The private shared numeric implementation is used by solver and diagnostics.

Current numeric families:

```text
Mate
Distance
Concentric
```

Constraint records use lexicographic graph order. Each free component contributes:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

Current scalar flattening:

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

Every finite-difference perturbation re-resolves semantic geometry and reconstructs residuals from current project intent.

Insert is not yet flattened in this shared path.

## Rigid-body solver

Participation policy:

```text
Grounded   -> fixed participant
Free       -> six numeric variables
Suppressed -> selected group rejected
Visibility -> no participation effect
```

At least one grounded component is required. Multiple grounded components may remain fixed.

The current solver uses deterministic damped Gauss-Newton:

```text
evaluate r
  -> central finite-difference J
  -> J^T J + lambda I
  -> -J^T r
  -> partial-pivot Gaussian elimination
  -> deterministic backtracking line search
  -> damping escalation
```

Mate, Distance, and Concentric participate through the same residual evaluator.

Insert currently fails explicitly before numeric solving through the dedicated composite-target boundary. It is never silently ignored and the source project remains unchanged.

## Explicit result application

`AssemblySolveResult` contains complete group component snapshots and proposed transforms for free components.

`AssemblySolveResultApplier`:

- accepts only converged results
- verifies complete source snapshots
- rejects changed transforms, grounding, or suppression state
- detects moved grounded anchors
- validates all proposals
- applies to a private project copy
- replaces the caller project only after every update succeeds

No solve result is persisted.

## Local Jacobian-rank and remaining-DOF diagnostics

For a converged integrated numeric state:

```text
variable_count  = 6 * free_component_count
constrained_dof = rank(J)
remaining_dof   = variable_count - rank(J)
```

Rank uses:

```text
max(rank_absolute_tolerance,
    rank_relative_tolerance * maximum_abs_jacobian_entry)
```

DOF, consistency, and residual-row rank structure are separate classifications.

Concentric rank is already evaluated through this shared analyzer.

Insert rank is currently proven by a focused direct residual/Jacobian test because Insert is not yet a shared numeric family. The next integration block must prove the same rank `5/6` and one remaining DOF through `AssemblySolveDiagnosticsAnalyzer`.

## Grounding, visibility, and suppression

Grounding is persisted model intent. Direct storage-level transform updates remain legal while grounded; solver policy interprets grounded components as fixed.

Visibility does not affect current solving.

Suppressed selected components are rejected rather than silently removed because suppression-aware graph and constraint participation has not yet been defined together.

## Persistence boundary

Assembly/project JSON persists:

- component identity and referenced part documents
- visibility, suppression, grounding
- `RigidTransform`
- Mate/Concentric/Distance/Insert constraint records
- target A/B semantic-reference strings

The following are derived and unpersisted:

- graph connectivity
- local plane/axis/seat descriptors
- assembly-space plane/axis/seat geometry
- Mate/Distance/Concentric/Insert residual descriptors
- flattened numeric residuals
- numeric Jacobians
- normal equations and damping state
- solve results and proposals
- rank/DOF diagnostics

`feature.hole.seat` uses the existing semantic-reference field. Insert uses the existing type string field. No new persistence field is introduced.

Only explicit application of a fresh converged solve result changes the existing persistent component transform.

## Next technical step

The next assembly block is **Insert integration into the shared numeric residual/Jacobian system, rigid-body solver, explicit result application, and local remaining-DOF diagnostics**.

Exact Insert flattening must be:

```text
direction_parallelism.x
direction_parallelism.y
direction_parallelism.z
axis_offset_mm.x / length_residual_scale_mm
axis_offset_mm.y / length_residual_scale_mm
axis_offset_mm.z / length_residual_scale_mm
signed_seating_separation_mm / length_residual_scale_mm
```

The integration must preserve all current deterministic ordering, finite-difference, damping, line-search, solve-state, grounding, suppression, snapshot, stale-result, and atomic-application contracts. It must solve lateral offset, tilt, and axial seating, preserve rotation about the common axis, and prove rank `5/6` with one remaining DOF through the exact shared diagnostics Jacobian.

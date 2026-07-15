# Architecture Summary

This document summarizes implemented BLCAD architecture and the current technical direction.
Feature-specific documents remain canonical for exact formulas, save-format spellings,
validation/failure policy, ordering, and focused proof.

## Goal and authority model

BLCAD is an independent parametric CAD system for Linux. BLCAD owns semantic model intent; OCCT/Open
CASCADE is the computed geometry and data-exchange kernel.

Fundamental decisions are:

- persist parametric and semantic intent rather than final BRep authority;
- persist stable semantic references rather than raw OCCT topology ids;
- use stable shared planar point identity rather than coordinate equality for Sketch connectivity;
- allow distinct point identities to occupy the same coordinate when topology does not connect them;
- solve planar constraints over Core topology identity, never over screen/OCCT interaction objects;
- keep Core model intent below Geometry query/execution types;
- separate stable semantic source identity from Geometry topology lookup;
- separate semantic source classification from geometric capability projection;
- separate direct component transform authority from rooted occurrence identity;
- separate authored hierarchy boundaries from derived traversal/composition;
- separate geometric relationships from motion-joint intent;
- separate exchange/product identity from OCCT/XDE/STEP identity;
- keep viewport pixels, hover, grid, hit stacks, snap candidates, and interaction samples transient;
- keep solver variables, residuals, Jacobians, rank, DOF, and conflict diagnostics derived;
- use explicit migration when a persistent identity model supersedes a historical representation;
- require validated candidate boundaries before solve/recompute results mutate persistent intent.

A lower authority layer must not depend on a higher execution or presentation layer for persistent
identity.

## Layer direction

```text
Qt user interface / commands / transient interaction
  -> Core planar Sketch topology + edit commands
  -> Core deterministic planar constraint solver
  -> Core Part/Assembly persistent model intent
  -> Geometry semantic target resolution and capability projection
  -> Geometry equation / residual / recompute execution
  -> freshness validation + atomic application
  -> posed geometry / analysis / exchange consumers
  -> OCCT geometry + XDE data-exchange kernel
```

The optional GUI is a client of Core and Geometry. Qt types stay out of `blcad_core`; widgets never
own BRep, constraint mathematics, transform authority, expressions, recompute, or persistent Sketch
topology.

## PartDocument and Part construction authority

`PartDocument` is persistent parametric definition authority. Implemented Part intent includes typed
quantities/parameters, formulas, datum and derived workplanes, construction geometry, historical planar
Sketch/profile records, projected/reference-driven Sketch geometry, Sketch constraints/dimensions,
Body identity, feature history, multi-body result operations, broad solid features, Sketch3D/PathCurve,
Sweep, path Extrude, Loft, Surface features, closed-shell-to-solid conversion, and multi-body STEP
export intent.

Stable `BodyId` and persistent Body result intent let recompute publish deterministic Body-scoped
products. Feature producers and consumers participate in dependency/invalidation planning. Core does
not persist `TopoDS_Shape` values.

`ShapeCache` and OCCT shapes are derived. Recompute validates current model intent, executes Geometry
adapters, and publishes checked Body/feature products. Consumers requiring fresh results use current
recompute/freshness boundaries rather than final-shape assumptions.

## Shared planar Sketch topology through Block 108

The historical planar `Sketch` representation embeds `Point2` coordinates directly in line, arc,
spline, and profile-center records. It remains load-compatible and remains the compatibility carrier
used by current Geometry/recompute consumers.

Block 108 adds the Core topology identity model used by solver/direct-manipulation consumers:

```text
SketchTopology
  SketchId
  points[]      : SketchTopologyPoint
  entities[]    : SketchTopologyEntity
  dependencies[]: SketchTopologyDependency
```

`SketchTopologyPoint` stores stable `SketchPointId`, planar position, construction state, and reference
state. `SketchTopologyEntity` stores stable id, kind, ordered point references, ordered entity
dependencies, construction state, and reference state.

Implemented topology kinds cover current Line, Arc, Spline, RectangleProfile, CircleProfile,
ClosedProfile, ArcClosedProfile, CompositeClosedProfile, CircularHolePattern, ProjectedPoint,
ProjectedLine, and ReferenceGeneratedLine intent.

Global point/entity collections are canonical lexicographic id order. Defining point roles and ordered
profile dependencies remain semantic order and are never sorted.

### Point identity is not coordinate equality

Two `SketchTopologyPoint` records may have equal coordinates and different `SketchPointId` values.
This is deliberate. Two logically distinct points can later be related by an explicit `Coincident`
constraint and remain two solver variables before solving.

Shared identity is established by explicit topology: multiple entities reference the same
`SketchPointId` when they own one topological junction.

### Deterministic historical migration

`SketchTopology::migrate_legacy(...)` projects historical Sketch records into canonical topology.
Stable topology ids use `entity/<SketchEntityId>` and `profile/<ProfileId>`.

Point usages propose owner/role ids such as:

```text
entity/line.web/start
entity/line.web/end
entity/arc.outer/mid
profile/profile.hole/center
```

Migration derives shared point equivalence only from explicit ordered historical profile connectivity:
`ClosedProfile`, `ArcClosedProfile`, and each CompositeClosedProfile contour. Current-end and next-start
usages are connected, including loop closure. Coordinates must agree within `1e-9`; the canonical id
is the lexicographically smallest proposed point id in the connected usage group.

`SketchTopologyMigrationReport` records every non-canonical usage collapsed into one shared point.
Unrelated equal-coordinate usages remain separate ids.

### Editable topology and persistence

`SketchEditCommandExecutor` implements AddPoint/AddEntity/MovePoint/ReplaceEntity/RemovePoint/
RemoveEntity over disposable canonical candidates. Reference topology is read-only and dependency-safe
removal uses topology dependency records.

Each successful command publishes complete canonical `before` and `after` snapshots.
`SketchTopologyUndoStack` stores those snapshots and never heuristically reconstructs inverse commands.

Canonical topology persistence is:

```text
schema  = blcad.sketch_topology.mvp8
version = 1
```

The historical PartDocument JSON schema is not retroactively reinterpreted. Explicit migration and a
lossless materialization check bridge representable topology edits to `PartDocument::update_sketch(...)`.
Any identity, flags, ordered dependency, or orphan-point state that historical Sketch JSON cannot
represent fails closed rather than being silently flattened.

Canonical contract: `docs/sketch-shared-topology-mvp8.md`.

## Deterministic planar Sketch solver through Block 109

Block 109 adds `SketchConstraintSolver` as a headless Core authority over Block-108 topology.

The solve path is:

```text
SketchTopology
  + SketchConstraintSystem
  -> canonical variable vector
  -> canonical residual vector
  -> central-difference Jacobian
  -> damped Gauss-Newton solve
  -> final Jacobian rank / remaining DOF
  -> redundancy or conflict attribution
  -> derived SketchSolveResult
```

No Qt, screen coordinate, OCCT owner, sampled polyline, or hit stack participates in solver identity or
mathematics.

### Canonical solve request and variables

`SketchConstraintSystem` owns a solve-request `SketchId` and `SketchSolverConstraint` records. The
system sorts constraints lexicographically by stable constraint id.

A solver target explicitly addresses one persistent `SketchPointId` or one `SketchTopologyEntity` id.
Family/type compatibility is validated before numeric solving.

Every non-reference topology point contributes variables in canonical point order:

```text
<SketchPointId>.x
<SketchPointId>.y
```

Reference points are excluded from the variable vector and retain source topology coordinates.

### Residual families

The direct headless solver supports:

```text
Coincident
Fixed
Horizontal
Vertical
Parallel
Perpendicular
Collinear
Equal
Tangent
Concentric
Midpoint
Symmetric
PointOnObject
HorizontalDistance
VerticalDistance
AlignedDistance
Radial
Diameter
Angular
```

Direct linear values use millimeters; angular values use degrees. Tangent requires a shared persistent
endpoint. PointOnObject supports Line/Arc, Equal supports Line/Arc measure, Concentric supports
Arc/CircleProfile/CircularHolePattern centers, Radial/Diameter target Arc, and Angular targets two
lines.

Invalid/missing targets report `InvalidReference`; the solver never invents a point/center/tangent from
coordinate equality or viewport samples.

Canonical formulas and family-local residual ordering are in
`docs/sketch-planar-constraint-solver-mvp8.md`.

### Scale normalization and numeric execution

The deterministic characteristic length is:

```text
max(1.0 mm, planar topology point bounding-box diagonal)
```

Length residuals divide by that scale. Orientation families use normalized dot/cross forms and angular
residuals use wrapped radians divided by pi.

The Jacobian uses central differences with default coordinate step:

```text
1e-7 * characteristic_length
```

The numeric engine solves:

```text
(J^T J + lambda I) step = -J^T r
```

with deterministic Gaussian elimination, damping sequence `1e-6 * 10^attempt`, and powers-of-two
line-search factors. Defaults freeze convergence RMS `1e-9`, rank absolute tolerance `1e-10`, rank
relative tolerance `1e-8`, 80 iterations, 8 damping attempts, and 12 line-search steps.

### DOF and diagnostics

After convergence, deterministic rank reduction of the final Jacobian yields:

```text
remaining_dof = canonical_variable_count - jacobian_rank
```

This is local differential DOF at the converged state and supersedes historical endpoint-warning
heuristics for solver-aware consumers.

Redundancy is attributed in canonical constraint order. One constraint residual block is appended at a
time; if a non-empty block does not increase cumulative rank, that stable id is redundant.

A stalled inconsistent system uses deterministic remove-one conflict attribution. Each constraint is
omitted in canonical order and the reduced system is re-solved from the original variable snapshot.
The omitted id is attributed when the reduced system converges. This is stable single-removal
attribution, not a minimum-unsatisfiable-subset algorithm.

Solve states are exactly:

```text
FullyConstrained
UnderConstrained
Redundant
Conflicting
NonConvergent
InvalidReference
```

`SketchSolveResult` contains derived solved topology, canonical variable order, final rank, remaining
DOF, redundant/conflicting ids, diagnostics, iteration count, and normalized residual summary.
Persistent Sketch intent is never mutated by `solve(...)`; solver/Jacobian/DOF state is not persisted.

### Historical Sketch adapter

`SketchConstraintSystemBuilder::from_legacy(...)` maps current persisted Fixed/Horizontal/Vertical/
Parallel/Perpendicular/EqualLength intent, TangentContinuity, and parameter-backed distance dimensions
onto the Block-109 solver boundary.

The direct solver supports more families than current historical Sketch JSON can author. Blocks 114
and 115 own persistence/UI authoring expansion. Projected-reference constraints without persistent
coordinate-capable Block-108 topology report `InvalidReference`; no screen/OCCT identity fills the gap.

Canonical contract: `docs/sketch-planar-constraint-solver-mvp8.md`.

## Semantic Part-feature input and generated topology identity

Part features use typed semantic input references and expected geometric capabilities instead of raw
kernel subshape ids. Stable generated-topology references encode producer/profile/role semantics;
producer-owned cardinality matrices define expected generated elements.

`ReferenceRecoveryEvaluator` evaluates current producer/profile/role intent read-only and returns
resolved/lost diagnostics. Recovery never writes OCCT traversal/hash/map, XDE, STEP, memory, or viewport
identity into persistent model intent.

## Project, AssemblyDocument, and hierarchy authority

`Project` owns one root `AssemblyDocument`, child assembly definitions, PartDocuments, and Project-level
cross-hierarchy relationship/joint records. A child assembly definition enters the rooted hierarchy
through authored `SubassemblyInstance` boundaries.

Persistent local component transform authority remains `(AssemblyDocumentId, local
ComponentInstanceId)`. Rooted occurrence identity is distinct and includes the exact subassembly path.
Repeated occurrences may therefore expose different root-space geometry while sharing one child
component transform authority.

`AssemblyHierarchyTraversal` derives deterministic root-first occurrence descriptors and transform
chains. Hidden/suppressed branches remove descendants. `AssemblyLeafOccurrenceResolver` is the
canonical hierarchy-to-Part leaf boundary for posed geometry consumers.

## Typed Assembly targets and solving

Assembly semantic target resolution is separated from equation geometry. Source classification and
canonical capability projection produce typed descriptors consumed by relationship/joint compatibility
and equation builders.

Assembly numeric solving uses deterministic variable ordering, residual systems, Jacobians, diagnostics,
and freshness-gated application. Direct and cross-hierarchy solve results are disposable until complete
source/reference freshness validation succeeds. Failed solve/freshness checks publish no partial
persistent component transforms.

This architecture provided the numeric-solving precedent reused conceptually by the Block-109 planar
Sketch solver, but Sketch variables/residual families and topology authority remain separate Core
contracts.

## Joint motion, posed geometry, analysis, and exchange

Implemented joint/motion authority includes Revolute, Prismatic, Cylindrical, Planar, and Spherical
families with local/cross-hierarchy motion paths and stable coordinate intent.

Posed geometry consumers resolve visible active Part leaves, recompute required Parts, and compose
component plus hierarchy transforms. Contact/interference/sweep analysis and STEP exporters consume
that derived posed state.

Flattened and structured assembly exchange derive stable product/occurrence identity; raw XDE/STEP
entity identity is never model authority.

## Qt GUI architecture through Block 110

`GuiDocumentSession` owns the current Project candidate/committed document view, recompute state,
semantic selection bridge, and exact GUI undo/redo snapshots. Generic command/task state remains
separate from Sketch-specific contextual staging.

Block 106 adds the contextual Sketch workspace, command HUD, stage model, and Enter/Finish Sketch
camera/selection/filter lifecycle.

Block 107 adds device-independent plane mapping, transient interaction scene projection, zoom-stable
hit testing, stacked-hit cycling, Window/Crossing selection, grid, snapping, and inference guides.

Block 108 adds Core shared point/entity identity and editable topology. The current Block-107 scene
builder still projects historical Sketch compatibility intent; a sampled interaction point is not
silently promoted to `SketchPointId`.

Block 109 adds the Core producer for remaining DOF and solve state.

Block 110 adds the first continuous GUI solver consumer. `GuiSketchDragController` derives stable
semantic handles from Block-108 point/entity identity and translates drag intent to transient Block-109
Coincident, Midpoint, Concentric, or Radial equations. The temporary pointer/center ids are removed from
the solved topology before publication. Preview topology must losslessly materialize and re-migrate.

`GuiSketchDragBinder` coalesces pointer moves to the latest pending sample and synchronously flushes the
exact release sample. Live preview rebuilds transient interaction presentation and publishes exact DOF/
solve state without document mutation. Successful release rechecks topology and constraint-system
freshness and commits one `GuiDocumentSession` transaction. Cancellation, lost capture, solve refusal,
or stale commit restores the pre-drag document/presentation state. Widgets still do not own constraint
mathematics.

## Persistence and regeneration split

Persisted or explicitly authored intent includes:

```text
parameters / expressions / workplanes / construction geometry
historical planar Sketch records and profile intent
canonical Block-108 SketchTopology snapshots when using topology persistence
Sketch constraints / dimensions / repair intent
Body / Part feature history and semantic references
Project / Assembly relationships / joints / hierarchy boundaries
stable exchange/product intent
```

Regenerated or transient products include:

```text
OCCT BRep and ShapeCache contents
reference-recovery evaluation results
Sketch legacy migration equivalence groups and reports
solver variables / residual vectors / Jacobians / rank / DOF
SketchSolveResult and solver diagnostics
Block-107 interaction samples / screen mapping / hit stacks / grid / snap candidates
Block-110 semantic drag handles / pointer samples / augmented drag equations / live preview candidates
Assembly solve/motion proposals and freshness snapshots
posed occurrence shapes
contact / interference / sweep analysis
XDE / STEP transfer sessions and exchange entity ids
GUI hover / preview / rubber-band / HUD staging
```

## Current boundary

Blocks 106–110 are implemented. Block 111 is the current next technical step.

Block 111 adds basic point/line/polyline/rectangle/parallelogram/polygon/centerline/construction
creation over the existing workspace, plane mapping, shared topology, solver, and document transaction
authorities. Creation commands must not turn Block-107 snap candidates or Block-110 handle positions
into implicit persistent identity.

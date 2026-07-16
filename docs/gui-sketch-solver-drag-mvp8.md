# Solver-Backed Sketch Drag MVP-8

Status: implemented in Block 110.

This document is the canonical GUI/Core integration contract for solver-backed planar Sketch mouse
dragging. Block 110 composes the Block-107 plane interaction layer, Block-108 shared topology identity,
Block-109 deterministic constraint solver, and the existing GUI document transaction/undo authority.

The GUI does not implement constraint mathematics. A pointer position becomes a transient semantic drag
target; `SketchConstraintSolver` remains the sole solve authority.

## Authority boundary

```text
screen pointer
  -> Block-107 Screen-DIP / active-plane mapping and snap
  -> GuiSketchDragHandle
  -> stable SketchPointId or SketchTopologyEntity role
  -> transient solver target point/entity/value
  -> Block-109 SketchConstraintSolver
  -> source-only solved SketchTopology preview
  -> lossless historical Sketch materialization
  -> viewport preview

release
  -> exact final pointer flush
  -> current-document/source freshness check
  -> lossless materialization + exact re-migration check
  -> GuiDocumentSession::commit_part_transaction(...)
  -> one recomputed document history entry
```

`GuiSketchDragController` owns the headless GUI-layer drag candidate. `GuiSketchDragBinder` connects the
controller to Qt pointer phases and presentation. `OcctViewport` owns only event delivery and transient
handle overlay.

## Semantic drag handles

The implemented handle families are:

```text
Endpoint
Midpoint
Center
Radius
Arc
Spline
Dimension
```

Stable handle ids are derived from the active `SketchId` and persistent topology identity:

```text
sketch/<sketch>/handle/point/<SketchPointId>
sketch/<sketch>/handle/entity/<entity-id>/midpoint
sketch/<sketch>/handle/entity/<entity-id>/center
sketch/<sketch>/handle/entity/<entity-id>/radius
sketch/<sketch>/handle/entity/<entity-id>/arc
sketch/<sketch>/handle/entity/<entity-id>/spline/control1
sketch/<sketch>/handle/entity/<entity-id>/spline/control2
sketch/<sketch>/handle/dimension/<SketchDimensionId>
```

Handle records are sorted lexicographically by stable id.

### Endpoint handles

Line start/end, Arc start/end, and Spline start/end map to one persistent `SketchPointId`.

A shared closed-profile junction creates one endpoint handle even when several topology entities
reference that point. The handle controls the shared point variable; Block 110 never searches for equal
coordinates or fans a move out to numerically equal endpoints.

### Midpoint handles

A Line midpoint handle addresses the persistent Line entity. The drag target uses the Block-109
`Midpoint` family with a temporary reference point as the requested midpoint.

The midpoint is not promoted to a persistent Sketch point merely because a handle is displayed.

### Center handles

Arc center is derived from the Arc's three persistent defining points. Dragging it uses a temporary
reference `CircleProfile` center and the Block-109 `Concentric` family.

RectangleProfile, CircleProfile, and CircularHolePattern center handles map directly to their persistent
center `SketchPointId`.

### Radius handles

Block 110 exposes Arc radius dragging. The handle anchor is deterministic: it uses the Arc center and a
radial direction derived from the defining mid point rotated by 30 degrees. This keeps the radius handle
separate from the Arc defining-point handle.

Pointer distance from the source Arc center becomes a positive millimeter target for the Block-109
`Radial` family.

Parameter-driven CircleProfile and CircularHolePattern sizes are not direct radius handles in Block
110. Their size authoring belongs to dimension/parameter workflows in Block 115.

### Arc handles

The Arc handle maps to the persistent three-point Arc `mid` point and therefore uses the ordinary
point-target drag path. The Arc start/end points are Endpoint handles.

### Spline handles

Cubic Bezier `control1` and `control2` points are explicit Spline handles and map to their persistent
`SketchPointId`. Spline endpoints use Endpoint handles.

### Dimension handles

Current historical driving dimensions address two explicit Line endpoint targets. Block 110 exposes a
Dimension handle at the second persistent target point and drags that point through the ordinary point
target path.

The existing driving dimension remains in the base constraint system. Therefore an incompatible pointer
is refused as a conflicting solve rather than silently changing or deleting dimension intent. In-canvas
dimension value editing and reference/driven dimension behavior remain Block 115.

## Read-only reference policy

A handle is read-only when its underlying topology point/entity is reference state. `begin(...)` rejects
reference handles.

Block-108 projected point/line records currently contain no invented resolved topology coordinates, so
they do not gain editable Block-110 handles from transient Block-107 projection samples.

## Drag target translation

`GuiSketchDragTargetKind` is:

```text
Point
LineMidpoint
ArcCenter
ArcRadius
```

Block 110 translates each target into existing Block-109 mathematics.

### Point

An augmented disposable topology receives one reference point:

```text
SketchPointId = __gui.drag.pointer
reference = true
position = snapped pointer plane coordinate
```

The transient constraint is:

```text
Coincident(controlled SketchPointId, __gui.drag.pointer)
```

### LineMidpoint

The transient constraint is:

```text
Midpoint(__gui.drag.pointer, line entity)
```

### ArcCenter

The augmented topology additionally receives a temporary reference CircleProfile entity:

```text
entity = __gui.drag.center
center = __gui.drag.pointer
reference = true
```

The transient constraint is:

```text
Concentric(arc entity, __gui.drag.center)
```

### ArcRadius

No pointer identity is persisted. The positive source-center-to-pointer distance becomes:

```text
Radial(arc entity, radius_mm)
```

### Stable transient constraint identity

The drag equation id is:

```text
zz.gui.drag.target
```

It sorts deterministically after ordinary persisted constraint ids. The id, temporary point, and
temporary center entity exist only in the disposable augmented topology/constraint system.

## Removing transient identity from preview

The raw Block-109 result may contain the temporary reference point/entity because the solve request was
augmented.

Before preview publication, Block 110 rebuilds a source-only topology:

1. iterate every point in the exact pre-drag source topology;
2. copy its solved position from the solver result by the same `SketchPointId`;
3. preserve source point flags;
4. copy the source entity and dependency collections exactly;
5. revalidate through `SketchTopology::create(...)`.

No transient id reaches `GuiSketchDragPreview::topology()` or commit.

The source-only topology is materialized with `SketchTopologyLegacyMaterializer`, re-migrated through
`SketchTopology::migrate_legacy(...)`, and required to compare exactly. A topology that cannot round-trip
without point-identity, flag, dependency, or orphan-state loss is refused before viewport preview.

## Solve acceptance and refusal

Accepted preview statuses are:

```text
FullyConstrained
UnderConstrained
Redundant
```

The `Redundant` state is accepted because the transient drag equation may be redundant when the pointer
already lies at the current fully constrained solution.

Refused statuses are:

```text
Conflicting
NonConvergent
InvalidReference
```

Fixed or fully constrained geometry is not weakened. Block 110 appends the drag equation to the complete
persisted base system. An incompatible pointer therefore produces a failed solve/refusal; no constraint
or dimension is deleted.

A refused live solve cancels the drag candidate, restores the pre-drag scene, clears the inference
anchor, and leaves the persistent document unchanged.

## Pointer coalescing and final-sample rule

`GuiSketchDragController::queue_pointer(...)` only overwrites one `pending_pointer`.

The Qt binder schedules at most one zero-delay solve callback. When it runs:

```text
consume latest pending pointer
solve once
publish latest valid preview
```

Several mouse moves before the callback therefore coalesce to one solve of the newest sample.

Release has stronger semantics:

```text
flush(final snapped pointer)
```

`flush(...)` overwrites any pending sample and synchronously solves the exact release position. Commit is
illegal while a pointer sample remains pending. A stale already-scheduled zero-delay callback observes
no active pending sample and becomes a no-op.

The final pointer position is therefore never dropped by drag throttling/coalescing.

## Preview publication

`GuiSketchDragPreview` contains:

```text
pointer
source-only solved SketchTopology
losslessly materialized temporary Sketch
published SketchSolveResult with source-only topology
```

The binder rebuilds the Block-107 interaction scene from the temporary Sketch and publishes handle
positions derived from the preview topology.

`PartDocument`, `ShapeCache`, document history, and dependency/invalidation state are not mutated during
live preview.

The existing Sketch status surface receives Block-109 `remaining_dof` and solve status during baseline
and live solve publication.

## Mouse lifecycle

Viewport pointer phases are explicit:

```text
Press
Move samples
Release
```

`OcctViewport` refreshes raw/snap/hit state before publishing Press and Release. A separate Block-110
drag pointer callback receives Move samples without replacing the Block-107 cursor/snap callback.

Press performs deterministic handle hit testing in Screen DIP:

```text
eligible distance <= 9 DIP
order by screen distance
then stable handle id
```

Handle hit testing is separate from Block-107 Point/Curve/Dimension/Glyph hit identity. Handle positions
are overlay state and never enter `GuiSelectionModel` or snap candidate identity.

The workspace lifecycle is:

```text
Idle/Hover
  -> SelectedHandle
  -> DragCandidate
  -> successful release: commit -> Idle
     or
  -> cancel/refusal/lost capture: rollback -> Idle
```

A simple press/release without any move or pending sample cancels without creating a document history
entry.

## Cancellation and lost capture

`Esc` is observed by the drag binder before `MainWindow::keyPressEvent(...)` advances the existing
workspace state machine. The binder restores the source preview and clears the controller candidate;
the existing workspace `escape(...)` then cancels the generic task.

`QEvent::UngrabMouse` and `QEvent::WindowDeactivate` are treated as lost pointer capture while a drag is
active. The binder restores source preview, cancels the controller and workspace task, clears the
inference anchor, and publishes no document mutation.

## Atomic release commit

A valid flushed preview commits through exactly one:

```text
GuiDocumentSession::commit_part_transaction("Drag sketch handle", mutation)
```

The mutation re-reads the current Sketch from the cloned candidate PartDocument and checks:

```text
current migrated topology == pre-drag source topology
current adapted constraint system == pre-drag source constraint system
```

This rejects stale release when Sketch geometry, constraints, dimensions, or bound parameter values
changed after preview began.

The final source-only solved topology is materialized against the current Sketch, re-migrated, and
required to compare exactly before `PartDocument::update_sketch(...)`.

The existing session transaction then recomputes the cloned Part and publishes it only after success.
One successful release therefore creates exactly one undo history entry. Undo restores the complete
pre-drag document snapshot and redo restores the complete committed result.

A failed mutation or recompute leaves the current PartDocument and last valid ShapeCache unchanged and
creates no history entry.

## Persistence

Block 110 adds no JSON field or schema.

Persisted state remains the existing Sketch/topology/document intent after a successful transaction.
The following remain transient/derived:

```text
GuiSketchDragHandle
handle screen positions
active handle index
pending / processed pointer samples
__gui.drag.pointer
__gui.drag.center
zz.gui.drag.target
augmented topology / constraint system
GuiSketchDragPreview
live solver result / residuals / Jacobian / DOF
coalescing timer state
```

## Focused proof

Focused tags:

```text
[gui][sketch-drag]
[integration][sketch-live-solve]
```

The proof covers:

- lexicographic stable handle order;
- one endpoint handle for one shared `SketchPointId` junction;
- endpoint, midpoint, center, radius, Arc, Spline-control, and Dimension handles;
- latest-pointer coalescing;
- source document unchanged during preview;
- exact final release sample flush;
- one atomic `Drag sketch handle` history entry;
- exact undo/redo document snapshots;
- explicit cancel rollback;
- incompatible fully constrained drag refusal;
- Arc radius and Arc center drag through Block-109 residual families;
- offscreen Qt mouse Press/Move/Release integration through the installed binder.

## Next boundary

Block 111 basic creation is implemented (`docs/gui-sketch-basic-creation-mvp8.md`); it adds no
second drag or solver authority, and a successful creation commit triggers
`blcad.action.sketch_topology_changed` so this drag binder rebuilds its handles and baseline solve
against the new topology. Block 112 adds circle/arc/ellipse/slot entities and their handles.

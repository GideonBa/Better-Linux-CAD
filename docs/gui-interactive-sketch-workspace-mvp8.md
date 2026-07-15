# Interactive Sketch Workspace MVP-8

Status: implemented in Block 106. Block 107 supplies plane-interaction producers, Block 108 supplies
persistent shared point/entity topology, and Block 109 supplies the deterministic headless solver/DOF
authority consumed by later GUI interaction.

This document is the canonical GUI contract for the contextual planar Sketch workspace introduced by
Block 106. It extends the Block-95/96 command/transaction rules and Block-99 Sketch workbench. It does
not own persistent Sketch geometry or solver mathematics.

Related canonical contracts:

- `docs/gui-sketch-plane-interaction-mvp8.md` — Block-107 mapping/hit/selection/grid/snap/inference;
- `docs/sketch-shared-topology-mvp8.md` — Block-108 point/entity identity and edit commands;
- `docs/sketch-planar-constraint-solver-mvp8.md` — Block-109 general planar solver and DOF.

## Scope

Block 106 promotes planar Sketch editing from a camera mode into a real contextual workspace.
`GuiWorkspace::Sketch` is transient GUI state. It is never serialized and never changes the Part
feature graph merely because the user enters or leaves a Sketch.

The workspace publishes five command groups in frozen order:

```text
Create | Constrain | Dimension | Modify | Project
```

The Sketch browser surface is organized around:

```text
Entities | Constraints | Dimensions | Diagnostics
```

The existing model browser remains semantic selection authority. The contextual Sketch surface changes
presentation and command availability; it does not create a second browser model or second selection
identity system.

## Enter Sketch

`GuiSketchWorkspace::enter(...)` requires:

- active Part document;
- no active `GuiTaskState` command;
- existing planar `SketchId` in the active Part;
- workplane resolving through the existing `geometry::WorkplaneResolver` path.

Before activation, the workspace captures previous GUI workspace and complete semantic selection.
`MainWindow` captures a transient `GuiViewportCameraBookmark` and selection-filter mask.

After successful entry:

- session workspace is `GuiWorkspace::Sketch`;
- Sketch is a contextual Part mode while global Part/Assembly/Inspect/Exchange tabs remain;
- workplane camera is normal to plane and orthographic;
- viewport selection mask is `SketchEntity | Edge | Vertex`;
- cursor becomes a crosshair;
- active Sketch remains undimmed while surrounding model presentation uses transient Dim by default;
- Sketch command groups and cursor/snap/DOF/solve status surfaces become visible;
- prior selection is a restore bookmark and working selection starts clear.

`OcctViewport` also exposes transient Isolate focus. Focus policies are view state only.

Block 107 installs the transient plane-interaction scene after the final normal camera is active.
Blocks 108 and 109 add no implicit Enter-Sketch mutation: entering the workspace does not migrate
historical Sketch JSON, edit topology, or solve/commit coordinates.

No Part feature is created by Enter Sketch.

## Finish Sketch

`GuiSketchWorkspace::finish(...)` is legal only when the workspace is active and no command,
numeric-input stage, preview, selected handle, or drag candidate remains active.

Finish currently runs the existing Sketch inspection/diagnostic authority. Diagnostic errors reject
Finish; warnings remain visible. Finish does not synthesize a downstream Part feature.

On success the controller restores previous workspace/selection. `MainWindow` clears Sketch focus,
restores Eye/Target/Up/Scale/projection/plane-camera state and selection filter, removes the crosshair,
and clears active Sketch workspace state. Block 107 clears its scene, grid, hover, snap marker,
inference guide, and box-selection products.

Block 109 does not silently commit a solve during Finish. Block 119 owns solver-aware region/profile/
repair Finish Sketch behavior.

## Frozen command state machine

```text
Idle
Hover
CollectingPicks
NumericInput
Preview
SelectedHandle
DragCandidate
```

Creation/edit path:

```text
Idle -> Hover -> CollectingPicks -> NumericInput -> Preview -> commit | cancel
```

Direct-manipulation path:

```text
Idle -> SelectedHandle -> DragCandidate -> commit | cancel
```

`GuiSketchWorkspace` projects stages onto existing `GuiTaskState`:

```text
CollectingPicks / SelectedHandle -> CollectingSelection
NumericInput                     -> EditingParameters
Preview / DragCandidate          -> Preview
```

There is no second GUI transaction or undo authority. Block-108 `SketchTopologyUndoStack` is a headless
Core snapshot utility. Document-level GUI commits still use session transaction/history authority.

Block 110 fills the `SelectedHandle -> DragCandidate` path with Block-109 solving.

## Escape and focus rules

`Esc` backs out one stage where a prior stage exists:

```text
Preview         -> NumericInput
NumericInput    -> CollectingPicks
CollectingPicks -> cancel command -> Idle
Hover           -> Idle
```

A selected-handle/drag-candidate command cancels atomically to Idle. Block 110 owns exact pre-drag
snapshot restoration and solver-preview cleanup.

Numeric input owns keyboard focus in `NumericInput`; `Enter` accepts the current field for candidate
validation. Canvas owns ordinary selection/placement focus. `MainWindow` routes unconsumed `Esc` to the
Sketch workspace controller.

While viewport remains on published normal-to-plane `GuiPlaneCamera`, right-drag orbit is disabled. A
deliberate standard-view change clears plane-camera state and enables existing orbit gesture. Short
right click routes to Sketch context handling and may expose Repeat Last compatible command.

## Numeric HUD and Preview/Commit

The numeric HUD is transient. Its text is not model identity and is never serialized.

The first wired command remains the historical planar line path:

```text
id, start-x, start-y, end-x, end-y
```

Coordinates are active-plane values. Before commit, `MainWindow` validates a disposable historical
Sketch candidate through `Sketch::add_entity(...)`. Only a valid candidate advances to Preview. Apply
uses `GuiSketchWorkbench::add_line(...)` and commits one document transaction. Block 107 then rebuilds
its read-only interaction projection.

Blocks 108–109 do not retrofit this seed HUD command into topology-native solver creation early. Block
110 adds topology-native solver-backed drag; Block 111 adds topology-native basic creation tools.

## Command repeat

A successful repeatable Sketch command records command id/tool hint as transient workspace state.
`repeat_last_command(...)` restarts only from Idle/Hover with no active generic task. Repeat state
clears on workspace exit.

## Selection filters and semantic identity

Frozen Sketch mask:

```text
SketchEntity | Edge | Vertex
```

The mask reuses `GuiSelectionKind` and existing viewport semantic bridge. No AIS owner, `TopoDS_Shape`,
screen coordinate, hover object, or Block-107 candidate id becomes persistent identity.

Block 107 hit/box selection publishes stable Sketch semantic ids to `GuiSelectionModel`. Direct
selection is disabled while task state collects picks, edits numeric input, or previews a command.

Block 108 adds `SketchPointId` as Core topology identity. Current selection code does not synthesize
point ids from screen hits. Block 110 must expose explicit semantic handles that map accepted interaction
landmarks to existing `SketchPointId`/entity roles.

Block 109 solver targets only stable point/entity ids. Qt must not pass screen coordinates, sampled hit
ids, AIS owners, or OCCT topology as solver identity.

The pre-Sketch filter mask is restored on Finish.

## Cursor, snap, DOF, and solve status

`GuiSketchWorkspaceStatus` is transient presentation for:

- tool hint;
- cursor coordinates in plane space;
- snap/inference text;
- optional remaining DOF;
- solve status;
- interaction stage/focus target.

Producer boundaries are now:

```text
Block 107 -> cursor coordinates, hit, snap, inference
Block 108 -> persistent shared point/entity topology identity
Block 109 -> headless solve state, exact local remaining DOF, solver diagnostics
Block 110 -> live drag solve invocation and status publication into GUI
```

Block 109 means DOF/Solve now have a real Core producer. The existing GUI does not yet continuously
invoke that producer, so it may still display `DOF: —` / `Solve: Not evaluated` outside a later
solver-aware command. Block 110 owns the first live publication during drag.

The UI must render `SketchSolveResult` status/remaining DOF; it must not infer them from endpoint counts,
constraint glyph counts, or topology migration.

## Camera bookmark and Sketch focus

`OcctViewport::camera_bookmark()` captures Eye, Target, Up, Scale, projection, and published plane-camera
state. Restore rejects non-positive scale and does not persist camera data.

`set_sketch_focus(...)` / `clear_sketch_focus()` are transient. Focus id is a stable Sketch semantic id
used only to identify `sketch/<SketchId>/...` presentations. Dim keeps active Sketch opaque and
increases surrounding transparency. Isolate erases non-active presentations from transient AIS
context. Clearing focus restores ordinary presentation policy.

## Transaction and persistence boundaries

Block 106 introduces no JSON field. Block 107 adds transient interaction products only. Block 108 adds
canonical `blcad.sketch_topology.mvp8` persistence and explicit historical migration. Block 109 adds no
JSON schema and persists no solver cache.

Persistent document mutation still goes through `GuiDocumentSession::commit_part_transaction(...)`,
which validates/recomputes a candidate before publishing one canonical history entry.

Transient state includes Preview, HUD text, repeat state, hover, cursor, snap text, focus, workspace/
camera bookmarks, screen mappings, grid products, hit cycles, solve status, remaining DOF, residuals,
Jacobian/rank diagnostics, and future drag preview candidates.

Block-108 topology commands provide exact Core before/after snapshots. The historical PartDocument
compatibility bridge requires lossless re-migration before atomic `PartDocument::update_sketch(...)`.
Block 109 returns a disposable solved topology; it does not choose a document commit boundary.

Block 110 must solve disposable candidates and commit exactly one validated document transaction on
successful release.

## Failure policy

The workspace fails closed when Enter Sketch lacks Part/Sketch/workplane or has an active command,
camera orientation fails, Finish has an active command/drag stage, existing Finish diagnostics contain
errors, a numeric candidate fails Core validation, or camera restore receives invalid state.

A failed Enter leaves previous workspace authoritative. Failed Finish leaves Sketch workspace active.
Failed candidate does not replace persistent Sketch intent. Block-107 mapping/scene failures leave
persistent Sketch unchanged. Block-108 topology failures publish no partial PartDocument mutation.

Block-109 `InvalidReference`, `Conflicting`, or `NonConvergent` results are derived diagnostics and do
not mutate persistent Sketch intent. Block 110 must restore the pre-drag snapshot and clear preview on
failed solve.

## Focused proof

Block 106:

```text
[gui][sketch-workspace]
[gui][sketch-command-lifecycle]
[gui][viewport][gui][navigation]
```

Block 107:

```text
[gui][sketch-hit-test]
[gui][sketch-snap]
[gui][sketch-box-selection]
```

Block 108:

```text
[core][sketch-topology]
[core][sketch-edit-command]
[core][sketch-json-migration]
```

Block 109:

```text
[core][sketch-solver]
[core][sketch-dof]
[core][sketch-conflict-diagnostics]
```

## Next boundary

Block 110 uses Block-107 pointer mapping, Block-108 semantic topology handles, and Block-109 solving for
live direct manipulation. It freezes handle identity, transient drag target semantics, throttled live
preview without dropping the final pointer position, fixed-geometry refusal, cancellation/lost-capture
rollback, and one atomic document commit on release.

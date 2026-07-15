# Interactive Sketch Workspace MVP-8

Status: implemented in Block 106; Block 107 supplies plane-interaction status producers and Block 108
now supplies the persistent shared point/entity topology used by future solver/edit consumers.

This document is the canonical GUI contract for the contextual planar Sketch workspace introduced by
Block 106. It extends the Block-95/96 command and transaction rules and the Block-99 Sketch workbench.
It does not own persistent Sketch geometry or solver mathematics. Device-independent mapping, hit
testing, box selection, grid, snapping, and inference are implemented by Block 107 in
`docs/gui-sketch-plane-interaction-mvp8.md`. Shared planar topology and headless edit commands are
implemented by Block 108 in `docs/sketch-shared-topology-mvp8.md`.

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

The existing model browser remains the semantic selection authority. The contextual Sketch surface
changes presentation and command availability; it does not create a second browser model or second
selection identity system.

## Enter Sketch

`GuiSketchWorkspace::enter(...)` requires:

- an active Part document;
- no active `GuiTaskState` command;
- an existing planar `SketchId` in the active Part;
- a workplane that resolves through the existing `geometry::WorkplaneResolver` path used by
  `GuiSketchWorkbench::plane_view(...)`.

Before the contextual workspace becomes active, it captures the previous GUI workspace and complete
semantic selection list. `MainWindow` captures a transient `GuiViewportCameraBookmark` and current
viewport selection-filter mask.

After successful entry:

- `GuiDocumentSession::workspace()` is `GuiWorkspace::Sketch`;
- Sketch is a contextual Part mode while global Part/Assembly/Inspect/Exchange tabs remain;
- the active workplane camera is normal to the plane and orthographic;
- the viewport uses `SketchEntity | Edge | Vertex` selection filtering;
- the cursor becomes a crosshair;
- the active Sketch remains undimmed while surrounding model presentation uses transient `Dim`
  focus by default;
- Sketch command groups and cursor/snap/DOF/solve status surfaces become visible;
- prior semantic selection is retained only as a restore bookmark and working selection starts clear.

`OcctViewport` also exposes transient `GuiSketchSurroundingsMode::Isolate`. Both focus policies are
view state only; no visibility/transparency intent is written to the Part.

Block 107's binder observes successful Sketch entry and installs the transient plane-interaction scene
after the normal camera is active. This lets native OCCT mapping use the final view and does not alter
the Enter-Sketch transaction boundary.

Block 108 adds no Enter-Sketch mutation. Shared topology is Core identity and is migrated/edited only
through explicit Core calls; entering a workspace does not silently rewrite historical Sketch JSON.

No Part feature is created by `Enter Sketch`.

## Finish Sketch

`GuiSketchWorkspace::finish(...)` is legal only when the workspace is active and no command,
numeric-input stage, preview, selected handle, or drag candidate remains active.

Finish runs the existing Block-99 Sketch inspection/diagnostic authority. Diagnostic errors reject
Finish; warnings remain visible and do not become hidden persistent consent. Finish does not
synthesize Extrude, Revolve, or another downstream feature.

On success the controller restores previous workspace and semantic selection. `MainWindow` clears
transient Sketch focus, restores Eye/Target/Up/Scale/projection/plane-camera publication and selection
filter, removes the crosshair, and clears active Sketch workspace state. Block 107 clears its scene,
grid, hover, snap marker, inference guide, and box-selection products.

For example, entering `sketch.bracket` from Inspect with a datum selected clears the working
selection, edits normal to the Sketch plane with surrounding Bodies dimmed, and returns to Inspect
with the same datum selection and camera when Finish succeeds.

## Frozen command state machine

Authoritative interaction stages are:

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

`GuiSketchWorkspace` projects these stages onto existing `GuiTaskState`:

```text
CollectingPicks / SelectedHandle -> CollectingSelection
NumericInput                     -> EditingParameters
Preview / DragCandidate          -> Preview
```

There is no second GUI transaction or undo authority. Block-108 `SketchTopologyUndoStack` is a
headless Core snapshot utility for topology commands; document-level GUI commits still use the
existing session transaction/history path.

## Escape and focus rules

`Esc` backs out exactly one command stage where a prior stage exists:

```text
Preview         -> NumericInput
NumericInput    -> CollectingPicks
CollectingPicks -> cancel command -> Idle
Hover           -> Idle
```

A selected-handle or drag-candidate command cancels atomically to Idle because Block 110 owns
solver-backed intermediate drag reconstruction. Block 106 only freezes transient lifecycle.

Numeric input owns keyboard focus in `NumericInput`. `Enter` accepts the current field for the active
command's candidate validation. Canvas owns ordinary selection/placement focus; task panels may take
focus for property editing. `MainWindow` routes unconsumed `Esc` to the Sketch workspace controller.

While the viewport remains on the published normal-to-plane `GuiPlaneCamera`, right-drag orbit is
disabled. A deliberate standard-view change clears that plane-camera state and enables the existing
orbit gesture. A short right click in active Sketch routes to contextual menu handling; the current
shell exposes `Repeat last Sketch command` when compatible and idle.

For example, while the line HUD contains `line.1, 0, 0, 25, 0`, first `Esc` returns from numeric
entry to pick collection and second `Esc` cancels. Neither persists geometry.

## Numeric HUD and Preview/Commit

The numeric HUD is transient. Its text is not model identity and is never serialized.

The first wired command remains the historical planar line authoring path:

```text
id, start-x, start-y, end-x, end-y
```

Coordinates are interpreted in the active Sketch plane. Before commit, `MainWindow` creates a
disposable Sketch copy and validates the candidate through `Sketch::add_entity(...)`. Only a valid
candidate advances to Preview. Applying ends transient Preview and the existing
`GuiSketchWorkbench::add_line(...)` path commits one document transaction.

For example, `line.web, 0, 0, 40, 0` validates on a copy, previews, and commits through the same
Part-document mutation used by headless callers. Duplicate entity id fails before persistent Sketch
replacement. After success, the Block-107 binder rebuilds its read-only interaction projection.

Block 108 does not retrofit this initial HUD command to topology-native creation early. Blocks 110–111
connect solver-backed drag and creation workflows to Block-108 topology commands in sequence.

## Command repeat

A successfully committed repeatable Sketch command records command id and tool hint as transient
workspace state. `repeat_last_command(...)` may restart only from Idle/Hover with no active generic
task.

`Sketch -> Repeat last Sketch command` and the active-Sketch context menu expose this authority. The
initial line integration reopens the HUD for `sketch.line`. Repeat state clears on workspace exit.

## Selection filters and semantic identity

The frozen Sketch mask is:

```text
SketchEntity | Edge | Vertex
```

The mask reuses `GuiSelectionKind` and the existing viewport semantic bridge. No AIS owner,
`TopoDS_Shape`, screen coordinate, hover object, or Block-107 candidate id becomes persistent
identity.

Block 107 hit and box selection publishes stable Sketch semantic ids into `GuiSelectionModel`.
Direct hit/box selection is disabled while the task state collects picks, edits numeric input, or
previews a command.

Block 108 adds `SketchPointId` as Core topology identity. The current GUI selection model does not
synthesize point ids from screen hits. Later solver-backed handle/creation commands resolve accepted
semantic picks and candidate positions through explicit Core topology operations.

The pre-Sketch filter mask is restored on Finish.

## Cursor, snap, DOF, and solve status

`GuiSketchWorkspaceStatus` is the transient presentation model for:

- tool hint;
- cursor coordinates in plane space;
- snap/inference text;
- optional remaining DOF;
- solve status;
- current interaction stage and focus target.

Producer staging is:

- Block 107 owns mapping, hit testing, snapping, inference, raw cursor coordinates, and selected
  snap/inference text;
- Block 108 owns persistent shared point/entity topology but does not publish solve status;
- Block 109 owns general planar solve state and remaining DOF;
- Block 110 owns solver-backed drag candidates.

Therefore the workspace may currently show real Cursor/Snap values while `DOF: —` and
`Solve: Not evaluated` remain explicit. The UI does not infer solver results from topology migration.

## Camera bookmark and Sketch focus

`OcctViewport::camera_bookmark()` captures:

```text
Eye
Target
Up direction
Scale
Perspective/Orthographic projection
published plane-camera state
```

`restore_camera_bookmark(...)` rejects non-positive scale and restores view state without persisting
camera data in Part or Project JSON.

`set_sketch_focus(...)` and `clear_sketch_focus()` are transient. The focus id is a stable Sketch
semantic id used only to identify `sketch/<SketchId>/...` viewport presentations. `Dim` keeps active
Sketch presentations opaque and increases surrounding transparency. `Isolate` erases non-active
presentations from transient AIS context. Clearing focus restores ordinary presentation policy.

## Transaction and persistence boundaries

Block 106 introduces no JSON field. Block 107 adds transient interaction products only. Block 108
adds canonical Core topology persistence in separate `blcad.sketch_topology.mvp8` JSON and an explicit
migration boundary from historical PartDocument Sketch JSON; entering/exiting the GUI workspace does
not run persistence migration implicitly.

Persistent document mutation still goes through `GuiDocumentSession::commit_part_transaction(...)`,
which validates and recomputes a candidate before publishing one canonical session history entry.
Preview state, HUD text, repeat state, hover, cursor coordinates, snap text, focus, workspace/camera
bookmarks, screen mappings, grid products, hit cycles, and status are transient.

Block-108 topology commands additionally provide exact Core `before`/`after` topology snapshots.
When a topology edit is projected back to current PartDocument Sketch records, the compatibility
bridge requires lossless re-migration and then uses atomic `PartDocument::update_sketch(...)`.

## Failure policy

The workspace fails closed when:

- Enter Sketch has no Part, has an active command, names a missing Sketch, or cannot resolve workplane;
- camera orientation cannot be established;
- Finish is requested while a command/drag stage remains active;
- existing Sketch diagnostics contain errors;
- a numeric candidate fails Core validation;
- camera restore receives an invalid bookmark.

A failed Enter leaves previous workspace authoritative. Failed Finish leaves Sketch workspace active.
Failed candidate does not replace persistent Sketch intent. Block-107 mapping/scene failures publish
a GUI diagnostic and leave persistent Sketch unchanged. Block-108 topology/migration/edit failures
are Core failures and likewise publish no partial PartDocument mutation.

## Focused proof

Block-106 tags:

```text
[gui][sketch-workspace]
[gui][sketch-command-lifecycle]
[gui][viewport][gui][navigation]
```

They cover context capture/restore, command/browser surface order, selection mask, status publication,
staged `Esc`, repeat, handle/drag-candidate transience, generic task backtracking, camera bookmark
restoration, Dim/Isolate focus, and the shell path Enter Sketch -> numeric line commit -> repeat/cancel
-> Finish Sketch.

Block 107 adds:

```text
[gui][sketch-hit-test]
[gui][sketch-snap]
[gui][sketch-box-selection]
```

Block 108 adds:

```text
[core][sketch-topology]
[core][sketch-edit-command]
[core][sketch-json-migration]
```

## Next boundary

Block 109 owns deterministic general planar constraint solving over Block-108 shared point/entity
topology. It produces remaining DOF and solve-state diagnostics for the existing Block-106 status
surfaces. The workspace and Block-107 interaction layer remain presentation/application clients and
must not implement solver mathematics in Qt.

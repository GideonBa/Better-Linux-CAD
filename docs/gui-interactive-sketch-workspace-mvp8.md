# Interactive Sketch Workspace MVP-8

Status: implemented in Block 106.

This document is the canonical GUI contract for the contextual planar Sketch workspace introduced by Block 106. It extends the Block-95/96 command and transaction rules and the Block-99 Sketch workbench. It does not introduce new persistent Sketch geometry, solver mathematics, snapping, or hit-testing semantics.

## Scope

Block 106 promotes planar Sketch editing from a camera mode into a real contextual workspace. `GuiWorkspace::Sketch` is transient GUI state. It is never serialized and never changes the Part feature graph merely because the user enters or leaves a Sketch.

The workspace publishes five command groups in frozen order:

```text
Create | Constrain | Dimension | Modify | Project
```

The Sketch browser surface is organized around:

```text
Entities | Constraints | Dimensions | Diagnostics
```

The existing model browser remains the semantic selection authority. The contextual Sketch surface changes presentation and command availability; it does not create a second browser model or second selection identity system.

## Enter Sketch

`GuiSketchWorkspace::enter(...)` requires:

- an active Part document;
- no active `GuiTaskState` command;
- an existing planar `SketchId` in the active Part;
- a workplane that resolves through the existing `geometry::WorkplaneResolver` path used by `GuiSketchWorkbench::plane_view(...)`.

Before the contextual workspace becomes active, it captures the previous GUI workspace and the complete semantic selection list. `MainWindow` captures a transient `GuiViewportCameraBookmark` and the current viewport selection-filter mask.

After successful entry:

- `GuiDocumentSession::workspace()` is `GuiWorkspace::Sketch`;
- the regular Part/Assembly/Inspect/Exchange tabs remain the global workspace tabs, with Sketch treated as a contextual Part mode;
- the active workplane camera is normal to the resolved plane and orthographic;
- the viewport uses the frozen Sketch selection mask: `SketchEntity | Edge | Vertex`;
- the viewport cursor becomes a crosshair;
- the active Sketch presentation remains undimmed while the surrounding model uses the transient `GuiSketchSurroundingsMode::Dim` focus policy;
- the Sketch command-group surface and cursor/snap/DOF/solve status surfaces become visible;
- the prior semantic selection is retained only as a restore bookmark and the active Sketch starts with a clear working selection.

`OcctViewport` also exposes the alternative transient `GuiSketchSurroundingsMode::Isolate` policy. Both policies are view state only. The default `MainWindow` entry path uses `Dim`; no visibility or transparency intent is written to the Part.

No Part feature is created by `Enter Sketch`.

## Finish Sketch

`GuiSketchWorkspace::finish(...)` is legal only when the Sketch workspace is active and no command, numeric-input stage, preview, selected handle, or drag candidate remains active.

Finish runs the existing Block-99 Sketch inspection/diagnostic authority. Diagnostic errors reject Finish; warnings do not become hidden persistent consent and remain available through the diagnostic surface. The command does not synthesize an Extrude, Revolve, or any other downstream feature.

On success the controller restores the previous GUI workspace and semantic selection. `MainWindow` clears the transient Sketch focus, restores the captured viewport Eye, Target, Up, Scale, projection, plane-camera publication, and selection-filter mask, removes the crosshair, and clears the active Sketch workspace state.

For example, entering `sketch.bracket` from the Inspect workspace with a datum selected clears the working selection, edits normal to the Sketch plane with surrounding Bodies dimmed, and returns to Inspect with the same datum selection and camera when Finish succeeds.

## Frozen command state machine

The authoritative interaction stages are:

```text
Idle
Hover
CollectingPicks
NumericInput
Preview
SelectedHandle
DragCandidate
```

The creation/edit path is:

```text
Idle -> Hover -> CollectingPicks -> NumericInput -> Preview -> commit | cancel
```

The direct-manipulation path is:

```text
Idle -> SelectedHandle -> DragCandidate -> commit | cancel
```

`GuiSketchWorkspace` projects these Sketch-specific stages onto the existing generic `GuiTaskState` authority:

```text
CollectingPicks / SelectedHandle -> CollectingSelection
NumericInput                     -> EditingParameters
Preview / DragCandidate          -> Preview
```

There is no second transaction or undo authority.

## Escape and focus rules

`Esc` backs out exactly one command stage where a previous stage exists:

```text
Preview         -> NumericInput
NumericInput    -> CollectingPicks
CollectingPicks -> cancel command -> Idle
Hover           -> Idle
```

A selected-handle or drag-candidate command cancels atomically to Idle because Block 110 owns solver-backed intermediate drag reconstruction. Block 106 only freezes the transient lifecycle.

Numeric input owns keyboard focus while `NumericInput` is active. `Enter` accepts the current numeric field for the active command's candidate validation. The canvas owns ordinary selection/placement focus; the task panel may explicitly take focus for property editing. `MainWindow` routes unconsumed `Esc` into the Sketch workspace controller.

While the viewport is still on the published normal-to-plane `GuiPlaneCamera`, right-drag orbit is disabled. A deliberate standard-view change clears that plane-camera state and makes the existing orbit gesture available again. A short right click in an active Sketch routes to the contextual menu hook; `MainWindow` currently exposes `Repeat last Sketch command` there when the last command is compatible and the workspace is idle.

For example, while the line HUD contains `line.1, 0, 0, 25, 0`, the first `Esc` returns from numeric entry to pick collection and the second cancels the line command. Neither action persists geometry.

## Numeric HUD and Preview/Commit

The Block-106 numeric HUD is transient. Its text is not model identity and is never serialized.

The first wired command is the existing planar line authoring path. The HUD accepts:

```text
id, start-x, start-y, end-x, end-y
```

Coordinates are interpreted in the active Sketch plane. Before commit, `MainWindow` creates a disposable Sketch copy and validates the candidate line through the existing Core `Sketch::add_entity(...)` authority. Only a valid candidate advances to Preview. Applying the generic task ends transient Preview and the existing `GuiSketchWorkbench::add_line(...)` path commits one document transaction.

For example, `line.web, 0, 0, 40, 0` validates on a copy, previews, and then commits through the same Part-document mutation used by headless callers. A duplicate entity id fails before the persistent Sketch is replaced.

## Command repeat

A successfully committed repeatable Sketch command records its command id and tool hint as transient workspace state. `repeat_last_command(...)` may restart it only from Idle/Hover with no active generic task.

The `Sketch -> Repeat last Sketch command` action and the active-Sketch right-click context menu expose this authority. The initial line integration reopens the numeric HUD for `sketch.line`. Repeat state is cleared when the user leaves the Sketch workspace.

## Selection filters and semantic identity

The frozen Block-106 Sketch mask is:

```text
SketchEntity | Edge | Vertex
```

The mask reuses `GuiSelectionKind` and the existing viewport semantic bridge. No AIS owner, `TopoDS_Shape`, screen coordinate, or hover object becomes persistent identity.

The pre-Sketch filter mask is restored on Finish.

## Cursor, snap, DOF, and solve status

`GuiSketchWorkspaceStatus` is the transient presentation model for:

- tool hint;
- cursor coordinates in plane space;
- snap/inference text;
- optional remaining DOF;
- solve status;
- current Sketch interaction stage and focus target.

Block 106 provides and renders these surfaces. Their producers are intentionally staged:

- Block 107 owns device-independent plane mapping, hit testing, snapping, and inference;
- Block 109 owns general planar solve state and remaining DOF;
- Block 110 owns solver-backed drag candidates.

Until those authorities exist, the Block-106 UI reports explicit unavailable/default states such as `Cursor: —`, `Snap: —`, `DOF: —`, and `Solve: Not evaluated` rather than inventing results.

## Camera bookmark and Sketch focus

`OcctViewport::camera_bookmark()` captures transient view state:

```text
Eye
Target
Up direction
Scale
Perspective/Orthographic projection
published plane-camera state
```

`restore_camera_bookmark(...)` rejects non-positive scale and restores the captured state without persisting any camera data in Part or Project JSON.

`set_sketch_focus(...)` and `clear_sketch_focus()` are likewise transient. The focus id is a stable Sketch semantic id used only to identify `sketch/<SketchId>/...` viewport presentations. In `Dim` mode, active-Sketch presentations remain opaque and surrounding presentations are displayed with increased transparency. In `Isolate` mode, non-active presentations are erased from the transient AIS context. Clearing focus displays all presentations again and restores the normal datum/non-datum transparency policy.

This closes the Block-106 requirement that normal-to-plane Sketch editing must return the user to the previous camera instead of forcing a generic isometric fallback and that surrounding model presentation follows an explicit transient focus policy.

## Transaction and persistence boundaries

Block 106 introduces no new JSON field and no schema migration.

Persistent geometry mutation still goes through `GuiDocumentSession::commit_part_transaction(...)`, which validates and recomputes the candidate before publishing one canonical snapshot/history entry. Preview state, HUD text, command repeat, hover, cursor coordinates, snap text, focus, workspace bookmarks, camera bookmarks, Sketch-focus policy, and Sketch status are transient only.

## Failure policy

The workspace fails closed when:

- Enter Sketch has no Part, has an active command, names a missing Sketch, or cannot resolve its workplane;
- camera orientation cannot be established;
- Finish Sketch is requested while a command/drag stage remains active;
- existing Sketch diagnostics contain errors;
- a numeric candidate fails Core validation;
- camera restore receives an invalid bookmark.

A failed Enter leaves the previous workspace authoritative. A failed Finish leaves the Sketch workspace active. A failed candidate does not replace persistent Sketch intent.

## Focused proof

Focused tests use:

```text
[gui][sketch-workspace]
[gui][sketch-command-lifecycle]
[gui][viewport][gui][navigation]
```

They cover context capture/restore, command/browser surface order, the selection mask, status publication, staged `Esc`, repeat, handle/drag-candidate transience, generic task backtracking, camera bookmark restoration, transient Dim/Isolate Sketch focus state, and an end-to-end shell path that enters a Sketch, commits a numeric line, repeats/cancels the command, and finishes back into Part context. Existing Block-99 tests continue to prove normal-to-plane Sketch entry and the shared workbench mutation path.

## Next boundary

Block 107 owns plane interaction, zoom-stable hit testing, box selection, grid, snapping, and inference preview. Block 106 exposes the transient status and command lifecycle those producers will drive; it does not implement their geometry or selection algorithms early.

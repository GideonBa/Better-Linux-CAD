# GUI Selection-First Modeling Workspace MVP-9

Status: the Block-122 application authority `GuiModelingWorkspace` is retained. Its original Qt
adapter was deleted with the Block-133 shell reset. Block 136 now restores camera navigation through
`ShellNavigationTools` on `ShellWindow`, as detailed below. Visible selection filters are also
restored. Contextual commands and repeat presentation remain open; the old mini-toolbar/catalog adapter specifics are historical.

Block 122 introduces the shared application-layer interaction boundary used by the Part, Surface, and
Assembly modeling workspaces. It upgrades the existing MVP-7 form-driven workbenches with
capability-driven preselection, contextual command recommendations, visible transient navigation
controls, and a single command-start lifecycle without adding persistent Part or Assembly intent.

## Authority boundary

```text
semantic viewport/browser selection
  + verified geometric capabilities
  -> GuiModelingPreselection
  -> GuiModelingWorkspace command catalog
     exact first-input compatibility
     contextual mini-toolbar ordering
     selection-filter synchronization
  -> GuiTaskState command start
     consumes preselection
     Apply records repeatable command
     Cancel restores complete preselection context
  -> Block-123 transient viewport manipulators
  -> existing MVP-7 Part / Surface / Assembly workbench
  -> existing GuiDocumentSession candidate transaction
```

`GuiModelingWorkspace` is transient GUI/application state. It owns no `PartDocument`, `Project`,
feature, relationship, joint, geometry result, or save-format field. Core and Geometry remain
unchanged. The existing workbenches remain the mutation and preview clients of public Core/Geometry
contracts.

The historical `GuiModelingWorkspaceShellBinder` was the Qt adapter owned by `MainWindow`. It added
the visible Part/Surface/Assembly modeling tabs, selection-filter control, Repeat command, contextual
floating mini-toolbar, ViewCube menu, Home capture, and named camera-bookmark controls. Stable Qt
object names make each control reachable by offscreen acceptance tests without making widgets model
authority.

## Workspace areas and command catalog

The frozen modeling areas are:

```text
Part | Surface | Assembly
```

Part and Surface require an active Part document. Assembly requires an active Project. Switching an
area is rejected while a task is active. Area switching clears stale preselection and restores the
workspace-wide selection filter in both the session and viewport.

The catalog records, for every exposed command:

```text
stable command id
label
modeling area
document kind
required first selection kind
required geometric capability
contextual mini-toolbar priority
repeatability
```

The catalog covers the interactive families owned by Blocks 124–130: Extrude/Revolve/path Extrude,
Sweep/Loft, finishing, patterns/mirror, Body Boolean/Transform, Surface operations, component
placement, relationships, joints, and joint drives. Block 122 freezes selection-first command start;
Block 123 now supplies reusable candidate-only manipulators. Feature-specific previews, authoring, and
motion remain owned by their later blocks.

## Capability-driven preselection

A `GuiModelingPreselection` combines one stable `GuiSelection` with a deduplicated set of verified
capability names. Command enablement requires all of the following:

1. the command belongs to the active modeling area;
2. the active document kind matches;
3. no task is active;
4. the semantic selection kind matches the command's first-input kind;
5. the preselection explicitly contains the command's required capability.

The workspace does not infer capabilities from semantic-id spelling, display shape, OCCT topology, or
widget-local type guesses. For example, a selected Sketch region carrying `ProfileRegion` enables the
profile-first command family, while an `Edge` selection enables Fillet/Chamfer. Unsupported or
filter-hidden selections clear transient capability context and enable nothing.

The contextual mini-toolbar is a deterministic priority-sorted subset of enabled commands. The Qt
binder positions it next to the latest viewport click and exposes a Cancel action while a Block-122
command-start task is active. It is a presentation product and is never persisted.

## Command lifecycle, cancellation, and repeat

Starting a command copies and consumes the current preselection, clears the shared selection model,
and enters the existing `GuiTaskState` collecting-selection stage. The owning later feature command
continues through parameter editing and preview.

```text
preselection -> begin command -> collect remaining inputs
             -> Block-123 candidate manipulation
             -> feature-specific preview -> Apply | Cancel
```

Apply records the last repeatable command after the task reaches preview and is accepted. Repeat
starts the same command id in the current compatible area without inventing prior parameter or
selection state. Cancel or `Esc` restores both the consumed semantic selection and its complete
capability context, so the mini-toolbar and command enablement return exactly to their pre-command
state. Active Block-123 handles are likewise transient and can be cleared without a document rollback.

## Finish Sketch handoff

`finish_sketch_handoff(...)` accepts an authoritative `SketchId` and `ProfileId`, verifies that the
Sketch exists in the active Part and that the profile is already present as one of the persistent
Sketch profile families, returns to the Part area, and publishes one transient profile-region
preselection:

```text
profile-region:<sketch-id>:<profile-id>
capability: ProfileRegion
```

A geometrically detected but not materialized region is rejected. The `MainWindow` Finish Sketch
adapter preserves the existing camera/filter/workspace exit and performs the handoff only when the
inspected region resolves to an existing profile. This gives Extrude/Revolve and the other
profile-first commands the same command-start path as a viewport-picked region without creating a
second profile, altering the Sketch, or persisting the transient semantic presentation id.

## Selection filters

The frozen filters are All, Profiles, Datums, Faces, Edges, Bodies, Components, and Assembly Targets.
The workspace maps each specific filter to the existing `GuiSelectionKind` bit mask and publishes the
same mask to both `GuiSelectionModel` and `OcctViewport`. `All` maps to the viewport's unfiltered
sentinel rather than a union of the modeling kinds, so activating the workspace never narrows viewport
selectability or overrides the default restored when a Sketch is finished. Changing a filter removes
incompatible transient selection and capability context instead of retaining an invisible command
source.

### Replacement-shell filter controls (Block 136)

`ShellWindow` owns the always-visible status-bar combo `blcad.selection.filter`, labelled Auswahl.
Its eight entries use the retained workspace mask mapping. Profiles denotes the existing
`SketchEntity` kind, including open-line selections; it does not infer a new geometry capability.
An incompatible browser selection may still show properties or activate an explicit edit task, but
cannot become model preselection or enable Create Sketch/Extrude through a hidden datum/profile.
Changing the filter also invalidates incompatible browser-only Finish Sketch fallbacks. Filters
restrict already-published semantic presentations; they do not create arbitrary final-Body subshape
IDs. Face/Edge/Assembly Target availability retains each command family's supported reference boundary.

The combo is disabled without a document, in reference-repair mode, during Sketch or any active task,
and while the viewport owns mouse capture. Slots check the same guard. Command components retain
ownership of their temporary viewport mask and exact semantic scope. The session temporarily accepts
command references during a task, so a Body filter cannot prevent a hole task from collecting a Datum
or a finishing task from collecting an Edge. The hole task explicitly saves its prior viewport
mask, semantic scope and actual selection, temporarily permits Body/Datum with unrestricted scope,
and restores them on Apply/Cancel. If another task has already taken ownership, expired hole cleanup
preserves that task's mask/scope. Ending the task restores the chosen model filter and
removes incompatible session entries. Sketch editing similarly owns its temporary selection rules;
Finish restores the model filter and only offers a profile handoff if that filter permits it.

Filter changes are transient: they do not dirty Core, create Undo entries or alter saved bytes.
Save and Undo/Redo retain the choice; failed Open and cancelled replacement retain it. Successful
New Part, New Project and Open reset to All. No filter field is added to Part/Project persistence.
The focused `[block136-selection-filters]` suite covers the complete kind/filter matrix, browser
fallback rejection, task reference collection and lifecycle. `[shell-combined-selection-filters]`
authors a plate, corrects invalid finishing/hole input under a Body filter, collects workplanes,
changes the source depth, verifies history, re-enters Sketch and reopens/exports the final model.

## ViewCube, home view, and camera bookmarks

The workspace reuses `OcctViewport` camera authority for Isometric, Front, Back, Left, Right, Top, and
Bottom targets. Home is an explicit captured `GuiViewportCameraBookmark`. Named bookmarks store and
restore complete transient camera values: eye, target, up direction, scale, projection, and optional
plane camera.

Home and named bookmarks are intentionally session-local. They do not enter Part, Project, sidecar,
or exchange persistence. Saving an existing bookmark name replaces its transient camera snapshot;
removal affects no model history. Block 123 derives its transient camera mapping from the same
bookmark structure without persisting manipulator state.

### Replacement-shell camera navigation — Block 136, 2026-10-24

`ShellWindow` explicitly owns `ShellNavigationTools`, which uses the retained workspace camera API.
The View ribbon keeps Fit and the four direct Isometric/Front/Top/Right buttons; the
`blcad.navigation.view_cube` menu exposes all seven standard views and captured Home. Stable action
IDs are `view_isometric`, `view_front`, `view_back`, `view_left`, `view_right`, `view_top`,
`view_bottom`, `view_home` and `capture_home`, prefixed by `blcad.action.`. Home is unavailable until
explicitly captured; capturing again replaces it.

The editable `blcad.navigation.bookmarks` combo selects an existing named view or accepts a new name.
`bookmark_save`, `bookmark_restore` and `bookmark_remove` use the same action prefix. Names are
trimmed, nonempty and case-sensitive; saving an existing name replaces its camera in place, while new
names retain insertion order. Unknown names cannot be restored or removed. Camera snapshots include
eye, target, up direction, scale, projection and optional plane camera. Projection controls
synchronize from the actual viewport after restoration without issuing a second camera command.

All controls use the active viewport camera. Standard orientation changes also update the fallback
camera without a native viewer, using OCCT's +Z-up orientation convention and orthogonal up vectors.
Projection changes notify camera consumers on both backends. Preview geometry, active task stage,
local invalid feature text, semantic selection filters/scopes, Core intent, dirty state and history
stay independent of camera commands. Native mappings keep using the real OCCT projection/ray API.

Model navigation, Home/bookmark capture and restoration, and projection switching are disabled in an
active planar Sketch and while the viewport owns a grip mouse grab. Command callbacks also guard
against those states. Fit remains available in a Sketch while preserving its normal-to-plane camera.
Finish Sketch restores the preceding model camera and re-enables navigation. Navigation is available
during a feature preview, including an invalid candidate; it does not make a failed candidate
applicable. A restored model view drives the existing transient manipulator mapping.

Home and named views remain outside document/exchange persistence and model history. Save and
Undo/Redo preserve the transient catalog. Successful New Part, New Project and Open clear it;
cancelled replacement and failed Open preserve it. This boundary avoids carrying named cameras from
an unrelated document while retaining them during ordinary modeling.

Current proof: five `[block136-navigation]` scenarios in `gui_shell_navigation_tests.cpp` and the
fully Qt-authored `[shell-combined-navigation]` workflow. The latter creates a plate/pocket, uses the
ViewCube menu and saved views, re-enters Sketch, corrects invalid taper text, cancels a real extent
drag with Escape after restoring a camera, applies a cut, changes the source depth, edits an earlier
feature, verifies three-step exact history, reopens canonically and exports STEP. Native tests execute
the same controls and validate the actual camera frames. The viewport-selection family remains
partial for contextual/repeat controls; Block 136 remains open.

## Focused proof

The `[gui][modeling-workspace]`, `[gui][in-context-command]`, and `[gui][view-navigation-aids]` cases
asserted against the deleted MainWindow shell and were removed with Block 133; Block 136 restores them
against `ShellWindow`. `GuiModelingWorkspace` is meanwhile exercised through the Block 124–130
controller proofs:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][extrude-revolve]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][interactive-modeling]"
```

The historical proof covered capability-exact command enablement, deterministic mini-toolbar
ordering, preselection consumption and complete Cancel restoration, rejection of a non-materialized profile,
materialized Finish Sketch handoff, command repeat, selection-filter rejection and viewport
synchronization, visible shell ownership through `MainWindow`, ViewCube targets, home restoration,
and named camera bookmark replacement/removal.

## Following boundary

Block 123 is implemented in `docs/gui-viewport-manipulators-mvp9.md`. Block 124 now consumes the
selection-first workspace and candidate-only manipulator layer for interactive Extrude, path Extrude,
and Revolve authoring.

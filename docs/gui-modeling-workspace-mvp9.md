# GUI Selection-First Modeling Workspace MVP-9

Status: implemented in Block 122; consumed by the Block-123 manipulator layer.

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

`GuiModelingWorkspaceShellBinder` is the Qt presentation adapter owned by `MainWindow`. It adds the
visible Part/Surface/Assembly modeling tabs, selection-filter control, Repeat command, contextual
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
The workspace maps each filter to the existing `GuiSelectionKind` bit mask and publishes the same mask
to both `GuiSelectionModel` and `OcctViewport`. Changing a filter removes incompatible transient
selection and capability context instead of retaining an invisible command source.

## ViewCube, home view, and camera bookmarks

The workspace reuses `OcctViewport` camera authority for Isometric, Front, Back, Left, Right, Top, and
Bottom targets. Home is an explicit captured `GuiViewportCameraBookmark`. Named bookmarks store and
restore complete transient camera values: eye, target, up direction, scale, projection, and optional
plane camera.

Home and named bookmarks are intentionally session-local. They do not enter Part, Project, sidecar,
or exchange persistence. Saving an existing bookmark name replaces its transient camera snapshot;
removal affects no model history. Block 123 derives its transient camera mapping from the same
bookmark structure without persisting manipulator state.

## Focused proof

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][modeling-workspace]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][in-context-command]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][view-navigation-aids]"
```

The proof covers capability-exact command enablement, deterministic mini-toolbar ordering,
preselection consumption and complete Cancel restoration, rejection of a non-materialized profile,
materialized Finish Sketch handoff, command repeat, selection-filter rejection and viewport
synchronization, visible shell ownership through `MainWindow`, ViewCube targets, home restoration,
and named camera bookmark replacement/removal.

## Following boundary

Block 123 is implemented in `docs/gui-viewport-manipulators-mvp9.md`. Block 124 now consumes the
selection-first workspace and candidate-only manipulator layer for interactive Extrude, path Extrude,
and Revolve authoring.

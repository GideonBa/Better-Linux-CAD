# GUI Selection-First Modeling Workspace MVP-9

Status: implemented in Block 122.

Block 122 introduces the shared application-layer interaction boundary used by the Part, Surface, and
Assembly modeling workspaces. It upgrades the existing MVP-7 form-driven workbenches with
capability-driven preselection, contextual command recommendations, transient navigation state, and a
single command-start lifecycle without adding persistent Part or Assembly intent.

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
  -> existing MVP-7 Part / Surface / Assembly workbench
  -> existing GuiDocumentSession candidate transaction
```

`GuiModelingWorkspace` is transient GUI/application state. It owns no `PartDocument`, `Project`,
feature, relationship, joint, geometry result, or save-format field. Core and Geometry remain
unchanged. The existing workbenches remain the mutation and preview clients of public Core/Geometry
contracts.

## Workspace areas and command catalog

The frozen modeling areas are:

```text
Part | Surface | Assembly
```

Part and Surface require an active Part document. Assembly requires an active Project. Switching an
area is rejected while a task is active. Area switching clears stale preselection and restores the
workspace-wide selection filter.

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
placement, relationships, joints, and joint drives. Block 122 only freezes selection-first command
start; manipulators, previews, authoring, and motion remain owned by their later blocks.

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
filter-hidden selections enable nothing.

The contextual mini-toolbar is a deterministic priority-sorted subset of enabled commands. It is a
presentation product and is never persisted.

## Command lifecycle, cancellation, and repeat

Starting a command copies and consumes the current preselection, clears the shared selection model,
and enters the existing `GuiTaskState` collecting-selection stage. The owning later feature command
continues through parameter editing and preview.

```text
preselection -> begin command -> collect remaining inputs -> edit -> preview -> Apply | Cancel
```

Apply records the last repeatable command after the task reaches preview and is accepted. Repeat
starts the same command id in the current compatible area without inventing prior parameter or
selection state. Cancel restores both the consumed semantic selection and its complete capability
context, so the mini-toolbar and command enablement return exactly to their pre-command state.

## Finish Sketch handoff

`finish_sketch_handoff(...)` accepts an authoritative `SketchId` and materialized `ProfileId`, returns
to the Part area, and publishes one transient profile-region preselection:

```text
profile-region:<sketch-id>:<profile-id>
capability: ProfileRegion
```

This gives Extrude/Revolve and the other profile-first commands the same command-start path as a
viewport-picked region. The handoff does not create another profile, alter the Sketch, or persist the
transient semantic presentation id.

## Selection filters

The frozen filters are All, Profiles, Datums, Faces, Edges, Bodies, Components, and Assembly Targets.
The workspace maps each filter to the existing `GuiSelectionKind` bit mask and can publish the same
mask to both `GuiSelectionModel` and `OcctViewport`. Changing a filter removes incompatible transient
selection and capability context instead of retaining an invisible command source.

## ViewCube, home view, and camera bookmarks

The workspace reuses `OcctViewport` camera authority for Isometric, Front, Back, Left, Right, Top, and
Bottom targets. Home is an explicit captured `GuiViewportCameraBookmark`. Named bookmarks store and
restore complete transient camera values: eye, target, up direction, scale, projection, and optional
plane camera.

Home and named bookmarks are intentionally session-local. They do not enter Part, Project, sidecar,
or exchange persistence. Saving an existing bookmark name replaces its transient camera snapshot;
removal affects no model history.

## Focused proof

```bash
./build/dev-gui/blcad_gui_tests "[gui][modeling-workspace]"
./build/dev-gui/blcad_gui_tests "[gui][in-context-command]"
./build/dev-gui/blcad_gui_tests "[gui][view-navigation-aids]"
```

The proof covers capability-exact command enablement, deterministic mini-toolbar ordering,
preselection consumption and complete Cancel restoration, Finish Sketch handoff, command repeat,
selection-filter rejection and viewport synchronization, shell ownership through `MainWindow`,
ViewCube targets, home restoration, and named camera bookmark replacement/removal.

## Next boundary

Block 123 adds reusable transient linear, angular, radial, triad, and pattern manipulators with
numeric-HUD coupling. Block 122 deliberately emits command and navigation state only; it does not
preview or mutate feature parameters.

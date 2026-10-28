# GUI Selection-First Modeling Workspace MVP-9

Status: the Block-122 application authority `GuiModelingWorkspace` is retained. Its original Qt
adapter was deleted with the Block-133 shell reset. Block 136 now restores camera navigation through
`ShellNavigationTools` on `ShellWindow`, as detailed below. Visible selection filters are also
restored. `ShellRepeatTools` now restores command-repeat presentation through the current command
owners. `ShellContextTools` supplies the shared bounded profile, finishing, pattern, Body, Surface
and Assembly mini-toolbar described below. The old catalog adapter specifics are historical.

Block 122 introduces the shared application-layer interaction boundary used by the Part, Surface, and
Assembly modeling workspaces. It upgrades the existing MVP-7 form-driven workbenches with
capability-driven preselection, contextual command recommendations, visible transient navigation
controls, and a single command-start lifecycle without adding persistent Part or Assembly intent.

## Desktop appearance and language

The current desktop uses English command labels, task instructions, diagnostics, browser headings,
file dialogs, and newly generated default names. Existing authored names and persistent identifiers
remain unchanged when a document is opened. The application sets an English locale with decimal-point
numeric input, independently of the system display language.

The Hermes-inspired redesign supplies the existing layout and typography. The 2026-10-27 color-only
follow-up makes `configure_gui_appearance` use the same dark-violet/black palette in the application
and GUI acceptance runner. Navigation, panels and inputs use the reference's dark violet tones;
off-white text and light lavender accents keep controls, selection and focus readable. The modeling
canvas uses charcoal gray (`#181a1b`) and retains semantic geometry, selection and reference markings.
Controls remain square with solid fills; widget sizes, spacing and interactions remain unchanged.
Native file dialogs are disabled so document dialogs follow the same contract. The startup splash is
restored to its pre-redesign Archimedes artwork, italic white title and centered `LOADING` on black.

Bundled Barlow supplies readable body text, Barlow Condensed supplies navigation/section headings,
and IBM Plex Mono supplies scalar inputs and model readouts. These fonts are registered offline from
Qt resources; their original SIL OFL notices and exact upstream checksums accompany the assets.
The ribbon retains horizontal scrolling, stable action labels and shortcuts. Primary task acceptance
buttons explicitly declare a presentation role; secondary actions remain quiet. Browser/property
rows, focused inputs and checked commands have distinct selection/focus states, including disabled
controls. Diagnostics remain visible with a compact initial height.

The `[gui][appearance]` regressions inspect rendered pixels, readable contrast, registered fonts,
enabled/disabled arrow affordances, focus and button states. `[hermes-design-workflow]` combines
Sketch dimensions, preview/cancel/apply, feature editing, history, Save/Open and STEP export at
1024×640, 1280×800 and 1600×1000, bounded to the available native desktop at higher DPI. Compact
workspace navigation is checked through visible arrows and actual mouse hits. The implementation
plan, exact visual roles and final native evidence live in `docs/gui-hermes-design.md`.

The 2026-10-28 Pixelarticons follow-up supplies local, theme-colored icons through an explicit
presentation catalog. Ribbon/menu actions and task buttons keep their existing labels and shortcuts;
mapped context shortcuts keep those labels in tooltips and accessibility names. Browser icons derive
from typed node/record metadata, independently of authored names. Functions without a representative
pack symbol remain text-only. Compact ribbon commands retain the existing horizontal overflow and
fixed page height. The icon renderer and provenance contract live in `assets/gui/pixelarticons/README.md`.

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

### Replacement-shell profile context (Block 136)

`ShellWindow` explicitly owns `ShellContextTools` and the viewport child
`blcad.context.toolbar`. It offers Extrude, Revolve, Sweep and Loft in the existing deterministic
priority order, subject to the normal owner's remaining-input requirements.
`ShellFeatureTools::profile_context()` resolves current semantic selection or a Finish Sketch/browser
handoff against Core and records whether its profile is already materialized. Extrude/Revolve also
retain their verified candidate-only raw-region workflow; opening a context task does not persist it.
A verified single geometry line with no profile can offer Extrude for Thin authoring; it never gains
a ProfileRegion capability or Revolve/Sweep/Loft actions. Two-line chains do not become closed
profiles. The normal owner action must also be enabled, including its compatible-axis checks for Revolve.

The context actions `context_extrude`, `context_revolve`, `context_sweep`, `context_loft` and
`context_cancel` are explicitly owned wrappers; they invoke the existing Feature entry points without
changing their triggered handlers.
The toolbar uses the last viewport left-press position plus a small offset and clamps to the viewport;
browser/Finish handoffs use the last anchor, initially the upper-left corner. It hides if the viewport
is too small to fit complete buttons. Sketch focus, repair, viewport mouse capture and unrelated
active tasks hide it and reject command starts. Input/resize/presentation notifications update it;
there are no polling timers or semantic-id/widget discovery heuristics.

After the owner successfully starts a context task, the adapter consumes session/viewport selection
and remembers its complete entries plus the verified typed Sketch/Profile or open-line source.
Only Cancel remains in the toolbar while that same task is active. Context Cancel, task-panel Cancel
and Escape all use the owner cleanup path, then restore the prior actual selection and typed handoff
if it remains valid. Filter/scope ownership stays with the existing command. Apply discards the
consumed context and lets the normal owner publish its result and repeat record. It adds no second
transaction or history entry. Ribbon/Repeat starts retain their existing lifecycle.

Successful New/Open resets context state. Cancelled replacement retains an active task. Failed Open
uses the existing interaction reset: it cancels the draft and clears selection while retaining the
committed model. Save serializes committed intent while the disposable feature preview remains local.
History may remove the source; eligibility is resolved again against current Core, so stale handoffs
never offer commands. No toolbar, anchor, selection or capability state is persisted.

The Sweep/Loft additions require a materialized closed region; an open profile PathCurve is not
promoted to that capability. Sweep also requires an open trajectory, and Loft requires the normal
owner's available section inputs. After the original QAction starts the task,
`ShellFeatureTools::select_spatial_profile` matches the verified Sketch/Profile pair against the
task's typed inputs: Sweep replaces its default source, while Loft appends exactly one first section.
It rejects absent profiles and duplicate Loft sections. Geometry still owns preview validity; starting
Loft with only its first section is local, invalid for Apply and ready for further collection.
Ribbon/Repeat starts retain their existing source/list defaults. Path Extrude remains available through
the normal Extrude task's path direction controls rather than another context button.

Initial proof: six `[block136-profile-context]` cases and the twenty-fourth Qt-authored combined
workflow `[shell-combined-profile-context]`. The latter selects a native materialized profile,
starts Extrude through the actual toolbar, restores an invalid cancelled Cut, creates a Thin wall on
another plane, updates source depth, edits an earlier feature, verifies five-step exact history,
reopens canonically and exports STEP. Five `[block136-spatial-context]` cases additionally cover
remaining-input eligibility, exact nondefault profile seeding, all cancellation paths, role/filter/scope
restoration, raw-region exclusion, three-section atomic Apply, Save and replacement. The twenty-fifth
combined workflow `[shell-combined-spatial-context]` authors all its Sketch/plane/path inputs through Qt, cancels an
invalid Sweep, corrects a three-section Loft's order and G2 input, creates a second Sweep, changes its
twist parameter, edits the earlier Loft, verifies four-step history, reopens and exports STEP.
Assembly contexts are covered by the final handoff section below.

### Replacement-shell finishing contexts (Block 136, 2026-10-24)

The same explicitly owned toolbar offers `context_fillet`/`context_chamfer` for a single verified
Edge and `context_shell`/`context_draft` for a single verified Face. Idle subshape presentations are
published only under the explicit **Kanten** or **Flächen** model filter in a fresh standalone Part.
The All filter retains its existing Body/profile picks. Eligibility rejects multiple selections,
wrong kinds, hidden or missing Bodies, stale derived results and ambiguous or lost semantic roles.
`ShellFeatureTools::finishing_context()` resolves the exact selection through the current
`FinishingSelectionBuilder`; the typed handoff carries Command, BodyId and the builder's semantic
identity. It does not parse ids or infer topology indices.

The wrapper invokes the original finishing QAction, selects the exact Body even when it is not the
chooser's first row, then idempotently selects the verified target in that task's freshly built list.
It consumes complete preselection only after task creation. The task owns its Edge/Face filter and a
semantic scope restricted to that Body's currently offered targets. Apply and all cancellation routes
restore the previous filter and scope. Context/task Cancel and Escape also restore the consumed
session entries, candidate identity and actual viewport selection when their source remains valid.
Apply uses the normal atomic transaction and repeat record. Ribbon/Repeat keep their ordinary fresh
selection defaults. There is no new Core intent or persistence field.

The scene cache includes the idle finishing mask as well as presentation revision, so model-filter,
Sketch and task transitions add/remove these presentations without requiring a Core mutation.
Unrelated tasks, repair, capture and Sketch focus retain the common toolbar guards. Successful
replacement resets context; failed Open cancels the local task; cancelled replacement retains it.
Save during a preview still serializes only committed intent.

Five `[block136-finishing-context]` cases cover both Body identities, kind/multiple-selection guards,
all four commands and cancellation paths, scope restoration, empty-target rejection, atomic history,
hidden/removed sources, Save and replacement. The twenty-sixth combined Qt-input workflow
`[shell-combined-finishing-context]` authors two Bodies, cancels Draft, shells one Body, fillets and
chamfers the other, changes two upstream parameters in one batch, edits an earlier feature, checks
five-step exact history, reopens canonically and exports STEP. It requires native OCCT subshape hits
and explicitly skips offscreen. Assembly contexts are covered by the final handoff section below.

### Replacement-shell Body pattern contexts (Block 136, 2026-10-24)

Exactly one current, visible and recomputed solid Body offers `context_linear_pattern`,
`context_circular_pattern` and `context_body_mirror` when the existing command owner has its required
references. Body picking works under All or the explicit Bodies filter. The current Part and cache,
selection kind, filter and semantic scope verify the typed BodyId; id spelling alone, multiple
selections, stale/hidden/missing sources and surface Bodies do not grant this capability. Curved
solids remain valid independently of the finishing tools' rectangular topology roles.

The wrapper invokes the original QAction and idempotently seeds exactly the verified Body through
`ShellFeatureTools::select_pattern_context()`. It consumes complete preselection after the task has
started. Other Bodies remain available for explicit additional collection. Browser Body/reference
selection reaches the same Pattern owner as viewport selection. The normal Pattern/Mirror owner
keeps reference, count, extent, direction, preview and atomic Apply authority. Its temporary
semantic scope switches between all offered Bodies and the command's offered axes/planes during
reference picking. Apply and every cancellation route restore the previous filter and scope;
context/task Cancel and Escape also restore the complete still-valid consumed selection. Save and
replacement follow the existing common toolbar contract. No selection or context state is persisted.

Five `[block136-pattern-context]` cases cover exact nondefault curved-Body seeding, eligibility guards,
reference/scope transitions, idempotence, all cancellation routes, empty sources, atomic Apply,
history, Save and replacement. The twenty-seventh combined workflow
`[shell-combined-pattern-context]` authors two Bodies through Qt, cancels and applies Mirror, makes a
Circular pattern and a Linear pattern from the reflected result, stages three upstream parameter
changes in one batch, edits the earlier Mirror, checks five-step exact history, reopens and exports
STEP. Offscreen selects the source through the browser; native input hits the actual OCCT Body.
Assembly contexts are covered by the final handoff section below.

### Replacement-shell Body contexts (Block 136, 2026-10-25)

Exactly one verified visible Body with a fresh nonempty cache offers `context_body_translate`,
`context_body_rotate` and `context_body_scale`. Solid Bodies also offer `context_body_boolean`
when the original command has at least two eligible solids. Surface Bodies support transforms but
never Boolean targets. Eligibility respects the current standalone Part, workspace, selection kind,
filter and semantic scope; multiple, hidden, missing and stale sources fail closed.

The shared wrapper starts the original QAction and passes a typed command/BodyId to
`select_body_context()`. Boolean receives the exact target with an empty tool set; transforms receive
exactly that source. Repeated seeding is idempotent. Additional sources/tools remain explicit. The
original owner handles numeric values, references, preview, atomic Apply and repeat defaults.
Browser Body/axis picks now reach that same owner using a snapshot across scope transitions.

The task owns the offered Body scope, excludes the current Boolean target from tool picking and
switches to all offered targets or resolvable rotation axes for reference collection. Apply and
Cancel restore the prior filter/scope; context Cancel, task Cancel and Escape also restore the
complete still-valid consumed selection. Save persists accepted intent while leaving drafts active;
New/Open use the existing replacement lifecycle. Context state remains transient. Interleaved
Body operations preserve executable order with optional `feature_order` as specified in
`docs/file-format.md`.

Five focused `[block136-body-context]` cases cover role separation, surface eligibility, exact
handoff, idempotence, all cancellation routes, atomic Apply/history, Save and replacement. The
twenty-eighth Qt-input workflow `[shell-combined-body-context]` authors two solids, cancels multi-Body
translation, moves the nondefault target, switches Boolean targets through the browser, cuts with
retained tools, rotates using an explicitly picked axis and scales about a typed center. It checks
bounds/volumes, invalid-input recovery, four-step exact history, canonical reopening and STEP.
Native input hits actual OCCT Bodies; offscreen input uses the visible browser. Assembly contexts
are covered by the final handoff section below.

### Replacement-shell Surface contexts (Block 136, 2026-10-25)

The shared toolbar exposes `context_surface_boundary` and `context_surface_fill` for one verified
PathCurve or supported stable semantic edge. `context_surface_trim`, `context_surface_extend` and
`context_surface_stitch` accept a visible recomputed Surface Body or supported stable semantic face.
`context_surface_to_solid` accepts only a Surface Body; the original preview diagnoses open shells.
Current standalone Part, fresh cache, selection kind, active filter/scope and typed reference keys
validate the handoff. Unsupported, hidden, missing, multiple or foreign selections fail closed.
Stable solid subshapes use the existing explicit Edge/Face model filters and Geometry builder.

The original QAction owns command startup. An idempotent typed handoff seeds exactly one ordered
Boundary/Fill curve or Stitch surface, or the exact target for Trim/Extend/solid conversion. Trim and
Extend switch to their remaining contour/edge role. Other inputs stay explicit. Browser picks reach
the same Surface owner as viewport picks, with selection snapshots across scope transitions.
Apply/Cancel restore the prior filter and scope; all three cancellation routes restore complete
still-valid consumed preselection. Cancel removes task-only subshape presentations, so All cannot
pick expired task targets. Save/New/Open retain the existing disposable-draft lifecycle.

Wrapped Surface guidance stays outside dynamic form rows. Six `[block136-surface-context]` cases
cover typed source identity, stable solid roles, idempotence, scope restoration, all cancellation
routes, scene cleanup, atomic Apply/history, Save and replacement. The twenty-ninth combined Qt
workflow `[shell-combined-surface-context]` authors a box, closed PathCurves and trim profile, cancels
Boundary, applies Fill/Trim/Extend, rejects an open shell, sews six surfaces, converts the result to a
solid and changes upstream depth. It checks bounds/volumes, duplicate/removal/reordering controls,
six-step exact history, canonical reopening and STEP on both platforms. Assembly contexts are
covered by the final handoff section below.

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

### Replacement-shell repeat (Block 136)

`ShellWindow` explicitly owns `ShellRepeatTools`. The ribbon action
`blcad.action.repeat_last_command` shows **Erneut: …** and has the window shortcut Ctrl+R.
The explicit ribbon button `blcad.repeat.button` tracks the changing label in its minimum width,
so the command name remains readable in a narrow scrollable ribbon. Redo is separately labelled
**Wiederherstellen** and retains its standard shortcut/history semantics.
The record is transient and scoped to the current document. Save and Undo/Redo retain it; successful
New Part, New Project and Open clear it. Failed Open and cancelled replacement retain it.

The owning command reports its exact QAction only after successful atomic Apply. Supported owners
are `ShellFeatureTools` (Extrude/Revolve, finishing, patterns, Body, spatial/path and Surface tasks),
`ShellHoleTools` and `ShellAssemblyTools` (insertion, placement, relationships, joints, drive and solve).
Sketch/construction editors, parameter/feature edits, repair, measurement and file operations do not
replace this modeling-command record. Cancel, failed Apply and failed previews never replace it.

Repeat invokes the original owner's existing entry point with current compatible inputs and normal
new-task defaults. It stores no previous selection, dimensions, Body operation, path, references,
component definition or local draft. Extrude's path/Thin/extent options are consequently chosen again
in the fresh task. Repeating can open a collecting task that still needs its required inputs; it
never directly commits a second result. A changed profile is used for a repeated Extrude, while a
hidden/missing profile leaves Repeat disabled until the normal Extrude entry becomes available.

The source action is supplied by its owner rather than discovered or inferred from task/history
labels. A direct `QAction::changed` observer keeps enablement synchronized with late Finish Sketch
handoffs; existing triggered handlers are neither replaced nor augmented. `QPointer` guards lifetime.
The new action requires the source to be enabled, matching document kind, no active task or Sketch,
no repair mode and no viewport mouse capture. The same guards run in its trigger handler. Native
capture notifications update it without polling. The shell refreshes it after owner enablement.

Six focused `[block136-repeat]` cases cover accepted-only recording, cancellation/failed input,
current-profile/default behavior, hole creation, guards, canonical/history/replacement boundaries and
fresh Assembly insertion. The twenty-third combined workflow `[shell-combined-repeat]` uses actual
Qt input for two different holes, invalid placement recovery, Fillet, Ctrl+R/Escape, an upstream depth
edit, four-step exact history, canonical reopening and STEP. Contextual command presentation remains
open; this bounded repeat slice does not close the viewport family or Block 136.

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
covered together with the subsequent context sections; overall acceptance is recorded in
`docs/gui-shell-acceptance-mvp9r.md`.

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

## Assembly context handoff — Block 136, 2026-10-25

The shared context toolbar now dispatches to the existing Assembly owner as well as the Part
owners. One visible active root component offers Placement and the connected active constraint
Solve group. Motion is offered only when exactly one active, driven root joint touches that component;
an ambiguous joint choice remains explicit in the ordinary Motion task. The exact source is checked
again when starting and seeding the original QAction task.

The explicit Assembly Targets model filter publishes disposable semantic root targets. Selecting
one offers Relationship and Joint, seeds that exact reference A and leaves B for compatibility-filtered
collection. The All filter does not publish these additional targets. Filter changes, task completion
and cancellation rebuild the selection scene so expired task targets cannot remain pickable.

Context tasks preserve the previous filter, semantic scope and complete valid preselection. Apply
consumes it; task Cancel, context Cancel and Escape restore it. Busy tasks, pointer capture, multiple
selections, mismatched kinds, hidden/suppressed occurrences and stale source identities reject the
handoff. Browser component selection uses the same active owner as viewport selection. Nested
occurrences and cross-hierarchy records use the explicit hierarchy task editors documented in
`docs/gui-interactive-assembly-mvp9.md`.

Focused proof: `[block136-assembly-context]`; integrated Qt proof:
`[shell-combined-assembly-context]` in `gui_shell_combined_workflow_tests.cpp`.


## Complex-Part candidate recompute — issue #114 follow-up

Primary Extrude/Revolve, finishing, pattern/Boolean/Body Transform, path/Loft and surface previews
reuse a copy of the committed Part ShapeCache when the session is Fresh, the Part identity matches,
committed invalidation entries are Clean and the cached producer-ID set matches the executable
records exactly. Matching counts alone do not authorize reuse.
Legacy bodyless, missing/incomplete and stale caches use complete recomputation. The committed cache
and document remain authoritative; previews never publish their working cache into the session.
This work builds on issue #113's non-destructive legacy Cut builders so shared source topology is
preserved during disposable previews.

The candidate still validates reference sources. Explicit feature roots, newly introduced graph
nodes and all Changed/Dirty/error entries feed the root-inclusive Core plan. It executes affected
producer/consumer chains in graph order, including Body aliases and associative Boolean tools.
Pending parameters/profiles/bodies remain candidate-only. Unchanged source backing identities can
be retained while an edited upstream parameter recomputes its complete dependency branch.
Failure, cancellation, history and persistence contracts remain unchanged. Existing Apply/history
and general parameter/feature-edit paths still perform their original validation/recompute work;
this optimization does not claim that all complex-Part costs have disappeared.

`[preview-cache][complex-part-correctness]` uses one connected, visible rotor: a revolved hub and
20 exact spline blades, with a hidden editable source blade. It compares cached candidates with
complete recomputation, verifies retained clean producer identities and changed upstream inputs,
rejects incomplete or differently identified Fresh caches, checks stale-cache fallback and proves preview/Apply
volume equality and history. `[complex-part-workflow]` opens the fixture through the actual shell,
picks a native result edge, corrects an oversized Fillet radius, cancels/applies, translates, edits
hub height and exercises Undo/Redo, canonical reopening and STEP. Its volume oracles check the analytic source hub/blade volumes, the changed unrounded producer
and unchanged material removal by the remote tip Fillet.

The opt-in `[complex-part-profile]` probe covers 5/20/40 blades (90/360/720 offered edges), recording
first-call and repeated p50/p95 values plus raw samples, CPU, architecture, kernel, Qt and build type.
It separates selection catalog construction, Fillet preview, empty-cache recompute, JSON roundtrip,
scene building, native publication, redraw with readback and a queued zero-timer's delay during a
synchronous preview. These are component observations on a deterministic representative fixture;
queued-timer delay is not a physical-input latency distribution, and redraw/readback includes the
capture cost. The deep-sequence variant below supplies separate history, solver/drag and native
picking traces.
The probe is excluded from CTest and the default binary suite; commands live in
`docs/development-setup.md`. Timing observations do not impose portable release budgets.


### Recorded rotor comparison — 2026-10-28

Native xcb, Debug, GCC 13.3.0, OCCT 7.6.3, Qt 6.4.2, Intel Core i7-1370P,
Linux 7.0.0-38-generic. Each size has one first call and ten repeated samples; the table uses
nearest-rank p50/p95 of those ten repeats, in **milliseconds**. The after-run keeps one 1100×700 DIP
viewport at device-pixel ratio 1, matching the shell's document-replacement lifetime.

| Blades | Offered edges | Preview before p50 / p95 | Preview after p50 / p95 | Selection before p50 / p95 | Selection after p50 / p95 |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 5 | 90 | 246.7 / 299.2 | 35.0 / 39.4 | 27.6 / 32.0 | 3.4 / 4.9 |
| 20 | 360 | 1531.2 / 1582.9 | 61.9 / 69.0 | 400.9 / 432.8 | 16.6 / 18.6 |
| 40 | 720 | 5017.4 / 5211.7 | 101.9 / 117.2 | 1633.3 / 1679.3 | 41.5 / 47.2 |

Raw samples and environment metadata are committed as
[before](benchmarks/complex-rotor-preview-before.json) and
[after](benchmarks/complex-rotor-preview-after.json). The after-data also includes full-candidate
empty-cache execution and native publication/capture for both candidate implementations, with an
output-volume comparison on every iteration. Provisional investigation targets for this Debug
fixture are p95 below 200 ms for the repeated preview and below 60 ms per selection catalog; these
are observations and local targets, not agreed release budgets or portable CI timing assertions.

In the final persistent-view run, cached-preview Qt event processing plus QWidget capture has
p95 14.0 / 13.3 / 16.2 ms for 5 / 20 / 40 blades. Capture can itself trigger painting and is reported separately from
event processing. An initial run that created a new native viewport for each size recorded an
approximately 1,014 ms event/capture anomaly for the 20-blade cached result. An isolated 20-blade
probe and the persistent-view series instead measured 10–16 ms. No product renderer change is
included, and this does not establish the cause of separate multi-window capture behavior. Local
diagnostic data are retained in `/tmp/blcad-issue114-after-multiwindow.json`,
`/tmp/blcad-issue114-render-split.json` and `/tmp/blcad-issue114-persistent-view.json`.

The initial preview/selection change is a bounded optimization. Commit/history and existing
parameter/feature-edit recomputation remain full-model paths. Surface metadata validation now uses
the same affected-root execution as its shape preview, preserving early geometric diagnostics.
No worker or cross-thread topology publication is introduced; existing thread ownership and
modeling validation remain unchanged.

### Deep dependency and interaction acceptance

`rotor_performance_sequence_part` extends the same curved 5/20/40-blade fixtures with 4/12/24 serial
alignment translations, giving 7/15/27 executable producers. Each positive 0.05 mm translation is
paired with a negative one; the committed graph retains every operation and the actual rotor
remains at its original coordinates. These are connected visible solids with a hidden editable
source blade, rather than many independent boxes or suppressed geometry.

The default `[complex-part-sequence]` correctness test changes upstream hub height and compares
the complete affected chain against full recomputation. Every transform output must be replaced;
the independent blade backing must be retained. It authors and edits a driving blade-tip dimension,
queues 32 pointer positions, solves only the latest one, commits a curved-profile drag, exercises
Undo/Redo, rejects an older drag after history changes, and reopens the canonical document with
its accepted dimension catalog. The native shell workflow separately runs both 20 and 40 blades
through picked Fillet preview, invalid-radius recovery, Cancel/Apply, Body Translate, hub parameter
editing, complete Undo/Redo, Save/Open and STEP. No geometric tolerance changes support this proof.

With `BLCAD_PROFILE_SEQUENCE=1`, the opt-in profile separates baseline solver, driving-dimension
preview, latest-pointer drag, actual viewport mouse selection, full preview publication/event
processing/capture and actual Apply/Undo/Redo. Existing recompute, JSON roundtrip, scene assembly,
publication and capture distributions remain separate. Apply/history metrics include their own
serialization and recomputation; they are inclusive totals and must not be added to the standalone
component timings. Posted zero-timer delay measures event-loop availability during the synchronous
operation, not physical input or compositor latency. Mouse selection times the actual Qt event
handlers and OCCT picking but does not include OS input transport.

Local Debug investigation targets for the identified interactive path are warm p95 below **200 ms**
for geometry preview, **60 ms** for a selection catalog, **16 ms** for solver or a coalesced drag,
**20 ms** for native picking and JSON roundtrip, and **250 ms** for preview through publication,
event processing and capture. Queued event delivery should remain below **200 ms** in that path.
These targets are explicit PR acceptance/review criteria for this hardware and fixture, not
user-approved release SLAs or portable CI assertions. Normal regression tests enforce geometry,
history, source identity and final-input correctness regardless of machine speed. Apply/Undo/Redo
are measured separately to prioritize subsequent work; a passing preview target does not establish
their responsiveness. Release targets and physical input traces on the user's own model remain
product validation rather than claims of this deterministic probe.


Verification of the initial preview/selection commit `e278188` (2026-10-28): all five targets build. Full Core passes
**546 cases / 17,061 assertions**, Geometry **459 / 35,243**, and offscreen GUI
**554 passed / 20 expected native-only skips / 76,545 assertions** (574 cases). Expanded affected
native feature workflows plus required shell coverage pass **25 / 9,772**, with no skips. The rotor
workflow at 150% plus supplementary Pattern/Body/Path/Loft controller checks pass **9 / 828**, with
no skips. The isolated final 11-sample profile passes **1 / 870**, including all full/cached candidate
volume oracles. A separate native startup reaches the intentional six-second timeout (124).

Inspected the ordinary-scale recovered-preview capture and the 150% reopened-rotor capture. All **739**
frozen source/test/asset/build-definition hashes matched that tested commit before publication. Builds, source edits, GUI suites
and startup were serial; timing runs were isolated from other builds/tests. Final native/scaled/
profile/startup logs contain no High-severity, BadDrawable or BadWindow errors; the X11 visual keeps
its known OCCT Medium depth/stencil warning. Logs are
`/tmp/blcad-issue114-{build,core-full,geometry-full,gui-full,native-regression,native-150,after,startup}.log`;
captures are under `/tmp/blcad-issue114-native{,-150}/`. Reproduction commands are in
`docs/development-setup.md`. Initial rotor test fixtures were corrected to use the existing list-row
contract and unitless Parameter Manager values; the analytic volume relation uses `1e-8` relative
tolerance for the existing ShapeSummary mass integration. Geometry/modeling tolerances were not
widened. A review regression reproduces the count-only cache-eligibility gap (2 failed assertions
with the earlier guard); the final guard compares producer identities as well as cardinality.


### Recorded deep-sequence components

The isolated native Debug series uses the same CPU/compiler/OCCT/Qt environment as above, one
persistent 1100×700 DIP viewport at DPR 1, and five samples per size (first plus four repeats).
Warm p50 / p95 in milliseconds are below; with four repeats, p95 is the maximum observation.
Raw samples, source fingerprint, metadata and explicit local target assessments are committed in
[the deep-sequence report](benchmarks/complex-rotor-deep-sequence.json).

| Component | 5 blades / 7 producers | 20 blades / 15 producers | 40 blades / 27 producers |
| --- | ---: | ---: | ---: |
| Geometry preview | 35.6 / 38.5 | 64.3 / 66.1 | 116.4 / 131.7 |
| Selection catalog | 3.1 / 3.2 | 16.2 / 20.0 | 44.1 / 47.9 |
| Driving dimension preview | 12.7 / 12.9 | 14.4 / 16.4 | 12.2 / 15.3 |
| 32-pointer coalesced drag | 12.6 / 14.6 | 12.4 / 17.8 | 12.9 / 15.4 |
| Native mouse pick | 0.2 / 0.3 | 0.4 / 0.6 | 0.6 / 0.6 |
| JSON roundtrip | 13.8 / 15.7 | 16.9 / 17.8 | 21.4 / 24.0 |
| Preview + publication/events/capture | 55.8 / 61.0 | 106.6 / 119.2 | 181.9 / 199.3 |
| Apply including history/recompute | 273.2 / 283.4 | 1595.2 / 1666.4 | 5449.5 / 5558.9 |
| Undo including recompute | 222.4 / 237.7 | 1524.2 / 1575.2 | 5546.9 / 5831.4 |
| Redo including recompute | 260.9 / 270.5 | 1575.1 / 1841.1 | 5450.5 / 5706.6 |

The interactive geometry/selection, native-pick, complete preview and queued-event targets pass.
Two local targets miss: the 20-blade 32-pointer drag has p95 **17.8 ms** against 16 ms, and the
40-blade JSON roundtrip has p95 **24.0 ms** against 20 ms. These are recorded misses, not targets
raised after measurement. The baseline curved-Sketch solver is below 0.14 ms; dimension authoring
includes much more than that solve. No claim of a universal 60 Hz editing budget follows.

Measurements prioritize the work as follows. Full upstream geometry and repeated kernel witness
sampling dominated the original previews, so incremental candidate execution and single sampling
address those confirmed hotspots. The extra Surface validation pass now follows that same path.
Full commit/history reconstruction is the largest remaining measured cost (about 5–6 seconds for
40 blades) and should be the next optimization area, with atomic state publication and bounded
BRep retention reviewed explicitly. Native scene publication grows to p95 95.8 ms at 40 blades;
scene construction is only 2.2 ms and picking is below 0.7 ms. Retaining presentations may reduce
that publication cost, but no renderer change is justified as the first fix for a five-second
geometry rebuild. Existing drag coalescing is verified to process only the final sample of 32;
adding workers or a second coalescer is not part of this fix. These residual costs and soft target
misses remain visible for PR/product review rather than being hidden by a passing preview metric.


The expanded 40-blade workflow exposed an unsupported assumption in the original test oracle.
Even with adaptive `1e-10` integration, the unrounded 40-blade pattern at hub height 5 measures
107,822.446984188 mm³ while ideal fused-volume scaling from height 6 predicts 107,822.449022357 mm³.
The same approximately 0.002 mm³ discrepancy therefore predates Fillet and is not explained by
ordinary mass quadrature. The test does not require that exact aggregate-scaling assumption.
Instead it checks the analytic cylinder and source-blade volumes, monotonic valid fused results,
and unchanged remote Fillet removal against the current unrounded producer. The final comparison
is tightened from `1e-8`/`1e-5` to `1e-10` relative / `1e-6` absolute. Adaptive integration and its
reported error are isolated in the test-only Geometry support library; production mass integration,
modeling tolerances and rendering remain unchanged. The dialog watchdog still grants five seconds
to discover a modal dialog, then stops on the response so subsequent model evaluation is not
misclassified as missing test input.


Final Surface/deep-sequence verification (2026-10-29): all five targets build, including the
new private Geometry test-support library. Focused offscreen cache/Surface/dimension/drag gates
pass **3 cases / 620 assertions**. The isolated native five-sample sequence profile passes
**1 / 1,349**; final native Surface/Stitch, viewport lifecycle, required shell coverage and both
20/40-blade workflows pass **8 / 1,858**, without skips. Native recovered-preview and reopened
40-blade captures were inspected. A separate native startup reaches intentional timeout **124**.
No High-severity, BadDrawable or BadWindow messages appear in the final logs; the known OCCT Medium
visual warning remains. Logs are `/tmp/blcad-issue114-{sequence,accept-offscreen,accept-native-final-pass,accept-startup}.log`;
captures are `/tmp/blcad-issue114-accept-native/`.

The measured production/controller sources are unchanged after profiling. The later source changes
only refine native test dialog handling and the volume oracle, with a private test-support target.
The profile records its exact 739-file fingerprint; final native verification and startup preserve
all **741** final source/test/asset/build-definition hashes. This focused follow-up builds on the
initial broad Core/Geometry/GUI and scaled native proof above; those entire suites and the 150%
deep workflow were not repeated after native test-only refinements. Local budgets and the remaining
full-model history/edit costs are exposed for PR/product review rather than claimed eliminated.

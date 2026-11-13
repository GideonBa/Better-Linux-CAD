# GUI Shell Reset Sequence MVP-9R

Blocks **132–136 were implemented and accepted on 2026-10-25**. This file retains the shell
architecture and regression hazards, not an active rollout checklist. `docs/mvp-plan.md` owns
numbered status; `docs/gui-shell-acceptance-mvp9r.md` owns the evidence rules and final acceptance.
Superseded slice logs and intermediate states are available in Git history.

Current placement belongs to `docs/user-interface.md`: Block 144 reorganized the same shell into
a full-width ribbon, persistent File/Quick Access and Model/Sketch/Surface/Assembly/Inspect/Manage/
View tabs, Ctrl+K discovery and recoverable supporting docks. Later workflow contracts extend this
foundation; feature semantics stay with their existing feature documents.

## Why a reset

The old `MainWindow` and seven binders used implicit widget discovery, foreign QAction rewiring,
self-install timers, polling and competing workspace bars. Rendering into a foreign native window
also caused whole-window flicker. The replacement removed that shell and retained the headless
authorities. Do not reintroduce its installation or synchronization patterns.

## Authority declarations

- `GuiDocumentSession` owns lifecycle, transactions, command context and presentation revision;
  `GuiCommandRegistry`, `GuiSelectionModel`, `GuiDocumentBrowser` and `GuiTaskState` keep their roles.
- Core/Geometry and the Qt-free workbenches/controllers retain intent, validation and execution.
  Shell components consume the existing feature coordinators, `GuiModelingWorkspace`,
  `GuiViewportManipulatorLayer`, `GuiFeatureEditController`, `GuiInteractiveAssemblyCoordinator`
  and `GuiMeasureController` rather than creating another authoring authority.
- `ShellWindow` owns shell components and passes explicit references. Refresh browser/viewport
  products only when the presentation revision requires it; previews do not become persisted intent.
- The machine-readable bounded-family inventory is `docs/gui-feature-coverage-manifest-mvp9.json`.
  Historical controller proof and current shell proof remain separate.

## Shell architecture contract

Forbidden in the new shell layer:

- `findChild`-based discovery of widgets or actions across components;
- self-installing components (`QTimer::singleShot` install patterns);
- rewiring foreign QActions (`QObject::disconnect` of another component's connections);
- polling timers for state synchronization (state changes notify explicitly);
- more than one workspace/ribbon tab authority.

`ShellRibbon` owns the tab/context-mode surface. Sketch entry activates its tab and leaving restores
the previous tab. The model browser is built from `GuiDocumentBrowser`; typed context tools invoke
the original command owner. Feature editors use the shared task surface. Current layout/dock rules
are in `docs/user-interface.md`, not the retired pre-144 ribbon arrangement.

## Block sequence

| Completed block | Retained implementation responsibility |
| --- | --- |
| 132 | QOpenGLWidget/OCCT render lifecycle behind the existing viewport API |
| 133 | Explicit ShellWindow/Ribbon ownership, document lifecycle, origin geometry and Sketch entry |
| 134 | ShellSketchTools over retained create/constraint/dimension/drag/modify authorities |
| 135 | Selection-first Extrude/Revolve, disposable previews and shared manipulators |
| 136 | All eighteen bounded feature families, actual shell entry points and integrated/native closure |

### Block 132 — QOpenGLWidget viewport foundation — Implemented

`OcctViewport` stays a plain `QWidget` container (public API, overlays, headless fallback
unchanged) and hosts a mouse-transparent `OcctViewportRenderSurface : QOpenGLWidget` child. OCCT
renders into Qt's GL context using the official OCCT pattern: `OpenGl_Context` wrapping the
current context, `Aspect_NeutralWindow`, default-FBO wrapping (`InitWrapper`) in `paintGL`, driver
options `buffersNoSwap`/`buffersOpaqueAlpha`/`useSystemBuffer=false`, `SetImmediateUpdate(false)`.
`paint_gl()` is the only place OCCT redraws; every state change funnels through
`render_update()` (a scheduled Qt repaint). Input handlers pass device-pixel coordinates
(`to_render_point`, DPR-scaled). Native hover and left-button picking preserve `QPointF` event
positions until the single device-pixel rounding; rounding in DIP first can miss a thin edge at
fractional desktop scale. [Projection and picking precision](gui-viewport-manipulators-mvp9.md#native-projection-precision--2026-10-24)
owns the focused contract. Fail-closed behavior is unchanged: non-xcb platforms (offscreen
tests) never create the render surface and keep the painted fallback message.

X11 implementation constraints (discovered by crash analysis, do not regress):

- The `Aspect_NeutralWindow` needs a real X drawable native handle for OCCT's GLX visual query; a
  purely virtual window segfaults in `OpenGl_Window`. The handle must be queried inside
  `initializeGL` (the top-level's `winId()` — never the QOpenGLWidget's own), because Qt may
  recreate the top-level window when the GL child switches its surface type; an earlier-captured
  handle goes stale ("Bad Drawable").
- Qt batches window creation on its xcb connection; before OCCT touches the drawable over its own
  X connection an explicit round-trip (`xcb_get_input_focus`) must flush and wait, otherwise
  `XGetWindowAttributes` fails silently (Qt's non-fatal X error handler) and OCCT dereferences
  garbage. `blcad_gui` links `xcb` for this.
- The OCCT warning "window Visual is incomplete: no depth buffer, no stencil buffer" is cosmetic:
  it refers to the top-level's X visual; rendering uses the wrapped Qt framebuffer (depth 24,
  stencil 8 via `QSurfaceFormat`).

### Block 133 — Shell reset — Implemented

Entry points live in `include/blcad/gui/shell/` and `src/gui/shell/`:

| Component | Ownership and lifecycle |
| --- | --- |
| `ShellWindow` (`shell_window.*`) | Session, workbench/workspace, viewport, ribbon and docks; dirty confirmation, new/open/save, selection/browser synchronization and revision-gated refresh. `new_part`, `open_path`, `save_to`, `create_sketch_on_selection`, `finish_active_sketch` are shared command/test surfaces. |
| `ShellRibbon` (`shell_ribbon.*`) | Explicitly owned tabs, grouped command controls and menu actions. |
| `shell_origin.*` | `seed_origin_geometry(session)` creates datum.xy/xz/yz and datum.axis.x/y/z in one transaction and rejects duplicates. |

`DatumPlane::xz()/yz()` preserve their JSON kinds; unknown kinds reject. The browser groups the six
origin IDs under Origin. Sketch entry requires an eligible selected plane, allocates a collision-
safe name, enters orthographic normal-to-plane editing and publishes the adaptive grid. Body-result
face support is covered by `docs/gui-sketch-workbench-mvp7.md`; it is not a topology ordinal.

### Block 134 — Sketch in the new shell — Implemented

`ShellSketchTools` receives explicit window/session/workbench/workspace/viewport/ribbon references
and hosts `GuiSketchCreateController`, `GuiSketchDragController`, `GuiSketchConstraintController`,
`GuiSketchDimensionController` and `GuiSketchInteractionSceneBuilder`. Ribbon slots share the
`begin_tool`/`add_pick`/`complete_tool`/`cancel_tool`/`apply_constraint`/
`remove_selected_constraint`/`apply_dimension`/`edit_selected_dimension` command surface.

Read these focused contracts instead of reconstructing behavior from old fix logs:

| Area | Contract |
| --- | --- |
| Tools, exact conics, tangent creation and repeated placement | `docs/gui-sketch-basic-creation-mvp8.md` |
| Selection, snapping, grid, plane camera and overlay hit testing | `docs/gui-sketch-plane-interaction-mvp8.md` |
| Constraint signatures, solver/DOF and drag | `docs/gui-sketch-constraint-authoring-mvp8.md`, `docs/sketch-planar-constraint-solver-mvp8.md`, `docs/gui-sketch-solver-drag-mvp8.md` |
| Driving/reference dimensions, measured-value initialization and persistent annotation placement | `docs/gui-sketch-dimension-authoring-mvp8.md` |
| Modify, transformations, projections and active editing | `docs/gui-interactive-sketch-workspace-mvp8.md` |

Shell-specific regression hazards retained from the reset:

- Line chaining locates its shared corner at the anchor, including alongside profile entities;
  auto Coincident failures are diagnostic. Right-click consumes the complete finish gesture,
  Escape backs out the active tool, and free-point creation re-arms after placement.
- Point/rim/midpoint roles retain their typed meanings: Coincident maps to point coincidence,
  PointOnObject or Midpoint. Line/circle Tangent uses the dimension-driven radius. Multi-selection
  survives 3D presentation rebuilds; a handle click without movement does not republish the scene.
- Deletion batches selected constraints, dimensions and entities into one transaction, including
  orphan points and referring intent. Legacy closed-profile groups delete as a whole, because partial
  groups cannot round-trip. Do not restore the older reject-on-dependent-delete behavior.
- Solver-backed drag is the default; fully constrained geometry may correctly remain immobile.
  Live drag avoids expensive annotation re-resolution until release. Scene republishing clears
  stale hover candidates while camera changes reproject handles, highlights and annotations.
- All profile representations have persistent viewport IDs (`sketch/<sid>/profile/<pid>`), including
  circles and materialized bounded contours. Endpoints/centers remain visible; centerlines use
  dash-dot presentation. Badges/glyphs are rendered and hit-tested, not merely selectable metadata.

### Shell usability additions (post-134) — Implemented

A browser Sketch double-click re-enters the exact planar Sketch when no Sketch is already active.
The property inspector mirrors `GuiBrowserNode::properties`; supported edits use
`edit_browser_property(..., commit=true)`, then queue refresh so `itemChanged` does not delete its
own table item. Browser-only targets retain semantic identity across rebuilds.

Modify uses `GuiSketchModifyController` with normalized Core entity IDs. Trim/Extend/Split remain
armed for repeated picks; Fillet/Chamfer require compatible selected curves. Corner operations
prune stale Coincident constraints, add accepted contact/tangency and driving size, and preserve
the fixed size under drag. The later hardening below makes the whole corner operation atomic.

Free `SketchPoint` geometry is persistent, drawable, draggable and constrainable/dimensionable,
with two DOF per unconstrained point; it is not a part-level ConstructionPoint. Selection maps a
Point entity to its underlying topology point. Its JSON/topology/solver paths must round-trip.

## Dimension workflow hardening

The replacement shell now authors an automatically provisioned dimension parameter and its solved
Sketch/catalog in one history entry. Duplicate or rejected dimensions leave no orphan parameter.
Dimension badges persist their user-authored plane-space offsets in the dimension sidecar; dragging
is preview-only until release and Escape/capture loss restores the committed placement. Save/Open,
Undo/Redo and topology remapping retain the layout. Values are edited at the canvas badge with
Enter/Escape, optional matching units, and `=formula` conversion to an expression binding.

Proof: `[sketch-dimension-workflow]` in `tests/gui/gui_shell_dimension_workflow_tests.cpp`.

## Sketch tool hardening after the shell reset

Open contours may finish as editable sketches; closed profile extraction and downstream feature
validation remain explicit. Split and corner edits preserve ordered profile ids, with polygon-to-curve
promotion for fillets. Connected line/arc/spline corners and exact cubic continuation expand Modify.
Corner geometry, automatic relationships and driving size now share one transaction.

Ellipse creation and internal elliptical-arc operations use exact rational conic spans; the standalone
elliptical-arc tool has been removed. Weighted curves survive JSON,
topology/solver materialization, editing, sampling and OCCT feature adapters. Tangent tools require a
real source curve and persist contact/tangency together with the new shape. Conic and slot coverage
has been restored with `[core][sketch-conics]`, `[geometry][sketch-conics]` and
`[gui][sketch-create-conics]`; see the creation and modify contracts for remaining bounded cases.

The Sketch follow-up also supports nonmodal mouse/numeric transformations with separate axis and
entity collection, persistent helper-line roles filtered before region discovery, off-plane/finite
semantic projection and explicitly unlinked copies from other Sketches. Copy previews disclose
relationships not duplicated. Focused regression tags include `[sketch-mouse-operations]`,
`[spline-create-workflow]` and `[sketch-delivery-review]`.

### Block 135 — Interactive features minimal — Implemented

`ShellFeatureTools` (`shell/shell_feature_tools.*`) introduced selection-first Extrude/Revolve over
`GuiInteractiveFeatureCoordinator`, disposable previews and the notification-driven shared
manipulator overlay. It receives explicit session/workbench/viewport/ribbon references. Current
source eligibility, extent/axis choices, invalid-input recovery and shared workflow actions belong
to `docs/gui-interactive-extrude-revolve-mvp9.md`; later blocks extended the initial profile-only task.

Retained shell lifecycle contracts:

- Finish Sketch hands off an eligible source through the same selection path as browser/viewport
  picks. Persistent profile presentations permit reselection; explicit non-profile selection
  invalidates a stale Finish-Sketch fallback, while Cancel preserves semantic selection.
- Required result Bodies and driving parameters stay candidate-only until the feature's atomic
  `commit_part_transaction`. Cancel leaves no artifacts; one Undo removes the committed batch.
- The transparent viewport overlay uses fixed-DIP hit testing, mouse capture, the exact final
  release sample, Escape cancellation and camera notifications; it uses no polling/discovery.
- Numeric input uses locale-independent dot decimals and handle-specific `mm`/`deg` units.
  Typed input during a drag becomes its numeric override through release. Escape, invalid mapping
  or deactivation restores the drag-start value; a second Escape cancels the command.
- Incompatible/non-finite candidates cannot commit, generated IDs/names skip existing objects,
  view actions remap the overlay and Apply rejects while a drag still owns mouse capture.

Focused proof:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][shell-features]"
```

The proof covers Sketch-Finish handoff and reselection, stale-selection invalidation, candidate-only
auto-provisioning and name collisions, strict numeric units and drag override/rollback, incompatible
candidate rejection, recomputed Extrude/Revolve geometry, mutation-free Cancel, and atomic Undo.

### Block 136 — Feature full coverage — Implemented and accepted

All eighteen bounded families are restored. The manifest supplies exact current proofs, action and
widget IDs, bounded scopes and owners; `docs/gui-shell-acceptance-mvp9r.md` supplies final dated
acceptance and the closure gate. Do not treat historical intermediate gaps as work still to do.

| Shell entry point | Authority/contract to extend |
| --- | --- |
| `ShellFeatureTools` and `shell_{finishing,pattern,body,spatial,surface}_tools.cpp` | `docs/gui-interactive-finishing-mvp9.md`, `docs/gui-interactive-pattern-body-mvp9.md`, `docs/gui-interactive-path-sweep-loft-mvp9.md`, `docs/gui-interactive-surface-mvp9.md` |
| `ShellConstructionTools`, `ShellReferenceRepairTools`, `ShellSketchDiagnosticsTools` | `docs/gui-sketch-workbench-mvp7.md`, `docs/reference-recovery-mvp.md` |
| `ShellSketch3DTools` | `docs/gui-sketch3d-interaction-mvp8.md` |
| `ShellParameterTools`, legacy `ShellHoleTools` | `docs/parameter-model.md`, `docs/gui-part-foundation-workbench-mvp7.md`; semantic Hole/Thread plans stay in `docs/hole-wizard.md` |
| `ShellNavigationTools`, `ShellContextTools`, `ShellRepeatTools` | `docs/gui-modeling-workspace-mvp9.md` |
| `ShellFeatureEditTools` | `docs/gui-feature-edit-mvp9.md`: exact feature identity, complete downstream disposable preview, atomic updates |
| `ShellMeasureTools` | `docs/gui-measure-mvp9.md`: generation-scoped picks, revision invalidation, no model transaction, dirty state or history |
| `ShellAssemblyTools` | `docs/gui-interactive-assembly-mvp9.md`: occurrence-qualified references and shared definition authority |
| `ShellInspectTools` | `docs/gui-analysis-step-export-workbench-mvp7.md`: read-only analysis, atomic staged STEP publication |

Across command families, new parameters and result Bodies stay candidate-only until one atomic
Apply. Preview uses the Geometry authority; invalid candidates never publish partial model state.
Task pick roles own their temporary filters/scopes and restore the previous context on completion.
Context cancellation restores still-valid consumed selection. Reference IDs must come from the
owning typed catalogue; temporary viewer IDs and topology ordinals are not persistent references.
Task forms scroll while Apply/Cancel remains reachable, including at 1280×800.

## Test accounting

The reset removed obsolete shell-bound tests and retained headless authorities. Current test/source
mappings belong to `families[].shell.proofs` in the v3 coverage manifest; historical `interactive`
records are not proof of a working ShellWindow. The registered-tag and entry-point inventory runs
under `[integration][gui-feature-coverage]` / `[gui][shell-coverage]` in
`tests/gui/gui_shell_coverage_tests.cpp`. Negative controls reject missing/duplicate families,
non-shell sources, deleted tags hidden behind OR, and completion claims with unresolved gaps.

A green inventory alone is insufficient. `BLCAD_REQUIRE_SHELL_ACCEPTANCE=1` requires complete
bounded coverage, and whole-block acceptance additionally requires full Core/Geometry/GUI suites,
actual native Qt/OCCT workflows, inspected captures and separate startup. Reproduction guidance:
`docs/development-setup.md`; final Block-136 counts and canonical filter:
`docs/gui-shell-acceptance-mvp9r.md`.

## Retained deferrals

The inference guide overlay (visual dashed guide line) is still
deferred; snap inference itself works and is reported in the status bar.

Color-coded gap overlays remain deferred;
this slice displays diagnostic text and valid result geometry.

These are retained bounds, not new implementation blocks. Future planning remains in the existing
feature contracts and `docs/inventor-parity-sequence.md`; this historical compaction changes none
of that planning.

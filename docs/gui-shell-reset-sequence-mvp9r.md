# GUI Shell Reset Sequence MVP-9R

Status: Blocks 132–136 implemented and accepted on 2026-10-25. Current completion evidence lives in
`docs/gui-shell-acceptance-mvp9r.md`; `docs/mvp-plan.md` owns numbered status. STEP Import MVP-10
follows in planned Blocks 137–143. Dated entries below preserve each slice's historical state.

This document is the canonical numbered implementation sequence for replacing the Qt shell layer.
It governs what is deleted, what is kept byte-for-byte, and the contracts of the replacement shell.
Feature semantics are not redefined here; the canonical feature documents of MVP-7..9 remain
authoritative for every controller the new shell hosts.

## Why a reset

The shell accumulated through MVP-7..9 instead of being designed:

- `MainWindow` (1115 lines) plus seven binders that install themselves via `QTimer::singleShot`,
  discover widgets via `findChild` object-name strings, rewire foreign QActions via `disconnect`,
  and poll a 33 ms timer;
- two competing workspace tab bars (`blcad.workspace_tabs` vs the Block-122 modeling tabs);
- viewport rendering into a foreign native window (`WA_NativeWindow` + `Xw_Window`), which produces
  an unfixable class of whole-window flicker on selection and repaint;
- action/menu surfaces that were never reachable or discoverable in practice.

The decision (2026-07-22): hard replacement. The old shell is deleted, not migrated.

## Authority declarations

Existing authority (kept unchanged, consumed as-is):

- Core and Geometry libraries, all headless authorities through Block 131.
- `GuiDocumentSession` (document lifecycle, transactions, command context, presentation revision),
  `GuiCommandRegistry`, `GuiSelectionModel`, `GuiDocumentBrowser`, `GuiTaskState`.
- All Qt-free workbenches and interactive controllers of Blocks 106–131, including
  `GuiModelingWorkspace` (Block 122), `GuiViewportManipulatorLayer` (Block 123), the interactive
  feature controllers (Blocks 124–128), `GuiFeatureEditController` (Block 129),
  `GuiInteractiveAssemblyCoordinator` (Block 130), `GuiMeasureController` (Block 131), and the
  Qt-free `GuiInteractiveFeatureCoordinator`.
- The revision-gated presentation refresh design (`presentation_revision` comparison before browser
  and viewport rebuilds) is kept as a pattern.

New intent allowed:

- A new shell window class and ribbon architecture (Inventor-inspired) with direct ownership
  wiring; deletion of `MainWindow` and every binder; a new `QOpenGLWidget`-based viewport
  implementation behind the existing `OcctViewport` public API; new shell-owned sketch and feature
  interaction components replacing the binder implementations; new shell tests replacing the
  deleted shell-bound tests.

Scope narrowing: during Blocks 133–135 the application intentionally exposes fewer commands than
the old shell pretended to; full feature surface parity is restored in Block 136 and proven by the
coverage manifest.

## Shell architecture contract

One shell window (`blcad::gui::ShellWindow`) owns every component and passes explicit references.

Forbidden in the new shell layer:

- `findChild`-based discovery of widgets or actions across components;
- self-installing components (`QTimer::singleShot` install patterns);
- rewiring foreign QActions (`QObject::disconnect` of another component's connections);
- polling timers for state synchronization (state changes notify explicitly);
- more than one workspace/ribbon tab authority.

Layout (Inventor-inspired):

```text
+------------------------------------------------------------------+
| Ribbon: [Skizze] [3D-Modell] [Oberfläche] [Baugruppe] [Prüfen]   |
|   grouped panels of labeled tool buttons per tab                 |
+----------------+-------------------------------------------------+
| Model browser  |                                                 |
| (tree)         |            3D viewport (QOpenGLWidget)          |
|                |                                                 |
+----------------+-------------------------------------------------+
| Diagnostics (collapsible)                          | Status bar  |
+------------------------------------------------------------------+
```

- The ribbon is the single command surface. Tabs activate contextually: entering sketch mode
  activates the sketch tab; leaving restores the previous tab.
- Properties/task editing appears in a right-side panel shown on demand (feature dialogs), not a
  permanently competing dock.
- The model browser is the single tree authority (built from `GuiDocumentBrowser`).

## Block sequence

### Block 132 — QOpenGLWidget viewport foundation — Implemented

`OcctViewport` stays a plain `QWidget` container (public API, overlays, headless fallback
unchanged) and hosts a mouse-transparent `OcctViewportRenderSurface : QOpenGLWidget` child. OCCT
renders into Qt's GL context using the official OCCT pattern: `OpenGl_Context` wrapping the
current context, `Aspect_NeutralWindow`, default-FBO wrapping (`InitWrapper`) in `paintGL`, driver
options `buffersNoSwap`/`buffersOpaqueAlpha`/`useSystemBuffer=false`, `SetImmediateUpdate(false)`.
`paint_gl()` is the only place OCCT redraws; every state change funnels through
`render_update()` (a scheduled Qt repaint). Input handlers pass device-pixel coordinates
(`to_render_point`, DPR-scaled). Fail-closed behavior is unchanged: non-xcb platforms (offscreen
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

Verified: core 483, geometry 880, gui 132 offscreen tests green; viewport, sketch-workbench, and
feature-coverage acceptance tests additionally green under a live xcb session with real GL.

### Block 133 — Shell reset — Implemented

Deleted: `main_window.hpp/.cpp`, all seven binders (`gui_sketch_interaction_binder`,
`gui_sketch_create_binder`, `gui_sketch_constraint_binder`, `gui_sketch_drag_binder`,
`gui_sketch_dimension_binder.hpp`, `gui_modeling_workspace_binder.hpp`,
`gui_viewport_manipulator_binder.hpp`) and the fully shell-bound test files
(`gui_application_shell_tests`, `gui_feature_coverage_acceptance_tests`,
`gui_interactive_features_tests`, `gui_measure_tests`, `gui_sketch_create_tests`,
`gui_sketch_drag_tests`, `gui_sketch_dimension_tests.inc`). Three mixed test files kept their
headless cases; only their single MainWindow-bound TEST_CASE each was removed
(`gui_document_browser_tests`, `gui_sketch_workbench_tests`, `gui_sketch_interaction_tests`).

Added `include/blcad/gui/shell/` + `src/gui/shell/`:

- `ShellWindow` — owns session, sketch workbench/workspace, browser model, viewport, ribbon,
  browser dock, diagnostics dock, status bar; document lifecycle (new/open/save with dirty
  confirmation) with a dialog-free command surface (`new_part`/`open_path`/`save_to`/
  `create_sketch_on_selection`/`finish_active_sketch`) that doubles as the testing surface;
  selection flow viewport → session selection → browser sync and back; revision-gated browser and
  viewport-scene refresh; ribbon tabs `3D-Modell` / `Skizze` (enabled only in sketch mode) /
  `Ansicht` (fit, standard views, projection, display modes).
- `ShellRibbon` — Inventor-style tab bar + grouped labeled tool buttons; explicit references only.
- `shell_origin` — `seed_origin_geometry(session)`: one transaction adding datum.xy/xz/yz and
  datum.axis.x/y/z; fails closed on duplicates.

Origin geometry: `DatumPlane` gained `xz()`/`yz()` principal factories (declared Core extension;
JSON kinds `xz`/`yz` round-trip, unknown kinds still rejected). `ViewportSceneBuilder` already
emitted datum-axis items generically — no geometry change was needed. `GuiDocumentBrowser` groups
the six origin ids under an "Ursprung" folder ahead of "Datums / Workplanes"; documents without
origin ids see no change. Sketch entry ("2D-Skizze erstellen") requires a selected origin/datum
plane, auto-names `SkizzeN`, enters orthographic normal-to-plane editing, and shows the white
adaptive decade grid via a minimal interaction scene; drawing tools follow in Block 134.

Verified: core 485, geometry 882, gui 85 offscreen tests green; shell, viewport, and sketch-snap
suites additionally green under live xcb; app smoke-run with screenshot.

### Block 134 — Sketch in the new shell — Implemented

The five deleted binder implementations are replaced by ONE shell-owned component,
`ShellSketchTools` (`shell/shell_sketch_tools.*`), constructed by ShellWindow with explicit
references (window, session, workbench, workspace, viewport, ribbon) and reusing the headless
controllers unchanged (`GuiSketchCreateController`, `GuiSketchDragController`,
`GuiSketchConstraintController`, `GuiSketchDimensionController`,
`GuiSketchInteractionSceneBuilder`). Every ribbon slot funnels through a dialog-free command
surface (`begin_tool`/`add_pick`/`complete_tool`/`cancel_tool`/`apply_constraint`/
`remove_selected_constraint`/`apply_dimension`/`edit_selected_dimension`) that doubles as the test
surface.

Skizze ribbon tab groups:

- Zeichnen — all 21 MVP-8 create tools as Inventor-style family menu buttons (Linie/Polylinie/
  Mittellinie, Rechteck ×5 incl. Polygon, Kreis ×6 incl. Ellipse, Bogen ×4, Langloch ×2, Punkt),
  with rubber-band preview, numeric buffer input, Escape pick-undo, polyline Enter/double-click.
- Abhängigkeiten — all 13 constraint families as compact buttons plus Löschen;
  compatibility-driven enablement from the current selection; commit via the headless preview →
  accept → commit lifecycle. End-to-end acceptance verified by test: draw off-horizontal line →
  select → Horizontal → catalog +1 and the solver actually levels the line; incompatible requests
  fail closed; glyph-selected removal works.
- Bemaßung — family menu (9 kinds), Referenz toggle, parameter binding for driving dimensions,
  Bearbeiten (literal/formula edit).
- Raster — Raster/Fang toggles over the adaptive white decade grid.
- Beenden — Skizze fertig stellen (from Block 133).

Solver-backed drag is the default tool (handles from `GuiSketchDragController`, live preview with
DOF/solve feedback, Escape/UngrabMouse cancel). Cursor/snap/DOF/solver/numeric-input status is
shown in the status bar (the dedicated numeric-HUD line edit of the old shell was intentionally
folded into the status bar; contract surface unchanged). The interaction scene (topology, glyphs,
constraint and dimension annotations) is rebuilt after every commit.

Scope narrowing (documented): the Ändern group was initially deferred; it is now wired (see "Shell
usability additions" below). The inference guide overlay (visual dashed guide line) is still
deferred; snap inference itself works and is reported in the status bar.

Verified: core 485, geometry 882, gui 89 offscreen tests green (4 new Block-134 suites: create,
constraints end-to-end, dimensions incl. driving edit, ribbon lifecycle); shell-sketch, shell, and
viewport suites additionally green under live xcb; app smoke-run.

Usability fixes after first user test (2026-07-23):

- The Line tool chains Inventor-style: each commit re-arms the tool anchored at the committed end
  and couples the shared corner with an auto Coincident constraint (skipped silently when
  redundant); Escape ends the chain. The separate Polyline ribbon entry was removed (the headless
  tool remains).
- Driving dimensions auto-create their parameter (`d1`, `d2`, …) carrying the CURRENT measured
  value via `SketchDimensionMeasurementEvaluator`, so commit never moves geometry and no dialog or
  pre-existing parameter is required.
- Screen-space sketch overlays (drag handles, preview, selection highlight) re-project on every
  camera change — `rebuild_sketch_grid` is the single re-projection funnel.
- Selected sketch entities highlight in clear Inventor blue via the overlay; point handles are
  small filled dots in the sketch line color; the snap marker shows only for meaningful geometric
  snaps (not the ever-active grid snap); the sketch cursor stays the normal arrow.
- Model browser dock: minimum 220 px, initial 300 px, user-resizable.

Second fix round (2026-07-23, user retest):

- Circles were committed as `CircleProfile` but `ViewportSceneBuilder` never rendered sketch
  profiles — the interactive circle tool produced invisible geometry. The builder now emits
  rectangle- and circle-profile items (`sketch/<sid>/profile/<pid>`, matching the interaction
  scene's hit ids); regression-tested.
- "Bemaßungen funktionieren nicht" root cause: dimension values and constraint glyphs were
  hit-testable but never DRAWN. `GuiSketchAnnotationPrimitive` gained a `label` (filled from the
  glyph resolvers' existing `value_text`/`token`), and the sketch overlay renders dimension
  badges and constraint tokens, re-projected on every camera change. The commit path itself was
  verified working end-to-end through the real viewport click path (offscreen AND live xcb).
- Right-click now finishes the active tool/chain (Inventor behavior).

Third fix round (2026-07-23, user retest of interaction correctness):

- Constraints now resolve selected point roles (`selected_targets(..., true)`), so clicking two
  endpoints and applying Coincident targets the points — previously they resolved to entities and
  Coincident silently did nothing. (The endpoint/midpoint role was already carried through the
  hit-role marker in `semantic_id`; the bug was only in constraint target resolution.)
- Unified delete (`delete_selection`, bound to Entf): removes the selected constraint glyph,
  dimension, or sketch entity by priority. Entity deletion removes the entity AND its orphaned
  points in one chained topology edit (`delete_sketch_entity`) so the legacy Sketch JSON stays
  representable; it fails closed with a clear message when a dependent dimension/constraint still
  references the geometry.
- Dimension delete via `GuiSketchDimensionController::remove_accepted` (new headless helper).
- Undo/Redo: ShellWindow gains ribbon buttons (3D-Modell → Bearbeiten) and Ctrl+Z / Ctrl+Y
  shortcuts over `GuiDocumentSession::undo/redo`; after either, the active sketch tool surface is
  re-activated (scene + drag handles resynced).
- Double-click a dimension glyph to edit its value/formula (in addition to the ribbon button).
- Drag survives constraints (regression test: parallel keeps handles armed).
- Dimension leaders: `SketchDimensionGlyph` gained `leader` extremities (distance/length span the
  measured points, radius/diameter span the circle, angle draws none); the overlay draws the
  dimension line with arrowheads plus a dotted extension to the value badge, re-projected on every
  camera change.
- Verified: core 485, geometry 882, gui 94 offscreen; shell-sketch + viewport suites green under
  live xcb (incl. new coincident-on-points, delete+undo+redo, and drag-after-parallel tests).

Fourth fix round (2026-07-23, user retest):

- Constraint compatibility (MVP-8 contract extension, `compatible_kinds`): mixed-kind two-target
  rules are order-insensitive (point+line == line+point for Midpoint/PointOnObject; Symmetric
  accepts any permutation of two points + one line); intent creation normalizes points before
  entities. Koinzident on a point + curve maps to PointOnObject (Inventor semantics) and the
  Koinzident button lights up for it.
- Tangent (Core solver contract extension): the tangent residual no longer requires one SHARED
  topology point — curves whose endpoints touch positionally (the normal case for interactively
  drawn geometry) pair their closest coincident corner, chosen statically from the source topology
  so the pairing is iteration-stable. Verified: core 485 still green, line+arc tangent end-to-end.
- Drag after constraints verified working at controller AND full mouse-event level (the earlier
  report was a misread: the solver satisfies e.g. Horizontal by moving both endpoints; fully
  constrained geometry legitimately stops moving — DOF/solver status shows in the status bar).
- Sketch endpoints and centers always render as dots (independent of the drag layer); centerlines
  (construction lines) render dash-dot; Backspace deletes like Entf; dimension value badges are
  draggable (transient per-dimension label offset, hit test follows).
- gui suite now 98 offscreen tests.

Fifth fix round (2026-07-23, user retest with diagnostics screenshot):

- Root cause of "Koinzident/Kette kaputt sobald ein Kreis in der Skizze ist": the chained-line
  coincidence coupled `entities.back()` with its predecessor — with a circle/rectangle profile in
  the topology those indexes point at the profile, and the coupling silently skipped. The corner
  pair is now found positionally at the chain anchor, and coupling failures report to the
  diagnostics instead of being swallowed. Hover-cached candidate roles are cleared on every scene
  republish (stale ids after topology round-trips caused "unknown Sketch point").
- Deleting a constrained/dimensioned entity now cascades: one sketch-intent transaction drops
  every constraint/dimension intent referencing the entity or its points, then removes the entity
  plus orphaned points.
- Tangent line↔circle (Core solver extension): `SketchSolverConstraint` may carry an optional
  value for Tangent — the circle's dimension-driven radius, baked in by both intent→solver
  conversion paths (`tangent_circle_radius_for_intent`). The residual holds squared
  center-to-line distance equal to the squared radius (smooth at contact); the circle keeps its
  parameter radius. GUI compatibility offers Tangent for line+circle without any shared corner.
- Drag responsiveness on dense sketches: the live drag preview republishes the scene WITHOUT
  re-resolving constraint/dimension annotations (measure + glyph layout per pointer move was the
  lag); the release publishes fully.
- gui suite now 101 offscreen tests (circle-profile constraint scenario, line↔circle tangent with
  distance==radius assertion, cascade delete).

Sixth fix round (2026-07-23, user retest):

- Root cause of "zwei Endpunkte auswählen geht nicht": `OcctViewport::set_scene` cleared the
  ENTIRE selection including the sketch-mode selection — and a 3D scene refresh runs between
  multi-select clicks whenever the presentation revision moves (e.g. the click's drag-handle
  lifecycle). `set_scene` now invalidates only the 3D pick state; the sketch selection survives.
  Verified end-to-end: click + Ctrl-click on two endpoints → Koinzident joins them (real mouse
  events under xcb). A handle click without movement also no longer republishes the scene.
- Koinzident with circles: the clicked circle CENTER resolves to the profile's center topology
  point (point↔point coincident works against it); Koinzident point ↔ circle RIM maps to
  PointOnObject, which now supports CircleProfile in the solver — the dimension-driven radius is
  baked as the constraint value (shared helper with tangent), residual |p−center| = r.
- The Punkt-auf-Objekt ribbon button was removed: Koinzident covers point↔point, point↔curve, and
  point↔circle (Inventor semantics); the solver kind remains internally.
- gui suite now 103 offscreen tests.

Seventh fix round (2026-07-23, user retest):

- Tangent after Koinzident (the real user flow) works now: the GUI "connected" tolerance was
  1e-6 mm while a coincident SOLVE leaves endpoints equal only to convergence tolerance — it now
  matches the solver's corner pairing (0.1 mm). Tangent between two straight lines is no longer
  offered (degenerate — that is Parallel); the Block-114 matrix test documents the change.
- Koinzident with a clicked LINE MIDPOINT maps to the Midpoint solver kind (the midpoint is not a
  topology point); the separate Mittelpunkt button was removed. Koinzident now covers: point↔point,
  point↔curve/circle rim (PointOnObject), point↔line-middle (Midpoint).
- Delete handles MULTI-selection: all selected constraints, dimensions, and entities go in ONE
  transaction (single undo step), including every intent referencing a doomed entity.
- Rectangle deletion: closed-profile groups delete as a WHOLE (container + members) — the legacy
  Sketch JSON expresses the shared corner points only through the profile, so a partial group
  cannot round-trip (documented legacy-persistence limitation). `SketchTopologyLegacyMaterializer`
  now supports INTENTIONAL profile removal (profiles absent from the edited topology are dropped
  instead of erroring); batch entity deletion materializes once
  (`delete_sketch_entities`).
- Constraint tokens and dimension badges render larger with a backdrop and use a 12 dip hit
  tolerance.
- gui suite now 106 offscreen tests.

### Shell usability additions (post-134) — Implemented

Incremental shell polish requested during use, independent of the numbered feature blocks:

- **Sketch re-entry.** Double-clicking a planar Sketch node in the "Modell" tree re-enters it for
  editing (`ShellWindow::handle_tree_activation` → the existing `enter_sketch`). A planar Sketch
  node carries the `SketchId` as its semantic id; child records and other node kinds are ignored,
  and re-entry is refused while a Sketch is already active.
- **Property inspector.** A second left dock ("Eigenschaften", a two-column `QTableWidget` split
  below the tree) mirrors the selected browser node's `GuiBrowserNode::properties`. Editable
  properties (currently parameter `value`/`formula`, per `edit_browser_property`) are editable
  in place; a commit runs `edit_browser_property(..., commit=true)` and a queued `refresh()`
  repaints from the model (reverting a rejected edit) — queued so the edited `QTableWidgetItem` is
  not deleted inside its own `itemChanged` emission. Nodes without a viewport id (parameters) are
  restored across a rebuild by semantic id so the inspector keeps its target after an edit.
- **Ändern group (Block 116 wiring).** The `Ändern` ribbon group exposes the headless
  `GuiSketchModifyController`: **Trimmen / Verlängern / Teilen** are modal pick tools (arm the tool,
  then click a curve — the press hit gives the target entity and the pick point; the tool stays
  armed for repeated ops, Escape or switching tools ends it), and **Verrundung / Fase** act on
  exactly two selected curves with a numeric radius/distance prompt. Command surface
  (`apply_modify_pick`, `apply_fillet`, `apply_chamfer`, `begin_modify_tool`) doubles as the test
  surface; each op previews once and commits one atomic transaction (constraint/dimension catalogs
  re-validated). Fillet/Chamfer enablement gates on a two-curve selection. Selections resolve to
  raw core entity ids via `selection_target` (strip the `entity/` topology prefix), so both the
  `sketch/<id>/entity/<id>` viewport form and the bare topology form work.
- **Auto-constraints on Fillet/Chamfer.** The core modify service is geometry-only, so an inserted
  fillet arc / chamfer line would drift apart on drag. After the geometry commit,
  `auto_constrain_corner` finds the inserted curve (the new-id Arc/Line), matches each of its
  endpoints positionally to the touched trimmed-line endpoint, and authors catalog constraints:
  **Coincident** at both corner touch points and, for fillets, **Tangent** between the arc and each
  line. Constraints go into the Block-114 constraint catalog (embedded geometric constraints have no
  general point-point coincidence), which the drag solver consumes via
  `SketchConstraintCatalogSystemBuilder` (`from_legacy` + catalog) — so the corner survives dragging.
  Each is authored through `GuiSketchConstraintController` (preview → accept → commit) and skipped
  silently if the solver rejects it (redundant/conflicting), never failing the modify. A right-angle
  fillet yields 2 Coincident + 2 Tangent; a chamfer 2 Coincident.
- **Stale-corner pruning (drag-collapse fix).** Chained corners carry an auto Coincident between the
  two corner points. A fillet/chamfer *separates* those points onto the two tangent/cut points, but
  the old coincidence lingered in the catalog and pulled them back together on the next solve/drag —
  collapsing the corner (the reported "fillet breaks when you move something", and it also broke
  chamfer). Before authoring the new corner constraints, `prune_broken_coincidences` removes any
  catalog Coincident whose two points are now apart (> 1e-4) or whose points vanished. Pruning does
  not re-solve, so the just-committed clean geometry is preserved; the drag then keeps the corner
  intact (proven by a real `GuiSketchDragController` drag in the tests: the arc endpoints stay glued
  to the lines).
- **Fixed corner size.** Coincident + Tangent keep the corner attached but leave the radius / chamfer
  length as a free DOF (it floated when a line was dragged). `auto_constrain_corner` therefore also
  authors a driving **Radius** dimension on the fillet arc (**Length** on the chamfer line) via
  `apply_dimension`, which bakes the CURRENT measured value into an auto `dN` parameter so the commit
  never moves geometry. The size then stays constant under drag (tests drag a line and assert the arc
  radius / chamfer length are unchanged). Skipped silently if the dimension is not accepted.
- gui suite now 119 offscreen test cases (adds sketch-reentry, property-editor, and eight sketch-modify
  shell tests incl. the auto-constraint, drag-robustness, and fixed-size proofs).

### Sketch usability fixes (post-modify)

- **Generalized Collinear.** The `Kollinear` constraint is now variable-arity — two-plus lines, a
  point and a line, or three-plus points — instead of line-line only (see
  `docs/sketch-planar-constraint-solver-mvp8.md`). `compatible_kinds` offers it for the new
  selections; the authoring/solver validate the arity by signature.
- **Free sketch points (`SketchPoint`).** A new first-class core entity: a free-standing 2D sketch
  point that is ordinary editable geometry — rendered, selectable, **draggable**, and
  **constrainable/dimensionable** like a line endpoint. It spans core (`SketchPoint` + `Sketch`
  storage/`add_entity`/`find_point`), topology (new `SketchTopologyEntityKind::Point` migrating to a
  single **non-reference** free point), JSON (`sketch_points`), the interaction scene (`add_point`
  over `sketch.points()`), the drag handle builder (Point → endpoint handle), and selection
  (`selected_targets` maps a selected Point entity to its underlying topology point so constraints
  and dimensions address the point directly). The **Point tool** now creates a `SketchPoint` (and
  re-arms after each placement, Inventor-style) instead of a bare part-level `ConstructionPoint`,
  which was invisible and inert inside the sketch. Proven by tests: place two points → two free
  points; drag one → it follows the pointer; coincident with a line → it lands on the line; and a
  JSON round-trip preserves it. DOF accounting confirms each point contributes two free DOF.

### Block 135 — Interactive features minimal — Implemented

Extrude and Revolve reachable from the ribbon: selection-first flow through
`GuiInteractiveFeatureCoordinator`, manipulator overlay (shell-owned replacement of the Block-123
overlay binder, notification-driven instead of polled), numeric coupling, preview/apply/cancel.
First fully usable end-to-end build: sketch → profile → extrude/revolve.

Implemented by `ShellFeatureTools` (`shell/shell_feature_tools.*`), constructed by `ShellWindow` with
explicit session/workbench/viewport/ribbon references:

- the 3D-Modell ribbon exposes Extrusion and Drehung only for a verified materialized profile;
  finishing a Sketch with exactly one profile hands it directly to this selection-first surface, a
  single freehand bounded line region is materialized during Finish Sketch, and Sketch/profile/
  contour selections from browser or viewport use the same path;
- the right-side Feature task panel owns numeric distance/angle editing plus Apply/Cancel; every edit
  revalidates through the existing controller's disposable preview;
- one transparent viewport child paints linear/angular handles, performs fixed-DIP hit testing,
  mouse capture, exact release, Escape cancellation, and camera remapping in response to events — no
  polling timer or widget discovery;
- a fresh Part's required result Body and Extrude Length parameter remain candidate-only and are
  provisioned in the same `commit_part_transaction` as the feature. Cancel leaves no artifacts; one
  Undo removes feature, Body, and parameter together;
- Revolve chooses an in-plane explicit origin axis by default (callers/tests may provide one) and
  starts with a 180-degree partial extent so the angle wheel and numeric field are immediately usable.

The Block-135 hardening audit additionally freezes these shell-lifecycle details:

- topology-authored `ClosedProfile`, `ArcClosedProfile`, `CompositeClosedProfile`, and circular-hole
  pattern records receive persistent `sketch/<sid>/profile/<pid>` viewport presentations, so a
  finished profile can be reselected after another object was picked;
- an explicit non-profile selection invalidates the Finish-Sketch fallback instead of silently
  reusing a stale profile, while Cancel preserves the exact semantic selection;
- the numeric field uses the same locale-independent dot-decimal and handle-specific `mm`/`deg`
  rules as the manipulator layer. Invalid text or an incompatible unit leaves the last valid
  candidate unchanged;
- typed input during a pointer drag becomes the layer's numeric override and remains authoritative
  through release. `Esc`, invalid mapping, or window deactivation restores the value captured at
  drag start; a second `Esc` cancels the command;
- incompatible/non-finite candidates fail closed, initial preview failure tears the task down, and
  generated ids/names skip existing user objects;
- Fit, standard-view, and projection actions immediately remap the overlay; Apply is rejected while
  a pointer drag still owns mouse capture.

Focused proof:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][shell-features]"
```

The proof covers Sketch-Finish handoff and reselection, stale-selection invalidation, candidate-only
auto-provisioning and name collisions, strict numeric units and drag override/rollback, incompatible
candidate rejection, recomputed Extrude/Revolve geometry, mutation-free Cancel, and atomic Undo.

### Block 136 — Feature full coverage — Implemented and accepted

Restored feature commands in the ribbon (Fillet, Chamfer, Shell, Draft, Pattern, Mirror, Body
Boolean, Body Transform, Sweep, Loft, Surface family, Measure, Assembly tab), feature edit via
browser double-click (`GuiFeatureEditController`), and acceptance coverage replacing the deleted
shell acceptance tests (coverage manifest updated).

Final acceptance also covers construction and reference repair, Sketch3D, parameter/extent editors,
navigation/filter/repeat/context controls, advanced Assembly and analysis. All eighteen families in
the current v3 shell inventory are restored, without gaps or pending workflows. Full Core/Geometry,
offscreen and native GUI regression, startup, capture review and the explicit closure gate pass.
The final evidence is in `docs/gui-shell-acceptance-mvp9r.md`; the subsections below record the
incremental implementation history.

#### Block 136 first slice — finishing commands

`ShellFeatureTools` restores Verrunden, Fase, Aushöhlen and Formschräge in the Bearbeiten ribbon
section with explicit ownership, current-body semantic picking, real disposable Geometry previews,
shared manipulators and numeric refinement. Driving parameters are provisioned only on Apply in the
same history entry as the feature. Four-line Sketch rectangles share the existing stable box-role
contract. Focused shell and native proof: `[block136-finishing]`, with the detailed contract in
`docs/gui-interactive-finishing-mvp9.md`.

The remaining commands listed above, browser feature editing and whole-block acceptance are still
open. This slice does not advance the numbered completed-block boundary.

Validation for this slice: 513 Core cases, 421 Geometry cases and 231 offscreen GUI cases pass.
Two native-only GUI cases skip offscreen; both pass in the selected native run (16 cases, 807
assertions), alongside existing viewport and Sketch UI regressions. Native preview captures and
application startup are checked separately. Commands: `docs/development-setup.md`.


#### Block 136 next slice — Body patterns and Mirror

`ShellFeatureTools` adds Lineares Muster, Kreismuster and Körper spiegeln in the Muster group with
multiple Body source picks, datum/construction reference selection, actual recomputed ghost previews,
shared scalar/count manipulators and numeric refinement. Result Body and driving parameters remain
candidate-only until one atomic Apply. Source Bodies remain unchanged. The focused shell and native
proof is `[block136-patterns]`; see `docs/gui-interactive-pattern-body-mvp9.md`.

The finishing and pattern slices are implemented. The Body-operation follow-up below restores
Boolean/Transform; Sweep/Loft, Surface, Measure, Assembly, browser feature editing and whole-block
acceptance remain open.

Validation for the pattern slice on 21 October 2026: full Core (513 cases, 15124 assertions),
Geometry (421 cases, 28002 assertions), and offscreen GUI (238 passed, three explicitly skipped
native cases, 8882 assertions) pass. The combined native pattern/finishing/Sketch UI/viewport
regression passes all 24 cases (1245 assertions), including the three offscreen skips. Actual mouse
picks toggle source Bodies through a live preview; spacing, angle and count handles retain numeric
refinement through release. Closed fitted-spline extrusions are also valid pattern sources. Preview
visibility updates preserve nonselectability, and invalid previews expose no active drag grips.
Native captures were inspected and the desktop startup smoke passed (intentional six-second timeout).
These checks validate this slice without closing whole-block coverage.

#### Block 136 next slice — Body Boolean and Transform

`ShellFeatureTools` adds Kombinieren, Verschieben, Drehen and Skalieren through the shared task
panel and coordinator. Boolean separates target and tool picks and supports Add/Subtract/Intersect,
ModifyTarget/NewBody and keep/consume tools. New result Bodies remain candidate-only until Apply.
Transforms support multiple Body targets, XYZ translation, datum/construction rotation references,
and unitless uniform scaling about an explicit center. Independent Sketch/construction ownership
flags remain explicit. Actual multi-Body previews hide all replaced/consumed sources; Cancel and
failed candidates restore the prior scene. One Apply commits the ordered per-Body transform batch
and one Undo reverses it. Contract/proof: `docs/gui-interactive-pattern-body-mvp9.md`, `[block136-body]`.

The spatial and Surface follow-ups below restore those families. Measure, Assembly, browser feature
editing and whole-block acceptance remain open.

Validation for the Body-operation slice on 21 October 2026: full Core (513 cases, 15124 assertions),
Geometry (421 cases, 28002 assertions), and offscreen GUI (246 passed, five explicit native skips,
9611 assertions) pass. The combined native Body/pattern/finishing/Sketch UI/viewport/lifecycle run
passes all 34 cases and 2140 assertions, including the offscreen skips. It exercises model target/tool
picking for a cut, two-Body transformation, numeric refinement and Escape rollback. Focused proof
also checks repeated translation drags, unitless scale input, owned Sketch frames, pending-Body
collisions and rollback when the second batch target has no producer. Native captures were inspected
and desktop startup passed (intentional six-second timeout). Commands: `docs/development-setup.md`.
These results validate the slice without advancing whole-block acceptance.

#### Block 136 next slice — PathCurve, Sweep and Loft

`ShellFeatureTools` adds Pfad erstellen, Sweep and Loft through the shared task panel and coordinator.
Ordered path segments and Loft sections support list dragging, task buttons and explicit direction/
normal reversal. Sweep exposes profile/trajectory/guide roles and coupled twist drag/numeric input;
Loft retains center paths, ordered guides and continuity. Solid, cut and surface output use correctly
typed pending Bodies or an existing cut target. Real Geometry previews remain disposable, and one
Apply commits all new inputs with the feature. Cancel, Undo/Redo and Save/Open retain their authorities.

The viewport scopes native detection to the selected input role and restores the prior capability and
semantic filters on completion. Bounded paths have a complete canonical presentation, including
construction paths. Existing planar rational spline intent now resolves through 32 deterministic
spans for preview and execution. The task inputs scroll while Apply/Cancel remains visible.
Contract/proof: `docs/gui-interactive-path-sweep-loft-mvp9.md`, `[block136-spatial]`.

The Surface follow-up below restores that family. Measure, Assembly, browser feature editing and
whole-block acceptance remain open.

Validation for the spatial slice on 21 October 2026: full Core (513 cases, 15124 assertions),
Geometry (423 cases, 28106 assertions), and offscreen GUI (254 passed, six explicit native skips,
11134 assertions) pass. The combined native spatial/Body/pattern/finishing/Sketch UI/viewport/lifecycle
run passes all 43 cases and 3870 assertions, including the offscreen skips. It verifies actual
profile/path selection changes, ordered Loft picks, twist drag/numeric refinement and a visible,
reachable Apply button and task inputs without horizontal clipping. Focused proof also covers solid/cut/surface results, reversed connected paths,
list-model section moves, G1 guides, rejected G2 and twist/guide conflicts, fitted spline profiles,
pending-input isolation, selection-scope restoration, exact Undo/Redo and Save/Open. Native captures
were inspected and desktop startup passed (intentional six-second timeout). Commands:
`docs/development-setup.md`. These checks do not close whole-block acceptance.

#### Block 136 next slice — Surface tools

`ShellFeatureTools` adds Boundary/Fill, Trim/Extend, Stitch (Knit/Sew) and closed-shell-to-solid in the
Flächen ribbon group. The shared task panel provides ordered input lists, separate target/contour
pick roles, a live extension-distance arrow with numeric refinement and typed stitch tolerance.
Stable semantic box faces/edges supplement existing paths, materialized Trim profiles and visible
Surface Bodies. Pick masks and semantic scopes restore on Apply/Cancel. The Geometry authority
provides the actual outward extension frame and exact planar Fill, retaining spatial filling.

Pending Bodies and Length parameters stay off the session until one Apply transaction. Real result
previews and authoritative diagnostics gate Apply; invalid numeric edits restore the prior preview.
Cancel, Undo/Redo and Save/Open retain their authorities. Color-coded gap overlays remain deferred;
this slice displays diagnostic text and valid result geometry. Contract/proof:
`docs/gui-interactive-surface-mvp9.md`, `[block136-surface]`.

The Measure follow-up below restores that family. Assembly, browser feature editing and whole-block
acceptance remain open.

Validation for the Surface slice on 21 October 2026: full Core (513 cases, 15124 assertions),
Geometry (424 cases, 28199 assertions), and offscreen GUI (260 passed, seven explicit native skips,
15252 assertions) pass. The combined native Surface/spatial/Body/pattern/finishing/Sketch UI/viewport/
lifecycle run passes all 50 cases and 8592 assertions, covering all seven offscreen skips. Native
Surface proof opens the ribbon command, changes target and boundary by actual model clicks, drags
the outward arrow, refines the distance numerically, restores a second drag with Escape and clicks
Apply. Focused proof also covers ordered Boundary inputs, a semantic solid-face Trim with a Sketch
profile, closed-shell conversion, candidate collisions and exact history/persistence. Native task
captures were inspected and desktop startup passed (intentional six-second timeout). Commands:
`docs/development-setup.md`. Whole-block acceptance is still open.

#### Block 136 next slice — Measure

`ShellMeasureTools`, owned explicitly by `ShellWindow`, adds Abstand, Winkel, Radius / Durchmesser
and Körperdaten to the Prüfen ribbon group. A read-only task dock separates A/B picks, offers native
point/edge/face filters, displays units and permits copying current results. The existing analytic
Measure authority retains supporting-line/plane distance and 0–90 degree angle semantics.

A Geometry catalogue extracts compatible subshapes from current visible Bodies and posed Project
components. Its generation-scoped viewer ids never become Core references or saved topology indices.
Model-revision/freshness changes clear all picks, readout and witnesses before reselection. Close,
Escape and dock close restore the preceding selection filters/scope; no model transaction, dirty
state or history entry is introduced. A mouse-pass-through witness overlay follows explicit viewport
view-change notifications. Intersecting planes share a witness lying on both planes.
Contract/proof: `docs/gui-measure-mvp9.md`, `[block136-measure]`.

The Assembly follow-up below restores that family. Browser feature editing and whole-block
acceptance remain open.

Validation for the Measure slice on 22 October 2026: full Core (513 cases, 15124 assertions),
Geometry (426 cases, 28348 assertions) and offscreen GUI (266 passed, eight explicit native skips,
15653 assertions) pass. The combined native Measure/Surface/spatial/Body/pattern/finishing/Sketch UI/
viewport/lifecycle run passes all 57 cases and 9066 assertions, covering all offscreen skips. Native
Measure proof opens the ribbon command, picks actual vertices, changes camera, picks circular edges
and cylindrical faces, then closes the reachable dock. Focused proof covers clipboard/clear, all
exit paths, same-target rejection, stale/replaced topology, a known 100 mm posed-component distance,
unchanged dirty/history state and byte-identical saved Part files. Native captures were inspected
and desktop startup passed (intentional six-second timeout). Commands: `docs/development-setup.md`.
These checks validate the Measure slice without advancing whole-block acceptance.

#### Block 136 next slice — Assembly tools

`ShellAssemblyTools` restores a Baugruppe ribbon and scrollable task dock on `ShellWindow`. Project
creation and definition loading feed reusable Part/child-Assembly occurrences. Root components and
rigid subassemblies receive actual posed preview geometry, XYZ translation arrows or a chosen rotation
ring, six numeric placement fields and occurrence-state controls. Replacement-only previews support
hiding/suppressing an occurrence with no ghost result. Browser component identities now match scene
identities so a native component pick selects its tree row.

Disposable semantic target presentations follow occurrence placement; native A/B selection uses the
same second-target compatibility matrix as the existing relationship/joint controllers. Relationships,
joint coordinate motion and root-group solve publish actual solved poses. The joint table preserves
all five families and their typed slot order/limits; Spherical remains passive. The common overlay is
shared with Part feature manipulators. Source snapshots, freshness checks, drag Escape and task Cancel
protect document intent; one Apply remains one Project transaction. Scope is root-local, with nested
endpoint authoring and constrained free dragging deferred. Contract/proof:
`docs/gui-interactive-assembly-mvp9.md`, `[block136-assembly]`.

Browser feature editing and whole-block acceptance remain open; the numbered plan remains at
Block 136.

Validation on 22 October 2026: Core passes 513 cases / 15124 assertions; Geometry passes 427 cases /
28518 assertions. Full offscreen GUI passes 274 cases with 11 explicit native-only skips and 16191
assertions. The combined native Assembly/Measure/Surface/spatial/Body/pattern/finishing/Sketch UI/
viewport/lifecycle suite passes all 68 cases / 9737 assertions and covers every offscreen skip.
Additional native Extrude/Revolve proof passes 34 cases / 1085 assertions. Assembly captures at
1280×800 were inspected. The desktop startup smoke passes (intentional six-second timeout).
Legacy Feature tests now locate Apply in their own task panel; native Sketch dialog bounds checks
wait at most 500 ms for asynchronous window-manager placement and still require the complete frame
inside the available screen. These checks validate this slice without closing whole-block acceptance.

#### Block 136 next slice — Browser Feature Edit

`ShellFeatureEditTools`, owned explicitly by `ShellWindow`, restores actual browser mouse double-click
editing for all thirteen persistent feature-update families. The task preloads existing names, typed
parameter bindings, scalar settings, references, ordered lists and optional Sweep/Loft settings.
Existing intent structure and result identities remain fixed. One selected scalar uses the shared
native manipulator layer with live/numeric input and drag Escape; references use the form controls.

The headless edit controller supplies a disposable candidate after prepared parameter updates and the
matching Core `update_*`. Exact FeatureId validation prevents redirecting an edit to another record.
The shell recomputes the whole candidate, including downstream features, and displays actual result
Bodies. Source snapshots, freshness/task checks, cancellation and atomic session history protect the
authored model. Shared parameter and expression behavior, abandoned-binding edit disposal and ordered
reference edits have focused proof. Contract: `docs/gui-feature-edit-mvp9.md`, `[block136-feature-edit]`.

Whole-block acceptance and manifest validation remain open.

Validation on 22 October 2026: Core passes 513 cases / 15124 assertions; Geometry passes 427 cases /
28518 assertions. Full offscreen GUI passes 281 cases with 12 explicit native-only skips and 17452
assertions. Combined native Feature Edit/Assembly/Measure/Surface/spatial/Body/pattern/finishing/
Sketch UI/viewport/lifecycle/Extrude-Revolve regression passes all 109 cases / 11967 assertions,
covering every offscreen skip. Focused edit proof passes 10 cases with one native-only skip and
1297 assertions; native mouse editing has separate proof within the combined run. Captures at
1280×800 were inspected, and desktop startup passes (intentional six-second timeout). Commands:
`docs/development-setup.md`. These checks validate this slice without closing whole-block acceptance.

### Sketch interaction follow-up — 21 October 2026

The shell uses a nonmodal editor for snapped mouse/numeric transformations and multiple curve
selection. Mirror and Symmetric separate axis selection from element selection. Helper-line roles
are persistent Sketch intent and are filtered before region discovery. Projection supports off-plane
sources and visible finite semantic edges, plus explicitly unlinked snapshots from other sketches.
Spline creation continues through fit points until right-click/Enter; closed fitted curves and mixed
spline/line contours support profile selection, extrusion, Undo/Redo and Save/Open. Creation consumes
the full right-click gesture. Copy previews report relationships that are not duplicated before Apply.

Focused tags: `[sketch-mouse-operations]`, `[spline-create-workflow]`, `[sketch-delivery-review]`.
These changes retain the Block-135/136 boundary above; see the feature-specific contracts for details.


## Test accounting

At the start of the reset, 59 headless GUI cases across 20 files were retained. The 12 shell-bound
test files (73 cases) were deleted in Block 133 and are replaced progressively by shell tests in
Blocks 133–136; Block 136 must end with equivalent-or-better coverage proven by the updated coverage
manifest.

The 21 October 2026 Sketch follow-up passes 512 Core cases and 419 Geometry cases. The complete
offscreen GUI run passes 225 cases; its single native-viewer lifecycle case is skipped on that backend.
The native xcb run passes all 17 selected interaction, spline, delivery-regression and viewer-lifecycle
cases. Repeatable commands are recorded in `docs/development-setup.md`.

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
This hardening does not mark the outstanding Block-136 feature command surface complete.

## Block 136 acceptance audit and consumer restoration — 2026-10-22

`ShellInspectTools`, explicitly owned by `ShellWindow`, restores Part and flat/structured Assembly
STEP output plus read-only interference, clearance, contact and root-constraint-group DOF inspection.
Target publication is atomic after a successful staged export; source intent/history/poses remain
unchanged. Native proof checks actual task reachability and Qt file-dialog accept/cancel at 1280×800.
Contract/proof: `docs/gui-analysis-step-export-workbench-mvp7.md`, `[block136-inspect-export]`.

The v3 feature coverage manifest preserves the historical Block-131 dispositions and adds a separate
`shell` record per accepted family. Registered test/source verification and constructed-window entry
point checks are restored under `[integration][gui-feature-coverage]` / `[gui][shell-coverage]`.
Its regular green audit means the inventory is consistent, including its declared gaps. Completion
requires all family gaps and pending integrated workflows to be resolved; use the explicit closure
gate in `docs/gui-shell-acceptance-mvp9r.md`. Retained controller API tests are not shell UI proof.

The audit still identifies missing construction/workplane authoring, reference repair, Sketch3D,
parameter/Hole/advanced-extent editors, workspace aids and supported advanced Assembly controls.
These are concrete follow-up work, not a reason to certify whole-block parity. Execution order and
verification live in `docs/gui-shell-acceptance-mvp9r.md`; numbered status remains Block 136 in
`docs/mvp-plan.md`.

## Block 136 construction/workplane creation — 2026-10-23

`ShellConstructionTools` is an explicitly owned shell task for nine construction creation modes,
with disposable resolved previews and atomic parameter-plus-plane Apply. Resolved helper geometry
now appears in the viewport, and construction/derived workplanes support Sketch creation from both
the browser and viewport. The source selectors, invalid-input rejection, task lifecycle, exact
history/persistence and native 1280×800 presentation are covered by `[block136-construction]`.
Contract: `docs/gui-sketch-workbench-mvp7.md`. Object editing and remaining relation/pick workflows
stay recorded as datum-family gaps; whole-block acceptance is unchanged.

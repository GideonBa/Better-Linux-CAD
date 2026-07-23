# GUI Shell Reset Sequence MVP-9R

Status: active after accepted Interactive Modeling MVP-9. Blocks 132–136; Block 132 is the current
next technical step. STEP Import MVP-10 shifts to Blocks 137–143.

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

Scope narrowing (documented): the Ändern group (trim/extend/split/fillet via
`GuiSketchModifyController`) is not yet on the ribbon; it moves into Block 136 alongside feature
full coverage. The inference guide overlay (visual dashed guide line) is deferred with it; snap
inference itself works and is reported in the status bar.

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

### Block 135 — Interactive features minimal

Extrude and Revolve reachable from the ribbon: selection-first flow through
`GuiInteractiveFeatureCoordinator`, manipulator overlay (shell-owned replacement of the Block-123
overlay binder, notification-driven instead of polled), numeric coupling, preview/apply/cancel.
First fully usable end-to-end build: sketch → profile → extrude/revolve.

### Block 136 — Feature full coverage

Remaining feature commands in the ribbon (Fillet, Chamfer, Shell, Draft, Pattern, Mirror, Body
Boolean, Body Transform, Sweep, Loft, Surface family, Measure, Assembly tab), feature edit via
browser double-click (`GuiFeatureEditController`), and acceptance coverage replacing the deleted
shell acceptance tests (coverage manifest updated).

## Test accounting

Headless GUI tests (59 TEST_CASEs across 20 files) must stay green through every block. The 12
shell-bound test files (73 TEST_CASEs) are deleted in Block 133 and replaced progressively by new
shell tests in Blocks 133–136; Block 136 must end with equivalent-or-better coverage proven by the
updated coverage manifest.

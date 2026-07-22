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

### Block 133 — Shell reset

Delete `main_window.hpp/.cpp`, all seven binders, `gui_sketch_dimension_binder.hpp`, and the
shell-bound test files. Add `src/gui/shell/` with `ShellWindow`, ribbon (tab bar + grouped panels),
model browser dock, diagnostics panel, status bar, document lifecycle (new/open/save/dirty
confirmation), selection flow (viewport → session selection → browser sync, single path), the
revision-gated refresh flow, and view controls (fit, standard views, projection, display mode).
Ribbon content in this block: document commands and view commands plus a "Skizze erstellen" entry
point (plane selection → sketch mode enter/finish skeleton). New shell tests cover construction,
refresh gating, selection sync, and document lifecycle offscreen.

### Block 134 — Sketch in the new shell

Sketch ribbon tab with draw (line, circle, arc, rectangle, …), modify, constrain, dimension, and
finish groups. The five binder implementations are replaced by shell-owned components
(`shell/sketch_*`) that receive explicit references (viewport, session, ribbon) and reuse the
headless sketch controllers unchanged. Sketch grid/snap/inference/numeric HUD surface as in the
MVP-8 contracts.

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

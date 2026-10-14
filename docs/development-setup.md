# Development Setup

This document covers the current local BLCAD build, test, formatting, and executable workflows.
Feature status and numbered sequencing live in `docs/mvp-plan.md`; exact contracts live in the
feature-specific documents under `docs/`.

## Target environment

- Ubuntu 24.04
- C++20
- CMake 3.28 or newer
- Ninja
- OCCT/Open CASCADE for optional Geometry targets
- Qt 6 for the optional desktop, Interactive Sketcher, and Interactive Modeling
- nlohmann-json
- Catch2 3

The Assembly numeric engine and Block-109 planar Sketch solver use project-owned deterministic dense
elimination routines. Eigen is installed by the reference dependency set but is not currently a BLCAD
target dependency.

## Install dependencies

See `docs/dependencies-ubuntu-24.04.md` for package notes.

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build pkg-config git clang-format clang-tidy \
  libocct-foundation-dev libocct-modeling-data-dev libocct-modeling-algorithms-dev \
  libocct-data-exchange-dev libocct-visualization-dev libocct-ocaf-dev libocct-draw-dev \
  qt6-base-dev libeigen3-dev libtbb-dev nlohmann-json3-dev libfmt-dev libspdlog-dev catch2 \
  doxygen graphviz
```

## Core build and tests

Complete Core workflow:

```bash
cmake --workflow --preset dev-build-test
```

Equivalent commands:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

## Geometry build and tests

Complete Geometry workflow:

```bash
cmake --workflow --preset dev-geometry-build-test
```

Equivalent commands:

```bash
cmake --preset dev-geometry
cmake --build --preset dev-geometry
ctest --preset dev-geometry --output-on-failure
```

## GUI build and tests

The GUI requires Geometry. Complete GUI workflow:

```bash
cmake --workflow --preset dev-gui-build-test
```

Equivalent commands:

```bash
cmake --preset dev-gui
cmake --build --preset dev-gui
ctest --preset dev-gui --output-on-failure
```

The executable output is `blcad`; the CMake target is `blcad_app`. Run it with:

```bash
./build/dev-gui/blcad
```

The `dev-gui` build directory registers the Core, Geometry, and GUI test executables, so
`ctest --preset dev-gui` is the broadest run. Only the discovered GUI cases:

```bash
ctest --preset dev-gui -R '^gui\.' --output-on-failure
```

`CMakeLists.txt` sets `ENVIRONMENT "QT_QPA_PLATFORM=offscreen"` on every discovered GUI test, so
ctest needs no platform prefix. Invoking the test binary directly does:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][shell]"
```

On Linux the OCCT viewport uses Qt xcb when `DISPLAY` is available and `QT_QPA_PLATFORM` is unset.
Offscreen GUI tests use a logical viewport without X11/GLX. A change to viewport rendering or to
interactive behaviour therefore needs a targeted run under `QT_QPA_PLATFORM=xcb` against the test
binary directly, plus a smoke run of `./build/dev-gui/blcad`; offscreen alone does not prove it.

## Interactive Sketcher focused proof

Blocks 106–121 are implemented and accepted.

Block 106 workspace and command lifecycle:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-workspace]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-command-lifecycle]"
```

Block 107 plane interaction:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-hit-test]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-snap]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-box-selection]"
```

Block 108 shared topology and migration:

```bash
./build/dev/blcad_core_tests "[core][sketch-topology]"
./build/dev/blcad_core_tests "[core][sketch-edit-command]"
./build/dev/blcad_core_tests "[core][sketch-json-migration]"
```

Block 109 deterministic planar solver:

```bash
./build/dev/blcad_core_tests "[core][sketch-solver]"
./build/dev/blcad_core_tests "[core][sketch-dof]"
./build/dev/blcad_core_tests "[core][sketch-conflict-diagnostics]"
```

Block 110 solver-backed drag:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-drag]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][sketch-live-solve]"
```

Block 111 basic creation — the Block-133 shell reset replaced the MVP-8 creation binder and its
tests; line, circle, and rectangle creation is now proved through the shell tool surface:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-create]"
./build/dev-geometry/blcad_geometry_tests "[gui][sketch-profile-selection]"
```

Block 112 conic and slot creation has no focused proof. Ellipse/slot creation lives in the GUI layer
only (`gui_sketch_create`, `shell/shell_sketch_tools`); there is no Core or Geometry conic intent, and
the shell tool tests cover line/circle/rectangle. Closing this gap belongs to Block 136 (feature full
coverage, `docs/gui-shell-reset-sequence-mvp9r.md`).

Block 113 spline editing and Sketch text:

```bash
./build/dev/blcad_core_tests "[core][sketch-spline-edit]"
./build/dev/blcad_core_tests "[core][sketch-text]"
./build/dev-geometry/blcad_geometry_tests "[geometry][sketch-spline-edit]"
./build/dev-geometry/blcad_geometry_tests "[geometry][sketch-text]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-spline]"
```

Block 114 manual/automatic constraints, conflict preview, and glyph interaction:

```bash
./build/dev/blcad_core_tests "[core][sketch-constraints]"
./build/dev/blcad_core_tests "[core][sketch-conflict-diagnostics]"
./build/dev-geometry/blcad_geometry_tests "[geometry][sketch-constraints]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-constraints]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][sketch-auto-constraint]"
```

Block 115 driving/reference dimensions, typed parameters, expressions, and annotation editing:

```bash
./build/dev/blcad_core_tests "[core][sketch-dimensions]"
./build/dev-geometry/blcad_geometry_tests "[geometry][sketch-dimensions]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-dimensions]"
./build/dev/blcad_core_tests "[integration][sketch-expression-edit]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][sketch-live-solve]"
```

The Block-115 proof covers all nine family signatures, deterministic sidecar roundtrip, typed
Length/Angle parameter compatibility, direct and expression-backed value changes, driving/reference
semantics, calibrated arc length, stable semantic dimension annotations, exact endpoint hit roles,
atomic add/edit/rebind/mode history, Save/Open, Qt command registration, and later drag enforcement.

Block 116 trim/extend/split, two-line fillet/chamfer, and remap-or-reject rewrite:

```bash
./build/dev/blcad_core_tests "[core][sketch-modify]"
./build/dev-geometry/blcad_geometry_tests "[geometry][sketch-modify]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-trim-extend]"
```

The Block-116 proof covers trim shorten/split-middle/remove, extend to the next intersection,
line/arc/spline split, fillet tangent-arc and chamfer connector insertion, preserved in-place
constraints, explicit rejection of modifications that invalidate a referencing dimension or profile,
a chamfer result resolving through the Geometry region pipeline, atomic GUI preview/commit with exact
undo/redo, and fail-closed commit against a Block-114 catalog reference.

Block 117 offset, associative projection/include, and break-link conversion:

```bash
./build/dev/blcad_core_tests "[core][sketch-offset-project]"
```

Block 118 transforms, mirror, and Sketch patterns:

```bash
./build/dev/blcad_core_tests "[core][sketch-transform-pattern]"
```

Block 119 region recognition, profile selection, diagnostics, and Finish Sketch:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][sketch-regions]"
./build/dev-geometry/blcad_geometry_tests "[integration][sketch-finish]"
./build/dev-gui/blcad_geometry_tests "[sketch-curved-regions]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[sketch-curved-regions]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[sketch-curved-regions]"
```

Block 120 Interactive Sketch3D (headless Core proof):

```bash
./build/dev/blcad_core_tests "[gui][sketch-3d-edit]"
./build/dev/blcad_core_tests "[integration][sketch-3d-direct-manipulation]"
```

## Interactive Modeling focused proof

Blocks 122 and 123 have no focused proof in the tree. Their cases asserted against the deleted
MainWindow shell — stable Qt object names, the mini toolbar, the ViewCube/bookmark widgets, and the
manipulator overlay binder — and were removed with the Block-133 shell reset
(`docs/gui-shell-reset-sequence-mvp9r.md`). The headless authorities they covered,
`GuiModelingWorkspace` and `GuiViewportManipulatorLayer`, are unchanged and still exercised by the
Block 124–130 controller proofs below. Block 135 re-establishes manipulator overlay coverage against
`ShellWindow`; Block 136 restores the remaining workspace-level coverage.

The Block 124–130 proofs below are headless controller tests and are unaffected by the shell reset.

Block 124 interactive Extrude, path Extrude, and Revolve authoring:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][extrude-revolve]"
```

These cases verify extent-parameter driving with one atomic transaction and exact undo, operation/
taper/thin/through-all modes with their offered handles, fail-closed rejection of invalid inputs,
full/angle/symmetric revolve authoring, and manipulator/typed document equivalence. The dedicated
`[gui][interactive-extrude]`, `[gui][interactive-revolve]`, and
`[integration][extrude-revolve-manipulator]` cases asserted against the deleted MainWindow shell and
were removed with the Block-133 shell reset; Block 135 restores their replacement-shell path through
the focused `[gui][shell-features]` proof.

Block 125 interactive Fillet, Chamfer, Shell, and Draft authoring:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-finishing]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-shell-draft]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][edge-chain-picking]"
```

These cases verify ordered edge collection with duplicate rejection and exact undo, the three chamfer
modes with their offered handles, shell direction toggle and thickness, draft pull/neutral/angle
authoring, fail-closed rejection of invalid inputs, and manipulator/typed document equivalence for
fillet radius and coordinator-routed shell thickness.

Block 126 interactive Pattern, Mirror, Body Boolean, and Body Transform authoring:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-pattern-mirror]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-body-operation]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][pattern-ghost-preview]"
```

These cases verify linear/circular pattern parameter driving with exact undo, mirror authoring,
boolean role validation and keep toggle, the translate/rotate/scale transform stack, fail-closed
rejection of invalid inputs, and manipulator/typed document equivalence for pattern spacing and
coordinator-routed transform translation.

Block 127 interactive PathCurve, Sweep, and Loft authoring:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-path-sweep]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-loft]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][ordered-section-picking]"
```

These cases verify ordered connected path collection with duplicate rejection, sweep authoring with
exact undo, the sweep-twist handle and combined guide-plus-twist rejection, loft two-section authoring
with G2 rejection, fail-closed rejection of invalid inputs, and drag-reorder/direct-order document
equivalence.

Block 128 interactive Surface authoring and surface-to-solid conversion:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-surface]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][stitch-diagnostic-overlay]"
```

These cases verify fill authoring with duplicate rejection, extend distance driving and apply,
closed-shell-to-solid fail-closed reporting, fail-closed rejection of invalid inputs, the
tolerance-driven stitch free-edge/gap diagnostic surfaced before commit, a clean coplanar stitch, and
coordinator-routed extend-distance/typed document equivalence.

Block 129 feature edit lifecycle and Core feature-update commands:

```bash
./build/dev/blcad_core_tests "[core][feature-update-command]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][feature-edit]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][feature-edit-recompute]"
```

These cases verify in-place feature update with stable id/position/JSON and preserved downstream and
co-producer edges, dependency-protected remove, double-click family identification, one update
transaction with exact undo and fail-closed rejection, and downstream recompute after an upstream
edit.

Block 130 interactive Assembly placement, relationships, joints, and motion:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-assembly-placement]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-relationships]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-joint-motion]"
```

These cases verify component and rigid-subassembly placement, translate/rotate triads, one-click state
editing and undo, compatibility-filtered relationship/joint picking, solved relationship candidates,
root-space joint frames, typed slot limits, one-coordinate vector drives, motion HUD solve/DOF state,
atomic pose/intent application, and Spherical drive rejection.

Block 131 integrated acceptance:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][interactive-modeling]"
```

Measure (`include/blcad/geometry/geometry_measure.hpp`) and the coverage-manifest check currently have
no tests: `[gui][measure]` and `[integration][modeling-coverage-manifest]` lived in the shell-bound
files deleted by Block 133, and nothing replaced them. `GeometryMeasure` is therefore an uncovered
Geometry authority; Block 136 owns restoring both (`docs/gui-shell-reset-sequence-mvp9r.md`).

## Representative existing validation tags

```bash
./build/dev/blcad_core_tests "[core][part-body]"
./build/dev/blcad_core_tests "[core][parameter-expression]"
./build/dev/blcad_core_tests "[core][sketch-update]"
./build/dev-geometry/blcad_geometry_tests "[geometry][revolve-feature]"
./build/dev-geometry/blcad_geometry_tests "[geometry][sweep-feature]"
./build/dev-geometry/blcad_geometry_tests "[geometry][surface-stitch]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-structured-step-export]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][shell]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][document-session]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][viewport]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][model-browser]"
```

The exact test registration in `CMakeLists.txt` is authoritative, and the tags there are the only
authority for what exists — this document lists representative entry points, not a complete set. All
cases live in the single `blcad_gui_tests` executable; no second Qt application instance is
introduced.

## Headless tools

```bash
./build/dev/blcad_inspect_project_components examples/component_instances.blcad.project.json
./build/dev-geometry/blcad_export_step examples/reference_plate.blcad.json build/reference_plate.step
./build/dev-geometry/blcad_export_project \
  examples/component_instances.blcad.project.json assembly.parameter 1 build/export
./build/dev-geometry/blcad_export_posed_assembly input.blcad.project.json output.step
./build/dev-geometry/blcad_export_structured_assembly input.blcad.project.json output.step
./build/dev-geometry/blcad_move_joint input.blcad.project.json joint.id 30 output.blcad.project.json
./build/dev-geometry/blcad_analyze_assembly input.blcad.project.json
```

## Public Blocks 122–131 boundaries

Block 133 deleted the MainWindow shell, all seven binders, and the shell-bound test files; the headless
boundaries below are what survived and are consumed unchanged by `ShellWindow`
(`docs/gui-shell-reset-sequence-mvp9r.md`). `gui_interactive_extrude_revolve_binder.hpp` was renamed to
`gui_interactive_feature_coordinator.hpp`: it never was a binder, and it coordinates all nine Block
124–128 controllers, not just Extrude/Revolve.

```text
include/blcad/gui/gui_modeling_workspace.hpp
include/blcad/gui/gui_viewport_manipulator.hpp
include/blcad/gui/gui_interactive_extrude_revolve.hpp
include/blcad/gui/gui_interactive_finishing.hpp
include/blcad/gui/gui_interactive_pattern_body.hpp
include/blcad/gui/gui_interactive_path_sweep_loft.hpp
include/blcad/gui/gui_interactive_surface.hpp
include/blcad/gui/gui_feature_edit.hpp
include/blcad/gui/gui_interactive_assembly.hpp
include/blcad/gui/gui_interactive_feature_coordinator.hpp
include/blcad/gui/gui_measure.hpp
include/blcad/geometry/geometry_measure.hpp
include/blcad/core/part_document.hpp
tests/gui/gui_interactive_finishing_tests.cpp
tests/gui/gui_interactive_pattern_body_tests.cpp
tests/gui/gui_interactive_path_sweep_loft_tests.cpp
tests/gui/gui_interactive_surface_tests.cpp
tests/gui/gui_feature_edit_tests.cpp
tests/gui/gui_interactive_assembly_tests.cpp
tests/gui/gui_interactive_modeling_mvp9_acceptance_tests.cpp
tests/core/feature_update_command_tests.cpp
docs/gui-modeling-workspace-mvp9.md
docs/gui-viewport-manipulators-mvp9.md
docs/gui-interactive-extrude-revolve-mvp9.md
docs/gui-interactive-finishing-mvp9.md
docs/gui-interactive-pattern-body-mvp9.md
docs/gui-interactive-path-sweep-loft-mvp9.md
docs/gui-interactive-surface-mvp9.md
docs/gui-feature-edit-mvp9.md
docs/gui-feature-update-inventory-mvp9.json
docs/gui-interactive-assembly-mvp9.md
docs/gui-measure-mvp9.md
docs/gui-feature-coverage-manifest-mvp9.json
docs/interactive-modeling-mvp9-acceptance.md
```

All Block-122 through Block-128 transient state — command catalogs, verified preselection
capabilities, mini-toolbar state, repeat, filters, Home/bookmarks, camera mappings, handle
descriptors, pointer measurements, hit products, HUD text, edge/face chains, pattern sources, tool
bodies, path segments, loft section order, boundary/surface selections, candidate feature parameters,
and previews — does not change Core/Geometry or any save format. A controller's Apply is the only
mutation, and it flows through one existing `commit_part_transaction`. Block 129 is the single
exception: it adds Core `update_<family>`/`remove_<family>` commands (the `[core][feature-update-command]`
boundary) that preserve ids, ordered position, and JSON shape, consumed by the headless
`GuiFeatureEditController` through the same transaction path.
Block 130 adds no new persistence authority: its transient occurrence candidates, resolved target
capabilities, oriented frames, solved poses, coordinate-control/HUD state, and motion results apply
through existing `commit_project_transaction`, solver-result, and motion-result boundaries.
Block 131 likewise adds no persistent authority: Measure selections/readouts/overlays are transient,
and coverage plus integrated acceptance are checked-in validation artifacts.

## Formatting

Formatting is configured by `.editorconfig` and `.clang-format`.

```bash
find include src tests -name '*.hpp' -o -name '*.cpp' | xargs clang-format -i
```

Formatting a focused change set instead:

```bash
git diff --name-only --diff-filter=ACM -- '*.hpp' '*.cpp' | xargs -r clang-format -i
```

## Clean generated files

```bash
rm -rf build/
```

## Documentation entry points

- `README.md`: repository entry point
- `docs/mvp-plan.md`: numbered status/current block
- `docs/architecture-summary.md`: condensed authority model
- `docs/file-format.md`: historical save-format authority
- `docs/interactive-sketcher-sequence-mvp8.md`: Blocks 106–121 sequence
- `docs/interactive-modeling-sequence-mvp9.md`: Blocks 122–131 sequence
- `docs/gui-modeling-workspace-mvp9.md`: Block-122 selection-first shell contract
- `docs/gui-viewport-manipulators-mvp9.md`: Block-123 manipulator and HUD contract
- `docs/gui-interactive-assembly-mvp9.md`: Block-130 Assembly interaction contract
- `docs/sketch-planar-constraint-solver-mvp8.md`: solver mathematics and dimension mappings
- `docs/gui-sketch-solver-drag-mvp8.md`: constraint/dimension-aware direct manipulation
- `docs/gui-sketch-constraint-authoring-mvp8.md`: Block-114 constraint/glyph contract
- `docs/gui-sketch-dimension-authoring-mvp8.md`: Block-115 dimension/expression contract

## Current development boundary

`docs/mvp-plan.md` is the status authority; this section only names the current build/test boundary.

Interactive Sketcher MVP-8 (Blocks 106–121) and Interactive Part & Assembly Modeling MVP-9 (Blocks
122–131) are implemented and accepted. GUI Shell Reset MVP-9R replaces the Qt shell those phases grew:
Blocks 132 (QOpenGLWidget viewport), 133 (`ShellWindow` + `ShellRibbon`, origin seeding), 134
(`ShellSketchTools`), and 135 (`ShellFeatureTools`, Extrude/Revolve, manipulator overlay) are
implemented; Block 136 restores full feature and acceptance coverage.

## GUI Shell Reset focused proof

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][viewport]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][shell]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][ribbon]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][origin]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-entry]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][shell-sketch]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][shell-features]"
```

Block 132 proves the QOpenGLWidget render surface and the single-repaint funnel; Block 133 proves the
ribbon, model browser with its origin folder, document lifecycle, selection and revision-gated refresh,
and origin seeding; Block 134 proves the shell-owned draw/constraint/dimension/modify tool surface and
solver-backed dragging. Block 135 proves persistent profile handoff/reselection, strict numeric
units, pointer-drag numeric override and rollback, candidate-only auto-provisioning, mutation-free
Cancel, recomputed Extrude/Revolve results, and atomic Undo.

### Sketch operation follow-up

The shell now wires numeric preview/Apply/Cancel for transforms, independent patterns, offsets,
associative projection and projected-line detachment. Focused regressions:

```bash
./build/dev-gui/blcad_core_tests "[core][sketch-transform-pattern],[core][sketch-offset-project]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-transform-pattern],[gui][sketch-offset-project],[gui][sketch-project]"
```

`gui_shell_sketch_operations_tests.cpp` covers the real ribbon editor as well as the dialog-free
surface, history/persistence, source-point preservation, copied rational profiles, and atomic
constraint/dimension conflict rejection. The remaining limits are recorded in the two operation
contracts and the interactive Sketch coverage manifest.

Sketch UI regressions for visible windows and native OpenGL (requires an authorized local X display):

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-ui-review]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[gui][sketch-ui-review],[gui][viewport]"
QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 ./build/dev-gui/blcad_gui_tests "[gui][sketch-ui-review]"
QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=2 ./build/dev-gui/blcad_gui_tests "[gui][sketch-ui-review]"
```

Set `BLCAD_UI_CAPTURE_DIR` to an optional writable directory to capture the test shell, previews and
operation dialogs for visual inspection. Captures use the test application's own widgets. The suite
checks 1280×800 layout, horizontal command reachability, supported/mixed selection gating, actual
button and Escape events, preview cleanup, and main-window Undo/Redo.

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

Block 112 conic and slot creation uses the shell tool surface and exact spline spans:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-create-conics]"
```

These focused cases cover persistent creation, degenerate-input rejection and tangent references.
They do not close the remaining Block-136 feature coverage work recorded in
`docs/gui-shell-reset-sequence-mvp9r.md`.

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

The original `[gui][measure]` and `[integration][modeling-coverage-manifest]` shell proofs were deleted
by Block 133. Their replacement-shell coverage is restored through `[block136-measure]` and
`[integration][gui-feature-coverage]`; commands and bounded acceptance are recorded below.

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

### Block 136 finishing slice

```bash
./build/dev-gui/blcad_core_tests "[rectangular-finishing]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-finishing]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-finishing]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-finishing],[sketch-ui-review],[gui][viewport],[native-viewer-lifecycle]"
```

The shell proof covers Fillet, three Chamfer modes, Shell and Draft through actual actions and task
controls, current-body multi-selection, recomputed solid results, pending parameter isolation,
Cancel, rejection/rollback, atomic Undo/Redo and Save/Open. The native-only case also checks actual
subshape mouse detection and drag/numeric coupling; it skips explicitly with the offscreen backend.
Optional `BLCAD_UI_CAPTURE_DIR` captures the test windows.

### Block 136 Body pattern and Mirror slice

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-patterns]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-patterns]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-patterns],[block136-finishing],[sketch-ui-review],[gui][viewport],[native-viewer-lifecycle]"
```

This proof covers Body multiselection, linear spacing/total extent and direction, circular angles,
Mirror planes, box and closed-spline solid sources, count and scalar rollback, candidate-only
parameters/result Body, Cancel, atomic Undo/Redo and Save/Open. The native-only case checks actual Body picking through a
live preview and spacing/angle/count drag refinement; it skips explicitly offscreen. Optional
`BLCAD_UI_CAPTURE_DIR` captures the test windows.

### Block 136 Body Boolean and Transform slice

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-body]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-body]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-body],[block136-patterns],[block136-finishing],[sketch-ui-review],[gui][viewport],[native-viewer-lifecycle]"
```

This proof covers Add/Subtract/Intersect, target/tool picking, both result modes, tool retention,
candidate-only NewBody, actual solid results, multi-Body XYZ/rotation/scale, ordered stack retention,
explicit ownership flags, failed-preview isolation, Cancel, atomic Undo/Redo and Save/Open. Native
checks exercise actual Boolean target/tool picks, multi-Body detection, drag/numeric refinement and
Escape rollback; offscreen skips those two cases explicitly. Optional `BLCAD_UI_CAPTURE_DIR` captures
the preview windows.

### Block 136 PathCurve, Sweep and Loft slice

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-spatial]"
./build/dev-gui/blcad_geometry_tests "[block136-spatial]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-spatial]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-spatial],[block136-body],[block136-patterns],[block136-finishing],[sketch-ui-review],[gui][viewport],[native-viewer-lifecycle]"
```

The proof covers connected/reversed PathCurve creation, canonical path presentation, planar rational
spline trajectories, solid/cut/surface Sweep and Loft, ordered list-model moves, twist, guides,
center paths, G1/G2 handling, fitted spline profiles, candidate-only inputs, failed-preview isolation,
Cancel, atomic Undo/Redo and Save/Open. Native checks exercise profile/path/section mouse picking
and twist drag/numeric refinement; offscreen skips the native case explicitly. Optional
`BLCAD_UI_CAPTURE_DIR` captures the preview windows.

### Block 136 Surface slice

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-surface],[interactive-surface]"
./build/dev-gui/blcad_geometry_tests "[surface-boundary-fill],[surface-trim-extend],[surface-stitch],[surface-to-solid]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-surface]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-surface],[block136-spatial],[block136-body],[block136-patterns],[block136-finishing],[sketch-ui-review],[gui][viewport],[native-viewer-lifecycle]"
```

The Surface slice verifies all six commands, candidate-only Bodies/Length parameters, invalid
geometry/numeric rollback, ordered selections, role-specific picking, actual result shapes, exact
Undo/Redo and Save/Open. Geometry proof covers exact planar Fill and recomputed outward extension
frames. Native proof uses real target/boundary clicks, extension dragging, typed refinement and the
visible Apply button. Optional `BLCAD_UI_CAPTURE_DIR` saves review captures. Contract and bounded
limits: `docs/gui-interactive-surface-mvp9.md`. These checks do not close Block-136 acceptance.

### Block 136 Measure slice

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-measure]"
./build/dev-gui/blcad_geometry_tests "[block136-measure]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-measure]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-measure],[block136-surface],[block136-spatial],[block136-body],[block136-patterns],[block136-finishing],[sketch-ui-review],[gui][viewport],[native-viewer-lifecycle]"
```

The slice verifies all four readouts, clipboard and clear, explicit pick roles/filters, cancellation
through all close paths, scope restoration, stale and replaced geometry, byte-identical saved Parts,
unchanged dirty/history state and a known posed Project-component distance. Geometry proof verifies
World-space targets and intersecting-plane witnesses. Native proof opens the ribbon command, clicks
actual model vertices, follows a camera change, selects circular edges/cylindrical faces and closes
the reachable task dock. Optional `BLCAD_UI_CAPTURE_DIR` records review captures. Scope and existing
analytic distance semantics: `docs/gui-measure-mvp9.md`. Block-136 acceptance remains open.

### Sketch operation follow-up

The shell wires pointer dragging, live previews, numeric refinement and Apply/Cancel for transforms,
independent patterns and offsets. Mirror and Symmetric have separate axis and element pick modes.
Projection supports associative references and explicitly unlinked snapshots of other Sketch curves.
Focused regressions:

```bash
./build/dev-gui/blcad_core_tests "[core][sketch-transform-pattern],[core][sketch-offset-project]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-transform-pattern],[gui][sketch-offset-project],[gui][sketch-project]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[sketch-mouse-operations],[spline-create-workflow],[sketch-delivery-review]"
./build/dev-gui/blcad_geometry_tests "[sketch-reference]"
```

`gui_shell_sketch_operations_tests.cpp` covers the real ribbon editor as well as the dialog-free
surface, history/persistence, source-point preservation, copied rational profiles, and atomic
constraint/dimension conflict rejection. The spline workflow covers unlimited fit points, right-click
and Enter completion, exact smooth closure, Finish Sketch handoff, helper-line exclusion, region
selection and actual extrusion. The remaining limits are recorded in the operation contracts and
the interactive Sketch coverage manifest.

Sketch UI regressions for visible windows and native OpenGL (requires an authorized local X display):

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-ui-review]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[gui][sketch-ui-review],[gui][viewport]"
QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 ./build/dev-gui/blcad_gui_tests "[gui][sketch-ui-review]"
QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=2 ./build/dev-gui/blcad_gui_tests "[gui][sketch-ui-review]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[native-viewer-lifecycle]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[sketch-mouse-operations],[spline-create-workflow],[sketch-delivery-review]"
```

Set `BLCAD_UI_CAPTURE_DIR` to an optional writable directory to capture the test shell, previews and
operation dialogs for visual inspection. Captures use the test application's own widgets. The suite
checks 1280×800 layout, horizontal command reachability, supported/mixed selection gating, actual
button and Escape events, preview cleanup, and main-window Undo/Redo.
`[native-viewer-lifecycle]` additionally creates, paints and destroys three native shell windows.
It rejects high-severity OCCT teardown messages, including attempts to make a GLX context current
after its top-level drawable has already been destroyed. The shell releases its central viewer while
that drawable and the document session still exist. This case is skipped on the offscreen backend.

### Block 136 Assembly slice

```bash
./build/dev-gui/blcad_geometry_tests "[block136-assembly]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-assembly]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-assembly]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-assembly],[block136-measure],[block136-surface],[block136-spatial],[block136-body],[block136-patterns],[block136-finishing],[sketch-ui-review],[gui][viewport],[native-viewer-lifecycle]"
```

The focused slice covers disposable semantic targets, definition imports, component/rigid-subassembly
insertion, placement and state previews, compatibility-filtered relationships, all joint families,
group solve, ordered slot limits, slider/numeric motion, history and persistence. Native cases also
verify real occurrence picking and translate/rotate/motion handles, numeric refinement, drag Escape,
and reachable task controls. Offscreen explicitly skips the native-only cases; run xcb to cover them.
`BLCAD_UI_CAPTURE_DIR` enables native captures. These checks do not complete Block 136 acceptance.

### Block 136 browser Feature Edit slice

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-feature-edit],[gui][feature-edit],[integration][feature-edit-recompute]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-feature-edit]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-feature-edit],[block136-assembly],[block136-measure],[block136-surface],[block136-spatial],[block136-body],[block136-patterns],[block136-finishing],[sketch-ui-review],[gui][viewport],[native-viewer-lifecycle],[shell-features]"
```

The focused shell cases cover all thirteen update-capable families, complete downstream previews and
failure rejection, parameter values/rebinding/formulas, ordered sections and edge membership, optional
Sweep twist, stale tasks and exact history/persistence. The native-only case checks actual tree mouse
double-clicks, scalar drag/numeric refinement, drag Escape and reachable Apply at 1280×800; offscreen
explicitly skips it. The headless proof also rejects attempts to redirect an edit to another feature id.
`BLCAD_UI_CAPTURE_DIR` enables captures. Whole-block acceptance and manifest validation remain open.


### Block 136 analysis and STEP consumers

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-inspect-export]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-inspect-export]"
```

The shell tests cover Part creation/STEP output, flat and structured nested Assembly export, fresh/idle
context guards, preserved targets and source files, read-only collision/clearance/contact/DOF results,
input/model invalidation and history/persistence preservation. The native case checks reachable task
controls at 1280×800 and Qt file-dialog acceptance/cancellation; only that case skips offscreen.
`BLCAD_UI_CAPTURE_DIR` enables captures. These bounded consumers do not complete whole-shell acceptance.

### Block 136 shell coverage inventory

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][gui-feature-coverage]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[gui][shell-coverage],[block136-inspect-export],[block136-feature-edit],[block136-assembly],[block136-measure],[block136-surface],[block136-spatial],[block136-body],[block136-patterns],[block136-finishing],[sketch-ui-review],[gui][viewport],[native-viewer-lifecycle],[shell-features]"
```

The inventory checks exact family parity, real registered proof tags/source files, shell entry points,
explicit gap accounting and plan/manifest completion consistency. Its negative controls reject missing
families, deleted tags, headless substitutions, OR masking and false readiness. Inventory validity
is separate from whole-shell acceptance: `docs/gui-shell-acceptance-mvp9r.md` describes the remaining
work and the opt-in `BLCAD_REQUIRE_SHELL_ACCEPTANCE=1` closure gate, which fails while gaps remain.
Run full Core, Geometry and offscreen GUI suites plus native interaction coverage before closing the
block; historical controller tests alone do not establish equivalent shell coverage.

### Block 136 construction and workplane creation

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app blcad_geometry_tests blcad_core_tests -j4
./build/dev-gui/blcad_geometry_tests "[geometry][construction-presentation]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-construction],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-construction-captures QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-construction]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[gui][shell-coverage],[block136-construction],[block136-inspect-export],[block136-feature-edit],[block136-assembly],[block136-measure],[block136-surface],[block136-spatial],[block136-body],[block136-patterns],[block136-finishing],[sketch-ui-review],[gui][viewport],[native-viewer-lifecycle],[shell-features]"
```

The shell cases cover nine creation modes, normalized directions, stable source selectors, disposable
Geometry previews, degenerate-input rejection, parameter-plus-plane atomicity and existing bindings,
exact Undo/Redo, canonical Save/Open, committed-only saving during preview, context/revision guards,
Escape/dock close and construction/derived planes as Sketch/Extrude inputs. Geometry tests inspect
actual OCCT bounds for resolved points, lines, derived axes and offset/generated workplanes.
The native case checks real ribbon mouse input, pending numeric keyboard input consumed by Apply,
reachable Apply and OCCT preview at 1280×800; only that case skips offscreen.
`BLCAD_UI_CAPTURE_DIR` enables captures.
Remaining relation/pick workflows keep the datum family and Block 136 open; see
`docs/gui-sketch-workbench-mvp7.md` and `docs/gui-shell-acceptance-mvp9r.md`.

### Combined replacement-shell UI workflows

```bash
cmake --build --preset dev-gui --target blcad_gui_tests -j4
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[shell-combined-workflow]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-combined-workflows QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[shell-combined-workflow]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-construction-manipulators],[block136-construction-picking],[shell-mixed-path-loft],[block136-construction-relations],[block136-construction-edit],[shell-combined-workflow],[gui][shell-coverage],[block136-construction],[block136-inspect-export],[block136-feature-edit],[block136-assembly],[block136-measure],[block136-surface],[block136-spatial],[block136-body],[block136-patterns],[block136-finishing],[sketch-ui-review],[gui][viewport],[native-viewer-lifecycle],[shell-features]"
```

`tests/gui/gui_shell_combined_workflow_tests.cpp` combines long interaction sequences in eight
scenarios: machining/edit/pattern/transform/Measure/STEP, Boolean/scale/Mirror with history branching,
multi-workplane Loft section ordering/editing, authored-Part import/Assembly placement/Mate/solve/
analysis/STEP, invalid/cancelled tasks plus dirty-file and document-switch recovery, and workplane
editing with cyclic-reference recovery and downstream solid recompute, plus generated-reference
construction chains feeding a second Sketch/Extrude and later upstream edits, and construction grips
rotating a workplane and moving its downstream solid.

The shared harness operates Qt ribbon buttons and popup menus, task widgets, browser clicks and
double-clicks, viewport Sketch picks/hover, Escape and absolute/relative numeric coordinates, and
actual Qt input/file/confirmation dialogs. It does not seed Core intent or invoke authoring
controllers/workbenches. Assertions observe canonical
intent, resolved volumes/solid counts/bounds, source/result identities, task/preview/filter cleanup,
multi-step Undo/Redo and redo-branch truncation. Save/Open/Save compares complete bytes, including GUI
sidecars; exports preserve source bytes. Dialog automation has bounded watchdogs, and native input
checks widget reachability at 1280×800. Both backends execute all eight scenarios; these tests introduce
no native-only skips. The native run is required to prove actual OCCT presentation.

These bounded combined workflows extend the shell evidence. Remaining families, manipulator/input
variants and whole-block acceptance stay tracked in `docs/gui-shell-acceptance-mvp9r.md`.

### Block 136 construction and workplane editing

```bash
cmake --build --preset dev-gui --target blcad_core_tests blcad_geometry_tests blcad_gui_tests blcad_app -j4
./build/dev-gui/blcad_core_tests "[core][construction-update]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-construction-edit],[block136-construction],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-construction-edit QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-construction-edit],[block136-construction]"
QT_QPA_PLATFORM=xcb timeout --signal=TERM 6s ./build/dev-gui/blcad
```

Core tests check identity/order preservation, reference rebinds, downstream invalidation, canonical
JSON and atomic rejection of missing references and cycles. GUI regressions reopen all nine modes
through browser double-click, preserve precision/dependencies, reject degenerate downstream geometry,
recompute dependent solids, exercise exact history/persistence and cancel stale tasks. The combined
UI scenario also covers pending numeric text, cyclic-reference correction, three-step history and
STEP via actual widget/dialog input. Run the combined native filter above and full suites for broad
regression evidence. Startup timeout exit 124 is expected; it is an intentional smoke-test stop.

### Block 136 generated construction references and parallel relations

```bash
cmake --build --preset dev-gui --target blcad_core_tests blcad_geometry_tests blcad_gui_tests blcad_app -j4
./build/dev-gui/blcad_core_tests "[core][construction-generated]"
./build/dev-gui/blcad_geometry_tests "[geometry][construction-generated]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-construction-relations],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-construction-relations QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-construction-relations]"
```

Core/Geometry regressions check generated source validation, dependency-ready loading without point
reordering, evaluated three-point and parallel plane frames, upstream parameter changes and evaluated
degeneracy. Shell regressions create/rebind all five added modes, consume generated points in existing
line/plane/axis and Sketch/Extrude workflows, reject missing/degenerate/cyclic inputs, recover/cancel,
and preserve exact history and canonical persistence. The seventh combined UI workflow uses actual
widget input through construction, downstream modeling, reference edits, three-step history, file
roundtrip and STEP, including horizontal task fit with long type labels. Native selectors and
captures use the existing 1280×800 harness; no new native-only
skips are introduced. Native reference collection/manipulators still keep Block 136 open.

### Mixed-path Loft and Sweep verification

```bash
cmake --build --preset dev-gui --target blcad_geometry_tests blcad_gui_tests blcad_app -j4
./build/dev-gui/blcad_geometry_tests "[mixed-path-loft]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[shell-mixed-path-loft],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-mixed-loft QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[shell-mixed-path-loft]"
```

Geometry checks three/five unequal sections on differently oriented/rolled planes along line,
32-span cubic spline and true circular-arc controls. Independent station/boundary assertions cover
closed/open Loft, LoftSurface, guide control and Sweep, plus recompute/persistence and failed edits.
The shell case imports only preauthored Sketch/plane inputs through Open, then uses actual Qt input
for path collection/reversal, five-section ordering, invalid/recovered previews, Loft Apply, middle
width property editing, three-step history, canonical reopen and STEP. This imported-input boundary
is separate from the eight fully UI-authored `[shell-combined-workflow]` scenarios. Add the distinct
`[shell-mixed-path-loft]` tag to the combined native regression above; both backends run it without
new skips. Run native GUI suites and the application startup smoke sequentially so a second window
cannot disturb popup focus. Contracts and current limits: `docs/part-guided-loft-geometry-mvp6.md` and
`docs/gui-interactive-path-sweep-loft-mvp9.md`. Full-suite evidence lives in
`docs/gui-shell-acceptance-mvp9r.md`; Block 136 remains open.

### Native construction reference collection

```bash
cmake --build --preset dev-gui --target blcad_core_tests blcad_geometry_tests blcad_gui_tests blcad_app -j4
./build/dev-gui/blcad_geometry_tests "[geometry][construction-picking]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-construction-picking],[block136-construction],[block136-construction-edit],[block136-construction-relations],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-construction-picking QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-construction-picking]"
```

Geometry checks the typed 26-source catalogue, bounded face area and upstream dimension changes.
Shell tests collect/revisit reference roles, reject/recover repeated defining points, edit generated
sources, preserve history/reopen and restore custom masks/scopes after Apply, Escape, close, Save and
revision changes. Hidden-Body and explicit numeric definitions cannot receive unintended topology
picks. The native case clicks actual model locations for three defining points, a parallel edge,
vertex point and face plane, then checks four-step history and canonical reopen at 1280×800; only
that case skips offscreen. Inspect captures and run the expanded combined native filter above, then
the startup smoke sequentially. The source-feature geometry contract and remaining construction
manipulators are documented in `docs/gui-sketch-workbench-mvp7.md`; Block 136 remains open.

### Construction position, direction/frame and new-offset grips

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app blcad_core_tests blcad_geometry_tests -j4
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-construction-manipulators],[shell-combined-manipulators],[construction-manipulator-camera],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-construction-manipulators QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-construction-manipulators],[shell-combined-manipulators],[construction-manipulator-camera]"
```

The same Qt mouse/key cases execute on both backends: XYZ position, composed direction/frame
rotations with full-precision persistence, new-offset normal/bindings, numeric override, final release,
Escape/focus/camera rollback, dependent-degeneracy recovery and capture cleanup on task expiry. The
eighth combined UI workflow adds rotated-plane Sketch/Extrude, downstream preview on same-ID plane
movement, exact history, canonical reopen and STEP. Native tests preserve the fitted OCCT zoom factor;
headless tests restore a useful model-span camera for their mapping. Include the new tag in the
combined native filter above and run startup separately. Contracts and remaining acceptance:
`docs/gui-sketch-workbench-mvp7.md`, `docs/gui-shell-acceptance-mvp9r.md`.


## Explicit reference-repair checks

```bash
./build/dev-gui/blcad_core_tests "[core][sketch-reference-repair]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-reference-repair],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-reference-repair QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-reference-repair]"
```

Run the native application smoke separately from GUI tests to preserve popup focus. The input case
opens authored fixtures, repairs point/line sources using actual ribbon/popup/button input, cancels
another preview, and checks history plus save/reopen. Initial-load failures, repair batches and
constraint-diagnostic suggestions remain open; see `docs/gui-sketch-workbench-mvp7.md` and the
Block-136 coverage manifest. The manifest continues to report the family as partial.


## Reference-repair list checks

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-reference-repair-batches],[block136-reference-repair],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-repair-batches QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-reference-repair-batches],[block136-reference-repair]"
```

The batch input workflow repairs two lost planes and their projected inputs through actual ribbon,
popup, list and button events. It exercises a failed partial preview, five queued changes,
remove/restage, whole-list preview/Apply, one-step history and canonical save/reopen. Retained
single-reference proofs run alongside it. Run native tests and the application startup smoke
serially. Repair-time loading and constraint-diagnostic controls remain the next documented work.


## Repair-time opening checks

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[repair-open-session],[block136-repair-open],[block136-reference-repair-batches],[block136-reference-repair],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-repair-open QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-repair-open]"
```

The new session cases verify admission, validation, cache/freshness guards, repair transactions,
failed-state history and canonical persistence. The native input case opens a failed-recompute Part
through the real file dialog, repairs two plane-driven solids, checks Preview/Fit/Apply and Undo/Redo,
then saves and reopens a fresh Part through file controls. Run native tests and startup serially.
The Core-invalid missing-source loading boundary remains separate planned work.

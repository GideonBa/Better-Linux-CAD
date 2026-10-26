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
- OpenSSL Crypto (SHA-256 for external STEP source freshness)

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
  qt6-base-dev libeigen3-dev libtbb-dev nlohmann-json3-dev libfmt-dev libspdlog-dev catch2 libssl-dev \
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
`ShellWindow`; Block 136 restores camera navigation through `[block136-navigation]` and
`[shell-combined-navigation]`. Context/filter/repeat presentation remains open.

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
implemented; Block 136 restores full bounded feature coverage and is accepted with full offscreen,
native Qt/OCCT and closure checks. Block 137 adds verified STEP source intent, freshness and shell
management. All Blocks through 137 are complete; Block 138 (STEP/XDE reader) is next. Source contracts
and acceptance evidence live in `docs/step-import-sequence-mvp10.md`; the completed shell milestone
lives in `docs/gui-shell-acceptance-mvp9r.md`. Dated proof sections below retain their original
incremental status.

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

`tests/gui/gui_shell_combined_workflow_tests.cpp` combines long interaction sequences in thirteen
scenarios, twelve with fully UI-authored inputs and one with an imported seated Part definition:
machining/edit/pattern/transform/Measure/STEP, Boolean/scale/Mirror with history branching,
multi-workplane Loft section ordering/editing, authored-Part import/Assembly placement/Mate/solve/
analysis/STEP, invalid/cancelled tasks plus dirty-file and document-switch recovery, and workplane
editing with cyclic-reference recovery and downstream solid recompute, plus generated-reference
construction chains feeding a second Sketch/Extrude and later upstream edits, and construction grips
rotating a workplane and moving its downstream solid, plus a typed Sketch3D chain feeding a
PathCurve/Sweep with endpoint editing and four-step history. Three further scenarios sew authored
solid faces into a shell/solid/Boolean chain, edit a revolved source through circular pattern and
rotation/scale consumers, and import the retained seated-Part example to author/drive all three
coordinates of a bounded Planar joint with branching motion history, analysis and Assembly exports.
The latter does not claim missing dedicated Hole-authoring coverage. A thirteenth workflow copies
UI-authored planar endpoints into Sketch3D and drives a PathCurve/Sweep through rejected/corrected
edits, history, canonical reopening and STEP export.

The shared harness operates Qt ribbon buttons and popup menus, task widgets, browser clicks and
double-clicks, viewport Sketch picks/hover, Escape and absolute/relative numeric coordinates, and
actual Qt input/file/confirmation dialogs. Native combo selection establishes and confirms native
window focus, synchronizes the desktop pointer and window-system events before opening/hovering,
awaits a visible popup with a two-second deadline, then checks the hit-tested row and its actual
highlight before the click. This preserves mouse coverage without direct index
assignment or timing sleeps. It does not seed Core intent or invoke authoring
controllers/workbenches. Assertions observe canonical
intent, resolved volumes/solid counts/bounds, source/result identities, task/preview/filter cleanup,
multi-step Undo/Redo and redo-branch truncation. Save/Open/Save compares complete bytes, including GUI
sidecars; exports preserve source bytes. Dialog automation has bounded watchdogs, and native input
checks widget reachability at 1280×800. Both backends execute all thirteen scenarios; these tests introduce
no native-only skips. The native run is required to prove actual OCCT presentation.

These bounded combined workflows extend the shell evidence. Remaining families, manipulator/input
variants and whole-block acceptance stay tracked in `docs/gui-shell-acceptance-mvp9r.md`.

The three extended cross-family workflows can also be run on their own:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[shell-combined-surface-solid],[shell-combined-revolve-pattern],[shell-combined-planar-motion]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-complex-workflows QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[shell-combined-surface-solid],[shell-combined-revolve-pattern],[shell-combined-planar-motion]"
```

The input harness edits numeric widgets and joint-limit table cells through keyboard events and
selects actual popup/list rows. These tests reject open-shell conversion, recover invalid pattern
counts and joint limits, compare every committed history checkpoint, and verify canonical file bytes
and read-only consumer/export behavior. Run native GUI tests serially with other GUI suites and
application startup.

The Surface/Boolean chain also guards the shared Body task's field geometry after a transform.
Wrapping guidance stays outside its conditional form rows: the reference Qt 6.4.2
[height-for-width pass](https://github.com/qt/qtbase/blob/v6.4.2/src/widgets/kernel/qformlayout.cpp#L479)
can otherwise process stale indices of hidden rows. The test must reach the actual result-mode
popup at 1280×800. Keep this input proof when changing the task layout.

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
is separate from the nine fully UI-authored `[shell-combined-workflow]` scenarios. Add the distinct
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
The bounded missing-source loading extension is verified separately below.


## Missing-source recovery checks

```bash
./build/dev-gui/blcad_core_tests "[missing-reference-recovery]"
./build/dev-gui/blcad_geometry_tests "[missing-reference-recovery]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[missing-reference-session],[block136-missing-reference],[repair-open-session],[block136-repair-open],[block136-reference-repair-batches],[block136-reference-repair],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-missing-reference QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[missing-reference-session],[block136-missing-reference]"
```

The fixture removes one producer and both construction source records, leaving six typed references,
constraints, a generated line and two plane-driven solids. Checks cover strict ordinary loading,
bounded admission, unchanged canonical failed intent, wrong-kind/cyclic/invalid rejection, partial
staging/Preview failures, complete atomic repair, cache protection, failed-state history and saving.
The shell input case uses actual file dialogs, popup/list/buttons, Fit, Undo/Redo and canonical
Save/Open. Run native tests and application startup serially. Constraint-diagnostic suggestion
controls remain the next work; Block 136 remains in progress.


## Explicit Sketch diagnostic repair checks

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-sketch-diagnostics],[gui][sketch-repair],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-sketch-diagnostics QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-sketch-diagnostics]"
```

The fixture combines a solid, raw lines/spline, all seven Core diagnostic kinds, supported and
manual suggestions, three Sketches and accepted constraint/dimension sidecars. Tests exercise
read-only inspection, all three supported repairs, catalog preservation, unsupported actions and
identity collisions, preview invalidation, cancellation/Save/history/context guards and restoration
of an active Sketch. The native input case uses actual model/Sketch ribbon buttons, diagnostic-list
clicks, Preview/Cancel/Apply, Fit, Undo/Redo and Save, with canonical reopening. Run native tests and
application startup serially. The subsequent typed Sketch3D slice is documented below; see
`docs/mvp-plan.md` for Block 136 status.


## Typed Sketch3D point/line checks

```bash
./build/dev-gui/blcad_core_tests "[core][sketch-3d-update]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-sketch3d],[shell-combined-sketch3d],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-sketch3d QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-sketch3d],[shell-combined-sketch3d]"
```

Core checks stable order/dependencies, canonical roundtrip and atomic rejection of missing sources,
removed segments and disconnected junctions. Shell checks typed staging, shared endpoints, retained
curved/guide intent and precision, explicit binding detachment, invalid geometry, history and task
expiry. The ninth fully UI-authored combined workflow builds a four-point chain, PathCurve and Sweep,
rejects collapse, extends the swept solid, exercises four-step history, cancels, reopens and exports
STEP. Both backends run all new scenarios without skips. Include both new tags in the combined native
regression above; run application startup separately. Contract and remaining native Sketch3D gaps:
`docs/gui-sketch3d-interaction-mvp8.md`. Block 136 remains in progress.

## Sketch3D viewport collection and direct point grips

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-sketch3d-grips],[shell-sketch3d-grips-workflow],[block136-sketch3d],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-sketch3d-grips QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-sketch3d-grips],[shell-sketch3d-grips-workflow]"
```

The same Qt input cases execute on both backends: candidate point/line roles, all seven grip-direction
modes, precise parameter detachment, raw final release, numeric override, Escape/camera/size/focus
rollback, invalid/recovered downstream candidates and stale task cleanup. The combined Sweep workflow
now picks the endpoint and drives a locked Z grip through rejected collapse and corrected solid
recompute, followed by exact history, canonical reopening and STEP. Include both new tags in the
combined native filter above, inspect the 1280×800 capture and run application startup serially.
Offscreen camera mapping uses a model span; native retains the fitted OCCT zoom factor. Contract,
bounded projection-copy semantics and block status:
`docs/gui-sketch3d-interaction-mvp8.md`, `docs/mvp-plan.md`.

## Anchored Sketch3D pointer placement

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app -j6
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-sketch3d-placement],[shell-sketch3d-placement-workflow],[block136-sketch3d-grips],[block136-sketch3d],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-sketch3d-placement QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-sketch3d-placement],[shell-sketch3d-placement-workflow]"
```

The same input cases cover all seven locks in orthographic and perspective views, copied bound and
local anchors, hover versus final-release staging, preserved earlier previews, pending spin text,
unobservable directions, Escape/camera/resize/focus/control changes and save/revision/document expiry.
The combined workflow authors a profile and spatial chain through actual ribbon, popup, viewport,
numeric and file input, then checks PathCurve/Sweep recompute, rejection/recovery, history and STEP.
Native pixel quantization is bounded using projected model axes; typed refinement keeps downstream
dimensions exact. Native Sketch3D fixtures await actual window activation through a conditional
event loop with a two-second deadline, retaining the active-window assertion before pointer input.
Inspect `sketch3d-point-placement-hover.png` at 1280×800. Include the two new tags in
the canonical native filter in `docs/gui-shell-acceptance-mvp9r.md` and run startup separately.
Pointer depth/anchor/copy semantics and the planar-copy contract:
`docs/gui-sketch3d-interaction-mvp8.md`, `docs/mvp-plan.md`.

## Sketch3D distance/direction checks

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app -j6
./build/dev-gui/blcad_core_tests "[gui][sketch-3d-edit],[integration][sketch-3d-direct-manipulation],[core][sketch-3d-update]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-sketch3d-direction],[shell-sketch3d-direction-workflow],[block136-sketch3d-placement],[block136-sketch3d-grips],[block136-sketch3d],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-sketch3d-direction QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-sketch3d-direction],[shell-sketch3d-direction-workflow]"
```

The new cases cover precise copied bound/local anchors, tuple conventions, numeric/text/coordinate
bounds, invalid/corrected candidates, competing authoring, live-gesture rollback, pending XYZ,
history, persistence and task expiry. The combined input workflow mixes XYZ, pointer and direction
placement, rejects an incomplete tuple, then authors PathCurve/Sweep and retains downstream grip
recovery, four-step history, canonical reopening and STEP export. Include the two new tags in the
canonical native regression in `docs/gui-shell-acceptance-mvp9r.md`. Inspect
`sketch3d-distance-direction-input.png` at 1280×800 and run startup separately, with builds and GUI
executions serial. The planar-point copy extension is documented below; Block 136 stays open.

## Explicit planar-point copying into Sketch3D

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app -j6
./build/dev-gui/blcad_core_tests "[gui][sketch-3d-edit],[integration][sketch-3d-direct-manipulation],[core][sketch-3d-update]"
./build/dev-gui/blcad_geometry_tests "[geometry][sketch-3d],[geometry][sketch-reference],[geometry][workplane]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-sketch3d-projection],[shell-combined-projected-sweep],[block136-sketch3d-direction],[block136-sketch3d-placement],[block136-sketch3d-grips],[block136-sketch3d],[shell-combined-sketch3d],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-sketch3d-projection QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-sketch3d-projection],[shell-combined-projected-sweep]"
```

The same focused cases check four plane orientations, projected construction points, cached
generated-face/semantic-vertex sources, copied precision, stable identities, per-copy consent,
pending-input guards, gesture rollback, bounds, independence after source editing and canonical
history/persistence/expiry. The thirteenth combined workflow authors both planar sources through
actual Sketch input, copies endpoints and adds a direction point, then checks PathCurve/Sweep,
failed/corrected edits, four-step history, canonical reopening and STEP. Its source rectangle hover
stays beyond the origin-axis snap tolerance and source rows use observed stable entity identities.
Inspect `sketch3d-planar-copy-controls.png` and `sketch3d-planar-copy-provenance.png` at 1280×800.
Include both new tags in the canonical native filter in `docs/gui-shell-acceptance-mvp9r.md`.
Builds, GUI runs and the separate startup smoke remain serial. Numeric/target/copy contracts live
in `docs/gui-sketch3d-interaction-mvp8.md`; Block 136 and remaining work live in `docs/mvp-plan.md`.

## Standalone Part parameter management

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app blcad_core_tests blcad_geometry_tests -j6
./build/dev-gui/blcad_core_tests "[core][parameter],[core][parameter-expression]"
./build/dev-gui/blcad_geometry_tests "[geometry][parameter-expression]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-parameters],[shell-combined-parameters],[gui][shell][property-editor],[sketch-dimension-workflow],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-parameters QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-parameters],[shell-combined-parameters]"
```

Core checks later-input formula loading, re-derived values/edges, authored order and strict rejection
of missing names/cycles/invalid units in both loading modes. Shell cases cover all literal types,
formula chains, full precision, global graph-ID collisions, pending input, duplicates/type/unit/cycle
rejection, complete downstream geometry, correction, one-step batch history and session expiry.
The fourteenth combined workflow authors a Sketch/Extrude and parameter batch through actual ribbon,
popup, field, button, browser and file input, rebinds the extrusion to a formula, checks 2400→3600→9600
mm³ volumes, recovers from invalid formulas/count text, exercises three-step Undo/Redo, cancels local
changes, reopens canonically and exports STEP. Thirteen combined workflows have fully UI-authored
inputs; Planar motion retains its imported seated Part boundary. Inspect
`parameter-manager-preview.png` and `parameter-manager-usage.png` at 1280×800. Include both new tags
in the canonical native regression in `docs/gui-shell-acceptance-mvp9r.md`, then run startup separately.
Contracts and remaining work: `docs/parameter-model.md`, `docs/mvp-plan.md`.

## Open-line Thin Extrude authoring

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app -j6
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-thin-extrude],[shell-combined-thin-extrude],[block136-extrude-extents],[block136-path-extrude],[gui][extrude-revolve],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-thin-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[native-projection-precision],[block136-thin-extrude],[shell-combined-thin-extrude]"
```

The focused cases cover all three wall modes and signed bounds on XY/XZ/YZ, every Body operation,
all seven extents, diagonal/reversed lines on a tilted offset plane, invalid text/recovery,
independent thickness/depth rollback, semantic selection,
pointer Escape in an oblique view with distinct handle projections, bounded input rejection,
global identity collisions, Cancel, document replacement
and canonical reopening. The nineteenth combined workflow authors a plate and two single-line
Sketches through actual UI input, cuts a mid-plane groove and a one-sided through slot, creates a
separate two-sided symmetric wall and edits an earlier thickness through the parameter manager.
It edits the earlier feature's name while retaining thin intent and checks downstream volumes,
exact five-step history, cancellation, canonical reopening and STEP.
Eighteen combined workflows have fully UI-authored inputs; Planar motion retains its imported Part.

Inspect `thin-midplane-cut-preview.png`, `thin-two-sided-new-body-preview.png` and
`thin-through-all-cut-preview.png` at 1280×800. Include both tags in the canonical native command in
`docs/gui-shell-acceptance-mvp9r.md`, then run startup separately. Keep builds, GUI runs and startup
serial. The one-line boundary and remaining native limit collection are recorded in
`docs/gui-interactive-extrude-revolve-mvp9.md` and `docs/mvp-plan.md`.

## Path-directed Extrude authoring

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app -j6
./build/dev-gui/blcad_core_tests "[core][path-extrude]"
./build/dev-gui/blcad_geometry_tests "[geometry][path-extrude]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-path-extrude],[shell-combined-path-extrude],[block136-extrude-extents],[gui][extrude-revolve],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-path-extrude-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-path-extrude],[shell-combined-path-extrude]"
```

The focused shell cases cover all four Body operations on XY/XZ/YZ, a line/spline/reversed-arc
trajectory, path/straight switching, unused input suppression, stale handles, missing selection,
excluded closed paths, incompatible orientation with recovery, Cancel, document replacement and
canonical reopening. The eighteenth combined workflow authors a plate, separate pocket/tool
profiles and a three-line shared-point Sketch3D path through actual UI
input. It applies a pocket and path Cut, edits the earlier pocket through the downstream route,
rejects a coincident endpoint and corrects it, checks exact four-step history, cancels a switched
draft, reopens and exports STEP. Seventeen combined workflows have fully UI-authored inputs;
Planar motion retains its imported Part boundary.

Inspect `path-extrude-cut-preview.png`, `path-extrude-downstream-edit-preview.png` and
`path-extrude-return-to-normal-preview.png` at 1280×800. Include both tags in the canonical native
regression in `docs/gui-shell-acceptance-mvp9r.md`, then run native startup separately. Keep builds,
GUI runs and startup serial. Contract and remaining work:
`docs/gui-interactive-extrude-revolve-mvp9.md`, `docs/mvp-plan.md`.

## Dedicated through-all hole authoring

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app -j6
./build/dev-gui/blcad_core_tests "*hole*,[core][extrude-extent]"
./build/dev-gui/blcad_geometry_tests "*circular*,[geometry][extrude-extent],[geometry][workplane]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-holes],[shell-combined-holes],[gui][part-foundation],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-holes QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-holes],[shell-combined-holes]"
```

The same new shell cases execute on both backends: single/circular creation; XY/XZ/YZ and tilted
construction planes; generated-face bounds; global ID/name collisions; literal/formula bindings,
typed/count limits and preserved local text; no-op/erased-solid rejection; whole-model previews,
atomic history, sidecars, canonical reopening and session expiry. The fifteenth combined workflow
uses actual ribbon, popup, field, browser, button and file input to author an 80×60×12 plate, Fillet,
four-hole pattern, parameter-driven six-hole recompute and an additional single hole sharing the
existing diameter. It exercises rejected/corrected inputs, browser workplane selection during the
task, four-step Undo/Redo, cancellation, canonical reopening and STEP. Fourteen combined workflows
have fully UI-authored inputs; Planar motion retains its imported seated Part boundary.

Inspect `hole-circle-preview.png` and `hole-single-bound-preview.png` at 1280×800. Include both new
tags in the canonical native regression in `docs/gui-shell-acceptance-mvp9r.md`, then run startup
separately, with builds and GUI executions serial. Contract and remaining extent-editor work:
`docs/gui-part-foundation-workbench-mvp7.md`, `docs/mvp-plan.md`. The semantic Hole Wizard is not
claimed by these plain cylindrical cut controls.

## Native Extrude limit-face collection regression

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app blcad_core_tests blcad_geometry_tests -j6
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-extrude-limit-picking],[shell-combined-extrude-limit-picking],[block136-extrude-extents],[block136-thin-extrude],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-limit-face-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-extrude-limit-picking],[shell-combined-extrude-limit-picking]"
```

The focused cases cover typed source identities, all three origin-plane normals, both limit roles,
transverse/coincident failure recovery, selection scope/highlights, Apply/Cancel/Escape, mode changes,
Undo and document replacement. The native case clicks actual Right/Top/Bottom source planes; it skips
offscreen. The combined Qt-input workflow authors a plate, stop Body, pocket, ToFace cut and independent
Between Thin wall; native runs click actual source-plane locations and offscreen uses the visible face
combos. Source-depth changes, an earlier-feature edit, exact history, canonical reopening and STEP
exercise the full chain. Inspect `extrude-native-both-limits.png`, `extrude-picked-to-face-cut.png` and
`extrude-picked-between-thin-wall.png` at 1280×800. Include both tags in the canonical native command in
`docs/gui-shell-acceptance-mvp9r.md`. Keep builds, GUI runs and separate native startup serial.

## ViewCube, Home and named-camera regression

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app blcad_core_tests blcad_geometry_tests -j6
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-navigation],[shell-combined-navigation],[gui][viewport],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-navigation-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-navigation],[shell-combined-navigation]"
```

Both backends execute all new cases. The focused tests compare independent standard camera axes,
full Home/bookmark restoration and projection state; guard Sketch/grips and preserve invalid previews,
reference scopes, history and document replacement. The combined Qt-input workflow covers named views,
Sketch re-entry, ViewCube popup input, real drag/Escape after camera restoration, cut/source-depth
edits, exact history, canonical reopening and STEP. Inspect `navigation-machining-preview.png` and
`navigation-bookmarks-after-history.png` at 1280×800. Include both tags in the canonical native command
in `docs/gui-shell-acceptance-mvp9r.md`; keep builds, GUI runs and separate native startup serial.

## Visible model selection-filter regression

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app blcad_core_tests blcad_geometry_tests -j6
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-selection-filters],[shell-combined-selection-filters],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-selection-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-selection-filters],[shell-combined-selection-filters]"
```

Both backends exercise the eight-filter semantic matrix including All/Vertex, browser fallback
rejection, task/Sketch/capture guards, task-owned Edge/Datum collection, history and document changes.
The combined workflow authors a plate, corrects invalid Fillet and Hole input under a Body filter,
changes the source depth, verifies exact history, re-enters Sketch, reopens canonically and exports
STEP. Native additionally clicks the actual Body through the filtered OCCT context. Inspect
`selection-filter-finishing-preview.png`, `selection-filter-hole-preview.png` and
`selection-filter-after-history.png` at 1280×800. Include both tags in the canonical native command in
`docs/gui-shell-acceptance-mvp9r.md`; keep builds, GUI runs and separate native startup serial.

## Last accepted modeling-command repeat regression

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app blcad_core_tests blcad_geometry_tests -j6
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-repeat],[shell-combined-repeat],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-repeat-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-repeat],[shell-combined-repeat]"
```

The focused cases distinguish accepted commands from failed/cancelled candidates, use a different
current profile and fresh Hole/Assembly defaults, guard tasks/Sketch/capture and check Save, exact
history and all document-replacement boundaries. The combined workflow authors two different holes,
rejects invalid placement, applies Fillet, uses Ctrl+R/Escape, edits upstream depth, verifies four-step
history, reopens canonically and exports STEP. Inspect `repeat-hole-preview.png` and
`repeat-after-history.png` at 1280×800. Include both tags in the canonical native regression in
`docs/gui-shell-acceptance-mvp9r.md`; keep builds, GUI runs and separate native startup serial.

## Bounded profile-context toolbar regression

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app blcad_core_tests blcad_geometry_tests -j6
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-profile-context],[shell-combined-profile-context],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-profile-context-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-profile-context],[shell-combined-profile-context]"
```

The focused cases validate materialized Core profiles and single-line Thin eligibility, reject hidden
or unmaterialized sources, consume/restore preselection across all cancellation routes, guard competing
interactions and check Save/history/replacement boundaries. The combined workflow uses actual toolbar
input to author a plate, cancel and correct a Cut, add a Thin wall on another plane, edit upstream
depth and an earlier feature, verify five-step history, reopen canonically and export STEP. Native
additionally clicks the actual profile and checks toolbar bounds. Inspect `context-cut-preview.png`,
`context-thin-preview.png` and `context-after-history.png` at 1280×800. Include both tags in the canonical
native command in `docs/gui-shell-acceptance-mvp9r.md`; keep builds, GUI runs and startup serial.

## Sweep/Loft profile-context regression

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app blcad_core_tests blcad_geometry_tests -j6
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-spatial-context],[shell-combined-spatial-context],[block136-profile-context],[block136-spatial],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-spatial-context-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-spatial-context],[shell-combined-spatial-context],[block136-profile-context]"
```

The five new focused cases exercise remaining-input eligibility, exact nondefault Sweep/Loft source
handoff, all three cancellation paths with actual viewport/session selection and restored filter/scope,
raw-region exclusion, duplicate rejection, three-section atomic Apply, Save and replacement. The combined workflow authors
all three Sketch sections, two offset planes and its trajectory through Qt, cancels an invalid Sweep,
corrects Loft order and G2 input, creates a separate Sweep, edits its twist parameter and the earlier
Loft, verifies four-step history, reopens canonically and exports STEP. Native additionally picks the
actual profile and verifies toolbar bounds. Inspect `spatial-context-loft-preview.png`,
`spatial-context-sweep-preview.png` and `spatial-context-after-history.png` at 1280×800. Include both
new tags in the canonical native command in `docs/gui-shell-acceptance-mvp9r.md`. Keep builds, GUI
runs and separate startup serial.

## Stable Edge/Face finishing-context regression

After building the four targets above, run builds, GUI suites and native startup serially:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-finishing-context],[shell-combined-finishing-context],[block136-profile-context],[block136-spatial-context],[block136-finishing],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-finishing-context-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-finishing-context],[shell-combined-finishing-context],[block136-profile-context],[block136-spatial-context],[block136-finishing]"
```

The five focused cases exercise actual shared context buttons, all four finishing commands,
nondefault Body seeding, complete selection/filter/scope restoration, every cancellation route,
empty-target rejection, atomic history, stale/hidden sources, Save and replacement. Explicit Edges or
Faces filters publish idle stable subshape targets; All retains its existing Body/profile picking.
The twenty-sixth combined workflow authors both Bodies and all modeling intent through Qt. It uses
actual native idle OCCT Edge/Face hits, cancels Draft and two-distance Chamfer, applies Shell/Fillet/
angular Chamfer, batches two upstream parameter edits, edits an earlier feature, checks five-step exact
history, reopens and exports STEP. It explicitly skips offscreen; the existing native finishing case
also skips there. Inspect `finishing-context-shell-preview.png`,
`finishing-context-chamfer-preview.png` and `finishing-context-after-history.png` at 1280×800 in the
capture directory. Include both new tags in the canonical native filter in
`docs/gui-shell-acceptance-mvp9r.md`. Block 136 remains open.

## Body pattern and Mirror context regression

Run builds, GUI suites and the native startup serially:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-pattern-context],[shell-combined-pattern-context],[block136-patterns],[block136-finishing-context],[block136-profile-context],[block136-selection-filters],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-pattern-context-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-pattern-context],[shell-combined-pattern-context],[block136-patterns]"
```

Five focused cases cover exactly one current Body, a nondefault cylinder, all three commands,
idempotent source seeding, task-owned Body/reference scopes, all cancellation routes, atomic Apply,
history, Save and replacement. The twenty-seventh combined Qt-input workflow cancels/applies Mirror,
creates Circular and Linear patterns, chains the reflected source, stages three upstream parameter
changes together, edits the earlier Mirror, checks five-step exact history, reopens and exports STEP.
Small rectangles first use a larger pointer preview before exact numeric refinement, beyond the
axis-inference tolerance. Offscreen source selection uses the browser; native input hits actual OCCT
Bodies. The existing
native pattern-handle case skips explicitly offscreen. Inspect four 1280×800 captures:
`pattern-context-mirror-preview.png`, `pattern-context-circular-preview.png`,
`pattern-context-linear-preview.png` and `pattern-context-after-history.png` in the capture directory.
Include both new tags in the canonical native filter in `docs/gui-shell-acceptance-mvp9r.md`.
Block 136 remains open.

## Replacement-shell Body contexts — 2026-10-25

After building the four targets above, run GUI suites serially with builds and native startup:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-body-context],[shell-combined-body-context],[block136-body],[block136-pattern-context],[block136-finishing-context],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-body-context-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-body-context],[shell-combined-body-context],[block136-body]"
```

Core and Geometry `[feature-order]` regressions cover interleaved producer graph/JSON restoration,
corrupt metadata, legacy compatibility and incremental geometry. Five focused cases verify typed
Boolean target/Transform source handoff, solid versus surface eligibility, idempotence, task-owned
role scopes, cancellation restoration, atomic Apply/history, Save and New/Open replacement. The twenty-eighth combined Qt-input workflow authors two solids,
cancels multi-Body translation, translates the nondefault target into overlap, switches Boolean
targets through the browser, cuts with retained tools, rotates using a picked axis and scales about
an explicit center. It checks unchanged previews, invalid-input recovery, bounds/volumes, exact
four-step history, canonical reopening and STEP. Native runs click actual OCCT Bodies; offscreen
uses browser selection. Inspect `body-context-translate-preview`, `body-context-boolean-preview`,
`body-context-rotate-preview` and `body-context-scale-preview` at 1280×800. Existing native Body
manipulator cases skip explicitly offscreen. Full-suite and native evidence live in
`docs/gui-shell-acceptance-mvp9r.md`. Surface/Assembly contexts remain next; Block 136 stays open.

## Replacement-shell Surface contexts — 2026-10-25

Build the four targets above before running GUI suites and native startup serially:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-surface-context],[shell-combined-surface-context],[block136-surface],[block136-body-context],[block136-finishing-context],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-surface-context-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-surface-context],[shell-combined-surface-context],[block136-surface]"
```

Six focused cases cover exact typed path/edge/surface handoff, all six commands, stable solid roles,
key/kind/scope guards, idempotence, cancellation selection/filter restoration, task-only subshape
cleanup, atomic Apply/history, Save and New/Open. The twenty-ninth combined Qt workflow authors its
box, closed PathCurves and trim profile through input events, cancels Boundary, applies Fill/Trim/
Extend, rejects an open shell, sews six surfaces and converts to solid. It checks duplicate/removal/
reordering controls, upstream depth edits, bounds/volumes, six-step exact history, canonical reopening
and STEP. Browser selections use the actual widgets on both platforms. Existing native Surface
regressions also hit actual surfaces/edges and drag/refine the extension handle; their offscreen
skip is explicit. Inspect `surface-context-fill-preview`, `surface-context-trim-preview`,
`surface-context-extend-preview`, `surface-context-stitch-preview` and `surface-context-solid-preview`
at 1280×800. Full evidence and remaining acceptance: `docs/gui-shell-acceptance-mvp9r.md`. Assembly
contexts remain next; Block 136 stays open.


## Final Assembly shell and acceptance proof — Block 136

Build all four GUI-preset targets before running these suites. Keep source edits, builds, GUI runs
and separate native startup serial. The Core/Geometry binaries may run alongside one GUI suite.

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app blcad_core_tests blcad_geometry_tests -j6
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-assembly-context],[block136-assembly-advanced],[block136-assembly-analysis],[shell-combined-assembly-context],[shell-combined-assembly-hierarchy],[block136-assembly],[block136-inspect-export],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-assembly-acceptance-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-assembly-context],[block136-assembly-advanced],[block136-assembly-analysis],[shell-combined-assembly-context],[shell-combined-assembly-hierarchy],[block136-assembly],[block136-inspect-export]"
```

The context cases exercise exact occurrence/target handoffs, all cancellation paths, guards, history
and Save during disposable active drafts. Advanced cases cover hierarchy validation, occurrence-qualified create/edit
and solve, all five joint families, four driven families and repeated definition authority. Analysis
cases cover local/cross Revolute samples, occurrence links, repeated clicks, read-only lifecycle,
preselection restoration and actual native framebuffer highlight/clear/reselect transitions.
Two new integrated Qt workflows combine these surfaces with real file
import, invalid candidates, branching history, canonical reopening and flat/structured STEP.

The canonical full/native command and final closure gate live in
`docs/gui-shell-acceptance-mvp9r.md`. Use its latest evidence section for actual results and omissions.

## STEP source identity and GUI proof — Block 137

Configure again after adding OpenSSL Crypto, then build the four targets serially before tests:

```bash
cmake --preset dev-gui
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app blcad_core_tests blcad_geometry_tests -j6
./build/dev-gui/blcad_core_tests "[step-import-source],[step-import-json],[step-import-freshness]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[step-import-workbench],[block137-step-import],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad137-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block137-step-import]"
```

The complex shell cases combine existing Sketch/Extrude modeling, explicit modes, preview invalidation,
external file changes, refresh, exact history, Save/Open and complete-folder relocation. They also
exercise missing/unreadable recovery, nested Project intent, source-owner isolation, native file
chooser acceptance/cancellation and 1280×800 task layout. Headless GUI checks cover stale/forged
evidence, no-op refresh, definition loading, Save As and document/sidecar/STEP overwrite protection.
Core checks cover canonical identity/JSON, SHA-256 known vectors, filesystem boundaries and stale
refresh evidence. These source-management fixtures do not claim STEP geometry reader coverage.

Run the full Core, Geometry and offscreen GUI suites and append `[block137-step-import]` to the
canonical native shell filter from `docs/gui-shell-acceptance-mvp9r.md`. Keep GUI runs and separate
native application startup serial; keep production/test sources frozen throughout completed runs.
Actual acceptance results are recorded in `docs/step-import-sequence-mvp10.md`.

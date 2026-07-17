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

The GUI requires Geometry.

```bash
cmake -S . -B build/dev-gui -G Ninja \
  -DBLCAD_BUILD_GEOMETRY=ON \
  -DBLCAD_BUILD_GUI=ON \
  -DBLCAD_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build/dev-gui --target blcad_app blcad_gui_tests
./build/dev-gui/blcad
```

The executable output is `blcad`; the CMake target is `blcad_app`.

Complete GUI tests:

```bash
QT_QPA_PLATFORM=offscreen ctest --test-dir build/dev-gui --output-on-failure
```

Only discovered GUI tests:

```bash
QT_QPA_PLATFORM=offscreen ctest --test-dir build/dev-gui -R '^gui\.' --output-on-failure
```

On Linux the native OCCT viewport uses Qt xcb when `DISPLAY` is available and
`QT_QPA_PLATFORM` is unset. Offscreen GUI tests use a logical viewport without X11/GLX.

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

Block 111 basic creation:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-create-basic]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][sketch-basic-profile]"
```

Block 112 conic and slot creation:

```bash
./build/dev/blcad_core_tests "[core][sketch-conics]"
./build/dev-geometry/blcad_geometry_tests "[geometry][sketch-conics]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-create-conics]"
```

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
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][sketch-expression-edit]"
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
```

Block 120 Interactive Sketch3D (headless Core proof):

```bash
./build/dev/blcad_core_tests "[gui][sketch-3d-edit]"
./build/dev/blcad_core_tests "[integration][sketch-3d-direct-manipulation]"
```

## Interactive Modeling focused proof

Block 122 selection-first modeling workspace, visible Qt shell, materialized profile handoff,
selection filters, repeat, and navigation aids:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][modeling-workspace]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][in-context-command]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][view-navigation-aids]"
```

These cases verify capability-exact enablement, deterministic mini-toolbar ordering, preselection
consumption and complete Cancel restoration, rejection of a non-materialized profile, synchronized
session/viewport filters, stable shell object names, ViewCube targets, Home, and camera bookmarks.

Block 123 reusable viewport manipulators and numeric coupling:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][viewport-manipulators]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][manipulator-numeric-coupling]"
```

These cases verify fixed-DIP presentation, deterministic hit tie breaking, model-space linear,
angular, and radial mapping, exact final release, typed-value precedence, translation/rotation triad
construction, PatternCount quantization, candidate-only behavior over a live Part session, and the
visible overlay/HUD shell.

Block 124 interactive Extrude, path Extrude, and Revolve authoring:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-extrude]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-revolve]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][extrude-revolve-manipulator]"
```

These cases verify extent-parameter driving with one atomic transaction and exact undo, operation/
taper/thin/through-all modes with their offered handles, fail-closed rejection of invalid inputs,
full/angle/symmetric revolve authoring, manipulator/typed document equivalence, and `MainWindow`
ownership of the interactive feature coordinator.

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

## Representative existing validation tags

```bash
./build/dev/blcad_core_tests "[core][part-body]"
./build/dev/blcad_core_tests "[core][parameter-expression]"
./build/dev/blcad_core_tests "[core][sketch-update]"
./build/dev-geometry/blcad_geometry_tests "[geometry][revolve-feature]"
./build/dev-geometry/blcad_geometry_tests "[geometry][sweep-feature]"
./build/dev-geometry/blcad_geometry_tests "[geometry][surface-stitch]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-structured-step-export]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][application-shell]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][document-session]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][viewport]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][model-browser]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][gui-feature-coverage]"
```

The exact test registration in `CMakeLists.txt` is authoritative. Block-122 and Block-123 cases remain
in the existing `blcad_gui_tests` executable; no second Qt application instance is introduced.

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

## Public Blocks 122–126 boundaries

```text
include/blcad/gui/gui_modeling_workspace.hpp
include/blcad/gui/gui_modeling_workspace_binder.hpp
include/blcad/gui/gui_viewport_manipulator.hpp
include/blcad/gui/gui_viewport_manipulator_binder.hpp
include/blcad/gui/gui_interactive_extrude_revolve.hpp
include/blcad/gui/gui_interactive_extrude_revolve_binder.hpp
include/blcad/gui/gui_interactive_finishing.hpp
include/blcad/gui/gui_interactive_pattern_body.hpp
include/blcad/gui/main_window.hpp
tests/gui/gui_feature_coverage_acceptance_tests.cpp
tests/gui/gui_interactive_features_tests.cpp
tests/gui/gui_interactive_finishing_tests.cpp
tests/gui/gui_interactive_pattern_body_tests.cpp
docs/gui-modeling-workspace-mvp9.md
docs/gui-viewport-manipulators-mvp9.md
docs/gui-interactive-extrude-revolve-mvp9.md
docs/gui-interactive-finishing-mvp9.md
docs/gui-interactive-pattern-body-mvp9.md
```

All Block-122 through Block-126 transient state — command catalogs, verified preselection
capabilities, mini-toolbar state, repeat, filters, Home/bookmarks, camera mappings, handle
descriptors, pointer measurements, hit products, HUD text, edge/face chains, pattern sources, tool
bodies, candidate feature parameters, and previews — does not change Core/Geometry or any save format.
A controller's Apply is the only mutation, and it flows through one existing `commit_part_transaction`.

## Formatting

Formatting is configured by `.editorconfig` and `.clang-format`.

```bash
clang-format -i \
  include/blcad/gui/gui_modeling_workspace.hpp \
  include/blcad/gui/gui_modeling_workspace_binder.hpp \
  include/blcad/gui/gui_viewport_manipulator.hpp \
  include/blcad/gui/gui_viewport_manipulator_binder.hpp \
  include/blcad/gui/gui_interactive_extrude_revolve.hpp \
  include/blcad/gui/gui_interactive_extrude_revolve_binder.hpp \
  include/blcad/gui/gui_interactive_finishing.hpp \
  include/blcad/gui/gui_interactive_pattern_body.hpp \
  include/blcad/gui/main_window.hpp \
  tests/gui/gui_feature_coverage_acceptance_tests.cpp \
  tests/gui/gui_interactive_features_tests.cpp \
  tests/gui/gui_interactive_finishing_tests.cpp \
  tests/gui/gui_interactive_pattern_body_tests.cpp
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
- `docs/sketch-planar-constraint-solver-mvp8.md`: solver mathematics and dimension mappings
- `docs/gui-sketch-solver-drag-mvp8.md`: constraint/dimension-aware direct manipulation
- `docs/gui-sketch-constraint-authoring-mvp8.md`: Block-114 constraint/glyph contract
- `docs/gui-sketch-dimension-authoring-mvp8.md`: Block-115 dimension/expression contract

## Current development boundary

Blocks 106–121 are implemented and accepted. Block 122 adds the shell-owned selection-first
Part/Surface/Assembly workspace. Block 123 adds reusable candidate-only viewport manipulators,
model-space mapping, deterministic fixed-DIP hit testing, exact release, and numeric-HUD coupling.
Block 124 interactive Extrude, path Extrude, and Revolve authoring is next.

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
- Qt 6 for the optional desktop and Interactive Sketcher
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

Blocks 106–113 are implemented.

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
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[core][sketch-conics]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[geometry][sketch-conics]"
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

The Block-113 proof covers connected-chain validation, control/fit handles, control polygons,
deterministic fit/control conversion, insertion/removal and stable ids, C0/G1 continuity, tangent
alignment, spline sampling/curvature, immutable preview, source freshness, one atomic document
transaction, exact undo/redo, expression-backed text substitution, sidecar JSON roundtrip, system font
fallback, and built-in vector-font fallback.

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

The exact test registration in `CMakeLists.txt` is authoritative.

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

## Public Block-113 boundaries

```text
include/blcad/core/sketch_spline_edit.hpp
include/blcad/core/sketch_text.hpp
include/blcad/core/sketch_text_json.hpp
include/blcad/geometry/sketch_spline_geometry.hpp
include/blcad/geometry/sketch_text_layout.hpp
include/blcad/gui/gui_sketch_spline.hpp
```

These boundaries are header-only deterministic services. Existing registered Core, Geometry, and GUI
test translation units exercise them without adding duplicate execution targets.

`SketchSplineEditModel` authoring state, spline samples, continuity diagnostics, resolved text, font
discovery, glyph strokes, and fallback selection are derived. Persistent products are existing cubic
SplineSegments/TangentContinuity and the explicit `blcad.sketch_text.mvp8` sidecar schema.

## Formatting

Formatting is configured by `.editorconfig` and `.clang-format`.

```bash
clang-format -i \
  include/blcad/core/sketch_spline_edit.hpp \
  include/blcad/core/sketch_text.hpp \
  include/blcad/core/sketch_text_json.hpp \
  include/blcad/geometry/sketch_spline_geometry.hpp \
  include/blcad/geometry/sketch_text_layout.hpp \
  include/blcad/gui/gui_sketch_spline.hpp \
  tests/core/sketch_spline_profile_json_tests.cpp \
  tests/geometry/spline_profile_pipeline_tests.cpp \
  tests/gui/gui_sketch_workbench_tests.cpp
```

## Clean generated files

```bash
rm -rf build/
```

## Documentation entry points

- `README.md`: repository entry point
- `docs/mvp-plan.md`: numbered status/current block
- `docs/architecture-summary.md`: condensed authority model
- `docs/file-format.md`: save-format authority
- `docs/interactive-sketcher-sequence-mvp8.md`: Blocks 106–121 sequence
- `docs/gui-sketch-spline-text-mvp8.md`: Block-113 spline/text contract

## Current development boundary

Blocks 106–113 are implemented. Block 114 is next: selection-driven manual constraints, accepted
automatic constraints, glyph interaction, and conflict preview through the existing Block-109 solver.

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

Blocks 106–115 are implemented.

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

The exact test registration in `CMakeLists.txt` is authoritative. Block-114 and Block-115 GUI cases are
included by `tests/gui/gui_test_main.cpp`; no second GUI test executable or Qt application instance is
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

## Public Block-115 boundaries

```text
include/blcad/core/sketch_dimension_authoring.hpp
include/blcad/core/sketch_dimension_authoring_json.hpp
include/blcad/core/sketch_dimension_catalog_system.hpp
include/blcad/geometry/sketch_dimension_glyph.hpp
include/blcad/gui/gui_sketch_dimensions.hpp
include/blcad/gui/gui_sketch_dimension_binder.hpp
include/blcad/gui/gui_document_session.hpp
include/blcad/gui/gui_sketch_drag.hpp
tests/core/sketch_dimension_tests.cpp
tests/geometry/spline_profile_pipeline_tests.cpp
tests/gui/gui_sketch_dimension_tests.inc
```

Persistent intent is the explicit `blcad.sketch_dimensions.mvp8` catalog. Measurements, calibrated
solver products, formatted values, anchors, hit-test products, and Qt dialog state are derived.

## Formatting

Formatting is configured by `.editorconfig` and `.clang-format`.

```bash
clang-format -i \
  include/blcad/core/sketch_dimension_authoring.hpp \
  include/blcad/core/sketch_dimension_authoring_json.hpp \
  include/blcad/core/sketch_dimension_catalog_system.hpp \
  include/blcad/geometry/sketch_dimension_glyph.hpp \
  include/blcad/gui/gui_document_session.hpp \
  include/blcad/gui/gui_sketch_dimensions.hpp \
  include/blcad/gui/gui_sketch_dimension_binder.hpp \
  include/blcad/gui/gui_sketch_drag.hpp \
  include/blcad/gui/gui_types.hpp \
  src/gui/gui_document_session.cpp \
  src/gui/gui_selection_model.cpp \
  src/gui/gui_sketch_interaction_binder.cpp \
  tests/core/sketch_dimension_tests.cpp \
  tests/geometry/spline_profile_pipeline_tests.cpp \
  tests/gui/gui_test_main.cpp \
  tests/gui/gui_sketch_dimension_tests.inc
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
- `docs/sketch-planar-constraint-solver-mvp8.md`: solver mathematics and dimension mappings
- `docs/gui-sketch-solver-drag-mvp8.md`: constraint/dimension-aware direct manipulation
- `docs/gui-sketch-constraint-authoring-mvp8.md`: Block-114 constraint/glyph contract
- `docs/gui-sketch-dimension-authoring-mvp8.md`: Block-115 dimension/expression contract

## Current development boundary

Blocks 106–115 are implemented. Block 116 is next: trim, extend, split, Sketch corner fillet, and
Sketch corner chamfer with explicit dependency remap or fail-closed behavior.

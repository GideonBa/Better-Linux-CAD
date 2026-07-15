# Development Setup

This document covers the current local BLCAD build, test, formatting, and executable workflows.
Feature status and numbered sequencing live in `docs/mvp-plan.md`; exact block contracts live in the
feature-specific documents under `docs/`.

## Target environment

- Ubuntu 24.04
- C++20
- CMake 3.28 or newer
- Ninja
- OCCT/Open CASCADE from Ubuntu packages for optional Geometry targets
- Qt 6 for the optional desktop and Interactive Sketcher
- nlohmann-json
- Catch2 3

The current assembly numeric engine uses project-owned dynamic containers plus deterministic dense
elimination routines. Eigen is installed by the reference dependency set but is not currently a
BLCAD target dependency.

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

## Configure, build, and test

Complete Core workflow:

```bash
cmake --workflow --preset dev-build-test
```

Equivalent commands:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Complete Geometry workflow:

```bash
cmake --workflow --preset dev-geometry-build-test
```

Equivalent commands:

```bash
cmake --preset dev-geometry
cmake --build --preset dev-geometry
ctest --preset dev-geometry
```

Current build directories are:

```text
build/dev
build/dev-geometry
build/dev-gui
build/release
```

## GUI build and interactive GUI tests

The GUI is optional and requires the Geometry layer.

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

The startup splash remains visible for 700 ms by default. For visual inspection its animation
length can be overridden, for example:

```bash
BLCAD_SPLASH_DURATION_MS=15000 ./build/dev-gui/blcad
```

Accepted splash durations are clamped to 250–60000 ms.

On Linux the native OCCT viewport uses Qt's xcb platform. `blcad` selects xcb automatically when
`DISPLAY` is available and `QT_QPA_PLATFORM` is unset. Offscreen GUI tests use a logical viewport
without creating an X11/GLX window.

`BLCAD_BUILD_GUI=ON` with `BLCAD_BUILD_GEOMETRY=OFF` is unsupported and fails during CMake
configuration. Core-only and Geometry-without-GUI configurations remain supported.

Run the complete GUI test target with:

```bash
QT_QPA_PLATFORM=offscreen ctest --test-dir build/dev-gui --output-on-failure
```

Or only the discovered GUI tests:

```bash
QT_QPA_PLATFORM=offscreen ctest --test-dir build/dev-gui -R '^gui\.' --output-on-failure
```

## Interactive Sketcher focused proof

Blocks 106–107 are implemented. The current Sketch workspace and command-lifecycle proof is:

```bash
./build/dev-gui/blcad_gui_tests "[gui][sketch-workspace]"
./build/dev-gui/blcad_gui_tests "[gui][sketch-command-lifecycle]"
./build/dev-gui/blcad_gui_tests "[gui][viewport][gui][navigation]"
```

Block 107 plane interaction, hit testing, grid, snapping, inference, and box selection are covered by:

```bash
./build/dev-gui/blcad_gui_tests "[gui][sketch-hit-test]"
./build/dev-gui/blcad_gui_tests "[gui][sketch-snap]"
./build/dev-gui/blcad_gui_tests "[gui][sketch-box-selection]"
```

For example, the Block-107 hit tests prove the frozen `Point -> Curve -> Dimension -> Glyph`
priority and deterministic repeated-click cycling. The snap tests prove device-independent mapping,
model-space-first snap selection with screen-space tie breaking, grid snap, and inference. The
box-selection tests prove left-to-right Window versus right-to-left Crossing semantics.

The current implementation handoff is Block 108. Its focused tags are:

```text
[core][sketch-topology]
[core][sketch-edit-command]
[core][sketch-json-migration]
```

## GUI validation and workbench tags

Useful existing focused GUI tags include:

```bash
./build/dev-gui/blcad_gui_tests "[gui][application-shell]"
./build/dev-gui/blcad_gui_tests "[gui][command-state]"
./build/dev-gui/blcad_gui_tests "[gui][startup-splash]"
./build/dev-gui/blcad_gui_tests "[gui][document-session]"
./build/dev-gui/blcad_gui_tests "[gui][document-transaction]"
./build/dev-gui/blcad_gui_tests "[gui][diagnostics]"
./build/dev-gui/blcad_gui_tests "[gui][viewport]"
./build/dev-gui/blcad_gui_tests "[gui][semantic-picking]"
./build/dev-gui/blcad_gui_tests "[gui][model-browser]"
./build/dev-gui/blcad_gui_tests "[gui][property-editor]"
./build/dev-gui/blcad_gui_tests "[gui][selection-sync]"
./build/dev-gui/blcad_gui_tests "[gui][datum-workplane]"
./build/dev-gui/blcad_gui_tests "[gui][sketch-workbench]"
./build/dev-gui/blcad_gui_tests "[gui][sketch-repair]"
./build/dev-gui/blcad_gui_tests "[gui][parameters]"
./build/dev-gui/blcad_gui_tests "[gui][part-foundation]"
./build/dev-gui/blcad_gui_tests "[gui][extrude-revolve]"
./build/dev-gui/blcad_gui_tests "[gui][part-pattern]"
./build/dev-gui/blcad_gui_tests "[gui][part-finishing]"
./build/dev-gui/blcad_gui_tests "[gui][body-operation]"
./build/dev-gui/blcad_gui_tests "[gui][path-workbench]"
./build/dev-gui/blcad_gui_tests "[gui][sweep-loft]"
./build/dev-gui/blcad_gui_tests "[gui][surface-workbench]"
./build/dev-gui/blcad_gui_tests "[gui][assembly-authoring]"
./build/dev-gui/blcad_gui_tests "[gui][assembly-relationships]"
./build/dev-gui/blcad_gui_tests "[gui][assembly-motion]"
./build/dev-gui/blcad_gui_tests "[gui][analysis]"
./build/dev-gui/blcad_gui_tests "[gui][step-export]"
./build/dev-gui/blcad_gui_tests "[integration][gui-feature-coverage]"
./build/dev-gui/blcad_gui_tests "[integration][gui-headless-equivalence]"
```

The exact source and test registration in `CMakeLists.txt` is authoritative.

## Core and Geometry focused test examples

Core model and persistence examples:

```bash
./build/dev/blcad_core_tests "[core][part-body]"
./build/dev/blcad_core_tests "[core][feature-body-operation-json]"
./build/dev/blcad_core_tests "[core][part-feature-input-reference]"
./build/dev/blcad_core_tests "[core][extrude-extent]"
./build/dev/blcad_core_tests "[core][revolve-feature]"
./build/dev/blcad_core_tests "[core][part-pattern]"
./build/dev/blcad_core_tests "[core][sketch-update]"
./build/dev/blcad_core_tests "[core][path-curve]"
./build/dev/blcad_core_tests "[core][loft-feature]"
./build/dev/blcad_core_tests "[core][surface-feature]"
./build/dev/blcad_core_tests "[integration][part-construction-mvp]"
```

Geometry execution examples:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][body-transform]"
./build/dev-geometry/blcad_geometry_tests "[geometry][extrude-extent]"
./build/dev-geometry/blcad_geometry_tests "[geometry][revolve-feature]"
./build/dev-geometry/blcad_geometry_tests "[geometry][linear-pattern]"
./build/dev-geometry/blcad_geometry_tests "[geometry][circular-pattern]"
./build/dev-geometry/blcad_geometry_tests "[geometry][sweep-feature]"
./build/dev-geometry/blcad_geometry_tests "[geometry][guided-loft]"
./build/dev-geometry/blcad_geometry_tests "[geometry][surface-boundary-fill]"
./build/dev-geometry/blcad_geometry_tests "[geometry][surface-stitch]"
./build/dev-geometry/blcad_geometry_tests "[geometry][multi-body-step-export]"
./build/dev-geometry/blcad_geometry_tests "[integration][part-construction-mvp]"
```

Assembly target, solve, motion, and exchange examples:

```bash
./build/dev/blcad_core_tests "[core][semantic-generated-topology-reference]"
./build/dev/blcad_core_tests "[core][semantic-generated-topology-recovery]"
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-graph]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-geometric-target-taxonomy]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generated-topology-target-resolution]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-target-compatibility]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-application]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-motion]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-contact]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-revolute-sweep]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-structured-step-export]"
```

## Headless tools

Core inspection:

```bash
./build/dev/blcad_inspect_project_components examples/component_instances.blcad.project.json
```

Part export examples:

```bash
./build/dev-geometry/blcad_export_step examples/reference_plate.blcad.json build/reference_plate.step
./build/dev-geometry/blcad_export_step examples/bolt_circle_plate.blcad.json build/bolt_circle_plate.step
```

Current command shapes:

```text
blcad_export_step <input.blcad.json> <output.step>
blcad_export_project <input.blcad.project.json> <assembly-parameter-id> <value> <output-dir>
blcad_inspect_project_components <input.blcad.project.json>
blcad_export_posed_assembly <input.blcad.project.json> <output.step>
blcad_export_structured_assembly <input.blcad.project.json> <output.step>
blcad_move_joint <input.blcad.project.json> <joint-id> <angle-deg> <output.blcad.project.json>
blcad_analyze_assembly <input.blcad.project.json> [clearance-threshold-mm]
```

The typed target/capability, generated-topology identity, and Sketch interaction layers are public
library contracts and intentionally add no CLI or persistent result format.

## Public versus private boundaries

Private Geometry headers remain private to `src/geometry`. Geometry tests have a private include
path to that directory so focused integration tests may verify residual/Jacobian ordering and shared
helper boundaries without promoting execution internals to the public API.

The Block-107 public GUI interaction boundary is:

```text
include/blcad/gui/gui_sketch_interaction.hpp
```

The shell integration boundary is:

```text
include/blcad/gui/gui_sketch_interaction_binder.hpp
```

`GuiSketchInteractionScene`, sampled curves, screen positions, hit stacks, grid lines, and snap
results are transient. Persistent Sketch intent remains in Core.

## Formatting

Formatting is configured by `.editorconfig` and `.clang-format`.

Format the Block-107 production and test files with:

```bash
clang-format -i \
  include/blcad/gui/gui_sketch_interaction.hpp \
  include/blcad/gui/gui_sketch_interaction_binder.hpp \
  include/blcad/gui/occt_viewport.hpp \
  src/gui/gui_sketch_interaction.cpp \
  src/gui/gui_sketch_interaction_binder.cpp \
  src/gui/occt_viewport.cpp \
  src/gui/main.cpp \
  tests/gui/gui_sketch_interaction_tests.cpp
```

When adding a feature block, register production and test sources in `CMakeLists.txt` and document
exact scope in the feature-specific canonical document plus `docs/mvp-plan.md`.

## Clean generated files

```bash
rm -rf build/
```

## Documentation entry points

- `README.md`: short repository entry point
- `docs/mvp-plan.md`: implementation sequence and current numbered block
- `docs/architecture-summary.md`: condensed implemented architecture
- `docs/file-format.md`: persistent save-format authority
- `docs/interactive-sketcher-sequence-mvp8.md`: Blocks 106–121 Interactive Sketcher sequence
- `docs/gui-interactive-sketch-workspace-mvp8.md`: Block-106 contextual workspace contract
- `docs/gui-sketch-plane-interaction-mvp8.md`: Block-107 mapping, hit, selection, grid, snap, and inference contract
- `docs/interactive-modeling-sequence-mvp9.md`: Blocks 122–131 interactive Part/Surface/Assembly plan
- `docs/step-import-sequence-mvp10.md`: Blocks 132–138 STEP import plan
- `docs/project-goal.md`: long-term direction

## Current development boundary

Blocks 1–107 are implemented according to the numbered sequence. Interactive Sketcher MVP-8 is in
progress. Block 108 is the immediate next technical step and owns shared planar point/entity
topology, dependency-safe editable Core commands, construction/reference flags, canonical ordering,
JSON migration from the existing planar Sketch format, and exact undo/redo semantics.

Interactive Modeling remains sequenced for Blocks 122–131. STEP Import remains sequenced for Blocks
132–138.

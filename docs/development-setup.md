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

The current Assembly numeric engine uses project-owned dynamic containers plus deterministic dense
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

Equivalent:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Complete Geometry workflow:

```bash
cmake --workflow --preset dev-geometry-build-test
```

Equivalent:

```bash
cmake --preset dev-geometry
cmake --build --preset dev-geometry
ctest --preset dev-geometry
```

Build directories:

```text
build/dev
build/dev-geometry
build/dev-gui
build/release
```

## GUI build and tests

The GUI is optional and requires Geometry.

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

The startup splash defaults to 700 ms. For visual inspection:

```bash
BLCAD_SPLASH_DURATION_MS=15000 ./build/dev-gui/blcad
```

Accepted values are clamped to 250–60000 ms.

On Linux the native OCCT viewport uses Qt's xcb platform. `blcad` selects xcb automatically when
`DISPLAY` is available and `QT_QPA_PLATFORM` is unset. Offscreen GUI tests use a logical viewport
without an X11/GLX window.

`BLCAD_BUILD_GUI=ON` with `BLCAD_BUILD_GEOMETRY=OFF` is unsupported and fails during configuration.
Core-only and Geometry-without-GUI configurations remain supported.

Complete GUI tests:

```bash
QT_QPA_PLATFORM=offscreen ctest --test-dir build/dev-gui --output-on-failure
```

Only discovered GUI tests:

```bash
QT_QPA_PLATFORM=offscreen ctest --test-dir build/dev-gui -R '^gui\.' --output-on-failure
```

## Interactive Sketcher focused proof

Blocks 106–108 are implemented.

Block 106 workspace and command lifecycle:

```bash
./build/dev-gui/blcad_gui_tests "[gui][sketch-workspace]"
./build/dev-gui/blcad_gui_tests "[gui][sketch-command-lifecycle]"
./build/dev-gui/blcad_gui_tests "[gui][viewport][gui][navigation]"
```

Block 107 plane interaction, hit testing, grid, snapping, inference, and box selection:

```bash
./build/dev-gui/blcad_gui_tests "[gui][sketch-hit-test]"
./build/dev-gui/blcad_gui_tests "[gui][sketch-snap]"
./build/dev-gui/blcad_gui_tests "[gui][sketch-box-selection]"
```

The Block-107 proof covers the frozen `Point -> Curve -> Dimension -> Glyph` hit priority,
deterministic repeated-click cycling, device-independent mapping, model-space-first snap selection
with screen-space tie breaking, grid snap/inference, and Window/Crossing selection.

Block 108 shared planar topology, edit commands, and JSON migration:

```bash
./build/dev/blcad_core_tests "[core][sketch-topology]"
./build/dev/blcad_core_tests "[core][sketch-edit-command]"
./build/dev/blcad_core_tests "[core][sketch-json-migration]"
```

These tests prove deterministic legacy migration independent of line insertion order, collapse of six
triangle endpoint usages into three shared `SketchPointId` records, explicit migration identity-change
reporting, ordered profile dependencies, shared-junction movement, dependency-safe removal, Add/
Replace/Remove candidate validation, exact full-topology snapshot undo/redo, migration from existing
PartDocument Sketch JSON, and exact `blcad.sketch_topology.mvp8` round-trip.

The current implementation handoff is Block 109. Its focused tags are:

```text
[core][sketch-solver]
[core][sketch-dof]
[core][sketch-conflict-diagnostics]
```

## Existing GUI validation tags

Useful focused tags include:

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

The exact source and test registration in `CMakeLists.txt` is authoritative. Block 108 adds public
header-only Core authorities and extends the already-registered `tests/core/sketch_tests.cpp`; no new
translation unit or CMake source registration is required.

## Core and Geometry focused examples

Core model and persistence:

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

Geometry execution:

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

Assembly target, solve, motion, and exchange:

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

Part export:

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

Block-108 topology and migration are public library APIs and intentionally add no CLI.

## Public versus private boundaries

Private Geometry headers remain private to `src/geometry`. Geometry tests have a private include path
to that directory so focused integration tests may verify residual/Jacobian ordering and shared helper
boundaries without promoting execution internals to the public API.

Block-107 public GUI interaction boundaries:

```text
include/blcad/gui/gui_sketch_interaction.hpp
include/blcad/gui/gui_sketch_interaction_binder.hpp
```

Block-108 public Core boundaries:

```text
include/blcad/core/sketch_topology.hpp
include/blcad/core/sketch_edit_commands.hpp
include/blcad/core/sketch_topology_json.hpp
include/blcad/core/sketch_topology_part_document.hpp
```

`SketchTopology` and `SketchPointId` are Core identity. `SketchTopologyMigrationReport` and legacy
materialization candidates are derived/adaptation products. `GuiSketchInteractionScene`, sampled
curves, screen positions, hit stacks, grid lines, and snap results remain transient.

## Formatting

Formatting is configured by `.editorconfig` and `.clang-format`.

Format Block-108 headers and focused proof with:

```bash
clang-format -i \
  include/blcad/core/id.hpp \
  include/blcad/core/sketch_topology.hpp \
  include/blcad/core/sketch_edit_commands.hpp \
  include/blcad/core/sketch_topology_json.hpp \
  include/blcad/core/sketch_topology_part_document.hpp \
  tests/core/sketch_tests.cpp
```

When adding a block, register new translation units/tests in `CMakeLists.txt` where required and
document exact scope in the feature-specific canonical document plus `docs/mvp-plan.md`.

## Clean generated files

```bash
rm -rf build/
```

## Documentation entry points

- `README.md`: short repository entry point
- `docs/mvp-plan.md`: implementation sequence and current numbered block
- `docs/architecture-summary.md`: condensed implemented architecture
- `docs/file-format.md`: historical PartDocument/Project save-format authority
- `docs/interactive-sketcher-sequence-mvp8.md`: Blocks 106–121 Interactive Sketcher sequence
- `docs/gui-interactive-sketch-workspace-mvp8.md`: Block-106 contextual workspace contract
- `docs/gui-sketch-plane-interaction-mvp8.md`: Block-107 plane interaction contract
- `docs/sketch-shared-topology-mvp8.md`: Block-108 shared topology, migration, edit, and topology JSON contract
- `docs/interactive-modeling-sequence-mvp9.md`: Blocks 122–131 interactive modeling plan
- `docs/step-import-sequence-mvp10.md`: Blocks 132–138 STEP import plan
- `docs/project-goal.md`: long-term direction

## Current development boundary

Blocks 1–108 are implemented according to the numbered sequence. Interactive Sketcher MVP-8 is in
progress. Block 109 is the immediate next technical step and owns the deterministic general planar
constraint solver over Block-108 topology: solver-variable ordering, scale normalization, tolerances,
iteration/damping limits, convergence classification, remaining-DOF accounting, and stable
redundant/conflicting/non-convergent/invalid-reference diagnostics.

Interactive Modeling remains sequenced for Blocks 122–131. STEP Import remains sequenced for Blocks
132–138.

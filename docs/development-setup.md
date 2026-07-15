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

The Assembly numeric engine and Block-109 planar Sketch solver use project-owned dynamic containers plus
deterministic dense elimination routines. Eigen is installed by the reference dependency set but is not
currently a BLCAD target dependency.

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
ctest --preset dev --output-on-failure
```

Complete Geometry workflow:

```bash
cmake --workflow --preset dev-geometry-build-test
```

Equivalent:

```bash
cmake --preset dev-geometry
cmake --build --preset dev-geometry
ctest --preset dev-geometry --output-on-failure
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

For splash inspection:

```bash
BLCAD_SPLASH_DURATION_MS=15000 ./build/dev-gui/blcad
```

Accepted splash values are clamped to 250–60000 ms.

On Linux the native OCCT viewport uses Qt xcb when `DISPLAY` is available and
`QT_QPA_PLATFORM` is unset. Offscreen GUI tests use a logical viewport without X11/GLX.

`BLCAD_BUILD_GUI=ON` with `BLCAD_BUILD_GEOMETRY=OFF` is unsupported. Core-only and
Geometry-without-GUI configurations remain supported.

Complete GUI tests:

```bash
QT_QPA_PLATFORM=offscreen ctest --test-dir build/dev-gui --output-on-failure
```

Only discovered GUI tests:

```bash
QT_QPA_PLATFORM=offscreen ctest --test-dir build/dev-gui -R '^gui\.' --output-on-failure
```

## Interactive Sketcher focused proof

Blocks 106–110 are implemented.

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

Block 108 shared planar topology, edit commands, and JSON migration:

```bash
./build/dev/blcad_core_tests "[core][sketch-topology]"
./build/dev/blcad_core_tests "[core][sketch-edit-command]"
./build/dev/blcad_core_tests "[core][sketch-json-migration]"
```

Block 109 deterministic planar solver, exact local DOF, and conflict diagnostics:

```bash
./build/dev/blcad_core_tests "[core][sketch-solver]"
./build/dev/blcad_core_tests "[core][sketch-dof]"
./build/dev/blcad_core_tests "[core][sketch-conflict-diagnostics]"
```

The Block-109 proof covers:

- canonical lexicographic constraint ordering;
- canonical non-reference `SketchPointId` variable ordering with X then Y;
- under-constrained remaining DOF from final Jacobian rank;
- fully constrained convergence and solved topology publication;
- every initial Block-109 residual family;
- deterministic canonical incremental redundancy attribution;
- stable remove-one conflict attribution;
- invalid-reference classification;
- deterministic non-convergence classification;
- adaptation of current persisted geometric constraints and parameter-backed dimensions.

Block 110 solver-backed semantic-handle drag and live solve:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-drag]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][sketch-live-solve]"
```

The proof covers stable handle order and shared-junction deduplication, latest-pointer coalescing, exact
release flush, source-document immutability during preview, cancel/refusal rollback, Arc center/radius
solver targets, one `Drag sketch handle` session history entry, exact undo/redo, and an offscreen Qt
Press/Move/Release path through the installed binder.

The current implementation handoff is Block 111. Its focused tags are:

```text
[gui][sketch-create-basic]
[integration][sketch-basic-profile]
```

## Existing GUI validation tags

Representative tags:

```bash
./build/dev-gui/blcad_gui_tests "[gui][application-shell]"
./build/dev-gui/blcad_gui_tests "[gui][command-state]"
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

The exact source/test registration in `CMakeLists.txt` is authoritative.

Block 109 registers:

```text
src/core/sketch_constraint_solver.cpp
src/core/sketch_solver_legacy_adapter.cpp
tests/core/sketch_constraint_solver_tests.cpp
```

## Core and Geometry focused examples

Core model/persistence:

```bash
./build/dev/blcad_core_tests "[core][part-body]"
./build/dev/blcad_core_tests "[core][feature-body-operation-json]"
./build/dev/blcad_core_tests "[core][part-feature-input-reference]"
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

Assembly target/solve/motion/exchange:

```bash
./build/dev/blcad_core_tests "[core][semantic-generated-topology-reference]"
./build/dev/blcad_core_tests "[core][semantic-generated-topology-recovery]"
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-graph]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-geometric-target-taxonomy]"
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

Block-108 topology and Block-109 solver are public library APIs and intentionally add no CLI.

## Public versus private boundaries

Private Geometry headers remain private to `src/geometry`. Geometry tests have a private include path
for focused residual/Jacobian/helper verification without promoting execution internals.

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

Block-109 public Core boundary:

```text
include/blcad/core/sketch_constraint_solver.hpp
```

Block-110 public GUI boundaries:

```text
include/blcad/gui/gui_sketch_drag.hpp
include/blcad/gui/gui_sketch_drag_binder.hpp
```

Registered Block-110 implementation/proof:

```text
src/gui/gui_sketch_drag.cpp
src/gui/gui_sketch_drag_binder.cpp
tests/gui/gui_sketch_drag_tests.cpp
```

Block-110 public GUI boundaries:

```text
include/blcad/gui/gui_sketch_drag.hpp
include/blcad/gui/gui_sketch_drag_binder.hpp
```

Registered Block-110 implementation/proof:

```text
src/gui/gui_sketch_drag.cpp
src/gui/gui_sketch_drag_binder.cpp
tests/gui/gui_sketch_drag_tests.cpp
```

Block-110 public GUI boundaries:

```text
include/blcad/gui/gui_sketch_drag.hpp
include/blcad/gui/gui_sketch_drag_binder.hpp
```

Registered Block-110 implementation/proof:

```text
src/gui/gui_sketch_drag.cpp
src/gui/gui_sketch_drag_binder.cpp
tests/gui/gui_sketch_drag_tests.cpp
```

`SketchTopology`/`SketchPointId` are persistent Core topology identity. `SketchConstraintSystem` is a
canonical solve request. `SketchSolveResult`, variable order, residual summary, Jacobian rank, remaining
DOF, and solver diagnostics are derived.

`GuiSketchInteractionScene`, sampled curves, screen positions, hit stacks, grid lines, and snap results
remain transient.

## Formatting

Formatting is configured by `.editorconfig` and `.clang-format`.

Format Blocks 108–109 Core changes with:

```bash
clang-format -i \
  include/blcad/core/id.hpp \
  include/blcad/core/sketch_topology.hpp \
  include/blcad/core/sketch_edit_commands.hpp \
  include/blcad/core/sketch_topology_json.hpp \
  include/blcad/core/sketch_topology_part_document.hpp \
  include/blcad/core/sketch_constraint_solver.hpp \
  src/core/sketch_constraint_solver.cpp \
  src/core/sketch_solver_legacy_adapter.cpp \
  include/blcad/gui/gui_sketch_drag.hpp \
  include/blcad/gui/gui_sketch_drag_binder.hpp \
  src/gui/gui_sketch_drag.cpp \
  src/gui/gui_sketch_drag_binder.cpp \
  tests/core/sketch_tests.cpp \
  tests/core/sketch_constraint_solver_tests.cpp \
  tests/gui/gui_sketch_drag_tests.cpp
```

When adding a block, register new translation units/tests in `CMakeLists.txt` and document exact scope
in the feature-specific canonical document plus `docs/mvp-plan.md`.

## Clean generated files

```bash
rm -rf build/
```

## Documentation entry points

- `README.md`: short repository entry point
- `docs/mvp-plan.md`: implementation sequence/current block
- `docs/architecture-summary.md`: condensed architecture
- `docs/file-format.md`: historical PartDocument/Project save-format authority
- `docs/interactive-sketcher-sequence-mvp8.md`: Blocks 106–121 phase authority
- `docs/sketch-shared-topology-mvp8.md`: Block-108 topology/migration/edit/persistence contract
- `docs/sketch-planar-constraint-solver-mvp8.md`: Block-109 solver/DOF/diagnostics contract
- `docs/gui-sketch-solver-drag-mvp8.md`: Block-110 semantic handles/live solve/atomic drag contract
- `docs/gui-sketch-solver-drag-mvp8.md`: Block-110 semantic handles/live solve/atomic drag contract
- `docs/gui-sketch-solver-drag-mvp8.md`: Block-110 semantic handles/live solve/atomic drag contract

## Current development boundary

Blocks 106–110 are implemented. Block 111 is next.

Block 111 adds basic point/line/polyline/rectangle/parallelogram/polygon/centerline/construction
creation. It reuses current Sketch workspace staging, Block-107 snap/inference, Block-108 topology
identity/edit commands, and Block-109 solver authority.

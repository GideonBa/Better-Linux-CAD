# Development Setup

This document covers local BLCAD development, build/test workflows, formatting, and current headless tools. Feature status and sequencing live in `docs/mvp-plan.md`.

## Target environment

- Ubuntu 24.04
- C++20
- CMake 3.28 or newer
- Ninja
- OCCT from Ubuntu packages for optional geometry targets
- Qt 6 packages for later GUI work
- nlohmann-json
- Catch2

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

The current assembly solver and rank diagnostics use project-owned dynamic numeric containers plus small deterministic dense elimination routines. Eigen is not yet a BLCAD target dependency.

## Configure, build, and test

Complete core workflow:

```bash
cmake --workflow --preset dev-build-test
```

Equivalent commands:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Complete geometry-enabled workflow:

```bash
cmake --workflow --preset dev-geometry-build-test
```

Equivalent commands:

```bash
cmake --preset dev-geometry
cmake --build --preset dev-geometry
ctest --preset dev-geometry
```

Build directories from `CMakePresets.json`:

```text
build/dev
build/dev-geometry
build/release
```

## Focused assembly tests

```bash
./build/dev/blcad_core_tests "[core][component-instance]"
./build/dev/blcad_core_tests "[core][assembly-constraint]"
./build/dev/blcad_core_tests "[core][assembly-constraint-graph]"
./build/dev/blcad_core_tests "[core][semantic-axis]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-target]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-transform]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-equation]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-diagnostics]"
```

The Concentric semantic suite covers generated-axis resolution, assembly-space axis evaluation, and geometric Concentric residual semantics.

The Concentric solver suite covers direct shared-numeric flattening, mixed residual order/dimension, rigid-body solving, result application, and local rank/remaining-DOF behavior.

The core solver and diagnostics suites remain regression coverage for the common numeric/application contracts.

## Current test coverage

The exact source registration in `CMakeLists.txt` is authoritative.

At a high level the suites cover:

- core values, parameters, validation, dependency graphs, invalidation, and recompute planning
- sketch/profile geometry, constraints/dimensions, arcs, splines, composite profiles, and construction geometry
- semantic/projected references and reference recovery
- sketch diagnostics, repair commands, transactions, undo, and read-only presentation snapshots
- assembly parameters and project propagation
- component occurrences, placement/state updates, shared part ownership, and JSON roundtrip
- Mate/Concentric/Distance model intent and semantic target persistence
- deterministic active-constraint graph behavior
- generated planar-face target resolution
- stable `SemanticAxisReference` identity and `feature.<feature-id>.axis` tokens
- single-circle subtractive-extrude axis resolution
- rigid-transform point/vector/plane/axis evaluation
- planar Mate/Distance residual construction and target-order semantics
- Concentric direction and lateral-axis-offset residual construction
- exact shared Mate/Distance/Concentric scalar residual flattening
- mixed constraint-family residual ordering and dimension
- central finite-difference Jacobian construction over all supported numeric families
- deterministic rigid-body solver participation, ordering, convergence/failure states, read-only solve behavior, stale-result detection, and atomic application
- lateral Concentric axis-offset and axis-tilt solving
- equal/opposed Concentric axis states and preserved axial/axis-rotation freedoms
- local Jacobian-rank tolerance and remaining-DOF behavior
- regular Concentric rank `4/6` with two remaining DOF
- regular aligned Distance+Concentric rank `5/6` with one remaining DOF
- all-grounded consistency, non-convergence propagation, and residual-row redundancy boundaries
- part, assembly, and project model-intent serialization/file workflows
- optional OCCT workplane/profile/recompute/shape-cache/STEP paths

When adding a feature block, register tests in `CMakeLists.txt` and document the scope in the feature-specific canonical document plus `docs/mvp-plan.md`.

## Private numeric-system test access

The shared assembly numeric-system header is intentionally private to `src/geometry`.

The geometry test target has:

```cmake
target_include_directories(
  blcad_geometry_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/geometry
)
```

This allows focused integration tests to verify exact internal scalar residual ordering and Jacobian dimensions without promoting the numeric-system implementation to public BLCAD API.

Production consumers still access solving and diagnostics only through public geometry-layer APIs.

## Headless tools

Core component/constraint/graph inspection:

```bash
./build/dev/blcad_inspect_project_components examples/component_instances.blcad.project.json
```

Geometry exports:

```bash
./build/dev-geometry/blcad_export_step examples/reference_plate.blcad.json build/reference_plate.step
./build/dev-geometry/blcad_export_step examples/bolt_circle_plate.blcad.json build/bolt_circle_plate.step
```

`blcad_export_project` updates one assembly parameter, propagates bindings, recomputes affected parts, and exports per-part STEP files.

Command shapes:

```text
blcad_export_step <input.blcad.json> <output.step>
blcad_export_project <input.blcad.project.json> <assembly-parameter-id> <value> <output-dir>
blcad_inspect_project_components <input.blcad.project.json>
```

The current target-resolution, transform-evaluation, residual, solver, and DOF-diagnostic APIs do not yet have dedicated CLI consumers.

## Documentation entry points

- `README.md`: short status and current next technical step
- `docs/mvp-plan.md`: implementation sequence
- `docs/architecture-summary.md`: condensed architecture
- `docs/assembly-system.md`: complete current assembly pipeline
- `docs/component-instance-mvp5.md`: component occurrences and placement/state
- `docs/assembly-constraint-model-intent-mvp5.md`: persistent constraint intent
- `docs/assembly-constraint-graph-mvp5.md`: active connectivity
- `docs/assembly-constraint-target-resolution-mvp5.md`: generated plane and axis target resolution boundaries
- `docs/assembly-rigid-transform-evaluation-mvp5.md`: transform convention and plane/axis evaluation
- `docs/assembly-planar-constraint-equations-mvp5.md`: planar residual semantics
- `docs/assembly-semantic-axis-concentric-residuals-mvp5.md`: generated-axis and Concentric residual semantics
- `docs/assembly-concentric-numeric-solver-dof-mvp5.md`: shared Concentric numeric/solver/DOF integration
- `docs/assembly-rigid-body-solver-mvp5.md`: shared rigid-body solver and explicit application
- `docs/assembly-solve-diagnostics-mvp5.md`: local Jacobian rank and remaining DOF
- `docs/file-format.md`: persisted model-intent boundary
- `docs/project-goal.md`: long-term goal

## Formatting

Formatting is configured by `.editorconfig` and `.clang-format`.

For the current Concentric numeric integration block:

```bash
clang-format -i \
  src/geometry/assembly_constraint_numeric_system.cpp \
  tests/geometry/assembly_concentric_numeric_solver_tests.cpp \
  tests/geometry/assembly_rigid_body_solver_tests.cpp \
  tests/geometry/assembly_solve_diagnostics_tests.cpp
```

## Clean generated files

```bash
rm -rf build/
```

## Current assembly development boundary

Implemented:

- persistent component occurrence and Mate/Concentric/Distance intent
- deterministic active connectivity
- generated planar-face target resolution
- stable generated-axis token `feature.<feature-id>.axis`
- generated-axis resolution for exactly-one-circle `SubtractiveExtrude` features
- canonical rigid-transform evaluation for points, vectors, planes, and axes
- planar Mate/Distance residuals
- Concentric axis-line residuals
- shared deterministic Mate/Distance/Concentric numeric residual/Jacobian path
- deterministic rigid-body solver on project copies
- Concentric lateral-offset and tilt solving
- explicit fresh-converged-result application
- local Mate/Distance/Concentric Jacobian-rank and remaining-DOF diagnostics

Important current rules:

- storage-level transform edits remain explicit and solver-independent
- target geometry stays component-local until the transform evaluator
- face and axis target APIs are separate
- `feature.hole.axis` is semantic model intent, not an OCCT topology id
- circular-hole patterns do not get one ambiguous axis token
- Concentric residuals are `cross(dA,dB)` and `cross(oB-oA,dA)`
- equal and opposed axis directions are valid Concentric orientations
- Concentric leaves axial translation and rotation about the common axis free by definition
- all supported numeric families use one residual/Jacobian implementation
- the solver requires one exact deterministic graph connected group
- at least one grounded component is required and all grounded components are fixed
- suppressed components are rejected; visibility does not affect solving
- free-component variables are ordered `tx,ty,tz,rx_deg,ry_deg,rz_deg`
- constraints are consumed in lexicographic id order while target A/B order is preserved
- Mate/Distance flatten to four scalar rows; Concentric flattens to six
- central finite differences are shared by solver and diagnostics
- diagnostics are evaluated only at a converged private solver state
- Concentric regular rank is measured as `4/6`, not hard-coded
- redundant residual rows are not automatically semantic overconstraint
- graph, target, frame, residual, Jacobian, solve-result, rank, and DOF descriptors are not persisted
- raw OCCT topology ids are not persistent model references
- GUI code must not own CAD logic

The next implementation sequence is maintained in `docs/mvp-plan.md`. The immediate next block is stable Insert constraint intent and read-only composite Insert residual semantics.

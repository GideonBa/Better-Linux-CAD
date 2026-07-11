# Development Setup

This document covers local BLCAD development, build/test workflows, formatting, and the current headless tools. Feature status and sequencing live in `docs/mvp-plan.md`.

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
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-target]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-transform]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-equation]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-diagnostics]"
```

The assembly diagnostics suite covers the shared numeric-system refactor as well as local rank and remaining-DOF behavior.

## Current test coverage

The exact source registration in `CMakeLists.txt` is authoritative.

At a high level the suites cover:

- core value types, parameters, validation, dependency graphs, invalidation, and recompute planning
- sketch/profile geometry, constraints/dimensions, arcs, splines, composite profiles, and construction geometry
- semantic/projected references and reference recovery
- sketch diagnostics, repair commands, transactions, undo, and read-only presentation snapshots
- assembly parameters and project propagation
- component occurrences, placement/state updates, shared part ownership, and JSON roundtrip
- Mate/Concentric/Distance model intent and semantic target persistence
- deterministic active-constraint graph behavior
- generated planar face assembly target resolution
- explicit rigid-transform point/vector/planar-frame evaluation
- planar Mate/Distance residual construction and target-order semantics
- deterministic rigid-body solver participation, ordering, convergence/failure states, read-only solve behavior, stale-result detection, and atomic application
- shared assembly numeric residual/Jacobian construction
- local Jacobian-rank tolerance, constrained/remaining DOF counts, underconstrained/locally-full classification, all-grounded consistency, non-convergence propagation, and residual-row redundancy boundaries
- part, assembly, and project model-intent serialization/file workflows
- optional OCCT workplane/profile/recompute/shape-cache/STEP paths

When adding a feature block, register tests in `CMakeLists.txt` and document the scope in the feature-specific canonical document plus `docs/mvp-plan.md`.

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

The current target-resolution, transform-evaluation, planar residual, rigid-body solver, and DOF-diagnostic APIs do not yet have dedicated CLI consumers.

## Documentation entry points

- `README.md`: short status and current next technical step
- `docs/mvp-plan.md`: implementation sequence
- `docs/architecture-summary.md`: condensed architecture
- `docs/assembly-system.md`: complete current assembly pipeline
- `docs/component-instance-mvp5.md`: component occurrences and explicit placement/state
- `docs/assembly-constraint-model-intent-mvp5.md`: persistent constraint intent
- `docs/assembly-constraint-graph-mvp5.md`: active connectivity
- `docs/assembly-constraint-target-resolution-mvp5.md`: generated planar face targets
- `docs/assembly-rigid-transform-evaluation-mvp5.md`: transform convention
- `docs/assembly-planar-constraint-equations-mvp5.md`: planar residual semantics
- `docs/assembly-rigid-body-solver-mvp5.md`: first rigid-body solver and explicit application
- `docs/assembly-solve-diagnostics-mvp5.md`: local Jacobian rank and remaining DOF
- `docs/file-format.md`: persisted model-intent boundary
- `docs/project-goal.md`: long-term goal

## Formatting

Formatting is configured by `.editorconfig` and `.clang-format`.

For the current diagnostics block:

```bash
clang-format -i \
  include/blcad/geometry/assembly_solve_diagnostics.hpp \
  src/geometry/assembly_constraint_numeric_system.hpp \
  src/geometry/assembly_constraint_numeric_system.cpp \
  src/geometry/assembly_rigid_body_solver.cpp \
  src/geometry/assembly_solve_diagnostics.cpp \
  tests/geometry/assembly_solve_diagnostics_tests.cpp
```

## Clean generated files

```bash
rm -rf build/
```

## Current assembly development boundary

Implemented:

- persistent component occurrence and constraint intent
- deterministic active connectivity
- generated planar face target resolution
- canonical rigid-transform evaluation
- planar Mate/Distance residuals
- shared deterministic numeric residual/Jacobian path
- deterministic rigid-body solver on project copies
- explicit fresh-converged-result application
- local Jacobian-rank and remaining-DOF diagnostics

Important current rules:

- storage-level transform edits remain explicit and solver-independent
- the solver requires one exact deterministic graph connected group
- at least one grounded component is required and all grounded components are fixed
- suppressed components are rejected by the first solver; visibility does not affect solving
- free-component variables are ordered `tx,ty,tz,rx_deg,ry_deg,rz_deg`
- residuals are constraint-id ordered and flattened orientation-first, scaled-length-last
- central finite differences are shared by solver and diagnostics
- solver damping does not affect diagnostic rank
- diagnostics are evaluated only at a converged private solver state
- `constrained_dof = rank(J)` and `remaining_dof = variable_count - rank(J)` are local linearized values
- redundant residual rows are not automatically semantic overconstraint
- graph, target, frame, residual, Jacobian, solve-result, rank, and DOF descriptors are not persisted
- semantic axis references and Concentric residual/solve support remain deferred
- raw OCCT topology ids are not persistent model references
- GUI code must not own CAD logic

The next implementation sequence is maintained in `docs/mvp-plan.md`.

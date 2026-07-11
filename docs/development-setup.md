# Development Setup

This document describes the local development setup for BLCAD. Feature status and the implementation sequence are maintained in `docs/mvp-plan.md`; this file focuses on building, testing, formatting, and running the current headless tools.

## System

Current target environment:

- Ubuntu 24.04
- C++20
- CMake 3.28 or newer
- Ninja
- OCCT from the Ubuntu packages for optional geometry targets
- Qt 6 from the Ubuntu packages for later GUI work
- nlohmann-json for model-intent serialization
- Catch2 for tests

## Install dependencies

The package list is described in more detail in `docs/dependencies-ubuntu-24.04.md`.

Standard installation from the Ubuntu repositories:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build pkg-config git clang-format clang-tidy \
  libocct-foundation-dev libocct-modeling-data-dev libocct-modeling-algorithms-dev \
  libocct-data-exchange-dev libocct-visualization-dev libocct-ocaf-dev libocct-draw-dev \
  qt6-base-dev libeigen3-dev libtbb-dev nlohmann-json3-dev libfmt-dev libspdlog-dev catch2 \
  doxygen graphviz
```

The first rigid-body solver uses project-owned dynamic numeric containers and a small partial-pivot dense linear solve. It does not currently add Eigen as a BLCAD build dependency despite the development package already being listed for future numeric work.

## Configure, build, and test

Run the complete core-only workflow:

```bash
cmake --workflow --preset dev-build-test
```

Equivalent commands:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Run the complete geometry-enabled workflow:

```bash
cmake --workflow --preset dev-geometry-build-test
```

Equivalent commands:

```bash
cmake --preset dev-geometry
cmake --build --preset dev-geometry
ctest --preset dev-geometry
```

Focused assembly test commands:

```bash
./build/dev/blcad_core_tests "[core][component-instance]"
./build/dev/blcad_core_tests "[core][assembly-constraint]"
./build/dev/blcad_core_tests "[core][assembly-constraint-graph]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-target]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-transform]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-equation]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-solver]"
```

The build directories are defined by `CMakePresets.json` as `build/dev`, `build/dev-geometry`, and `build/release`.

## Test coverage

The exact test source registration in `CMakeLists.txt` is the source of truth. Do not duplicate an exhaustive per-file list here.

At a high level, the current suites cover:

- core value types, parameters, document validation, dependency graphs, invalidation, and recompute planning
- sketches, constraints/dimensions, profile regions, arcs, splines, composite profiles, and construction geometry
- semantic/projected references, reference recovery, sketch diagnostics, repair commands, transactions, undo, and presentation snapshots
- assembly parameters, project propagation, component instances, placement/state updates, shared part ownership, and JSON roundtrip
- Mate/Concentric/Distance constraint intent, semantic component targets, validation, transform immutability, project structure, and JSON roundtrip
- active-constraint graph nodes/edges, inactive filtering, multi-edges, deterministic adjacency, connected groups, and isolated nodes
- generated-face assembly target resolution and explicit unsupported-family failures
- explicit rigid-transform point/vector/planar-frame evaluation and combined-axis rotation order
- planar Mate/Distance residual construction, target order, transformed target planes, signed Distance direction, and failure paths
- rigid-body solve participation, deterministic variable/residual ordering, Mate and Distance solve convergence, orientation correction, multi-component chains, all-grounded consistency, maximum-iteration state, read-only solve behavior, stale-result detection, and explicit atomic application
- part, assembly, and project model-intent serialization/file workflows
- optional OCCT workplane resolution, profile geometry, recompute execution, shape caching, and STEP export

When a feature block is added, register its tests in `CMakeLists.txt` and document its scope in the feature-specific document plus `docs/mvp-plan.md`.

## Headless tools

The core build provides component, stored assembly-constraint, and graph-group inspection:

```bash
./build/dev/blcad_inspect_project_components examples/component_instances.blcad.project.json
```

The geometry build provides single-part export:

```bash
./build/dev-geometry/blcad_export_step examples/reference_plate.blcad.json build/reference_plate.step
./build/dev-geometry/blcad_export_step examples/bolt_circle_plate.blcad.json build/bolt_circle_plate.step
```

The geometry build also provides `blcad_export_project`. Use it with a valid `.blcad.project.json` file and an assembly parameter id that exists in that project.

Command shapes:

```text
blcad_export_step <input.blcad.json> <output.step>
blcad_export_project <input.blcad.project.json> <assembly-parameter-id> <value> <output-dir>
blcad_inspect_project_components <input.blcad.project.json>
```

The component inspector is read-only. It validates project assembly structure, prints component placement/state and stored constraints, builds `AssemblyConstraintGraph`, and prints active-edge and deterministic connected-group summaries.

Target resolution, transform evaluation, planar residual construction, and rigid-body solving are currently geometry-library APIs with focused tests. They do not yet have dedicated CLI consumers.

## Documentation entry points

Use these maintained status documents:

- `README.md`: short repository status and current next technical step
- `docs/mvp-plan.md`: detailed implementation sequence and implemented/deferred scope
- `docs/architecture-summary.md`: condensed current and target architecture
- `docs/component-instance-mvp5.md`: component instances and explicit placement/state updates
- `docs/assembly-constraint-model-intent-mvp5.md`: Mate/Concentric/Distance record layer
- `docs/assembly-constraint-graph-mvp5.md`: active-constraint connectivity graph
- `docs/assembly-constraint-target-resolution-mvp5.md`: generated-face target resolution
- `docs/assembly-rigid-transform-evaluation-mvp5.md`: local-to-assembly transform convention and evaluator
- `docs/assembly-planar-constraint-equations-mvp5.md`: planar Mate/Distance residual conventions and builder
- `docs/assembly-rigid-body-solver-mvp5.md`: first deterministic rigid-body solver and explicit result-application boundary
- `docs/assembly-system.md`: assembly pipeline, solver, DOF, and joint roadmap
- `docs/file-format.md`: persisted model-intent format
- `docs/project-goal.md`: long-term project goal

Feature-specific documents remain canonical for their own implemented blocks. Avoid copying full feature status into unrelated setup documents.

## Formatting

Project formatting is configured through:

- `.editorconfig`
- `.clang-format`

Use `clang-format` for changed C++ files before committing. For the rigid-body solver block:

```bash
clang-format -i include/blcad/geometry/assembly_rigid_body_solver.hpp \
  src/geometry/assembly_rigid_body_solver.cpp \
  tests/geometry/assembly_rigid_body_solver_tests.cpp
```

## Clean generated files

CMake build directories are under `build/`.

```bash
rm -rf build/
```

## Current development boundaries

The current implementation includes the parametric bolt circle, richer sketch/profile blocks, assembly parameters, the project container, component instances, placement/state updates, persistent Mate/Concentric/Distance intent, active-constraint connectivity, generated-face target resolution, rigid-transform evaluation, planar Mate/Distance residual construction, and the first rigid-body assembly solver plus explicit successful-result application boundary. Those capabilities must not be listed as future or missing in current-status documentation.

The current assembly boundary is deliberate:

- storage-level transform updates remain explicit edits and do not infer constraints
- the solver requires one exact deterministic graph-connected group
- at least one grounded component is required; every grounded component is fixed
- multiple grounded components are allowed
- suppressed group components are rejected by the first solver; visibility does not affect solving
- variable transforms are ordered as `tx,ty,tz,rx_deg,ry_deg,rz_deg` per lexicographically ordered free component
- residuals are ordered by constraint id and flattened orientation-first, length-last
- length residuals use an explicit default `1 mm` scale
- central finite differences, damped Gauss-Newton, partial-pivot elimination, line search, and damping escalation define the first numeric solve policy
- solve states distinguish convergence, maximum iterations, fixed-geometry inconsistency, and numerical failure
- solving occurs on a project copy and never mutates source placement
- solve results snapshot every group component input, including grounded anchors
- only fresh converged results may be explicitly applied
- application occurs through a project copy and atomic replacement
- graph connectivity, resolved targets, evaluated frames, residual descriptors, Jacobians, and solve results are not persisted
- remaining-DOF computation, Jacobian-rank diagnostics, and under/fully/overconstrained analysis remain unimplemented
- semantic axis references and Concentric residual/solve support remain deferred
- collision analysis, subassemblies, and assembly-level STEP export remain future work
- persistent model references remain semantic; raw OCCT topology ids are not core model intent
- no GUI code should own CAD logic

For the exact next implementation sequence, use `docs/mvp-plan.md`.

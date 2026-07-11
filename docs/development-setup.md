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
- read-only active-constraint graph nodes/edges, inactive filtering, multi-edges, deterministic adjacency, connected groups, isolated nodes, and unchanged assembly intent
- generated-face assembly target resolution, unsupported-family failures, deterministic local planar descriptors, separate placement intent, and unchanged project model intent
- explicit rigid-transform evaluation for points, vectors, and planar frames, including identity, translation, right-handed single-axis rotations, combined-axis order, length/orthogonality preservation, determinism, and read-only behavior
- planar Mate and Distance residual construction, including target identity/order, transformed assembly-space target planes, satisfied and unsatisfied residuals, signed Distance direction, inactive/Concentric rejection, unsupported target propagation, determinism, and unchanged project intent
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

Target resolution, rigid-transform evaluation, and planar equation/residual construction are geometry-library APIs with focused tests. They do not yet have dedicated CLI consumers.

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
- `docs/assembly-system.md`: solver, DOF, and joint roadmap over implemented assembly layers
- `docs/file-format.md`: persisted model-intent format
- `docs/project-goal.md`: long-term project goal

Feature-specific documents remain canonical for their own implemented blocks. Avoid copying full feature status into unrelated setup documents.

## Formatting

Project formatting is configured through:

- `.editorconfig`
- `.clang-format`

Use `clang-format` for changed C++ files before committing. For the planar equation block:

```bash
clang-format -i include/blcad/geometry/assembly_constraint_equation_builder.hpp \
  src/geometry/assembly_constraint_equation_builder.cpp \
  tests/geometry/assembly_constraint_equation_builder_tests.cpp
```

## Clean generated files

CMake build directories are under `build/`.

```bash
rm -rf build/
```

## Current development boundaries

The current implementation includes the parametric bolt circle, richer sketch/profile blocks, assembly parameters, the project container, component instances, placement/state updates, persistent Mate/Concentric/Distance intent, the active-constraint graph, generated-face target resolution, explicit rigid-transform evaluation, and planar Mate/Distance equation-residual construction. Those capabilities must not be listed as future or missing in current-status documentation.

The current assembly boundary is deliberate:

- component transforms remain explicit placement model intent, not solver output
- grounding, visibility, and suppression are persisted state and are not enforced by current geometry consumers
- semantic target tokens are persisted; geometry resolution supports the generated planar face family
- target resolution produces component-local planar descriptors plus separate placement intent
- `AssemblyTransformEvaluator` applies the active right-handed fixed-axis X-then-Y-then-Z convention, equivalent to `Rz * Ry * Rx` for column vectors
- points are rotated and translated; vectors, axes, and normals are rotated only
- assembly-space planar descriptors are derived data
- `AssemblyConstraintEquationBuilder` supports active planar Mate and Distance constraints only
- Mate residuals use `nA + nB` and `dot(oB - oA, nA)`
- Distance residuals use `cross(nA, nB)` and signed A-to-B separation along `nA`
- target order is semantically observable for Distance and must be preserved
- graph connectivity, resolved targets, evaluated frames, and residual descriptors are not persisted
- the next block is the first rigid-body assembly solver seed
- no solved transform application, remaining-DOF computation, overconstraint analysis, solver participation for suppression, collision analysis, subassemblies, or assembly-level STEP export exists yet
- semantic axis references and Concentric residual construction remain deferred
- persistent model references remain semantic; raw OCCT topology ids are not core model intent
- no GUI code should own CAD logic

For the exact next implementation sequence, use `docs/mvp-plan.md`.

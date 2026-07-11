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

The equivalent step-by-step commands are:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Run the complete geometry-enabled workflow:

```bash
cmake --workflow --preset dev-geometry-build-test
```

The equivalent step-by-step commands are:

```bash
cmake --preset dev-geometry
cmake --build --preset dev-geometry
ctest --preset dev-geometry
```

Run only the component-instance tests after a core build:

```bash
./build/dev/blcad_core_tests "[core][component-instance]"
```

The build directories are defined by `CMakePresets.json` as `build/dev`, `build/dev-geometry`, and `build/release`.

## Test coverage

The test targets and exact source files registered in the current build are the source of truth in `CMakeLists.txt`. Do not maintain another exhaustive per-test-file list here; it becomes stale as MVP blocks are added.

At a high level, the current suites cover:

- core value types, parameters, document validation, dependency graphs, invalidation, and recompute planning
- sketches, constraints/dimensions, profile regions, arcs, splines, composite profiles, and construction geometry
- semantic/projected references, reference recovery, sketch diagnostics, repair commands, transactions, undo, and presentation snapshots
- assembly parameters, project-level propagation, component instances, free placement/state updates, shared part ownership, and JSON roundtrip
- part, assembly, and project model-intent serialization/file workflows
- optional OCCT workplane resolution, profile geometry, recompute execution, shape caching, and STEP export

When a feature block is added, register its tests in `CMakeLists.txt` and document its scope in the feature-specific document plus `docs/mvp-plan.md`.

## Headless tools

The core build provides component inspection:

```bash
./build/dev/blcad_inspect_project_components examples/component_instances.blcad.project.json
```

The geometry build provides single-part and project export:

```bash
./build/dev-geometry/blcad_export_step examples/reference_plate.blcad.json build/reference_plate.step
./build/dev-geometry/blcad_export_step examples/bolt_circle_plate.blcad.json build/bolt_circle_plate.step
./build/dev-geometry/blcad_export_project \
  examples/flange_project.blcad.project.json assembly.bolt_count 8 build/project-export
```

Command shapes:

```text
blcad_export_step <input.blcad.json> <output.step>
blcad_export_project <input.blcad.project.json> <assembly-parameter-id> <value> <output-dir>
blcad_inspect_project_components <input.blcad.project.json>
```

The component inspector is read-only. It validates project assembly structure and prints component references, visibility, suppression state, grounding state, translation, and rotation.

## Documentation entry points

Use these documents as the maintained status entry points:

- `README.md`: short repository status and current next technical step
- `docs/mvp-plan.md`: detailed implementation sequence and implemented/deferred scope
- `docs/architecture-summary.md`: condensed current and target architecture
- `docs/component-instance-mvp5.md`: component instances and explicit free-placement/state updates
- `docs/assembly-system.md`: assembly constraint, solver, DOF, and joint roadmap
- `docs/file-format.md`: persisted model-intent format
- `docs/project-goal.md`: long-term project goal

Feature-specific documents remain the canonical detail for their own implemented blocks. Avoid copying the full feature status or next-step checklist into unrelated setup documents.

## Formatting

Project formatting is configured through:

- `.editorconfig`
- `.clang-format`

Use `clang-format` for changed C++ files before committing. For example:

```bash
clang-format -i src/core/assembly_document.cpp tests/core/component_instance_tests.cpp
```

## Clean generated files

CMake build directories are located under `build/`.

```bash
rm -rf build/
```

## Current development boundaries

The current implementation already includes the parametric bolt circle, richer sketch/profile blocks, assembly parameters, the project container, component instances, and explicit free-placement/state updates. Those capabilities must not be listed as future or missing in current-status documentation.

The current assembly boundary is still deliberate:

- component transforms are explicit free-placement model intent, not solver output
- grounding, visibility, and suppression are stored state and are not yet enforced by assembly consumers
- assembly Mate, Concentric, and Distance constraint records are the next model-intent block
- no assembly constraint graph, rigid-body solver, remaining-DOF computation, collision analysis, subassemblies, or assembly-level STEP export exists yet
- persistent model references must remain semantic; raw OCCT topology ids are not core model intent
- no GUI code should own CAD logic

For the exact next implementation sequence, use `docs/mvp-plan.md` rather than duplicating it here.

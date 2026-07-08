# Development Setup

This document describes the local development setup for BLCAD.

## System

Current target environment:

- Ubuntu 24.04
- C++20
- CMake 3.28 or newer
- Ninja
- OCCT 7.6 from the Ubuntu packages
- Qt 6 from the Ubuntu packages
- Catch2 for later tests

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

If the local `.deb` packages already exist in the project directory:

```bash
sudo apt-get install ./downloads/apt/*.deb
```

## Verify installation

```bash
cmake --version
ninja --version
pkg-config --version
g++ --version
```

Optional:

```bash
dpkg-query -W cmake ninja-build pkg-config qt6-base-dev libeigen3-dev catch2
dpkg-query -W libocct-foundation-dev libocct-modeling-data-dev libocct-modeling-algorithms-dev libocct-data-exchange-dev
```

## Configure the project

The project provides CMake presets. The default preset builds only the MVP-1 core and tests. OCCT and GUI targets are disabled by default.

Debug configuration:

```bash
cmake --preset dev
```

Release configuration:

```bash
cmake --preset release
```

Expected output for the current state:

```text
BLCAD MVP-1 core skeleton configured.
Geometry/OCCT targets enabled: OFF
GUI/Qt targets enabled: OFF
```

Optional geometry build with OCCT:

```bash
cmake --preset dev-geometry
```

Expected output:

```text
BLCAD MVP-1 core skeleton configured.
Geometry/OCCT targets enabled: ON
GUI/Qt targets enabled: OFF
```

## Build

Debug build:

```bash
cmake --build --preset dev
```

This currently builds:

- `blcad_core`
- `blcad_core_tests`

The geometry build additionally builds:

- `blcad_geometry`
- `blcad_geometry_tests`

## Run tests

Standard command:

```bash
ctest --preset dev
```

Geometry tests:

```bash
ctest --preset dev-geometry
```

Current tests:

- `Quantity` accepts positive millimeter lengths.
- `Quantity` rejects `0`, negative, and non-finite values.
- `Parameter` stores ID, name, scope, and value.
- `Parameter` rejects empty IDs and names.
- `PartDocument` stores ID and name.
- `PartDocument` manages parameters with unique IDs and names.
- `PartDocument` allows parameter lookup by ID and name.
- `PartDocument` manages datum planes with unique IDs.
- `PartDocument` manages sketches with unique IDs.
- `PartDocument` validates sketch workplanes against existing datum planes.
- `PartDocument` validates profile parameter references against existing parameters.
- `PartDocument` manages features with unique IDs.
- `PartDocument` validates feature sketch, feature parameter, and feature target references.
- `DatumPlane` creates the standard `XY` plane.
- `Sketch` stores a workplane reference.
- `RectangleProfile` and `CircleProfile` store parameter references.
- Profile IDs are unique within a sketch.
- `AdditiveExtrude` and `SubtractiveExtrude` store feature intent without geometry.
- `DependencyGraph` stores nodes and dependency edges.
- `DependencyGraph` finds direct and transitive dependents.
- `DependencyGraph` returns a topological order for the reference plate.
- `DependencyGraph` detects cycles as dependency errors.
- `PartDocument` creates dependency graph nodes for parameters, sketches, and features.
- `PartDocument` creates dependency graph edges from profile and feature references.
- `InvalidationState` stores `clean`, `changed`, `dirty`, and `error`.
- `InvalidationState` marks transitive dependents through the `DependencyGraph`.
- `PartDocument` marks affected nodes after a parameter change.
- `RecomputePlan` lists `dirty` nodes in topological order.
- `PartDocument` can derive a recompute plan from its state.
- `RectangleExtrusionAdapter` creates a non-empty OCCT solid from positive lengths in the optional geometry build.
- `GeometryShape` wraps a computed shape as an opaque handle.
- `ShapeCache` stores feature shapes and a final shape in the optional geometry build.
- `GeometryRecomputeExecutor` executes an `AdditiveExtrude` from a rectangle profile in the optional geometry build and writes to the `ShapeCache`.
- `Error` and `Result` store expected errors and successful values.

Later MVP-1 tests should add centered cut and STEP export.

## Documentation

Important documents:

- `zielarchitektur-parametrisches-cad-system.tex`: original target architecture
- `docs/architecture-summary.md`: condensed architecture overview
- `docs/core-mvp1-skeleton.md`: current core skeleton
- `docs/sketch-mvp1-data-model.md`: sketch and profile data model
- `docs/feature-mvp1-data-model.md`: feature data model
- `docs/dependency-graph-mvp1-data-model.md`: dependency graph data model
- `docs/invalidation-mvp1-data-model.md`: invalidation-state data model
- `docs/recompute-plan-mvp1-data-model.md`: recompute-plan data model
- `docs/geometry-adapter-mvp1-rectangle-extrusion.md`: first OCCT adapter
- `docs/shape-cache-mvp1-data-model.md`: ShapeCache data model
- `docs/recompute-execution-mvp1-additive-extrude.md`: first additive recompute execution
- `docs/mvp-plan.md`: MVP sequence
- `docs/mvp-1-specification.md`: detailed MVP-1 specification
- `docs/decisions/`: architecture decision records
- `docs/dependencies-ubuntu-24.04.md`: package list for Ubuntu 24.04

## Formatting

Project formatting is prepared:

- `.editorconfig`
- `.clang-format`

Use `clang-format` for C++ files before committing.

## Clean generated files

CMake build directories are located under `build/`.

```bash
rm -rf build/
```

LaTeX helper files are listed in `.gitignore`. The generated target-architecture PDF may remain as a working artifact in the directory.

## Development rule for MVP 1

The first code should only affect the MVP-1 path:

- no GUI code
- no assembly
- no engineering assistants
- no bolt circle
- no general constraint solver

Only extend the scope after parameters, recompute, OCCT geometry, and STEP export work for the reference plate.

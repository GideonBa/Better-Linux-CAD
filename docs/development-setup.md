# Development Setup

This document describes the local development setup for BLCAD.

## System

Current target environment:

- Ubuntu 24.04
- C++20
- CMake 3.28 or newer
- Ninja
- OCCT 7.6 from the Ubuntu packages
- Qt 6 from the Ubuntu packages for later GUI work
- nlohmann-json for MVP-1 model-intent serialization
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
dpkg-query -W cmake ninja-build pkg-config qt6-base-dev libeigen3-dev catch2 nlohmann-json3-dev
dpkg-query -W libocct-foundation-dev libocct-modeling-data-dev libocct-modeling-algorithms-dev libocct-data-exchange-dev
```

## Configure the project

The project provides CMake presets. The default preset builds the MVP-1 core and core tests. OCCT and GUI targets are disabled by default.

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

Current test coverage includes:

- `Quantity`, `Error`, and `Result`
- `Parameter` creation and value updates
- `PartDocument` identity, validation, lookup, and reference management
- datum-plane, sketch, rectangle-profile, and circle-profile data models
- additive and subtractive feature-intent data models
- dependency graph construction, topological ordering, and cycle detection
- invalidation state and recompute-plan creation
- MVP-1 JSON serialization and deserialization of `PartDocument` model intent
- rectangle extrusion through OCCT in the optional geometry build
- centered circular cut through OCCT in the optional geometry build
- `ShapeCache` storage of feature shapes and final shapes
- additive and subtractive recompute execution
- full document recompute into a fresh `ShapeCache`
- STEP export of the final shape
- numeric incremental recompute after a real parameter change
- recompute and STEP export from a JSON-restored document

## Documentation

Important documents:

- `docs/architecture-summary.md`: condensed architecture overview
- `docs/core-mvp1-skeleton.md`: current core skeleton
- `docs/sketch-mvp1-data-model.md`: sketch and profile data model
- `docs/feature-mvp1-data-model.md`: feature data model
- `docs/dependency-graph-mvp1-data-model.md`: dependency graph data model
- `docs/invalidation-mvp1-data-model.md`: invalidation-state data model
- `docs/recompute-plan-mvp1-data-model.md`: recompute-plan data model
- `docs/geometry-adapter-mvp1-rectangle-extrusion.md`: first OCCT adapter
- `docs/geometry-adapter-mvp1-circular-cut.md`: centered cut OCCT adapter
- `docs/shape-cache-mvp1-data-model.md`: ShapeCache data model
- `docs/recompute-execution-mvp1-additive-extrude.md`: additive recompute execution
- `docs/recompute-execution-mvp1-subtractive-cut.md`: subtractive recompute execution
- `docs/step-export-mvp1.md`: STEP export
- `docs/document-recompute-mvp1.md`: full document recompute and reference-part pipeline
- `docs/parameter-update-mvp1.md`: parameter update and numeric incremental recompute
- `docs/json-serialization-mvp1.md`: JSON serialization of model intent
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

LaTeX helper files are listed in `.gitignore`. Generated architecture PDFs may remain as working artifacts if needed.

## Development rule for MVP 1

The current code should still stay on the MVP-1 path:

- no GUI code
- no assembly
- no engineering assistants
- no bolt circle
- no general constraint solver
- no serialization of OCCT geometry

The next step should add file-level `.blcad.json` workflow support and a non-GUI command-line example before larger modeling features are introduced.

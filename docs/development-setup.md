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

## Configure and build

Core-only build:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Geometry build:

```bash
cmake --preset dev-geometry
cmake --build --preset dev-geometry
ctest --preset dev-geometry
```

Current test coverage includes:

- `Quantity`, `Error`, and `Result`
- `Parameter` creation and value updates
- `PartDocument` identity, validation, lookup, and reference management
- datum-plane, derived-workplane, construction-geometry, sketch, line-segment, rectangle-profile, circle-profile, and closed-profile data models
- construction point, construction line, and construction plane validation
- construction plane workplane references from sketches
- construction-geometry parameter-dependency invalidation paths
- closed-profile validation for ordered connected loops and self-intersection rejection
- semantic top, bottom, right, left, front, and back face references for generated additive extrudes
- additive and subtractive feature-intent data models
- dependency graph construction, topological ordering, and cycle detection
- invalidation state and recompute-plan creation
- JSON serialization/deserialization of model intent, including top, bottom, right, left, front, and back derived workplanes
- JSON serialization/deserialization of line segments and line-based closed profiles
- JSON serialization/deserialization of explicit construction points, lines, planes, and their parameter dependencies
- `.blcad.json` file read/write helpers
- rectangle extrusion through OCCT in the optional geometry build
- centered and axis-directed circular cuts through OCCT in the optional geometry build
- line-based closed-profile extrusion through OCCT in the optional geometry build
- line-based closed-profile through-all cuts through OCCT in the optional geometry build
- explicit construction-plane resolution through `WorkplaneResolver`
- `ShapeCache` storage, replacement, removal, and final-shape tracking
- geometry-layer `WorkplaneResolver`
- rectangular bounds on resolved top, bottom, right, left, front, and back workplanes
- mapping local sketch points through resolved datum, derived, and construction workplanes
- additive and subtractive recompute execution
- full document recompute into a fresh `ShapeCache`
- STEP export of the final shape
- numeric incremental recompute after a real parameter change
- recompute and STEP export from a JSON-restored document
- recompute of off-center cuts whose sketches reference derived top, bottom, right, left, front, or back workplanes
- recompute of triangle closed-profile prisms and triangle closed-profile through-all cuts
- recompute of a closed-profile prism whose sketch references an explicit construction plane
- bounded face validation for valid and out-of-bounds holes and closed-profile vertices
- incremental recompute through derived-workplane and construction-geometry dependency paths
- stale dirty feature-shape removal before incremental recompute

## Headless examples

After configuring and building the geometry preset, export the checked-in models:

```bash
./build/dev-geometry/blcad_export_step examples/reference_plate.blcad.json build/reference_plate.step
./build/dev-geometry/blcad_export_step examples/top_face_cut.blcad.json build/top_face_cut.step
./build/dev-geometry/blcad_export_step examples/bottom_face_cut.blcad.json build/bottom_face_cut.step
./build/dev-geometry/blcad_export_step examples/right_face_cut.blcad.json build/right_face_cut.step
./build/dev-geometry/blcad_export_step examples/left_face_cut.blcad.json build/left_face_cut.step
./build/dev-geometry/blcad_export_step examples/front_face_cut.blcad.json build/front_face_cut.step
./build/dev-geometry/blcad_export_step examples/back_face_cut.blcad.json build/back_face_cut.step
./build/dev-geometry/blcad_export_step examples/triangle_prism.blcad.json build/triangle_prism.step
./build/dev-geometry/blcad_export_step examples/triangle_cut_plate.blcad.json build/triangle_cut_plate.step
./build/dev-geometry/blcad_export_step examples/construction_plane_prism.blcad.json build/construction_plane_prism.step
```

Depending on the local CMake preset output directory, the executable path may differ. The command shape is:

```text
blcad_export_step <input.blcad.json> <output.step>
```

## Documentation

Important documents:

- `docs/project-goal.md`: explicit long-term project goal
- `docs/architecture-summary.md`: condensed architecture overview
- `docs/core-mvp1-skeleton.md`: current core skeleton
- `docs/sketch-mvp1-data-model.md`: sketch and profile data model
- `docs/feature-mvp1-data-model.md`: feature data model
- `docs/dependency-graph-mvp1-data-model.md`: dependency graph data model
- `docs/invalidation-mvp1-data-model.md`: invalidation-state data model
- `docs/recompute-plan-mvp1-data-model.md`: recompute-plan data model
- `docs/geometry-adapter-mvp1-rectangle-extrusion.md`: first OCCT adapter
- `docs/geometry-adapter-mvp1-circular-cut.md`: centered and axis-directed cut OCCT adapter
- `docs/shape-cache-mvp1-data-model.md`: ShapeCache data model
- `docs/recompute-execution-mvp1-additive-extrude.md`: additive recompute execution
- `docs/recompute-execution-mvp1-subtractive-cut.md`: subtractive recompute execution
- `docs/step-export-mvp1.md`: STEP export
- `docs/document-recompute-mvp1.md`: full document recompute and reference-part pipeline
- `docs/parameter-update-mvp1.md`: parameter update and numeric incremental recompute
- `docs/json-serialization-mvp1.md`: JSON serialization of model intent
- `docs/json-file-workflow-mvp1.md`: `.blcad.json` file workflow and headless export example
- `docs/derived-workplane-mvp2-seed.md`: semantic generated-face workplanes and sketches on generated planar faces
- `docs/workplane-resolver-mvp2.md`: geometry-layer resolver for derived and construction workplanes
- `docs/bounded-workplane-validation-mvp2.md`: bounded circle and closed-profile validation on derived workplanes
- `docs/incremental-derived-workplane-recompute-mvp2.md`: incremental recompute through derived-workplane dependencies
- `docs/bottom-workplane-mvp2.md`: bottom-face derived workplane for simple additive extrudes
- `docs/right-workplane-mvp2.md`: right-face derived workplane for simple additive extrudes
- `docs/left-workplane-mvp2.md`: left-face derived workplane for simple additive extrudes
- `docs/front-workplane-mvp2.md`: front-face derived workplane for simple additive extrudes
- `docs/back-workplane-mvp2.md`: back-face derived workplane for simple additive extrudes
- `docs/general-closed-sketch-profile-mvp.md`: implemented first line-based closed profile block
- `docs/construction-geometry-mvp.md`: implemented explicit construction geometry and next relation-driven construction geometry
- `docs/inventor-like-sketcher-and-feature-roadmap.md`: future Inventor-like sketcher and sketch-driven feature roadmap
- `docs/advanced-surfacing-and-3d-sketch-mvp.md`: future 3D sketch and surfacing block
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

## Development rule after explicit construction geometry

The current code should still move carefully:

- no GUI code yet
- no assembly yet
- no engineering assistants yet
- no bolt circle yet
- no general constraint solver yet
- no serialization of OCCT geometry
- no full topological naming system yet
- no arbitrary generated-face topology yet
- no arcs, splines, multiple contours, inner holes, or automatic profile-region detection yet
- no relation-driven construction geometry yet
- no generated semantic edge/vertex construction references yet
- no 3D sketching, sweep, loft, surfacing, or closed-shell-to-solid conversion yet

The next technical step should start relation-driven construction geometry with `ConstructionRelation`, `PlaneOffsetFromPlane`, `LineThroughTwoPoints`, and `PlaneThroughThreePoints`, while keeping explicit construction geometry as the stable fallback path.

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

Build directories:

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
./build/dev/blcad_core_tests "[core][semantic-seat]"
./build/dev/blcad_core_tests "[core][assembly-insert]"

./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-target]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-transform]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-equation]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-insert]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-diagnostics]"
```

The Concentric semantic suite covers generated-axis resolution, assembly-space axis evaluation, and geometric residual semantics.

The Concentric solver suite covers shared-numeric flattening, mixed residual order/dimension, rigid-body solving, application, and local rank/DOF behavior.

The Insert core suites cover `SemanticSeatingPlaneReference`, `AssemblyConstraintType::Insert`, distance-free validation, JSON roundtrip, and unchanged component placement during intent creation/load.

The Insert geometry suite covers `.seat` composite endpoint resolution, source feature/profile identity, opposite-extrude right-handed seat frames, assembly-space axis/seat evaluation, canonical composite residuals, signed target-order seating, direct finite-difference rank `5/6`, rotation-about-axis freedom, and the explicit current solver boundary.

## Current test coverage

The exact source registration in `CMakeLists.txt` is authoritative.

At a high level the suites cover:

- core values, parameters, validation, dependency graphs, invalidation, and recompute planning
- sketch/profile geometry, constraints/dimensions, arcs, splines, composite profiles, and construction geometry
- semantic/projected references and reference recovery
- sketch diagnostics, repair commands, transactions, undo, and read-only presentation snapshots
- assembly parameters and project propagation
- component occurrences, placement/state updates, shared part ownership, and JSON roundtrip
- Mate/Concentric/Distance/Insert model intent and semantic target persistence
- deterministic active-constraint graph behavior
- generated planar-face target resolution
- stable `SemanticAxisReference` / `feature.<feature-id>.axis`
- stable `SemanticSeatingPlaneReference` / `feature.<feature-id>.seat`
- single-circle subtractive-extrude axis/seat resolution
- rigid-transform point/vector/plane/axis evaluation
- composite Insert axis/seat assembly-space evaluation through existing transform semantics
- planar Mate/Distance residual construction and target-order semantics
- Concentric direction/lateral-axis-offset residual construction
- Insert direction/axis-offset/signed-seating residual construction
- exact shared Mate/Distance/Concentric scalar flattening
- mixed integrated constraint-family ordering/dimensions
- central finite-difference Jacobian construction over current shared numeric families
- deterministic rigid-body solver participation, ordering, convergence/failure, read-only solve, stale-result detection, and atomic application
- lateral Concentric offset/tilt solving
- equal/opposed Concentric states and preserved axial/axis-rotation freedoms
- local rank tolerance and remaining-DOF behavior
- regular Concentric rank `4/6`
- regular aligned Distance+Concentric rank `5/6`
- direct regular Insert residual rank `5/6` with one remaining axis-rotation DOF
- all-grounded consistency, non-convergence propagation, and residual-row redundancy boundaries
- part, assembly, and project serialization/file workflows
- optional OCCT workplane/profile/recompute/shape-cache/STEP paths

When adding a feature block, register tests in `CMakeLists.txt` and document the scope in the feature-specific canonical document plus `docs/mvp-plan.md`.

## Private numeric-system test access

The shared assembly numeric-system header remains private to `src/geometry`.

The geometry test target has a private include path to `src/geometry` so focused integration tests may verify exact internal scalar residual order and Jacobian dimensions without promoting the numeric implementation to public BLCAD API.

Production consumers access solving and diagnostics only through public geometry-layer APIs.

The current Insert rank seed intentionally does not use this private numeric path because Insert is not yet integrated. Its test constructs the documented seven raw scalar residuals from `InsertResidualDescriptor`, finite-differences the public placement path, and measures local matrix rank independently.

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

Current target-resolution, transform-evaluation, residual, solver, and DOF APIs do not yet have dedicated CLI consumers.

## Documentation entry points

- `README.md`: short status and current next step
- `docs/mvp-plan.md`: implementation sequence
- `docs/architecture-summary.md`: condensed architecture
- `docs/assembly-system.md`: complete current assembly pipeline
- `docs/component-instance-mvp5.md`: component occurrences and placement/state
- `docs/assembly-constraint-model-intent-mvp5.md`: persistent relationship intent
- `docs/assembly-constraint-graph-mvp5.md`: active connectivity
- `docs/assembly-constraint-target-resolution-mvp5.md`: plane/axis/seat target resolution
- `docs/assembly-rigid-transform-evaluation-mvp5.md`: transform convention and plane/axis evaluation
- `docs/assembly-planar-constraint-equations-mvp5.md`: planar residual semantics
- `docs/assembly-semantic-axis-concentric-residuals-mvp5.md`: axis and Concentric semantics
- `docs/assembly-concentric-numeric-solver-dof-mvp5.md`: shared Concentric numeric/solver/DOF integration
- `docs/assembly-insert-intent-composite-residuals-mvp5.md`: Insert intent, seating targets, composite residuals, and rank seed
- `docs/assembly-rigid-body-solver-mvp5.md`: shared rigid-body solver and explicit application
- `docs/assembly-solve-diagnostics-mvp5.md`: local Jacobian rank and remaining DOF
- `docs/file-format.md`: persistence boundary
- `docs/project-goal.md`: long-term goal

## Formatting

Formatting is configured by `.editorconfig` and `.clang-format`.

For the current Insert block:

```bash
clang-format -i \
  include/blcad/core/assembly_constraint.hpp \
  include/blcad/core/datum_plane.hpp \
  include/blcad/geometry/assembly_constraint_target_resolver.hpp \
  include/blcad/geometry/assembly_insert_constraint_equation_builder.hpp \
  src/core/assembly_constraint.cpp \
  src/core/datum_plane.cpp \
  src/geometry/assembly_constraint_equation_builder.cpp \
  src/geometry/assembly_constraint_target_resolver.cpp \
  src/geometry/assembly_insert_constraint_equation_builder.cpp \
  tests/core/assembly_insert_constraint_tests.cpp \
  tests/geometry/assembly_insert_constraint_equation_builder_tests.cpp
```

## Clean generated files

```bash
rm -rf build/
```

## Current assembly development boundary

Implemented:

- persistent component occurrence and Mate/Concentric/Distance/Insert intent
- deterministic active connectivity
- generated planar-face target resolution
- stable generated-axis token `feature.<feature-id>.axis`
- stable generated-seat token `feature.<feature-id>.seat`
- single-circle `SubtractiveExtrude` axis and composite Insert seat resolution
- canonical rigid-transform evaluation for points, vectors, planes, and axes
- planar Mate/Distance residuals
- Concentric axis-line residuals
- composite Insert axis/offset/seating residuals
- shared Mate/Distance/Concentric numeric residual/Jacobian path
- deterministic rigid-body solver on project copies
- Concentric lateral-offset and tilt solving
- explicit fresh-converged-result application
- local shared Mate/Distance/Concentric rank/DOF diagnostics
- direct read-only Insert residual rank `5/6` seed

Important current rules:

- storage-level transform edits remain explicit and solver-independent
- target geometry stays component-local until the transform evaluator
- plane, axis, and composite seat target APIs are explicit
- `feature.hole.axis` and `feature.hole.seat` are semantic model intent, not OCCT topology ids
- circular-hole patterns do not get ambiguous single axis/seat tokens
- Concentric residuals are `cross(dA,dB)` and `cross(oB-oA,dA)`
- Insert adds `dot(sB-sA,nA)` signed seating to the same axis-alignment convention
- equal and opposed axis directions remain valid Concentric/Insert axis-line orientations
- Insert seat normal is canonically aligned with its semantic axis direction
- Insert has seven scalar residual components but regular local rank five
- the regular Insert remaining freedom is rotation about the common axis
- current shared numeric system/solver/diagnostics support Mate, Distance, and Concentric only
- the solver requires one exact deterministic connected group
- at least one grounded component is required and all grounded components are fixed
- suppressed components are rejected; visibility does not affect solving
- free-component variables are ordered `tx,ty,tz,rx_deg,ry_deg,rz_deg`
- integrated constraints use lexicographic id order while target A/B order is preserved
- central finite differences are shared by solver and diagnostics
- graph, target, frame, residual, Jacobian, solve-result, rank, and DOF descriptors are not persisted
- raw OCCT topology ids are not persistent model references
- GUI code must not own CAD logic

The immediate next block is Insert numeric-system, solver, explicit application, and shared DOF-diagnostics integration. Exact sequencing is maintained in `docs/mvp-plan.md`.

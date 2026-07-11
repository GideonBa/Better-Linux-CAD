# BLCAD

BLCAD is a planned independent parametric CAD system for Linux. The project stores CAD model intent in BLCAD data structures and uses OCCT/Open CASCADE as computed geometry, not as the primary model.

Detailed architecture and feature status live in `docs/`. This README is intentionally short.

## Status

Current state: MVP-1 core skeleton, staged MVP-2 sketch/workplane/profile/recompute/reference blocks, the MVP-3 parametric bolt circle, the MVP-4 assembly/project container path, and MVP-5 assembly infrastructure through deterministic Mate/Distance/Concentric rigid-body solving, local Jacobian-rank/remaining-DOF diagnostics, and stable read-only Insert target/residual semantics.

The implemented assembly path now includes:

```text
component and Mate/Distance/Concentric/Insert model intent
  -> deterministic active-constraint graph
  -> generated planar face, circular-cut axis, and circular-cut seat target resolution
  -> explicit local-to-assembly rigid-transform evaluation
  -> planar Mate/Distance, axis-line Concentric, and composite Insert residual construction
  -> Mate/Distance/Concentric shared numeric residual/Jacobian system
  -> damped Gauss-Newton rigid-body solve on Project copies
  -> explicit atomic converged-result application
  -> read-only local Jacobian-rank and remaining-DOF diagnostics
```

Concentric uses the shared numeric system and solver. A regular one-free-body Concentric relationship is proven as rank `4/6` with `2` remaining local DOF.

Insert now has explicit persistent relationship intent and a stable `feature.<feature-id>.seat` endpoint family. Each supported seat endpoint derives one primary axis plus one oriented seating plane from the same circular-cut feature/profile. The read-only composite Insert residual is proven by direct finite differences as rank `5/6`, leaving only rotation about the common axis free. Insert is not yet connected to the shared numeric solver.

There is no GUI yet.

For the complete implementation sequence, see `docs/mvp-plan.md`.

## Technical basis

- C++20
- CMake + Ninja
- OCCT / Open CASCADE Technology
- nlohmann-json
- Catch2
- Qt 6 planned for the future GUI

## Build and test

Core workflow:

```bash
cmake --workflow --preset dev-build-test
```

Geometry-enabled workflow:

```bash
cmake --workflow --preset dev-geometry-build-test
```

Manual geometry build:

```bash
cmake --preset dev-geometry
cmake --build --preset dev-geometry
```

Focused current assembly tests:

```bash
./build/dev/blcad_core_tests "[core][semantic-axis]"
./build/dev/blcad_core_tests "[core][semantic-seat]"
./build/dev/blcad_core_tests "[core][assembly-insert]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-equation]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-transform]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-insert]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-diagnostics]"
```

## Headless tools

```text
blcad_export_step <input.blcad.json> <output.step>
blcad_export_project <input.blcad.project.json> <assembly-parameter-id> <value> <output-dir>
blcad_inspect_project_components <input.blcad.project.json>
```

Examples:

```bash
./build/dev-geometry/blcad_export_step examples/reference_plate.blcad.json build/reference_plate.step
./build/dev-geometry/blcad_export_step examples/bolt_circle_plate.blcad.json build/bolt_circle_plate.step
./build/dev/blcad_inspect_project_components examples/component_instances.blcad.project.json
```

## Repository structure

- `include/blcad/`: public headers
- `src/`: core and optional geometry implementation
- `tests/`: Catch2 tests
- `examples/`: `.blcad.json` / `.blcad.project.json` examples and headless examples
- `docs/`: architecture, implemented MVP blocks, and roadmaps

## Key documents

Start here:

- `docs/project-goal.md`
- `docs/architecture-summary.md`
- `docs/mvp-plan.md`
- `docs/development-setup.md`
- `docs/file-format.md`

Implemented assembly blocks:

- `docs/assembly-parameters-mvp4.md`
- `docs/project-container-mvp4.md`
- `docs/component-instance-mvp5.md`
- `docs/assembly-constraint-model-intent-mvp5.md`
- `docs/assembly-constraint-graph-mvp5.md`
- `docs/assembly-constraint-target-resolution-mvp5.md`
- `docs/assembly-rigid-transform-evaluation-mvp5.md`
- `docs/assembly-planar-constraint-equations-mvp5.md`
- `docs/assembly-rigid-body-solver-mvp5.md`
- `docs/assembly-solve-diagnostics-mvp5.md`
- `docs/assembly-semantic-axis-concentric-residuals-mvp5.md`
- `docs/assembly-concentric-numeric-solver-dof-mvp5.md`
- `docs/assembly-insert-intent-composite-residuals-mvp5.md`

Broader implemented sketch/profile documents remain listed from `docs/mvp-plan.md` and `docs/architecture-summary.md`.

Future roadmaps:

- `docs/multi-body-transform-and-path-features-roadmap.md`
- `docs/inventor-like-sketcher-and-feature-roadmap.md`
- `docs/advanced-surfacing-and-3d-sketch-mvp.md`

## Next technical step

The next technical step is explicit Insert integration into the shared numeric residual/Jacobian system, rigid-body solver, result-application path, and remaining-DOF diagnostics.

The block must route Insert through the dedicated composite builder, preserve every existing Mate/Distance/Concentric ordering and solver contract, flatten axis alignment before scaled axial seating, solve lateral offset, tilt, and axial seating through the existing finite-difference/Gauss-Newton path, preserve rotation about the common axis, and prove the regular one-free-body Insert system has rank five with one remaining DOF.

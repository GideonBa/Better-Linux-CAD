# BLCAD

BLCAD is a planned independent parametric CAD system for Linux. The project stores CAD model intent in BLCAD data structures and uses OCCT/Open CASCADE as computed geometry, not as the primary model.

Detailed architecture and feature status live in `docs/`. This README is intentionally short.

## Status

Current state: MVP-1 core skeleton, staged MVP-2 sketch/workplane/profile/recompute/reference blocks, the MVP-3 parametric bolt circle, the MVP-4 assembly/project container path, and MVP-5 assembly infrastructure through deterministic planar rigid-body solving and read-only local Jacobian-rank/remaining-DOF diagnostics.

The implemented assembly path now includes:

```text
component and constraint model intent
  -> deterministic active-constraint graph
  -> generated planar face target resolution
  -> explicit local-to-assembly rigid-transform evaluation
  -> planar Mate/Distance residual construction
  -> shared deterministic numeric residual/Jacobian system
  -> damped Gauss-Newton rigid-body solve on Project copies
  -> explicit atomic converged-result application
  -> read-only local Jacobian-rank and remaining-DOF diagnostics
```

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
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-equation]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-transform]"
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

Broader implemented sketch/profile documents remain listed from `docs/mvp-plan.md` and `docs/architecture-summary.md`.

Future roadmaps:

- `docs/multi-body-transform-and-path-features-roadmap.md`
- `docs/inventor-like-sketcher-and-feature-roadmap.md`
- `docs/advanced-surfacing-and-3d-sketch-mvp.md`

## Next technical step

The next technical step is a stable semantic generated-axis reference family and a read-only Concentric target/residual pipeline.

The block should expose supported feature-produced axis intent without raw OCCT topology ids, resolve component-local axis descriptors through project-owned part documents, evaluate axis lines in assembly space with the canonical `RigidTransform` convention, and construct deterministic Concentric residual descriptors. Solver integration follows only after those target and residual semantics are stable. Insert remains downstream because it combines axis alignment with axial seating semantics.

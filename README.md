# BLCAD

BLCAD is a planned independent parametric CAD system for Linux. The project stores CAD model intent in BLCAD data structures and uses OCCT/Open CASCADE as computed geometry, not as the primary model.

Detailed architecture and feature status live in `docs/`. This README is intentionally short.

## Status

Current state: MVP-1 core skeleton, staged MVP-2 sketch/workplane/profile/recompute/reference blocks, the MVP-3 parametric bolt circle, the MVP-4 assembly/project container path, and MVP-5 assembly infrastructure through deterministic Mate/Distance/Concentric/Insert/Angle rigid-body solving, local Jacobian-rank/remaining-DOF diagnostics, suppressed-component solve filtering, and posed assembly STEP compound export.

The implemented assembly path now includes:

```text
component and Mate/Distance/Concentric/Insert/Angle model intent
  -> deterministic active-constraint graph
  -> generated planar face, circular-cut axis, and circular-cut seat target resolution
  -> explicit local-to-assembly rigid-transform evaluation
  -> planar, axis-line, and composite Insert residual construction
  -> shared numeric residual/Jacobian system
  -> damped Gauss-Newton rigid-body solve on Project copies
  -> explicit atomic converged-result application
  -> read-only local Jacobian-rank and remaining-DOF diagnostics
  -> suppressed-component filtering over the active solve subgroup
  -> one recomputed ShapeCache per referenced part document
  -> visible active component shape posing with the same X-then-Y-then-Z transform convention
  -> one derived OCCT compound and STEP export
```

Concentric, Insert, and planar Angle use the shared numeric system and solver. Regular one-free-body Concentric and Insert relationships are proven as rank `4/6` and `5/6`; the scalar cosine Angle seed is rank `1/6` away from its documented extremal degeneracies.

Suppressed components contribute no solve variables and every constraint touching them vanishes from the active numeric subgroup while full solve snapshots still protect result application from stale suppression changes.

`AssemblyStepExporter` recomputes each referenced part once per export, reuses that cache across repeated occurrences, skips hidden and suppressed components, applies persisted rigid transforms to shape copies, composes one OCCT compound, and delegates final file writing to the existing STEP writer. Export geometry remains derived and unpersisted.

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
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-step-export]"
```

## Headless tools

```text
blcad_export_step <input.blcad.json> <output.step>
blcad_export_project <input.blcad.project.json> <assembly-parameter-id> <value> <output-dir>
blcad_inspect_project_components <input.blcad.project.json>
blcad_export_posed_assembly <input.blcad.project.json> <output.step>
```

Examples:

```bash
./build/dev-geometry/blcad_export_step examples/reference_plate.blcad.json build/reference_plate.step
./build/dev-geometry/blcad_export_step examples/bolt_circle_plate.blcad.json build/bolt_circle_plate.step
./build/dev/blcad_inspect_project_components examples/component_instances.blcad.project.json
./build/dev-geometry/blcad_export_posed_assembly examples/posed_assembly.blcad.project.json build/posed_assembly.step
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
- `docs/assembly-insert-numeric-solver-dof-mvp5.md`
- `docs/assembly-angle-constraint-mvp5.md`
- `docs/assembly-suppressed-component-solving-mvp5.md`
- `docs/assembly-posed-step-export-mvp5.md`

Broader implemented sketch/profile documents remain listed from `docs/mvp-plan.md` and `docs/architecture-summary.md`.

Future roadmaps:

- `docs/multi-body-transform-and-path-features-roadmap.md`
- `docs/inventor-like-sketcher-and-feature-roadmap.md`
- `docs/advanced-surfacing-and-3d-sketch-mvp.md`

## Next technical step

The next technical step is the first joint/limit model-intent and motion seed: define persistent solver-independent joint records and limit ranges on semantic assembly targets, derive their active graph participation without persisting numeric state, and integrate one minimal motion-capable joint family through the existing rigid-body solve/application boundary. See `docs/mvp-plan.md`.

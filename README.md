# BLCAD

BLCAD is a planned independent parametric CAD system for Linux. The project stores CAD model intent in BLCAD data structures and uses OCCT/Open CASCADE as the computed geometry kernel cache, not as the primary model.

Detailed architecture and feature status live in `docs/`. This README is intentionally kept short.

## Status

Current state: MVP-1 core skeleton, staged MVP-2 seeds for sketches, workplanes, profile geometry, recompute, STEP export, reference recovery, sketch diagnostics, and repair-command infrastructure, the MVP-3 parametric bolt circle, the MVP-4 assembly/project container path, and MVP-5 component instances with explicit free-placement/state updates.

There is no GUI yet.

For the full implementation sequence, see:

```text
docs/mvp-plan.md
```

## Technical basis

- C++20
- CMake + Ninja
- OCCT / Open CASCADE Technology
- nlohmann-json
- Catch2
- Qt 6 planned for the future GUI

## Build and test

Run core configure, build, and tests:

```bash
cmake --workflow --preset dev-build-test
```

Run geometry configure, build, and tests:

```bash
cmake --workflow --preset dev-geometry-build-test
```

Manual geometry build:

```bash
cmake --preset dev-geometry
cmake --build --preset dev-geometry
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
./build/dev-geometry/blcad_inspect_project_components examples/component_instances.blcad.project.json
```

## Repository structure

- `include/blcad/`: public headers
- `src/`: core and optional geometry implementation
- `tests/`: Catch2 tests
- `examples/`: `.blcad.json` / `.blcad.project.json` example models and headless examples
- `docs/`: architecture, MVP, and roadmap documents

## Key documents

Start here:

- `docs/project-goal.md`
- `docs/architecture-summary.md`
- `docs/mvp-plan.md`
- `docs/development-setup.md`
- `docs/json-serialization-mvp1.md`
- `docs/json-file-workflow-mvp1.md`

Implemented feature blocks:

- `docs/bolt-circle-pattern-mvp3.md`
- `docs/assembly-parameters-mvp4.md`
- `docs/project-container-mvp4.md`
- `docs/component-instance-mvp5.md`
- `docs/general-closed-sketch-profile-mvp.md`
- `docs/composite-closed-profile-holes-mvp.md`
- `docs/arc-and-trim-extend-sketch-profile-mvp.md`
- `docs/spline-and-tangent-continuity-mvp.md`
- `docs/sketch-plane-extrude-direction-mvp.md`
- `docs/derived-workplane-mvp2-seed.md`
- `docs/workplane-resolver-mvp2.md`
- `docs/construction-geometry-mvp.md`
- `docs/projected-sketch-reference-geometry.md`
- `docs/reference-recovery-mvp.md`
- `docs/reference-generated-profile-helpers-mvp.md`
- `docs/sketch-constraints-and-dimensions-mvp.md`
- `docs/automatic-profile-region-detection-mvp.md`
- `docs/sketch-solver-diagnostics-mvp.md`
- `docs/sketch-repair-suggestions-mvp.md`
- `docs/sketch-repair-commands-mvp.md`
- `docs/sketch-repair-transactions-mvp.md`
- `docs/sketch-repair-undo-stack-mvp.md`
- `docs/sketch-repair-undo-stack-summary-mvp.md`
- `docs/sketch-repair-command-labels-mvp.md`
- `docs/sketch-repair-presentation-metadata-mvp.md`
- `docs/sketch-repair-presentation-snapshot-mvp.md`
- `docs/sketch-repair-presentation-snapshot-query-mvp.md`

Future roadmaps:

- `docs/multi-body-transform-and-path-features-roadmap.md`
- `docs/inventor-like-sketcher-and-feature-roadmap.md`
- `docs/advanced-surfacing-and-3d-sketch-mvp.md`

## Next technical step

The next technical step should add the first solver-independent assembly constraint model-intent records on semantic component targets.

1. Add a typed `AssemblyConstraintId` and an `AssemblyConstraintType` limited to Mate, Concentric, and Distance.
2. Add a semantic component-target record that combines an existing `ComponentInstanceId` with a persistent semantic reference token; never store raw OCCT face, edge, or vertex ids.
3. Add `AssemblyConstraint` records with stable id, name, type, target A, target B, active state, and a distance value only for Distance constraints.
4. Let `AssemblyDocument` own constraints and validate unique constraint ids, existing component instance targets, non-empty semantic reference tokens, and type-specific distance requirements.
5. Keep constraint creation and JSON loading read-only with respect to component transforms: no placement solving, constraint inference, DOF computation, or grounding enforcement in this block.
6. Persist optional `assembly_constraints` through assembly/project JSON while keeping older files without the field loadable.
7. Add core and project tests for invalid targets, duplicate ids, Mate/Concentric/Distance roundtrip, unchanged free-placement transforms, shared part ownership, and valid project structure.
8. Extend `blcad_inspect_project_components` or add a focused headless constraint inspector so stored constraint type and semantic targets are observable from a project file.
9. Keep geometric semantic-reference resolution, constraint graph construction, rigid-body solving, remaining DOF display, Insert/Angle/Tangent constraints, collision checks, subassemblies, and assembly-level STEP export deferred.

The completed component-instance placement/state update block is documented in `docs/component-instance-mvp5.md`.

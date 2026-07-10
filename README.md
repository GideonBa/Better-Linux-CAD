# BLCAD

BLCAD is a planned independent parametric CAD system for Linux. The project stores CAD model intent in BLCAD data structures and uses OCCT/Open CASCADE as the computed geometry kernel cache, not as the primary model.

Detailed architecture and feature status live in `docs/`. This README is intentionally kept short.

## Status

Current state: MVP-1 core skeleton, staged MVP-2 seeds for sketches, workplanes, profile geometry, recompute, STEP export, reference recovery, sketch diagnostics, and repair-command infrastructure, the MVP-3 parametric bolt circle (count parameters and `CircularHolePattern`), and the MVP-4 seed for assembly parameters shared across parts (`AssemblyDocument` and `ParameterBinding`).

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

## Headless STEP export

```text
blcad_export_step <input.blcad.json> <output.step>
```

Examples:

```bash
./build/dev-geometry/blcad_export_step examples/reference_plate.blcad.json build/reference_plate.step
./build/dev-geometry/blcad_export_step examples/bolt_circle_plate.blcad.json build/bolt_circle_plate.step
```

## Repository structure

- `include/blcad/`: public headers
- `src/`: core and optional geometry implementation
- `tests/`: Catch2 tests
- `examples/`: `.blcad.json` example models and headless export example
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

The sketch-repair presentation chain is frozen until a GUI or CLI consumer exists (see the frozen block in `docs/mvp-plan.md`). Development follows the numbered core-CAD MVP sequence.

The next technical step is a project container for the assembly and its member parts.

1. Add a `Project` model owning one `AssemblyDocument` and its member `PartDocument`s.
2. Validate that every assembly member id resolves to an owned part document.
3. Auto-apply parameter bindings to affected member parts after an assembly parameter change.
4. Surface per-part recompute plans from one project-level update call.
5. Add JSON persistence for the project (embedded documents or a manifest referencing part files).
6. Extend the headless example: load a project, update an assembly parameter, recompute, export all member parts to STEP.
7. Add tests for membership validation, automatic propagation, per-part invalidation, JSON roundtrip, and headless export.
8. Keep component instances and constraints out of scope (MVP 5, `docs/assembly-system.md`).

The completed MVP-3 bolt circle block is documented in `docs/bolt-circle-pattern-mvp3.md`. The completed MVP-4 seed is documented in `docs/assembly-parameters-mvp4.md`.
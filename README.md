# BLCAD

BLCAD is a planned independent parametric CAD system for Linux. The project stores CAD model intent in BLCAD data structures and uses OCCT/Open CASCADE as the computed geometry kernel cache, not as the primary model.

Detailed architecture and feature status live in `docs/`. This README is intentionally kept short.

## Status

Current state: MVP-1 core skeleton plus staged MVP-2 seeds for sketches, workplanes, profile geometry, recompute, STEP export, reference recovery, sketch diagnostics, and repair-command infrastructure.

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

Example:

```bash
./build/dev-geometry/blcad_export_step examples/reference_plate.blcad.json build/reference_plate.step
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

Future roadmaps:

- `docs/multi-body-transform-and-path-features-roadmap.md`
- `docs/inventor-like-sketcher-and-feature-roadmap.md`
- `docs/advanced-surfacing-and-3d-sketch-mvp.md`

## Next technical step

The next technical step should add a sketch repair presentation metadata seed.

1. Add a `SketchRepairPresentationMetadata` or equivalent read-only record with stable machine-readable label IDs.
2. Add display categories such as `safe_apply`, `requires_user_choice`, `requires_parameter_value`, and `undo_entry`.
3. Add a lightweight severity or display-priority enum for repair actions and undo entries.
4. Compute affected-record counts for added constraints, removed constraints, and removed dimensions.
5. Expose a concise `affected_summary` string for undo-stack summary entries without changing undo semantics.
6. Include the metadata fields in undo-stack summary debug JSON next to the existing title and description.
7. Keep all presentation metadata separate from model intent and `.blcad.json` persistence.
8. Add core tests and documentation for stable IDs, categories, priorities, affected counts, summaries, and JSON output.
9. Keep localization, icons, GUI widgets, redo, persistent journals, timestamps, multi-sketch grouping, parameter-creating repairs, full solve iteration, exact DOF counting, and arbitrary model rewriting deferred.

The completed command label block is documented in `docs/sketch-repair-command-labels-mvp.md`.

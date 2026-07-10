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
- `docs/sketch-repair-presentation-metadata-mvp.md`
- `docs/sketch-repair-presentation-snapshot-mvp.md`

Future roadmaps:

- `docs/multi-body-transform-and-path-features-roadmap.md`
- `docs/inventor-like-sketcher-and-feature-roadmap.md`
- `docs/advanced-surfacing-and-3d-sketch-mvp.md`

## Next technical step

The next technical step should add a sketch repair presentation snapshot query seed.

1. Add a `SketchRepairPresentationSnapshotQuery` or equivalent read-only filter/options record.
2. Add a `SketchRepairPresentationSnapshotQueryEngine` that consumes a snapshot and returns a filtered snapshot without mutating the original.
3. Support filtering by display category, display priority, latest-only, and undoable-only.
4. Preserve the original stack order for all filtered outputs.
5. Add optional count helpers for category totals and priority totals so CLI/GUI code can show compact badges.
6. Add debug JSON output for queried snapshots with a distinct schema marker and query metadata.
7. Keep the unfiltered snapshot JSON intact as the default presentation export.
8. Add core tests for empty queries, category filtering, priority filtering, latest-only filtering, undoable-only filtering, preserved ordering, count helpers, and JSON output.
9. Keep localization, icons, GUI widgets, custom sorting, persistent history, redo, multi-sketch grouping, timestamps, parameter-creating repairs, full solve iteration, exact DOF counting, and arbitrary model rewriting deferred.

The completed presentation snapshot block is documented in `docs/sketch-repair-presentation-snapshot-mvp.md`.

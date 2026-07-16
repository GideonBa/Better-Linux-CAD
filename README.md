# BLCAD

BLCAD will be an independent, headless-first parametric CAD system for Linux with an optional
desktop UI, in active development.

All architecture, feature contracts, persistence semantics, roadmaps, and current implementation status
live in [`docs/`](docs/). This README is intentionally kept small and evergreen — it is not a status
page.

The assembly sequence is implemented through Block 47, Part Construction MVP-6 is complete through
Block 94, GUI Feature Validation MVP-7 is accepted through Block 105, and Interactive Sketcher MVP-8
is accepted through Block 121. Blocks 106–120 implement the contextual Sketch workspace,
device-independent plane interaction, stable shared topology, deterministic planar solving, semantic
handle dragging, planar creation and editing, constraints and dimensions, reference-aware modify and
pattern tools, automatic regions and Finish Sketch, and direct Sketch3D point/line interaction.

Block 121 closes MVP-8 with a machine-readable coverage manifest for all fifteen capability families,
two deterministic tutorial documents, eight mandatory acceptance-evidence categories, and frozen
representative hover, drag, solve, and region-recognition performance budgets. The manifest is compiled
and verified by the registered Core test target; wall-clock budgets remain dedicated benchmark metadata
rather than nondeterministic ordinary-CI assertions.

Canonical Interactive Sketcher contracts include
[`docs/gui-interactive-sketch-workspace-mvp8.md`](docs/gui-interactive-sketch-workspace-mvp8.md),
[`docs/gui-sketch-plane-interaction-mvp8.md`](docs/gui-sketch-plane-interaction-mvp8.md),
[`docs/sketch-shared-topology-mvp8.md`](docs/sketch-shared-topology-mvp8.md),
[`docs/sketch-planar-constraint-solver-mvp8.md`](docs/sketch-planar-constraint-solver-mvp8.md),
[`docs/gui-sketch-solver-drag-mvp8.md`](docs/gui-sketch-solver-drag-mvp8.md),
[`docs/gui-sketch-basic-creation-mvp8.md`](docs/gui-sketch-basic-creation-mvp8.md),
[`docs/gui-sketch-conic-slot-creation-mvp8.md`](docs/gui-sketch-conic-slot-creation-mvp8.md),
[`docs/gui-sketch-spline-text-mvp8.md`](docs/gui-sketch-spline-text-mvp8.md),
[`docs/gui-sketch-constraint-authoring-mvp8.md`](docs/gui-sketch-constraint-authoring-mvp8.md),
[`docs/gui-sketch-dimension-authoring-mvp8.md`](docs/gui-sketch-dimension-authoring-mvp8.md),
[`docs/gui-sketch-modify-mvp8.md`](docs/gui-sketch-modify-mvp8.md),
[`docs/gui-sketch-offset-project-mvp8.md`](docs/gui-sketch-offset-project-mvp8.md),
[`docs/gui-sketch-transform-pattern-mvp8.md`](docs/gui-sketch-transform-pattern-mvp8.md),
[`docs/gui-sketch-regions-finish-mvp8.md`](docs/gui-sketch-regions-finish-mvp8.md),
[`docs/gui-sketch3d-interaction-mvp8.md`](docs/gui-sketch3d-interaction-mvp8.md), and
[`docs/interactive-sketcher-acceptance-mvp8.md`](docs/interactive-sketcher-acceptance-mvp8.md).

The optional Qt desktop covers document, Sketch, Part, Surface, Assembly, motion, analysis, and
STEP-export validation workflows without moving authority out of Core/Geometry. Interactive Part &
Assembly Modeling MVP-9 begins at Block 122 in
[`docs/interactive-modeling-sequence-mvp9.md`](docs/interactive-modeling-sequence-mvp9.md); STEP Import
MVP-10 follows in Blocks 132–138.

## Repository layout

- [`include/blcad/`](include/blcad/) — public headers for Core model and optional Geometry/GUI boundaries
- [`src/`](src/) — Core and optional Geometry/GUI implementation
- [`tests/`](tests/) — Catch2 tests
- [`examples/`](examples/) — `.blcad.json` / `.blcad.project.json` examples and headless flows
- [`docs/`](docs/) — architecture, implemented contracts, sequences, and roadmaps

## Documentation

Start here:

- [`docs/project-goal.md`](docs/project-goal.md) — what BLCAD is and why
- [`docs/architecture-summary.md`](docs/architecture-summary.md) — layered architecture and authority model
- [`docs/mvp-plan.md`](docs/mvp-plan.md) — implementation sequence and current status
- [`docs/development-setup.md`](docs/development-setup.md) — building, testing, and focused test tags
- [`docs/file-format.md`](docs/file-format.md) — save-format authority
- [`docs/interactive-sketcher-sequence-mvp8.md`](docs/interactive-sketcher-sequence-mvp8.md) — Blocks 106–121 sequence and explicit deferrals
- [`docs/interactive-sketcher-acceptance-mvp8.md`](docs/interactive-sketcher-acceptance-mvp8.md) — Block-121 coverage, evidence, tutorials, and performance budgets
- [`docs/tutorials/interactive-sketcher-planar-mvp8.md`](docs/tutorials/interactive-sketcher-planar-mvp8.md) — deterministic planar acceptance tutorial
- [`docs/tutorials/interactive-sketcher-sketch3d-mvp8.md`](docs/tutorials/interactive-sketcher-sketch3d-mvp8.md) — deterministic Sketch3D acceptance tutorial

## License

See [`LICENSE`](LICENSE).

# BLCAD

BLCAD will be an independent, headless-first parametric CAD system for Linux with an optional
desktop UI, in active development.

All architecture, feature contracts, persistence semantics, roadmaps, and current implementation status live in [`docs/`](docs/). This README is intentionally kept small and evergreen — it is not a status page.

The assembly sequence is implemented through Block 47, Part Construction MVP-6 is complete through Block 94, and GUI Feature Validation MVP-7 is accepted through Block 105. Interactive Sketcher MVP-8 is in progress: Blocks 106–108 implement the contextual Sketch workspace, device-independent plane interaction, deterministic hit/selection/grid/snap/inference behavior, and shared planar point/entity topology with stable `SketchPointId`, migration, dependency-safe edit commands, exact topology undo/redo, and canonical topology JSON. Their implemented contracts are documented in [`docs/gui-interactive-sketch-workspace-mvp8.md`](docs/gui-interactive-sketch-workspace-mvp8.md), [`docs/gui-sketch-plane-interaction-mvp8.md`](docs/gui-sketch-plane-interaction-mvp8.md), and [`docs/sketch-shared-topology-mvp8.md`](docs/sketch-shared-topology-mvp8.md); Block 109, the deterministic general planar constraint solver, is next in [`docs/interactive-sketcher-sequence-mvp8.md`](docs/interactive-sketcher-sequence-mvp8.md). The optional Qt desktop covers document, Sketch, Part, Surface, Assembly, motion, analysis, and STEP-export workflows without moving authority out of Core/Geometry. Blocks 122–131 plan Interactive Part & Assembly Modeling in [`docs/interactive-modeling-sequence-mvp9.md`](docs/interactive-modeling-sequence-mvp9.md); STEP Import MVP-10 follows in Blocks 132–138.

## Repository layout

- [`include/blcad/`](include/blcad/) — public headers (Core model and the optional Geometry layer)
- [`src/`](src/) — Core and optional Geometry implementation
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
- [`docs/sketch-shared-topology-mvp8.md`](docs/sketch-shared-topology-mvp8.md) — Block-108 shared planar topology and migration contract

## License

See [`LICENSE`](LICENSE).

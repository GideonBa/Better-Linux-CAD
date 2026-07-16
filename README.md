# BLCAD

BLCAD will be an independent, headless-first parametric CAD system for Linux with an optional
desktop UI, in active development.

All architecture, feature contracts, persistence semantics, roadmaps, and current implementation status
live in [`docs/`](docs/). This README is intentionally kept small and evergreen — it is not a status
page.

The assembly sequence is implemented through Block 47, Part Construction MVP-6 is complete through
Block 94, and GUI Feature Validation MVP-7 is accepted through Block 105. Interactive Sketcher MVP-8
is in progress: Blocks 106–114 implement the contextual Sketch workspace, device-independent plane
interaction, stable shared `SketchPointId` topology, deterministic planar solving with local
DOF/conflict diagnostics, solver-backed semantic-handle dragging, line/conic/slot creation, spline
editing, parameter-backed Sketch text, and manual/automatic geometric constraint authoring.

Block 114 adds stable point/entity constraint targets, selection-driven family compatibility, automatic
snap-to-constraint candidates, disposable conflict/redundancy preview, semantic clickable glyphs,
canonical `blcad.sketch_constraints.mvp8` sidecar persistence, and coordinated atomic Sketch/catalog
undo and redo. Dimension value and expression workflows remain Block 115.

Canonical Interactive Sketcher contracts include
[`docs/gui-interactive-sketch-workspace-mvp8.md`](docs/gui-interactive-sketch-workspace-mvp8.md),
[`docs/gui-sketch-plane-interaction-mvp8.md`](docs/gui-sketch-plane-interaction-mvp8.md),
[`docs/sketch-shared-topology-mvp8.md`](docs/sketch-shared-topology-mvp8.md),
[`docs/sketch-planar-constraint-solver-mvp8.md`](docs/sketch-planar-constraint-solver-mvp8.md),
[`docs/gui-sketch-solver-drag-mvp8.md`](docs/gui-sketch-solver-drag-mvp8.md),
[`docs/gui-sketch-basic-creation-mvp8.md`](docs/gui-sketch-basic-creation-mvp8.md),
[`docs/gui-sketch-conic-slot-creation-mvp8.md`](docs/gui-sketch-conic-slot-creation-mvp8.md),
[`docs/gui-sketch-spline-text-mvp8.md`](docs/gui-sketch-spline-text-mvp8.md), and
[`docs/gui-sketch-constraint-authoring-mvp8.md`](docs/gui-sketch-constraint-authoring-mvp8.md). Block
115, dimensions and expression-backed in-canvas editing, is next in
[`docs/interactive-sketcher-sequence-mvp8.md`](docs/interactive-sketcher-sequence-mvp8.md).

The optional Qt desktop covers document, Sketch, Part, Surface, Assembly, motion, analysis, and
STEP-export validation workflows without moving authority out of Core/Geometry. Blocks 122–131 plan
Interactive Part & Assembly Modeling in
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
- [`docs/sketch-shared-topology-mvp8.md`](docs/sketch-shared-topology-mvp8.md) — shared planar topology/migration
- [`docs/sketch-planar-constraint-solver-mvp8.md`](docs/sketch-planar-constraint-solver-mvp8.md) — planar solver/DOF/diagnostics
- [`docs/gui-sketch-solver-drag-mvp8.md`](docs/gui-sketch-solver-drag-mvp8.md) — semantic handles/live solver/atomic drag
- [`docs/gui-sketch-spline-text-mvp8.md`](docs/gui-sketch-spline-text-mvp8.md) — spline editing and Sketch text
- [`docs/gui-sketch-constraint-authoring-mvp8.md`](docs/gui-sketch-constraint-authoring-mvp8.md) — constraint intent, inference, glyphs, and conflict preview

## License

See [`LICENSE`](LICENSE).

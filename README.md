# BLCAD

BLCAD will be an independent, headless-first parametric CAD system for Linux with an optional
desktop UI, in active development.

All architecture, feature contracts, persistence semantics, roadmaps, and current implementation status
live in [`docs/`](docs/). This README is intentionally kept small and evergreen — it is not a status
page.

The assembly sequence is implemented through Block 47, Part Construction MVP-6 is complete through
Block 94, and GUI Feature Validation MVP-7 is accepted through Block 105. Interactive Sketcher MVP-8
is in progress: Blocks 106–116 implement the contextual Sketch workspace, device-independent plane
interaction, stable shared `SketchPointId` topology, deterministic planar solving with local
DOF/conflict diagnostics, solver-backed semantic-handle dragging, line/conic/slot creation, spline
editing, parameter-backed Sketch text, geometric constraint authoring, typed driving/reference
Sketch dimensions, and trim/extend/split/fillet/chamfer geometry modification.

Block 115 adds horizontal, vertical, aligned, point-to-point, line-length, arc-radius, arc-diameter,
angle, and arc-length dimensions; explicit driving/reference semantics; typed Length/Angle parameter
and expression binding; deterministic semantic in-canvas annotations; canonical
`blcad.sketch_dimensions.mvp8` sidecar persistence; combined constraint/dimension drag enforcement; and
atomic parameter/Sketch/catalog undo and redo. Block 116 adds trim, extend, split, two-line fillet,
and two-line chamfer that rewrite a disposable candidate Sketch and preserve or explicitly reject
affected profiles, constraints, dimensions, and continuity.

Canonical Interactive Sketcher contracts include
[`docs/gui-interactive-sketch-workspace-mvp8.md`](docs/gui-interactive-sketch-workspace-mvp8.md),
[`docs/gui-sketch-plane-interaction-mvp8.md`](docs/gui-sketch-plane-interaction-mvp8.md),
[`docs/sketch-shared-topology-mvp8.md`](docs/sketch-shared-topology-mvp8.md),
[`docs/sketch-planar-constraint-solver-mvp8.md`](docs/sketch-planar-constraint-solver-mvp8.md),
[`docs/gui-sketch-solver-drag-mvp8.md`](docs/gui-sketch-solver-drag-mvp8.md),
[`docs/gui-sketch-basic-creation-mvp8.md`](docs/gui-sketch-basic-creation-mvp8.md),
[`docs/gui-sketch-conic-slot-creation-mvp8.md`](docs/gui-sketch-conic-slot-creation-mvp8.md),
[`docs/gui-sketch-spline-text-mvp8.md`](docs/gui-sketch-spline-text-mvp8.md),
[`docs/gui-sketch-constraint-authoring-mvp8.md`](docs/gui-sketch-constraint-authoring-mvp8.md), and
[`docs/gui-sketch-dimension-authoring-mvp8.md`](docs/gui-sketch-dimension-authoring-mvp8.md), and
[`docs/gui-sketch-modify-mvp8.md`](docs/gui-sketch-modify-mvp8.md). Block 117, offset/projection and
associative references, is next in
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
- [`docs/gui-sketch-dimension-authoring-mvp8.md`](docs/gui-sketch-dimension-authoring-mvp8.md) — driving/reference dimensions, typed values, expressions, and in-canvas editing

## License

See [`LICENSE`](LICENSE).

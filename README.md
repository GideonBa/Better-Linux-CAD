# BLCAD

BLCAD will be an independent, headless-first parametric CAD system for Linux with an optional
desktop UI, in active development.

All architecture, feature contracts, persistence semantics, roadmaps, and current implementation status live in [`docs/`](docs/). This README is intentionally kept small and evergreen — it is not a status page.

The assembly sequence is implemented through Block 47, and Part Construction MVP-6 is complete through Block 94. It provides Body identity, body-scoped recompute/inspection, Body Booleans and transforms, reusable semantic Part-feature inputs, richer Extrude/Cut, Revolve, Pattern, Mirror, Fillet, Chamfer, Shell, Draft, model-space 3D Sketches, connected PathCurves, Sweep/SweepCut/SweepSurface, path-following Extrude/Cut, guided multi-section Loft, first Surface features, closed-shell-to-solid conversion, and deterministic multi-body STEP export. The integrated contract is [`docs/part-construction-mvp6-acceptance.md`](docs/part-construction-mvp6-acceptance.md). Blocks 95–99 implement the optional Qt shell, branded Archimedes startup splash, document lifecycle, OCCT viewport, synchronized browser/property editing, and the datum/workplane/planar-Sketch workbench with explicit repair; Blocks 100–105 extend it into a validation GUI for all implemented capabilities. [`docs/gui-feature-validation-sequence-mvp7.md`](docs/gui-feature-validation-sequence-mvp7.md) is canonical. Block 100 is next, and STEP Import follows in Blocks 106–112.

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

## License

See [`LICENSE`](LICENSE).

# BLCAD

BLCAD will be an independent, headless parametric CAD system for Linux, in active development.

All architecture, feature contracts, persistence semantics, roadmaps, and current implementation status live in [`docs/`](docs/). This README is intentionally kept small and evergreen — it is not a status page.

The assembly sequence is implemented through Block 47. Part Construction Blocks 48–88 add Body identity, body-scoped recompute/inspection, Body Booleans, associative Body transforms, reusable semantic Part-feature inputs, richer Extrude/Cut intent plus Geometry, persistent plus executed Revolve/RevolveCut, general Pattern and Mirror Geometry, persistent plus executed Fillet/Chamfer/Shell/Draft Geometry, persistent model-space 3D Sketch Geometry, reusable connected semantic PathCurves, Sweep/SweepCut/SweepSurface through spatial paths, twist, and guide control, path-following Extrude/Extruded Cut, persistent plus executed path/guide-controlled multi-section Loft Geometry, and the first persistent Surface-feature family. The current contract is [`docs/part-surface-feature-intent-mvp6.md`](docs/part-surface-feature-intent-mvp6.md). Block 89 Boundary and Fill Surface Geometry is the next technical step.

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

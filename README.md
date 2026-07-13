# BLCAD

BLCAD will be an independent, headless parametric CAD system for Linux, in active development.

All architecture, feature contracts, persistence semantics, roadmaps, and current implementation status live in [`docs/`](docs/). This README is intentionally kept small and evergreen — it is not a status page.

The assembly sequence is implemented through Block 47. The canonical final Assembly-MVP contract is [`docs/assembly-spherical-joint-mvp5.md`](docs/assembly-spherical-joint-mvp5.md). Block 48 — Body identity and `PartDocument` ownership — is the next technical step in the Part Construction sequence.

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

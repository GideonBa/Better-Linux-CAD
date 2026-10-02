# BLCAD

BLCAD will be an independent, headless-first parametric CAD system for Linux with an optional
desktop UI, in active development.

BLCAD owns semantic, parametric model intent; OCCT/Open CASCADE is the computed geometry and
data-exchange kernel. Documents persist intent and stable semantic references rather than raw BRep
or OCCT topology identity, so a model stays editable across recomputes.

All architecture, feature contracts, persistence semantics, roadmaps, and current implementation
status live in [`docs/`](docs/). This README is intentionally kept small and evergreen — it is not a
status page. For what is implemented and what comes next, read
[`docs/mvp-plan.md`](docs/mvp-plan.md), which is the single implementation-status authority.

## Repository layout

- [`include/blcad/`](include/blcad/) — public headers for the Core model and the optional
  Geometry/GUI boundaries
- [`src/`](src/) — Core and optional Geometry/GUI implementation
- [`tests/`](tests/) — Catch2 tests
- [`examples/`](examples/) — `.blcad.json` / `.blcad.project.json` examples and headless flows
- [`docs/`](docs/) — architecture, implemented contracts, sequences, and roadmaps
- [`docs/decisions/`](docs/decisions/) — architecture decision records

## Build and test

Three build configurations exist: Core only, Core + Geometry, and Core + Geometry + the optional Qt
GUI. [`docs/development-setup.md`](docs/development-setup.md) is authoritative for dependencies,
presets, focused test tags, and the headless tools.

```bash
cmake --workflow --preset dev-build-test           # Core
cmake --workflow --preset dev-geometry-build-test  # Core + Geometry
cmake --workflow --preset dev-gui-build-test       # Core + Geometry + Qt GUI (offscreen tests)
```

## Documentation

Start here:

- [`docs/project-goal.md`](docs/project-goal.md) — what BLCAD is and why
- [`docs/architecture-summary.md`](docs/architecture-summary.md) — layered architecture and authority
  model
- [`docs/mvp-plan.md`](docs/mvp-plan.md) — implementation sequence and current status
- [`docs/development-setup.md`](docs/development-setup.md) — building, testing, and focused test tags
- [`docs/file-format.md`](docs/file-format.md) — save-format authority
- [`docs/user-interface.md`](docs/user-interface.md) — UI architecture and interaction authority

Per-phase sequence contracts, each canonical for its block range:

- [`docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`](docs/assembly-cross-hierarchy-solver-sequence-mvp5.md)
  — Assembly system (MVP-5)
- [`docs/part-construction-sequence-mvp6.md`](docs/part-construction-sequence-mvp6.md) — Part
  Construction (MVP-6)
- [`docs/gui-feature-validation-sequence-mvp7.md`](docs/gui-feature-validation-sequence-mvp7.md) —
  GUI Feature Validation (MVP-7)
- [`docs/interactive-sketcher-sequence-mvp8.md`](docs/interactive-sketcher-sequence-mvp8.md) —
  Interactive Sketcher (MVP-8)
- [`docs/interactive-modeling-sequence-mvp9.md`](docs/interactive-modeling-sequence-mvp9.md) —
  Interactive Part & Assembly Modeling (MVP-9)
- [`docs/gui-shell-reset-sequence-mvp9r.md`](docs/gui-shell-reset-sequence-mvp9r.md) — GUI Shell
  Reset (MVP-9R)
- [`docs/step-import-sequence-mvp10.md`](docs/step-import-sequence-mvp10.md) — STEP Import (MVP-10)

Feature-specific documents under [`docs/`](docs/) remain canonical for exact mathematics, persistence
spellings, migration rules, ordering, and failure policy. Each states its own implementation status.

## License

See [`LICENSE`](LICENSE).

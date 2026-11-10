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

For a repository task, read `AGENTS.md`, then the current status in
[`mvp-plan.md`](docs/mvp-plan.md). Follow its link to the affected contract and inspect the matching
headers, implementation and tests. Read the future sequence/test plan when implementing planned
work; use the setup guide for commands. There is no need to load all documentation as context.

Choose additional context by task:

- [`docs/project-goal.md`](docs/project-goal.md) — what BLCAD is and why
- [`docs/architecture-summary.md`](docs/architecture-summary.md) — layered architecture and authority
  model
- [`docs/mvp-plan.md`](docs/mvp-plan.md) — implementation sequence and current status
- [`docs/development-setup.md`](docs/development-setup.md) — building, testing, and focused test tags
- [`docs/file-format.md`](docs/file-format.md) — save-format authority
- [`docs/user-interface.md`](docs/user-interface.md) — UI architecture and interaction authority
- [`docs/inventor-parity-sequence.md`](docs/inventor-parity-sequence.md) — planned advanced modeling blocks and existing contract ownership
- [`docs/inventor-parity-test-plan.md`](docs/inventor-parity-test-plan.md) — planned feature, combination, native UI and performance acceptance

The completed-block index in [`mvp-plan.md`](docs/mvp-plan.md) links the phase and feature owners.
Feature-specific documents remain canonical for exact mathematics, persistence spellings, migration,
ordering and failure policy. Use their test tags to find executable proofs. Superseded implementation
logs are available in Git/PR history; unimplemented plans and their acceptance requirements remain
in the working documentation.

## License

See [`LICENSE`](LICENSE).

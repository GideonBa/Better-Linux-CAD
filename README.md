# BLCAD

BLCAD will be an independent, headless-first parametric CAD system for Linux with an optional
desktop UI, in active development.

All architecture, feature contracts, persistence semantics, roadmaps, and current implementation status
live in [`docs/`](docs/). This README is intentionally kept small and evergreen — it is not a status
page.

The assembly sequence is implemented through Block 47, Part Construction MVP-6 is complete through
Block 94, GUI Feature Validation MVP-7 is accepted through Block 105, Interactive Sketcher MVP-8 is
accepted through Block 121, and Interactive Part & Assembly Modeling MVP-9 is active through Block
125. Blocks 106–120 implement the contextual Sketch workspace, device-independent plane interaction,
stable shared `SketchPointId` topology, deterministic planar solving with local DOF/conflict
diagnostics, solver-backed semantic-handle dragging, line/conic/slot creation, spline editing,
parameter-backed Sketch text, geometric constraint authoring, typed driving/reference Sketch
dimensions, trim/extend/split/fillet/chamfer modification, line-chain/loop offset, associative
projection/include, construction references, explicit break-link conversion, selected-geometry
move/rotate/scale/copy/mirror, rectangular/circular Sketch patterns, multi-region recognition,
profile selection, contour diagnostics, atomic Finish Sketch candidates, and direct Sketch3D point/
line placement and manipulation.

Block 119 adds connected-component region recognition over solved line geometry, stable
open/self-crossing/ambiguous diagnostics, point-based profile selection, and fail-closed Finish Sketch
materialization. Block 120 adds orthogonal axis/plane locks, typed XYZ and distance/angle placement,
atomic 3D point/line creation, shared-point handle editing, persistent guide roles, and explicit
planar-Sketch point projection source intent without introducing a full 3D variational solver.
Block 122 adds the shared selection-first Part/Surface/Assembly workspace authority: capability-exact
preselection, deterministic contextual command recommendations, Finish-Sketch handoff, command
repeat, synchronized selection filters, and transient ViewCube/home/bookmark navigation. Block 123
adds reusable candidate-only linear, angular, radial, translate/rotate-triad, and pattern handles with
model-space mapping, fixed-DIP hit testing, exact final release, and bidirectional numeric-HUD coupling.

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
[`docs/gui-sketch-regions-finish-mvp8.md`](docs/gui-sketch-regions-finish-mvp8.md), and
[`docs/gui-sketch3d-interaction-mvp8.md`](docs/gui-sketch3d-interaction-mvp8.md). Block 121 adds the
machine-checked coverage manifest, GUI/headless equivalence, atomic interaction checks, and measured
performance acceptance in
[`docs/interactive-sketcher-mvp8-acceptance.md`](docs/interactive-sketcher-mvp8-acceptance.md).

The optional Qt desktop covers document, Sketch, Part, Surface, Assembly, motion, analysis, and
STEP-export validation workflows without moving authority out of Core/Geometry. Block 122 starts
Interactive Part & Assembly Modeling with the selection-first workspace contract in
[`docs/gui-modeling-workspace-mvp9.md`](docs/gui-modeling-workspace-mvp9.md). Block 123 implements the
shared manipulator and numeric-HUD infrastructure in
[`docs/gui-viewport-manipulators-mvp9.md`](docs/gui-viewport-manipulators-mvp9.md). Block 124
implements interactive Extrude, path Extrude, and Revolve authoring in
[`docs/gui-interactive-extrude-revolve-mvp9.md`](docs/gui-interactive-extrude-revolve-mvp9.md).
Block 125 implements interactive Fillet, Chamfer, Shell, and Draft authoring in
[`docs/gui-interactive-finishing-mvp9.md`](docs/gui-interactive-finishing-mvp9.md).
Block 126, interactive Pattern/Mirror/Boolean/Transform, is next in
[`docs/interactive-modeling-sequence-mvp9.md`](docs/interactive-modeling-sequence-mvp9.md); STEP
Import MVP-10 follows in Blocks 132–138.

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
- [`docs/gui-sketch-offset-project-mvp8.md`](docs/gui-sketch-offset-project-mvp8.md) — offset, associative projection/include, and break-link semantics
- [`docs/gui-sketch-transform-pattern-mvp8.md`](docs/gui-sketch-transform-pattern-mvp8.md) — selected-geometry transforms, mirror, and pattern intent
- [`docs/gui-sketch-regions-finish-mvp8.md`](docs/gui-sketch-regions-finish-mvp8.md) — regions, profile selection, diagnostics, and Finish Sketch
- [`docs/gui-sketch3d-interaction-mvp8.md`](docs/gui-sketch3d-interaction-mvp8.md) — axis/plane locks, typed 3D placement, handles, guides, and projection
- [`docs/gui-modeling-workspace-mvp9.md`](docs/gui-modeling-workspace-mvp9.md) — selection-first modeling commands, filters, handoff, repeat, and transient navigation
- [`docs/gui-viewport-manipulators-mvp9.md`](docs/gui-viewport-manipulators-mvp9.md) — reusable candidate-only handles, mapping, hit testing, and numeric coupling

## License

See [`LICENSE`](LICENSE).

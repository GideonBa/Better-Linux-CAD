# Replacement shell acceptance — MVP-9R

Block 136 was accepted on **2026-10-25**: all eighteen bounded families restored, no family gaps
or pending integrated acceptance workflows. This is a completed milestone, not the repository's
current implementation boundary. `docs/mvp-plan.md` owns numbered status; `docs/user-interface.md`
owns the current UI layout. Later feature contracts may extend this accepted scope.

Use this file for the shell evidence rules, baseline regression filter and final dated acceptance.
Use `docs/gui-feature-coverage-manifest-mvp9.json` for current family owners, contracts, entry points
and exact test/source mappings; use `docs/development-setup.md` for build/native execution guidance.
Superseded per-slice progress, repeated test totals and intermediate failures are available in Git
history; they are not additional implementation tasks.

## Current evidence and historical acceptance

`docs/gui-feature-coverage-manifest-mvp9.json` uses schema `blcad.gui-feature-coverage.v3`.
Its `implemented_through: 131`, `acceptance_contract`, `validation_owner` and `interactive` records
preserve the accepted MVP-9 history. Those records describe the pre-reset architecture. Their tags
may have been deleted, partially reused by replacement tests, or retained only for headless
controllers. They are not current proof of a working ShellWindow entry point.

Each of the eighteen accepted MVP-7 families now has a separate `shell` record with owner, contract,
bounded scope, current proofs, action/widget entry points and explicit gaps. `restored` means the
recorded bounded command surface has shell regression coverage; it does not enlarge the geometry
contract or certify the entire block. `partial` requires at least one concrete gap. A headless API
test cannot stand in for a current shell proof. Existing headless acceptance and performance tests
remain useful independent authority checks.

`[integration][gui-feature-coverage]` / `[gui][shell-coverage]` checks:

- Exactly the accepted eighteen families, without missing or duplicate entries; preserved validation
  owners and historical interactive dispositions/reasons.
- Existing repository contract paths and a nonempty bounded scope for each current record.
- Every current positive tag filter against Catch's actual registered test metadata, including its
  exact shell test source. Each filter must match tests, and every match must come from that source.
  Comma-OR alternatives must be separate proofs, so a surviving arm cannot hide a deleted tag.
- Every listed action has a unique object identity and belongs to a direct ribbon button, its menu
  or the explicitly owned shared context toolbar; every listed widget belongs to that window.
  The ViewCube workflow exercises actual menu input. This is a reachability inventory, not a
  substitute for executing the interaction regressions.
- Gap/status consistency, exact blocked-family accounting and completion derived from remaining
  family gaps plus pending integrated workflows. Accepted shell completion must be covered by the
  plan's numbered boundary. Later phases may advance the plan without rewriting this completed
  Block-136 milestone; an unfinished shell record still requires its exact current boundary.

Negative controls deliberately remove/duplicate families, use a deleted tag, substitute a headless
source, hide a missing tag behind OR and claim completion with gaps. They must all be rejected.

## Accepted bounded coverage

`shell_reset_progress.acceptance` records `complete: true`, with empty `blocked_families` and
`pending_workflows`. The manifest's `families[].shell` records supply the detailed inventory;
`workflow_program` tracks later capabilities separately. A restored shell surface inherits its
feature's bounds. It does not imply general geometry support or implementation of later plans.

| Accepted surface | Contract to read before changing behavior |
| --- | --- |
| Document/session ownership, viewport lifecycle and shell wiring | `docs/gui-shell-reset-sequence-mvp9r.md` |
| Construction/workplanes, diagnostic and single/batch reference repair | `docs/gui-sketch-workbench-mvp7.md`, `docs/reference-recovery-mvp.md` |
| Sketch3D points, shared-point chains, collection/grips, anchored and distance/direction placement, independent planar-point copies | `docs/gui-sketch3d-interaction-mvp8.md` |
| Part parameters and expressions | `docs/parameter-model.md`, `docs/parameter-expression-mvp.md` |
| Seven Extrude terminations, four Body operations, path/single-line Thin Extrude and expanded Revolve | `docs/gui-interactive-extrude-revolve-mvp9.md` |
| Legacy single/bolt-circle through-hole task | `docs/gui-part-foundation-workbench-mvp7.md`; semantic Hole/Thread ownership is separate in `docs/hole-wizard.md` |
| Finishing, Body patterns/transforms, Sweep/Loft and Surface tasks | `docs/gui-interactive-finishing-mvp9.md`, `docs/gui-interactive-pattern-body-mvp9.md`, `docs/gui-interactive-path-sweep-loft-mvp9.md`, `docs/gui-interactive-surface-mvp9.md` |
| Navigation, filters, repeat and typed context handoff | `docs/gui-modeling-workspace-mvp9.md` |
| Feature editing, Measure, Assembly and analysis/STEP consumers | `docs/gui-feature-edit-mvp9.md`, `docs/gui-measure-mvp9.md`, `docs/gui-interactive-assembly-mvp9.md`, `docs/gui-analysis-step-export-workbench-mvp7.md` |

Integrated proof lives in `tests/gui/gui_shell_combined_workflow_tests.cpp`, under
`[shell-combined-workflow]` and its scenario tags. It combines real Qt input with independent
Geometry oracles, exact/branching Undo/Redo, canonical reopening, read-only consumers and cleanup.
The manifest identifies current proofs and any imported-fixture boundaries. The separate
`[shell-mixed-path-loft]` workflow opens prepared Sketch/plane inputs before authoring its path and
Loft through widgets. Retained `[integration][interactive-modeling]` exercises controller APIs and
must not be relabeled as replacement-shell acceptance.

## Verification and closure

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app blcad_core_tests blcad_geometry_tests -j6
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][gui-feature-coverage]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests
./build/dev-gui/blcad_core_tests
./build/dev-gui/blcad_geometry_tests
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-assembly-context],[block136-assembly-advanced],[block136-assembly-analysis],[block136-surface-context],[shell-combined-surface-context],[block136-body-context],[shell-combined-body-context],[block136-pattern-context],[shell-combined-pattern-context],[block136-finishing-context],[shell-combined-finishing-context],[block136-spatial-context],[shell-combined-spatial-context],[block136-profile-context],[shell-combined-profile-context],[block136-repeat],[shell-combined-repeat],[block136-selection-filters],[shell-combined-selection-filters],[block136-navigation],[shell-combined-navigation],[block136-extrude-limit-picking],[shell-combined-extrude-limit-picking],[block136-thin-extrude],[shell-combined-thin-extrude],[block136-path-extrude],[shell-combined-path-extrude],[block136-revolve],[shell-combined-revolve-controls],[block136-extrude-extents],[shell-combined-extrude-extents],[block136-holes],[shell-combined-holes],[block136-parameters],[shell-combined-parameters],[block136-sketch3d-projection],[shell-combined-projected-sweep],[block136-sketch3d-direction],[shell-sketch3d-direction-workflow],[block136-sketch3d-placement],[shell-sketch3d-placement-workflow],[block136-sketch3d-grips],[shell-sketch3d-grips-workflow],[block136-sketch3d],[shell-combined-sketch3d],[block136-sketch-diagnostics],[missing-reference-session],[block136-missing-reference],[repair-open-session],[block136-repair-open],[block136-reference-repair-batches],[block136-reference-repair],[block136-construction-manipulators],[block136-construction-picking],[shell-mixed-path-loft],[block136-construction-relations],[block136-construction-edit],[shell-combined-workflow],[gui][shell-coverage],[block136-construction],[block136-inspect-export],[block136-feature-edit],[block136-assembly],[block136-measure],[block136-surface],[block136-spatial],[block136-body],[block136-patterns],[block136-finishing],[sketch-ui-review],[gui][viewport],[native-viewer-lifecycle],[shell-features]"
```

The regular inventory audit checks consistency. `BLCAD_REQUIRE_SHELL_ACCEPTANCE=1` additionally
requires computed readiness and now passes for the accepted Block-136 state. This explicit closure
gate also rejects future regressions that claim completion while leaving gaps or pending workflows:

```bash
BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][gui-feature-coverage]"
```

Closure requires actual shell proofs, passing full Core/Geometry/GUI regression and native Qt/OCCT
checks, with plan/manifest/documentation updated together. These requirements are fulfilled in the
final acceptance below. A green inventory audit alone does not establish whole-block acceptance.

The long native filter above is the retained Block-136 baseline, not a complete filter for every
later feature. Add the affected current proofs from the manifest and feature contract, following
`docs/development-setup.md`. Keep builds, GUI runs and separate application startup serial; inspect
native captures and report every offscreen-only skip accurately.

## Final Assembly completion and Block 136 acceptance — 2026-10-25

The final slice restored root Assembly context commands, occurrence-qualified hierarchy and
relationship/joint create/edit tasks, connected cross-hierarchy solving, typed coordinate motion,
sampled root/cross Revolute analysis and exact occurrence-linked result highlighting. It retained
Core/Geometry solvers, shared definition authority, transactions and persistence.

The final `[shell-combined-workflow]` suite covered 31 workflows, including repeated imported child
Assemblies, root/cross relationships and joints, all five joint families/four driven families,
sequential Planar coordinates, invalid-input recovery, shared repeated-child poses, draft Save,
exact history, canonical reopening and flat/structured STEP. The mixed-path Loft scenario separately
covered five oriented profiles, mixed line/spline/arc path, reordering, edits and history.

| Suite | Cases | Assertions | Result |
| --- | ---: | ---: | --- |
| Full Core | 530 | 16,174 | All passed |
| Full Geometry | 441 | 30,227 | All passed |
| Full offscreen GUI | 535 | 69,283 | 516 passed; 19 native-only skips |
| Full canonical native shell | 351 | 65,703 | All passed; no skips |
| Explicit completion gate | 3 | 296 | All passed |

The native suite covered all 19 offscreen skips, real OCCT picking/grips, task layout and file
input. The framebuffer proof waited for completed GL frames, checked changed pixels for the exact
repeated occurrence, clearing/reselection and input-change cleanup. Captures were inspected at
1280×800; rotated hierarchy targets and result links selected the intended occurrence. Sample
links showed the authored current pose and stated that discrete samples omit intermediate positions.

All four application/GUI/Core/Geometry targets built. Builds, suites and startup ran serially with
no runtime source changes after final regressions. Startup reached intentional six-second timeout
124. Logs had only the known medium OCCT incomplete-visual depth/stencil warning, no high-severity
or drawable-lifecycle errors; formatting and working/staged diff checks passed. Historical local
logs used `/tmp/blcad-136-completion-{core,geometry,offscreen-full,native-full,closure,startup}.log`;
these temporary files are not required repository artifacts. The explicit completion gate and
plan/manifest updates closed this milestone without widening feature-specific geometry contracts.

## Accepted follow-up reference families — 2026-10-29

These completed issue fixes did not advance a numbered implementation block. Test filters below
identify their source evidence; current contract and source files own implementation details.

| Completed change | Persistent/behavioral contract | Regression entry points |
| --- | --- | --- |
| Issue #111: Body Rotate referenced axes | Explicit/derived DatumAxis, ConstructionLine and supported straight SemanticEdge use distinct typed chooser identities and the shared `BodyRotationAxisResolver`; numeric axes retain consuming-frame semantics. See `docs/gui-interactive-pattern-body-mvp9.md`, `docs/part-body-transform-geometry-mvp6.md`. | `[body-axis-native]`: cancel, native reference picks, upstream pivot edit, exact history, reopen/full recompute and STEP; passed at 100% and 150%. |
| Issue #110: Sketch-line axes | Exact `(SketchId, SketchEntityId)` for editable normal/construction/centerline segments; strict membership/JSON, shared current-endpoint/workplane resolution, no coordinate retargeting or double owned transform. Revolve retains in-plane/non-crossing validation. See `docs/gui-interactive-extrude-revolve-mvp9.md`, `docs/gui-interactive-pattern-body-mvp9.md`. | `[sketch-line-axis-native]`: all line roles, oblique Revolve/Rotate, source drag, downstream pattern, independent volume oracle, history/reopen/STEP; passed at both scales. |
| Issue #112: associative result-face Sketches | Faces → Create 2D Sketch creates typed support and Sketch atomically; Cancel/idle Escape and initial history restore the pair, Finish preserves profile/filter handoff, missing sources use explicit Preview/Apply repair. See `docs/gui-sketch-workbench-mvp7.md`. | `[result-face-native]`, `[result-face-producers-native]`, `[result-face-sketch]`, Geometry `[result-face-support]`: actual UI base/pocket and explicit polygon/Loft/Boolean/Fillet fixtures, source edits, pair history, repair, reopen/STEP; native proof passed at both scales. |

## Retained limits and deferred work

These original bounds remain explicit; full future implementation plans stay in their canonical
feature documents and `docs/inventor-parity-sequence.md` / `docs/inventor-parity-test-plan.md`.

- Loft: 32-span spline
approximation and unsupported G2 remain documented limits.
- Thin Extrude: General curved/open chains and closed-profile
thin semantics remain outside the retained Geometry boundary.
- Parameters: literal/formula conversion, rename/delete,
Angle formulas and Assembly parameter management retain their existing Core boundaries.

Existing feature-specific bounds, including shared child-component
placement authority, discrete Revolute sampling and the absence of constrained free-drag or imported
cross-intent remapping, are unchanged.

# Replacement shell acceptance — MVP-9R

Status: Block 136 in progress; inventory updated 2026-10-24. Numbered completion remains owned by
`docs/mvp-plan.md`. STEP Import remains Blocks 137–143.

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
- Every listed action has a unique object identity and a button in the constructed shell ribbon;
  every listed widget belongs to that window. This is a reachability inventory, not a substitute
  for executing the interaction regression tests.
- Gap/status consistency, exact blocked-family accounting and completion derived from remaining
  family gaps plus pending integrated workflows. Manifest completion must agree with the plan.

Negative controls deliberately remove/duplicate families, use a deleted tag, substitute a headless
source, hide a missing tag behind OR and claim completion with gaps. They must all be rejected.

## Remaining acceptance work

The machine-readable details live in `families[].shell.gaps`; this is the execution order:

1. Audit and restore advanced extent
   editors against the foundation API.
2. Restore contextual commands, selection-filter UI, view cube, view bookmarks and repeat command.
3. Restore the supported hierarchy validation, cross-hierarchy relationship/joint/solve controls,
   sampled Revolute analysis and occurrence-linked result highlighting.
4. Finish integrated Part and Assembly acceptance using actual ShellWindow input paths, disposable
   previews, invalid-input rejection, history, canonical save/load and native presentation.

The bounded Sketch3D/PathCurve surface includes typed/shared-point editing, native collection and
grips, anchored pointer/direction placement and explicit independent planar-point copies with visible
source identity. Exact copy semantics live in `docs/gui-sketch3d-interaction-mvp8.md`. The bounded
planar/reference surface retains diagnostic repair, repair opening and single/batch source replacement.

The datum family's fourteen creation/edit modes, native role-based reference collection and explicit
position/direction/frame/new-offset grips now have a restored bounded surface. Reference-driven
objects and shared parameter bindings keep their existing authority; the exact supported boundary
lives in `docs/gui-sketch-workbench-mvp7.md`.

Current finishing/pattern, Body, spatial, Surface, Measure, Assembly, Feature Edit and STEP/analysis
slices retain their individual documented bounds. The new Part Sketch/Extrude/STEP regression and
nested Assembly export tests cover those consumers; they do not replace the larger integrated
modeling workflows. Retained `[integration][interactive-modeling]` exercises controller APIs and
must not be relabeled as replacement-shell acceptance.

## Verification and closure

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app blcad_core_tests blcad_geometry_tests -j6
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][gui-feature-coverage]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests
./build/dev-gui/blcad_core_tests
./build/dev-gui/blcad_geometry_tests
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-holes],[shell-combined-holes],[block136-parameters],[shell-combined-parameters],[block136-sketch3d-projection],[shell-combined-projected-sweep],[block136-sketch3d-direction],[shell-sketch3d-direction-workflow],[block136-sketch3d-placement],[shell-sketch3d-placement-workflow],[block136-sketch3d-grips],[shell-sketch3d-grips-workflow],[block136-sketch3d],[shell-combined-sketch3d],[block136-sketch-diagnostics],[missing-reference-session],[block136-missing-reference],[repair-open-session],[block136-repair-open],[block136-reference-repair-batches],[block136-reference-repair],[block136-construction-manipulators],[block136-construction-picking],[shell-mixed-path-loft],[block136-construction-relations],[block136-construction-edit],[shell-combined-workflow],[gui][shell-coverage],[block136-construction],[block136-inspect-export],[block136-feature-edit],[block136-assembly],[block136-measure],[block136-surface],[block136-spatial],[block136-body],[block136-patterns],[block136-finishing],[sketch-ui-review],[gui][viewport],[native-viewer-lifecycle],[shell-features]"
```

The regular inventory audit passes while it accurately records unfinished work. For closure,
`BLCAD_REQUIRE_SHELL_ACCEPTANCE=1` additionally requires computed readiness. The following command
is expected to fail while Block 136 has gaps; it is a closure gate, not the everyday regression run:

```bash
BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][gui-feature-coverage]"
```

Close Block 136 only after the gaps and pending workflows are resolved with actual shell proofs,
full Core/Geometry/GUI regression and native Qt/OCCT checks pass, and plan/manifest/documentation are
updated together. A green inventory audit alone does not permit advancing the numbered boundary.

## Audit verification — 2026-10-22

| Suite | Cases | Assertions | Result |
| --- | ---: | ---: | --- |
| Core | 513 | 15,124 | All passed |
| Geometry | 427 | 28,518 | All passed |
| Full offscreen GUI | 303 | 17,763 | 290 passed; 13 native-only skips |
| Combined native shell | 119 | 12,304 | All passed; no skips |
| Focused inventory | 3 | 123 | All passed |

The native run covers the offscreen-only omissions, including the new Qt file-dialog and task-panel
checks at 1280×800. Captures were inspected; the viewer log has no high-severity or drawable-lifecycle
errors. Startup remained running until the intentional six-second timeout. The explicit closure gate
was also exercised and correctly refused the open acceptance state. Eight families have covered
bounded surfaces; ten retain explicit gaps. These results validate the current slices and inventory,
without closing Block 136.

## Construction creation verification — 2026-10-23

| Suite | Cases | Assertions | Result |
| --- | ---: | ---: | --- |
| Core | 513 | 15,124 | All passed |
| Geometry | 429 | 28,575 | All passed |
| Full offscreen GUI | 311 | 18,333 | 297 passed; 14 native-only skips |
| Combined native shell | 127 | 12,995 | All passed; no skips |
| Focused inventory | 3 | 128 | All passed |

The eight new shell cases cover construction/workplane creation, exact history and persistence,
input rejection, context/revision guards and downstream Sketch/Extrude consumption. Two new Geometry
cases inspect resolved OCCT presentation bounds. The native regression covers the offscreen skips,
actual ribbon mouse input, pending numeric text committed by Apply and reachable controls at
1280×800. Captures were inspected; startup remained running until the intentional six-second timeout.
The native log contains the existing incomplete-visual depth/stencil warning, with no high-severity
or drawable-lifecycle errors. The explicit closure gate still correctly refuses acceptance. Eight
families retain covered bounded surfaces and ten retain gaps; creation narrows the datum-family gap
without closing it. The subsequent editing slice is recorded below; remaining relation/pick
workflows and the acceptance sequence above stay open.

## Combined UI workflow regression — 2026-10-23

`[shell-combined-workflow]` covers seven replacement-shell scenarios using actual Qt widget/input paths:

1. Offset workplane → mouse/numeric Sketch → Extrude → Fillet → upstream feature edit → linear Body
   pattern → XYZ translation → Body Measure/copy → multi-step history → canonical reopen → Part STEP.
2. Two authored Bodies → cancelled Boolean preview → subtract with retained tool → scale → Mirror →
   multi-step history → new translation branch that truncates redo → canonical reopen.
3. Three mouse/numeric sections on origin/offset workplanes → Loft source collection and list reordering
   → feature rename → exact history → canonical reopen.
4. UI-authored Part → save → new Project → definition file import → grounded/free placements → Mate
   preview/solved pose → group Solve → clearance/contact analysis → exact history → canonical reopen
   → flat and structured Assembly STEP.
5. Saved Sketch → cancelled Extrude → rejected/recovered construction input and Escape → extrusion →
   cancelled Open and dirty-file confirmation → history/save → document switch/reopen → cancelled
   partial Sketch gesture.
6. Offset workplane → Sketch/Extrude → same-ID workplane edit with a new offset binding → semantic
   face workplane → cyclic-reference rejection/recovery/Escape → rename → three-step Undo/Redo →
   canonical reopen → Part STEP. Pending numeric text is consumed by the Apply mouse event.
7. Authored solid → semantic vertex/midpoint points → parallel edge line and derived axis → parallel
   point workplane → Sketch/Extrude → upstream point rebind and cyclic-reference recovery → edge/line
   edits → three-step exact history → canonical reopen → Part STEP.

These tests use no Core fixtures or direct authoring-controller calls. They check independent
Geometry results, intent/identity preservation, atomic and branching history, complete persistence,
read-only consumers and interaction cleanup. Both offscreen and native backends execute the same
scenarios; native widget reachability and OCCT availability are explicit assertions. Repeatable
commands: `docs/development-setup.md`.

Sketch creation combines an actual viewport pick, Escape backout, absolute keyboard coordinates,
mouse hover/rubber-band feedback and relative keyboard coordinates. This preserves exact authored
dimensions across native pixel quantization. Loft bounds are checked as conservative enclosures of
the authored end planes; native display triangulation may enlarge that envelope. Volume/solid-count
checks remain independent Geometry assertions.

The scenarios cover selected combinations of the restored command surfaces. Remaining family gaps,
joint/motion combinations, native manipulator variants and broader integrated acceptance still need
proof; neither the numbered boundary nor the closure gate changes.

| Suite | Cases | Assertions | Result |
| --- | ---: | ---: | --- |
| Combined workflows, offscreen | 5 | 1,337 | All passed; no skips |
| Full offscreen GUI | 316 | 19,670 | 302 passed; 14 native-only skips |
| Combined native shell | 132 | 14,343 | All passed; no skips |
| Focused inventory | 3 | 128 | All passed |

The native run includes all five new workflows and every offscreen-only omission. Machining, Loft
and Assembly captures at 1280×800 were inspected. The log retains the existing incomplete-visual
depth/stencil warning, without high-severity or drawable-lifecycle errors. The explicit closure gate
was exercised and still rejects the open Block-136 acceptance state. This increment changes tests,
test registration and documentation; Core/Geometry and application behavior remain unchanged.

## Construction editing verification — 2026-10-23

| Suite | Cases | Assertions | Result |
| --- | ---: | ---: | --- |
| Focused Core construction updates | 4 | 116 | All passed |
| Full Core | 517 | 15,240 | All passed |
| Full Geometry | 429 | 28,575 | All passed |
| Full offscreen GUI | 322 | 20,508 | 308 passed; 14 native-only skips |
| Combined native shell | 138 | 15,184 | All passed; no skips |

Five new browser-edit regressions cover all nine restored definitions, reference rebinding,
downstream degenerate-input rejection, exact identity/history/persistence, precision/dependency
retention and task cancellation. The sixth combined UI workflow exercises these consumers through
actual ribbon/browser/task/file input, including pending numeric text, cyclic-reference recovery,
three-step Undo/Redo, canonical Save/Open/Save and STEP export. Core regression also covers forward
plane references and construction planes rebound to generated-face workplanes: JSON loading preserves
authored array order rather than changing intent during dependency-ready insertion.

Native 1280×800 captures were inspected. The startup smoke run remained running until the intentional
six-second timeout (exit 124). The native log contains the existing incomplete-visual depth/stencil
warning and no high-severity or drawable-lifecycle errors. The explicit closure gate correctly fails
one of three cases (129 assertions) because whole-shell acceptance remains open: eight families have
covered bounded surfaces and ten retain gaps. Continue with generated-reference/parallel construction
creation/editing and native reference collection/manipulators; the remaining acceptance order above
is unchanged. This slice does not close the datum family or Block 136.

## Generated construction relations verification — 2026-10-23

| Suite | Cases | Assertions | Result |
| --- | ---: | ---: | --- |
| Full Core | 519 | 15,311 | All passed |
| Full Geometry | 431 | 28,613 | All passed |
| Full offscreen GUI | 327 | 21,266 | 313 passed; 14 native-only skips |
| Combined native shell | 143 | 15,945 | All passed; no skips |

Two new Core cases cover missing semantic source rejection and canonical loading across later
feature/point/plane dependencies without changing point order. Two Geometry cases check evaluated
three-point and parallel frames, upstream depth changes and evaluated degeneracy. Four shell cases
exercise creation and same-ID reference edits for all five added modes, exact history/persistence,
generated-point consumers, cyclic/degenerate input recovery and cancellation with unavailable inputs.
The seventh combined UI workflow extends the real input proof through semantic points, parallel
line/axis/plane construction, a downstream Sketch/Extrude, three upstream edits, cyclic-reference
recovery, three-step Undo/Redo, canonical file roundtrip and STEP. It also checks that long type names
do not force horizontal task scrolling at 1280×800.

Native captures were inspected after correcting type-selector sizing; labels and input controls
remain visible. The startup smoke reached the intentional six-second timeout (124). Native logs
contain the existing incomplete-visual depth/stencil warning, with no high-severity or drawable-
lifecycle errors. The explicit closure gate continues to reject the open acceptance state (one of
three cases, 129 assertions). Eight families have covered bounded surfaces and ten retain gaps.
Native construction reference collection/manipulators remain next; the datum family and Block 136
stay partial and the subsequent acceptance sequence is unchanged.

## Mixed-path oriented Loft verification — 2026-10-23

Four new Geometry cases exercise three/five unequal rectangle/trapezoid Sketches on differently
oriented and rolled planes, following a connected line–cubic spline–reversed circular arc path.
The original sampler reduced each circular control arc to two chords: the first three cases failed
at Loft recompute against that implementation. The correction retains circular length/positions for
center paths and guides. Independent formula-based station expectations are checked against OCCT
boundary faces, including vertices, edge midpoints and an intermediate arc station; tests also cover
valid solids/surfaces, open sections, a mixed guide, Sweep volume, middle-profile edits, canonical
JSON/recompute and cached native-shape retention after invalid edits.

The distinct `[shell-mixed-path-loft]` workflow opens only prepared Sketch/plane inputs through the
Open dialog; those inputs are not authored by this UI scenario. PathCurve and five-section Loft
creation, segment reversal, section reordering, rejected/recovered G2 preview, middle-width property
editing, three-step history, canonical Save/Open/Save and STEP all use real Qt widget/dialog input.
The seven `[shell-combined-workflow]` scenarios retain their separate fully UI-authored boundary.
The inventory links the new exact tag/source without changing whole-block readiness.

| Suite | Cases | Assertions | Result |
| --- | ---: | ---: | --- |
| Focused mixed-path Geometry | 4 | 989 | All passed |
| Full Core | 519 | 15,311 | All passed |
| Full Geometry | 435 | 29,602 | All passed |
| Full offscreen GUI | 328 | 21,591 | 314 passed; 14 native-only skips |
| Focused native mixed-path UI | 1 | 328 | All passed; no skips |
| Combined native shell, serial rerun | 144 | 16,111 | All passed; no skips |

The native five-plane Loft capture was inspected. Startup reached the intentional six-second timeout
(exit 124). The first combined native run failed one existing workplane test at a ComboBox popup
selection; the startup smoke had also opened an application window during that run. The rerun without
a concurrent application window passed all 144 cases;
the first failure was in UI selection rather than a Geometry assertion. Native logs contain the
existing incomplete-visual depth/stencil warning, without high-severity or drawable-lifecycle errors.
The closure gate still correctly rejects Block 136 (one of three cases fails, 129 assertions).
Uniform normalized path-length section stations, compatible section topology, 32-span spline
approximation and unsupported G2 remain documented limits. Native construction reference
collection/manipulators remain next; Block 136 stays in progress.

## Native construction reference collection verification — 2026-10-23

The construction task collects existing construction points/lines and datum/construction/derived
planes through an explicit **Modellklick wählt** role. Ordered defining-point picks advance roles;
generated face/edge/vertex picks use the same typed source catalogue as their selectors. Scope filters
exclude overlapping unrelated geometry. Generated sources retain the existing rectangular-additive-
extrude source-feature contract; they are not final-Body subshape ordinals or automatic topology
remaps through later finishing/transforms. Hidden-Body and unavailable sources are omitted.

One new Geometry case verifies the 26-source typed catalogue, bounded face area, source identities
and upstream depth changes. Five shell cases cover ordered/revisited roles, repeated-point recovery,
semantic create/edit, exact history/canonical reopen, hidden sources, explicit numeric modes and
custom-mask/scope restoration after Apply, Escape, dock close, Save or revision expiry. The new native
case clicks real model coordinates at 1280×800 through three-point plane, parallel edge/point,
vertex-point and face-plane workflows, four-step history and canonical reopen. Before each generated
edge/vertex/face click, a different source is selected so defaults cannot satisfy the pick assertion.
Only that case skips offscreen; the seven fully UI-authored combined workflows retain their bounds.

| Suite | Cases | Assertions | Result |
| --- | ---: | ---: | --- |
| Full Core | 519 | 15,311 | All passed |
| Full Geometry | 436 | 29,779 | All passed |
| Full offscreen GUI | 333 | 22,045 | 318 passed; 15 native-only skips |
| Combined native shell | 149 | 16,861 | All passed; no skips |

Native captures were inspected; the role selector and Apply/Cancel remain reachable at 1280×800.
The separate startup smoke reached the intentional six-second timeout (124). Native logs contain
the existing incomplete-visual depth/stencil warning, without high-severity or drawable-lifecycle
errors. The explicit closure gate correctly rejects unfinished acceptance (one of three cases,
130 assertions). Coverage inventory now includes the exact proof tag/source and role widget.
Construction position/direction/offset manipulators remain next. The datum family stays partial,
Block 136 remains in progress, and the subsequent acceptance sequence above is unchanged.

## Construction manipulator verification — 2026-10-23

The construction task adds XYZ position arrows, direction/frame rotation rings and a positive
new-offset arrow using the shared candidate-only layer. Explicit plane rotation preserves both axes
at full precision. Reference-driven definitions keep their picks; shared/expression offset bindings
stay read-only unless a new candidate parameter is chosen. Separated, labeled rings keep world axes
selectable. Numeric override, exact final release, two-level Escape, focus/camera rollback and command
expiry retain existing preview/Apply/history authority. Preview redraws do not abort the frozen drag.

Seven new construction cases cover explicit authoring, composed rotations and precision across edits,
offset bindings, drag rollback, final mouse sample, dependent degeneracy/recovery and task expiry.
An additional viewport regression retains restored camera values without a native viewer. The eighth
combined UI workflow creates a rotated workplane, authors Sketch/Extrude, moves the same plane with a
downstream solid preview, exercises exact history/canonical reopen and exports STEP. All nine cases
execute on both backends; no new native-only skips are introduced. Native tests retain the fitted
OCCT zoom factor, while headless tests restore a useful model span for their mapping.

| Suite | Cases | Assertions | Result |
| --- | ---: | ---: | --- |
| Focused offscreen manipulators/camera/combined | 9 | 897 | All passed |
| Full Core | 519 | 15,311 | All passed |
| Full Geometry | 436 | 29,779 | All passed |
| Full offscreen GUI | 342 | 22,945 | 327 passed; 15 native-only skips |
| Combined native shell | 158 | 17,788 | All passed; no skips |

Native rotation-grip and downstream-solid captures were inspected at 1280×800. Task controls and
Apply/Cancel remain reachable. The separate startup smoke reached the intentional six-second timeout
(124). Native logs contain the existing incomplete-visual depth/stencil warning, with no high-severity
or drawable-lifecycle errors. The explicit closure gate correctly refuses unfinished acceptance (one
of three cases, 133 assertions). Nine families now have restored bounded surfaces; nine retain gaps.
The datum family is restored within its documented definition/parameter boundaries. Block 136 remains
in progress; explicit broken-reference repair is next, followed by Sketch3D and the remaining sequence.


## Explicit reference-repair continuation — 2026-10-23

The shell now offers a bounded explicit repair task for projected points/lines and generated-face
workplanes. Geometry diagnostics, unselected replacement choices, explicit Preview, selector-change
invalidation and one atomic Apply preserve stable consumer identities. The three Core regressions
and seven shell cases cover replacement/dependency integrity, cyclic and wrong-kind rejection,
construction fallbacks, lost-reference diagnostics, face placement, lifecycle expiry, canonical files
and native ribbon/popup/button input on imported fixtures. Contract and remaining limits:
`docs/gui-sketch-workbench-mvp7.md`. This advances the existing partial planar/reference family;
nine blocked families and the pending integrated workflows still keep Block 136 open.

Verification of the final implementation:

- GUI/application/Core/Geometry targets built with the `dev-gui` preset and `-j4`.
- Focused Core: **3 cases, 99 assertions**, all passed. Repair plus shell-inventory regression:
  **10 cases, 1,189 assertions**, all passed offscreen.
- Full Core: **522 cases, 15,410 assertions**, all passed. Full Geometry:
  **436 cases, 29,779 assertions**, all passed.
- Full offscreen GUI: **349 cases — 334 passed, 15 native-only skips; 24,002 assertions passed**.
  No additional skip was introduced by this slice.
- Combined native Qt/OCCT filter above: **165 cases, 18,851 assertions**, all passed without skips.
  Point/line repair previews were captured and inspected at 1280×800; task selectors, diagnostic
  text, Preview/Apply/Cancel and the ribbon scroll path remain reachable.
- Separate application startup reached the intentional six-second timeout (124). Native logs retain
  the existing medium incomplete-visual depth/stencil warning, without high-severity or drawable
  lifecycle errors. New/changed C++ formatting, JSON parsing and `git diff --check` passed.
- The explicit closure gate still correctly fails **one of three cases, 141 assertions** because
  acceptance is incomplete. The ordinary inventory audit passes. Nine families retain gaps;
  Block 136 remains in progress, with the remaining repair work next in `docs/mvp-plan.md`.


## Atomic reference-repair list continuation — 2026-10-23

The task now collects changes across several workplanes and Sketches before a complete preview and
one history transaction. Six new cases cover partial Geometry failure/recovery, scoped identities,
upsert/remove/clear, cycle rejection with the prior list preserved, complete transient replacement
scope, all task exits and canonical history/files. The native input workflow stages five changes,
removes/restages a plane and commits the complete repair through actual ribbon/popup/list/button
input. Existing single-reference tests remain compatibility proofs. Contract and limits live in
`docs/gui-sketch-workbench-mvp7.md`. The planar/reference family remains partial; nine blocked
families and integrated acceptance keep Block 136 open.

Verification of the final implementation:

- All GUI/application/Core/Geometry targets built with `dev-gui` and `-j4`.
- Focused single/batch repair plus inventory: **16 cases, 2,120 assertions**, all passed offscreen.
- Full Core: **522 cases, 15,410 assertions**; full Geometry: **436 cases, 29,779 assertions**,
  all passed.
- Full offscreen GUI: **355 cases — 340 passed, 15 native-only skips; 24,933 assertions passed**.
  The six new batch cases add no skip.
- Combined native Qt/OCCT filter above: **171 cases, 19,785 assertions**, all passed without skips.
  The five-entry repair list, readable target labels, complete preview, native Fit and reachable
  Add/Remove/Clear/Preview/Apply/Cancel controls were captured and inspected at 1280×800.
- Changed C++ formatting, manifest JSON and `git diff --check` passed. The ordinary inventory audit
  passes; the explicit closure gate correctly fails **one of three cases, 145 assertions** for the
  recorded remaining acceptance gaps. Block 136 remains in progress.

The separate native application startup reached the intentional six-second timeout (124). Native
logs retain the existing medium incomplete-visual depth/stencil warning, without high-severity or
drawable-lifecycle errors.


## Repair-time opening continuation — 2026-10-23

The explicit Part repair-open command admits Core-valid intent after failed initial Geometry recompute
without publishing a partial cache. Three session and three shell cases cover normal-open rejection,
strict validation, ordinary mutation/freshness guards, complete atomic repair, failed-state history,
preview saving, cancellation and actual file/ribbon/popup/list/button/camera input. Undo clears derived
presentation when restoring the failed original; Redo recomputes the repaired model. Exact contract:
`docs/reference-recovery-mvp.md` and `docs/gui-sketch-workbench-mvp7.md`. The family remains partial for
Core-invalid source recovery and constraint-diagnostic controls; Block 136 remains open.

Verification of the final implementation:

- GUI/application/Core/Geometry targets built with `dev-gui` and `-j4`; GUI and GUI-test objects
  were rebuilt after identifying cached objects with missing header-dependency metadata.
- Focused session/repair/inventory filter: **22 cases, 2,590 assertions**, all passed offscreen.
- Full Core: **522 cases, 15,410 assertions**; full Geometry: **436 cases, 29,779 assertions**,
  all passed.
- Full offscreen GUI: **361 cases — 346 passed, 15 native-only skips; 25,403 assertions passed**.
  The six new cases introduce no skip.
- Combined native Qt/OCCT filter above: **177 cases, 20,259 assertions**, all passed without skips.
  File-dialog input, two-plane repair Preview/Fit/Apply, Undo/Redo presentation replacement and
  canonical file actions executed natively. The repair preview was captured and inspected at
  1280×800; task/file/view controls remain reachable.
- Changed-line C++ formatting, new fixture formatting, manifest JSON and `git diff --check` passed.
  The normal inventory passes; the explicit closure gate correctly fails **one of three cases,
  147 assertions** while Core-invalid source recovery and other acceptance gaps remain open.

The separate native startup smoke reached the intentional six-second timeout (124). Native logs
retain the existing medium incomplete-visual depth/stencil warning without high-severity or
drawable-lifecycle errors. Block 136 remains in progress.


## Missing-source reference recovery continuation — 2026-10-23

Explicit repair opening now retains missing sources of face-derived workplanes and projected
semantic/construction points and lines while normal loading stays strict. The six-input fixture
preserves constraints, a generated-line consumer and two plane-driven solids. Three Core cases,
one Geometry case, two session cases and two shell cases cover canonical failed intent, unrelated
invalid/wrong-kind/cyclic rejection, bounded intermediate updates, complete recompute/cache guards,
committed-only saving, cancellation and one-step failed/successful history. The native input proof
uses actual file dialogs, popups, lists, buttons, Fit, Undo/Redo and strict Save/Open after repair.
Contract: `docs/reference-recovery-mvp.md` and `docs/gui-sketch-workbench-mvp7.md`.

Verification of the final implementation:

- GUI/application/Core/Geometry targets built with `dev-gui` and `-j4`.
- Focused Core: **3 cases, 135 assertions**; focused Geometry: **1 case, 42 assertions**, all passed.
- Focused session/repair/inventory filter: **26 cases, 2,959 assertions**, all passed offscreen.
- Full Core: **525 cases, 15,545 assertions**; full Geometry: **437 cases, 29,821 assertions**,
  all passed.
- Full offscreen GUI: **365 cases — 350 passed, 15 native-only skips; 25,772 assertions passed**.
  The four new GUI cases add no skip.
- Combined native Qt/OCCT filter above: **181 cases, 20,631 assertions**, all passed without skips.
  The six-entry repair Preview was captured and inspected at 1280×800. The list is scrollable,
  diagnostics identify the missing source, and Preview/Apply/Cancel remain reachable. Every partial
  Preview failed before the complete six-input repair succeeded; Undo cleared repaired presentation.
- Changed-line C++ formatting, new fixture formatting, manifest JSON and `git diff --check` passed.

The native application startup reached the intentional six-second timeout (124). Native logs retain
only the existing medium incomplete-visual depth/stencil warning, without high-severity or
drawable-lifecycle errors. The family remains partial for explicit constraint-diagnostic suggestion
controls. Nine families and integrated workflows still retain acceptance gaps; Block 136 remains in
progress. The ordinary inventory passes; the opt-in closure gate correctly fails one of three
cases (147 assertions) for the remaining acceptance gaps.


## Explicit Sketch diagnostic suggestion continuation — 2026-10-23

The model and Sketch ribbons now open an owned diagnostic task over the existing Core diagnostic
and safe repair subset. All seven diagnostic kinds are visible; three supported actions require
Preview and one explicit Apply. Unsupported/manual suggestions stay visible and blocked. A complete
session copy validates candidate catalogs and Geometry without touching live history, caches or
files. Active Sketch authoring pauses and resumes from committed intent. Contract:
`docs/gui-sketch-workbench-mvp7.md`.

Six `[block136-sketch-diagnostics]` cases cover all supported/manual/no-suggestion paths, duplicate
identity preservation, deterministic repair-ID collisions, three Sketches and a solid, accepted
constraint/dimension sidecars, Preview/Apply/history/persistence, freshness/revision/foreign-command
and reference-repair guards, cancellation/Save/Finish and active Sketch restoration. The native
input proof uses actual model/Sketch ribbon, diagnostic-list, button, Fit, history and Save events.
Combined workflow popup input now centers the visible row and synchronizes native desktop pointer,
hover and click events; it keeps event-level selection assertions instead of assigning combo indices.

Verification of the final implementation:

- All GUI/application/Core/Geometry targets built with `dev-gui` and `-j4`.
- Focused diagnostics/workbench/inventory: **10 cases, 1,357 assertions**, all passed offscreen.
- Full Core: **525 cases, 15,545 assertions**; full Geometry: **437 cases, 29,821 assertions**,
  all passed.
- Full offscreen GUI: **371 cases — 356 passed, 15 native-only skips; 26,969 assertions passed**.
  The six new diagnostic cases add no skip.
- Combined native Qt/OCCT filter above: **187 cases, 21,882 assertions**, all passed without skips.
  Preview was captured and inspected at 1280×800. Diagnostic target/message, selected repair,
  changed IDs, remaining errors/warnings and Preview/Apply/Cancel controls are readable/reachable.
- Changed-line C++ and new-file formatting, manifest JSON and `git diff --check` passed.

The native application startup reached the intentional six-second timeout (124). Native logs retain
the existing medium incomplete-visual depth/stencil warning without high-severity or drawable
lifecycle errors. The bounded planar/reference family is restored: ten families have restored
surfaces and eight retain explicit gaps. Sketch3D authoring/editing is next. Block 136 remains in
progress until remaining families and integrated acceptance pass the documented closure gate.
The ordinary inventory passes; the opt-in closure gate correctly fails one of three cases
(158 assertions) for the remaining eight families and integrated workflows.

## Typed Sketch3D point/line verification — 2026-10-23

The owned Sketch3D task now creates explicit XYZ points and shared-point line chains and reopens
existing sketches for point editing. Disposable previews validate consuming paths and downstream
Geometry before one Apply transaction. Parameter detachment is explicit; untouched precision,
curves/guides, history and the existing persistence schema remain intact. Contract and limits:
`docs/gui-sketch3d-interaction-mvp8.md`.

The ninth fully UI-authored combined workflow creates four points, three lines, PathCurve and Sweep,
rejects an endpoint collapse, extends the swept solid from 480 to 640 mm³, exercises four-step exact
Undo/Redo, cancels another edit, reopens canonically and exports STEP. The shared input harness now
confirms native window activation and synchronizes desktop-pointer/window-system events before
popup input, awaits actual visibility with a deadline and checks hit/highlight identities before
mouse selection. These assertions also cover the existing construction and mixed-path workflows.

All four `dev-gui` targets built successfully. Final verification:

| Suite | Cases | Assertions | Result |
| --- | ---: | ---: | --- |
| Core | 527 | 15,779 | All passed |
| Geometry | 437 | 29,821 | All passed |
| Full offscreen GUI | 378 | 28,135 | 363 passed; 15 native-only skips |
| Combined native workflows | 10 | 3,377 | All passed; nine UI-authored plus imported mixed-path Loft |
| Full combined native shell | 194 | 23,137 | All passed; no skips |

Native captures `sketch3d-chain-preview.png` and `sketch3d-downstream-sweep-preview.png` were inspected
at 1280×800: coordinate/reference inputs, candidate geometry, messages and Apply are readable and
reachable. The six-second application startup smoke exits with intentional timeout 124. Native
runs retain the known medium depth/stencil visual warning; no high-severity, BadDrawable or fatal
lifecycle diagnostics occurred. Run startup and GUI tests serially.

The ordinary inventory passes. The opt-in closure gate still fails one of three cases (161
assertions), as expected: ten bounded families are restored and eight retain gaps. Native Sketch3D
collection/direct lock handles, distance/direction input and explicit planar projection remain next.
Block 136 stays in progress; `docs/mvp-plan.md` retains the numbered boundary at Block 135.

## Sketch3D viewport collection and point-grip verification — 2026-10-23

The active Sketch3D candidate now supports projected point/straight-line collection and opt-in
translation grips. All seven direction restrictions expose the permitted independent axis handles;
numeric grip input specifies an absolute XYZ component in millimeters. Disposable previews validate
the complete downstream model before Apply. Gesture rollback preserves earlier local edits and
precision, and parameter detachment still requires explicit consent. Contract, picking bounds and
remaining placement/distance/projection work: `docs/gui-sketch3d-interaction-mvp8.md`.

Six additional Qt input cases cover local-ID collection, all locks, bound-point consent, final
release and numeric precedence, invalid/recovered paths, camera/resize/focus/task cleanup and typed
coordinate edits during a drag. The existing UI-authored Sketch3D/Sweep workflow now collects its
endpoint in the viewport, rejects a locked Z-grip collapse, recomputes the corrected 640 mm³ solid
and retains exact history, canonical reopening and STEP export.

All four `dev-gui` targets built successfully. Final verification:

| Suite | Cases | Assertions | Result |
| --- | ---: | ---: | --- |
| Core | 527 | 15,779 | All passed |
| Geometry | 437 | 29,821 | All passed |
| Focused offscreen Sketch3D and inventory | 16 | 1,615 | All passed |
| Full offscreen GUI | 384 | 28,674 | 369 passed; 15 native-only skips |
| Focused native point grips and combined Sweep | 7 | 1,143 | All passed; no skips |
| Full combined native shell | 200 | 23,706 | All passed; no skips |

The native `sketch3d-downstream-sweep-preview.png` capture was inspected at 1280×800: the Z grip,
40 mm numeric input, candidate solid and pinned Apply footer are readable and reachable. The
separate six-second native application startup smoke exits with intentional timeout 124. Native
logs retain the known medium depth/stencil visual warning; no high-severity, BadDrawable or fatal
lifecycle diagnostics occurred. Startup and GUI runs were serial.

The ordinary inventory passes. The opt-in closure gate correctly fails one of three cases (161
assertions): ten bounded families are restored and eight retain explicit gaps. Block 136 remains
in progress, with the numbered completion boundary at Block 135. Next Sketch3D work is pointer
placement of new points, explicit distance/direction input and planar-point projection.

## Additional cross-family UI workflows — 2026-10-23

Three further `[shell-combined-workflow]` cases extend the combined coverage to twelve scenarios:
eleven with fully UI-authored inputs and one with an imported seated Part definition. They use the
same real widget/input harness on offscreen and native Qt/OCCT:

- `[shell-combined-surface-solid]`: Sketch/Extrude → sew five semantic faces → reject open-shell
  conversion → cancel/Undo → replace the redo branch with six faces → closed-shell solid → translate
  → overlapping Boolean union into a new Body with retained sources → four-step exact history →
  canonical Save/Open/Save → Part STEP. Independent volumes are 4,000 mm³ per source and 6,000 mm³
  for the union; disposable previews leave committed intent unchanged.
- `[shell-combined-revolve-pattern]`: authored rectangle → 180° Revolve → four-instance circular
  pattern → Rotate → Scale about an explicit center → edit the same Revolve to 90° and check all
  downstream volumes → reject count one, correct to six and rename the same pattern → five-step
  exact history → Body Measure/copy → canonical reopen. Rotation and scaling about the entered
  center also have independent bounds checks. This uses the current bounded partial-angle
  editor; it does not claim a Full-extent mode selector.
- `[shell-combined-planar-motion]`: retained seated-Part example file → new Project/import →
  grounded/free component placements → Planar joint with three table-edited bounded coordinate slots
  → reject and correct inverted limits → slider Home/End and cancelled motion preview → commit
  U/V/normal-rotation separately → exact
  four-step history → Undo and a new rotation branch → clearance analysis → canonical reopen → flat
  and structured Assembly STEP. Coordinate retention, the grounded pose, driven normal rotation,
  source-definition bytes and read-only consumers remain explicit assertions. The clearance result
  is checked against the analytical corner distance of the posed rectangles. It imports
  `examples/revolute_joint.blcad.project.json` for Frame-capable seat references, without claiming
  missing dedicated Hole-authoring coverage.

The harness types into numeric widgets and actual table-cell editors; it does not seed Core intent
or drive authoring controllers. Every history checkpoint also checks task/preview/filter cleanup and
derived freshness. Focused commands and serial native-run requirements are recorded in
`docs/development-setup.md`. The coverage manifest links the new tags to their exact shell source.
These proofs preserve the bounded contracts, ten restored/eight partial families and Block 136's
open status; whole-shell acceptance and remaining family gaps still require the documented work.

The Surface/Boolean chain also caught collapsed fields when reusing the shared task after a
transform: wrapped guidance in the dynamic form allowed hidden rows to corrupt the visible field
heights. Guidance now lives beside the form in its containing column. The regression operates the
actual result-mode popup at 1280×800 and checks NewBody identity and retained target/tools; it does
not bypass hit testing with direct field assignment. Core authority and persistence are unchanged.

`blcad_gui_tests` and `blcad_app` built successfully with `dev-gui`. Final verification:

| Suite | Cases | Assertions | Result |
| --- | ---: | ---: | --- |
| Focused new workflows and inventory, offscreen | 6 | 1,719 | All passed |
| Full offscreen GUI | 387 | 30,233 | 372 passed; 15 native-only skips |
| Full combined native shell | 203 | 25,346 | All passed; no skips |

The native run includes all twelve combined workflows and the separate imported mixed-path Loft
case. Captures `combined-sewn-boolean-controls.png`, `combined-revolve-pattern-edit-preview.png`,
`combined-revolve-pattern.png` and `combined-planar-motion-preview.png` were inspected at 1280×800.
The corrected Boolean result-mode input and pinned Apply footer are reachable; candidate geometry
and the joint's coordinate/limit controls remain visible. Native logs retain the known medium
depth/stencil warning, without high-severity, BadDrawable or fatal lifecycle errors. The separate
six-second startup smoke reached intentional timeout 124. GUI tests and startup were serial.

Core/Geometry suites were not rerun for this GUI layout/test change; their sources are unchanged.
The ordinary inventory passes. The opt-in closure gate still correctly rejects acceptance (one of
three cases fails, 161 assertions) because the eight partial families and pending whole-shell
workflows remain open. The numbered completion boundary stays at Block 135, with Block 136 in
progress.

## Anchored Sketch3D pointer placement — 2026-10-23

The spatial source task stages new points through an explicit origin/local-point anchor and final
viewport release. XYZ fixes depth in a camera-facing plane; six axis/plane locks reuse model
directions. Hover markers remain transient, ambiguous directions reject placement, and bound
anchors retain their bindings while newly placed points copy explicit values. Existing complete
downstream Preview/Apply, identity allocation and persistence remain authoritative. Exact bounds:
`docs/gui-sketch3d-interaction-mvp8.md`.

Six new cases tagged `[block136-sketch3d-placement]` cover all seven modes in both projections,
pixel-aware coordinates, stable anchors, precise copied bindings, final-release staging, earlier
local preview preservation, pending numeric text, invalid/unobservable rays and
Escape/camera/resize/focus/capture/control/save/revision/document cleanup. The existing UI-authored
Sketch3D/PathCurve/Sweep workflow also places its points via actual popup/viewport input before typed
refinement; its exact downstream volumes, failed/corrected edits, four-step history, canonical reopen
and STEP checks remain intact. Its additional tag is `[shell-sketch3d-placement-workflow]`; the
combined workflow count stays twelve.

The coverage inventory narrows the Sketch3D gap to explicit distance/direction input and planar-point
projection. Ten families remain restored and eight partial; Block 136 remains in progress. Repeatable
focused commands and the canonical native filter are in `docs/development-setup.md` and above.

`blcad_gui_tests` and `blcad_app` built successfully with `dev-gui`. Final verification:

| Suite | Cases | Assertions | Result |
| --- | ---: | ---: | --- |
| Focused Core Sketch3D edit/update contracts | 6 | 291 | All passed |
| Focused offscreen Sketch3D and inventory | 22 | 2,889 | All passed |
| Full offscreen GUI | 393 | 31,507 | 378 passed; 15 native-only skips |
| Focused native placement and combined Sweep | 7 | 1,931 | All passed; no skips |
| Full combined native shell | 209 | 26,684 | All passed; no skips |

The native `sketch3d-point-placement-hover.png` capture was inspected at 1280×800: the amber
anchor/candidate marker, explicit anchor and Z lock, coordinate summary and pinned task footer are
visible and reachable. The native fixture now conditionally awaits the window manager's actual
activation before input, with a two-second deadline and the original active-window assertion.
The separate six-second native startup smoke reaches intentional timeout 124. Native logs retain
the known medium depth/stencil visual warning, without high-severity, BadDrawable or fatal lifecycle
diagnostics. Builds, GUI runs and startup were serial.

Core/Geometry sources and persistence are unchanged; the full Core/Geometry suites were not rerun
for this slice. The ordinary inventory passes. The opt-in closure gate correctly rejects acceptance
(one of three cases fails, 163 assertions) while eight families and whole-shell workflows retain
documented gaps. Block 136 stays open, with the numbered completion boundary at Block 135.

## Sketch3D distance/direction input — 2026-10-23

The task stages new points from an explicit distance/azimuth/elevation tuple relative to the same
origin/local-point anchor as pointer placement. It uses the Core resolver and precise copied anchor
values, independent of the pointer lock. Invalid text and unresolved XYZ bounds reject atomically.
Pending tuples invalidate complete previews and block competing authoring until staged or cleared;
editing tuple text rolls back a live gesture. Full downstream Preview/Apply remains authoritative.
Numeric bounds, coordinate conventions and lifecycle rules:
`docs/gui-sketch3d-interaction-mvp8.md`.

Five `[block136-sketch3d-direction]` cases cover precise bound/local anchors, zero distance, signed
angles, complete/incomplete tuples, non-finite/out-of-range/overflow rejection and correction,
competing inputs, gesture rollback, pending spin text, exact history, canonical persistence and
Save/revision/Escape cleanup. The existing combined Sweep workflow adds
`[shell-sketch3d-direction-workflow]`: actual field/button input rejects an incomplete tuple and
recovers its endpoint, then retains the PathCurve/Sweep, failed/corrected grip, history/reopen/STEP
sequence. The combined workflow count remains twelve. Inspect
`sketch3d-distance-direction-input.png` at 1280×800; commands live in `docs/development-setup.md`.

The inventory narrows the remaining Sketch3D gap to explicit planar projection. Ten bounded families
remain restored and eight partial. Block 136 remains in progress; numbered completion stays at 135.

`blcad_gui_tests` and `blcad_app` built successfully with `dev-gui`. Final verification:

| Suite | Cases | Assertions | Result |
| --- | ---: | ---: | --- |
| Focused Core Sketch3D edit/update contracts | 6 | 291 | All passed |
| Focused offscreen Sketch3D and inventory | 27 | 3,457 | All passed |
| Full offscreen GUI | 398 | 32,075 | 383 passed; 15 native-only skips |
| Focused native direction and combined Sweep | 6 | 1,321 | All passed; no skips |
| Full combined native shell | 214 | 27,260 | All passed; no skips |

The native `sketch3d-distance-direction-input.png` capture was inspected at 1280×800: the explicit
anchor, complete tuple and staging button are visible, with the pinned Preview/Apply/Cancel footer
reachable. Native logs retain the known medium depth/stencil visual warning, without high-severity,
BadDrawable or fatal lifecycle errors. The separate six-second application startup smoke reaches
intentional timeout 124. Builds, GUI runs and startup were serial; formatting and diff checks pass.

Core/Geometry sources and persistence are unchanged; their full suites were not rerun for this
slice. The ordinary inventory passes. The opt-in closure gate correctly rejects unfinished
acceptance (one of three cases fails, 168 assertions). Remaining family and whole-shell gaps keep
Block 136 open, with explicit planar-point projection next for Sketch3D.

## Explicit planar-point copying into Sketch3D — 2026-10-23

The source task stages current planar line endpoints/projected points as independent XYZ copies,
with stable typed source identities, per-copy consent and local provenance receipts. Geometry uses
the current workplane/reference cache, including arbitrary and generated-face planes. Pending input,
invalid sources, interrupted gestures and task boundaries retain the existing complete Preview/Apply
authority. Committed copies remain independent after source edits; persistence stores only explicit
XYZ values. Contract: `docs/gui-sketch3d-interaction-mvp8.md`.

Six `[block136-sketch3d-projection]` cases cover four oriented planes, construction-projected points,
cached generated-face/semantic-vertex sources, precision, consent, pending numeric input, gesture
rollback, coordinate bounds, independent source edits, history, canonical reopening and expiry.
`[shell-combined-projected-sweep]` is the thirteenth combined workflow: actual ribbon/Sketch/popup/
checkbox/button/numeric/file input authors an XZ source and XY profile, copies endpoints, adds a
direction point and shared lines, builds PathCurve/Sweep and verifies failed/corrected downstream
edits, 640→800 mm³ volumes, four-step history, canonical reopening and STEP. Twelve combined workflows
have fully UI-authored inputs; the retained Planar-motion workflow imports its seated Part example.

The inventory restores this bounded family: eleven restored, seven partial. Whole-shell acceptance
and Block 136 remain open. Repeatable commands and native captures
`sketch3d-planar-copy-controls.png` / `sketch3d-planar-copy-provenance.png` are recorded below and in
`docs/development-setup.md`.

`blcad_gui_tests` and `blcad_app` built successfully with `dev-gui`. Final verification:

| Suite | Cases | Assertions | Result |
| --- | ---: | ---: | --- |
| Focused Core Sketch3D edit/update contracts | 6 | 291 | All passed |
| Focused Geometry Sketch3D/reference/workplanes | 38 | 1,505 | All passed |
| Focused offscreen Sketch3D and inventory | 34 | 4,442 | All passed |
| Full offscreen GUI | 405 | 33,060 | 390 passed; 15 native-only skips |
| Focused native planar copies and combined Sweep | 7 | 1,021 | All passed; no skips |
| Full combined native shell | 221 | 28,286 | All passed; no skips |

The native controls and provenance captures were inspected at 1280×800: source identity, per-copy
consent, staging action, resolved XYZ and both source receipts are readable, with the pinned
Preview/Apply/Cancel footer reachable. Native logs retain the known medium depth/stencil visual
warning, without high-severity, BadDrawable or fatal lifecycle diagnostics. The separate six-second
application startup smoke reaches intentional timeout 124. Builds, GUI runs and startup were serial;
formatting and diff checks pass.

Core/Geometry sources and the persistence schema are unchanged; their full suites were not rerun
for this slice. The ordinary inventory passes. The opt-in closure gate correctly rejects unfinished
acceptance (one of three cases fails, 173 assertions). Eleven bounded families are restored and seven
remain partial; Block 136 stays open with numbered completion at Block 135. Standalone parameter
management is the next documented step.

## Standalone Part parameter management — 2026-10-23

The Model ribbon now owns an explicit parameter manager with a read-only catalog and direct graph
consumer IDs. It creates typed Length/Count/Angle literals and Length/Count expressions, edits
existing literals/formulas and collects changes locally. Complete downstream Preview precedes one
atomic Apply. Pending input, invalid numbers/names/units, cycles and failed geometry remain
correctable without publishing live intent or history. Selection/reset, Escape, dock close, foreign
commands, revisions, Save, history and document boundaries retain the session authority. Contract:
`docs/parameter-model.md`; formula grammar/loading: `docs/parameter-expression-mvp.md`.

Seven `[block136-parameters]` cases cover all literal types, formula chains, full precision,
global graph-ID collisions, direct usages, local rejection/recovery, complete downstream geometry,
accepted constraint/dimension sidecars, atomic history, canonical reopening and context/lifetime
guards. The fourteenth `[shell-combined-workflow]` case, `[shell-combined-parameters]`, uses actual
ribbon/popup/field/button/browser/file input to create a Sketch/Extrude and parameter batch, rebind
the feature to a formula, correct rejected cycles/count text, check 2400→3600→9600 mm³ volumes,
exercise three-step Undo/Redo, cancel a local batch, reopen canonically and export STEP. Thirteen
combined workflows have fully UI-authored inputs; Planar motion retains its imported seated Part.

Formula editing exposed a persistence gap: a formula can reference a later authored parameter.
Core now loads parameters in dependency-ready passes, re-derives values and graph edges, then restores
authored order. The focused Core regression covers canonical roundtrip, stale stored values,
downstream edits, duplicates, units, missing names and cycles in both strict/reference-repair modes.
There is no persistence schema change or relaxation of structural dependency errors.

`blcad_gui_tests`, `blcad_app`, `blcad_core_tests` and `blcad_geometry_tests` built with `dev-gui`.
Final verification:

| Suite | Cases | Assertions | Result |
| --- | ---: | ---: | --- |
| Full Core | 528 | 15,861 | All passed |
| Full Geometry | 437 | 29,821 | All passed |
| Full offscreen GUI | 413 | 34,495 | 398 passed; 15 native-only skips |
| Full combined native shell | 229 | 29,773 | All passed; no skips |

The final 1280×800 `parameter-manager-preview.png` and `parameter-manager-usage.png` captures in
`/tmp/blcad-parameters-native` were inspected: Name/Type/Value are the leading catalog columns,
formula and consumer text are readable, scrolling reaches local actions and the Preview/Apply/Cancel
footer stays pinned. Fixed DatumPlane records keep their normal wire presentation so unchanged
planes do not cover the body preview. Native logs retain the known medium depth/stencil visual
warning, without high-severity, BadDrawable or fatal lifecycle diagnostics. The separate six-second
application startup reaches intentional timeout 124. Builds, GUI runs and startup were serial;
formatting and diff checks pass. Repeatable commands live in `docs/development-setup.md`.

The ordinary inventory audit passes; the opt-in closure gate correctly rejects unfinished acceptance
(one of three cases fails, 177 assertions). Twelve bounded families are restored and six remain
partial. Block 136 stays open with numbered completion at Block 135. Dedicated Hole authoring and
advanced extent editors are the next documented step; literal/formula conversion, rename/delete,
Angle formulas and Assembly parameter management retain their existing Core boundaries.

## Dedicated through-all hole authoring — 2026-10-24

The Model ribbon's **Bohrungen** task now creates a single cylindrical through-hole or a complete
parametric bolt circle. It collects explicit Body/workplane/local-center inputs and typed new values
or existing literal/formula bindings. A disposable complete-model Preview validates Geometry,
material removal and retained Solid output before one Apply publishes the placement Sketch,
required parameters and Cut. This uses the frozen CircleProfile/CircularHolePattern/SubtractiveExtrude
contract, with no Core, Geometry or schema changes. Exact target, coordinate, count, bounds and
creation-only limits: `docs/gui-part-foundation-workbench-mvp7.md`.

Seven `[block136-holes]` cases cover single/circular creation; XY/XZ/YZ and tilted construction planes;
generated-face bounds; graph-wide ID/name collisions; typed literal/formula bindings, full precision,
local text retention and bounded counts; invalid/no-op/erased-solid rejection; other-body preservation;
atomic history, accepted constraint/dimension sidecars, canonical reopening and task/context expiry,
including a queued binding event after a direct session close.

`[shell-combined-holes]` is the fifteenth combined workflow. Actual ribbon, popup, field, browser,
button and file input authors an 80×60×12 plate, applies Fillet, creates four holes, edits the driving
count to six and adds a single hole sharing the existing diameter. Checks cover exact removed
volumes, retained modifier history, rejected/corrected count/no-op inputs, browser plane selection
during the task, four-step Undo/Redo, cancellation, canonical reopening and STEP. Fourteen combined
workflows have fully UI-authored inputs; the retained Planar-motion case imports its seated Part.

The inventory retains twelve restored and six partial families. Hole authoring is covered, while
advanced extent/termination editors remain a gap in the same family. Whole-shell acceptance and
Block 136 stay open with numbered completion at Block 135. The next step is the extent-editor audit
and implementation; semantic standards/thread/countersink HoleFeature intent remains separate target
architecture in `docs/hole-wizard.md`.

`blcad_gui_tests` and `blcad_app` built successfully with `dev-gui`. Final verification:

| Suite | Cases | Assertions | Result |
| --- | ---: | ---: | --- |
| Focused Core hole/Extrude contracts | 11 | 379 | All passed |
| Focused Geometry circular/extent/workplane pipelines | 46 | 2,161 | All passed |
| Full offscreen GUI | 421 | 35,800 | 406 passed; 15 native-only skips |
| Full combined native shell | 237 | 31,094 | All passed; no skips |

The final 1280×800 `hole-circle-preview.png` / `hole-single-bound-preview.png` captures in
`/tmp/blcad-holes-native` were inspected: labels are not clipped horizontally, parameter bindings
and literal fields remain readable, circular-only fields are disabled for single placement and
Preview/Apply/Cancel stay pinned while the form scrolls. Supporting planes keep their normal wire
presentation. Native logs retain the known medium depth/stencil visual warning, without
high-severity, BadDrawable or fatal lifecycle diagnostics. The separate six-second application
startup reaches intentional timeout 124. Builds, GUI runs and startup were serial; formatting and
diff checks pass. Repeatable commands live in `docs/development-setup.md`.

Core/Geometry sources and persistence schema are unchanged; their full suites were not rerun for
this slice. The ordinary inventory audit passes. The opt-in closure gate correctly rejects the
remaining acceptance work (one of three cases fails, 180 assertions). This does not close the
extent-editor gap, the six partial families or Block 136.

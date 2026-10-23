# Replacement shell acceptance — MVP-9R

Status: Block 136 in progress; inventory updated 2026-10-23. Numbered completion remains owned by
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

1. Complete native construction reference collection/manipulators. Fourteen creation/edit modes
   including generated-reference and parallel-through-point definitions, downstream recompute previews,
   resolved helper presentation and Sketch-on-workplane entry are restored;
   the exact implemented boundary lives in `docs/gui-sketch-workbench-mvp7.md`.
2. Restore explicit broken-reference repair and Sketch3D authoring/editing.
3. Restore standalone parameter management, dedicated Hole authoring and audit advanced extent
   editors against the foundation API.
4. Restore contextual commands, selection-filter UI, view cube, view bookmarks and repeat command.
5. Restore the supported hierarchy validation, cross-hierarchy relationship/joint/solve controls,
   sampled Revolute analysis and occurrence-linked result highlighting.
6. Finish integrated Part and Assembly acceptance using actual ShellWindow input paths, disposable
   previews, invalid-input rejection, history, canonical save/load and native presentation.

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
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[shell-mixed-path-loft],[block136-construction-relations],[block136-construction-edit],[shell-combined-workflow],[gui][shell-coverage],[block136-construction],[block136-inspect-export],[block136-feature-edit],[block136-assembly],[block136-measure],[block136-surface],[block136-spatial],[block136-body],[block136-patterns],[block136-finishing],[sketch-ui-review],[gui][viewport],[native-viewer-lifecycle],[shell-features]"
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

# GUI Interactive PathCurve, Sweep, and Loft MVP-9

Status: implemented in Block 127.

The replacement `ShellWindow` owns PathCurve, Sweep and Loft through `ShellFeatureTools`, the
shared task panel and the existing `GuiInteractiveFeatureCoordinator`. The Block-136 spatial slice
restores these commands, their disposable Geometry previews and atomic Apply workflows.

Block 127 upgrades the MVP-7 form-driven spatial commands
(`docs/gui-spatial-surface-workbench-mvp7.md`) into direct-manipulation authoring over the frozen
Block-70..79 contracts (`docs/part-path-curve-core-mvp6.md`, `docs/part-sweep-intent-mvp6.md`,
`docs/part-loft-intent-mvp6.md`). It consumes the Block-122 selection-first workspace and the
Block-123 candidate-only manipulator layer. No new Core or Geometry intent is introduced.

## Authority boundary

```text
ordered connected segments (path) / profile + trajectory (sweep) / ordered sections (loft)
  [+ twist parameter, guide, continuity]
  -> GuiInteractivePathSweepController | GuiInteractiveLoftController (headless)
     candidate path/feature parameter set
     Block-123 sweep-twist angle handle
     disposable PartDocument clone + recompute plan (fail closed)
  -> one commit_part_transaction (seed twist + add record)
  -> existing recompute / freshness / undo authority
```

The controllers own no `PartDocument`, `ShapeCache`, or save-format field. Both are owned by the
Block-124 `GuiInteractiveFeatureCoordinator`, which publishes the active sweep-twist handle to the
shared manipulator layer and folds the viewport candidate back into the candidate.

## PathCurve authoring

`GuiInteractivePathSweepController` in PathCurve mode collects an ordered, duplicate-free chain of
`PathSegmentReference`s (planar sketch curves, ConstructionLines, Sketch3D curves, semantic edges),
rejecting a duplicate stable key. Open/Closed closure and the orientation rule
(`ProfileNormal`/`MinimumTwist`/`FixedUpVector`) are explicit, with an optional continuity hint and
fixed up-vector. Apply adds one `PathCurve`; connectivity and tolerance are validated by the frozen
Core contract, which fails closed on a disconnected or ambiguous chain.

## Sweep authoring

Sweep mode picks a `SweepProfileReference` (closed region or open path), a trajectory `PathCurve`, and
a `Sweep`/`SweepCut`/`SweepSurface` kind. An optional twist binds an existing or candidate-only `Angle` parameter and
exposes the `sweep.twist` angle-wheel handle; an optional guide path is also supported. Providing both
a non-zero twist and a guide is rejected by the frozen Geometry contract and surfaces as a fail-closed
Apply diagnostic. Apply provisions any new result Body and twist parameter and adds one sweep feature atomically.

## Loft authoring

`GuiInteractiveLoftController` collects ordered `ProfileSectionReference`s (closed regions or open
paths) as task-list chips, with duplicate rejection and `reorder_section(from, to)` drag-reorder that
edits the persistent section order through the normal transaction path. It exposes an optional guide
rail and guide curves and a `C0`/`G1`/`G2` continuity. `G2` continuity is rejected by the frozen
Geometry contract without a verified curvature guarantee, and a loft below two sections fails closed —
both surface as authoritative diagnostics. Loft has no scalar handle; sections are chips.

## Replacement-shell PathCurve, Sweep and Loft (Block 136)

The 3D-Modell **Sweep / Loft** group provides **Pfad erstellen**, **Sweep** and **Loft** through the
shared task panel with scrollable inputs and an always-visible Apply/Cancel row. The source chooser and explicit model-pick role feed the same candidate. A transient semantic
selection scope limits native detection to the role's actual input identities, so overlapping datum
planes, profiles and path source curves cannot intercept an unrelated role. Apply/Cancel restores
the prior capability mask and semantic scope.
New result Bodies and driving Angle parameters remain disposable until Apply. Every preview resolves
and recomputes the actual Geometry; invalid input clears the ghost and disables Apply. Cancel,
Undo/Redo and Save/Open use the existing authorities and schema.

The profile context mini-toolbar also starts Sweep and Loft through their original owner actions.
It requires a verified materialized closed Sketch region and the normal command's enabled state.
The selected typed Sketch/Profile pair replaces Sweep's default source or becomes Loft's first
section, even when another profile is first in the source chooser. Loft still requires the remaining
sections before Apply. Duplicate sections remain rejected. The context adapter consumes preselection
only for a successfully started task; context Cancel, task Cancel and Escape restore it after the
owner restores its previous filter and semantic scope. Apply retains the existing single transaction.
The ribbon and Repeat entry points keep their previous default source/list behavior. Exact context
contracts and proofs live in `docs/gui-modeling-workspace-mvp9.md`.

The shared task layout propagates minimum/maximum size constraints to its scroll content. The
spatial form updates its minimum height from its preferred row layout whenever command/mode
visibility changes. Excess height scrolls instead of compressing the order buttons and overlapping
the hint text with the path chooser after switching from Sweep to Loft. Ordering buttons and their
hint share one layout group; the workflow checks complete button height and the actual gap to the
trajectory chooser.

The order controls have explicit `blcad.spatial.order.up`/`down`/`remove` identities; the continuity
chooser is `blcad.spatial.continuity`. They remain normal local-candidate controls.

PathCurve collects planar lines/arcs/splines, bounded construction lines and existing Sketch3D
line/polyline/arc/spline/helix sources. Click sources or append them from the chooser, then reorder
by dragging the list or using Hoch/Runter. Check a row to reverse its segment. Duplicate sources,
disconnected chains, invalid closure and self-intersection fail closed. Closure, orientation and the
fixed up-vector are explicit. Unbounded construction lines and projected infinite lines are not
silently converted to finite trajectories. Whole resolved paths now have canonical `path/<id>`
viewport identities, including paths made from bounded construction lines, and are selectable from
the browser. Unsupported historical paths retain their available source presentations.

Sweep selects a closed Sketch region for solid/cut output or an open PathCurve for surface output,
a trajectory and an optional guide. **Modellklick wählt** separates profile, path and guide picks.
The selected trajectory's orientation remains authoritative. The optional twist checkbox enables the
shared angle grip and strict `deg` numeric editor; numeric refinement during a drag remains authoritative
through release. A nonzero twist combined with a guide fails with the existing diagnostic. Cut mode
requires an existing visible solid target, previews its removed material and preserves its identity.
Solid and surface modes create a candidate-only Body of the matching kind.

Loft collects closed regions or open PathCurves in explicit list order, with row checks to flip normals.
Dragging rows, Hoch/Runter and Entfernen change the same candidate order. A center path, ordered guide
list and C0/G1/G2 continuity are explicit. Fewer than two sections, mixed incompatible section kinds,
misbound paths/guides and unsupported G2 clear the preview and prevent Apply.
For a center path, section `i` of `n` must have its center at normalized path-length station
`i / (n - 1)`. Differently oriented/rolled planes and distinct compatible outlines are supported;
arbitrary plane intersections are not automatically inferred. Differing edge counts are automatically
reconciled on derived Geometry copies during preview and Apply; original Sketch entities and control
points remain unchanged. Exact line/arc/rational-Bézier subdivision preserves the section contours.
The seam, normal flips and explicit order remain authoritative. Invalid/self-intersecting results,
unsupported continuity and incompatible guide anchors still clear the preview and prevent Apply.
Circular path/guide arcs retain their true length and sampled circle positions in Loft control;
splines remain 32-span approximations. See `docs/part-guided-loft-geometry-mvp6.md`.
G1 remains verified by Geometry; it is not silently substituted for G2.

The Loft task states that profiles are reconciled automatically. The complex
`[shell-loft-auto-segments]` workflow opens three synthetic fitted outlines with 8/9/6 edges on
different planes, reorders sections, cancels/restarts, rejects G2 and recovers C0, commits the Loft,
edits it, translates the resulting Body, checks exact history, canonically reopens and exports STEP.
Original Sketch JSON is checked throughout. The shared synthetic fixture models the reported
segmentation pattern without embedding a user's document in the repository.
Solid, cut and surface results use the matching Body
contract. Cut mode uses the selected existing target instead of incorrectly creating NewBody intent.

Geometry shares one bounded-path resolver between preview and execution. Planar rational splines now
use 32 deterministic line spans as existing spatial splines do; line and circular-arc spans remain
exact. This extends execution of existing spline intent without changing persistence. Raw profile
recovery remains with Finish Sketch: the chooser offers materialized profiles, including closed fitted
splines, and existing open paths. Semantic-edge path authoring, projected infinite paths and richer
section alignment remain headless/deferred capabilities.

Focused shell and Geometry proof:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-spatial]"
./build/dev-gui/blcad_geometry_tests "[block136-spatial]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-spatial]"
```

The native-only case exercises actual profile/path picks, ordered Loft section picks, and Sweep twist
drag/numeric refinement. It skips explicitly offscreen. `BLCAD_UI_CAPTURE_DIR` optionally records
preview windows.

## Mixed-path oriented-section regression — 2026-10-23

`[shell-mixed-path-loft]` in `tests/gui/gui_shell_combined_workflow_tests.cpp` opens preauthored
Sketch/plane inputs through the actual Open dialog. This is an input-consumption test: authoring the
five differently oriented planes and their rectangle/trapezoid Sketches is outside this scenario.
The imported file contains no PathCurve, Loft or Body. All subsequent authoring uses Qt controls:
collect line/spline/arc → reject the disconnected reverse arc → reverse its list row → Apply path →
collect five sections out of order → bind center path → reject/correct section order → reject G2 →
restore C0 → Apply Loft → edit middle width through the property delegate → three-step Undo/Redo →
canonical Save/Open/Save → STEP export. Rejected candidates preserve committed intent and clear the
preview; recovered candidates restore it. Volume, solid count, identities, cache cleanup and source
file preservation are checked. The same test runs offscreen and with native Qt/OCCT at 1280×800.

```bash
./build/dev-gui/blcad_geometry_tests "[mixed-path-loft]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[shell-mixed-path-loft]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-mixed-loft QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[shell-mixed-path-loft]"
```

These tests uncovered and now guard against treating circular Loft controls as two straight chords.
They extend bounded spatial evidence without closing Block 136 or adding G2/Sketch3D authoring.

## Manipulator coupling

The only scalar handle is the sweep twist. Dragging it produces a Block-123 candidate the controller
folds into the twist parameter (`apply_manipulator`); a numeric HUD override is authoritative the same
way. The ordered-section proof shows a drag-reordered loft committing a byte-identical document to a
directly ordered loft, establishing that reorder edits the same persistent intent.

## Failure policy

Every controller fails closed before any document mutation: a missing result Body without valid
candidate-only replacement inputs, a missing trajectory
PathCurve, a disconnected/duplicate path chain, fewer than two loft sections, an out-of-range reorder
index, unsupported `G2` continuity, and a combined guide-plus-twist sweep. Preview never publishes a
partial `ShapeCache`; Apply keeps the last valid result and surfaces the authoritative diagnostic.

## Focused proof

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-path-sweep]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-loft]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][ordered-section-picking]"
```

The proof covers ordered connected path collection with duplicate rejection, sweep authoring with
exact undo, the sweep-twist handle and combined guide-plus-twist rejection, loft two-section authoring
with G2 rejection, fail-closed rejection of invalid inputs, and drag-reorder/direct-order document
equivalence.

## Scope and deferrals

Bounded here and not introduced implicitly by this block:

- live **connectivity/tolerance feedback**, sweep **orientation-frame** rendering, and the loft
  section-chip list are transient viewport/task-panel presentation owned by the shell; the controllers
  own the candidate parameter set and validated preview, not pixels;
- segment/profile/section/guide **picking** resolves to already-built references; their interactive
  selection is the caller's selection responsibility (interactive Sketch3D editing is MVP-8 Block 120);
- mapping the Block-122 `part.path_curve` / `part.sweep` / `part.loft` command ids to resolved inputs
  and a manipulator frame is a `ShellFeatureTools` command-handler policy consuming the coordinator's
  path/sweep and loft controllers.

## Next boundary

Block 128 adds interactive Surface authoring (Boundary/Fill, Trim, Extend, Stitch) and closed-shell to
solid conversion over the same manipulator layer.


The Block-136 typed Sketch3D point/line editor now supplies newly authored spatial paths as well as
existing geometry. Its combined UI proof creates a shared-point chain, consumes it as PathCurve/Sweep,
then edits the source endpoint with complete downstream preview, exact history and persistence.
The point/line contract and remaining native interaction gaps live in
`docs/gui-sketch3d-interaction-mvp8.md`; this does not close whole-shell acceptance.

The spatial source editor also supports active-candidate point/line collection and direct point
translation grips with axis/plane restrictions. The combined Sweep proof now selects the source
endpoint through the viewport and moves its locked Z grip, retaining complete downstream validation,
exact history and persistence. Contracts and bounded planar-copy semantics remain owned by
`docs/gui-sketch3d-interaction-mvp8.md`.

The spatial source task additionally stages new points by anchored viewport placement. Its combined
Sweep proof now places the chain points with a locked Z pointer, refines them numerically and retains
the downstream grip/rejection/history/reopen/export sequence. Depth, copied-anchor and input-gesture
contracts are documented in `docs/gui-sketch3d-interaction-mvp8.md`.

The source task now also stages explicit distance/azimuth/elevation points relative to its anchor.
The same combined Sweep proof mixes pointer placement with direction input, including incomplete
tuple rejection/recovery, then retains downstream edits/history/reopen/export. Exact numeric and
pending-input contracts live in `docs/gui-sketch3d-interaction-mvp8.md`.

The shell additionally copies supported planar line endpoints/projected points into explicit local
Sketch3D points with source selection, per-copy consent and visible transient provenance. Geometry
resolves current model XYZ; committed points remain independent after source edits. The new actual-
input `[shell-combined-projected-sweep]` workflow authors an XZ source and XY profile, copies two
endpoints, adds a direction point and drives PathCurve/Sweep through rejected/corrected edits,
history, canonical reopening and STEP. Exact target and copy semantics remain owned by
`docs/gui-sketch3d-interaction-mvp8.md`.


## Automatic section-refinement verification — 2026-10-26

The refinement follow-up adds seven Geometry cases and one complex UI case. A local reproduction of
the supplied 8/9/6-edge document, with its Sketch records intact and only Loft intent added,
recomputes a valid solid and exports STEP successfully. Public regression fixtures use synthetic
outlines with the same segmentation pattern.

| Suite | Cases | Assertions | Result |
| --- | ---: | ---: | --- |
| Focused Geometry | 23 | 3,054 | All passed |
| Focused offscreen Loft/path UI | 12 | 2,582 | 11 passed; one native-only skip |
| Focused native Loft/path UI | 12 | 2,844 | All passed; no skips |
| Full Core | 539 | 16,579 | All passed |
| Full Geometry | 448 | 31,285 | All passed |
| Full offscreen GUI | 548 | 71,569 | 528 passed; 20 native-only skips |

The native run covers `[shell-loft-auto-segments]`, `[shell-combined-loft]`,
`[shell-mixed-path-loft]` and `[block136-spatial]`. It covers the focused offscreen exclusion;
other native-only cases outside the changed Loft/path scope were not rerun in this follow-up.
Builds, source edits, GUI runs and separate startup stayed serial. All five targets (application,
GUI/Core/Geometry tests and STEP exporter) build. Native startup remained running until the planned
six-second timeout (124). The known medium OCCT incomplete-visual depth/stencil warning remains;
no high-severity graphics, BadDrawable or fatal lifecycle errors occurred.

Native preview/result captures at 1280×800 in `/tmp/blcad-loft-auto-native` were inspected, including
`loft-automatic-unequal-spline-preview` and `loft-automatic-unequal-spline-result`. The task explains
automatic reconciliation, its full hint is readable, Apply/Cancel remain reachable and the resulting
blade-like solid is visible. Exact source Sketch JSON and original 8/9/6 counts survive the entire
GUI workflow. Changed C++ formatting and working/staged whitespace checks pass.

Focused commands live in `docs/development-setup.md`. Local evidence logs use
`/tmp/blcad-loft-auto-` with suffixes `geometry-focused.log`, `gui-focused.log`, `native.log`,
`core-full.log`, `geometry-full.log`, `gui-full.log`, `startup.log`, `user-file.log` and
`build-final.log`. Earlier development build failures are superseded by completed successful runs.
No persistence version changes or numbered STEP import progress are implied.

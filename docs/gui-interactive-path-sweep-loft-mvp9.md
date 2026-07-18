# GUI Interactive PathCurve, Sweep, and Loft MVP-9

Status: implemented in Block 127.

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
a `Sweep`/`SweepCut`/`SweepSurface` kind. An optional twist binds an existing `Angle` parameter and
exposes the `sweep.twist` angle-wheel handle; an optional guide path is also supported. Providing both
a non-zero twist and a guide is rejected by the frozen Geometry contract and surfaces as a fail-closed
Apply diagnostic. Apply seeds the twist parameter and adds one sweep feature.

## Loft authoring

`GuiInteractiveLoftController` collects ordered `ProfileSectionReference`s (closed regions or open
paths) as task-list chips, with duplicate rejection and `reorder_section(from, to)` drag-reorder that
edits the persistent section order through the normal transaction path. It exposes an optional guide
rail and guide curves and a `C0`/`G1`/`G2` continuity. `G2` continuity is rejected by the frozen
Geometry contract without a verified curvature guarantee, and a loft below two sections fails closed —
both surface as authoritative diagnostics. Loft has no scalar handle; sections are chips.

## Manipulator coupling

The only scalar handle is the sweep twist. Dragging it produces a Block-123 candidate the controller
folds into the twist parameter (`apply_manipulator`); a numeric HUD override is authoritative the same
way. The ordered-section proof shows a drag-reordered loft committing a byte-identical document to a
directly ordered loft, establishing that reorder edits the same persistent intent.

## Failure policy

Every controller fails closed before any document mutation: a missing result Body or trajectory
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
  section-chip list are transient viewport/task-panel presentation the binder renders; the controllers
  own the candidate parameter set and validated preview, not pixels;
- segment/profile/section/guide **picking** resolves to already-built references; their interactive
  selection is the caller's selection responsibility (interactive Sketch3D editing is MVP-8 Block 120);
- mapping the Block-122 `part.path_curve` / `part.sweep` / `part.loft` command ids to resolved inputs
  and a manipulator frame is a `MainWindow` command-handler policy consuming the coordinator's
  path/sweep and loft controllers.

## Next boundary

Block 128 adds interactive Surface authoring (Boundary/Fill, Trim, Extend, Stitch) and closed-shell to
solid conversion over the same manipulator layer.

# GUI Interactive Pattern, Mirror, Body Boolean, and Body Transform MVP-9

Status: implemented in Block 126.

Block 126 upgrades the MVP-7 form-driven pattern/mirror/body commands
(`docs/gui-part-operations-workbench-mvp7.md`) into direct-manipulation authoring over the frozen
Block-63..69 contracts (`docs/part-pattern-core-mvp6.md`, `docs/part-mirror-intent-mvp6.md`,
`docs/part-body-boolean-mvp6.md`, `docs/part-body-transform-ownership-mvp6.md`). It consumes the
Block-122 selection-first workspace and the Block-123 candidate-only manipulator layer. No new Core or
Geometry intent is introduced.

## Authority boundary

```text
ordered Feature/Body sources (pattern/mirror) or target + tool Bodies (boolean)
  + typed direction/axis/plane [+ count/spacing parameters]
  -> GuiInteractivePatternMirrorController | GuiInteractiveBodyOperationController (headless)
     candidate feature/transform parameter set
     Block-123 spacing/count/angle handles or translate-triad/rotate/scale handles
     disposable PartDocument clone + recompute plan (fail closed)
  -> one commit_part_transaction (seed driving parameters + add record)
  -> existing recompute / freshness / undo authority
```

The controllers own no `PartDocument`, `ShapeCache`, or save-format field. Both are owned by the
Block-124 `GuiInteractiveFeatureCoordinator`, which publishes the active handles to the shared
manipulator layer and folds viewport candidates back into the candidate. At most one interactive
command is active at a time.

## Pattern and Mirror authoring

`GuiInteractivePatternMirrorController` collects an ordered, duplicate-free set of Feature/Body
sources and authors one of:

- **Linear pattern** — a typed direction axis, an existing `Count` count parameter, `Spacing` or
  `TotalExtent` mode with an existing `Length` extent parameter, and a direction sign. The
  `pattern.spacing` linear handle drags the extent and the `pattern.count` handle edits the integer
  count.
- **Circular pattern** — a typed axis, an existing `Count` count parameter, an equal-spacing flag, and
  a literal total angle. The `pattern.angle` angle-wheel handle drags the total angle and
  `pattern.count` edits the count.
- **Mirror** — a Datum/Construction/semantic plane with a ghost preview and no scalar handle.

Count and spacing seed their existing parameters on Apply; the circular total angle is a literal Core
value. Every family produces a NewBody result; instance ghosts are a transient presentation of the
validated candidate.

## Body Boolean and Body Transform authoring

`GuiInteractiveBodyOperationController` authors either:

- **Body Boolean** — an `Add`/`Subtract`/`Intersect` operation with a target Body and an ordered set
  of tool Bodies (the target can never also be a tool, and duplicates are rejected), a
  `ModifyTarget`/`NewBody` result mode (NewBody requires an existing produced Body), and a keep/consume
  tools toggle. Boolean exposes no scalar handle.
- **Body Transform** — one `Translate`, `Rotate`, or `UniformScale` record appended to the persistent
  ordered transform stack. Translate shows the `transform.translate` triad and folds each axis into
  the translation vector; Rotate drags `transform.angle` about a typed rotation axis; UniformScale
  drags the non-negative `transform.scale` factor. Each Apply appends exactly one record and never
  collapses the stack; owned-Sketch/construction movement is an explicit per-record flag.

## Manipulator coupling

Each offered handle produces a Block-123 candidate the controller folds into the matching parameter or
literal (`apply_manipulator`); a numeric HUD override is authoritative the same way. The proof shows a
dragged linear-pattern spacing and a coordinator-routed translate-triad drag committing a
byte-identical document to the equivalent typed edit, establishing GUI/headless equivalence.

## Failure policy

Every controller fails closed before any document mutation: a missing result/target/produced Body, a
missing or wrong-typed driving parameter (`Count`/`Length`), an empty source set, a tool that is also
the target, and any value rejected by the Core/Geometry contract. Preview never publishes a partial
`ShapeCache`; Apply keeps the last valid result and surfaces the authoritative diagnostic.

## Focused proof

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-pattern-mirror]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-body-operation]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][pattern-ghost-preview]"
```

The proof covers linear/circular pattern parameter driving with exact undo, mirror authoring, boolean
role validation and keep toggle, the translate/rotate/scale transform stack (never collapsed), fail
closed rejection of invalid inputs, and manipulator/typed document equivalence for pattern spacing and
coordinator-routed transform translation.

## Scope and deferrals

Bounded here and not introduced implicitly by this block:

- **role color-coding**, instance/mirror **ghost rendering**, and the browser transform-stack display
  are transient viewport/browser presentation the binder renders; the controllers own the candidate
  parameter set and validated preview, not pixels;
- direction/axis/plane and source/tool **picking** resolve to already-built references and Body ids;
  their interactive selection is the caller's selection responsibility;
- mapping the Block-122 `part.linear_pattern` / `part.circular_pattern` / `part.mirror` /
  `part.body_boolean` / `part.body_transform` command ids to resolved inputs and a manipulator frame is
  a `MainWindow` command-handler policy consuming the coordinator's pattern/mirror and body-operation
  controllers.

## Next boundary

Block 127 adds interactive PathCurve, Sweep, and Loft authoring with ordered-section picking over the
same manipulator layer.

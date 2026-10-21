# GUI Interactive Pattern, Mirror, Body Boolean, and Body Transform MVP-9

Status: implemented in Block 126.

The replacement `ShellWindow` owns Pattern and Mirror through `ShellFeatureTools`, the shared task
panel and the existing `GuiInteractiveFeatureCoordinator`. The Block-136 pattern slice restores
these commands. The following Body-operation slice restores Boolean and Transform through the
same task, preview and transaction authorities.

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

- **Linear pattern** — a typed direction axis, a `Count` count parameter, `Spacing` or
  `TotalExtent` mode with a `Length` extent parameter, and a direction sign. The
  `pattern.spacing` linear handle drags the extent and the `pattern.count` handle edits the integer
  count.
- **Circular pattern** — a typed axis, a `Count` count parameter, an equal-spacing flag, and
  a literal total angle. The `pattern.angle` angle-wheel handle drags the total angle and
  `pattern.count` edits the count.
- **Mirror** — a Datum/Construction/semantic plane with a ghost preview and no scalar handle.

Count and spacing seed their driving parameters on Apply; these can be existing parameters or
candidate-only new inputs. The circular total angle is a literal Core value. Every family produces
a NewBody result; instance ghosts are a transient presentation of the validated candidate.

## Replacement-shell Body patterns (Block 136)

The 3D-Modell **Muster** group exposes **Lineares Muster**, **Kreismuster** and **Körper spiegeln**.
Start with selected Bodies or collect them by clicking the model or the source list. Each click
toggles one source without replacing the other picks. Only visible, recomputed solid Bodies are
listed. An empty source set removes the preview and disables Apply. Body selection works for curved
solids as well as boxes; it does not depend on the finishing tools' rectangular topology roles.

The reference chooser offers existing explicit datum axes and resolvable construction lines for
patterns, and datum/construction planes for Mirror. **Referenz im Modell wählen** switches the
viewport to datum selection; a valid pick updates the chooser and restores Body picking without
losing the source set. The same chooser remains available for precise reference selection.

Linear patterns support spacing per instance or total extent, positive/negative direction, and an
integer count including the starting placement. Circular patterns use equal angular spacing over a
positive total angle up to 360 degrees; a full circle omits the duplicated final position. Counts
range from 2 to 1000, with at most 10000 selected-Body instances per interactive operation. This is
a GUI authoring bound; the persisted feature authority is unchanged. Mirror reflects the source
Bodies across the chosen plane and exposes no scalar handle.

Spacing and circular angle use the shared numeric editor with `mm`/`deg` and viewport handles.
Count has its own integer editor and a separate, perpendicular handle. Numeric refinement during a
drag remains authoritative through release. Rejected edits restore the last valid count/value and
preview. Escape during a drag restores its start value; another Escape cancels the task.
Handles appear only with a valid preview and are suspended while choosing a reference. The ghost
remains nonselectable after visibility updates, so it cannot intercept clicks on source Bodies.

The controller accepts candidate-only count/extent parameters and a candidate-only result Body.
Preview recomputes the actual result through the same feature path as Apply. The original Bodies
remain intact; patterns include the original placement in their new result, and Mirror creates only
the reflected result. No result Body, parameters, features or history entry are persisted until Apply.
A successful Apply creates all of them atomically; one Undo removes the whole operation. Cancel,
Undo/Redo and Save/Open use the existing authorities and schema.

Focused shell proof:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-patterns]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-patterns]"
```

The native-only case detects source Bodies through actual mouse events, toggles a selected source
through an active ghost preview, and couples spacing/angle/count dragging with numeric refinement.
`BLCAD_UI_CAPTURE_DIR` optionally records preview windows. Offscreen skips the native case explicitly.

## Body Boolean and Body Transform authoring

`GuiInteractiveBodyOperationController` authors either:

- **Body Boolean** — an `Add`/`Subtract`/`Intersect` operation with a target Body and an ordered set
  of tool Bodies (the target can never also be a tool, and duplicates are rejected), a
  `ModifyTarget`/`NewBody` result mode (NewBody accepts an existing or candidate-only produced Body), and a keep/consume
  tools toggle. Boolean exposes no scalar handle.
- **Body Transform** — `Translate`, `Rotate`, or `UniformScale` records appended to the persistent
  ordered transform stack. Translate shows the `transform.translate` triad and folds each axis into
  the translation vector; Rotate drags `transform.angle` about a typed rotation axis; UniformScale
  drags the positive, dimensionless `transform.scale` factor. Each Apply appends one record per
  selected Body in one transaction and never collapses the stack; owned-Sketch/construction movement
  is an explicit per-record flag. A single-Body command retains the original one-record behavior.

## Replacement-shell Body operations (Block 136)

The 3D-Modell **Körper** group exposes **Kombinieren**, **Verschieben**, **Drehen** and **Skalieren**.
Commands offer visible Bodies with current cached shapes; Boolean requires at least two solids.
Transforms accept solid and surface Bodies. The existing World-coordinate transform contract is
used, with independent X/Y/Z translation, datum/construction rotation axes, and an explicit scale
center. Other coordinate spaces and semantic rotation edges remain headless capabilities.

Boolean separates the target chooser from a multi-select tool list. **Zielkörper im Modell wählen**
temporarily routes Body picks to the target; normal picks toggle tools. The target is unavailable as
a tool. Operation, result mode and tool retention rebuild the same disposable candidate. NewBody
provisions the result only in that candidate and in the final Apply transaction. A failed or empty
intersection clears the preview and blocks Apply without adding Bodies or history.

Transforms collect multiple Body picks in the model/list and share one set of transform inputs.
Translation uses three independent handles and fields; the handles start from the current X/Y/Z
values, including repeated drags. Rotation has a reference chooser and model axis-pick mode. Uniform
scale has a unitless factor and center X/Y/Z fields in millimeters. Its linear grip changes the factor
by one for 25 model millimeters of travel. Numeric refinement remains authoritative through release;
wrong units, invalid factors and failed Geometry candidates restore the prior valid value and preview.
Escape during a drag restores its start value; task cancellation discards all pending intent.

Owned Sketch and associated construction movement are separate, explicit checkboxes. They derive
transformed frames through the existing ownership/cache authority, without changing local Sketch
coordinates or inferring new ownership. Both default to off for every command.

All affected result shapes are recomputed together in a disposable cache. The viewport can show
multiple nonselectable preview presentations and hide multiple replaced/consumed Bodies. Boolean
NewBody retains the target; retained tools stay visible. Transform previews replace all selected
Bodies. The source list remains available to remove picks whose original presentation is hidden.
Cancel or failure restores the original scene. Apply appends the per-Body transform records in
selection-list order in a single transaction. One Undo reverses the entire batch; Undo/Redo and
Save/Open preserve the ordered stack and existing schema.

Focused shell proof:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-body]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-body]"
```

The native-only cases use actual model target/tool picks for a cut, multi-Body mouse selection,
drag/numeric refinement for translation, rotation and unitless scale, and Escape rollback. They skip
explicitly offscreen. Optional
`BLCAD_UI_CAPTURE_DIR` records preview windows.

## Manipulator coupling

Each offered handle produces a Block-123 candidate the controller folds into the matching parameter or
literal (`apply_manipulator`); a numeric HUD override is authoritative the same way. The proof shows a
dragged linear-pattern spacing and a coordinator-routed translate-triad drag committing a
byte-identical document to the equivalent typed edit, establishing GUI/headless equivalence.

## Failure policy

Every controller fails closed before any document mutation: a missing result/target/produced Body
without valid candidate-only replacement inputs, a
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
  are transient viewport/browser presentation owned by the shell; the controllers own the candidate
  parameter set and validated preview, not pixels;
- direction/axis/plane and source/tool **picking** resolve to already-built references and Body ids;
  their interactive selection is the caller's selection responsibility;
- mapping the Block-122 `part.linear_pattern` / `part.circular_pattern` / `part.mirror` /
  `part.body_boolean` / `part.body_transform` command ids to resolved inputs and a manipulator frame is
  a shell command-handler policy consuming the coordinator's pattern/mirror and body-operation
  controllers. Pattern/Mirror and Boolean/Transform are wired through `ShellFeatureTools`.

## Next boundary

Block 127 adds interactive PathCurve, Sweep, and Loft authoring with ordered-section picking over the
same manipulator layer.

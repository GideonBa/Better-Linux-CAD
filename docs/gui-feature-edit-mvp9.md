# Feature Edit Lifecycle and Core Feature-Update Commands MVP-9

Status: implemented in Block 129.

Block 129 makes editing existing persistent feature intent a first-class interaction. It adds the
missing per-family Core feature-update/remove commands and a GUI edit lifecycle that reuses the
Block-124..128 authoring controllers preloaded with the persistent intent. Unlike Blocks 124–128, this
block is explicitly allowed new Core intent, under `[core][feature-update-command]`; it introduces no
JSON shape changes and keeps ids and referenced identities stable across updates.

## Update-capability inventory

The checked-in artifact `docs/gui-feature-update-inventory-mvp9.json`
(schema `blcad.feature-update-inventory.v1`) records, for every feature family implemented through
Block 94, which public `PartDocument` mutation authorities exist. Before Block 129 only `update_sketch`
and `remove_surface_feature` (plus parameter-value edits) existed; every solid/surface feature family
otherwise had `add` only. Block 129 adds `update_*` for all thirteen feature families and `remove_*`
for the twelve that lacked it. `BodyTransform` stack records and feature reorder are explicit
non-goals recorded in the inventory.

## Core feature-update commands

```text
find record by FeatureId in its typed vector
  -> validate the replacement through the family add (same rules)
  -> keep the same id, ordered position, JSON shape
  -> preserve downstream dependency edges and the result body's co-producers
  -> fail closed with no partial mutation
```

`update_<family>(record)` replaces the record with the same `FeatureId` in the same position and
re-runs the family's add validation. The dependency graph is rebuilt for the edited feature while its
downstream dependents and the result body's other producers are preserved: the re-add is first tried
directly (so a feature that modifies a body in place re-forms its own chain) and, only if the add
rejects the body as already produced, retried with the body's co-producer edges released and restored
around the add. `remove_<family>(id)` deletes a record only when nothing downstream depends on its
result body or its intent, mirroring the existing `remove_surface_feature` protection. Both fail
closed and never partially mutate the document.

Parameter-value edits keep their existing path: changing a bound `Length`/`Angle`/`Count` parameter
re-drives every referencing feature on recompute without any feature update.

## GUI edit lifecycle

`GuiFeatureEditController` (headless) drives a double-click edit:

1. `begin(session, feature_id)` identifies the feature family (`GuiFeatureEditKind`) so the shell can
   preload the matching Block-124..128 authoring controller with the persistent intent.
2. `preview(session, replacement, updater)` validates the edited replacement on a disposable clone
   through the Core update command and the recompute plan, without mutating the document.
3. `commit(session, replacement, updater)` commits exactly one document transaction. Because the
   transaction recomputes, a downstream feature that fails after the edit fails the whole edit closed
   (last-valid rule) — there is no partial commit and no silent suppression.

The controller is generic over the Core update commands: a caller passes the edited record and the
matching `&PartDocument::update_<family>` member, so every family with a Core update command is
editable through the same boundary.

## Failure policy

`begin` fails closed on a feature id that is not present in any family. `preview`/`commit` fail closed
when the edited replacement is invalid (missing references, unknown id) or when recompute rejects the
edited feature or any downstream feature. A rejected commit leaves the edit active and the document
byte-for-byte unchanged.

## Focused proof

```bash
./build/dev/blcad_core_tests "[core][feature-update-command]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][feature-edit]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][feature-edit-recompute]"
```

The Core proof covers in-place update with stable id and position, re-validation, JSON stability, the
downstream-modifier (fillet-on-extrude) co-producer case, and dependency-protected remove. The GUI
proof covers family identification of a double-clicked feature, one update transaction with exact undo
and fail-closed rejection, and downstream recompute after an upstream edit.

## Scope and non-goals

- **BodyTransform** stack editing/removal and **feature reorder/rollback** are explicit non-goals
  recorded in the inventory; feature order is dependency-derived and no reorder contract exists.
- Preloading the persistent intent into each Block-124..128 manipulator UI and the double-click
  routing are the `MainWindow`/binder presentation of this boundary; the controllers own the validated
  edit transaction, not the widget wiring.

## Next boundary

Block 130 adds interactive Assembly placement, relationships, joints, and motion in
`docs/gui-interactive-assembly-mvp9.md`. Block 131 adds Measure, the coverage manifest v2, and
integrated acceptance.

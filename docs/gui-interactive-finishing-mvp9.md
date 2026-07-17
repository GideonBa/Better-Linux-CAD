# GUI Interactive Fillet, Chamfer, Shell, and Draft MVP-9

Status: implemented in Block 125.

Block 125 upgrades the MVP-7 form-driven finishing commands
(`docs/gui-part-operations-workbench-mvp7.md`) into direct-manipulation authoring over the frozen
Block-56..61 contracts (`docs/part-edge-treatment-intent-mvp6.md`, `docs/part-fillet-geometry-mvp6.md`,
`docs/part-chamfer-geometry-mvp6.md`, `docs/part-shell-intent-mvp6.md`,
`docs/part-draft-intent-mvp6.md`). It consumes the Block-122 selection-first workspace and the
Block-123 candidate-only manipulator layer. No new Core or Geometry intent is introduced.

## Authority boundary

```text
target Body + ordered semantic edges (finishing) or faces (shell/draft)
  + driving Length/Angle parameter(s) [+ pull axis + neutral plane for Draft]
  -> GuiInteractiveFinishingController | GuiInteractiveShellDraftController (headless)
     candidate feature parameter set
     Block-123 radius/distance/angle/thickness handles
     disposable PartDocument clone + recompute plan (fail closed)
  -> one commit_part_transaction (seed driving parameters + add feature)
  -> existing recompute / freshness / undo authority
```

The controllers own no `PartDocument`, `ShapeCache`, or save-format field. Preview builds a throwaway
document clone, seeds the driving parameter values, adds the finishing feature, and validates the
recompute plan; Apply re-validates and commits exactly one transaction that seeds the parameters and
adds the feature, so undo reverts parameter and feature together. Both controllers are owned by the
Block-124 `GuiInteractiveFeatureCoordinator`, which publishes the active handles to the shared
manipulator layer and folds viewport candidates back into the candidate.

## Edge and face chain picking

Fillet and Chamfer collect an ordered chain of `EdgeReference`s; Shell and Draft collect
`FaceReference`s. `add_edge`/`add_face` reject a duplicate using the same stable per-edge / per-face
identity the Core edge/face-treatment contracts use, so pick-time rejection agrees with feature
validation. Edges and faces without stable semantic identity are unpickable at the selection layer
(`docs/assembly-generated-topology-reference-mvp5.md`, `docs/semantic-references.md`) and never reach
these controllers as references; lost or ambiguous recovered topology enters the existing
reference-recovery/repair workflow rather than being approximated.

## Finishing authoring

`GuiInteractiveFinishingController` begins Fillet from a target Body and an existing `Length` radius
parameter, or Chamfer in one of the three frozen modes:

- `EqualDistance` — one `Length` distance parameter and a distance handle;
- `TwoDistance` — two `Length` distance parameters and two distance handles;
- `DistanceAngle` — a `Length` distance parameter plus an `Angle` parameter, with a distance handle
  and an angle-wheel handle.

The current parameter value seeds each handle (`fillet.radius`, `chamfer.distance`,
`chamfer.distance2`, `chamfer.angle`); dragging or typing folds into the candidate.

## Shell and Draft authoring

`GuiInteractiveShellDraftController` begins Shell from a target Body, an existing `Length` thickness
parameter, and an `Inward`/`Outward` direction toggle, collecting removal faces and dragging the
`shell.thickness` handle. Draft begins from a target Body, a typed `AxisReference` pull direction, a
`PlaneReference` neutral plane, and an existing `Angle` parameter, collecting draft faces and dragging
the `draft.angle` handle with the documented signed convention.

## Manipulator coupling

Each offered handle produces a Block-123 candidate whose value the controller folds into the matching
parameter (`apply_manipulator`); a numeric HUD override is authoritative the same way. The proof shows
a dragged fillet radius and a dragged shell thickness — the latter routed through the coordinator —
committing a byte-identical document to the equivalent typed edit, establishing GUI/headless
equivalence.

## Failure policy

Every controller fails closed before any document mutation: a missing target Body, a missing or
wrong-typed driving parameter (Length vs Angle), an empty edge/face chain, and any value rejected by
the Core/Geometry contract (for example a radius that no longer fillets or lost recovered topology).
Preview never publishes a partial `ShapeCache`; Apply keeps the last valid result and surfaces the
authoritative diagnostic.

## Focused proof

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-finishing]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-shell-draft]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][edge-chain-picking]"
```

The proof covers ordered edge collection with duplicate rejection and exact undo, the three chamfer
modes with their offered handles, shell direction toggle and thickness, draft pull/neutral/angle
authoring, fail-closed rejection of invalid inputs, and manipulator/typed document equivalence for
fillet radius and coordinator-routed shell thickness.

## Scope and deferrals

Bounded here and not introduced implicitly by this block:

- interactive **hover highlighting** and the derived chamfer reference side / draft angle-sign
  visualization are transient viewport presentation the binder renders; the controllers own the
  candidate parameter set and validated preview, not pixels;
- the pull-direction and neutral-plane **picking** for Draft resolve to already-built `AxisReference`
  / `PlaneReference` inputs; their interactive selection is the caller's selection responsibility;
- mapping the Block-122 `part.fillet` / `part.chamfer` / `part.shell` / `part.draft` command ids to a
  resolved target Body, driving parameters, and edge/face manipulator frame is a `MainWindow`
  command-handler policy consuming the coordinator's finishing/shell-draft controllers.

## Next boundary

Block 126 adds interactive Pattern, Mirror, Body Boolean, and Body Transform authoring with ghost
previews over the same manipulator layer.

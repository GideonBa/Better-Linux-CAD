# GUI Interactive Extrude, Path Extrude, and Revolve MVP-9

Status: implemented in Block 124.

Block 124 upgrades the MVP-7 form-driven Extrude and Revolve validation commands
(`docs/gui-part-foundation-workbench-mvp7.md`) into direct-manipulation authoring. It consumes the
Block-122 selection-first workspace and the Block-123 candidate-only manipulator layer. No new Core or
Geometry intent is introduced: every one of the seven extrude extent modes, taper, thin intent, path
direction, and the three revolve extents are the frozen Block-52..62 contracts
(`docs/part-extrude-extent-intent-mvp6.md`, `docs/part-revolve-intent-mvp6.md`,
`docs/part-feature-body-operation-mvp6.md`).

## Authority boundary

```text
profile-region preselection / Finish-Sketch handoff
  + driving Length parameter / revolve axis reference / result Body
  -> GuiInteractiveExtrudeController | GuiInteractiveRevolveController (headless)
     candidate feature parameter set
     Block-123 extent / taper / thin / angle handles
     disposable PartDocument clone + recompute plan (fail closed)
  -> one commit_part_transaction (seed driving parameters + add feature)
  -> existing recompute / freshness / undo authority
```

The controllers own no `PartDocument`, `ShapeCache`, or save-format field. They compose existing public
Core operations (`set_parameter_value`, `add_feature`, `add_revolve_feature`, `create_recompute_plan`)
on a throwaway clone for preview and inside one transaction for Apply. `GuiInteractiveFeatureCoordinator`
is a Qt-free connector owned by `MainWindow`; it publishes a controller's handles to the shared
`GuiViewportManipulatorLayer` and routes manipulator candidates back into the active controller. The
manipulator shell (`MainWindow`) forwards its candidate callback into the coordinator, so a viewport
drag folds into the candidate without any Core authority living in a widget.

## Extrude authoring

`GuiInteractiveExtrudeController` begins from a materialized profile region, an existing `Length`
distance parameter (whose current value seeds the extent arrow), and an existing result `Body`. It
exposes:

- operation toggle `NewBody | Join | Cut | Intersect` (modifying modes target an existing Body);
- all seven extent modes: `Distance`, `Symmetric`, and `TwoSided` drag the driving distance
  parameter(s); `ThroughAll` and `ToNext` carry no scalar handle; `ToFace` and `Between` consume
  pre-resolved semantic `FaceReference`s and fail closed when a limit face is missing;
- optional taper (an angle-wheel handle) and optional thin-wall intent (`OneSided | TwoSided |
  MidPlane`) driven by an existing `Length` thickness parameter with a non-negative thickness handle;
- path extrude: `set_path_curve(...)` switches to `ExtrudeDirection::Path` and authors a
  `create_additive_path_extrude` feature previewing the trajectory (no scalar extent).

The extent arrow (`extrude.extent`, Linear), taper wheel (`extrude.taper`, Angular), and thin handle
(`extrude.thin`, Linear) are only offered when the current mode uses them. Apply seeds the driving
distance and thickness parameter values and adds the feature in one transaction, so undo reverts the
parameter change and the feature together.

## Revolve authoring

`GuiInteractiveRevolveController` begins from a materialized profile region, an `AxisReference`
(datum axis, construction line, or semantic axis/edge), and a result `Body`. It exposes the
`Revolve | RevolveCut` kind, the operation toggle, and the three frozen extents: `Full` (360°, no
handle), signed partial `Angle` with an explicit side, and `Symmetric` total angle. Partial and
symmetric modes offer the `revolve.angle` angle-wheel handle seeded with the current magnitude.
Revolve angles are literal Core values, so no parameter is seeded; Apply adds the revolve feature in
one transaction.

## Manipulator coupling

Dragging an offered handle produces a Block-123 candidate whose scalar value the controller folds into
the matching parameter (`apply_manipulator`). A numeric HUD override on the manipulator layer is
authoritative in exactly the same way. The proof shows a dragged extent and a dragged revolve angle
committing a byte-identical document to the equivalent typed edit, establishing GUI/headless
equivalence for the new commands.

## Failure policy

Every controller fails closed before any document mutation: missing or non-materialized profile
region, a missing or non-`Length` distance parameter, a missing result Body, a missing second
distance parameter for `TwoSided`, missing limit faces for `ToFace`/`Between`, a missing revolve axis,
and any extent whose value is rejected by the Core contract (for example a zero or 360° partial
revolve angle). Preview validates a disposable clone through the recompute plan and never publishes a
partial `ShapeCache`; Apply re-validates and commits exactly one transaction.

## Focused proof

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-extrude]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-revolve]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][extrude-revolve-manipulator]"
```

The proof covers extent-parameter driving with one atomic transaction and exact undo, operation/taper/
thin/through-all modes with their offered handles, fail-closed rejection of invalid inputs, full/angle/
symmetric revolve authoring, manipulator/typed document equivalence for both features, and
`MainWindow` ownership of the coordinator committing a viewport-style drag.

## Scope and deferrals

Bounded here and not introduced implicitly by this block:

- the **bolt-circle hole** workflow keeps its dedicated MVP-7 command
  (`GuiPartFoundationWorkbench::apply_hole_workflow`); Block 124 does not re-implement it;
- `ToFace`/`Between` consume already-resolved semantic `FaceReference`s; interactive limit-face
  **picking/highlighting** is the caller's selection responsibility;
- continuous **shaded viewport preview** rendering is the existing recompute/presentation path after a
  validated candidate; the controllers publish the validated preview summary, not pixels;
- mapping the Block-122 `part.extrude` / `part.revolve` / `part.path_extrude` command ids to a fully
  auto-provisioned driving parameter, result body, and resolved profile-plane frame is a `MainWindow`
  command-handler policy consuming `GuiInteractiveFeatureCoordinator::begin_extrude` /
  `begin_revolve`.

## Next boundary

Block 125 adds interactive Fillet, Chamfer, Shell, and Draft over the same manipulator layer and the
supported-topology picking boundary.

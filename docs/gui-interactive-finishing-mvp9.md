# GUI Interactive Fillet, Chamfer, Shell, and Draft MVP-9

Status: implemented in Block 125.

The replacement `ShellWindow` owns these tools through `ShellFeatureTools` and its existing
`GuiInteractiveFeatureCoordinator`. The Block-136 finishing slice restores the four commands in the
**Bearbeiten** ribbon group; other Block-136 feature families remain open.

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
validation. The edge picker retains supported producer-owned roles and exposes remaining finite edges of the
current Body through producer-anchored geometric witnesses (`ResultEdgeReference`), including
curved rims and subsequent finishing results. Edges without a finite unambiguous witness and
faces without stable semantic identity are unpickable at the selection layer
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

## Replacement-shell workflow (Block 136, first slice)

**Verrunden**, **Fase**, **Aushöhlen** and **Formschräge** open the existing nonmodal Feature task
panel. Choose the target solid Body, then toggle one or more edges/faces in the viewport or target
list. Only stable references matching exactly one subshape of the current Body are offered; lost or
ambiguous roles are omitted. An empty selection keeps Apply disabled. Changing the Body clears the
old subshape selection. Ordinary commands remain unavailable while a feature or Sketch owns the task.

The primary value supports the shared drag handle and numeric editor with `mm` or `deg`. Fase
supports equal distance, two distances, and distance/angle, including the secondary numeric field
and corresponding controller handle. Aushöhlen exposes inward/outward direction. Formschräge exposes
existing explicit datum axes as pull directions and datum planes as neutral planes. These controls
all rebuild the same disposable candidate; no dialog owns a second document or feature authority.

Both controller families use the shared `gui_pending_feature_inputs` validation/provisioning helpers
for candidate-only driving parameters and produce actual recomputed Body shapes for the viewport.
The replaced Body is hidden during a valid preview, so removed material
is visible; selection presentations stay transient. Invalid edits restore the previous valid numeric
candidate. Clearing the selection removes the preview and disables Apply. Escape/Cancel removes
handles, selection presentations and pending parameters, restores the previous selection filter and
semantic scope, and leaves history and serialization unchanged. Apply recomputes again and commits parameters plus the
feature atomically; one Undo removes the whole operation. Save/Open retains the existing schema.

Stable box roles now also accept a four-line `ClosedProfile` rectangle produced by the shell, in
either traversal direction and with the source Sketch workplane/origin. The same Core rectangular
profile authority feeds producer classification, semantic edge/vertex evaluation and generated face
workplanes. Rectangles must align with their Sketch axes and use a normal, untapered, non-thin distance
extrusion. Other source shapes and missing current-body matches are unavailable rather than being
assigned a transient OCCT index. General curved topology selection and additional Draft reference
families remain later extensions.

Focused shell proof:

```bash
./build/dev-gui/blcad_core_tests "[rectangular-finishing]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-finishing]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-finishing]"
```

The native case detects actual OCCT edges/faces from mouse events and drives their handles with a
numeric override. `BLCAD_UI_CAPTURE_DIR` optionally records preview windows for visual inspection.
Offscreen skips that native-only case explicitly.

## Replacement-shell selection contexts — 2026-10-24

Select the **Kanten** or **Flächen** model filter before picking an idle stable subshape. The shared
`ShellContextTools` toolbar offers Verrunden/Fase for exactly one verified Edge and
Aushöhlen/Formschräge for exactly one verified Face. All retains its existing Body/profile picking.
The current `FinishingSelectionBuilder` verifies the selected id, Body identity and command-specific
role against the fresh cache. Missing, hidden, ambiguous and unsupported targets offer no command.
The existing rectangular/stable-role Geometry boundary above remains unchanged.

A context command starts the original QAction, seeds the exact typed Body and one offered target,
then consumes preselection. The normal task keeps all parameter/preview/Apply authority. It temporarily
owns a semantic scope containing only that Body's offered targets, plus its Edge/Face filter. Body
changes rebuild both targets and scope. Apply, task Cancel, context Cancel and Escape restore the
prior filter and scope; cancellation also restores the complete still-valid consumed selection.
Ribbon/Repeat starts retain their ordinary fresh selection defaults. No context or selection state
is persisted, and successful Apply remains one atomic history entry.
The finishing form groups each label/editor in a complete layout row and preserves preferred row
heights, including wrapped hints, inside the shared scroll area after command/mode switches. The mode
and secondary-value editors remain complete and reachable at 1280×800.

Proof: `[block136-finishing-context]` and the native Qt-authored
`[shell-combined-finishing-context]`. The latter combines two Bodies, Draft cancellation, Shell,
Fillet, both cancelled and applied Chamfer modes, batched upstream edits, earlier-feature editing,
five-step exact Undo/Redo, canonical reopening and STEP. It explicitly skips offscreen because its
idle source selection must hit actual OCCT subshapes; the focused logical lifecycle tests run on both
platforms. Commands and capture paths are in `docs/development-setup.md`.

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
- mapping `part.fillet` / `part.chamfer` / `part.shell` / `part.draft` to a target Body, candidate-only
  parameters and an edge/face manipulator frame belongs to the shell-owned `ShellFeatureTools`.

## Next boundary

Block 126 adds interactive Pattern, Mirror, Body Boolean, and Body Transform authoring with ghost
previews over the same manipulator layer.

## Curved current-result workflow

`[curved-finishing]` exercises circular rim selection, a real OCCT fillet, a subsequent chamfer on
the current result and strict saved-reference replay. `[curved-union-workflow]` extends the GUI
sequence with radius refinement, cancel/preview isolation, Undo/Redo, a second overlapping cylinder,
Union, consumed-tool checks and Save/Open. Under native xcb it selects the first circular edge through
real viewport mouse events. Kernel-invalid radii/distances still reject Apply without mutation.


## Complex-result selection construction — issue #114 follow-up

Result-edge catalog construction samples each unique current edge once per build, then checks the
sampled witnesses for ambiguity. Previously every offered edge caused a fresh resampling of the
entire Body. No catalog is retained across model changes. Selection and persistent recovery share
the same curve-kind, length and complete 17-point forward/reverse comparison at the existing
absolute tolerance; ambiguous, changed and non-finite witnesses remain unavailable. Producer IDs,
reference spellings and native picking behavior remain unchanged. Fillet and Chamfer enablement
also use this builder, so its cost can affect other actions while the shell refreshes.

Finishing previews participate in the shared candidate-cache contract in
`docs/gui-modeling-workspace-mvp9.md`. The representative rotor profile and actual shell workflow
exercise curved current-result edges and radius recovery without weakening the existing reference
or geometry validation rules.

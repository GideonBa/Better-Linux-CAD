# GUI Interactive Assembly Placement, Relationships, Joints, and Motion MVP-9

Status: headless authorities implemented in Block 130; replacement-shell integration restored in
the Assembly slice of Block 136 (22 October 2026).

Block 130 upgrades the MVP-7 Assembly validation workbench into direct-manipulation authoring over
the existing component-instance, relationship, joint-coordinate, solve, and vector-drive contracts.
It consumes the Block-122 selection-first workspace and Block-123 candidate-only manipulators. No
new Core, Geometry, or persistence intent is introduced.

## Authority boundary

```text
shared Part / child Assembly + occurrence identity
  -> GuiInteractiveAssemblyPlacementController
     translate/rotate triad + transient state candidate
  -> one Project transaction
     [register root member Part] + add/edit occurrence

first semantic target + relationship/joint family
  -> resolve typed target capabilities
  -> compatibility-filtered second targets
  -> disposable relationship solve / oriented joint-frame preview
  -> one Project transaction

selected active joint + one coordinate role
  -> slider or in-viewport handle
  -> one-role AssemblyJointDrive + live posed solve result
  -> AssemblyJointMotionResultApplier in one Project transaction
```

The controllers own only transient candidates. `Project`, `AssemblyDocument`, target resolution,
compatibility matrices, numeric solving, stale-result validation, serialization, and undo remain the
authorities of their existing layers.

## Component and subassembly placement

`GuiInteractiveAssemblyPlacementController` inserts a project-owned shared Part or child Assembly and
edits an existing occurrence. A Part selected for first insertion is registered as a root Assembly
member in the same atomic transaction. Six Block-123 handles expose XYZ translation and rotation;
typed values use the same candidate transform. Component grounding, visibility, and suppression and
subassembly visibility/suppression are carried by the candidate and remain one Apply transaction.

Preview validates a throwaway `Project` and never changes the live Assembly. Insert and edit preserve
the existing `ComponentInstance` / `SubassemblyInstance` placement and state formats.

## Compatibility-filtered relationship authoring

`GuiInteractiveAssemblyRelationshipController` resolves the first semantic endpoint with
`AssemblyConstraintTargetResolver`. `compatible_second_targets(...)` resolves each pick candidate and
keeps only different-component endpoints accepted by `AssemblyTargetCompatibilityResolver`; final
selection repeats the same fail-closed check. Thus the highlight filter and Apply validation consume
the same Block-37 capability matrix.

Preview adds the relationship to a disposable Project, obtains rank/remaining-DOF diagnostics, and
runs the real rigid-body solver for the two-component connected group. A non-converged candidate is
rejected. Apply adds the relationship and applies that solved pose through the snapshot-validating
applier in one Project transaction, so neither intent nor pose can commit alone.

## Joint authoring and frame preview

`GuiInteractiveAssemblyJointController` uses the joint compatibility resolver for the same two-stage
selection policy. Revolute, Prismatic, Cylindrical, and Planar require oriented Frame/Frame targets;
Spherical uses Point/Point. The preview returns both resolved targets and root-space oriented frames
where the selected family exposes them. Apply stores the existing typed `AssemblyJoint` and coordinate
slots without persisting any resolved descriptor or viewport frame.

## Coordinate controls, motion preview, and HUD

`GuiInteractiveAssemblyMotionController` exposes every ordered coordinate slot as a typed control with
role, Angular/Linear kind, authored value, lower/upper limits, and selected state. One selected slot
maps to one angular or linear in-viewport handle. Slider, numeric, and drag input all build a
single-coordinate `AssemblyJointDrive`; omitted coordinates retain their authored values through the
existing vector-drive semantics.

Motion preview runs `AssemblyJointMotionSolver` without mutation. Its HUD projection reports limits,
the joint's coordinate DOF count, undriven coordinate DOF, solve state, proposed-component count, and
final RMS. Apply delegates the successful preview to `AssemblyJointMotionResultApplier`, which checks
component, semantic-Part, and joint-input snapshots before atomically applying the posed transforms
and the selected persistent coordinate. Spherical remains passive and is rejected as a selected
drive.

## Failure policy

The block fails closed for a missing Project/definition/occurrence, malformed or unresolved semantic
target, same-component endpoint pair, capability-incompatible second target, invalid relationship or
joint slot set, failed relationship/motion solve, stale solve result, inactive or missing joint,
unknown coordinate role, value outside existing limits, and selected Spherical drive. Preview never
mutates the document; Apply is the only mutation boundary.

## Focused proof

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-assembly-placement]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-relationships]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-joint-motion]"
```

The proof covers component and rigid-subassembly insertion, six-axis triad input, state editing and
undo, compatibility-filtered Mate selection, solved-pose preview and atomic relationship Apply,
Frame/Frame joint preview, limit-bearing coordinate controls, one-coordinate in-viewport motion,
live solve/HUD status, atomic coordinate/pose Apply, and Spherical drive rejection.

## Scope and deferrals

General constrained free-drag is not introduced. It needs a Geometry drag-objective solve mode that
does not exist in the current contracts. These direct-manipulation controllers operate on root-Assembly occurrences so one existing local
solve/application boundary remains atomic. The former MVP-7 cross-hierarchy validation workbench was
removed by the shell reset; its Core/Geometry contracts remain, but cross-hierarchy authoring is not
exposed by this replacement shell.

## Next boundary

Block 131 adds read-only Measure, the coverage manifest v2, and integrated MVP-9 acceptance in
`docs/interactive-modeling-mvp9-acceptance.md`. Block 132 starts GUI Shell Reset MVP-9R
(`docs/gui-shell-reset-sequence-mvp9r.md`).

## Replacement-shell integration — Block 136

`ShellWindow` owns `ShellAssemblyTools` and the shared transparent manipulator overlay. The
**Baugruppe** ribbon creates a Project, loads definitions, inserts Part/rigid-subassembly occurrences,
edits placement and states, creates relationships/joints, drives one joint coordinate, and solves an
explicit root-component group. The scrollable task panel keeps Apply/Cancel reachable.

Definition loading imports a Part or a Project's root/child Assembly definitions in one transaction.
An identical already-owned Part can be reused; conflicting Part identities and duplicate Assembly
identities reject the whole import. Projects with cross-hierarchy intent are rejected before import
because that intent cannot currently be transferred to a child definition without remapping.

Placement uses XYZ translation arrows or one selected X/Y/Z rotation ring, together with six numeric
fields. A root component can be picked in the model; a root subassembly is selected by name in the
panel. A preview Project supplies actual posed leaf shapes for the changed occurrence/subtree.
Visibility/suppression can publish a replacement-only preview with no ghost shapes. Other occurrences
stay pickable. Escape restores the complete transform at drag start; a second Escape cancels the
command. Typed input during a drag accepts mm/deg and Enter finishes the drag. Grounding is a solver
state; free placement does not automatically solve existing relationships.

`AssemblyInteractionGeometry` enumerates resolvable semantic root targets over visible active
components: generated faces/edges/vertices, supported cylindrical axes and seat frames, and existing
datum/construction references. Actual generated planar faces and disposable point/axis/frame glyphs
follow occurrence transforms. Their ids are presentation-only; authored endpoints retain existing
component ids and Core semantic-reference spellings. There are no persisted OCCT topology indices.
The second endpoint chooser and native selection whitelist share the controller compatibility matrix.
Only the current pick role's targets and the selected first reference are presented.

Revolute, Prismatic, Cylindrical and Planar expose their existing ordered typed slots. Cylindrical
orders linear translation before angular rotation; Planar orders U, V, normal rotation. All driven
slots require both limits; angular limits stay in the principal -180..180 degree range. Spherical is
passive with no coordinate rows and does not appear in the motion chooser. Motion uses a slider,
numeric field or one native handle; an invalid requested value restores the last valid candidate.
Relationships, motion and group solve display actual solved-pose previews and diagnostics. Relationship
authoring retains the controller's two-component connected-group boundary; group solve requires the
complete selected connected group. Nested/cross-hierarchy endpoint authoring remains deferred.

Every task records its source Project snapshot. External intent changes, stale geometry, document
replacement, Undo/Redo or cancellation discard its targets, ghosts and handles. Apply repeats authority
validation and commits one Project history entry. Selection filters are restored on exit. No file
schema, solver equations or semantic-reference contracts change.

```bash
./build/dev-gui/blcad_geometry_tests "[block136-assembly]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-assembly]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-assembly]"
```

The shell proof covers definition loading, component/subassembly insertion, placement/state preview,
all five joint families, compatibility-filtered relationships, group solve, limits, motion,
Undo/Redo and Save/Open. Native cases exercise occurrence picking and translate/rotate/motion handles
with numeric refinement and drag cancellation. Geometry proof checks posed semantic glyphs, unique
identities, valid OCCT shapes and unchanged authored JSON.

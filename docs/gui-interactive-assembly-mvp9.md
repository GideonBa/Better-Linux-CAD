# GUI Interactive Assembly Placement, Relationships, Joints, and Motion MVP-9

Status: headless authorities implemented in Block 130; replacement-shell integration restored in
the Assembly slices of Block 136 (22–25 October 2026), including occurrence-qualified hierarchy
authoring, solving and motion. Whole-block acceptance is tracked in `docs/mvp-plan.md`.

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
does not exist in the current contracts. The direct-manipulation controllers operate on root-Assembly
occurrences so one existing local solve/application boundary remains atomic. The replacement shell
also exposes the existing occurrence-qualified hierarchy authorities through dedicated author/edit,
solve and typed-motion tasks, described below. Hierarchy motion uses its coordinate field and slider;
it does not introduce a cross-hierarchy free-drag solver.

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
complete selected connected group. The dedicated hierarchy commands below operate on exact rooted
occurrence paths and their existing shared transform authorities.

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

## Hierarchy tasks and shared occurrence authority — 25 October 2026

The same `ShellAssemblyTools` owner exposes five additional ribbon commands. **Hierarchie prüfen**
validates the complete Project structure through Core and reports the deterministic root/child
occurrence paths, visibility/suppression and active visible leaf count. The task is read-only;
Apply is disabled and inspection adds no history entry.

**Hierarchie-Beziehung** and **Hierarchie-Gelenk** offer a New/Edit selector, a name and active state,
then the same family-specific input controls. Edit loads the existing exact ID, ordered endpoint
paths, scalar/coordinate values and limits. Apply replaces that record atomically; a new command
allocates a free Project-level ID. Candidate endpoints contain a typed
`AssemblyHierarchyConstraintEndpoint`, never a path reconstructed from a display label. Repeated
occurrences of the same child definition remain separate choices with different rooted paths.

Visible active paths supply the supported semantic faces, edges, vertices, axes, seat frames and
reference geometry already accepted by `AssemblyInteractionGeometry`. Local glyph geometry is reused
per definition and transformed through each parent chain; `AssemblyHierarchyConstraintTargetResolver`
resolves the actual Project endpoint. Presentation IDs encode complete occurrence paths and are
transient. The second-reference chooser and current native pick scope expose the same compatible
candidates. Mate/Distance/Angle/Concentric/Insert/Coincident/Parallel/Perpendicular use the hierarchy
equation builder. Revolute/Prismatic/Cylindrical/Planar use Frame/Frame targets and their existing typed
coordinate slots; passive Spherical uses Point/Point. Unresolved, hidden or suppressed targets are
absent, and incompatible inputs or malformed values prevent Apply.

Authoring previews validate geometry and Core structure on a Project copy. Apply stores intent in one
Project transaction. Explicit **Hierarchie lösen** computes placement: its list contains complete
deterministic connected groups from `AssemblyCrossHierarchyConstraintGraph`, including their local
relationships. Exactly one group is selected. Rank/remaining-DOF diagnostics and final RMS accompany
the posed preview. Only a converged result can be applied through
`AssemblyCrossHierarchySolveResultApplier` in one transaction.

**Hierarchie bewegen** lists active non-Spherical Project-level joints and their ordered coordinate
roles. Numeric and slider input drive one selected role through `AssemblyJointDrive` and
`AssemblyCrossHierarchyJointMotionSolver`; omitted roles retain their authored values. Invalid limits,
unknown roles or a non-converged result block Apply and leave the source unchanged. Apply passes the
fresh result through `AssemblyCrossHierarchyJointMotionResultApplier`, updating the selected
coordinate and resulting poses atomically. No geometric solve, preview, glyph or graph cache is
serialized.

Both solvers retain the existing shared `ComponentTransformAuthority` contract. A child component's
internal transform is definition-owned, so all repeated occurrences show that new internal pose.
Subassembly boundary transforms remain unchanged. The UI states this consequence alongside the
solve/motion preview. It does not provide independent internal poses for repeated rigid instances or
remap imported Project-level cross intent into child definitions.

The existing source snapshot, freshness checks and task ownership cover all hierarchy commands.
Save writes accepted intent while keeping a draft active. Cancel restores the prior selection filter
and semantic scope; document replacement or external source mutation discards the candidate. Failed
Apply leaves accepted intent and history unchanged. Targets disappear after task exit unless an
explicit idle AssemblyTargets filter requests supported root semantic targets.

Five selection-first context actions also enter the same owner: a visible root component seeds
Placement or its complete connected Solve group; Motion is offered when exactly one active drivable
root joint touches that component. An explicit root AssemblyTarget seeds Relationship or Joint.
Consumed preselection is restored on cancellation through the shared context lifecycle.

The focused `[block136-assembly-advanced]` GUI cases exercise hierarchy reporting, exact occurrence
paths, relationship editing and shared-authority solves, all five joint families, typed motion for
all four driven families, limit/invalid-text recovery, history, canonical persistence and source
lifecycle. A multi-step Planar workflow applies U, V and normal rotation separately, rejects an
out-of-limit intermediate value, preserves all undriven roles, and traverses all three accepted poses
with Undo/Redo before Save/Open. A native case checks transformed target geometry and an actual viewport pick through a
translated, rotated repeated parent while retaining the exact endpoint path. The complex workflow
suite adds actual ribbon, browser, chooser, numeric, Apply and file
input across repeated child definitions; acceptance evidence is recorded in
`docs/gui-shell-acceptance-mvp9r.md` after verification.

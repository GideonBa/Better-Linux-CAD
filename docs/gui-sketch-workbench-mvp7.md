# GUI Datum, Workplane, and Planar Sketch Workbench MVP-7

Status: implemented in Block 99.

## Authority boundary

`GuiSketchWorkbench` exposes the implemented planar Core contracts through `GuiDocumentSession`
transactions. Widgets never mutate `Sketch` storage directly. `PartDocument::update_sketch` validates
a replacement with the same rules as initial insertion, preserves authored sketch order and
downstream dependency edges, marks affected graph nodes stale, and publishes the replacement only
after the complete candidate is valid.

The workbench covers datum planes/axes, construction geometry, derived workplanes, planar sketch
creation/editing, lines, arcs, splines, circle/rectangle and explicit closed profiles, trim/extend,
projected and reference-generated geometry, implemented constraints/dimensions, ordered profiles,
profile/region inspection, Core diagnostics, suggestions, preview, and repair.

Each operation has a capability-specific selection prompt. Numeric sketch-plane coordinates remain
available through the workbench and initial `Sketch` menu. The plane mapper converts between model
and local 2D coordinates. Entering sketch edit mode switches the OCCT camera to orthographic
projection along the resolved workplane normal and uses the workplane Y axis as up.

The Block-98 browser now expands planar sketches into entities, projected references, constraints,
dimensions, trim/extend records, profiles, and Core diagnostic entries. Stable entity presentation
ids continue to synchronize browser and viewport selection.

## Repair policy

Diagnostics and suggestions are read-only until the user explicitly previews a repair. Preview
applies the existing executor to a sketch copy. Apply repeats it inside one document transaction,
so recompute, dirty state, undo, and redo remain atomic. Unsupported suggestions cannot be silently
applied.

## Verification

```bash
./build/dev-gui/blcad_core_tests "[core][sketch-update]"
./build/dev-gui/blcad_gui_tests "[gui][datum-workplane]"
./build/dev-gui/blcad_gui_tests "[gui][sketch-workbench]"
./build/dev-gui/blcad_gui_tests "[gui][sketch-repair]"
```

Coverage includes atomic replacement/order preservation, construction and projected geometry,
planar entities, constraints, coordinate mapping, semantic prompts, diagnostics/repair preview,
undoable repair, and normal-to-plane camera activation.

## Block-110 direct-manipulation integration

The MVP-7 Sketch workbench remains a validation/transaction client over historical Sketch intent. Block
110 does not move live drag authority into `GuiSketchWorkbench`. `GuiSketchDragController` consumes
Block-108 topology and Block-109 solving; successful release enters the same
`GuiDocumentSession::commit_part_transaction(...)` authority used by validation workbenches.

The final solved topology is materialized and re-migrated exactly before `PartDocument::update_sketch`.
Live handle positions, pointer samples, temporary drag constraints, and preview Sketches remain
transient. This preserves MVP-7 atomic recompute/undo semantics while adding direct manipulation.

## Replacement-shell construction authoring — 2026-10-23

`ShellWindow` explicitly owns `ShellConstructionTools`. **3D-Modell → Konstruktion** provides
**Konstruktionsgeometrie** and **Arbeitsebene**, opening one nonmodal task with nine creation modes:

- Explicit XYZ point, line through a point with a direction, and datum axis with an origin/direction.
- Construction line through two existing construction points, and datum axis from a construction line.
- Explicit plane with an origin and perpendicular X/Y directions, a positive offset from an existing
  datum/construction/derived plane, and a plane through three existing construction points.
- Derived workplane on a supported generated semantic face. The picker offers only faces resolved by
  `WorkplaneResolver`; the rectangular additive-extrude topology contract remains authoritative.

Coordinates use millimeters; direction vectors are normalized before Core frame validation. Zero
directions, nonperpendicular plane axes, coincident/collinear defining points, empty names and missing
references disable Apply and clear the preview. Source roles use explicit task selectors with stable
document identities. The initial slice uses selectors; native reference collection is added below.
The manipulator slice below adds direct editing of explicit definitions and new offsets.

Preview adds intent to a disposable Part copy and resolves it through the existing construction and
workplane authorities. The viewport displays only the new candidate shape. Neither preview nor
Cancel changes intent, history, dirty state or save data. Offset planes may bind an existing Length
parameter (including an expression) or provision a new positive Length parameter. A new parameter and
its plane enter one `commit_part_transaction`; one Undo removes both. Existing parameter bindings
are never overwritten. Id allocation avoids existing objects and the generated parameter id.

Apply consumes pending numeric editor text, then revalidates inputs and revision before the
transaction. Escape, dock close,
document replacement, Undo/Redo, stale Geometry or a session presentation revision change discard the
task and preview. Saving during preview writes committed intent and expires the task. Project and
active-Sketch contexts reject construction authoring, and a construction task blocks sketch entry.

`ViewportSceneBuilder` now presents resolved construction points, relation-driven lines/planes,
construction-line-derived axes and derived workplanes. Browser and viewport identities agree.
Selecting a resolvable construction or derived plane in either surface enables **2D-Skizze erstellen**;
camera mapping, bounded face checks and downstream feature recompute use the existing workplane
contracts. These changes add no persistence fields or raw OCCT identities.

Additional generated-reference/parallel modes and native reference collection are recorded below.
The later manipulator slice extends direct editing. Offset parameter value
and formula edits remain available through the existing parameter property inspector. Reference
diagnostics/repair retain their separate acceptance gap.

Proof: `[block136-construction]` in `tests/gui/gui_shell_construction_tools_tests.cpp` and
`[geometry][construction-presentation]` in `tests/geometry/viewport_scene_tests.cpp`. Commands and
native/offscreen accounting live in `docs/development-setup.md` and
`docs/gui-shell-acceptance-mvp9r.md`.

## Replacement-shell construction editing — 2026-10-23

Double-click a construction object, datum axis or derived workplane in the Part browser to reopen
its existing definition in `ShellConstructionTools`. The nine restored creation modes can now be
edited in place: names, explicit coordinates/directions, defining points, source lines/planes,
offset-parameter bindings and semantic faces. Type and persistent identity stay fixed; principal
origin geometry remains fixed. The additional generated-reference and parallel definitions are
recorded in the subsequent relation slice below.

The task loads the current references, preserves relation IDs and explicit parameter dependencies,
and retains original full-precision coordinates/frame components when their displayed fields are
unchanged. An existing offset binding displays its evaluated value read-only, including expressions.
Choose another existing Length parameter to rebind, or **Neuer Parameter** for a different positive
offset. The latter allocates a collision-free parameter ID/name together with the object update in
one transaction; the previously bound parameter and its other consumers remain intact. Unused old
parameters are retained. Direct value/formula changes continue to use the parameter inspector.

Core typed `update_*` commands replace construction intent at its original vector position through
the corresponding validated add path. They rebuild incoming dependencies, preserve outgoing edges,
reject cycles, mark the object changed and its consumers dirty, and publish only a complete valid
candidate. Core remains independent of Geometry and does not claim to validate downstream shapes.

Editing previews recompute a disposable complete Part into a fresh ShapeCache and resolve the scene.
The viewport temporarily replaces the affected helpers/Sketch entities and candidate Body results;
Cancel restores committed presentation. Degenerate downstream geometry, invalid references or cycles
clear the preview and disable Apply. Apply repeats validation/recompute through the existing session
transaction. History, stale-revision cancellation, Escape/close, document replacement and committed-only
Save semantics are the same as creation. The JSON loader resolves forward construction/workplane
dependencies while preserving authored array order, so same-ID rebinds retain exact history and
canonical roundtrips. No persistence schema or raw kernel IDs are added.

Proof: `[core][construction-update]`, `[block136-construction-edit]`, including
`[shell-combined-workplane-edit]` with real ribbon, browser, task, numeric, history and file-dialog
input on offscreen and native xcb. The subsequent relation, source-picking and manipulator slices
extend this boundary.

## Replacement-shell generated references and parallel relations — 2026-10-23

The construction task adds five creation and same-type editing modes, bringing the restored total
to fourteen:

- **Punkt auf Modellkante (Mitte)** stores a semantic edge reference and evaluates its midpoint.
- **Punkt auf Modellecke** stores a semantic vertex reference.
- **Linie parallel zu Linie durch Punkt** reuses a construction line's evaluated direction at a
  selected construction point.
- **Linie parallel zu Modellkante durch Punkt** uses a supported semantic edge's direction at the
  selected point.
- **Ebene parallel zu Ebene durch Punkt** retains a source workplane's frame at the evaluated point.

Dedicated **Modellkante**/**Modellecke** selectors list only references resolved by the existing
semantic evaluator, labeled by source feature and semantic element. The current supported topology
is rectangular additive-extrude intent; arbitrary kernel edges/vertices and edge parameterization
beyond midpoint remain unsupported. Source roles use stable document IDs and enum tokens, never
transient OCCT identities. Existing definitions preload the exact feature/element pair when reopened.
The type selector uses a bounded width so long definition names do not push field labels off the
left edge; the combined 1280×800 UI proof checks horizontal task fit.

Generated points can feed two-point lines, three-point planes and parallel definitions, including
axes from those lines and Sketch/Extrude consumers on those planes. Workplane resolution evaluates
all defining points through `ConstructionPointResolver`; stored placeholders must not become plane
origins. Core retains its early explicit-point collinearity check and validates generated source
features. Geometry rejects coincident/collinear evaluated points during disposable preview. The JSON
loader waits for generated source features and point-dependent inputs, preserving saved point and
other authored collection order without changing the schema.

The existing preview, revision, transaction, cancellation, Undo/Redo and canonical Save/Open contracts
apply to all five modes. Point/line/plane rebinding revalidates the complete candidate, so a generated
point cannot be rebound to an extrusion that already depends on it. Invalid or absent references,
degenerate downstream geometry and cycles clear preview and disable Apply; recovery stays within the
task. The subsequent native reference collection and manipulator slices extend this boundary.

Proof: `[core][construction-generated]`, `[geometry][construction-generated]`,
`[shell-construction-relations]` and the seventh combined UI scenario
`[shell-combined-construction]`. Validation and commands remain in
`docs/gui-shell-acceptance-mvp9r.md` and `docs/development-setup.md`.

## Replacement-shell native construction reference collection — 2026-10-23

The construction task's **Modellklick wählt** selector makes each applicable reference role explicit:
source plane, source construction line, defining points 1–3, generated face, edge or vertex. A model
click updates the same stable reference as the corresponding input selector. Defining-point picks
advance to the next role; roles can always be revisited explicitly. Parallel-through-point modes
expose both inputs. Explicit coordinate/direction definitions retain numeric input and offer no
reference-pick role.

A task-local semantic scope activates only sources offered for the current role. Overlapping datum
planes, Bodies and unrelated topology cannot steal its pick. Generated-source presentations are
limited to the active definition's applicable kinds. Selectors and presentations share one typed
`ViewportSceneBuilder::build_construction_sources` catalogue: six bounded faces, twelve straight
edges and eight vertices for each supported recomputed rectangular additive-extrude source. Missing,
unsupported or hidden-Body sources are omitted. These are source-feature semantic references evaluated
by the same authorities as the resulting construction relations/derived workplanes, not inferred
final-Body subshape ordinals or automatic remaps through later finishing/transforms. Ordinary existing
construction points/lines and datum/construction/derived planes use their established scene IDs.

Create and same-ID Edit use identical picking. Preview remains disposable; repeated defining points,
cyclic rebinding and invalid downstream geometry still clear the preview and disable Apply. Apply,
Cancel, Escape, dock close, revision/freshness expiry and committed-only Save restore the previous
capability mask and semantic scope and remove transient source presentations. The additional role
selector and transient catalogue add no Core intent or persistence fields.

Proof: `[block136-construction-picking]` / `[shell-construction-picking]` in
`tests/gui/gui_shell_construction_tools_tests.cpp`, plus `[geometry][construction-picking]` in
`tests/geometry/viewport_scene_tests.cpp`. The native case clicks actual projected model locations at
1280×800 through point/plane, parallel edge/point, generated vertex and generated face workflows,
four-step history and canonical reopen; it explicitly skips offscreen. Other cases exercise roles,
repeated-point recovery, semantic editing, hidden sources and custom-scope restoration on exit.
Verification lives in `docs/gui-shell-acceptance-mvp9r.md`; reproducible commands live in
`docs/development-setup.md`. The following manipulator slice completes this bounded datum surface;
whole-block acceptance remains open.

## Replacement-shell construction manipulators — 2026-10-23

**Griffe** selects the editable quantity in the existing construction task:

- Explicit point/line/axis/plane: World XYZ position arrows with absolute coordinate values in `mm`.
- Explicit line/axis: World-axis rotation rings for the direction; explicit planes rotate both frame
  axes together, preserving an orthonormal frame. **Ebenenrahmen drehen** changes the complete frame,
  not independent rounded axis components. Rotation values are relative to drag start, in `deg`.
- Offset plane with **Neuer Parameter**: one positive offset arrow along the evaluated source-plane
  normal. Existing shared/expression parameter bindings retain their read-only value; choosing a new
  parameter enables a separate candidate offset without overwriting the old parameter.

Reference-driven definitions retain their source-pick controls; they do not receive independent
coordinate grips that would change the definition kind. Arrows/rings have fixed DIP sizes and axis
labels. Rotation ring radii are separated so all three world axes remain independently selectable.
The transparent shell-owned overlay is `blcad.construction.manipulator_overlay`; the role and numeric
controls are `blcad.construction.manipulation` and `blcad.construction.grip_numeric`.

Dragging updates the same disposable candidate as the numeric fields. Keyboard focus moves to
**Griffwert** during a drag: a valid number with an optional matching `mm`/`deg` suffix overrides
later motion/release. Values respect existing coordinate/positive-offset bounds. Invalid text keeps
the last accepted candidate but disables Apply; correcting it recovers, while release with invalid
text restores the drag start. Release consumes the exact final pointer sample. No drag writes intent,
history or dirty state. Apply remains the single validated transaction.

Escape first restores the complete pre-drag candidate, including displayed fields; a second Escape
cancels the task. Window deactivation, resize or a changed camera aborts the drag. Preview redraws
retain the frozen drag frame. Save, revision/freshness expiry, document replacement, task close and
Cancel remove grips and release capture through the existing task lifetime. Rotated frame vectors
and manipulated coordinates/offsets retain full precision behind the six-decimal fields; unchanged
fields in later edits preserve that precision and existing dependencies. No new persistent fields or
Core/Geometry intent types are introduced.

Proof: `[shell-construction-manipulators]` exercises XYZ authoring, composed rotations, exact frame
precision, shared offset bindings, dependent degeneracy/recovery, capture/expiry and final release
through Qt mouse/key events. `[shell-combined-manipulators]` is the eighth fully UI-authored combined
workflow: rotated workplane → Sketch/Extrude → same-ID plane movement with downstream Body preview
→ history → canonical reopen → STEP. All these cases execute offscreen and natively. The camera
regression additionally preserves restored bookmark values without a native viewer so the headless
mapping follows the requested camera. Commands/results live in `docs/development-setup.md` and
`docs/gui-shell-acceptance-mvp9r.md`.

The datum family's bounded command surface is restored. Block 136 remains open for the other recorded
families and integrated acceptance. Explicit broken-reference repair is the next implementation step.


## Replacement-shell explicit reference repair — 2026-10-23

`ShellWindow` owns `ShellReferenceRepairTools`. **3D-Modell → Referenzen → Referenzen reparieren**
opens a nonmodal task listing every projected point, projected line and generated-face workplane in
an open Part. Each entry reports its current semantic source and the actual Geometry resolver result
(`Aufgelöst` or `Verloren` plus the failure message). Resolved references can be deliberately rebound
as well. The command requires an idle Part context with a cache or explicit repair mode; it rejects Project,
active-Sketch and occupied-task contexts. A stale flag alone does not disable the repair command.

A point may be rebound to a resolvable construction point or supported semantic vertex; a line to a
resolvable construction line or supported semantic edge. A face workplane may be rebound to a
supported semantic face. Generated choices use the existing typed rectangular additive-extrude source
contract and omit hidden source Bodies. Projected candidates also pass `SketchReferenceProjector`,
so an edge projecting to a point is excluded. The original target is excluded and no replacement is
preselected. Source identities are stable feature/role or construction IDs; kernel subshape indices
never become persistent intent.

**Gesamtvorschau** adds a selected replacement to the visible repair list, then applies the whole
list to a disposable Part, validates graph dependencies, recomputes downstream features and builds
the candidate presentation. **Hinzufügen** can stage more changes before that full recompute. Apply
stays disabled until that succeeds. Changing either selector discards the preview and requires another explicit Preview.
**Anwenden** revalidates and executes one `GuiDocumentSession::commit_part_transaction`. Projected
entity IDs, Sketch order, constraints, generated-line consumers and workplane IDs/names are retained;
Core rebuilds the changed dependency edges. A regression exposed a missing cycle guard in
`PartDocument::update_sketch`; it now rejects the candidate before publishing any state. One Undo/Redo
restores the whole repair list. Rebinding changes the selected consumer's authored source, rather than interpreting a historical remap record
as a global rewrite rule. It adds no schema fields.

Preview and diagnostics leave history, dirty state and persisted intent unchanged. Cancel, dock close,
Escape, document replacement, history commands and revision expiry remove the task and transient
presentation. Save during Preview writes committed intent and expires the task. Failed/cyclic
candidates clear the preview and cannot publish. Their messages stay in the task so a failed
candidate does not itself expire the task via a session diagnostic revision. A too-small replacement
face is rejected when its downstream profile exceeds the workplane bounds; a valid reversed plane
recomputes its dependent solid. The shared session task excludes competing tools.

This is a bounded repair surface, not completion of the entire planar/reference family: documents
rejected during initial Core/source validation still cannot be opened for repair. Explicit repair
opening now admits Core-valid Parts whose complete Geometry recompute fails. The
complete repair list must make the candidate scene/recompute valid. Arbitrary topology
matching, construction-relation remapping and constraint-diagnostic suggestion controls remain
separate work. The batch extension below permits multiple changes before complete recompute. The
coarse Core `ReferenceRecoveryEvaluator` status is not used as a substitute for Geometry diagnostics. Stored historical status/remap records are not rewritten.

Proof: three `[core][sketch-reference-repair]` cases and seven `[block136-reference-repair]` shell cases.
The dedicated `[shell-reference-repair-input]` case uses actual ribbon/button/popup mouse input on
imported intent, repairs two references, cancels another preview and checks history/save/reopen.
The other cases cover missing/wrong-kind/cyclic sources, construction fallbacks, face placement,
selection-change invalidation, stale/revision/context guards and canonical JSON. Imported fixture
setup is explicit; this is not a claim of a wholly UI-authored modeling acceptance workflow.


## Replacement-shell repair lists — 2026-10-23

The same task now collects multiple projected point/line and generated-face-workplane replacements.
**Hinzufügen** stages a selected change in **Vorgemerkte Änderungen**, keyed by reference kind,
owner Sketch/workplane and entity ID. Generated targets show the feature name and a German
face/edge/vertex role; tooltips and item data retain the exact semantic identity. Repeating a reference
replaces its queued choice; equal entity IDs in different Sketches remain distinct. **Entfernen** removes the selected list item, while
**Liste leeren** restores the original disposable candidate. Selecting an item only changes which
entry Remove targets. Every content or source-selector change revokes the previous preview.

Staging rebuilds a Part copy from committed intent and all queued changes, using existing Core
source validation, same-ID updates and cycle guards. An invalid stage leaves the previous list and
candidate intact, with Apply disabled and the error shown locally. Staging does not require valid
complete Geometry yet: one repaired workplane can enable projected-reference choices while another
broken workplane still prevents a complete scene. The target selectors evaluate the staged intent;
the committed source/status remains visible, with a separate `Vorgemerkt` annotation. The existing
cache supports provisional target selection; only a new full recompute validates the final result.

**Gesamtvorschau** first stages the current selection when one exists, then recomputes the complete
list and presents all affected workplanes/Sketches plus downstream solid/surface outputs. The visible
list is the exact scope of the preview and Apply. A failure leaves that list editable without
publishing or changing dirty/history state. Apply repeats complete validation and publishes all
queued replacements in one transaction (`Repair references` for multiple entries, the existing
`Repair reference` for one). Single-selection Preview remains supported. One Undo/Redo restores the
whole batch; Save/Open retains canonical authored intent without any persistent queue fields.

Cancel, close, Escape, save, history commands, document replacement and revision expiry discard the
entire list and transient presentation. Saving during staging/preview writes committed intent. The
repaired candidate is never copied directly into the session outside the transaction authority.

The batch is ordered: each intermediate Core update must validate. It does not bypass cycle checks
to permit temporary cyclic graph rewrites, open Core-invalid documents, or implement new topology/constraint repair kinds. The repair-open
extension below supports failed initial Geometry recompute. These boundaries keep the family partial and Block 136 open.

Proof: six `[block136-reference-repair-batches]` cases in
`tests/gui/gui_shell_reference_repair_tests.cpp`. They cover two lost planes with dependent projected
inputs, identical entity IDs across Sketches, upsert/remove/reset, invalid cyclic stages, complete
preview replacement scope, a failed two-solid bounds candidate followed by a valid reversed-plane
batch, all task exits and single-entry compatibility via the retained tests.
`[shell-repair-batch-input]` uses actual ribbon/popup/button/list mouse input: partial repair failure →
five staged changes across two planes/two Sketches → remove/restage → whole preview → native Fit →
one Apply → Undo/Redo → canonical save/reopen → clear/cancel. Input fixtures are explicitly imported.


## Explicit repair-open integration — 2026-10-23

**Zur Reparatur öffnen** retains a Core-valid Part whose initial full recompute fails and immediately
opens this task. The session exposes failed status and no authoritative ShapeCache; the shell clears
old derived geometry and blocks ordinary modeling/Sketch/property editing. Diagnostics and typed
replacement choices use disposable successful producer results, rebuilt on staged intent. One partial
plane repair may still fail full Preview until its dependent consumers and other lost planes are
repaired. A complete valid list can be previewed and published atomically through the dedicated
session repair transaction, restoring normal modeling.

Undo back to the original failed snapshot clears repaired Geometry and re-enters repair mode; Redo
recomputes the repaired Part. Cancellation leaves the loaded failed intent intact. Save during Preview
writes committed original intent and expires the task. The full session/history/source-validation and
persistence boundaries are documented in `docs/reference-recovery-mvp.md`. Missing Core sources remain
rejected and explicitly tracked as a separate gap.

Proof: `[block136-repair-open]` in `tests/gui/gui_shell_reference_repair_tests.cpp`. The imported
fixture has two plane-driven solids that fail initial recompute. The real file dialog opens it for
repair, followed by partial-preview failure, two staged plane replacements, whole Preview/Fit/Apply,
Undo/Redo with different derived presentations, and canonical Save/Open through file controls.
Session authority proofs are `[repair-open-session]` in `tests/gui/gui_document_session_tests.cpp`.

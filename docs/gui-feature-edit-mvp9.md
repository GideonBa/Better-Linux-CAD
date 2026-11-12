# Feature Edit Lifecycle and Core Feature-Update Commands MVP-9

History extension: accepted Blocks 146–148 in [the consolidated sequence](inventor-parity-sequence.md)
add suppression, validated reorder and End of Part editing through the existing browser/editor.
Their current contracts live in `feature-system.md`, `shape-cache-mvp1-data-model.md` and
`user-interface.md`. The MVP-9 non-goals below remain the historical feature-update boundary;
BodyTransform stack editing remains separate from the implemented history commands.

Status: headless lifecycle implemented in Block 129; replacement-shell browser editing restored
in the Feature Edit slice of Block 136 (22 October 2026).

> **GUI Shell Reset MVP-9R:** Block 133 deleted the Qt shell this document names — `MainWindow` and
> all seven self-installing binders — and replaced it with `ShellWindow` + `ShellRibbon`
> (`src/gui/shell/`, contract: `docs/gui-shell-reset-sequence-mvp9r.md`). The headless authorities and
> the Core/Geometry contracts below are unchanged and are consumed by the new shell; read every
> "`MainWindow`" and "binder" below as the shell's Qt presentation layer, whose current form is
> Block 135/136 work.

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
downstream dependents and the result body's other producers are preserved. With the same result
Body, the typed adder receives the original immediate Body predecessor, then the original terminal
producer is restored. A reference to an older producer does not replace the immediate predecessor.
Updates at the start, middle and end of an in-place chain retain exact Body edges; new input
references replace old inputs without retaining stale dependencies. This prevents downstream
modifiers from being reattached as predecessors and creating a cycle. `[revolve-update-history]`
checks axis rebinds, graph-equivalent reopening and atomic invalid-input rejection.
When rebinding to a different result Body, the direct/released-co-producer validation paths remain.
`remove_<family>(id)` deletes a record only when nothing downstream depends on its
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

## Replacement-shell browser editing — Block 136

`ShellWindow` routes a double-click on a current Part's Feature node to its explicitly owned
`ShellFeatureEditTools`. The family-specific task title and input form preload existing persistent
intent for Extrude, Revolve, Sweep, Loft, Surface, linear/circular Pattern, Mirror, Fillet, Chamfer,
Shell, Draft and Body Boolean. Sketch activation keeps its existing route. Project-owned definitions
require their own Part editing context. Individual BodyTransform stack entries remain outside this
edit boundary and fail without starting a task.

The editor exposes names, scalar values, typed parameter bindings, existing reference choices,
booleans and ordered inputs. Edge membership, section/boundary/source order and guide lists have
add/remove/up/down controls. A required list can be emptied and then refilled within the same
edit; its invalid intermediate state disables Apply. Optional Sweep/Loft guides, twist, orientation, up vector, alignment and
rotation-offset inputs can be enabled against existing document definitions. Reference choices retain
their stored source category. Feature type, extent-mode discriminators and result-body identities are
fixed context; the editor updates inputs within that existing intent structure. It does not introduce
feature conversion, reorder, rollback or transform-stack mutation authorities.

A parameter row selects a compatible existing binding and displays its value with mm/deg/count units.
Literal values are editable; computed values display their formula and remain driven by it. Shared
parameter changes affect every use, including expressions and downstream geometry. After rebinding,
abandoned edits to parameters no longer referenced by this feature are discarded. Only declared
parameter fields count as dependencies; matching display names or other strings do not. Parameter
changes and the feature update share one transaction. No parameter or feature is silently duplicated.

The **Dynamischer Wert** chooser connects one scalar to the shared native manipulator layer. Dragging,
typed refinement and direct numeric fields rebuild the same candidate. Enter finishes a drag;
Escape restores its starting value, and a further Escape cancels the edit. The scrollable task inputs
keep Apply/Cancel reachable at 1280×800. Reference and ordered-input changes use form controls;
model picking is suspended while this edit task owns the viewer.

Internally, the existing canonical record codec translates preloaded form values into typed Core
records. `GuiFeatureEditController::candidate` applies prepared parameter changes and the matching
`update_*` to a disposable original Part. It requires the replacement's exact edited FeatureId.
The shell recomputes the complete candidate with `GeometryRecomputeExecutor`, then displays actual
visible result Bodies, including downstream modifiers. It never publishes a rewritten whole-document
record as the edit authority. Apply repeats preview validation and the same prepared update through
one `GuiDocumentSession` transaction. Unrelated intent, feature identity and graph ordering remain
under Core authority; the file schema is unchanged.

An invalid input or downstream Geometry failure disables Apply and restores the authored scene.
The document stays unchanged and the user can correct the candidate. Cancel, task Escape, Undo/Redo,
document replacement and stale/source-snapshot changes discard previews and restore viewer filters.
A failed commit leaves the edit available for correction. Normal browser/property selection survives
refreshes through its existing semantic identity mechanism.

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-feature-edit],[gui][feature-edit],[integration][feature-edit-recompute]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-feature-edit]"
```

Proof covers preloading and atomic updates for all thirteen families, upstream edits with downstream
Fillet recompute/failure, literal and expression parameter semantics, unused-binding edit disposal,
ordered Loft sections, Fillet edge membership, optional Sweep twist, stale inputs, exact Undo/Redo
and Save/Open. Native proof performs actual tree mouse double-clicks, drags the scalar handle, refines
it numerically, restores another drag with Escape and clicks the visible Apply button at 1280×800.
Whole-block replacement-shell acceptance remains separate work.

## Hole and Thread property bridge — 157A

The existing editor now identifies typed Hole/Thread records and calls `update_hole_feature` /
`update_thread_feature`; browser double-click and History Edit share this boundary. Existing
thirteen-family behavior and legacy cuts retain their own editing path. Current 157A scope and
fixed support/resource fields are owned by [Hole/Thread](hole-wizard.md#property-editing--157a-design-and-boundary).
The full Wizard remains 157B/C. Numerical controls follow parameter units/formulas, and complete
Geometry preview/Apply preserves downstream validation and one transaction. Existing viewport
preview authority regenerates cosmetic graphics from the candidate Bodies and hides replaced
annotations. Support-anchored grips remain assigned to 157B.

Focused proof: `[hole-thread-edit-shell]` in
[the current shell tests](../tests/gui/gui_shell_hole_thread_edit_tests.cpp), including actual
native keyboard/Apply input under `[native-hole-thread-edit]`. `mvp-plan.md` owns the executed
verification and parent-block status; this bridge alone does not close Block 157.

### Semantic Plain Hole creation — 157B1

Model → Design → Hole uses the same panel/controller with the typed Core add command. A canonical
serialized candidate supplies the initial form; the committed Part remains the freshness authority.
Only used pending dimensions are added on Apply. The draft's support is a typed PlaneReference;
changing target clears it and requires another compatible choice. Body/support pick scopes are
owned by this task, and callbacks copy incoming selections before changing viewport state.

Create Another commits once and opens a fresh identity. Cancel restores the consumed selection,
camera and previous pick scope when the entry model revision is still current. Existing edits keep
their update path and pinned fields. The [Hole contract](hole-wizard.md#semantic-creation--157b1-bounded-design)
owns the current creation limits. Focused proof is `[hole-create-shell]` /
`[native-hole-create]` in the same current shell test file; numbered acceptance remains in the plan.

### Recipe transitions and dimension sources — 157B2a

The same editor composes Hole recipe, termination, tip and custom internal Thread transitions using
`GuiHoleRecipeCommands`. Its disposable typed Part result reseeds the canonical form; committed
freshness remains unchanged. Required pending dimensions are added for creation and existing-feature
editing, while explicit changes to shared parameters remain part of that same transaction. Source
controls select compatible pinned cells or convert a resolved source to an owned custom dimension.
[The Hole contract](hole-wizard.md#recipe-and-dimensional-source-controls--157b2a) owns exact limits;
the existing recipe/source and native test tags provide focused proof. Full Thread/support selectors,
manipulators, Sketch return, presets and legacy conversion remain assigned to 157B2b/B3/C.

### External Thread creation — 157B2b

`begin_new_thread` joins `begin_new_hole` in the shared controller's creation lifecycle. A candidate
must retain the fresh identity and match the creation kind; typed add commands publish one
`Create Thread` or `Create Hole` transaction, including used pending inputs. The shell shares
initialization, committed-source freshness, preview, pick scope, context restoration and final
publication. External support dimensions come from Geometry inspection; the disposable custom
seed uses `GuiThreadRecipeCommands`. Existing feature updates retain their update commands.

The [Hole/Thread contract](hole-wizard.md#external-creation-and-new-modeled-templates--157b2b) owns
defaults, supported representations and the remaining selectors/Create Another boundary.
`[thread-create-shell]` and `[native-thread-create]` exercise new internal/external modeled geometry,
actual scoped face picks and transactional history; run existing creation/edit regression alongside.

Thread source/start/taper controls use `GuiThreadRecipeCommands` over the same typed Hole/Thread
records. Mapping drafts live only in the form; complete choices become a `ThreadStandardProfile`.
Commands filter scalar edits against the resulting record's dependencies before its Core update,
so an explicitly removed optional input cannot leak an invalid unused value. Existing shared and
formula bindings retain their established ownership. See [source and interval controls](hole-wizard.md#thread-sources-and-optional-intervals--157b2b)
and `[thread-source-shell]` / `[native-thread-source]`; remaining limit/context scope and combined
acceptance stay assigned.

Existing Hole/Thread support selectors evaluate the feature's exact predecessor history in a
separate checked input cache. Scoped preview targets show that geometry during picks; they retain
typed references and never publish preview IDs. Existing references are preserved until explicitly
rebound, and the full downstream candidate remains the preview/Apply authority. Cancel restores
the entry context. See [the support-edit contract](hole-wizard.md#existing-support-editing--157b2b)
and `[hole-support-edit-shell]` / `[native-hole-support-edit]` for the visible-solid boundary and
retained extended-context scope.

Hole To Face and Up to Body add required typed limit selectors and scoped model picks to that same
lifecycle. Native/imported/result face choices use the existing Core extent/serialization authority;
Body choices exclude the target. Cleared or invalid candidates preserve the committed model and
remain correctable, including a target change that invalidates the old limit. See
[the limit contract](hole-wizard.md#explicit-hole-limits--157b2b) and `[hole-limit-shell]` /
`[native-hole-limit]` for geometry, source freshness, transactional and native proof.

New external conical seeds choose a positive inspected entry and bound a narrowing interval before
an apex. Thread scalar controls retain nine decimal places for exact support/profile compatibility.
Complete rejected profile mappings retry after a used scalar/binding correction; rebuilding retains
Thread numeric focus. Existing definitions retain their explicit bindings/directions. See the
[conical combinations contract](hole-wizard.md#conical-thread-combinations--157b2b) and
`[thread-cone-shell]` / `[native-thread-cone]` for combined source, interval and physical proof.

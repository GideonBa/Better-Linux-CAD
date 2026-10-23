# GUI Parameter, Body, and Foundational Part Workbench MVP-7

Status: implemented in Block 100 for every currently persistent Core authority.

`GuiPartFoundationWorkbench` provides typed Length/Angle/Count parameters, expression creation and
editing, Solid/Surface Body creation and transient active-Body selection, Extrude/Extruded Cut with
all existing extent/direction/body-operation intent, Revolve/RevolveCut, atomic bolt-circle hole
creation, and body-result inspection. Capability-specific prompts state profile, axis, Body, and
unit requirements.

Feature preview validates a copied `PartDocument` and derives its recompute plan without publishing
intent or shape-cache changes. Apply repeats the validated operation through one
`GuiDocumentSession` transaction. The acceptance scenario creates a deterministic solid tutorial
Part entirely through these GUI application services, recomputes it, saves it, reloads it, and
recomputes it again.

## Replacement-shell through-all holes — Block 136

`ShellWindow` explicitly owns `ShellHoleTools`. The Model ribbon's **Bohrungen** action
(`blcad.action.hole`) opens `blcad.shell.hole_task`. The task creates an independent placement
Sketch and a historical `SubtractiveExtrude` with an in-place Cut Body context. Single placement
uses `CircleProfile`; a complete circular placement uses `CircularHolePattern`. Both use the
existing axis-directed through-all Geometry executor. This restores the bounded foundational cut
workflow; semantic standards/thread/countersink HoleFeature intent remains the separate target in
`docs/hole-wizard.md`.

Availability requires a fresh Part, no repair session, no planar Sketch editor and no active task.
The target selector offers visible cached Solid Bodies with at least one Extrude-family Feature
in their Body history. The Core still requires an Extrude-family `target_feature` for this historical
cut; selecting an arbitrary unrelated producer would misrepresent dependencies. Existing modifiers
such as Fillet remain in the Body chain and the cut operates on their current cached result. Legacy
feature-only solids, hidden/Surface Bodies and Bodies with only Revolve/Sweep/Loft ancestry are
outside this bounded task. Other bodies retain their intent and geometry.

Choose a resolvable datum/construction/generated-face workplane, then enter sketch-local center X/Y
in mm. A single hole uses its diameter; a full circular pattern also uses radius, Count and a signed
finite start angle in degrees. Through-all extends across the target bounding interval along the
plane normal in both directions; it does not add a blind-depth or side selector. Geometry retains
its per-hole checks on bounded generated faces. Unbounded planes retain the existing circular-cut
contract: no additional minimum wall/spacing or per-instance material-intersection policy is
invented. A complete preview must remove a positive material volume and retain a nonempty Solid
result (volume tolerance `max(1e-9 mm³, original_volume * 1e-10)`); a wholly ineffective cut or an
erased target is rejected.

Diameter/radius accept positive Length values; count accepts whole values from 1 to 1000, matching
the shell's bounded pattern authoring limit. Bare numeric input uses a decimal point, never silent
clamping/rounding. Each field can bind to an existing compatible literal or computed parameter;
bound values are read-only and preserve their formula and precision. Returning to **Neuer Wert**
restores that field's earlier local text. Disabled circular-only fields do not affect single-hole
candidates. New names use expression-compatible `hole_N_diameter/radius/count`; the first free
namespace reserves all generated Feature/Sketch/Profile/Parameter IDs against the global dependency
graph and parameter names. Existing records are never overwritten.

All form changes invalidate the old viewport preview and Apply permission. **Vorschau** creates the
complete definition in a disposable session transaction, validates/recomputes the whole model,
checks the resulting target and displays candidate Body results. Supporting planes and sketches
retain their normal wire presentation so workplanes cannot fill the visible hole openings.
Invalid text or geometry stays correctable; live intent, cache, dirty state, history,
constraint/dimension sidecars and files remain unchanged. **Anwenden** requires
a current successful preview, rechecks it and publishes the new Sketch, required parameters and
cut in one Part transaction/Undo entry. Existing parameter bindings create no duplicate values.

Body/workplane selectors are always explicit; compatible browser/viewport selection can fill them
before or during the task through the owned selection bridge. Other selections cannot inject
foreign identities. Escape in child controls, dock close, Cancel, Save, Undo/Redo, revision/task/context
changes and document replacement discard candidates and previews. Cleanup never cancels a foreign
task or changes selection filters/cameras. There are no polling timers or widget/action discovery.

Proof: `[block136-holes]` in `tests/gui/gui_shell_hole_tests.cpp` and actual-input
`[shell-combined-holes]` in `tests/gui/gui_shell_combined_workflow_tests.cpp`. Contracts for generic
Feature Edit and Part parameters remain `docs/gui-feature-edit-mvp9.md` / `docs/parameter-model.md`;
this task is dedicated creation, not a new profile-position editor or semantic Hole Wizard.

## Suppression boundary

Persistent Part-feature suppression is not represented by the current Core model or JSON format;
the Block-63 contract explicitly forbids inventing a hidden flag. The GUI therefore exposes the
required capability as unavailable and fails closed with a precise diagnostic. It does not present
a switch that recompute would ignore. Completing suppression requires a separate Core/persistence/
recompute contract before a GUI can authorize it.

## Verification

```bash
./build/dev-gui/blcad_gui_tests "[gui][parameters]"
./build/dev-gui/blcad_gui_tests "[gui][part-foundation]"
./build/dev-gui/blcad_gui_tests "[gui][extrude-revolve]"
```


## Explicit reference-repair state

A Core-valid Part opened through the explicit failed-recompute repair path blocks ordinary Part,
constraint, dimension and Sketch transactions until a complete repair succeeds. The dedicated repair
transaction retains the same full recompute/publication authority. Rejected Part candidates preserve
the current intent, cache and freshness status. Failed-snapshot history and file semantics are owned
by `docs/reference-recovery-mvp.md`; the supported repair UI is in `docs/gui-sketch-workbench-mvp7.md`.

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

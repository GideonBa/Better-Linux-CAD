# GUI Part Operations Workbench MVP-7

Status: implemented in Block 101.

`GuiPartOperationsWorkbench` exposes all currently implemented conventional Part-operation
authorities: Linear/Circular Pattern, Mirror, Fillet, Chamfer, Shell, Draft, Body Boolean, and
associative Body Transform. Capability-specific prompts preserve ordered source/tool/edge/face
lists and identify required axes, planes, directions, parameters, coordinate spaces, and Body modes.

Every preview applies normal Core validation to a copied `PartDocument` and reports the candidate
recompute-plan size. It cannot publish intent, presentation revisions, or partial Body-cache results.
Apply repeats the operation in one `GuiDocumentSession` transaction. Missing or lost semantic
topology fails closed and remains visible through the existing diagnostics/browser boundary.

The acceptance scenario creates multiple Bodies through GUI application services, patterns a Body,
composes it with a Body Boolean, applies an associative transform, recomputes, saves, reloads,
recomputes, and exports all visible fresh Body results through the existing deterministic STEP/XDE
exporter. Export is rejected while results are absent or stale.

## Verification

```bash
./build/dev-gui/blcad_gui_tests "[gui][part-pattern]"
./build/dev-gui/blcad_gui_tests "[gui][part-finishing]"
./build/dev-gui/blcad_gui_tests "[gui][body-operation]"
```

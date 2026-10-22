# GUI Analysis and STEP Export Workbench MVP-7

Status: implemented in Block 104.

`GuiAnalysisExportWorkbench` exposes read-only local solve/DOF inspection, posed interference,
clearance, contact classification, and bounded sampled Revolute motion sweeps. Returned records keep
the authoritative numeric values and stable rooted occurrence/component identities used by the
headless analyzers, allowing browser/viewport selection without OCCT identity leakage. Analysis
never mutates authored placements or joint coordinates.

The export boundary supports visible Part multi-body STEP, flattened posed Assembly STEP, and
structured Assembly product-graph STEP. Preflight requires a non-empty output, the matching active
document kind, valid Assembly structure where applicable, and fresh recompute results. Existing
BodyId/product naming contracts are reused unchanged.

Progress reports bounded writer stages. Cancellation is accepted before the writer commit and
therefore leaves no partial target file. Completion returns mode, exported item count, byte count,
and a deterministic diagnostic. Writer or preflight failures remain normal GUI diagnostics and do
not mutate the source document.

## Verification

```bash
./build/dev-gui/blcad_gui_tests "[gui][analysis]"
./build/dev-gui/blcad_gui_tests "[gui][step-export]"
```

## Replacement shell — Block 136 (2026-10-22)

`ShellWindow` explicitly owns `ShellInspectTools`. The 3D-Modell ribbon provides Part STEP;
the Baugruppe ribbon provides flattened STEP, structured STEP and Baugruppe analysieren.
Analysis and export require fresh geometry, the matching document kind, no active Sketch and no
competing task. Empty Part export, invalid paths and writer failures report diagnostics.

The analysis dock exposes interference, numeric minimum-clearance threshold, contact classification,
and local remaining-DOF inspection of an explicitly selected connected root component group.
Collision volumes and non-interfering clearance distances remain separate results. Contact lists
distinguish separated, touching and interfering pairs. Component names include the named occurrence
path; tooltips and item data retain the derived rooted identity, including repeated nested instances.
DOF inspection reports consistency and whether remaining freedom was actually determined; it does
not apply a solver proposal. Input changes clear old results. Model revision/freshness changes,
Undo/Redo, document replacement, Escape and dock close dispose the analysis task and its results.

Export uses the existing visible-Body / visible-active posed-occurrence inclusion and naming contracts.
Kernel writers run synchronously, without a promise of cancellation during writing. Cancelling the
file dialog starts no writer. The shell stages output in a temporary sibling directory, copies it to
`QSaveFile` and commits only a complete successful export. Failures preserve an existing target;
temporary output is removed. The current BLCAD source file cannot be replaced with STEP. Completion
reports actual exported item and byte counts, including nested assembly leaves. Neither consumer
changes document bytes, dirty state, history, placements or joint coordinates.

This bounded restoration does **not** complete Block 136: sampled Revolute sweep controls,
occurrence-linked result highlighting and the remaining shell acceptance audit are still open.
The headless sweep API above remains available and is not a shell UI proof.

Shell proof: `[block136-inspect-export]`, including Part creation through Sketch/Extrude and export,
flat/structured nested occurrence output, read-only results, positive clearance/contact boundaries,
DOF selection, stale/busy rejection, failure preservation and source-file protection. The native-only
case uses Qt file dialogs, verifies action/task reachability at 1280×800, dialog accept/cancel, Escape
and the resulting STEP file. `BLCAD_UI_CAPTURE_DIR` enables captures.

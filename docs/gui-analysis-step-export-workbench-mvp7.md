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

Block 137 also protects registered STEP source paths from export, including sources of embedded
Parts. Preflight and shell publication reject lexical/canonical aliases and existing hardlinks.
The saved document supplies the asset root. Source registration does not change export inclusion
or geometry semantics; details live in `docs/step-import-sequence-mvp10.md`.

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
not apply a solver proposal. Input changes clear old results. Model intent/freshness changes,
Undo/Redo, document replacement, Escape and dock close dispose the analysis task and its results.

Export uses the existing visible-Body / visible-active posed-occurrence inclusion and naming contracts.
Kernel writers run synchronously, without a promise of cancellation during writing. Cancelling the
file dialog starts no writer. The shell stages output in a temporary sibling directory, copies it to
`QSaveFile` and commits only a complete successful export. Failures preserve an existing target;
temporary output is removed. The current BLCAD source path cannot be replaced with STEP, even if that
file was removed externally. A removed source file does not block export of the loaded geometry to
another path. Completion
reports actual exported item and byte counts, including nested assembly leaves. Neither consumer
changes document bytes, dirty state, history, placements or joint coordinates.

### Sampled Revolute analysis and occurrence links — 2026-10-25

The fifth analysis mode, **Drehgelenk-Abtastung**, exposes the existing bounded sampler through
an explicit active Revolute joint chooser. Root-local and Project cross-hierarchy joints retain
their typed scope and exact JointId; inactive joints and other joint families are omitted. The
start/end degree editors initially use the selected joint's authored limits. The sample count
is bounded to 2–1001, inclusive; reverse intervals preserve the requested direction. Out-of-limit
intervals and solver/contact failures produce diagnostics and no partial result rows. Every sample
starts from the same source Project copy. The source placements, coordinates, dirty state, history
and canonical document bytes remain unchanged.

Each sampled contact row displays its sample number, exact angle and contact classification; the
summary reports the actual sample and evaluated-pair counts. Both the input hint and result summary
state that intermediate positions are not checked. This is discrete sampling, with no claim of
continuous collision detection or support for richer joint families.

The A/B cells of interference, clearance, contact and sampled-contact results are occurrence links.
Mouse clicks (including a repeated click on the current cell) and keyboard cell navigation select
the exact existing Component presentation through the normal shell selection callback. The link
is derived from the typed rooted subassembly path and local ComponentInstanceId; repeated instances
of one child definition remain distinct. The browser follows where it exposes that occurrence.
Sample-result highlighting always shows the selected occurrence in the current authored model pose,
as stated in the UI; analysis does not install sampled poses in the source viewport.

While the analysis task is active it owns a temporary Component filter and the visible-active leaf
scope. Closing or cancelling restores the previous filter/scope and any still-valid preselection.
Input changes and reruns clear the analysis-owned highlight and results. Source-intent/freshness changes,
Undo/Redo and document replacement discard them, without restoring a stale preselection.
Diagnostic-only presentation revisions preserve the active task and results when canonical Project
intent is unchanged, so an invalid interval can be corrected and rerun without reopening analysis.

Focused shell proof: `[block136-assembly-analysis]` covers root-local bounds and reverse sampling,
inactive-joint exclusion, cross-hierarchy sampling, exact repeated-occurrence links, repeated-cell
clicks, input invalidation, read-only persistence/history and Escape/dock/stale/Undo/New cleanup.
`[native-assembly-analysis-highlight]` also compares completed OCCT framebuffer images for an
isolated repeated occurrence: selecting a result changes model pixels, clearing restores them,
clicking the same cell selects it again, and changed input removes the highlight. Captures preserve
the before/selected/cleared states at 1280×800.
The integrated acceptance audit remains a separate requirement for closing Block 136.

Shell proof: `[block136-inspect-export]`, including Part creation through Sketch/Extrude and export,
flat/structured nested occurrence output, read-only results, positive clearance/contact boundaries,
DOF selection, stale/busy rejection, failure preservation and source-file protection. The native-only
case uses Qt file dialogs, verifies action/task reachability at 1280×800, dialog accept/cancel, Escape
and the resulting STEP file. `BLCAD_UI_CAPTURE_DIR` enables captures.

### Imported Reference geometry — Block 140 integration

Reference Parts participate in the same visible/active posed-leaf inclusion, exact transform chains,
contact, interference, clearance, sampled Revolute analysis and flat/structured exports. A saved
document supplies the explicit asset root. Queries and final publication revalidate actual imported
source bytes, and a stale/missing asset fails without applying poses, changing history or replacing
existing output. Every registered source path, including embedded Part registries and canonical or
existing hardlink aliases, is protected as an export destination. The document-aware headless STEP
export APIs use the same protection and stage complete output before publication. The CLI derives
its root from the input document and preserves historical single-result Part export support.

The detailed immutable definition, refresh and conversion boundaries are maintained in
`docs/step-import-sequence-mvp10.md`; accepted Block-140 validation is recorded in
`docs/development-setup.md`.

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

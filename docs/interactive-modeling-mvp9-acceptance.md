# Interactive Modeling MVP-9 Acceptance

Status: accepted through Block 131.

Block 131 closes Interactive Part & Assembly Modeling MVP-9 without adding persistent modeling
intent. The machine-readable disposition inventory is
`docs/gui-feature-coverage-manifest-mvp9.json` (`blcad.gui-feature-coverage.v2`). Its CI proof
compares the family set with the accepted MVP-7 manifest, rejects missing or duplicate families,
requires an owner in Blocks 106–131 for every interactive disposition, and requires a one-line
reason for every `validation_sufficient` disposition.

## Integrated proof

`[integration][interactive-modeling]` covers two end-to-end documents:

- a Part workflow with mouse-picked Sketch geometry, dragged Extrude and Fillet values, Shell,
  pattern candidate/ghost preview, Body Boolean, downstream-safe feature edit, recompute,
  save/load canonical equivalence, and multi-body STEP export;
- an Assembly workflow with placed components, capability-filtered Mate picking, Revolute and
  Prismatic coordinate motion, converged in-viewport candidates, DOF/interference analysis, and
  save/load canonical equivalence.

`[gui][measure]` proves point/edge/face distance, angle, radius/diameter, and Body volume/solid
counts, including stale/incomplete/incompatible fail-closed cases and exact non-mutation.

GUI/headless equivalence is checked by canonical Part/Project JSON and recomputed derived results.
Specialized Blocks 122–130 tests retain the per-command dragged-versus-typed equivalence and invalid
pick/preview/solve/edit rollback proofs.

`[performance][manipulator-hit]` and `[performance][preview]` measure 200 manipulator hit tests at
6 and 300 handles and 100 Assembly placement previews at 2 and 82 components. Each representative
case has a 5000 ms CI ceiling; the model size and measured duration are emitted as test diagnostics.

## Result

Blocks 122–131 are accepted. Block 132 is the next implementation boundary.

# Interactive Sketcher MVP-8 Acceptance

Status: accepted through Block 121. Interactive Sketcher MVP-8 is complete.

The machine-readable coverage authority is
`docs/interactive-sketcher-coverage-manifest-mvp8.json`. It maps every Block-106–121 capability to
its canonical contract, implementation authority, user interaction, focused proof, and honest GUI
disposition. Pattern/offset limits, nested curved regions and the remaining Sketch3D GUI work stay
explicit. The October 2026 follow-up adds interactive line/arc/spline region recognition, exact
profile/feature materialization and native UI proof; it does not claim a planar arrangement solver.

The designated deterministic tutorial documents are:

- `examples/triangle_prism.blcad.json`
- `examples/arc_profile_prism.blcad.json`
- `examples/spline_profile_prism.blcad.json`
- `examples/projected_sketch_references.blcad.json`

Integrated acceptance proves canonical GUI/headless loading and recompute equivalence for the
tutorial documents; direct pointer/model-target equivalence; exact drag Undo/Redo; stale-preview and
keyboard Cancel atomicity; high-DPI device-independent mapping; and measured hit-test, solver, drag,
and region-recognition paths. The focused tests owned by Blocks 106–120 remain the detailed proof
for every individual family.

```bash
./build/dev-gui/blcad_gui_tests "[integration][interactive-sketcher]"
./build/dev-gui/blcad_gui_tests "[integration][sketch-gui-headless]"
./build/dev-gui/blcad_gui_tests "[performance][sketch-interaction]"
```

Manual visual and pointer checks are frozen in
`docs/interactive-sketcher-manual-smoke-mvp8.md`.

Block 122 begins Interactive Part & Assembly Modeling MVP-9. STEP Import remains sequenced after
that phase in Blocks 137–143.

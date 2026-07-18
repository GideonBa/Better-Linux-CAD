# GUI Feature Validation MVP-7 Acceptance

Status: accepted through Block 105. MVP-7 is complete.

The machine-readable coverage authority is
`docs/gui-feature-coverage-manifest-mvp7.json`. It assigns every implemented public feature family
through Block 94 to a GUI create/edit or consumer command, browser/property inspection,
viewport/semantic-selection disposition, persistence/recompute proof, and focused test. CI rejects
missing fields, duplicate or absent required families, and `read_only` claims for mutable Core
contracts.

The designated checked-in deterministic samples are:

- `examples/bolt_circle_plate.blcad.json`
- `examples/spline_profile_prism.blcad.json`
- `examples/nested_subassembly.blcad.project.json`
- `examples/revolute_joint.blcad.project.json`

Integrated acceptance proves canonical GUI/headless JSON equivalence, equivalent Part recompute
products, Part parameter edit plus Undo/Redo freshness, nested Project load/recompute, and fail-closed
selection, feature-preview, and export-preflight boundaries. Blocks 99–104 additionally prove the
complete Sketch, Part, Surface, Assembly, analysis, motion, save/load, and STEP workflows owned by
their focused tags. Manual camera, pointer, and bidirectional picking checks are frozen in
`docs/gui-feature-validation-manual-smoke-mvp7.md`.

```bash
./build/dev-gui/blcad_gui_tests "[integration][gui-feature-coverage]"
./build/dev-gui/blcad_gui_tests "[integration][gui-headless-equivalence]"
```

Block 106 starts Interactive Sketcher MVP-8; it extends direct manipulation without reopening or
weakening this validation boundary. Interactive Modeling MVP-9 follows in Block 122 and STEP
Import MVP-10 in Block 132.

Block 131 preserves this manifest as the baseline and extends it with
`docs/gui-feature-coverage-manifest-mvp9.json`; CI requires an interactive owner or explicit
`validation_sufficient` reason for every family above.

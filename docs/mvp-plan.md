---
doc: MVP Plan
role: Implementation-sequence source of truth; feature contracts remain authoritative for exact semantics.
implemented_through: Block 121
current_block: 122
current_boundary: Interactive Part and Assembly Modeling MVP-9
current_tag: "[integration][interactive-modeling]"
phase_status:
  mvp_1: "Single-part modeling — implemented"
  mvp_2: "Semantic references and richer sketch workflows — implemented"
  mvp_3: "Parametric bolt-circle pattern — implemented"
  mvp_4: "Assembly parameters and Project container — implemented"
  mvp_5: "Assembly relationships, motion, hierarchy, analysis, exchange — Blocks 1–47 implemented"
  mvp_6: "Part Construction — Blocks 48–94 implemented; MVP complete"
  mvp_7: "GUI Feature Validation — Blocks 95–105 implemented; MVP complete"
  mvp_8: "Interactive Sketcher — Blocks 106–121 implemented; MVP complete"
  mvp_9: "Interactive Part & Assembly Modeling — Blocks 122–131 planned"
  mvp_10: "STEP Import — Blocks 132–138 planned"
---

# MVP Plan

This document is the numbered implementation-sequence source of truth for BLCAD. Feature-specific contracts remain authoritative for mathematics, persistence, migration, ordering, and failure policy.

## Current status

```text
implemented through  Block 121
current block        Block 122
current phase        Interactive Part & Assembly Modeling MVP-9
current boundary     first interactive modeling block
```

Interactive Sketcher MVP-8 is accepted. Block 122 is the next technical step.

## Phase map

```text
MVP-1   single-Part parametric foundation                         implemented
MVP-2   semantic references and richer Sketch workflows           implemented
MVP-3   parametric bolt-circle pattern                            implemented
MVP-4   assembly parameters and Project container                 implemented
MVP-5   Assembly system                                 Blocks 1–47 implemented
MVP-6   Part Construction                              Blocks 48–94 implemented
MVP-7   GUI Feature Validation                        Blocks 95–105 implemented
MVP-8   Interactive Sketcher                          Blocks 106–121 implemented
MVP-9   Interactive Part & Assembly Modeling          Blocks 122–131 planned
MVP-10  STEP Import                                    Blocks 132–138 planned
```

Canonical phase sequences:

- `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`
- `docs/part-construction-sequence-mvp6.md`
- `docs/gui-feature-validation-sequence-mvp7.md`
- `docs/interactive-sketcher-sequence-mvp8.md`
- `docs/interactive-modeling-sequence-mvp9.md`
- `docs/step-import-sequence-mvp10.md`

## Implemented foundation through Block 47 — Assembly MVP-5

Blocks 1–47 establish the single-Part foundation, semantic workplane/reference workflows, Project container, local and cross-hierarchy Assembly authority, relationship families, shared numeric solving, rooted hierarchy, five joint families, posed geometry, analysis, and flattened/structured STEP exchange.

## Implemented Blocks 48–94 — Part Construction MVP-6

Part Construction implements stable Body identity, Body-scoped recompute and inspection, body operations, typed feature inputs, Extrude/Cut, Revolve, Patterns, Mirror, Fillet, Chamfer, Shell, Draft, Sketch3D/PathCurve, Sweep, Loft, Surface features, multi-body workflows, and integrated Core/Geometry acceptance.

## Implemented Blocks 95–105 — GUI Feature Validation MVP-7

The optional Qt layer implements the CAD shell, `GuiDocumentSession`, generic task state, document lifecycle, atomic candidate transactions, exact undo/redo, recompute diagnostics, OCCT viewport, stable semantic picking, deterministic browser/property projection, synchronized selection, and validation workbenches. Qt remains a client of Core and Geometry authority.

## Implemented Blocks 106–121 — Interactive Sketcher MVP-8

```text
106 contextual Sketch workspace, command lifecycle, HUD
107 device-independent plane mapping, hit testing, selection, grid, snapping
108 shared planar point/entity topology, mutation commands, migration, undo
109 deterministic planar constraint solver, DOF, conflicts, diagnostics
110 solver-backed handles, live preview, atomic drag commit
111 point/line/polyline/rectangle/polygon/construction creation
112 circle/arc/ellipse/slot creation
113 spline editing, continuity, parameter-backed Sketch text
114 manual/automatic geometric constraints and glyph interaction
115 driving/reference dimensions and expression binding
116 trim/extend/split/fillet/chamfer
117 offset, project/include, construction references, break-link
118 move/rotate/scale/copy/mirror and Sketch patterns
119 regions, profile selection, diagnostics, Finish Sketch
120 interactive Sketch3D locks, placement, handles, guides, projection
121 coverage manifest, deterministic tutorials, acceptance evidence, performance budgets
```

The canonical acceptance authority is `include/blcad/core/interactive_sketcher_acceptance.hpp`; the contract is `docs/interactive-sketcher-acceptance-mvp8.md`. All fifteen capability families carry mouse/script equivalence, persistence/recompute, exact undo/redo, conflict atomicity, reference repair, keyboard Apply/Cancel, high-DPI mapping, and stale-preview rejection evidence.

## Next phase — Interactive Part & Assembly Modeling MVP-9

Blocks 122–131 are defined by `docs/interactive-modeling-sequence-mvp9.md`. The sequence must reuse existing Core/Geometry authority and the accepted GUI transaction, selection, preview, history, and diagnostics boundaries rather than introducing widget-owned CAD state.

STEP Import MVP-10 follows in Blocks 132–138.

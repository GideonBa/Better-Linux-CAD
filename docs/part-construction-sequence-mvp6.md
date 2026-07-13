# Part Construction Sequence MVP-6

Status: planned post-Assembly-MVP sequence. Blocks 48-94 are not implemented yet.

Current repository work remains on Block 43 of the assembly target/relationship/joint sequence. Blocks 43–47 remain mandatory before this sequence starts. Block 48 becomes the next technical step only after Block 47 is implemented and the Assembly MVP handoff is complete.

This document is the active status, phase-order, authority-boundary, and handoff summary for the first broad BLCAD Part Construction MVP after the Assembly MVP.

The complete original Blocks 48-94 per-block planning detail is preserved byte-for-byte in:

- `docs/part-construction-sequence-mvp6-planning-detail.md`

That companion document's block goals, required work, failure policies, acceptance tags, proofs, and explicit deferrals are incorporated by reference here. Its historical top-level statement that repository work was on Block 35 is preserved planning context, not current status authority.

This sequence consolidates architecture from:

- `docs/multi-body-transform-and-path-features-roadmap.md`
- `docs/inventor-like-sketcher-and-feature-roadmap.md`
- `docs/advanced-surfacing-and-3d-sketch-mvp.md`

Those documents remain canonical planning sources for detailed use cases and long-term capability breadth. This active sequence is authoritative for current block order, authority boundaries, MVP cuts, and implementation handoffs.

## Goal

After Block 94, one `PartDocument` should support a first serious headless parametric construction workflow covering:

```text
multiple solid/surface bodies
explicit feature-to-body result authority
new-body / join / cut / intersect operation modes
body booleans
body transforms with explicit sketch/reference ownership behavior
richer extrude/cut extents
Revolve / RevolveCut
general Linear Pattern
general Circular Pattern
Mirror Feature
Fillet
Chamfer
Shell
Draft
3D sketches
semantic connected PathCurve values
Sweep / SweepCut / SweepSurface
path-following extrude/cut
Loft / LoftCut / LoftSurface
multi-section and guided lofts
first continuity-controlled loft semantics
Boundary / Fill surfaces
Trim / Extend surfaces
Stitch / Knit / Sew shell
closed-shell-to-solid conversion
multi-body STEP export
```

The goal is not to clone SolidWorks or Inventor feature-for-feature. The goal is to finish a coherent first Part Construction kernel whose persistent model intent, dependency graph, semantic references, geometry execution, recompute behavior, and exchange products use one architecture.

## Non-negotiable architecture rules

1. `PartDocument` remains persistent parametric model authority.
2. OCCT shapes, wires, faces, shells, solids, and topology maps remain derived Geometry/cache products.
3. Raw `TopoDS_*` identity, traversal index, memory address, XDE label, and STEP entity id are never persistent feature input identity.
4. Blocks 35-36 semantic generated face/edge/vertex identity and recovery are reused by later Part features.
5. Existing semantic reference and reference-recovery infrastructure is extended rather than bypassed.
6. Every feature input/output relationship participates in dependency, invalidation, and recompute planning.
7. Multi-body authority is established before body booleans, transforms, general patterns, and richer solid features.
8. Core intent and JSON/structure validation precede Geometry execution for each new authority family.
9. `ShapeCache` is generalized once for multi-body results; richer features do not invent independent shape stores.
10. A feature that creates or modifies volume states its operation mode and target/produced body explicitly.
11. Pattern and mirror features preserve parametric source identity; they do not persist copied raw geometry.
12. Fillet/Chamfer/Shell/Draft consume semantic feature/body topology references, not arbitrary OCCT picks.
13. 3D sketch entities are Core model intent in model space and become OCCT curves only in Geometry.
14. `PathCurve` stores ordered semantic curve-segment references and an explicit orientation rule.
15. Sweep and path-following extrude may share internal Geometry helpers while remaining distinct user/model feature types.
16. Loft section order, alignment/seam intent, and continuity intent are persistent and deterministic.
17. Surface bodies are first-class `Body` values, not temporary unaddressable OCCT faces.
18. Surface stitching and shell-to-solid conversion are explicit features with strict validation.
19. Existing straight Extrude/Cut remain simple fast paths.
20. No block silently broadens into GUI editing, direct modeling, Class-A surfacing, sheet metal, or topology healing.

## Frozen phase order

```text
48-57   multi-body/result/transform authority
58-60   shared Part feature inputs and richer Extrude/Cut
61-62   Revolve / RevolveCut
63-65   general Linear/Circular Pattern
66-67   Mirror Feature
68-70   Fillet and Chamfer
71-74   Shell and Draft
75-78   3D Sketch
79      PathCurve
80-83   Sweep and path-following Extrude/Cut
84-87   Loft
88-92   Surface Features and surface-to-solid
93      multi-body STEP exchange
94      integrated Part Construction MVP acceptance
```

Do not merge these phases into one feature branch.

## Current handoff

Blocks 35–37 stable generated-topology identity, Geometry target resolution, and explicit relationship compatibility are implemented. Blocks 38–39 generic relationship intent/equations, Block 40 joint target compatibility/oriented Frame, Block 41 general joint coordinate/limit Core state, and Block 42 additive compatible coordinate JSON are also implemented. Block 43 vector joint drives are the current assembly next technical step.

Blocks 43–47 remain mandatory before this Part Construction sequence begins. After Block 47, Block 48 — Body identity and `PartDocument` ownership — becomes the next technical step. The complete Block 48-94 implementation contracts remain in `docs/part-construction-sequence-mvp6-planning-detail.md`.

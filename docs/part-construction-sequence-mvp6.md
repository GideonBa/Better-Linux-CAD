# Part Construction Sequence MVP-6

Status: active post-Assembly-MVP sequence. Blocks 48–53 are implemented; Blocks 54–94 remain planned. Blocks 95–101 form the subsequent STEP Import MVP.

Block 47 completed the Assembly MVP handoff. Blocks 48–53 establish Body identity, persistent feature Body-result operations, body-scoped recompute, and checked result inspection. Block 54 — BodyBooleanFeature Core intent and JSON — is the current next technical step.

This document is the active status, phase-order, authority-boundary, and handoff summary for the first broad BLCAD Part Construction MVP after the Assembly MVP.

The complete original Blocks 48-94 per-block planning detail is preserved byte-for-byte in:

- `docs/part-construction-sequence-mvp6-planning-detail.md`

The implemented Blocks 48–53 contracts are canonical in:

- `docs/part-body-identity-mvp6.md`
- `docs/part-body-json-mvp6.md`
- `docs/part-feature-body-operation-mvp6.md`
- `docs/part-feature-body-dependency-mvp6.md`
- `docs/part-multi-body-recompute-mvp6.md`
- `docs/part-multi-body-inspection-mvp6.md`

The planned post-Block-94 STEP import handoff is canonical in:

- `docs/step-import-sequence-mvp7.md`

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

Blocks 35–37 stable generated-topology identity, Geometry target resolution, and explicit relationship compatibility are implemented. Blocks 38–39 generic relationship intent/equations, Block 40 joint target compatibility/oriented Frame, Blocks 41–42 typed coordinate state/JSON, and Block 43 shared vector joint drives are also implemented. Blocks 44–47 Prismatic, Cylindrical, Planar, and passive Ball/Spherical are implemented.

The prerequisite Assembly sequence is complete. Blocks 48–50 provide Body identity, persistence,
and explicit NewBody/Join/Cut/Intersect intent. Block 51 persists that Feature context and connects
Body producer/consumer state to dependency, invalidation, recompute planning, cycle rejection, and
removal protection. Block 52 adds deterministic body-scoped cache products and transactional
recompute. Block 53 adds checked headless result inspection and final-shape compatibility. Block 54
— BodyBooleanFeature Core intent and JSON — is the next technical step. The remaining Block 54-94
implementation contracts stay in
`docs/part-construction-sequence-mvp6-planning-detail.md`.

## Post-Block-94 handoff

Block 94 completes the first Part Construction MVP. Blocks 95–101 then add STEP import with two
explicit modes: immutable Reference Parts for Assembly use and EditableBody Parts whose
`ImportedBodyFeature` is followed by normal BLCAD feature history. Stable imported topology,
source freshness, structured STEP assembly import, and integrated refresh/re-export behavior are
separate blocks rather than implicit additions to Block 93 or 94.

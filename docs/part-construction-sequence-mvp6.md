# Part Construction Sequence MVP-6

Blocks 48–94 are implemented and accepted. This document maps each completed block to its
current contract. [mvp-plan.md](mvp-plan.md) owns current numbered status and the next block;
[inventor-parity-sequence.md](inventor-parity-sequence.md) owns the later extensions.

## Implemented contract map

Read the affected contract, public header and tests; the original implementation chronology is
available in Git history. Core intent/JSON and Geometry execution have separate owners below.

| Block | Implemented boundary | Canonical contract |
| --- | --- | --- |
| 48 | Body identity and PartDocument ownership | [part-body-identity-mvp6](part-body-identity-mvp6.md) |
| 49 | Body JSON and structural validation | [part-body-json-mvp6](part-body-json-mvp6.md) |
| 50 | Feature output-body and operation-mode Core intent | [part-feature-body-operation-mvp6](part-feature-body-operation-mvp6.md) |
| 51 | Feature body-operation JSON, dependencies, and invalidation | [part-feature-body-dependency-mvp6](part-feature-body-dependency-mvp6.md) |
| 52 | Multi-body ShapeCache and recompute execution | [part-multi-body-recompute-mvp6](part-multi-body-recompute-mvp6.md) |
| 53 | Multi-body compatibility and body-result inspection | [part-multi-body-inspection-mvp6](part-multi-body-inspection-mvp6.md) |
| 54 | BodyBooleanFeature Core intent and JSON | [part-body-boolean-mvp6](part-body-boolean-mvp6.md) |
| 55 | Body boolean Geometry and recompute | [part-body-boolean-geometry-mvp6](part-body-boolean-geometry-mvp6.md) |
| 56 | BodyTransform and SketchOwnership Core intent + JSON | [part-body-transform-ownership-mvp6](part-body-transform-ownership-mvp6.md) |
| 57 | Body transform Geometry and associative ownership behavior | [part-body-transform-geometry-mvp6](part-body-transform-geometry-mvp6.md) |
| 58 | Part feature semantic input reference contract | [part-feature-input-reference-mvp6](part-feature-input-reference-mvp6.md) |
| 59 | Richer Extrude/Cut extent and thin-feature intent + JSON | [part-extrude-extent-intent-mvp6](part-extrude-extent-intent-mvp6.md) |
| 60 | Richer Extrude/Cut Geometry | [part-extrude-extent-geometry-mvp6](part-extrude-extent-geometry-mvp6.md) |
| 61 | Revolve/RevolveCut Core intent and JSON | [part-revolve-intent-mvp6](part-revolve-intent-mvp6.md) |
| 62 | Revolve/RevolveCut Geometry | [part-revolve-geometry-mvp6](part-revolve-geometry-mvp6.md) |
| 63 | General Part Pattern Core model and JSON | [part-pattern-core-mvp6](part-pattern-core-mvp6.md) |
| 64 | General Linear Pattern Geometry | [part-linear-pattern-geometry-mvp6](part-linear-pattern-geometry-mvp6.md) |
| 65 | General Circular Pattern Geometry | [part-circular-pattern-geometry-mvp6](part-circular-pattern-geometry-mvp6.md) |
| 66 | MirrorFeature Core intent and JSON | [part-mirror-intent-mvp6](part-mirror-intent-mvp6.md) |
| 67 | MirrorFeature Geometry | [part-mirror-geometry-mvp6](part-mirror-geometry-mvp6.md) |
| 68 | Edge treatment Core intent and JSON | [part-edge-treatment-intent-mvp6](part-edge-treatment-intent-mvp6.md) |
| 69 | Fillet Geometry | [part-fillet-geometry-mvp6](part-fillet-geometry-mvp6.md) |
| 70 | Chamfer Geometry | [part-chamfer-geometry-mvp6](part-chamfer-geometry-mvp6.md) |
| 71 | ShellFeature Core intent and JSON | [part-shell-intent-mvp6](part-shell-intent-mvp6.md) |
| 72 | ShellFeature Geometry | [part-shell-geometry-mvp6](part-shell-geometry-mvp6.md) |
| 73 | DraftFeature Core intent and JSON | [part-draft-intent-mvp6](part-draft-intent-mvp6.md) |
| 74 | DraftFeature Geometry | [part-draft-geometry-mvp6](part-draft-geometry-mvp6.md) |
| 75 | Basic 3D Sketch Core intent | [part-sketch-3d-core-mvp6](part-sketch-3d-core-mvp6.md) |
| 76 | 3D spline, arc, helix, and guide-curve intent | [part-sketch-3d-curves-core-mvp6](part-sketch-3d-curves-core-mvp6.md) |
| 77 | 3D Sketch JSON and semantic references | [part-sketch-3d-json-mvp6](part-sketch-3d-json-mvp6.md) |
| 78 | 3D Sketch Geometry conversion | [part-sketch-3d-geometry-mvp6](part-sketch-3d-geometry-mvp6.md) |
| 79 | Connected PathCurve Core intent, JSON, and validation | [part-path-curve-core-mvp6](part-path-curve-core-mvp6.md) |
| 80 | Sweep feature Core intent and JSON | [part-sweep-intent-mvp6](part-sweep-intent-mvp6.md) |
| 81 | Basic Sweep Geometry | [part-sweep-geometry-mvp6](part-sweep-geometry-mvp6.md) |
| 82 | 3D path, twist, and guide-controlled Sweep | [part-sweep-3d-geometry-mvp6](part-sweep-3d-geometry-mvp6.md) |
| 83 | Path-following Extrude and Extruded Cut | [part-path-extrude-geometry-mvp6](part-path-extrude-geometry-mvp6.md) |
| 84 | ProfileSectionReference and Loft Core intent + JSON | [part-loft-intent-mvp6](part-loft-intent-mvp6.md) |
| 85 | Two-section Loft Geometry on arbitrary planes | [part-loft-geometry-mvp6](part-loft-geometry-mvp6.md) |
| 86 | Multi-section Loft | [part-multi-section-loft-geometry-mvp6](part-multi-section-loft-geometry-mvp6.md) |
| 87 | Guided and continuity-controlled Loft | [part-guided-loft-geometry-mvp6](part-guided-loft-geometry-mvp6.md) |
| 88 | Surface feature Core intent and JSON | [part-surface-feature-intent-mvp6](part-surface-feature-intent-mvp6.md) |
| 89 | Boundary and Fill Surface Geometry | [part-boundary-fill-surface-geometry-mvp6](part-boundary-fill-surface-geometry-mvp6.md) |
| 90 | Trim and Extend Surface Geometry | [part-trim-extend-surface-geometry-mvp6](part-trim-extend-surface-geometry-mvp6.md) |
| 91 | Stitch/Knit/Sew shell Geometry | [part-surface-stitch-geometry-mvp6](part-surface-stitch-geometry-mvp6.md) |
| 92 | Closed shell to solid conversion | [part-closed-shell-to-solid-geometry-mvp6](part-closed-shell-to-solid-geometry-mvp6.md) |
| 93 | Multi-body STEP export and deterministic body naming | [part-multi-body-step-export-mvp6](part-multi-body-step-export-mvp6.md) |
| 94 | Integrated Part Construction MVP acceptance and headless workflows | [part-construction-mvp6-acceptance](part-construction-mvp6-acceptance.md) |

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

## Acceptance and later work

[Part Construction acceptance](part-construction-mvp6-acceptance.md) owns the combined mechanical,
routed/Sweep and Loft/Surface fixtures, persistence, invalidation, incremental recompute, semantic
recovery and multi-body STEP proof. Run its `[integration][part-construction-mvp]` filter in both
Core and Geometry using the current presets in [development-setup.md](development-setup.md).
Acceptance of this bounded MVP does not imply full feature breadth: consult each contract's
unsupported-input and failure policy before extending it.

The [retained planning detail](part-construction-sequence-mvp6-planning-detail.md) preserves the
original deferred scope and combined proof requirements. Broader use cases and future plans remain
in [multi-body/transform/path](multi-body-transform-and-path-features-roadmap.md),
[Sketcher/features](inventor-like-sketcher-and-feature-roadmap.md) and
[Surfacing/Sketch3D](advanced-surfacing-and-3d-sketch-mvp.md); their content is not compacted here.

Subsequent GUI/Sketcher/modeling, replacement-shell and STEP-import phases are also completed
milestones, not a new queue. Their contracts remain in
[GUI validation](gui-feature-validation-sequence-mvp7.md),
[Interactive Sketcher](interactive-sketcher-sequence-mvp8.md),
[Interactive Modeling](interactive-modeling-sequence-mvp9.md),
[Shell reset](gui-shell-reset-sequence-mvp9r.md) and
[STEP import](step-import-sequence-mvp10.md).

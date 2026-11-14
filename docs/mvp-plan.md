---
doc: MVP Plan
role: >-
  Implementation-sequence source of truth. Feature-specific documents remain
  canonical for exact contracts, formulas, persistence details, failure
  policies, ordering, and focused proofs.
implemented_through: Block 164
current_block: 165
current_boundary: Block 164 verified; normal serial delivery pending
current_tag: "[parity-b165]"
phase_status:
  mvp_1: "Single-part modeling — implemented"
  mvp_2: "Semantic references and richer sketch workflows — implemented"
  mvp_3: "Parametric bolt-circle pattern — implemented"
  mvp_4: "Assembly parameters and Project container — implemented"
  mvp_5: "Assembly relationships, motion, hierarchy, analysis, exchange — Blocks 1–47 implemented"
  mvp_6: "Part Construction — Blocks 48–94 implemented; MVP complete"
  mvp_7: "GUI Feature Validation — Blocks 95–105 implemented; MVP complete"
  mvp_8: "Interactive Sketcher — Blocks 106–121 implemented; MVP complete"
  mvp_9: "Interactive Part & Assembly Modeling — Blocks 122–131 implemented; MVP complete"
  mvp_9r: "GUI Shell Reset — Blocks 132–136 implemented; MVP complete"
  mvp_10: "STEP Import — Blocks 137–143 implemented; MVP complete"
  gui_workflow_revision: "Inventor-inspired desktop workflows — Block 144 implemented and accepted"
  advanced_modeling_workflows: "Blocks 145–164 accepted; Blocks 165–204 planned"
---

# MVP Plan

This document owns numbered implementation status. Read the relevant contract linked below for
behavior, persistence, failure policy and focused tests; use [development setup](development-setup.md)
for checks. Completed work is indexed here rather than repeated as dated implementation logs.
Historical delivery details remain in the linked PRs and Git history.

## Current status

```text
implemented through  Block 164
current block        Block 165 (planned)
current phase        Planar and spatial sketch extensions
current boundary     Block 164 verified; normal serial delivery pending
```

The assigned sequence continues through Block 174. Deliver verified 164 normally, then start 165
from clean updated main.
[The consolidated sequence](inventor-parity-sequence.md)
contains dependencies and implementation criteria; [the test plan](inventor-parity-test-plan.md)
contains individual and combined acceptance; [the UI contract](user-interface.md) owns the
Inventor-like workflows and layout. These plans retain their full unimplemented scope.

## Delivery discipline

The authorized delivery order is one active implementation branch and at most one open PR. Finish
the complete issue, inspect the staged diff, run appropriate headless/native checks, update existing
contracts and evidence, commit and normally push, then merge its PR through main's rules. Fetch and
verify the delivered tree before starting the next issue from main. Do not stack unfinished PRs.

### Assigned continuation through Block 174 — updated 2026-11-14

The current request assigns the complete sequence through 174. Execute each delivery serially;
this assignment does not change acceptance status. Keep the detailed requirements in
[the sequence](inventor-parity-sequence.md) and the scenario oracles in
[the test plan](inventor-parity-test-plan.md).

| Next delivery | Completion boundary / proof |
| --- | --- |
| 156 | Delivered in PR #175: shared placements, Hole consumers, full suites, native 100%/150% and startup verified. |
| 157 | Delivered in PR #181: semantic Hole/Thread creation/editing, explicit conversion, presets and native B01–B04/UI01. |
| 158 | Delivered in PR #182: full suites, fresh/migrated native history/STEP and isolated recompute measurements. |
| 159 | Delivered in PR #183: persistent oriented G2, normalized solver/rank, dimension/catalog/drag and native consumer regression; native Smooth UI remains 162. |
| 160–161 | Accepted typed equation intent/persistence and tolerance-controlled Geometry with real S02 profile/path/solid consumers; native authoring remains 162. |
| 162 | Delivered in PR #186: native Smooth/equation authoring, conflict correction, closed-profile Extrude/Revolve and exact persisted history. |
| 163 | Delivered in PR #187: spatial constraint ownership, strict V9 and actual legacy Geometry/session preservation. |
| 164 | Verified deterministic spatial solver, normalized native queries, physical rank/gauge and exact rejection with independent S03 oracles; serial delivery pending. |
| 165 | Solver-backed spatial curve/constraint interaction, explicit formula override and native S03–S04 downstream acceptance. |
| 166–167 | Explicit compatible curve-preserving path execution, semantic-edge inputs and multi-wire Sweep with G01 physical oracles. |
| 168–169 | Persistent Loft boundary/guide intent followed by independently verified G2 execution and G02 acceptance. |
| 170–172 | Offset/Thicken intent and checked Geometry, curved Trim/Extend and derived gap diagnostics with G03–G04 acceptance. |
| 173–174 | Native advanced curve/Surface workflows, integrated edits/history/reopen and physical STEP roundtrip acceptance. |

Each row retains its full numbered prerequisites and acceptance criteria; grouped rows do not
authorize partial acceptance of any block. Preserve all planned scope after 174.

Every new commit must contain at most **1,500 added lines**, checked from the staged numstat. Split
large deliveries into coherent commits and use an allowed merge method that preserves this limit.
The limit applies to new work; already delivered history remains intact.

## Completed blocks through 144

Every row below is implemented and accepted within its linked contract's bounds. A completed phase
does not imply completion of future extensions. Core/Geometry own model and geometry semantics;
Qt is a client. Persistent references are semantic, never raw OCCT topology, XDE labels, STEP entity
IDs or memory addresses. Blocks 132–136 replaced the old shell; use current `ShellWindow` evidence
when changing native interaction.

| Blocks / phase | Accepted scope | Canonical contracts and focused proof |
| --- | --- | --- |
| MVP-1–4; 1–47 / MVP-5 | Single-Part foundation, semantic references, bolt-circle pattern, Project container; Assembly hierarchy, relationships, joints, posed geometry, analysis and STEP exchange | [Assembly sequence](assembly-cross-hierarchy-solver-sequence-mvp5.md) |
| 48–94 / MVP-6 | Body identity/result intent, Boolean/Transform, Extrude/Revolve, Pattern/Mirror, finishing, Sketch3D/PathCurve, Sweep/Loft/Surface and multi-Body STEP | [Part construction sequence](part-construction-sequence-mvp6.md) |
| 95–105 / MVP-7 | Qt session, document lifecycle, candidate transactions/history, recompute/diagnostics, viewport/picking, browser/selection and validation workbenches | [GUI validation sequence](gui-feature-validation-sequence-mvp7.md) |
| 106 | Sketch workspace, interaction state and HUD | [Workspace](gui-interactive-sketch-workspace-mvp8.md) |
| 107 | Plane mapping, picking/box selection, grid, snapping and inference | [Plane interaction](gui-sketch-plane-interaction-mvp8.md) |
| 108 | Stable shared point/entity topology, mutation, migration and history | [Shared topology](sketch-shared-topology-mvp8.md) |
| 109 | Deterministic planar constraints, DOF, redundancy and conflict attribution | [Planar solver](sketch-planar-constraint-solver-mvp8.md) |
| 110 | Semantic handles, disposable live solve and atomic drag release | [Solver drag](gui-sketch-solver-drag-mvp8.md) |
| 111–112 | Points/lines/polylines, rectangle/polygon families, construction geometry, circles/arcs, exact ellipses and slots | [Basic creation](gui-sketch-basic-creation-mvp8.md), [conics/slots](gui-sketch-conic-slot-creation-mvp8.md) |
| 113 | Spline editing/continuity diagnostics and persisted Sketch text | [Spline/text](gui-sketch-spline-text-mvp8.md) |
| 114 | Manual/automatic geometric constraint intent, conflict preview and glyphs | [Constraint authoring](gui-sketch-constraint-authoring-mvp8.md) |
| 115 | Driving/reference dimensions, expressions, in-canvas editing and solver integration | [Dimension authoring](gui-sketch-dimension-authoring-mvp8.md) |
| 116 | Trim/Extend/Split and two-line Fillet/Chamfer; reference-preserving atomic edits | [Sketch modification](gui-sketch-modify-mvp8.md) |
| 117 | Line-chain/loop Offset, associative Project/Include and explicit break link | [Offset/projection](gui-sketch-offset-project-mvp8.md) |
| 118 | Move/Rotate/Scale/Copy/Mirror and associative/exploded Sketch patterns | [Transform/pattern](gui-sketch-transform-pattern-mvp8.md) |
| 119 | Region/profile recognition, typed diagnostics and fail-closed Finish Sketch | [Regions/Finish](gui-sketch-regions-finish-mvp8.md) |
| 120 | Direct Sketch3D creation/manipulation, axis/plane locks, XYZ/distance/direction input and explicit planar-point copying | [Sketch3D interaction](gui-sketch3d-interaction-mvp8.md) |
| 121 / MVP-8 closure | Coverage inventory, persistence, solver, GUI/headless equivalence, native interaction and measured acceptance | [Sketcher acceptance](interactive-sketcher-mvp8-acceptance.md), [phase sequence](interactive-sketcher-sequence-mvp8.md) |
| 122 | Shared selection-first Part/Surface/Assembly command/navigation authority | [Modeling workspace](gui-modeling-workspace-mvp9.md) |
| 123 | Transient viewport handles, numeric override, candidate-only drag and exact final sample | [Manipulators](gui-viewport-manipulators-mvp9.md) |
| 124 | Interactive Extrude, path Extrude and Revolve | [Extrude/Revolve](gui-interactive-extrude-revolve-mvp9.md) |
| 125 | Interactive Fillet/Chamfer/Shell/Draft over semantic inputs | [Finishing](gui-interactive-finishing-mvp9.md) |
| 126 | Interactive Pattern/Mirror and Body Boolean/Transform | [Pattern/Body operations](gui-interactive-pattern-body-mvp9.md) |
| 127 | Interactive PathCurve/Sweep/Loft and ordered section picking | [Path/Sweep/Loft](gui-interactive-path-sweep-loft-mvp9.md) |
| 128 | Interactive Boundary/Fill/Trim/Extend/Stitch and ClosedShellToSolid | [Surface](gui-interactive-surface-mvp9.md) |
| 129 | Stable typed feature update/remove and complete downstream preview/Apply | [Feature editing](gui-feature-edit-mvp9.md), [update inventory](gui-feature-update-inventory-mvp9.json) |
| 130 | Interactive Assembly placement, compatibility-filtered relationships/joints and bounded coordinate motion | [Assembly interaction](gui-interactive-assembly-mvp9.md) |
| 131 / MVP-9 closure | Read-only Measure, coverage inventory and integrated modeling acceptance | [Measure](gui-measure-mvp9.md), [acceptance](interactive-modeling-mvp9-acceptance.md), [phase sequence](interactive-modeling-sequence-mvp9.md) |
| 132 | QOpenGLWidget viewport with scheduled paintGL-only redraw | [Shell sequence](gui-shell-reset-sequence-mvp9r.md) |
| 133 | Explicitly owned ShellWindow/ribbon/browser/task/viewport and revision-gated refresh | [Shell sequence](gui-shell-reset-sequence-mvp9r.md) |
| 134 | Sketch creation, constraints, dimensions, modification and drag in the replacement shell | [Shell sequence](gui-shell-reset-sequence-mvp9r.md) |
| 135 | Replacement-shell Extrude/Revolve, profile handoff, numeric/manipulator preview and one-step publication | [Shell sequence](gui-shell-reset-sequence-mvp9r.md) |
| 136 / MVP-9R closure | Eighteen bounded families restored: modeling/editing, advanced extents, construction/workplanes, explicit reference repair, Sketch3D, parameters, contexts, navigation/filter/repeat, Assembly and analysis | [Shell acceptance](gui-shell-acceptance-mvp9r.md), [coverage inventory](gui-feature-coverage-manifest-mvp9.json) |
| 137 | STEP source registry, explicit modes, fingerprints, freshness, protected Save As/refresh and GUI management | [STEP sequence](step-import-sequence-mvp10.md) |
| 138 | Verified immutable STEP bytes, unit-normalized deterministic XDE reader and read-only product/body inspection | [STEP sequence](step-import-sequence-mvp10.md), [PR #130](https://github.com/GideonBa/Better-Linux-CAD/pull/130) |
| 139 | Persisted imported topology, explicit Resolved/Lost/Ambiguous recovery and Part/Assembly consumers | [STEP sequence](step-import-sequence-mvp10.md), [PR #131](https://github.com/GideonBa/Better-Linux-CAD/pull/131) |
| 140 | Immutable Reference Parts, shared/nested occurrences, constraints/motion/analysis/export, coordinated refresh and conversion to a new Editable Part | [STEP sequence](step-import-sequence-mvp10.md), [PR #132](https://github.com/GideonBa/Better-Linux-CAD/pull/132) |
| 141 | EditableBody immutable bases, ordered Body outputs, downstream modeling, imported face/edge authoring and strict complete refresh | [STEP sequence](step-import-sequence-mvp10.md), [PR #133](https://github.com/GideonBa/Better-Linux-CAD/pull/133) |
| 142 | Shared STEP definitions, exact direct occurrences/names and synthetic root in Reference/EditableBody Projects | [STEP sequence](step-import-sequence-mvp10.md), [PR #134](https://github.com/GideonBa/Better-Linux-CAD/pull/134) |
| 143 / MVP-10 closure | Continuous import/modeling/relationships/motion/analysis/save/refresh/re-export, atomic failures and portable JSON | [STEP sequence](step-import-sequence-mvp10.md), [PR #135](https://github.com/GideonBa/Better-Linux-CAD/pull/135) |
| 144 | Full-width Inventor-inspired ribbon, workspace command organization, Ctrl+K discovery and recoverable panels | [UI contract](user-interface.md), [PR #137](https://github.com/GideonBa/Better-Linux-CAD/pull/137) |

## Completed unnumbered maintenance

These changes advance no numbered block. The former issue queue is complete; its successive PRs
were merged serially. Use the linked current contracts, not superseded intermediate delivery status.

| Work | Accepted result / authoritative contract | Delivery |
| --- | --- | --- |
| Loft subdivision and correspondence | Derived unequal-edge subdivision and optional new-Loft contour alignment preserve original Sketch intent and explicit seam/order/guide choices; [Path/Sweep/Loft](gui-interactive-path-sweep-loft-mvp9.md) | 2026-10-26 follow-ups |
| Desktop/modeling usability | English labels, exact nested curved regions, result-edge finishing, validated Union, Sketch origin/axes, spline seam drag, element visibility and Rapid Dimension; contracts in the relevant rows above | 2026-10-26 follow-up |
| Desktop appearance | Licensed offline fonts/icons and coordinated shell/task/canvas presentation; [design](gui-hermes-design.md) and current [UI contract](user-interface.md) | 2026-10-26 follow-up |
| #105, #107, #108 | Exact Coincident equality groups; shared painted/picked dimension bounds, double-click editing and Delete/Backspace; [regions](gui-sketch-regions-finish-mvp8.md), [dimensions](gui-sketch-dimension-authoring-mvp8.md) | Closed before the serial queue |
| #113 | Ineffective/full-target Cut diagnostics, exact curved extents and atomic recovery; [Boolean geometry](part-body-boolean-geometry-mvp6.md), [Extrude/Revolve](gui-interactive-extrude-revolve-mvp9.md) | Completed follow-up |
| #114 | Affected-root previews/transactions, bounded validated history caches and native deep-rotor acceptance; [workspace](gui-modeling-workspace-mvp9.md), [benchmarks](benchmarks/) | [PR #123](https://github.com/GideonBa/Better-Linux-CAD/pull/123) |
| #104 | Stable endpoint snap identity and automatic Coincident geometry/history transaction; [creation](gui-sketch-basic-creation-mvp8.md) | [PR #124](https://github.com/GideonBa/Better-Linux-CAD/pull/124) |
| #109 | Reversible curve/group construction roles, lossless curved persistence and region exclusion; [creation](gui-sketch-basic-creation-mvp8.md), [regions](gui-sketch-regions-finish-mvp8.md) | [PR #125](https://github.com/GideonBa/Better-Linux-CAD/pull/125) |
| #111 | Derived datum/construction/semantic-edge Body Rotate axes through one cache-aware resolver; [Body operations](gui-interactive-pattern-body-mvp9.md) | [PR #126](https://github.com/GideonBa/Better-Linux-CAD/pull/126) |
| #110 | Persistent scoped Sketch-line axes for Revolve/Body Rotate and associative source edits; [Extrude/Revolve](gui-interactive-extrude-revolve-mvp9.md), [Body operations](gui-interactive-pattern-body-mvp9.md) | [PR #127](https://github.com/GideonBa/Better-Linux-CAD/pull/127) |
| #112 | Associative planar-result-face Sketch creation, strict lineage/recovery and explicit repair; [workspace](gui-modeling-workspace-mvp9.md) | [PR #128](https://github.com/GideonBa/Better-Linux-CAD/pull/128) |
| #106 | Shared dimension geometry/layout for paint, picking and editor placement; [dimensions](gui-sketch-dimension-authoring-mvp8.md) | [PR #129](https://github.com/GideonBa/Better-Linux-CAD/pull/129) |

#114's final 40-blade Apply/Undo/Redo p95 was **126.3 / 15.9 / 18.4 ms**. Its JSON p95
**22.949 ms** still exceeded the local **20 ms** target; the earlier interaction profile also
recorded 20-blade drag **17.8 ms** against **16 ms**. Cache misses and conservative fallbacks still
perform full geometry work. These measurements are local evidence, not a universal latency bound;
raw samples, benchmark limits and future performance scope remain in their existing contracts.

## Blocks 145–204 — Advanced modeling workflows

Status authority: Blocks 145–164 are **Implemented and accepted**;
Blocks 165–204 are **Planned; not started**.
The explicit 2026-11-09 resume led to whole-154 acceptance on 2026-11-10.
The 2026-11-06 implementation request starts the serial slices below. Detailed block dependencies,
contract owners and completion criteria live in `inventor-parity-sequence.md`. Scenario definitions,
negative cases, combination matrix, native proof and performance protocol live in
`inventor-parity-test-plan.md`. Shared UI/layout behavior is owned by the planned section of
`user-interface.md`. Existing catalog lists are mapped there rather than assigned competing sequences.

| Phase | Blocks | Assigned scope | Status |
| --- | --- | --- | --- |
| A | 145–151 | Shared workflow/evidence, feature history/suppression/reorder, material resources and physical properties | Implemented and accepted |
| B | 152–158 | Semantic holes, extents/seats, cosmetic/modeled threads, placement patterns, Wizard UI and everyday acceptance | Implemented and accepted |
| C | 159–165 | Planar G2/equation curves, spatial constraint model/solver and interactive 3D curves | 159–164 accepted; 165 planned |
| D | 166–174 | Curve-preserving paths, multi-wire/semantic-edge Sweep, verified G2 Loft, Offset/Thicken, curved Trim/Extend and Surface acceptance | Planned; not started |
| E | 175–184 | Variable/face/full-round Fillet and corners, Rib/Web, Emboss/Engrave and their property workflows | Planned; not started |
| F | 185–188 | Direct face edits, checked healing, immutable imported bases and integrated advanced Part acceptance | Planned; not started |
| G | 189–198 | Model states/variants, occurrence-local flexibility, adaptive cross-document design, coordinated Assembly holes and UI | Planned; not started |
| H | 199–204 | Motion/contact extensions, finite measurement, quality/thickness analysis, measured performance and final combined acceptance | Planned; not started |

Advance individual completed blocks or named slices only after their full criteria and relevant
native checks pass. A phase range remains incomplete while any constituent block is open. Serial
PR/merge discipline applies to implementation and planning deliveries; no successor branch starts
before the preceding PR is merged and main refreshed. Deferred drawings/BOMs, sheet metal,
CAM/FEM, automatic fastener libraries and general physical dynamics are not assigned here.

### Accepted Blocks 145–156

The historical request above has been implemented through 156; the completed slice logs are
replaced by this accepted block index and its PR references.

All rows are implemented and accepted. The [sequence](inventor-parity-sequence.md) retains detailed
block criteria and [test plan](inventor-parity-test-plan.md) retains scenario IDs/combination gates.
PR links preserve individual slice provenance without copying intermediate completion states.

| Block | Accepted result | Contract / acceptance gate | Delivery PRs |
| --- | --- | --- | --- |
| 145 | Audited API/command/proof inventory, shared property selectors/diagnostics, complete Part checkpoint, source-Sketch breadcrumbs, command-first collection and Create Another; one final publication and committed-only Save | [UI](user-interface.md); UI01/P01; [baseline](benchmarks/block145a-workflow-baseline.json) | [139](https://github.com/GideonBa/Better-Linux-CAD/pull/139), [140](https://github.com/GideonBa/Better-Linux-CAD/pull/140), [141](https://github.com/GideonBa/Better-Linux-CAD/pull/141), [142](https://github.com/GideonBa/Better-Linux-CAD/pull/142), [143](https://github.com/GideonBa/Better-Linux-CAD/pull/143) |
| 146 | Typed executable inventory/replay, persistent authored order and sparse active/suppressed intent, guarded candidates/removal and legacy byte compatibility | [Feature system](feature-system.md), [file format](file-format.md) | [144](https://github.com/GideonBa/Better-Linux-CAD/pull/144), [145](https://github.com/GideonBa/Better-Linux-CAD/pull/145) |
| 147 | Derived effective/prefix availability and actual full/prefix geometry, modifier predecessor restoration, atomic failed suffixes and complete-source cache ownership; explicit-history incremental calls use full fresh fallback | [Feature system](feature-system.md), [cache](shape-cache-mvp1-data-model.md); H01/H02 | [146](https://github.com/GideonBa/Better-Linux-CAD/pull/146), [147](https://github.com/GideonBa/Better-Linux-CAD/pull/147) |
| 148 | Browser suppress/restore/reorder, explicit dependent closure, transient End of Part edit/insert, full suffix validation, committed Save and complete Cancel/camera/selection restoration | [UI](user-interface.md), [feature system](feature-system.md); H01–H03/UI01 | [148](https://github.com/GideonBa/Better-Linux-CAD/pull/148), [149](https://github.com/GideonBa/Better-Linux-CAD/pull/149), [150](https://github.com/GideonBa/Better-Linux-CAD/pull/150), [151](https://github.com/GideonBa/Better-Linux-CAD/pull/151) |
| 149 | Provenance-bearing immutable Material/standards revisions, Part/Body pins, strict Part V3/Project V2 snapshots, deduplicated offline transport, explicit selected-member adoption and guarded removal | [Engineering resources](engineering-modules.md), [file format](file-format.md), [parameters](parameter-model.md); R01 backend | [152](https://github.com/GideonBa/Better-Linux-CAD/pull/152), [153](https://github.com/GideonBa/Better-Linux-CAD/pull/153), [154](https://github.com/GideonBa/Better-Linux-CAD/pull/154), [155](https://github.com/GideonBa/Better-Linux-CAD/pull/155) |
| 150 | Density-aware Part/Assembly mass, centroid/tensors and explicit participation; immutable freshness, supplier-byte guards and strict material-only BRep reuse | [Engineering properties](engineering-modules.md), [Measure](gui-measure-mvp9.md); R02 | [156](https://github.com/GideonBa/Better-Linux-CAD/pull/156), [157](https://github.com/GideonBa/Better-Linux-CAD/pull/157), [158](https://github.com/GideonBa/Better-Linux-CAD/pull/158) |
| 151 / Phase A closure | Manage Material assignment/revision tasks and read-only Inspect Physical Properties with qualified scopes, units, Refresh, clipboard and transient centroid; native combined workflows | [Engineering](engineering-modules.md), [Measure](gui-measure-mvp9.md), [UI](user-interface.md); R01–R02/UI01 | [159](https://github.com/GideonBa/Better-Linux-CAD/pull/159), [160](https://github.com/GideonBa/Better-Linux-CAD/pull/160), [161](https://github.com/GideonBa/Better-Linux-CAD/pull/161), [162](https://github.com/GideonBa/Better-Linux-CAD/pull/162) |
| 152 | Typed plain/clearance Hole intent, stable owned/Sketch placements, support/direction/extents/tips, pinned/custom dimensions, graph/history/repair, atomic resource remapping and strict portable Part V4 JSON | [Hole](hole-wizard.md), [file format](file-format.md); semantic Core/standards/persistence gate | [163](https://github.com/GideonBa/Better-Linux-CAD/pull/163), [164](https://github.com/GideonBa/Better-Linux-CAD/pull/164), [165](https://github.com/GideonBa/Better-Linux-CAD/pull/165) |
| 153 | Physical Hole tools, six bounded extent modes, finite native/imported/posed support, material checks and stable bore/bottom origins for finishing/Sketch consumers; atomic failure | [Hole](hole-wizard.md); B01 combined geometry/history/material/import/exchange gate | [166](https://github.com/GideonBa/Better-Linux-CAD/pull/166), [167](https://github.com/GideonBa/Better-Linux-CAD/pull/167), [168](https://github.com/GideonBa/Better-Linux-CAD/pull/168), [169](https://github.com/GideonBa/Better-Linux-CAD/pull/169) |
| 154 | Counterbore/countersink/spotface/stepped/fitted/reamed/center/slot intent, table/parameter/resource integration and Part V5 persistence; physical seats/steps/cones/slots and stable seat/step/curve roots | [Hole](hole-wizard.md), [file format](file-format.md); B02 combined acceptance | [170](https://github.com/GideonBa/Better-Linux-CAD/pull/170) |
| 155 | Shared internal/external Thread definitions, Part V6/Project snapshots and resource/history integration; cosmetic invariance/derived graphics and checked ideal-V helical cuts with explicit limits | [Hole/Thread](hole-wizard.md), [file format](file-format.md); B03 first gate, native authoring completion remains 157 | [172](https://github.com/GideonBa/Better-Linux-CAD/pull/172), [173](https://github.com/GideonBa/Better-Linux-CAD/pull/173), [174](https://github.com/GideonBa/Better-Linux-CAD/pull/174) |
| 156 | Shared two-direction placements, dormant exclusions and Part V7; physical Hole operation copies, per-instance axes/seats and Assembly consumers; current Pattern authoring with stable checkbox rows | [Pattern](part-pattern-core-mvp6.md), [Hole](hole-wizard.md), [Pattern UI](gui-interactive-pattern-body-mvp9.md); B04 / native 100%–150% | [175](https://github.com/GideonBa/Better-Linux-CAD/pull/175) |
| 157 | Shared semantic Hole/Thread creation/editing, presets/repeat, explicit legacy conversion, keyboard/source return and complete B01–B04/UI01 native acceptance | [Hole workflow](hole-wizard.md#whole-workflow-acceptance--157c) | [181](https://github.com/GideonBa/Better-Linux-CAD/pull/181) and its serial component PRs |
| 158 / Phase B closure | Fresh/migrated Phase A/B integration, physical history/Assembly/STEP, fractional native picking correction and isolated recompute evidence | [Integrated contract/proof](hole-wizard.md#phase-ab-integration--158-design), [raw measurements](benchmarks/block158-phase-ab-recompute.json) | [182](https://github.com/GideonBa/Better-Linux-CAD/pull/182) |
| 159 | Persistent oriented planar G2, rational/arc/line endpoint math, normalized rank/solve, dimension-aware authoring and exact accepted-catalog drag/history/reopen | [Planar solver/S01 proof](sketch-planar-constraint-solver-mvp8.md#block-159-smoothg2), [sidecar](file-format.md#planar-smoothg2-constraint-sidecar--159); [native task](gui-sketch-constraint-authoring-mvp8.md#block-162-native-smooth-task) | [183](https://github.com/GideonBa/Better-Linux-CAD/pull/183) |
| 160 | Bounded typed equation expressions, existing Sketch/Sketch3D ownership and dependencies, atomic domains, lossless topology/edit/session paths and strict Part V8/Project persistence | [Contract and S02/Core/session proof](parameter-expression-mvp.md#curve-local-expression-contract), [format](file-format.md#equation-curve-intent--160); construction and native authoring are covered in 161–162 | [184](https://github.com/GideonBa/Better-Linux-CAD/pull/184) |
| 161 | Bounded analytic/adaptive equation Geometry, proven closure/regions, existing profile/PathCurve and real Extrude/Revolve/Sweep consumers | [Construction/proof](part-sketch-3d-geometry-mvp6.md#equation-construction-contract), [profile](arc-and-trim-extend-sketch-profile-mvp.md#equation-profile-contract), [PathCurve](part-path-curve-core-mvp6.md#equation-source-contract) | [185](https://github.com/GideonBa/Better-Linux-CAD/pull/185) |
| 162 | Native Smooth/equation tasks, typed domains/bindings/tolerance, actual differential and candidate overlays, conflict/cancel/freshness, command search and closed-profile Extrude/Revolve | [Curve task/proof](gui-sketch-spline-text-mvp8.md#block-162-native-curve-task), [Smooth](gui-sketch-constraint-authoring-mvp8.md#block-162-native-smooth-task), [focused checks](development-setup.md) | [186](https://github.com/GideonBa/Better-Linux-CAD/pull/186) |
| 163 | Stable owned spatial constraint targets/dimensions, deterministic coordinate/scalar/auxiliary layout, literal Helix bindings, atomic source/graph protection and strict necessary-only V9/Project persistence | [Ownership and proof](part-sketch-3d-core-mvp6.md#spatial-constraint-ownership--163), [curve bindings](part-sketch-3d-curves-core-mvp6.md), [JSON](part-sketch-3d-json-mvp6.md); solving/DOF and authoring remain 164/165 | [187](https://github.com/GideonBa/Better-Linux-CAD/pull/187) |
| 164 | Bounded normalized spatial solve, native differential/projection queries, deterministic physical rank/redundancy, admissible rigid gauge/internal DOF and exact source rejection | [Solver contract/proofs](part-sketch-3d-core-mvp6.md#spatial-solver-boundary--164), [native capabilities](part-sketch-3d-geometry-mvp6.md#spatial-solver-queries--164); native authoring remains 165 | Verified; normal serial delivery pending |

The initial full parity plan was delivered in [PR #138](https://github.com/GideonBa/Better-Linux-CAD/pull/138).

### Block 156 delivery record — 2026-11-11

Shared linear/circular placement intent, stable logical exclusions and per-copy Hole consumers are
accepted within the [Hole execution limits](hole-wizard.md#shared-placement-execution-and-assembly-consumers-156).
The current Pattern task authors both directions and retained exclusions; repeated checkbox input
preserves live Qt rows through synchronous preview. B04 combines bound plate/cover counts, physical
seats, materials/mass, finishing, Assembly Insert, exact reopen and structured STEP readback.
Dedicated semantic Hole/Thread creation and editing remain assigned to 157.

| Verification | Result |
| --- | --- |
| Five build targets | Core/Geometry/Qt tests, app and STEP exporter built |
| Full Core | 706 cases / 26,802 assertions; passed |
| Full Geometry, including B04 | 671 / 57,576; passed |
| Focused Qt | 14 passed / 2 native-only skips / 677 assertions |
| Full Qt with required filesystem access | 778 cases: 738 passed / 40 native-only skips; 92,738 assertions passed |
| Relevant native 100% | 26 / 1,876; no failures/skips |
| Compact native 150% | 26 / 1,892; no failures/skips |
| Native startup | PID-matched X11 window viewable; intentional six-second timeout 124 |
| Frozen runtime | 1,050 files; SHA256 `7829ca89aca52f8d224489dc63e64a0396a7251966439c8feb839c0a4e7fb7e9` |

The manifest covers `src/`, `include/`, `tests/`, `assets/`, `cmake/` and both root CMake files.
All five binary hashes and runtime inputs remain unchanged. Changed C++ formatting, working/staged
diffs and original 100%/150% captures are reviewed. The relevant native filter covers affected
Pattern/Hole/Thread/Assembly/inventory workflows; it does not prove every offscreen skip.
The restricted full-Qt attempt was interrupted after atomic-publication failures; an isolated
`QSaveFile` probe confirmed the filesystem restriction, and the complete rerun above passes with
the needed access. No export assertion was changed.

Proof: [Core placements](../tests/core/pattern_placement_tests.cpp),
[Hole Pattern combinations/B04](../tests/geometry/hole_pattern_combined_tests.cpp),
[row lifetime/native authoring](../tests/gui/gui_shell_hole_pattern_tests.cpp) and
[Assembly task consumers](../tests/gui/gui_shell_assembly_tools_tests.cpp).
Delivery: [PR #175](https://github.com/GideonBa/Better-Linux-CAD/pull/175), correction `5b5a05f`;
merged normally as `5c669d0`. The merged tree matches the reviewed delivery, and main was
refreshed before creating `feat/hole-thread-property-editing` for 157A.

### Retained extension boundaries

These existing conditions are retained when compacting completed delivery logs. The complete future
sequence/test/UI plans remain authoritative; completion of the preceding backend does not grant a
new authoring capability.

**Shell deletion:** Before a future shell Delete action, separately audit terminal-modifier removal and
restoration of the preceding Body producer.

**Explicit reference repair:** Missing sources in other feature/construction kinds remain outside this repair contract.

**Thin Extrude:** General curved/open chains remain outside the retained Geometry boundary.

**STEP boundary completion:** Exact analytic UV boundaries retain existing 3D curves/ranges,
surfaces, vertices and tolerances. Broader healing remains a later workflow.

**Sweep/Loft:** Combined
guide-plus-twist and unsupported G2 continuity surface as fail-closed diagnostics.

**Engineering resource ownership:** This is
one common resource store for later Hole/Thread tables, not independent per-module catalogs.

**Material-only reuse:** Tables
remain geometry inputs for their assigned later consumers.

**Hole workflow:** Existing top-to-bottom property/preview/Apply/Undo contracts apply; dedicated authoring remains 157.

Phase B stays open for 158's integrated acceptance.

### Block 157 — accepted

All [157/B01–B04/UI01 requirements](inventor-parity-sequence.md#block-157-hole-and-thread-property-panel-workflow)
are verified within the supported capability contracts. Earlier verification records below retain
their slice's original delivery boundary; the final 157C entry owns whole-block closure.

| Slice | Delivery boundary |
| --- | --- |
| 157A — verified | Typed Hole/Thread property editing over existing Core update commands, stable IDs/history, formula and pinned-resource ownership, whole downstream preview and rollback. |
| 157B1 — verified | Semantic Plain/ThroughAll Hole creation in the existing panel, target/support selection, pending dimensional ownership, Create Another and one final publication. Normal complete Part context first; no new Hole schema or cut interpreter. |
| 157B2 — delivered | Complete recipe/custom-standard/internal-external Thread selectors and native/imported/result supports and limits; keep 157A editing compatible. |
| 157B2a — verified | Nine recipes, six extent transitions, tip and custom internal Thread controls, typed dimensional table/custom sources, stable owned inputs and existing-feature recipe changes. |
| 157B2b — delivered | Complete standard Thread profile mappings, optional start/taper authoring, external Thread creation and scoped native/imported/result support and limit collection. |
| 157B3 — verified | In-canvas position/depth/diameter interaction, referenced Sketch points and breadcrumb return, presets, explicit legacy conversion, full recipe/thread Create Another ownership and extended shared context lifecycle. |
| 157C — verified | Native B01–B04/UI01 combinations at 100%/150%, invalid/corrected candidates, exact history/reopen, physical structured STEP/reimport and synchronized command/API/proof inventory. |

No new Hole/Thread persistence authority or independent pattern evaluator is planned. Preserve
legacy cut editing and all subsequent 158–204 requirements. Acceptance of 157A alone does not
close 157 or provide the complete Wizard workflow.

### 157A verification — 2026-11-11

Existing semantic records now share the typed Feature editor and History Edit. Focused proof covers
all nine manufacturing variants, Pattern seats after diameter changes, formula rebinds, immutable
resource pins, retired placement references, drill/thread depth, full rollback and exact history/reopen.
Native keyboard/representation/mouse Apply produces a real modeled Thread cut. The
[Hole contract](hole-wizard.md#property-editing--157a-design-and-boundary),
[Feature edit contract](gui-feature-edit-mvp9.md#hole-and-thread-property-bridge--157a) and
[current tests](../tests/gui/gui_shell_hole_thread_edit_tests.cpp) own behavior/proof.

| Verification | Result |
| --- | --- |
| GUI/app/STEP-export targets | Built; final runtime inputs frozen |
| Current editor/Pattern/prefix/inventory Qt regression | 41 passed / 3 native-only skips / 3,507 assertions |
| Relevant native 100% | 44 cases / 3,705 assertions; no failures/skips |
| Native 150%, new property fixture compact | 44 / 3,705; no failures/skips; no horizontal form overflow |
| Separate startup | PID-matched X11 window viewable; intentional six-second timeout 124 |
| Retained backend acceptance | Both Core/Geometry test binaries and 663 backend/support inputs match the complete Block 156 evidence |
| Runtime manifest | 1,051 files; SHA256 `cb1a1dbd3837bc22da6ef1264cca2190bfcac014db88537c7e0503ac98bc06f5` |

All five binary hashes are verified (three current targets plus the unchanged two backend suites).
The full Core/Geometry and full Qt suites are not rerun for this UI-only slice; their latest complete
run is recorded under 156. Current native filters execute all three native-only cases in the focused
regression, not every skip from that full Qt run. Original ordinary/compact captures, changed C++
formatting, Markdown links/anchors/fences and working/staged diffs are reviewed.

Delivery: [PR #176](https://github.com/GideonBa/Better-Linux-CAD/pull/176), reviewed implementation
`456e5ec`, based on refreshed main `5c669d0` after verified merged PR #175. The final delivery record
changes documentation only. PR #176 is verified merged as `5dce938`; its tree matches the reviewed
delivery. Main was refreshed before starting `feat/semantic-hole-creation`. Parent 157 and the full
assigned continuation through 165 remain open.

### 157B1 verification — 2026-11-11

The shared property lifecycle now creates actual Plain / Through All Hole intent. Focused proof covers
disposable inputs and committed Save during preview, existing-parameter rebinding, invalid/corrected
diameters, context restoration, fresh Create Another identities, target/support invalidation, face
preselection, physical per-Body cuts and exact Undo/Redo/reopen. Native mouse picks and numeric keys
select the actual top support and create the expected cut. The datum-overlap correction retains
existing profile/History/imported-topology selection in the same native run.

Behavior and proof: [Hole contract](hole-wizard.md#semantic-creation--157b1-bounded-design),
[Feature editor](gui-feature-edit-mvp9.md#semantic-plain-hole-creation--157b1),
[current shell tests](../tests/gui/gui_shell_hole_thread_edit_tests.cpp) and original ordinary/compact
captures linked from the Hole contract. No Core/Geometry model or persistence schema changed.

| Verification | Result |
| --- | --- |
| GUI/app/STEP-export targets | Built from frozen final runtime inputs |
| Shared editor/creation/Pattern/prefix/inventory/viewport Qt regression | 54 passed / 5 native-only skips / 3,845 assertions |
| Native 100%, plus profile/History/imported STEP selection | 62 cases / 5,208 assertions; no failures/skips |
| Same native combination at 150%, creation/edit fixtures compact | 62 / 5,210; no failures/skips; form overflow and footer reachability checked |
| Separate startup | PID-matched X11 window viewable; intentional six-second timeout 124 |
| Retained backend acceptance | Both backend test binaries and 663 backend/support inputs match complete Block 156 evidence |
| Runtime manifest | 1,051 files; SHA256 `4786a01fd73ead9b87503ad218cb1c330bbc844efebb4a72bb4dca370a094cf9` |

All five binary hashes match the verified artifacts. Full Core/Geometry and full Qt are not rerun for
this GUI-only slice; the latest full runs remain under 156. All five native-only cases in the current
Qt regression execute in the native combination. Changed formatting, local documentation links /
anchors / fences, original captures and working/staged content are reviewed. The branch starts at
refreshed main `5dce938`, after verified merged PR #176. Delivery is through the normal PR rules.

Delivery: [PR #177](https://github.com/GideonBa/Better-Linux-CAD/pull/177), reviewed implementation
`8da8ef7`. The delivery record changes documentation only; runtime inputs and verified binaries
remain frozen. PR #177 is verified merged as `e3729a1`; its tree matches the reviewed delivery.
Main was refreshed before starting `feat/hole-recipe-authoring`.

Parent 157, 158 acceptance and the assigned 159–165 sequence remain open in full.

### 157B2a verification — 2026-11-12

The shared panel now composes typed recipe, extent, tip, dimensional source and custom internal
Thread transitions. Independent physical oracles cover all nine recipes and six extents; the limit
Body remains unchanged. Proof also covers pin/custom and Angle sources, shared bindings, new recipe
inputs on existing features, retired step IDs, unused inputs and exact history/reopen. Core role
translation, fresh custom ownership and retained keyboard focus are checked explicitly.

Contracts/proof: [Hole](hole-wizard.md#recipe-and-dimensional-source-controls--157b2a),
[Feature editor](gui-feature-edit-mvp9.md#recipe-transitions-and-dimension-sources--157b2a),
[shell regressions](../tests/gui/gui_shell_hole_thread_edit_tests.cpp) and
[native imported selection](../tests/gui/gui_shell_step_import_tests.cpp). The native STEP fixture
now fits the available screen and verifies actual on-screen targets before exact analytic picks.
Original recipe captures are linked from the Hole contract. No Core/Geometry model/schema changed.

| Verification | Result |
| --- | --- |
| GUI/app/STEP-export targets | Built from final frozen inputs |
| Shared recipe/creation/editor/Pattern/prefix/inventory/viewport Qt regression | 63 passed / 6 native-only skips / 4,745 assertions |
| Native 100%, including existing profile/History/imported STEP selection | 72 cases / 6,173 assertions; no failures/skips |
| Same native combination at compact 150% | 72 / 6,175; no failures/skips; all physical selection assertions retained |
| Separate startup | PID-matched X11 window viewable; intentional six-second timeout 124 |
| Retained backend acceptance | Both backend test binaries and 663 backend/support inputs match complete Block 156 evidence |
| Runtime manifest | 1,053 files; SHA256 `eb3145b2ad9e0013cd40759e88516d8ed9ff661e6efe30f737bdbf7c74546375` |

All five binary hashes match verified artifacts. Full Core/Geometry and full Qt are not rerun for
this GUI-only slice; the last complete runs remain under 156. All six native-only cases in the
focused Qt regression execute in the native combination. Changed formatting, local links / anchors /
fences, raw captures and working/staged content are reviewed. The branch starts from refreshed main
`e3729a1`, after verified merged PR #177, and uses normal PR delivery.

Delivery: [PR #178](https://github.com/GideonBa/Better-Linux-CAD/pull/178), reviewed implementation
`7e107b7`. This final delivery record changes documentation only; verified runtime inputs/binaries
remain unchanged. PR #178 is verified merged as `1348d19`; its tree matches the reviewed delivery.
Main was refreshed before starting `feat/hole-thread-support-authoring`.

The following 157B2b verification closes the named selector/authoring slice. 157B3/C and the full
158–165 sequence remain assigned; neither 157B2a nor 157B2b closes Block 157.

### 157B2b implementation and verification — delivered

The serial delivery is [PR #179](https://github.com/GideonBa/Better-Linux-CAD/pull/179), based on
merged `1348d19`. Reviewed final implementation `f4e0d87` is merged normally as `d4bc4c2`; the
merged tree matches the verified delivery. Main was refreshed before starting
`feat/hole-thread-workflow-lifecycle`. Parent 157 and the assigned continuation through 165 remain open.

| Component | Contract / focused proof | Delivery |
| --- | --- | --- |
| Native/imported/result planar creation supports; current placement, source guards and used-only pending workplanes | [Support contract and captures](hole-wizard.md#planar-support-collection--157b2b); `[hole-support-shell]`, `[native-hole-support]` | `3c0b01c` |
| Read-only exterior Thread frame/radius/span/signed taper; shared execution resolver, placed imports and stale/wrong/inner/source rejection | [Geometry contract](hole-wizard.md#exterior-thread-support-inspection--157b2b); `[thread-support-query]`, `[thread-import]` | `58b0ca7`, terminal placement correction in `8973417` |
| External creation and new modeled cylindrical templates; transactional used inputs, scoped picks, invalid correction, exact history/reopen and physical STEP | [Creation contract and captures](hole-wizard.md#external-creation-and-new-modeled-templates--157b2b); `[thread-create-shell]`, `[native-thread-create]` | `8973417` |
| Explicit Thread revision/row/typed columns, custom conversion, optional start/taper and live scoped selectors | [Source/interval contract](hole-wizard.md#thread-sources-and-optional-intervals--157b2b); `[thread-source-shell]`, `[native-thread-source]` | Verified in PR #179 |
| Existing Hole/Thread support editing against exact predecessor history; preserved references, later-Body exclusion and used-only pending workplanes | [Support-edit contract and captures](hole-wizard.md#existing-support-editing--157b2b); `[hole-support-edit-shell]`, `[native-hole-support-edit]`, Geometry `[thread-support-prefix]` | Verified in PR #179 |
| Explicit native/imported/result To Face and distinct Up to Body limits; invalid/cleared/retarget correction and unchanged limit Bodies | [Limit contract and captures](hole-wizard.md#explicit-hole-limits--157b2b); `[hole-limit-shell]`, `[native-hole-limit]` | Verified in PR #179 |
| Native polygon-Revolve source-edge roots; positive conical seeds, offset/opposite-direction/source correction, numeric precision and complete mapping retry | [Conical contract](hole-wizard.md#conical-thread-combinations--157b2b), [Revolve origins](part-revolve-geometry-mvp6.md#selectable-polygon-profile-face-origins); `[thread-cone-shell]`, `[native-thread-cone]`, `[revolve-lineage]` | Verified in PR #179 |

Geometry/GUI/app/export targets build. Final Revolve/Thread support, query, import and execution
regression passes **25 cases / 1,744 assertions**. The combined creation/recipe/support/limit/edit/
source/Pattern/prefix/inventory/icon/viewport/lifecycle Qt selection passes **88 cases / 14 native
skips / 7,393 assertions**. All fourteen skipped cases execute natively at **100% and compact
150%: 14 cases / 1,179 assertions at each scale**, without failures/skips. The 100% proof comprises
two disjoint runs (2 / 265 and 12 / 914) on the same final test binary. This is complete coverage of
the selected regression's skips, not the full native suite. Native proof includes real face/Body
picks, corrected complete mappings and retained numeric focus, modeled/cosmetic geometry, exact
history/reopen, physical STEP and viewer teardown. Original captures are inspected without edits.
Separate PID-matched startup is viewable (intentional timeout 124).

Core inputs and binary remain unchanged from the accepted backend evidence. Full Core, full
Geometry and full Qt are not rerun for this named slice; complete integration remains 157C/158.
The final GUI-test-only additions leave Geometry/app/export input closures and binaries unchanged
from the 25-case proof. Frozen runtime inputs: 1,055 files, SHA256
`0446ae69aa332d6de03e43eb201495052eded6bda9270195bb64b34852ff74bd`.
Detailed earlier component runs remain in Git/PR history.

157B3 implements full recipe/thread Create Another ownership, support-anchored manipulators, referenced
Sketch points and breadcrumb return, presets without captured targets, explicit legacy conversion
with mapping or diagnostic, and extended shared contexts. Then complete 157C's B01–B04/UI01 gates,
Block 158 integrated acceptance and the full 159–165 sequence. Preserve all scope after 165.

### 157B3 workflow lifecycle — delivered

157B3 is **implemented and verified** in [PR #180](https://github.com/GideonBa/Better-Linux-CAD/pull/180).
PR #180 merged as `248d7c2`; refreshed Main and its runtime match the verified delivery.
Its serial branch started from verified merged `d4bc4c2`. This named slice does not accept parent
157: complete B01–B04/UI01 combinations and capability promotion remain 157C, followed by 158's
Phase A/B integration and the assigned 159–165 sequence. Preserve all scope after 165.

| Completed component | Contract / proof source |
| --- | --- |
| Complete Create Another ownership | [Repeat contract](hole-wizard.md#complete-create-another--157b3); `[hole-repeat-shell]`, `[native-hole-repeat]` |
| Referenced positions and Source Sketch return | [Position/source contract](hole-wizard.md#referenced-position-workflow--157b3); `[hole-position-shell]`, `[source-sketch-workflow]`, `[native-hole-position]` |
| Support-anchored position/diameter/depth/Thread controls | [Canvas contract](gui-feature-edit-mvp9.md#holethread-canvas-controls--157b3); `[hole-canvas-shell]`, native canvas cases |
| Property value presets without captured targets | [Preset contract](hole-wizard.md#property-presets--157b3); typed/storage, `[hole-preset-shell]`, `[native-hole-preset]` |
| Explicit legacy conversion with mapping/material certification | [Conversion contract](hole-wizard.md#explicit-legacy-conversion--157b3); Core replacement, `[hole-conversion-shell]`, native conversion/context cases |
| Hidden/consumed predecessor input selection | [Input-history contract](hole-wizard.md#hidden-and-consumed-input-bodies--157b3); Geometry visibility/scope, `[hole-input-history-shell]`, native input cases |
| Prefix child commands, source return and enclosing workflow guards | [Prefix contract](hole-wizard.md#end-of-part-and-shared-contexts--157b3); `[hole-thread-insertion]`, `[hole-prefix-shell]`, `[hole-context-shell]`, `[native-hole-prefix]` |
| Stable-ID recipe/support/limit rebasing before a same-family suffix | Same prefix contract; `[hole-canonical-rebase-shell]` covers Source return, Through All/To Face, Counterbore, exact unchanged suffix, independent volume, history/reopen and native source/keyboard input |

Final rebasing/component verification on frozen runtime inputs:

| Check | Final result |
| --- | --- |
| Full Core, unchanged Core closure from prefix delivery | 708 cases / 27,016 assertions |
| Focused Geometry, unchanged Body/Pattern/support closure | 37 cases / 2,987 assertions |
| Focused canonical-rebasing Qt | 1 case / 169 assertions, both Through All and To Face |
| Affected recipe/position/support/limit/Thread/preset/source/editor/history Qt selection | 79 cases: 69 passed, 10 native skips / 5,996 assertions |
| Native rebasing/prefix/support/limit/Thread/preset, 100% and compact 150% | 7 cases / 1,026 assertions at each scale, all with exit 0 |
| Native app startup | PID-matched viewable window; intentional timeout 124 |

The preceding prefix transaction/context component passed its 161-case strict shared Qt selection
and ten-case native selection at both scales; detailed counts and earlier component runs remain in
Git/PR history and the linked contracts. Core/Geometry/export inputs and binaries are unchanged
after GUI-only rebasing. Original native captures are linked from the prefix contract. All 155
current GUI/app dependency records are valid and nonempty. Working/staged diffs, documentation
links, binary hashes and the frozen runtime are checked before delivery: **1,062 files**, SHA256
`4a8bafe1f25b31e2d2b3d2b6732e87700478766726cd07c76ccbc6b3584b26a2`.
Failed, interrupted and superseded runs contribute no final proof. These are focused component
results; full Geometry/Qt and whole-parent integration remain assigned to 157C/158.

### Combined acceptance — 157C

The serial acceptance branch starts from verified merged `248d7c2`, refreshed clean Main, no open
delivery PR and an unchanged delivered runtime. The reviewed delivery is
[PR #181](https://github.com/GideonBa/Better-Linux-CAD/pull/181), merged normally as `ad85b28`.
The delivered tree matches reviewed `8e41108`. Main was refreshed and verified clean, with no open
delivery PR and an unchanged frozen runtime, before starting 158's serial branch.

The first focused correction implements Hole/Thread field-owned Ctrl+Enter validation and
Escape handling, with local Enter/Tab and inline compact feedback. The keyboard/geometry/history
case and existing Extrude property-control baseline pass offscreen and natively at both scales;
the affected Qt selection passes with its eight native-only skips. The
[keyboard contract and proof](hole-wizard.md#keyboard-input-and-correction--157c) record counts,
original captures and the frozen runtime. The remaining native reconciliation and inventory gates
are completed by the closure proof below.

The combined proofs pass natively at 100% and compact 150%: newly authored two-position
Counterbores continue through Pattern exclusions, bound Assembly targets/mass and structured STEP
reimport; paired internal/external Threads exercise pins, hands, partial intervals and representation
changes. The [combined workflow contract](hole-wizard.md#combined-authored-hole-and-thread-workflows--157c)
records explicit oracles, execution counts, original captures and the remaining acceptance boundary.

| Verified gate | Completion boundary |
| --- | --- |
| B01/B02 + UI01 | Combine native Hole authoring, ordered selection/picks, custom/pinned sizes, seat/extent edits, canvas/source return, preset/repeat, invalid correction and exact one-step history with the existing Extrude baseline. Cover Enter/Tab/Ctrl+Enter/Escape, command search and reachable fixed actions at both scales. |
| B03 + UI01 | Combine native internal/external Thread authoring, pinned revisions, hand/interval edits, cosmetic/modeled transitions and correction with physical material/helix oracles and portable reopen. Retain supported/imported capability limits. |
| B04 combination | Continue newly authored multiple-position Holes through the existing Pattern/skip/count and Assembly per-hole target authorities into physical structured STEP readback. Reuse existing material and old-feature authorities; 158 still owns the complete Phase A/B suppression/reorder/mass/Assembly and incremental-versus-fresh acceptance repeat. |
| Negative and compatibility coverage | Reconcile the existing legacy-conversion, supplier, stale/forbidden input, source/prefix and Cancel/Save proofs with the new combined cases; every claimed combination needs actual native execution, not only an inventory filter. |
| Capability inventory and closure | Promote Hole/Thread mappings only after their API/command/proof references resolve and B01–B04/UI01 pass natively at 100%/compact 150%. Keep all later capabilities planned. Then accept only whole 157, deliver its PR serially and begin 158 from refreshed Main. |

The [final Hole/Thread acceptance proof](hole-wizard.md#whole-workflow-acceptance--157c) reconciles
every gate with current shell execution, native-only skip coverage, original captures and independent
material/history/reopen oracles. Both GUI capabilities now map to real commands, existing public
APIs and executed headless/native proof filters; later capabilities retain their planned state.
The strict inventory audit passes after this documentation-only promotion; the linked contract
records its final result. Runtime inputs are unchanged.

Only whole 157 is accepted here. Block 158 still owns full Core/Geometry/Qt, fresh/migrated Phase
A/B integration with suppression/reorder/finishing/mass/Assembly/STEP, and isolated incremental-
versus-fresh measurements. Continue through the assigned 159–165 implementations afterwards;
all 166–204 planning remains intact.

### Block 158 integrated acceptance — 2026-11-13

`feat/everyday-modeling-acceptance` starts from verified merged `ad85b28`. Whole 158 is verified
within the [integrated contract](hole-wizard.md#phase-ab-integration--158-design): fresh and pre-Hole
V3 migration, finishing, suppression/reorder, material mass, exact history/reopen, bound Assembly
and physical STEP/reimport. The compact native edge miss is fixed without changing persistence.

| Gate | Final result |
| --- | --- |
| Full Core / Geometry | 708 / 27,016 and 682 / 58,224; passed, no skips; unchanged input/binary closure retained |
| Full Qt | 896 cases: 829 passed / 67 native-only skips; 101,149 assertions; exit 0 |
| Native integration + Hole/Thread, 100% and compact 150% | 44 / 6,495 at each scale; no skips |
| Native fractional finishing / relevant picking | 1 / 138 and 14 / 2,514 at each scale; no skips |
| Required skips, captures and app startup | All 26 retained required cases execute at each scale; 41/67 offscreen skips covered across native selections. Original captures reviewed; separate PID-matched visible startup, intentional timeout 124. |
| Isolated recompute | Three processes, 18 repeats/variant plus separate first pairs/seeds; independent material/cache checks pass. Plan/empty-cache repeated medians: fresh 506.1/500.5 ms, migrated 533.8/533.2 ms. Existing full-history fallback, no speedup claim. |
| Frozen runtime | 1,065 files; SHA256 `2390a7d153bf2c8927586e94ae5b9146aa45b45d7919f21f538b6386fad533c7`; [complete inventory/raw measurements](benchmarks/block158-phase-ab-recompute.json) |

Delivery: [PR #182](https://github.com/GideonBa/Better-Linux-CAD/pull/182) merged normally as
`b1f34f7`; its tree `a87c9dd` matches the reviewed delivery. Main was refreshed and verified clean
before starting `feat/planar-smooth-g2`. The complete 159–165 assignment and later planning remain.

### Block 159 backend acceptance — 2026-11-13

`feat/planar-smooth-g2` starts from verified merged `b1f34f7`. Whole 159 is verified within the
[planar solver contract](sketch-planar-constraint-solver-mvp8.md#block-159-smoothg2): persistent
ordered endpoint identity, oriented tangent/curvature, normalized solve and rank, fixed/driving
and G1/G2 conflict/redundancy, sidecar compatibility, exact catalog/dimension drag/history/reopen.
Historical non-Smooth numerics remain unchanged. Native Smooth UI/closed-contour solid proof
remains 162; no later block or whole Phase C is accepted here.

| Gate | Final result |
| --- | --- |
| Focused Core / Qt | 12 / 1,938 and 2 / 108; passed; final documentation/inventory audit 5 / 704 |
| Full Core / Geometry | 720 / 28,954 and 682 / 58,224; no skips |
| Full Qt | 898 cases: 831 passed / 67 native-only skips; 101,256 assertions; exit 0 |
| Relevant native 100% / 150% | 34 / 1,504 and 34 / 1,505; no skips; existing endpoint/constraint/dimension/drag/inventory consumers and G2 headless API |
| Captures and startup | Original overlap/closed-chain drag captures reviewed at both scales; separate PID-matched visible app startup, intentional timeout 124 |
| Final runtime / retained proof | 1,070 files; SHA256 `61b8d4be6dab4d40bc20241f4006e9429c0a3f9f292ba4bc3859cbf09585fdf9`. Final Core-only combined-conflict fixture rebuilt/rerun; unchanged Geometry/GUI/app/exporter inputs and hashes retain their complete results. |

Delivery: [PR #183](https://github.com/GideonBa/Better-Linux-CAD/pull/183) merged normally as
`9e5bda2`; its tree `c173a10` matches the reviewed delivery. Main was refreshed and verified clean
before `feat/typed-equation-curve-intent`. The continuation through 165 and all 166–204 planning remain.

### Block 160 acceptance and delivery

Accepted the complete typed-intent/Core S02 boundary: four curve modes, bounded safe expressions,
stable parameter references and derived jets; existing Sketch/Sketch3D ownership and dependencies;
atomic domain edits, zero-point topology and lossless edit/materialization/history; strict Part V8
and Project resource persistence with historical canonical output. The
[contract and frozen-source proof](parameter-expression-mvp.md#expressionintent-proof) records
focused/full Core, Geometry, Qt, native 100%/150%, separate startup and reviewed captures.

Delivery: [PR #184](https://github.com/GideonBa/Better-Linux-CAD/pull/184) normally merged as
`1bc0461` on 2026-11-13; tree `3c13e18` matches reviewed `bff7ccc`. Main was refreshed, clean and
identical to upstream before `feat/equation-curve-geometry`. The complete continuation through 165
remains assigned; all later planning is preserved.

### Block 161 equation Geometry acceptance

`feat/equation-curve-geometry` starts from verified merged `1bc0461`. Whole 161 is accepted within
[the construction contract and proof sources](part-sketch-3d-geometry-mvp6.md#equation-construction-contract):
shared bounded AST analysis, exact supported closure, analytic/native or error-controlled C1 curves,
finite resource/numeric limits, existing regions/Finish Sketch/profile/PathCurve authorities,
stable source roles and actual Extrude/Revolve/Sweep consumers. Raw equations remain authored intent.
The [expression](parameter-expression-mvp.md#geometry-analysis-boundary--161),
[profile](arc-and-trim-extend-sketch-profile-mvp.md#equation-profile-contract) and
[PathCurve](part-path-curve-core-mvp6.md#equation-source-contract) contracts retain exact limits.

| Gate | Final result |
| --- | --- |
| Five-target build | Core/Geometry/Qt tests, application and STEP exporter built successfully; final affected targets rebuilt after review corrections. |
| Focused Core / Geometry | 7 / 4,429 and 11 / 10,400; independent derivative/error, physical solid, closure/intersection, domain/tolerance/reversal and portable replay oracles pass. |
| Full Core / Geometry | 739 / 33,751 and 693 / 68,624; passed, no skips. |
| Full Qt / final inventory audit | 901 cases: 834 passed / 67 native-only skips; 101,364 assertions; exit 0. Final documentation/inventory audit: 5 / 705. |
| Relevant native 100% / 150% | 37 / 1,614 and 37 / 1,615; no skips; new equation-solid/source/history/reopen plus existing endpoint, constraints, dimensions and drag consumers. |
| Captures and startup | Six original equation/endpoint/closed-chain captures reviewed after all GUI exit; separate PID-matched visible app startup, intentional timeout 124. |
| Frozen runtime | 1,091 files; SHA256 `3f0c5ad80a6f0dfa5249b0be537b005e27228f70ff5d74d6d770e4191827bb3e`. Binary hashes, 158 GUI/app dependency records, formatting, documentation links and working/staged review checked before delivery. |

Delivery: [PR #185](https://github.com/GideonBa/Better-Linux-CAD/pull/185) merged normally as
`1041c0b` on 2026-11-13. Tree `2d908f1` matches reviewed `892847e`. Main was refreshed, clean and
identical to upstream before `feat/planar-equation-smooth-authoring`; the runtime and five executable
hashes are unchanged. Native Smooth/equation authoring remains 162; spatial schema/solver/interaction
remains 163–165. Equation twist and historical path refinement retain their explicit 166 boundary; no whole
Phase C or later scope is accepted here. Preserve all 166–204 planning.


### Block 162 native planar authoring

**Implemented and accepted — 2026-11-13.** Started from normally merged `1041c0b`, refreshed clean
Main and no open delivery PR. The existing [S01/S02/UI01 gates](inventor-parity-test-plan.md#s01-g2-planar-spline-constraints)
are complete for native planar authoring:

| Delivered scope | Contract and proof boundary |
| --- | --- |
| Native Smooth | Ordered curve/endpoint selectors reuse the existing G2 catalog and solver. Actual inward tangent/curvature diagnostics, conflict correction, Cancel, exact catalog history and existing constrained grips lead to a finished closed-profile Extrude. |
| Equation task | Draw, command search and direct double-click editing cover all three planar modes, typed parameter insertion, domain/tolerance/closure and derived endpoint diagnostics. Full-session previews update existing consumers without editable sampled poles. |
| Candidate presentation | Sketch-scoped native graphics and candidate 2D markers preserve suspended feature preview isolation, hidden bodies and original source intent. Locked grips disappear during preview; correction/Cancel restore the committed interaction scene. |
| Native workflow | Finish Sketch to existing Extrude/Revolve, driven radius/source-domain edits, invalid grammar/domain/tolerance correction, final native release, exact Undo/Redo/save/open, keyboard and fixed compact footer at both scales. Only the implemented planar G2/equation inventory entries are promoted. |

Canonical contracts: [curve task](gui-sketch-spline-text-mvp8.md#block-162-native-curve-task),
[Smooth](gui-sketch-constraint-authoring-mvp8.md#block-162-native-smooth-task),
[equation source](parameter-expression-mvp.md#curve-local-expression-contract) and
[shared UI](user-interface.md). Executable proof lives in
[task/native tests](../tests/gui/gui_shell_sketch_curve_authoring_tests.cpp),
[preview ownership](../tests/gui/gui_occt_viewport_tests.cpp) and
[bounded display sampling](../tests/geometry/equation_curve_geometry_tests.cpp).
Physical oracles use the actual release ray, `πr²d` and `2π²Rr²`, independently of the task's residuals.

| Acceptance gate | Result |
| --- | --- |
| Build | All five Core/Geometry/Qt/app/export targets exit 0; 160 GUI/app dependency records valid and nonempty. |
| Complete Core / Geometry | **739 / 33,751** and **694 / 68,922** cases/assertions, no skips. Inputs and binaries remain identical across the final GUI-only refinement. |
| Complete offscreen Qt | **909 cases: 839 passed, 70 native-only skipped; 101,612 assertions**, exit 0. Native-only execution is supplied below, not inferred from skips. |
| Focused task/inventory Qt | **13 cases: 10 passed, 3 native-only skipped; 941 assertions**, exit 0. |
| Relevant native consumers | Final 100%: **59 / 3,266**; compact 150%: **59 / 3,267** cases/assertions, **0 skips**. Combines 159–162, existing constraint/dimension/drag/endpoint, spline, source-return and inventory regressions; this is a focused native gate, not a full native suite. |
| Captures and startup | Twelve original equation/Smooth/grip/source-edit/Revolve captures reviewed only after all GUI exit. Separate PID-matched visible application startup succeeds; intentional timeout 124. |
| Frozen delivery inputs | **1,096 files**, SHA256 `bd926b1df2b488283b09ad04a82a7c04f64b9a796483506d38ad4f9495cd32b7`. Five binary hashes, formatting, local documentation links and working/staged review checked. |

Delivery: [PR #186](https://github.com/GideonBa/Better-Linux-CAD/pull/186) merged normally as
`06eeeb2` on 2026-11-13. Tree `a2787e8` matches reviewed `45dd68c`. Main was refreshed, clean and
identical to upstream before `feat/spatial-sketch-constraint-model`; runtime and all five executable
hashes are unchanged.
The complete spatial schema/solver/interaction assignment remains 163–165; this closes no whole
Phase C. Preserve all 166–204 planning, including equation twist and historical path refinement.

### Block 163 spatial constraint model — verified

Start from normally merged `06eeeb2`, clean updated Main and no open delivery PR. Extend the existing
Sketch3D authority. Constraint ownership and strict V9 persistence are verified; solving, rank/gauge
and native constraint/curve authoring remain the distinct 164/165 gates.

| Step | Completion boundary |
| --- | --- |
| Owned intent and bindings | Stable constraint IDs, explicit ordered point/curve/endpoint and read-only external targets; typed dimensional bindings and deterministic point/scalar/auxiliary variable ordering. Preserve historical three-point arcs, fit/control splines, positive Length radius/pitch and integer Count turns. |
| Validation and persistence | Publish a strict, necessary-only Part V9 extension for spatial constraints and owned scalars; retain historical canonical records and Project resource ownership. Guard local removal/rebind, units, dependency cycles, source loss and failed updates transactionally. |
| S03 schema proof | Complete Core target matrix, strict JSON and Project/session history, parameter-driven points, exact legacy interpretation and actual Geometry equivalence. Solver/rank/gauge and native curve/constraint interaction remain 164/165. |

Use [Core ownership](part-sketch-3d-core-mvp6.md), [curve intent](part-sketch-3d-curves-core-mvp6.md),
[JSON](part-sketch-3d-json-mvp6.md), [file format](file-format.md) and
[existing interaction](gui-sketch3d-interaction-mvp8.md).

| Verification | Accepted evidence |
| --- | --- |
| Focused S03 schema | Core **9 cases / 258 assertions**; Geometry **3 / 194**. The V9 combination includes equation curves, extended Pattern/Hole history and Project-owned resources. |
| Complete Core / Geometry | **748 / 34,009** and **697 / 69,116**, respectively; no failures/skips. |
| Complete offscreen Qt | **911 cases: 840 passed, 71 native-only skipped; 101,625 assertions**, exit 0. This does not claim full native-suite coverage. |
| Native 100% and compact 150% | **16 cases / 1,367 assertions at each scale**, no failures/skips: new literal Helix/schema preservation, existing Sketch3D, equation/Smooth and shell inventory. Both original captures reviewed after GUI exit. |
| Build / startup | All five targets exit 0; **161** GUI/app objects have nonempty valid dependencies. Separate application PID **35359** owns a visible window; intentional timeout **124**. |
| Frozen inputs | **1,103 files**, SHA256 `a8375928a34a405e6b427e9aa19927d4a378b0fa6da651bb2defed4f7e40150f`. Final Core-only test addition rebuilt/retested; Geometry, Qt, app and export binaries retain their checked hashes. Formatting, links and working/staged content reviewed. |

[Core proofs](../tests/core/sketch_3d_constraint_tests.cpp),
[native Geometry](../tests/geometry/sketch_3d_constraint_geometry_tests.cpp),
[actual Sweep](../tests/geometry/sweep_feature_pipeline_tests.cpp) and
[session/native preservation](../tests/gui/gui_spatial_constraint_schema_tests.cpp) own assertions.
Delivery: [PR #187](https://github.com/GideonBa/Better-Linux-CAD/pull/187) merged normally by rebase
as `5c4c0c3` and `e6d94c7` (1,476 and 964 added lines). Tree `ef4fd13` matches reviewed `a71d77b`.
Main was refreshed and clean before `feat/spatial-sketch-solver`. Continue serially through 174;
retain all 175–204 planning.

### Block 164 spatial solver — verified

Extend the accepted Sketch3D authority with deterministic normalized residuals, bounded numerical
solving and derived rank/DOF/conflict/redundancy reports. Reuse the existing dense elimination/rank
helpers without altering planar behavior. Geometry supplies current-candidate native derivatives,
measures and trimmed-face projections; numerical perturbations never publish to Part/history.

| Step | Completion boundary |
| --- | --- |
| Core solver | Stable variable/constraint order, preserved parameter bindings, normalized coordinates/residuals, bounded damping/line search/evaluation work and exact source return on every rejected solve. |
| Diagnostics | Final numerical rank, auxiliary-aware attribution, admissible local rigid gauge and internal DOF; unsigned endpoint angles and signed projected angles have explicit conventions. |
| Geometry and S03 proof | Actual arc/spline/helix differentials, finite trimmed-face interiors and fresh external sources. Independent rigidity counts, rigid motion/scale/repeatability, conflicts, singular/unsupported queries and exact rollback; existing planar/Geometry/session consumers remain regression gates. |

Native solver-backed curve/constraint editing remains 165. The accepted numerical boundary retains
its explicit regular-curve and trimmed-face-interior capabilities; it claims local feasibility/rank,
not global uniqueness, surface-boundary G2 Loft or a historical path-policy upgrade.

| Verification | Accepted evidence |
| --- | --- |
| Independent S03 Core | **9 cases / 316 assertions**: elementary rigid counts, all relation residuals, proper arbitrary rotation and independently computed invariant scale, deterministic repeated solves, stationary curve branch, formula/Count immutability, owned scalars and bounded exact rejection. |
| Native Geometry | **4 cases / 158 assertions**: analytic arc/cubic/Helix jets and length, current-candidate coordinates, actual radius/pitch solving, trimmed interiors/stale sources, rank-seven G2 and fixed G1-only impostor/singular-owner rejection. |
| Complete Core / Geometry | **757 / 34,325** and **701 / 69,274**, respectively; no failures/skips. Extracted dense numeric function bodies match their prior implementation after whitespace normalization. |
| Qt regression | **23 cases: 19 passed, 4 native-only skipped / 1,444 assertions**. Native 100% and compact 150% each pass **16 / 1,355**, no skips. Full Qt is not claimed for this solver-only delivery; authoring remains 165. |
| Build / startup | All five targets exit 0; **161** GUI/app objects have valid nonempty dependencies. Visible application PID **40432**, intentional startup timeout **124**. |
| Frozen delivery inputs | **1,110 files**, SHA256 `2d387d08834335e706c3e62ef7af2c93cdbb813c4803c2ead24026b247d7741c`; binary hashes, formatting, links and working/staged review checked. |

[Core oracle assertions](../tests/core/sketch_3d_solver_tests.cpp) and
[native Geometry assertions](../tests/geometry/spatial_solver_geometry_tests.cpp) own the proofs.
Deliver normally with bounded commits; refresh clean main after merge before 165. Continue through
174 without accepting any later partial scope; preserve all 175–204 planning.

## Later phases

Interactive Part & Assembly Modeling MVP-9 is complete through Block 131. GUI Shell Reset MVP-9R
is complete through Block 136. STEP Import MVP-10 is complete through Block 143, including source
management, geometry inspection, semantic topology/recovery, immutable Reference Parts in
assemblies, EditableBody downstream modeling, structured STEP Projects and integrated end-to-end
acceptance in `docs/step-import-sequence-mvp10.md`. The next assigned program is the planned
advanced workflow sequence above; capabilities outside Blocks 145–204 remain unassigned.

## Documentation maintenance — 2026-11-10

Compact completed implementation/delivery logs into status and contract/proof indexes for efficient
LLM navigation. Retain the numbered 154/155 boundary, all unimplemented planning and explicit future
scope. Six working documents are about 89% smaller; all `docs/` Markdown is about 25% smaller.
README/AGENTS define focused reading and concise upkeep; original logs remain in Git at `46ab49c`.

Validation: 226 other Markdown files unchanged byte-for-byte, 33 protected mixed-document excerpts
retained verbatim, 1,073 local links/path references checked without new broken links/anchors,
balanced fences, all 47 Part block/contract mappings and working/staged whitespace review.
The existing `[gui][shell-coverage]` gate with `BLCAD_REQUIRE_SHELL_ACCEPTANCE=1` passes **5 cases /
659 assertions**, no skips. All 1,027 previously verified runtime inputs are unchanged; no fresh
CAD build, full regression or native run is needed or claimed for this documentation-only change.

Delivery: [PR #171](https://github.com/GideonBa/Better-Linux-CAD/pull/171), prepared on
`docs/compact-implemented-documentation` after verifying PR #170 merged and refreshing main.
Reviewed compaction commit: `7e71e9841177b99381eae07415c94251e2b9cb47`; this delivery-record
update changes no runtime input or numbered implementation status.

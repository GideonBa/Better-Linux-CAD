---
doc: MVP Plan
role: >-
  Implementation-sequence source of truth. Feature-specific documents remain
  canonical for exact contracts, formulas, persistence details, failure
  policies, ordering, and focused proofs.
implemented_through: Block 154
current_block: 155
current_boundary: Block 154 accepted; 155A delivered in PR 172; 155B verified in PR 173; 155C planned
current_tag: "[parity-b155]"
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
  advanced_modeling_workflows: "Blocks 145–154 accepted; Block 155 in progress; Blocks 156–204 planned"
---

# MVP Plan

This document owns numbered implementation status. Read the relevant contract linked below for
behavior, persistence, failure policy and focused tests; use [development setup](development-setup.md)
for checks. Completed work is indexed here rather than repeated as dated implementation logs.
Historical delivery details remain in the linked PRs and Git history.

## Current status

```text
implemented through  Block 154
current block        Block 155 (155B verified; 155C next)
current phase        Semantic holes and threads
current boundary     Whole Block 154 accepted; Thread document integration verified; execution/display open
```

The next assigned work is Block 155. [The consolidated sequence](inventor-parity-sequence.md)
contains dependencies and implementation criteria; [the test plan](inventor-parity-test-plan.md)
contains individual and combined acceptance; [the UI contract](user-interface.md) owns the
Inventor-like workflows and layout. These plans retain their full unimplemented scope.

## Delivery discipline

The authorized delivery order is one active implementation branch and at most one open PR. Finish
the complete issue, inspect the staged diff, run appropriate headless/native checks, update existing
contracts and evidence, commit and normally push, then merge its PR through main's rules. Fetch and
verify the delivered tree before starting the next issue from main. Do not stack unfinished PRs.

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

Status authority: Blocks 145–154 are **Implemented and accepted**;
Block 155 is **In progress**; Blocks 156–204 are **Planned; not started**.
The explicit 2026-11-09 resume led to whole-154 acceptance on 2026-11-10.
The 2026-11-06 implementation request starts the serial slices below. Detailed block dependencies,
contract owners and completion criteria live in `inventor-parity-sequence.md`. Scenario definitions,
negative cases, combination matrix, native proof and performance protocol live in
`inventor-parity-test-plan.md`. Shared UI/layout behavior is owned by the planned section of
`user-interface.md`. Existing catalog lists are mapped there rather than assigned competing sequences.

| Phase | Blocks | Assigned scope | Status |
| --- | --- | --- | --- |
| A | 145–151 | Shared workflow/evidence, feature history/suppression/reorder, material resources and physical properties | Implemented and accepted |
| B | 152–158 | Semantic holes, extents/seats, cosmetic/modeled threads, placement patterns, Wizard UI and everyday acceptance | 152–154 accepted; 155 in progress; 156–158 planned |
| C | 159–165 | Planar G2/equation curves, spatial constraint model/solver and interactive 3D curves | Planned; not started |
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

### Accepted Blocks 145–154

The historical request above has been delivered through 154; the completed slice logs are
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

The initial full parity plan was delivered in [PR #138](https://github.com/GideonBa/Better-Linux-CAD/pull/138).

### Latest implementation acceptance — Block 154, 2026-11-10

All 154A/B/C gates pass. Seat depths measure from the authored support; seats must fit/intersect
current target material, precede termination and exceed the primary bore diameter. Fits retain
explicit authored tolerances and nominal geometry. Slots consume an existing outward-capsule Sketch
profile without generating Sketch entities. Part V5 manufacturing intent retains ordinary V4 bytes.

Exact STEP roundtrip completes only analytic UV-boundary representation while retaining every
existing 3D curve/range, surface, vertex and tolerance; broader healing remains a later workflow.

| Final verification | Accepted result |
| --- | --- |
| Full Core | 680 cases / 26,299 assertions |
| Full Geometry | 646 / 55,985 |
| Focused Qt | 12 / 1,309 |
| Full Qt | 728 passed / 39 native-only or optional skips / 92,244 assertions |
| Relevant native 100% | 16 / 1,848; no failures/skips |
| Compact native 150% | 16 / 1,866; no failures/skips |
| Inventory | 5 / 658 |
| Native startup | Window reached; intentional timeout 124 |
| Frozen runtime | All 1,027 fingerprints match the tested state |

Earlier native STEP-menu watchdog failures and their corrected test-helper verification remain in
[PR #170](https://github.com/GideonBa/Better-Linux-CAD/pull/170) and the original setup record
(`git show 46ab49c:docs/development-setup.md`). The current [setup guide](development-setup.md) retains
the reusable watchdog rule: actual actions, dialogs and physical results with the original five-second
phase limits. Unrelated skipped native/optional tests are not claimed as rerun. Shared UI captures
are original ordinary/compact screenshots. Delivery:
[PR #170](https://github.com/GideonBa/Better-Linux-CAD/pull/170), implementation
`dc8d40f50f6e8c11f62542448a494bfd8092be83`, reviewed implementation tree
`7484af4d6386c1067a1bc8f7a3a2dc0b42803834`. Block 155 had not started at that acceptance.

### Retained extension boundaries

These existing conditions are retained when compacting completed delivery logs. The complete future
sequence/test/UI plans remain authoritative; completion of the preceding backend does not grant a
new authoring capability.

**Shell deletion:** Before a future shell Delete action, separately audit terminal-modifier removal and
restoration of the preceding Body producer.

**Explicit reference repair:** Missing sources in other feature/construction kinds remain outside this repair contract.

**Thin Extrude:** General curved/open chains remain outside the retained Geometry boundary.

**Sweep/Loft:** Combined
guide-plus-twist and unsupported G2 continuity surface as fail-closed diagnostics.

**Engineering resource ownership:** This is
one common resource store for later Hole/Thread tables, not independent per-module catalogs.

**Material-only reuse:** Tables
remain geometry inputs for their assigned later consumers.

**Hole workflow:** Existing top-to-bottom property/preview/Apply/Undo contracts apply; dedicated authoring remains 157.

Phase B stays open for 155–158.

### Block 155 serial implementation

The unchanged Block-155/B03 criteria remain in the sequence/test plan. Execute these steps in order;
individual delivery does not close the whole block or advance 156. The dedicated authoring panel
remains 157.

| Slice | Scope and acceptance | Status |
| --- | --- | --- |
| 155A | One shared Thread definition, authored/pinned profiles with exact provenance, independent start/length, handedness/mode/taper, dependency/remap queries and standalone external input record. Read-only Core validation and negative tests. | Implemented and verified; [PR #172](https://github.com/GideonBa/Better-Linux-CAD/pull/172) |
| 155B | Internal Hole and standalone Thread registration in Part/Project persistence, Body history, dependency/removal/suppression and atomic resource adoption. Preserve legacy bytes; prove reopen, parameter edits and failed late consumers. | Implemented and verified; [PR #173](https://github.com/GideonBa/Better-Linux-CAD/pull/173) |
| 155C | Checked supports/material/depths, cosmetic geometry invariance and derived display, explicit modeled helical execution with cost/capability limits. Complete independent pitch/hand/radial/volume oracles, blind clearance, resources/history/material/import/STEP combinations and relevant native checks at both scales. | Planned |

The shared contract lives in `hole-wizard.md`; V6 spelling lives in `file-format.md`. 155A was
merged in [PR #172](https://github.com/GideonBa/Better-Linux-CAD/pull/172). Main was verified/refreshed
before 155B began on `feat/semantic-thread-persistence`.

155B validation (cases / assertions): focused Core **17 / 382**, full Core **697 / 26,681**;
focused Geometry failure boundaries **2 / 13**, full Geometry **648 / 55,998**; focused offscreen Qt
**9 / 991**. Full offscreen Qt: **768 cases, 729 passed, 39 native-only skips; 92,292 assertions
passed**. Affected native filters pass with no skips at ordinary 100% (**9 / 1,013**) and compact 150%
(**9 / 1,027**); original captures are inspected. The Thread/Hole failure regressions also prevent
stale History success messages in the status bar. Five affected targets build; formatting/diff review
pass. All 1,035 runtime inputs remain frozen; unchanged Core/Geometry binaries retain their full-run
proof after the final GUI-only correction. Separate native app startup passes: a PID-matched X11
window is viewable before the intentional six-second timeout (124). Delivery:
[PR #173](https://github.com/GideonBa/Better-Linux-CAD/pull/173), reviewed implementation `b5b52b3`;
this delivery-record update leaves runtime inputs unchanged.
Whole Block 155 and the full B03 gate remain open; 155C is next, dedicated authoring remains 157.

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

---
doc: MVP Plan
role: >-
  Implementation-sequence source of truth. Feature-specific documents remain
  canonical for exact contracts, formulas, persistence details, failure
  policies, ordering, and focused proofs.
implemented_through: Block 156
current_block: 157
current_boundary: Block 156 accepted; 157A/B1 delivered; 157B2a verified; 157B2b next
current_tag: "[parity-b157]"
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
  advanced_modeling_workflows: "Blocks 145–156 accepted; Block 157 in progress; Blocks 158–204 planned"
---

# MVP Plan

This document owns numbered implementation status. Read the relevant contract linked below for
behavior, persistence, failure policy and focused tests; use [development setup](development-setup.md)
for checks. Completed work is indexed here rather than repeated as dated implementation logs.
Historical delivery details remain in the linked PRs and Git history.

## Current status

```text
implemented through  Block 156
current block        Block 157 (157B2a verified; 157B2b next)
current phase        Semantic holes and threads
current boundary     Editing, recipes and dimensional sources verified; full Wizard and combined acceptance remain open
```

The next assigned work is Block 157. [The consolidated sequence](inventor-parity-sequence.md)
contains dependencies and implementation criteria; [the test plan](inventor-parity-test-plan.md)
contains individual and combined acceptance; [the UI contract](user-interface.md) owns the
Inventor-like workflows and layout. These plans retain their full unimplemented scope.

## Delivery discipline

The authorized delivery order is one active implementation branch and at most one open PR. Finish
the complete issue, inspect the staged diff, run appropriate headless/native checks, update existing
contracts and evidence, commit and normally push, then merge its PR through main's rules. Fetch and
verify the delivered tree before starting the next issue from main. Do not stack unfinished PRs.

### Assigned continuation through Block 165 — 2026-11-11

The current request assigns the complete sequence through 165. Execute each delivery serially;
this assignment does not change acceptance status. Keep the detailed requirements in
[the sequence](inventor-parity-sequence.md) and the scenario oracles in
[the test plan](inventor-parity-test-plan.md).

| Next delivery | Completion boundary / proof |
| --- | --- |
| 156 | Delivered in PR #175: shared placements, Hole consumers, full suites, native 100%/150% and startup verified. |
| 157 | Semantic Hole/Thread creation and editing in the shared property workflow, explicit legacy conversion, presets and native B01–B04/UI01. |
| 158 | Phase A/B integrated acceptance, exact history/reopen/STEP and incremental-versus-fresh measurements. |
| 159 | Persistent oriented planar G2 constraints, solver/drag integration and independent S01 derivative/rank oracles. |
| 160–161 | Safe typed equation intent/persistence, then tolerance-controlled Geometry and real downstream S02/G01 consumers. |
| 162 | Native Smooth/equation authoring, conflict correction, closed profiles and persisted history. |
| 163–164 | Existing Sketch3D constraint schema, then deterministic solver/DOF/conflict diagnostics with S03 independent oracles. |
| 165 | Solver-backed spatial curve/constraint interaction, explicit formula override and native S03–S04 downstream acceptance. |

Each row retains its full numbered prerequisites and acceptance criteria; grouped rows do not
authorize partial acceptance of either block. Preserve all planned scope after 165.

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

Status authority: Blocks 145–156 are **Implemented and accepted**;
Block 157 is **In progress**; Blocks 158–204 are **Planned; not started**.
The explicit 2026-11-09 resume led to whole-154 acceptance on 2026-11-10.
The 2026-11-06 implementation request starts the serial slices below. Detailed block dependencies,
contract owners and completion criteria live in `inventor-parity-sequence.md`. Scenario definitions,
negative cases, combination matrix, native proof and performance protocol live in
`inventor-parity-test-plan.md`. Shared UI/layout behavior is owned by the planned section of
`user-interface.md`. Existing catalog lists are mapped there rather than assigned competing sequences.

| Phase | Blocks | Assigned scope | Status |
| --- | --- | --- | --- |
| A | 145–151 | Shared workflow/evidence, feature history/suppression/reorder, material resources and physical properties | Implemented and accepted |
| B | 152–158 | Semantic holes, extents/seats, cosmetic/modeled threads, placement patterns, Wizard UI and everyday acceptance | 152–156 accepted; 157 in progress; 158 planned |
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

The initial full parity plan was delivered in [PR #138](https://github.com/GideonBa/Better-Linux-CAD/pull/138).

### Latest implementation acceptance — Block 156, 2026-11-11

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

Phase B stays open for 157–158.

### Block 157 implementation

All [157/B01–B04/UI01 requirements](inventor-parity-sequence.md#block-157-hole-and-thread-property-panel-workflow)
remain assigned. Deliver named slices serially, with the parent block open until all criteria pass:

| Slice | Delivery boundary |
| --- | --- |
| 157A — verified | Typed Hole/Thread property editing over existing Core update commands, stable IDs/history, formula and pinned-resource ownership, whole downstream preview and rollback. |
| 157B1 — verified | Semantic Plain/ThroughAll Hole creation in the existing panel, target/support selection, pending dimensional ownership, Create Another and one final publication. Normal complete Part context first; no new Hole schema or cut interpreter. |
| 157B2 — in progress | Complete recipe/custom-standard/internal-external Thread selectors and native/imported support collection; keep 157A editing compatible. |
| 157B2a — verified | Nine recipes, six extent transitions, tip and custom internal Thread controls, typed dimensional table/custom sources, stable owned inputs and existing-feature recipe changes. |
| 157B2b | Complete standard Thread profile mappings, optional start/taper authoring, external Thread creation and scoped native/imported/result support and limit collection. |
| 157B3 | In-canvas position/depth/diameter interaction, referenced Sketch points and breadcrumb return, presets, explicit legacy conversion, full recipe/thread Create Another ownership and extended shared context lifecycle. |
| 157C | Native B01–B04/UI01 combinations at 100%/150%, invalid/corrected candidates, exact history/reopen and physical STEP; synchronize capability inventory. |

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
remain unchanged. Verify the merged tree and refresh main before starting the next branch.

Next: 157B2b completes Thread profiles/start/taper, external Thread creation and native/imported/result
support/limit collection. Then 157B3/C and full 158–165 remain assigned. Acceptance of 157B2a does
not close its parent 157B2 or Block 157.

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

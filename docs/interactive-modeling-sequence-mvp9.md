# Interactive Part & Assembly Modeling Sequence MVP-9

Status: active through implemented Block 123 after accepted Interactive Sketcher Block 121. Blocks
124–131 precede STEP Import MVP-10 (Blocks 132–138); Block 124 is the current next technical step.

This phase closes the interaction gap between the accepted validation GUI (MVP-7, Blocks 95–105)
and an intuitive, Inventor-familiar modeling experience for **every feature family implemented
through Block 94**. MVP-7 proved that each family is reachable through form-based task panels
(`docs/gui-feature-coverage-manifest-mvp7.json`). MVP-8 (Blocks 106–121) gives the Sketch
environment direct manipulation. MVP-9 gives the Part, Surface, and Assembly environments the same
quality: preselection-driven commands, in-viewport manipulators, live previews, feature editing,
and joint motion manipulation — without moving any authority out of Core/Geometry.

The goal is familiarity and directness, not a pixel-identical Inventor clone. Product names, icons,
artwork, and proprietary workflows of other CAD systems are not copied.

## Product outcome

A user can select a face or a finished Sketch profile and start Extrude directly, drag the extent
arrow while watching a live shaded preview, flip Join/Cut with one click, pick edges for a Fillet
with chain highlighting and drag the radius, ghost-preview a pattern before committing, double-click
any existing feature to edit it with the same manipulators, place components with a triad, author a
Mate by picking two compatible faces, and drive a joint through an in-viewport handle — all with one
undoable transaction per commit and full headless equivalence.

## Anti-hallucination authority contract

This sequence is written so that an implementing agent (human or AI) cannot silently invent
capabilities:

1. Every block lists **Existing authority** — the canonical documents whose contracts the block may
   rely on. An implementer must read those documents before coding and may rely only on contracts
   named there or verified in the current code.
2. Every block declares **New intent allowed** explicitly. `none` means the block is pure
   GUI/application/integration work over existing Core/Geometry contracts; any need for new
   persistent intent discovered during implementation stops the block and is escalated as an
   explicit new tagged boundary — it is never worked around inside widgets.
3. A capability that is *not* named in a block's Existing authority and *not* declared as allowed
   new intent does not exist for that block. Feature reorder, appearance persistence, section
   views, and constrained assembly free-drag are examples of frequently assumed but **absent**
   contracts; they are listed under Explicit deferrals.
4. Where a block requires an inventory of current mutation authorities (Block 129), the inventory
   result is a checked-in artifact, not implicit knowledge.

## MVP cut

Included:

- preselection-driven in-context feature commands and a contextual mini-toolbar;
- reusable transient viewport manipulators (linear, angular, radial, triad, pattern handles) with
  numeric HUD coupling;
- live candidate previews for all Part, Surface, and Boolean/Transform features through Block 94;
- feature editing (double-click) over explicit Core update commands;
- interactive Assembly placement, relationship/joint authoring with compatibility-filtered picking,
  and per-joint motion manipulation over existing coordinate drives;
- ViewCube/home-view navigation aids and a read-only measure tool;
- a machine-readable coverage manifest v2 mapping every Block-94 family to its interactive
  disposition.

Explicitly deferred (see the final section for the complete list): feature history reorder and
rollback, persistent appearance/material color, section views, drawings, direct modeling, and
general constrained free-drag of assembly components.

## Non-negotiable architecture rules

1. `blcad_core` and `blcad_geometry` remain free of Qt types; widgets remain clients of public
   Core/application commands (unchanged from
   `docs/gui-feature-validation-sequence-mvp7.md`).
2. Manipulators, ghosts, chips, mini-toolbars, ViewCube state, and previews are transient GUI state
   and are never serialized into model identity.
3. A manipulator edits exactly one candidate parameter set. Dragging previews a disposable
   candidate; release or Apply commits one document transaction; `Esc` or failure restores the
   pre-interaction state (the Block-110 drag contract generalized to features).
4. Preview never publishes a partial `ShapeCache`; failed validation, recompute, or solve keeps the
   last valid result and presents the authoritative diagnostic (MVP-7 rule 7 unchanged).
5. Viewport picking offers only semantic-supported topology for the active command. Topology
   without a stable semantic identity (see `docs/assembly-generated-topology-reference-mvp5.md`)
   is visibly unpickable with a stated reason — it is never approximated with raw OCCT identity.
6. Every interactive command has an equivalent headless Core/Geometry path, and acceptance proves
   equivalence (MVP-7 rule 8 unchanged).
7. Command enablement derives from selection capability via the existing target/capability
   taxonomy (`docs/assembly-geometric-target-taxonomy-mvp5.md`,
   `docs/part-feature-input-reference-mvp6.md`), not from widget-local heuristics.
8. No block silently extends Core semantics to make a widget possible. New Core intent appears
   only where a block's **New intent allowed** declares it, as its own tagged, tested boundary.

## Frozen workspace additions

```text
+--------------------------------------------------------------------------------+
| File | Undo/Redo | Sketch | Part | Surface | Assembly | Inspect | Exchange     |
+----------------------+---------------------------------------------------------+
| Model browser        |                                    +---------+          |
| - parameters         |          3D viewport               |ViewCube |          |
| - bodies/features    |                                    +---------+          |
| - assemblies         |   [selection mini-toolbar]                              |
|----------------------|   [manipulator + numeric HUD]                           |
| task panel           |   [live candidate preview]                              |
+----------------------+---------------------------------------------------------+
| status | capability hint | measure readout | recompute/solve diagnostics       |
+--------------------------------------------------------------------------------+
```

Interaction conventions reuse the frozen MVP-8 mouse rules (`docs/interactive-sketcher-sequence-mvp8.md`):
left click picks, drag manipulates a handle, `Esc` backs out one stage and never silently commits,
numeric typing opens the transient HUD, `Enter` accepts, double click edits.

## Frozen phase order

```text
122 modeling workspace, preselection command start, mini-toolbar, and navigation aids
123 transient viewport manipulator infrastructure and numeric coupling
124 interactive Extrude, path Extrude, and Revolve authoring
125 interactive Fillet, Chamfer, Shell, and Draft authoring
126 interactive Pattern, Mirror, Body Boolean, and Body Transform authoring
127 interactive PathCurve, Sweep, and Loft authoring
128 interactive Surface authoring and surface-to-solid conversion
129 feature edit lifecycle and the Core feature-update command boundary
130 interactive Assembly placement, relationships, joints, and motion
131 measure tool, coverage manifest v2, and integrated acceptance
132–138 STEP Import MVP-10
```

Do not merge these blocks. Manipulator infrastructure (123) precedes every feature block; the Core
update boundary (129) precedes nothing else but must not leak into 124–128, which author new
features only.

## Block 122 — Modeling workspace, in-context commands, and navigation aids — Implemented

`GuiModelingWorkspace` is now owned by the application shell and provides the shared selection-first
boundary for Part, Surface, and Assembly. Its catalog maps every later interactive command to an
active modeling area, document kind, first semantic selection kind, required verified capability,
mini-toolbar priority, and repeatability. Exact capability matching, rather than semantic-id or OCCT
shape heuristics, controls command enablement.

Starting a command consumes the current semantic preselection and enters the existing `GuiTaskState`.
Cancel restores both the selected object and its complete capability context; accepted preview-stage
commands record an area-compatible repeat command. Finish Sketch handoff publishes a transient
`ProfileRegion` preselection from authoritative `SketchId`/`ProfileId` values. All/Profile/Datum/
Face/Edge/Body/Component/AssemblyTarget filters synchronize the session and viewport masks and
remove incompatible hidden selections.

The same transient controller owns Part/Surface/Assembly tab state, deterministic contextual command
recommendations, ViewCube standard orientations, an explicit captured home view, and named camera
bookmarks. Camera state, mini-toolbar state, capabilities, filters, and handoff presentation ids are
not serialized. Core, Geometry, Part JSON, Project JSON, and sidecar formats are unchanged.

Existing authority:

- `docs/gui-feature-validation-sequence-mvp7.md` (shell, session, command lifecycle — Blocks 95–98)
- `docs/gui-part-foundation-workbench-mvp7.md`, `docs/gui-part-operations-workbench-mvp7.md`,
  `docs/gui-spatial-surface-workbench-mvp7.md`, `docs/gui-assembly-workbench-mvp7.md`
- `docs/interactive-sketcher-sequence-mvp8.md` (Block 106 command state machine, Block 119
  profile/Finish-Sketch results)
- `docs/part-feature-input-reference-mvp6.md`, `docs/assembly-geometric-target-taxonomy-mvp5.md`
  (capability-driven enablement)

New intent allowed: none.

Canonical contract: `docs/gui-modeling-workspace-mvp9.md`.

Focused tags: `[gui][modeling-workspace]`, `[gui][in-context-command]`,
`[gui][view-navigation-aids]`.

## Block 123 — Transient viewport manipulator infrastructure — Implemented

`GuiViewportManipulatorLayer` now supplies reusable Linear, Angular, Radial, TranslateAxis,
RotateAxis, PatternCount, and PatternSpacing handles. Each stable handle descriptor carries a
model-space origin/frame, current reference value, optional limits, fixed DIP presentation size, and
fixed DIP hit tolerance. Camera-ray mapping resolves linear values against model axes and radial or
angular values against explicit model planes. Display arrows and rings remain zoom-stable while the
resulting candidate values follow the current camera scale.

Hit testing uses point-to-segment or distance-to-ring screen metrics, ordered by DIP distance and then
stable handle id. Drag start freezes the initial model measurement so the parameter does not jump to
the visual handle endpoint. Release processes the exact final pointer sample. Valid numeric input
updates the same candidate and overrides later pointer movement until the override is cleared;
invalid units or fractional pattern counts leave the last valid candidate unchanged.

`GuiViewportManipulatorShellBinder` owns the transparent viewport overlay, mouse capture, `Esc` and
window-deactivation cancellation, camera refresh, and the shared numeric HUD. The controller and
binder emit candidate parameter values only and have no `GuiDocumentSession`, Core mutation,
Geometry-preview, or persistence authority. Blocks 124–130 remain responsible for disposable
feature previews and one validated Apply transaction.

Existing authority:

- `docs/interactive-sketcher-sequence-mvp8.md` (Block 107 mapping/hit-test conventions, Block 110
  candidate/commit contract)
- `docs/gui-feature-validation-sequence-mvp7.md` (Block 97 viewport and picking boundary)

New intent allowed: none.

Canonical contract: `docs/gui-viewport-manipulators-mvp9.md`.

Focused tags: `[gui][viewport-manipulators]`, `[gui][manipulator-numeric-coupling]`.

## Block 124 — Interactive Extrude, path Extrude, and Revolve

Start from a picked profile region or Finish-Sketch handoff. Show a live shaded candidate preview
with the operation mode color-coded; NewBody/Join/Cut/Intersect is a one-click toggle. The extent
arrow drags Distance/Symmetric/TwoSided values; ToNext/ToFace/Between switch to semantic face
picking with target highlighting; taper uses an angle wheel; thin-wall mode exposes one/two-sided
and mid-plane thickness handles. Path Extrude picks a connected PathCurve with trajectory preview.
Revolve picks a DatumAxis/ConstructionLine/semantic axis, previews the revolution, and drags full,
signed partial, or symmetric angles. The bolt-circle hole workflow keeps its dedicated command with
count/diameter fields and instance ghosts.

Existing authority:

- `docs/part-extrude-extent-intent-mvp6.md`, `docs/part-extrude-extent-geometry-mvp6.md`
- `docs/sketch-plane-extrude-direction-mvp.md`
- `docs/part-path-extrude-geometry-mvp6.md`, `docs/part-path-curve-core-mvp6.md`
- `docs/part-revolve-intent-mvp6.md`, `docs/part-revolve-geometry-mvp6.md`
- `docs/part-feature-body-operation-mvp6.md` (operation-mode and result-body rules)
- `docs/bolt-circle-pattern-mvp3.md` (hole workflow)
- `docs/gui-part-foundation-workbench-mvp7.md` (existing validation command set being upgraded)

New intent allowed: none — all seven extent modes, taper, thin intent, path direction, and Revolve
extents are frozen Core contracts.

Fail closed: extent modes whose semantic limit resolution fails stay previewed as errors and never
commit; incompatible profile selections disable the command with the required capability named.

Focused tags: `[gui][interactive-extrude]`, `[gui][interactive-revolve]`,
`[integration][extrude-revolve-manipulator]`.

## Block 125 — Interactive Fillet, Chamfer, Shell, and Draft

Hover highlights supported semantic edges/faces; clicking collects an ordered chain with duplicate
rejection. Edges without stable semantic identity are visibly unpickable with the reason from the
supported-producer matrix. Fillet drags the radius at the picked edge with live preview; Chamfer
switches EqualDistance/TwoDistance/DistanceAngle with per-mode handles and shows the derived
reference side. Shell picks removal faces, drags thickness, and flips Inward/Outward with an arrow.
Draft picks faces, a typed pull direction, and a neutral plane, then drags the signed angle with
the documented positive/negative convention visualized.

Existing authority:

- `docs/part-edge-treatment-intent-mvp6.md`, `docs/part-fillet-geometry-mvp6.md`,
  `docs/part-chamfer-geometry-mvp6.md`
- `docs/part-shell-intent-mvp6.md`, `docs/part-shell-geometry-mvp6.md`
- `docs/part-draft-intent-mvp6.md`, `docs/part-draft-geometry-mvp6.md`
- `docs/assembly-generated-topology-reference-mvp5.md`, `docs/semantic-references.md`,
  `docs/reference-recovery-mvp.md` (pickable-topology boundary)
- `docs/gui-part-operations-workbench-mvp7.md`

New intent allowed: none.

Fail closed: radius/thickness/angle values that fail Geometry keep the last valid preview state and
the authoritative diagnostic; missing/ambiguous recovered topology enters the existing repair
diagnostics instead of guessing.

Focused tags: `[gui][interactive-finishing]`, `[gui][interactive-shell-draft]`,
`[integration][edge-chain-picking]`.

## Block 126 — Interactive Pattern, Mirror, Body Boolean, and Body Transform

Patterns pick ordered Feature/Body sources, a typed direction or axis, and preview instance ghosts;
spacing/total-extent drags linearly, total-angle drags angularly, count edits through the HUD.
Mirror picks a Datum/Construction/semantic plane with a ghost preview. Body Boolean color-codes
target and tool roles and toggles keep/consume. Body Transform shows a triad on the target body;
dragging translate/rotate and a uniform-scale handle appends to the ordered persistent transform
stack (shown in the browser), with owned-Sketch movement indicated.

Existing authority:

- `docs/part-pattern-core-mvp6.md`, `docs/part-linear-pattern-geometry-mvp6.md`,
  `docs/part-circular-pattern-geometry-mvp6.md`
- `docs/part-mirror-intent-mvp6.md`, `docs/part-mirror-geometry-mvp6.md`
- `docs/part-body-boolean-mvp6.md`, `docs/part-body-boolean-geometry-mvp6.md`
- `docs/part-body-transform-ownership-mvp6.md`, `docs/part-body-transform-geometry-mvp6.md`
- `docs/pattern-and-mirror-features.md` (capability catalog)
- `docs/gui-part-operations-workbench-mvp7.md`

New intent allowed: none. The transform stack order is persistent intent; the triad appends
Translate/Rotate/UniformScale records and never collapses the stack.

Focused tags: `[gui][interactive-pattern-mirror]`, `[gui][interactive-body-operation]`,
`[integration][pattern-ghost-preview]`.

## Block 127 — Interactive PathCurve, Sweep, and Loft

PathCurve authoring picks connected segments (planar entities, ConstructionLines, Sketch3D curves,
semantic edges) with live connectivity/tolerance feedback; open/closed state and orientation rule
are explicit. Sweep picks profile and trajectory, previews orientation frames along the path, drags
twist with an angle wheel, and adds an optional guide with the guide-plus-twist rejection surfaced.
Loft collects ordered sections as numbered chips with drag-reorder in the task list, previews the
surface/solid between sections, exposes seam/alignment and normal intent where the Core contract
stores it, and offers C0/G1/G2 continuity with the explicit G2 failure rule visible.

Existing authority:

- `docs/part-path-curve-core-mvp6.md`
- `docs/part-sweep-intent-mvp6.md`, `docs/part-sweep-geometry-mvp6.md`,
  `docs/part-sweep-3d-geometry-mvp6.md`
- `docs/part-loft-intent-mvp6.md`, `docs/part-loft-geometry-mvp6.md`,
  `docs/part-multi-section-loft-geometry-mvp6.md`, `docs/part-guided-loft-geometry-mvp6.md`
- `docs/part-sketch-3d-geometry-mvp6.md` (spatial curve sources; interactive Sketch3D editing is
  MVP-8 Block 120)
- `docs/gui-spatial-surface-workbench-mvp7.md`

New intent allowed: none. Section order is persistent Loft intent; chip reorder edits that intent
through the normal transaction path.

Focused tags: `[gui][interactive-path-sweep]`, `[gui][interactive-loft]`,
`[integration][ordered-section-picking]`.

## Block 128 — Interactive Surface authoring and surface-to-solid

Boundary/Fill collect boundary-curve chains with connectivity preview. Trim picks one target
surface and one closed contour, highlighting the deterministic kept region (the no-keep-side-choice
contract stays visible instead of being hidden behind a guess). Extend picks the linear boundary
and drags the distance. Stitch collects surfaces and overlays free-edge/gap diagnostics with
color-coded open edges before commit; the tolerance is a typed HUD value. Closed-shell-to-solid
runs the closedness/manifold/orientation checks and reports each failure class explicitly before
offering conversion.

Existing authority:

- `docs/part-surface-feature-intent-mvp6.md`
- `docs/part-boundary-fill-surface-geometry-mvp6.md`
- `docs/part-trim-extend-surface-geometry-mvp6.md`
- `docs/part-surface-stitch-geometry-mvp6.md`
- `docs/part-closed-shell-to-solid-geometry-mvp6.md`
- `docs/gui-spatial-surface-workbench-mvp7.md`

New intent allowed: none.

Focused tags: `[gui][interactive-surface]`, `[integration][stitch-diagnostic-overlay]`.

## Block 129 — Feature edit lifecycle and Core feature-update commands

Primary boundary: editing existing persistent feature intent as first-class interaction.

First produce a checked-in **update-capability inventory**: for every feature family through Block
94, which public Core mutation authorities exist today (add, update, remove, parameter-driven
recompute). `PartDocument::update_sketch` and parameter edits are known examples; nothing else may
be assumed. Where a family lacks an update/remove command, add it as a narrow Core boundary:
validated, transactional, dependency-graph-consistent, id-stable, JSON-compatible — one tagged
sub-boundary per family, tested headlessly before any widget uses it.

Then: double-clicking a feature in browser or viewport opens the same task/manipulator UI from
Blocks 124–128 preloaded with the persistent intent; edits preview live and commit as one
transaction. Downstream features that fail after an edit are reported with the authoritative
diagnostics and the transaction fails closed (last-valid rule); there is no partial commit and no
silent suppression.

Existing authority:

- `docs/gui-browser-property-selection-mvp7.md` (existing typed property edit path)
- `docs/part-feature-body-dependency-mvp6.md` (graph, invalidation, removal protection)
- `docs/file-format.md` (JSON stability constraints)
- `docs/gui-feature-validation-sequence-mvp7.md` (transaction/undo rules)

New intent allowed: **yes, explicitly** — per-family Core update/remove commands where the
inventory proves them missing, under `[core][feature-update-command]`. No JSON shape changes; ids
and referenced identities remain stable across updates.

Explicit non-goals: feature history reorder and rollback markers. Feature order is
dependency-derived (`docs/part-feature-body-dependency-mvp6.md`); no Core reorder contract exists,
and this block must not invent one.

Focused tags: `[core][feature-update-command]`, `[gui][feature-edit]`,
`[integration][feature-edit-recompute]`.

## Block 130 — Interactive Assembly placement, relationships, joints, and motion

Insert a component by choosing the shared Part/subassembly and placing it with a triad; the triad
edits the persistent `ComponentInstance` placement. Grounding, visibility, and suppression stay one
click away. Relationship authoring picks the first semantic target, then highlights only
capability-compatible second targets (the Block-37 compatibility matrix drives the filter); the
solved pose previews on a candidate before Apply. Joint authoring previews the oriented frames.
Motion: each joint exposes its coordinate slots as sliders plus an in-viewport handle whose drag
maps to a single-coordinate vector drive with live posed preview and one atomic apply; limits and
DOF/solve status stay in the HUD.

Existing authority:

- `docs/component-instance-mvp5.md`, `docs/project-container-mvp4.md`
- `docs/assembly-constraint-model-intent-mvp5.md`, `docs/assembly-constraint-graph-mvp5.md`,
  `docs/assembly-constraint-target-resolution-mvp5.md`
- `docs/assembly-geometric-target-taxonomy-mvp5.md`, `docs/assembly-joint-target-compatibility-mvp5.md`
  (compatibility-filtered picking)
- `docs/assembly-generic-relationship-intent-mvp5.md`,
  `docs/assembly-generic-relationship-equations-mvp5.md`
- `docs/assembly-joint-coordinate-model-mvp5.md`, `docs/assembly-joint-coordinate-json-mvp5.md`,
  `docs/assembly-vector-joint-drive-mvp5.md` (motion drives)
- `docs/assembly-revolute-joint-motion-mvp5.md`, `docs/assembly-prismatic-joint-mvp5.md`,
  `docs/assembly-cylindrical-joint-mvp5.md`, `docs/assembly-planar-joint-mvp5.md`,
  `docs/assembly-spherical-joint-mvp5.md`
- `docs/assembly-rigid-subassembly-nested-export-mvp5.md`,
  `docs/assembly-flexible-subassembly-solving-mvp5.md`,
  `docs/assembly-suppressed-component-solving-mvp5.md`
- `docs/assembly-solve-diagnostics-mvp5.md`
- `docs/gui-assembly-workbench-mvp7.md`

New intent allowed: none. Placement transforms and coordinate drives are existing authorities;
Spherical remains rejected as a selected drive.

Explicit non-goal: general constrained free-drag (dragging a constrained component anywhere while
the solver follows). That requires a drag-objective solve mode that no Core contract provides; it
is deferred and may only arrive later as its own `[geometry][assembly-drag-solve]` boundary.

Focused tags: `[gui][interactive-assembly-placement]`, `[gui][interactive-relationships]`,
`[gui][interactive-joint-motion]`.

## Block 131 — Measure, coverage manifest v2, and integrated acceptance

Add a read-only measure command over derived Geometry queries: point/edge/face distance, angle,
radius/diameter, and body volume/solid counts from the existing inspection summaries. Results are
transient overlays and status readouts; nothing is persisted.

Publish `docs/gui-feature-coverage-manifest-mvp9.json` (schema
`blcad.gui-feature-coverage.v2`): every family of the MVP-7 manifest gains an `interactive`
disposition naming its owning Block 106–131 — or the explicit value `validation_sufficient` with a
one-line justification. CI fails when a family through Block 94 lacks an interactive disposition.

Acceptance proves at least:

1. a bracket Part authored end-to-end with mouse-first interaction: sketch (MVP-8), Extrude with
   dragged extent, Fillet with dragged radius, Shell, a pattern with ghosts, one Boolean — then
   feature edits via double-click, save/load/recompute, STEP export;
2. an Assembly with placed components, one compatibility-filtered Mate, one Revolute and one
   Prismatic joint, in-viewport motion drives, and analysis;
3. GUI/headless equivalence for every new interactive command;
4. fail-closed behavior: invalid pick capability, failed preview, failed solve, and failed edit
   commit produce no document mutation;
5. measured interaction budgets: manipulator hit testing and preview latency on representative
   models, reported by model size.

Existing authority:

- `docs/gui-feature-validation-mvp7-acceptance.md`, `docs/gui-feature-coverage-manifest-mvp7.json`
  (pattern being extended)
- `docs/part-multi-body-inspection-mvp6.md` (volume/solid summaries)
- `docs/assembly-interference-analysis-mvp5.md`, `docs/assembly-contact-swept-motion-mvp5.md`
- `docs/part-multi-body-step-export-mvp6.md`, `docs/assembly-posed-step-export-mvp5.md`,
  `docs/assembly-structured-step-products-mvp5.md`

New intent allowed: none.

Focused tags: `[gui][measure]`, `[integration][interactive-modeling]`,
`[integration][modeling-coverage-manifest]`.

After Block 131, STEP Import MVP-10 begins with Block 132
(`docs/step-import-sequence-mvp10.md`).

## Block-94 feature coverage matrix

Authoritative mapping from every capability family implemented through Block 94 to its validation
owner (implemented, MVP-7), its interactive owner (planned, MVP-8/MVP-9), and the canonical
contracts an implementer must read. `validation sufficient` means the MVP-7 workflow already fits
the interaction model (list/dialog workflows without a direct-manipulation upgrade path) — the
family is deliberately dispositioned, not forgotten.

| Capability through Block 94 | Canonical contracts | Validation (MVP-7) | Interactive owner |
|---|---|---:|---:|
| Parameters, expressions | `parameter-model.md`, `parameter-expression-mvp.md`, `parameter-update-mvp1.md`, `assembly-parameters-mvp4.md` | 98, 100 | 115 (dimension binding), 123 (HUD) |
| Datum planes/axes, workplanes | `workplane-resolver-mvp2.md`, `derived-workplane-mvp2-seed.md`, `bounded-workplane-validation-mvp2.md`, `assembly-reference-geometry-intent-mvp5.md` | 99 | 106/107 (sketch entry), 122 |
| Planar Sketch entities/constraints/dimensions/profiles/repair | see matrix in `interactive-sketcher-sequence-mvp8.md` | 99 | 106–119 |
| Bolt-circle hole pattern | `bolt-circle-pattern-mvp3.md` | 100 | 118 (sketch pattern), 124 (hole command) |
| Bodies, visibility, multi-body inspection | `part-body-identity-mvp6.md`, `part-body-json-mvp6.md`, `part-multi-body-recompute-mvp6.md`, `part-multi-body-inspection-mvp6.md` | 98, 100 | 122 (selection-first), 131 (measure) |
| Feature body operation modes | `part-feature-body-operation-mvp6.md`, `part-feature-body-dependency-mvp6.md` | 100, 101 | 124–128 (mode toggles) |
| Extrude/Cut extents, taper, thin | `part-extrude-extent-intent-mvp6.md`, `part-extrude-extent-geometry-mvp6.md`, `sketch-plane-extrude-direction-mvp.md` | 100 | 124 |
| Path-following Extrude/Cut | `part-path-extrude-geometry-mvp6.md` | 102 | 124 |
| Revolve/RevolveCut | `part-revolve-intent-mvp6.md`, `part-revolve-geometry-mvp6.md` | 100 | 124 |
| Linear/Circular Pattern | `part-pattern-core-mvp6.md`, `part-linear-pattern-geometry-mvp6.md`, `part-circular-pattern-geometry-mvp6.md` | 101 | 126 |
| Mirror | `part-mirror-intent-mvp6.md`, `part-mirror-geometry-mvp6.md` | 101 | 126 |
| Fillet, Chamfer | `part-edge-treatment-intent-mvp6.md`, `part-fillet-geometry-mvp6.md`, `part-chamfer-geometry-mvp6.md` | 101 | 125 |
| Shell | `part-shell-intent-mvp6.md`, `part-shell-geometry-mvp6.md` | 101 | 125 |
| Draft | `part-draft-intent-mvp6.md`, `part-draft-geometry-mvp6.md` | 101 | 125 |
| Body Boolean | `part-body-boolean-mvp6.md`, `part-body-boolean-geometry-mvp6.md` | 101 | 126 |
| Body Transform, Sketch ownership | `part-body-transform-ownership-mvp6.md`, `part-body-transform-geometry-mvp6.md` | 101 | 126 |
| 3D Sketch | `part-sketch-3d-core-mvp6.md`, `part-sketch-3d-curves-core-mvp6.md`, `part-sketch-3d-json-mvp6.md`, `part-sketch-3d-geometry-mvp6.md` | 102 | 120 |
| PathCurve | `part-path-curve-core-mvp6.md` | 102 | 127 |
| Sweep/SweepCut/SweepSurface, twist, guides | `part-sweep-intent-mvp6.md`, `part-sweep-geometry-mvp6.md`, `part-sweep-3d-geometry-mvp6.md` | 102 | 127 |
| Loft family, multi-section, guided, continuity | `part-loft-intent-mvp6.md`, `part-loft-geometry-mvp6.md`, `part-multi-section-loft-geometry-mvp6.md`, `part-guided-loft-geometry-mvp6.md` | 102 | 127 |
| Boundary/Fill Surface | `part-surface-feature-intent-mvp6.md`, `part-boundary-fill-surface-geometry-mvp6.md` | 102 | 128 |
| Trim/Extend Surface | `part-trim-extend-surface-geometry-mvp6.md` | 102 | 128 |
| Stitch/Knit/Sew shell | `part-surface-stitch-geometry-mvp6.md` | 102 | 128 |
| Closed-shell-to-solid | `part-closed-shell-to-solid-geometry-mvp6.md` | 102 | 128 |
| Feature editing (all families) | `part-feature-body-dependency-mvp6.md`, `file-format.md` | 98 (properties only) | 129 |
| Project, Assembly hierarchy, component placement | `project-container-mvp4.md`, `component-instance-mvp5.md`, `assembly-rigid-subassembly-nested-export-mvp5.md`, `assembly-flexible-subassembly-solving-mvp5.md` | 103 | 130 |
| Relationships (Mate/Distance/Angle/Concentric/Insert/generic) | `assembly-constraint-model-intent-mvp5.md`, `assembly-generic-relationship-intent-mvp5.md`, `assembly-generic-relationship-equations-mvp5.md`, `assembly-constraint-target-resolution-mvp5.md` | 103 | 130 |
| Joints (Revolute/Prismatic/Cylindrical/Planar/Spherical) | `assembly-joint-coordinate-model-mvp5.md`, `assembly-joint-target-compatibility-mvp5.md`, joint family docs | 103 | 130 |
| Motion drives | `assembly-vector-joint-drive-mvp5.md` | 103 | 130 |
| Suppression-aware solving | `assembly-suppressed-component-solving-mvp5.md` | 98, 103 | 130 |
| Solve/DOF diagnostics | `assembly-solve-diagnostics-mvp5.md` | 103, 104 | 130 (HUD) |
| Interference/clearance/contact/swept analysis | `assembly-interference-analysis-mvp5.md`, `assembly-contact-swept-motion-mvp5.md` | 104 | validation sufficient — list-driven; 131 adds measure overlays |
| Part multi-body STEP export | `part-multi-body-step-export-mvp6.md`, `step-export-mvp1.md` | 104 | validation sufficient — modal export dialog |
| Assembly flattened/structured STEP export | `assembly-posed-step-export-mvp5.md`, `assembly-structured-step-products-mvp5.md` | 104 | validation sufficient — modal export dialog |
| Semantic references, recovery, repair | `semantic-references.md`, `reference-recovery-mvp.md`, `assembly-generated-topology-reference-mvp5.md` | 99 | cross-cutting rule 5 (every pick flow) |

Any capability discovered later that is implemented through Block 94 but absent from this matrix is
added here with an owner before Block-131 acceptance is declared complete.

## Explicit deferrals

```text
feature history reorder, rollback markers, end-of-part editing
persistent appearance, material, and per-face color intent
section views, exploded views, and drawing/annotation output
direct modeling and face push/pull
constrained assembly free-drag (needs a new drag-objective solve boundary)
contact/collision-driven motion in the viewport
workspace customization, macros, add-ins, command search
photorealistic rendering and simulation
```

Each deferral is absent from Core contracts through Block 94; none of them may be introduced as a
side effect of a GUI block.

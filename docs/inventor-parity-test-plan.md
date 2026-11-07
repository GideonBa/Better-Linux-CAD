# Verification Plan for Advanced Modeling Workflows

This is the acceptance and test-design companion to [Blocks 145–204](inventor-parity-sequence.md).
Scenario IDs and future tags reserve planned work. They become evidence only when implemented
tests run against a recorded tree; dated evidence below identifies completed portions. `development-setup.md` owns actual commands/results;
`mvp-plan.md` owns numbered completion. Existing accepted test suites remain regression baselines.

## Test layers and required evidence

Each block adds its reserved `[parity-bNNN]` tag to executable tests in the appropriate existing
Core, Geometry and GUI targets. Multi-block scenarios retain their semantic scenario ID in the
test name or fixture metadata. Do not create placeholder passing tests for unimplemented blocks.

| Layer | What it must establish | Evidence that is insufficient alone |
| --- | --- | --- |
| Core | typed intent, unit math, IDs, dependency/cycle rules, mutations, strict JSON and migrations | successful JSON parsing without semantic checks |
| Geometry | independently checked real curves/shapes, topology recovery, full/incremental recompute and transactions | OCCT builder success or nonempty bounding box |
| Headless GUI | candidate lifecycle, input capability, exact history, selection/task state and preview freshness | direct calls to a lower-level feature adder |
| Qt offscreen | actual widgets, enablement, focus, keyboard, field bindings and dialog ownership | controller-only tests |
| Native Qt/OCCT | real selection, manipulators, visible geometry/overlays and ordinary/compact/scaled controls | offscreen viewport or screenshots without interaction/assertions |
| Exchange/integration | real files, imported-source identity, physical STEP reimport and portable reopening | file existence or STEP byte count |
| Performance | controlled workloads, cold/warm distributions, memory, correctness and responsive cancellation | one fastest sample or timings without environment/source tree |

Native UI changes require 100% and 150% scale runs, ordinary 1280×800 and compact 1024×640 checks,
reviewed captures and a separate application-startup smoke run. Save logs/captures under a stable
artifact location and record their paths/hashes in `development-setup.md`. A skip is not a pass;
if display/OCCT support is unavailable, leave that acceptance item open with the exact limitation.

## Invariants applied to every new feature family

For each applicable family test all of the following, selecting meaningful cases rather than
duplicating the implementation's branches line by line:

1. **Create, edit and remove:** valid inputs, dependency-protected deletion, names/IDs retained on
   update, all typed variants and source capabilities, multiple independent Bodies and historical
   bodyless compatibility fixtures where still supported.
2. **Persistence:** canonical Save/Open/Save, missing optional fields, exact legacy examples,
   malformed/unknown versions and enum values, duplicate/dangling IDs, wrong units, non-finite or
   out-of-range data, path traversal/asset policies inherited from STEP import, explicit migration
   repeated safely, and a new-document example that uses the actual writer.
3. **Dependencies:** edit each source category independently (parameter, sketch, face/edge, path,
   resource revision, state, pose); compare invalidated nodes and unchanged unrelated producers.
   Rejected cycles must identify the dependency chain without mutating the source.
4. **Geometry:** independent analytical or differential oracle, valid manifold/connected/oriented
   output as required, expected topology roles and interior/section witnesses. Confirm an intentionally
   wrong implementation would fail; do not only call the production query twice.
5. **Freshness/cache:** fresh full recompute equals incremental replay in physical invariants and
   semantic identity. Cover stale results after Undo, reorder, suppression, source refresh, material,
   state and occurrence-pose changes; deleted/reused IDs cannot revive obsolete cache entries.
6. **Atomicity:** snapshot complete committed Part/Project, sidecars, owned children, source hashes,
   undo/redo depth and freshness state. Invalid preview, failed apply, kernel/solver failure, Cancel,
   lost capture and stale candidate leave committed intent/history/source bytes unchanged. Scene
   restoration follows the explicit last-valid/repair contract rather than comparing raw pointers.
7. **History:** exact multi-step Undo/Redo, edit-after-Undo clears the redo branch, failed apply
   creates no history item, Save during preview stores committed intent, reopened documents replay
   the same physical result and bindings. Exercise prefix rollback and reordered mixed families.
8. **UI:** no preselection and valid/invalid preselection, keyboard-only and mouse paths, changed
   input roles, parameter/HUD synchronization, disabled action explanation, Ctrl+K discovery, Repeat,
   Escape at drag/task levels, document replacement, task closure and restored filters/camera.
9. **Consumers:** semantic target recovery, downstream Sketch/Hole/finishing, Body summary/physical
   properties, Assembly placement/constraints, analysis and STEP where supported. Unsupported
   consumers reject capabilities explicitly; no synthetic topology identity may be guessed.
10. **Limits:** zero/tiny/huge dimensions, near tangency, coincident-but-distinct sources, reversed
    orientation, mirrored/scaled input, singular derivatives, solver iteration/subdivision budgets,
    source disappearance and ambiguous recovery, with bounded execution and actionable diagnostics.

Fixtures use fixed seeds and known source revisions. Scale variants cover small/ordinary/large
geometry with a documented absolute plus relative tolerance policy. Numerical tolerance choices
belong in the owning feature contract and oracle metadata, not hidden multipliers in assertions.
Never assert raw OCCT face numbers, tessellation counts, unstable eigenvector signs or exact timing.

## Scenario catalog

Each scenario has a positive path, a deliberately invalid path and a recovery path. Shared
invariants above apply throughout. The release block is the first required integrated execution;
individual contributing blocks add focused portions as soon as their authorities exist.

### UI01 Inventor-like command and property workflow

- **First gate:** 145; repeat in every visible feature block and 204.
- Use one existing Extrude as baseline, then each new command: preselect source, open the expected
  ribbon/context action, fill ordered selectors, cross-highlight, edit a value in-panel and in-canvas,
  navigate the Sketch breadcrumb and return, Preview, Apply/Create Another, Cancel and edit via browser.
- Verify one transaction per Apply, no captured semantic target in a preset, invalid-field focus,
  Enter/Tab/Ctrl+Enter/Escape behavior, command search, Repeat, task conflicts and layout restoration.
- Use real Qt events. Assert enabled controls and visible geometry; inspect captures for clipped text,
  overlap, hidden selectors and unreachable bottom actions at both sizes/scales.

### H01 Mixed history suppression and reorder

- **First gate:** 147; native completion 148.
- Fixture: imported/native base → boss → Hole → Fillet → Chamfer → Body Transform → Boolean,
  plus an unrelated second Body. Insert all new feature families into this pattern when implemented.
- Suppress/restore modifier and creator separately, reorder legal independent operations, attempt a
  dependent-before-source cycle and remove/reuse a terminal ID. Verify bypassed Body predecessor,
  unavailable created Body, explicit dependent diagnostics and unaffected second Body.
- Oracle: fresh independent rebuild, measured volumes/sections and canonical graph/JSON snapshots.

146A foundation evidence is `[core][feature-history-inventory]`: all fifteen existing executable
families, historical byte oracles, interleaved predecessors, non-feature consumers and malformed
orders. Fixture provenance and exact checks live in `development-setup.md`. This prerequisite
does not satisfy suppression/reorder or Geometry/native H01 acceptance; those gates stay above.

146B adds `[core][history-intent]` for complete disposable candidates, sparse states, explicit
independent order, typed producer/resource dependency rejection, same-Body predecessor replacement,
append/remove/ID reuse across every current family, result-face/Project embedding and strict
version-1/2 migration. `[history-evaluation-boundary]` originally proved at 146B delivery that unsupported explicit
intent could not consume old Geometry or publish a falsely fresh GUI result. That temporary
boundary is replaced by physical effective/prefix/freshness proof in 147 below; the historical
rejection alone did not satisfy H01's effective-evaluation gate or the native browser gate in 148.

147A `[core][history-evaluation-plan]` proves logical availability across all families and every
prefix boundary, exact Body-input predecessors, suppressed creators/modifiers, resource-mediated
semantic output loss, blocked active chains, explicit dependent closures, independent Bodies,
consumed tools, imported/reference ownership, missing creators and misleading resource ID prefixes.
Canonical JSON, source graph and invalidation remain unchanged. This remains a Core projection;
147B must supply physical H01/H02, complete-source cache and native consumer evidence.

147B `[geometry][history-physical]` and `[history-evaluation-boundary]` compare all fourteen native
families against independent legacy rebuilds, including every prefix and suppression/restore.
Mixed/negative cases cover independent reorder, explicit dependent closure, unaffected Bodies,
identical ID/JSON reuse, failed suffix atomicity, imported base/boss/cut/Hole/Fillet/transform with
STEP readback, immutable Reference scopes and assembly analysis/export. Public cache writes/clear,
old intent, partial final-row scopes, source changes and direct-operation resurrection cannot
certify complete results. `[gui][history-evaluation-boundary]` combines native scene ownership,
Measure, empty-result export preflight, persistence, one-entry Undo/Redo and repair. Exact full/native
runs and acceptance remain governed by `development-setup.md`/`mvp-plan.md`; browser events, marker
insertion and Save during marker preview are 148 acceptance, not supplied by these evaluator tests.

### H02 End of Part and suffix validation

- **First gate:** 148.
- Move the marker before finishing, insert a feature, edit an upstream dimension and replay the suffix.
  Make a later Hole/Fillet invalid, cancel, then correct the dimension and commit once.
- Verify transient prefix cannot be exported or counted as a fully fresh model; Save during prefix
  preview retains complete committed history. Undo/Redo restores complete intent and marker policy.
- Oracle: clean full rebuild and unchanged persisted snapshots after rejected suffixes.

148B's session foundation is exercised by `[gui][history-prefix-session]`: complete retained intent,
every partial marker boundary, insertion/predecessor replay, invalid suffix correction, local and
complete Undo/Redo, committed main/Sketch-sidecar bytes, original redo restoration, copied sessions,
suppressed rows, imported suppliers and lost cache/lifetime ownership. Complete consumers reject
the partial cache even after the last row. That foundation supplies headless lifecycle evidence.

Native integration adds `[history-prefix-child]` session/controller checkpoint scenarios and
`[geometry][history-prefix-scope]` for explicitly qualified scenes, construction/rotation sources,
finishing and result-face inputs: default/wrong/stale/cleared scopes reject, while independent prefix
Body volume is checked. `[history-marker-shell]` drives actual Qt context, marker double-click,
private drag/drop, keyboard, Quick Access Save/Undo/Redo and existing property controls. It combines
upstream edits with invalid/corrected suffixes, prefix Extrude/source Sketch/gesture backout/Create
Another and inserted finishing, with independent Body checks and complete fresh replay. The source
Sketch and finishing cases require native xcb; their offscreen skips are not passes. Existing
`[history-import]` adds native EditableBody/reference marker availability. B integration results and
raw 100%/compact-150% captures are recorded in the existing setup/UI docs. Whole 148C separately
reconciles H01–H03/UI01 and capability promotion; the foundation alone does not complete them.

### H03 Native browser history workflow

- **First gate:** 148; repeat 188/204.
- Use actual drag/drop, context menus, double-click and keyboard navigation on mixed feature types.
  Cover folded browser nodes, long names, disabled illegal drop and suppressed feature selection.
- Verify diagnostic points to the affected dependency, correction keeps the task usable, selection
  remains semantic and Undo labels distinguish move/suppress/edit. Check compact/high-DPI controls.


148C composes the existing history authorities through `[history-combined-acceptance]` in
`gui_shell_history_tests.cpp`. Its mixed native-base/boss/Hole/Fillet/Chamfer/Transform/Boolean
fixture checks legacy/authored physical equivalence, explicit creator closure, modifier bypass,
independent reorder and two unaffected Body volumes. Another scenario combines suppression,
reorder, upstream prefix edit, restore, exact complete history and physical STEP readback. The
native parent-cancel case covers long names, folded state, an active Extrude child, selection,
camera, original redo and replacement. Final status and ordinary-1280×800 / compact-1024×640 native
results remain in the setup/MVP docs; registered tests alone do not promote the capability.


Completed 148 gate reconciliation (exact run counts/source/captures in `development-setup.md`):

| Gate | Executed evidence | Distinct oracle |
| --- | --- | --- |
| H01 | 146 Core inventory/intent; 147 physical all-family/imported evaluation; 148 controller/browser and `[history-mixed-chain]` native chain | Canonical intent/graph snapshots, independent legacy physical comparison, modifier bypass, creator closure and unaffected 200/1,600 mm³ Bodies |
| H02 | `[history-prefix-session]`, `[history-prefix-scope]`, `[history-marker-shell]`, combined prefix edit/restore/exchange | Exact retained complete intent/checkpoint bytes, explicit partial-cache rejection, invalid suffix correction, one root history entry and physical STEP readback |
| H03 | `[history-shell]`, `[history-marker-shell]`, `[history-combined-acceptance]` native 100%/150% | Real context/drag/double-click/keyboard input, forbidden/stale/foreign drops, folded/long-name cases, semantic selection/camera and distinct Undo/completion labels |
| UI01 | Accepted 145 baseline/source Sketch/Create Another plus 148 child/root input and native context cases | Existing command ownership, scoped child cancellation, committed Save, restored redo, fixed accessible root/child controls and 1280×800 / 1024×640 layout captures |

This closes those history gates at 148; their later repeats and combinations with future feature
families remain allocated by the existing sequence and combination matrix. Inventory proof resolution
is checked separately from the executed native scenarios and never substitutes for them.

### R01 Material resources and pinned revisions

- **First gate:** 149; native completion 151.
- Assign a synthetic material to a Part and another to one Body, insert the standalone Part into a
  Project, deduplicate identical resource IDs/revisions and explicitly update a catalog revision.
- Cover missing density, mixed units, conflicting same-ID contents, absent external library, state
  override later in 190 and failed multi-document resource update. Appearance changes cannot alter mass.
- Oracle: dimensionally converted pinned values, resource hashes and unchanged geometry fingerprints.


149A `[core][engineering-resource]` supplies the typed foundation: normalized density/pressure,
per-value provenance, absent density, schema/row/cell validation, immutable key collisions,
kind stability, explicit coexistence of revisions, copied snapshots and atomic late-conflict merge.
The count boundary is validated before integer conversion. These synthetic Core fixtures do not
supply document assignment, JSON/offline portability, coordinated multi-document updates or native
material UI; those remain 149B/C and 151. Exact execution/status stays in the setup/MVP docs.

149B1 adds `[engineering-resource-json]` / `[part-materials]`: all nine table value kinds and
four material properties, canonical units/provenance, malformed field/type/version/count boundaries,
Part defaults/Body overrides, removal/ID reuse, both history modes and result-face loading, exact
legacy bytes and explicit temporary Project-v1 rejection. `[resource-history-geometry]` checks
physical volume/bounds and scene identities independently of material pins, stale complete-cache
rejection, reopening and suppressed creator metadata. `[engineering-resource-integration]` exercises
session assignment/Undo/Redo/save, failed-open rollback, and real shell prefix edit/Cancel/insert/
Apply with committed Save, inherited defaults and retained overrides. Project-owned deduplication
and offline member portability remain B2; coordinated revision updates remain C. These tests use
synthetic resources and the existing history UI; they do not claim the material UI assigned to 151.

149B2 adds `[project-engineering-resources]`: deduplication with distinct Part defaults, copied
snapshots, late insertion/synchronization conflicts, explicit writer consistency, versioned shared
values/ordered member subsets, mixed legacy members, authored history/result-face staging, strict
malformed root/reference cases and exact empty/legacy normalization. `[project-resource-integration]`
uses real document-session transactions for Save/Open, Undo/Redo, automatic candidate reconciliation,
late-conflict and malformed-Open rollback, and standalone file extraction/reopening after removing
the Project file. These proofs retain pinned values without fetching an external library; explicit
multi-document revision switching/removal and material-only geometry reuse remain C/150.

149C adds `[resource-revision]`: exact default/Body pin remapping, retained old revisions and
unselected members, identical no-op/conflicting immutable keys, missing/duplicate/Reference/late
member rollback, kind/identity guards, table availability, referenced/default/override removal,
unassigned member snapshot guards, borrowed-key erasure and exact legacy normalization. Copies
retain their old snapshots. `[resource-revision-integration]` exercises one Project transaction,
Save/Open, Undo/Redo, failed late adoption with retained freshness/dirty/history state, and guarded
removal/clear/reopen. `[resource-revision-geometry]` independently compares volume/bounds/semantic
identity, unchanged executable generation, exact-source cache seals and reopened rebuilds.
Table consumer remapping/removal must extend this authority when 152 adds actual consumers.
Material-only BRep reuse remains 150; visible resource management remains 151.

### R02 Physical properties in transformed assemblies

- **First gate:** 150; native completion 151; extend 193.
- 150B `[assembly-physical-properties]` checks repeated and nested occurrences against independent
  root-frame centroid/tensor oracles, including hidden active parents/components, suppression,
  unused definitions and one integration per participating Part definition. Core
  `[assembly-physical-participation]` verifies unchanged default presentation filtering and stable
  physical ordering. Missing-density contributions retain a separate known-subset centroid.
  Pose/material/lifetime changes invalidate immutable snapshots; duplicate ownership, unsynchronized
  resources and active broken imports fail without publishing partial totals. Real STEP-source
  changes invalidate both Part and repeated hidden-occurrence Project results.
- 150A `[physical-properties]` supplies independently checked Part/shape oracles: boxes/cylinder,
  central and reconstructed principal tensors, large coordinates, rotation/scale/reflection,
  asymmetric differently weighted hidden Bodies, unavailable/partial/missing-density mass,
  empty/surface-only scope, legacy anonymous aggregate, consumed tools/suppression, near symmetry,
  numeric overflow/underflow and exact source/lifetime freshness. `[physical-imports]` uses a real
  STEP file for editable/Reference queries, missing context, changed supplier bytes and recovery.
  Owned complete recompute is the A boundary; posed assemblies and material-only reuse remain B/C.
  Exact scalar/tensor tolerance budgets are recorded in `engineering-modules.md`. These headless
  numerical tests do not claim the Physical Properties controls assigned to 151.
- Use analytic box/cylinder fixtures with asymmetric material assignment, a translated/rotated
  repeated child, uniform scale and reflected geometry. Sum the tensor with parallel-axis theorem.
- Compare whole Assembly mass against a hand-computed reference; hidden-but-active members remain,
  suppressed members leave, overlapping independent Bodies follow the documented summation policy.
- Check empty/open surfaces, missing material partial/undefined totals, near-symmetric principal axes,
  state/pose changes, unit conversions, clipboard and stale-query rejection.

### B01 Semantic bores and termination

- **First gate:** 153; native completion 157.
- Drill blind/through/ToNext/ToFace/up-to-Body holes into extruded, revolved, swept and imported
  supports with nonidentity workplanes. Change support thickness and each extent parameter.
- Oracle: cylinder/cone volumes, axial section intersections and expected entry/bottom coordinates.
  Check no-op/erased target, invalid support boundaries, tangent limits and ambiguous limit selection.
- Keep legacy CircleProfile through-all fixtures byte/meaning compatible and test optional conversion.

### B02 Seats fits and manufacturing variants

- **First gate:** 154; native completion 157.
- Exercise Counterbore/Countersink/Spotface, stepped, fitted, center-hole and slot variants using
  synthetic table entries and explicit dimensions. Verify preserved center/axis roles after type edits.
- Oracle: analytic annulus/cone/cylinder combinations and independent section profiles. Reject
  impossible depth/angle/seat size while showing nonblocking spacing recommendations as warnings.
- Combine with existing Chamfer, material mass and imported targets; check supported metadata export.

### B03 Internal external cosmetic and modeled threads

- **First gate:** 155; native completion 157.
- Pair an internal blind thread and external cylinder, both handednesses, coarse/fine synthetic
  pitches and partial thread lengths. Enable modeled geometry explicitly and return to cosmetic.
- Oracle: cosmetic mode preserves base volume/topology; modeled helix has independently sampled
  pitch, direction and radial envelope. Drill/thread depth and bottom clearance remain distinct.
- Exercise resource revision change, overflow/cost limits, incompatible support, no arbitrary norm
  values, actual STEP geometry and unchanged immutable imported base.

### B04 Plate cover and patterned hole assembly

- **First gate:** 156/158; expand 197/198.
- Build a material-assigned base and cover, multi-position holes and rectangular/circular patterns;
  change count, skip an instance, resize, add Fillet/Chamfer and assemble via per-hole axes/seats.
- Combine old Assembly parameter bindings with new Hole data. Retained instances keep identity;
  removed instances diagnose their own relationships. Verify physical alignment and mass change.
- Native chain: author → invalid edit → correction → Undo/Redo → save/reopen → structured STEP
  reimport. At 197 repeat through one coordinated assembly Hole with mixed participant recipes.

### S01 G2 planar spline constraints

- **First gate:** 159; native completion 162.
- Join cubic/rational splines to spline/line/arc fixtures, constrain tangent and curvature, drive
  dimensions and drag endpoints. Reverse curve orientation and rescale coordinates.
- Oracle: derivative-based oriented tangent/curvature vectors independent of solver residual builder;
  rank/DOF for analytically counted systems. Cover zero-speed, inflection and impossible fixed endpoints.
- Finish a closed contour to existing Extrude/Revolve and verify topology/constraints after reopen.

### S02 Equation curves and downstream modeling

- **First gate:** 160/161; native completion 162.
- Author parametric circle, parabola, sinusoid and spatial helix using typed coordinate/domain input;
  change parameters/domain and use valid curves as profiles or path sources.
- Oracle: analytic points, tangent, length/error bounds and real section/solid measurements. Test
  singular domains, high oscillation, mixed units, invalid grammar, open/closed mismatch and budgets.
- Changing display tessellation cannot change modeled shape/identity; explicit modeling-tolerance
  changes can, and must invalidate downstream results with a single undoable transaction.

### S03 Spatial constraints and curve interaction

- **First gate:** 163/164; native completion 165.
- Constrain a spatial wire with point/axis/plane/curve/face relations, dimensions, arc radius,
  spline tangency/G2 and a helix. Count rigid/gauge freedoms and compare independently checked residuals.
- Rotate/translate the complete system; solve from fixed seeds. Test coincident distinct IDs,
  external reference loss, rank deficiency, bad scales, incompatible bindings and iteration limits.
- Native grips/triad/typed HUD must agree and exact release commits once; invalid or cancelled
  interaction preserves formulas, source coordinates and every downstream consumer.

### S04 Spatial guide driven smooth duct

- **First gate:** 165; enhanced acceptance 174.
- Create two/three oriented profiles, constrained 3D guide curves and Sweep/Loft. Change one spatial
  dimension, correct a conflicting relation, then introduce impossible section/guide binding.
- Oracle: guide/section witnesses on actual surfaces, unaffected source IDs and unchanged committed
  document on failure. Replay the chain through Undo/Redo and reopened JSON with the same curve policy.

### G01 Path precision multi-wire and semantic sources

- **First gate:** 166/167; native completion 173.
- Mix exact rational spans, spatial splines, helix, arcs, reversed generated/imported edges and a
  hollow profile. Exercise Sweep/SweepCut/SweepSurface and path Extrude orientation/twist/guide modes.
- Oracle: analytic curve deviation/length plus independent path-normal sections and expected inner
  voids. Check closed-path seams, cumulative frame drift, self-intersection and competing controls.
- Compare old fixed-sampling replay with explicit policy upgrade; do not silently replace one result.

### G02 G2 Loft guide and support continuity

- **First gate:** 168/169; native completion 173.
- Loft unequal/rolled airfoil sections with two rails, center path and adjacent support faces. Test
  longitudinal smoothness separately from G2 across a boundary into a neighboring surface.
- Oracle: independent surface derivatives and normal curvature limits at multiple boundary/section
  stations, rail and section adherence, normal orientation and numerical refinement stability.
- Reject C1-only impostors, reversed curvature, invalid pole/guide binding and nearly failing G2;
  retain the last-valid shape and compare fresh/incremental output after edits.

### G03 Offset and Thicken geometry

- **First gate:** 170/171; native completion 173.
- Offset planes/cylinders and thicken open curved quilts with holes, both sides and symmetrically;
  exercise all supported Body operations and later Hole/Fillet on generated faces.
- Oracle: analytic radii/volumes and independently measured normal distances, side-wall closure,
  manifoldness and actual declared thickness error. Check narrow concavities and excessive offsets.
- Approximation modes must meet their min/max policy; lack of proof produces failure, not success.

### G04 Trim Extend Stitch and conversion

- **First gate:** 172; native completion/milestone 173/174.
- Trim a curved multi-face quilt, explicitly select keep region, extend a supported curved boundary,
  inspect free edges/gaps, Stitch and create a solid or Thicken before adding a bore.
- Oracle: intersection witnesses, loop orientation, shell connectivity, gap tolerance boundary and
  positive solid volume. Introduce a gap/non-manifold seam and verify actionable rejection/repair.
- Update the original contour/guide; after Undo/Redo and STEP reimport independently inspect geometry.

### F01 Variable radii and chain selection

- **First gate:** 175/176; native completion 179.
- Variable-fillet native/imported and patterned edges with multiple stations and retained chain IDs.
  Reverse a path, mirror a Body and resize an upstream Hole or patterned feature.
- Oracle: independent normal-section radius checks, solid validity and semantic endpoint mapping.
  Reject discontinuous chains, branch ambiguity, negative/excessive radii and stale picked edges.

### F02 Face full rounds and corner behavior

- **First gate:** 177/178; native completion 179.
- Blend two face sets, full-round a rib top and test supported three-way setback corners on native,
  imported and thickened geometry. Cover symmetric-looking but semantically distinct selections.
- Oracle: support contact, removed middle face region, real tangent/G2 checks and solid connectivity.
  Test tiny middle faces, invalid face roles, impossible corners and intended failure diagnostics.

### F03 Rib web emboss and engrave combinations

- **First gate:** 180–184.
- Add crossing ribs/webs to a housing, apply full rounds/holes, emboss parameter-driven text on a
  planar/cylindrical support and engrave a wrapped profile across a seam. Change skeleton and text.
- Oracle: known rib volumes/termination and wrapped coordinates; text holes remain holes. Physical
  mass tracks material change. Invalid joins/wraps preserve previous results and support identities.
- Reuse the resulting model for suppression/reorder, reference repair and direct-edit scenario D02.

### D01 Direct face edits and source preservation

- **First gate:** 185/186; native completion 187.
- Move/rotate a planar face, resize a cylinder, delete/heal bore faces and edit after existing blends.
  Cover native Bodies and converted EditableBody; Reference-mode editing stays unavailable.
- Oracle: physical selected-face displacement/diameter, expected changed material and untouched
  unrelated faces/Bodies, valid solid and unchanged source/imported-base hashes. Reject bad healing.
- Directly resizing a semantic Hole routes to Hole parameters or refuses, protecting thread metadata.

### D02 Edited import with downstream features and refresh

- **First gate:** 188.
- Import a housing, direct-edit it, create supported-face Sketch → boss/Hole → Rib → variable/full
  Fillet → Emboss and Body Pattern. Change material and reorder/suppress a safe operation.
- Refresh a compatible source, then an incompatible source with a removed face. Verify complete
  history replay or atomic rollback, explicit repair and retained source/version semantics.
- Native save/open and STEP reimport check actual changed geometry, mass and independent base shape.

### V01 Model states and evaluation isolation

- **First gate:** 189/190; native completion 191.
- Create size/manufacturing/material variants with sparse overrides; share a Primary Part between
  two occurrences selecting different states. Include formulas and suppressed creators/modifiers.
- Oracle: hand-evaluated parameters, independently rebuilt geometry/mass and unchanged Primary.
  Alternate state evaluations to detect cache contamination; reject bad references and cycles.
- View visibility and positional changes must not rewrite manufacturing overrides or vice versa.

### V02 Variant table assembly and member export

- **First gate:** 191; extend 198.
- Bulk-edit variant cells, duplicate/rename state, attempt invalid type/reference deletion, insert two
  variants and export one selected member as an independent document with new identity.
- Check atomic table failure, offline pinned resources, unchanged sibling states, explicit export
  state and independently reopened geometry. Combine Hole/Thread and suppression with physical data.

### A01 Independent repeated flexible subassemblies

- **First gate:** 192/193; native completion 198.
- Insert the same cylinder Assembly twice under differently rotated parents, give each a different
  prismatic extension, solve cross-hierarchy constraints and save a named position.
- Oracle: world-space endpoint geometry, per-occurrence transforms and unchanged child definition.
  Mass, contact, Measure and structured STEP must agree with the viewer's effective poses.
- Test grounded members, suppressed paths, stale apply, deleted nested path and old shared-child solve.

### A02 Directed adaptive cover and duct

- **First gate:** 194/195; native completion 198.
- In-context reference one Part's datum/face/parameter to shape another cover/duct, including a
  nonidentity source frame and repeated source occurrences. Change the explicit driving context.
- Oracle: independently transformed reference points and rebuilt consuming shapes; unchanged unrelated
  children. Try direct and indirect parameter/geometry/pose cycles and failed downstream booleans.
- Assert atomic root transaction, source-linked versus detached-copy meaning and complete reopened graph.

### A03 Coordinated holes through an assembly

- **First gate:** 196/197; native completion 198.
- One layout creates cover clearance/countersink and base blind thread holes on explicit participants.
  Drive count/size through existing Assembly bindings, model states and pinned standard revisions.
- Oracle: aligned axes, correct per-Part depths/sections and per-hole seats for Assembly Insert.
  Duplicate shared definitions, suppressed/missing participants and one failed recipe must not leave
  partial changes. Test explicit make-independent conversion and one root Undo for all Parts.

### A04 Context and representation lifecycle

- **First gate:** 198.
- Navigate Assembly → in-context Part → source Sketch → feature and back, with a pending preview.
  Switch model state, design view and position individually, close the task and reopen the Project.
- Assert visible breadcrumb/root identity, correct edit scope, restored filters, no unintended source
  edit, task conflict handling and distinct persisted representation data. Repeat at compact/high DPI.

### Q01 Motion contact and interval guarantees

- **First gate:** 199; native completion 202.
- Sweep revolute/prismatic/cylindrical and supported spherical trajectories past a thin obstacle,
  including a collision strictly between uniform samples, grazing contact and nested flexible Parts.
- Oracle: analytic moving primitive collision intervals plus dense independent checks for complex
  examples. Broad-phase candidate pruning cannot discard an actual collision. Refine reported brackets.
- Resource-limited unresolved intervals remain Unknown, and sampled Clear is labelled sampled.
  No source pose or persisted coordinate changes merely because an analysis was run.

### Q02 Real finite-domain measurements

- **First gate:** 200; native completion 202.
- Pick finite skew segments, trimmed planes, circles, spline edges and curved trimmed faces in
  transformed assemblies. Compare finite minimum to supporting-geometry distance where different.
- Oracle: independent analytic segment/domain distances, witness containment and numerical extrema
  checks. Cover multiple minima, intersections, singular curvature and stale state/pose selections.

### Q03 Surface continuity curvature section and thickness

- **First gate:** 201; native completion 202.
- Use known G0/G1/G2 joins, plane/cylinder/sphere surfaces and a hollow housing with ribs, emboss,
  fillets and deliberately thin engraving. Query sections and different declared thickness methods.
- Oracle: curvature 0/1/R and known Gaussian values, analytical wall thickness/section areas, valid
  normal orientation and refinement convergence. Zebra images complement derivative checks.
- Test seams, open surfaces, under-resolved small features and explicit uncertainty/coverage limits.

### Q04 Inspection scene and freshness

- **First gate:** 202.
- Run Physical Properties, finite Measure, Zebra/Curvature/Thickness, Section and Motion/Contact
  on one model; change camera, source geometry, material, selected model state and occurrence pose.
- Results invalidate only for their actual inputs; geometric/material stale data never remains
  labelled current. Verify legends, unit conversions, overlays, selection masks, clipboard and close.
- Inspect/cancel/save must not create CAD history or serialize derived sample/results as intent.

## Required combination matrix

Each row is a mandatory integration edge. Both directions of upstream change matter where meaningful.
These obligations accumulate; implementing a late feature must rerun the earlier consumer edge it
changes. Every new family must participate in at least one persisted three-feature workflow.

| Combination | Required cases | Scenario and release gate |
| --- | --- | --- |
| History × each new family | suppress/reorder/restore and blocked dependent | H01/H02, each family UI block and 204 |
| Material × new solid operations | independent volume/mass, override and no-density result | R02, 158/174/188/204 |
| Hole × Thread × Pattern | retained placements, skip, modeled/cosmetic, count change | B03/B04, 158 |
| Hole × Fillet/Chamfer × history | entry after modifiers, reorder invalidation and correction | B04/F01, 158/188 |
| Planar G2 × equation × Extrude/Revolve | closed regions, constraints and domain changes | S01/S02, 162/174 |
| Spatial solver × equation × Sweep/Loft | source updates and rejected constraints/guide bindings | S04/G01/G02, 174 |
| Loft G2 × support faces × exact paths | verified boundary and longitudinal continuity | G02, 174 |
| Offset/Thicken × Trim/Extend × Stitch | tolerances, seams and valid solid conversion | G03/G04, 174 |
| Variable/full Fillet × Thicken × Hole | semantic face/edge recovery after thickness/size edit | F01/F02, 188 |
| Rib/Web × full round × Shell/Hole | connected material and failed thin/intersection cases | F03/F02, 188 |
| Emboss/Engrave × Thread × material | geometry versus metadata changes, correct mass | F03/B03/R02, 188 |
| Direct Edit × imported refresh × downstream feature | immutable base and atomic lost-face handling | D02, 188 |
| State × Hole/Thread × material/suppression | per-state geometry, metadata and physical mass | V01/V02, 191 |
| State × flexible pose × nested Assembly | state/pose cache isolation and correct instances | V02/A01, 198 |
| Adaptive reference × Sweep/Loft/Thicken | transformed source, cycles and full-project rollback | A02/G04, 198 |
| Assembly Hole × state × shared definition | explicit participant policy and one root transaction | A03/V02, 198 |
| Assembly Hole × adaptive source × pattern/Thread | coordinated axes/depths after driver change | A03/B04, 198 |
| Flexible occurrence × contact × mass/STEP | every consumer observes identical effective pose | A01/Q01/R02, 202 |
| Representation × every active task | cancellation/switching, correct persistent edit scope | A04/UI01, 198/204 |
| Finite Measure × new faces × transformed Assembly | real trimmed domains and freshness | Q02/D02/A01, 202 |
| Zebra/Curvature × G2/Fillet × direct edit | numerical continuity and visual agreement | Q03/F02/D01, 202 |
| Thickness × Rib/Engrave/Thicken × state | thin regions, variant-specific coverage | Q03/F03/V01, 202 |
| All families × save/open/history/exchange | portability, semantic identity, physical result | X01–X03, 204 |

### X01 Rotor with smooth blades and patterned hardware

- **Gate:** 174 curve/surface portion; 188 advanced Part portion; 204 complete including model states.
- Build a revolved hub, equation/3D-constrained blade guides, verified Loft/Thicken, variable root
  blends, pattern, threaded mounting holes and material assignments. Add raised/engraved identification.
- Change blade count, guide dimension, thickness and root radius; insert history edits and model
  states. Independently check blade sections, voids, continuity, mass and retained pattern targets.
- Native end-to-end create/edit/repair, rejected geometry, Undo/Redo, save/open and STEP reimport.

### X02 Configurable housing with coordinated cover

- **Gate:** 198 partial; 204 complete.
- Imported base → direct changes → ribs/full rounds → semantic holes/threads → embossed text;
  adaptive cover from explicit source geometry and coordinated assembly holes, size/material states.
- Change source revision and driver dimensions, then inject a lost face and one failed participant.
  Verify atomic correction/repair, independent variants, physical mass/thickness and unchanged base.
- Native context navigation, table edits, history, save/open and structured re-export must agree.

### X03 Twin-cylinder mechanism and inspection

- **Gate:** 202 partial; 204 complete.
- Reuse one subassembly at different flexible extensions and orientations, with state-specific
  end parts, named positions and an obstacle. Run motion/contact, finite Measure, mass and Section.
- Compare actual posed endpoints, collision intervals, total inertia and physical STEP reimport.
  Change a source/state/pose and check every dependent query invalidates correctly; view-only
  changes preserve physical intent. History/reopen reproduces the same named mechanism positions.

## P01 Performance and robustness workload

Capture existing-capability baselines in 145; add each future workload's first baseline when its
producer is implemented. Measure each affected family during delivery and enforce the final
chosen budgets in 203. Use the existing rotor benchmark JSON style; keep data under `docs/benchmarks/`
and reproducible scene generators/examples. Do not manufacture measurements in this planning change.

| Workload | Initial sizes to measure | Correctness and performance observations |
| --- | --- | --- |
| Planar constraints | 50/250/1000 scalar variables, sparse and coupled | residual/rank, edit/drag p50/p95, solve/diagnostic cost |
| Spatial curves | 10/50/200 constrained points plus arcs/splines/helix | DOF, exact release, iterations and rejected solves |
| Rotor | existing 20/40 blades, new 100-blade stress | preview/edit/history/reopen latency and semantic pick cost |
| Hole and threads | 1/25/100 placements, cosmetic versus modeled | topology count, mass, time, memory and explicit modeling limits |
| Long Part history | 25/100/300 mixed operations | suppress/reorder, fresh/incremental identity and retained caches |
| Surfaces | increasing guide/section/quilt count | error/continuity/thickness accuracy versus build/query cost |
| States | 2/10/50 effective variants | cache isolation, activation cost and bounded memory |
| Nested Assembly | 10/100/1000 occurrences, repeated flexible definitions | resolver/solve/analysis/export, visibility versus physical scope |
| Contact | separated dense scene and thin-obstacle trajectory | candidate-pair pruning, no missed collision and Unknown fraction |

Record hardware, OS, Qt/OCCT/compiler versions, build type, seed, fixture/source hashes, scale,
threading, cache state and warmup. Measure at least ten warm trials and three cold trials for each
published comparison; report p50/p95/observed max, sample count, peak memory and failures. Startup
and file dialogs are measured separately from geometry execution. For costly stress cases, record
an explicit timeout and a bounded smaller workload rather than silently skipping them.

Initial UX targets to evaluate: coalesced input processing within a 16 ms frame budget when no
Geometry solve is required; input/cancel feedback within 100 ms; ordinary supported feature preview
and sketch drag solver within 200 ms p95 on the recorded reference machine. These are planning
targets, not measured guarantees. If synchronous kernel work prevents responsiveness, introduce
immutable candidate jobs with revision-based publication and cancellation checks; never mutate Qt
widgets from workers or publish stale intermediate results. Preserve deterministic final release.

Choose realistic workload-specific budgets from the baseline before optimizing. Fixed tolerances,
semantic identity, final-release accuracy and failure atomicity cannot be relaxed to meet latency.

## Running and recording acceptance

Use existing presets and paths from `development-setup.md`. Future focused invocations, after the
reserved block tag has actual registered tests, follow this pattern:

```bash
./build/dev/blcad_core_tests "[parity-bNNN]"
./build/dev-geometry/blcad_geometry_tests "[parity-bNNN]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[parity-bNNN]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[parity-bNNN]"
QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 ./build/dev-gui/blcad_gui_tests "[parity-bNNN]"
```

`NNN` is a placeholder; replace it with the implemented block number. Do not report an empty
Catch2 filter as successful validation. Existing CTest GUI discovery sets offscreen mode, so a
native gate must call the GUI binary directly. Full phase/final gates use the existing three
build/test workflows; serialize native runs/build mutations and follow actual configured paths.

A block completion entry records the tested tree, focused and regression results, native tests
without hidden skips, reviewed captures, geometric/transaction oracles, scenario edges and remaining
limitations. For a documentation-only planning delivery, validate links, block dependencies,
scenario references, ownership consistency and existing documentation-consuming tests; do not
claim future feature or native execution.

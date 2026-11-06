# Implementation Plan for Advanced Modeling Workflows

Planning baseline: 2026-11-06, main `e08280c`, after merged PR #137. This is the requested
implementation plan for closing the identified gaps inside the existing Sketch, 3D Model,
Surface, Assembly, Inspect and Manage workspaces. It allocates Blocks 145–204; implementation
has not started. `mvp-plan.md` alone owns numbered implementation status.

The delivery target is an Inventor-like engineering workflow backed by BLCAD's existing semantic
model, transactional editing and OCCT boundary. It is a bounded capability program, not a claim
of complete Autodesk Inventor parity. Drawings/BOM authoring, sheet metal, frame/weldment design,
CAM/FEM, general rigid-body dynamics, automatic fastener placement and a complete Content Center
remain outside this program. Existing implemented functions are extended rather than rebuilt.

## Reading and execution order

An implementing agent reads, in order:

1. `AGENTS.md`, `mvp-plan.md`, `development-setup.md` and the current delivery record.
2. This document's ownership table, architecture rules and the next assigned block only.
3. That block's existing contract owners and their relevant code/tests.
4. `user-interface.md#planned-inventor-like-workflow-contract` for any visible interaction.
5. `inventor-parity-test-plan.md`, including the block's scenario IDs and shared gates.

Before coding, write a bounded design entry in the owning contract: exact API/JSON changes,
supported target types, mathematics/tolerances, compatibility, failure policy and test oracles.
Proposed type names below describe responsibilities, not mandatory public API spellings. Inspect
existing authorities before introducing types. A block that needs another persistence/solver
authority must split into named serial subdeliveries in `mvp-plan.md` before implementation;
do not hide the expansion in a GUI patch or mark the parent complete after one slice.

## Consolidation and document ownership

This document replaces competing future execution lists for the capabilities mapped below. Older
MVP block sequences remain historical implementation records. Their implemented contracts and
legacy behavior remain authoritative until explicitly amended by the assigned new block. The
test plan owns scenario composition and evidence rules; it does not redefine feature mathematics.

| Capability | Existing contract to extend | New block owner |
| --- | --- | --- |
| Feature history, suppression, reorder | `feature-system.md`, `gui-feature-edit-mvp9.md`, `file-format.md`, `part-multi-body-recompute-mvp6.md` | 146–148 |
| Shared catalogs, materials, mass | `engineering-modules.md`, `parameter-model.md`, `file-format.md`, `gui-measure-mvp9.md` | 149–151 |
| Semantic Hole Wizard and threads | `hole-wizard.md`, `gui-part-foundation-workbench-mvp7.md`, `pattern-and-mirror-features.md` | 152–157 |
| Planar G2 and equation curves | `sketch-planar-constraint-solver-mvp8.md`, `gui-sketch-spline-text-mvp8.md`, `inventor-like-sketcher-and-feature-roadmap.md` | 159–162 |
| Spatial constraints and handles | `part-sketch-3d-core-mvp6.md`, `part-sketch-3d-curves-core-mvp6.md`, `gui-sketch3d-interaction-mvp8.md` | 163–165 |
| Paths, Sweep and verified Loft continuity | `part-sweep-3d-geometry-mvp6.md`, `part-sweep-geometry-mvp6.md`, `part-guided-loft-geometry-mvp6.md`, `part-loft-intent-mvp6.md` | 166–169 |
| Offset, Thicken, curved Trim/Extend | `advanced-surfacing-and-3d-sketch-mvp.md`, `part-surface-feature-intent-mvp6.md`, `part-trim-extend-surface-geometry-mvp6.md`, `gui-interactive-surface-mvp9.md` | 170–173 |
| Advanced Fillet | `fillet-chamfer-features.md`, `part-edge-treatment-intent-mvp6.md`, `part-fillet-geometry-mvp6.md`, `gui-interactive-finishing-mvp9.md` | 175–179 |
| Rib/Web and Emboss/Engrave | `feature-system.md`, `inventor-like-sketcher-and-feature-roadmap.md` | 180–184 |
| Direct face editing | `step-import-sequence-mvp10.md`, `feature-system.md`, `semantic-references.md` | 185–187 |
| Model states and variants | `parameter-model.md`, `file-format.md`, `assembly-system.md` | 189–191 |
| Independent flexible occurrences | `assembly-flexible-subassembly-solving-mvp5.md`, `assembly-cross-hierarchy-application-diagnostics-mvp5.md`, `assembly-joint-coordinate-model-mvp5.md` | 192–193 |
| Adaptive design and assembly holes | `parameter-model.md`, `assembly-parameters-mvp4.md`, `assembly-system.md`, `hole-wizard.md` | 194–198 |
| Motion/contact extensions | `assembly-contact-swept-motion-mvp5.md`, `gui-interactive-assembly-mvp9.md` | 199 |
| Finite measurement and quality analysis | `gui-measure-mvp9.md`, `gui-analysis-step-export-workbench-mvp7.md` | 200–202 |
| Shared UI and native proof | `user-interface.md`, `gui-shell-acceptance-mvp9r.md`, `development-setup.md` | 145 and every UI/integration block |

New mathematical contracts may be extracted from an oversized owner during implementation, but
must be linked from that owner and this table in the same change. There must be one authority for
each rule. Do not create parallel Hole, thread, material, history or parameter databases.

### Reconciled baseline

- Part features, Bodies, patterns, Mirror, Shell, Draft, Sweep/Loft and Surface operations already
  exist. `feature_order` already preserves interleaved feature/transform execution; it is not yet
  an unrestricted reorder or rollback command. Extend it deliberately.
- Constant Fillet and all three Chamfer modes already execute. Only the advanced Fillet variants
  are allocated here; historical catalog text must not trigger their reimplementation.
- Sketch constraints, dimensions including arc length, spline editing, snapping, region detection,
  repair and Finish Sketch already exist. Reuse them for the new curve and G2 families.
- Sketch3D already contains arcs, splines and helices. The gap is solver-backed spatial relations
  and complete interactive editing, not a second entity model.
- Guided Loft supports its documented C0/G1 contract. G2 is persisted but explicitly rejected.
  Spline/helix Sweep paths currently use fixed subdivisions; do not call those paths exact.
- Existing through-all Hole authoring is a CircleProfile/CircularHolePattern cut. Keep old files
  editable; introduce semantic HoleFeature separately, with explicit optional conversion.
- Assembly parameters/bindings, constraints, five joint families, nested placement, static contact,
  sampled Revolute analysis and structured STEP exchange already exist.
- Flexible-child solving currently updates shared child-document transforms. Independent internal
  occurrence poses need a new authority; they cannot be added as viewer-only transforms.
- Reference/EditableBody STEP import, explicit refresh, topology recovery and downstream features
  are accepted. Direct editing extends their immutable-base pipeline.
- The full-width seven-tab shell, Quick Access, command search and recoverable docks are accepted.
  Extend `ShellWindow` and its explicitly owned tools; do not introduce another shell/binder layer.

## Architecture decisions shared by every block

### Implementation entry points

Use this as a navigation map, then inspect actual callers and tests before choosing a patch.
Paths listed here exist at the planning baseline. New family-specific files are added to the
existing CMake targets; no standalone application, secondary geometry cache or new test runner is
required. A block extends only the relevant modules, not every file in its row.

| Area | Existing code entry points | Existing regression entry points |
| --- | --- | --- |
| History, feature mutation and JSON | `include/blcad/core/part_document.hpp`, `src/core/part_document.cpp`, `src/core/part_document_json.cpp`, `src/geometry/recompute_executor.cpp` | `tests/core/part_document_tests.cpp`, `tests/core/part_document_json_tests.cpp`, `tests/geometry/recompute_executor_tests.cpp`, `tests/gui/gui_feature_edit_tests.cpp` |
| Hole, resources and parameters | `include/blcad/core/feature.hpp`, `src/core/parameter_expression.cpp`, `src/gui/shell/shell_hole_tools.cpp`, `src/gui/shell/shell_parameter_tools.cpp` | `tests/core/parameter_expression_tests.cpp`, `tests/gui/gui_shell_hole_tests.cpp`, `tests/gui/gui_shell_parameter_tests.cpp` |
| Planar constraints | `include/blcad/core/sketch_constraint_authoring.hpp`, `src/core/sketch_constraint_solver.cpp`, `src/gui/shell/shell_sketch_tools.cpp` | `tests/core/sketch_constraint_solver_tests.cpp`, existing `tests/gui/gui_sketch_constraint_*` cases |
| Spatial entities and interaction | `include/blcad/core/sketch_3d.hpp`, `src/core/sketch_3d.cpp`, `src/geometry/sketch_3d_geometry_adapter.cpp`, `src/gui/shell/shell_sketch_3d_tools.cpp` | `tests/core/sketch_3d_tests.cpp`, `tests/core/sketch_3d_curve_tests.cpp`, `tests/core/sketch_3d_json_tests.cpp`, `tests/gui/gui_shell_sketch_3d_tests.cpp` |
| Paths and Loft | `src/core/path_curve.cpp`, `include/blcad/geometry/recompute_executor.hpp`, `src/gui/shell/shell_spatial_tools.cpp` | `tests/core/path_curve_tests.cpp`, `tests/geometry/sweep_feature_pipeline_tests.cpp`, `tests/geometry/guided_loft_pipeline_tests.cpp` |
| Surfaces and finishing | `src/core/surface_feature.cpp`, `src/core/edge_treatment_feature.cpp`, `src/gui/shell/shell_surface_tools.cpp`, `src/gui/shell/shell_finishing_tools.cpp` | `tests/core/surface_feature_tests.cpp`, `tests/core/edge_treatment_feature_tests.cpp`, `tests/geometry/fillet_feature_pipeline_tests.cpp` |
| Imported bases and Direct Edit | `src/geometry/step_editable_commands.cpp`, `src/core/step_import_source.cpp`, `src/gui/shell/shell_step_import_tools.cpp` | `tests/geometry/step_editable_commands_tests.cpp`, `tests/geometry/imported_body_downstream_feature_tests.cpp` |
| Assembly authoring and pose consumers | `include/blcad/core/assembly_document.hpp`, `src/gui/shell/shell_assembly_tools.cpp`, `src/gui/shell/shell_assembly_advanced.cpp`, `src/geometry/assembly_contact_analyzer.cpp` | existing `tests/core/assembly_*`, `tests/geometry/assembly_*` and `tests/gui/gui_shell_combined_workflow_tests.cpp` |
| Measurement and physical queries | `include/blcad/geometry/geometry_measure.hpp`, `src/geometry/geometry_measure.cpp`, `src/geometry/measure_selection.cpp`, `src/gui/shell/shell_measure_tools.cpp` | `tests/geometry/measure_selection_tests.cpp`, `tests/gui/gui_shell_measure_tools_tests.cpp` |
| Shared shell and coverage | `src/gui/shell/shell_window.cpp`, `src/gui/shell/shell_ribbon.cpp`, existing `GuiTaskState` and feature controllers | `tests/gui/gui_shell_coverage_tests.cpp`, `tests/gui/gui_shell_combined_workflow_tests.cpp` |

### Cross-cutting rules

1. Core owns persistent typed intent and identities; Geometry owns derived curves/shapes/queries;
   GUI controllers own candidates; Qt owns presentation. Existing dependency direction remains.
2. Stable feature/body/entity/occurrence IDs and semantic references are mandatory. Kernel indexes,
   sampled points, display labels, catalog row positions and floating-point equality are not IDs.
3. All authored relationships enter dependency/invalidation/recompute. Material and catalog versions,
   effective model state and occurrence pose become cache/freshness inputs where they affect results.
4. Introduce each new schema in `file-format.md` before writing it. Define missing-field defaults,
   strict enum/version validation, downgrade behavior and explicit migrations. Historical examples
   must reopen with unchanged meaning. Old readers must reject unsupported new intent, not drop it.
5. One command publishes one complete validated candidate. Preview, Cancel, failed solves and stale
   sources cannot change model, sidecars, history or files. Cross-document changes need a single
   Project transaction; never publish modified children before the root is validated.
6. Preserve last-valid Geometry on failed authoring. Loading/repair/history may preserve valid intent
   with diagnosed failed derived state under existing contracts. Do not silently fall back to an
   older shape, suppressed feature, weaker continuity or a different semantic target.
7. Quantities use dimensional analysis. Add density, mass, area/volume and inertia representations
   deliberately; do not force them into Length or use unlabelled scalar conventions in public APIs.
   Display units can differ from canonical calculation units without changing physical meaning.
8. Solver success requires residuals, validity, rank/DOF and declared tolerance checks. OCCT success
   alone does not prove manifold solids, G2 continuity, exact guide satisfaction or machining intent.
9. Shared resources have stable IDs, source provenance and immutable revisions. A document pins the
   values/revision used; installing a new library never silently changes authored geometry or mass.
   Standalone Parts must remain portable without an external Project or online service.
10. Geometric approximation has an explicit policy, tolerance and failure limit. Separate display
    tessellation from modeling curves. Preserve historical approximation semantics until an explicit
    migration or versioned intent selects the new executor.
11. UI placement and interaction follow the shared planned contract in `user-interface.md`. Every
    feature has creation, editing, cancellation, diagnostics, Undo/Redo and command discovery.
12. Completion requires implementation, contract updates, focused tests and relevant native proof.
    A planned capability is never enabled or added to implemented coverage before its evidence exists.

## Program phases and dependency order

Delivery is serial under `AGENTS.md`: merge the current PR normally, refresh main, then start the
next implementation branch. Logical prerequisites below supplement that delivery order. They allow
future agents to understand why an earlier authority is required; they do not authorize parallel PRs.

| Phase | Blocks | User-visible outcome | Exit evidence |
| --- | --- | --- | --- |
| A | 145–151 | Predictable history editing, materials and physical properties | H01–H03, R01–R02, UI01 |
| B | 152–158 | Complete bounded Hole/Thread workflow | B01–B04 and everyday milestone |
| C | 159–165 | G2/equation sketches and constrained spatial curves | S01–S04 |
| D | 166–174 | Accurate paths, verified G2 Loft and richer surfaces | G01–G04 and surface milestone |
| E | 175–184 | Advanced rounds, ribs/webs and emboss/engrave | F01–F03 |
| F | 185–188 | Editable imported faces and integrated advanced Parts | D01–D02 and Part milestone |
| G | 189–198 | Variants, independent flexible occurrences and adaptive assemblies | V01–V02, A01–A04 |
| H | 199–204 | Broader analysis, performance evidence and complete acceptance | Q01–Q04, X01–X03, P01 |

Scenario definitions and combination obligations are in [the test plan](inventor-parity-test-plan.md).
Every block registers its future `[parity-bNNN]` tag only when executable tests are added. Tags in
this plan are reservations, not claims that tests exist today.

## Phase A History and physical foundations

### Block 145 Shared workflow contract and evidence inventory

- **Prerequisites:** accepted Block 144 and this planning delivery merged.
- **Implement:** extend the existing coverage inventories with a planned/implemented distinction and
  mappings from command → capability → headless API → native test. Validate mappings without
  enabling planned commands. Capture baseline geometry, JSON, runtime hashes and interaction timings.
- **UI:** implement reusable accordion groups, active semantic selectors, field diagnostics,
  breadcrumbs and create-another behavior within existing task ownership. Resolve keyboard focus,
  compact layouts and task transitions once; feature blocks adopt these controls.
- **Proof:** UI01 and P01 baseline, including negative inventory entries and actual keyboard/mouse
  input. Existing workflow/appearance/coverage suites remain green.
- **Done:** shared controls and evidence format work with one existing feature; no CAD capability
  is newly claimed. Record any deliberate visual/workflow deviations from the Autodesk references.

### Block 146 Authored history and suppression intent

- **Prerequisites:** 145. Extend existing `feature_order`, feature/body predecessor semantics and
  `PartDocument` mutation commands rather than storing an unrelated feature tree.
- **Implement:** persistent per-feature active/suppressed intent, validated reorder candidates and
  typed dependency reasons. Cover every executable family, including BodyTransform and imported
  bases; define which indispensable base/producer operations cannot be suppressed or moved.
- **Contract:** suppression keeps identity and parameters. A modifier on an existing Body bypasses
  its operation; a suppressed creator supplies no Body. Missing produced outputs block consumers
  with explicit diagnostics. Preview may offer an explicit dependent closure; no silent cascade.
- **Proof:** H01 Core/JSON, mixed-family order, cycle rejection, legacy defaults and unchanged bytes
  for historical files without new state. Freeze migration policy in `file-format.md`.

### Block 147 Effective history evaluation and cache correctness

- **Prerequisites:** 146. Implement effective dependency evaluation, suppressed-feature bypass,
  Body predecessor restoration and bounded history-prefix recompute.
- **Contract:** prefix evaluation is disposable and cannot masquerade as a full fresh result.
  No suppressed/deleted producer cache may leak into picking, Measure, analysis or STEP export.
  Reordering preserves semantic references or reports their exact failure before publication.
- **Proof:** H01–H02 Geometry; independent fresh/full versus incremental comparisons after
  suppress/restore/reorder and reuse of IDs after deletion. Exercise imported and transformed Bodies.
- **Done:** existing history and STEP consumers explicitly choose a complete evaluation context.

### Block 148 History browser and End of Part workflow

- **Prerequisites:** 147. Add browser suppression, dependency-aware drag/drop reorder, context-menu
  edit and an End of Part marker using the existing browser and feature edit controller.
- **Contract:** moving the marker opens a transient prefix editing context. Saving writes committed
  complete intent; export/analysis from an incomplete prefix is disabled with a reason. Inserting
  or editing there stages the changed history and validates the complete suffix before one Apply.
  Cancel restores marker, selection, full scene and original history.
- **Proof:** H02–H03/UI01 actual input: legal/illegal drops, multiple Bodies, suppressed consumers,
  failed suffix, Save while previewing, Ctrl+Z/Redo, reopening and prefix-to-full camera restoration.
- **Done:** rollback editing, Undo and suppression have distinct visible meanings and test coverage.

### Block 149 Engineering resource revisions and material assignment

- **Prerequisites:** 146. Extend the catalog direction in `engineering-modules.md` and Project
  resources in `file-format.md`, including standalone Part snapshots and deduplication on insertion.
- **Implement:** stable resource IDs/revisions, source metadata, custom standards tables and materials;
  typed density plus optional engineering properties, Part default and Body override. Keep appearance
  separate. Custom fixture values are explicitly synthetic and need no normative label.
- **Contract:** precedence is Body override → Part default → unassigned. Material revisions update
  only by explicit transaction. Missing density yields unavailable mass, never a guessed density.
- **Proof:** R01 Core/JSON, dimensional conversions, duplicate/revision collisions, offline portability,
  resource update rollback and unchanged old documents. Reuse this store for Hole standards in 152.

### Block 150 Physical properties for Parts and posed assemblies

- **Prerequisites:** 147,149. Extend existing read-only Body inspection and assembly posed-leaf
  queries with volume, area, mass, centroid, full inertia tensor and principal properties.
- **Contract:** aggregate repeated occurrences separately; hidden components still have physical mass,
  suppressed components do not. Physical participation must not inherit viewport visibility filtering.
  State this separately from current visible-only export. Open surfaces have area but no solid mass.
  Define scale/reflection, overlapping independent Bodies, missing materials and partial totals.
- **Proof:** R02 analytic boxes/cylinders and parallel-axis transformations, nested occurrences,
  asymmetric rotations, unit changes, suppression and material-only invalidation without BRep rebuild.
- **Done:** report precision, coordinate frame and freshness; no identity depends on eigenvector sign.

### Block 151 Materials and physical properties UI

- **Prerequisites:** 148,150. Manage exposes Material management; Body/Part properties expose
  assignment; Inspect exposes Physical Properties with units, scope, centroid triad and clipboard.
- **UI:** reuse owned task/dock controls, explicit refresh and pending candidate presentation. Show
  material revision, missing assignments, whole/partial totals and coordinate frame. Changing display
  units does not author a model edit; assigning a material does.
- **Proof:** R01–R02/UI01 native assignment, overrides, multi-Body and nested totals, resource revision
  update, Save/Open and Undo/Redo. Verify property panels at ordinary, compact and scaled sizes.

## Phase B Semantic holes and threads

### Block 152 HoleFeature Core model and standards resolution

- **Prerequisites:** 149. Extend `hole-wizard.md`; introduce typed semantic Hole intent with target
  Body, stable placement IDs, semantic support/direction, dimensional parameters and revision-pinned
  standard selection or explicit custom dimensions. Reuse feature result and typed input authorities.
- **Contract:** independent placement points belong to Hole intent or reference existing Sketch points;
  repeated placement uses the shared pattern authority from 156, not a second pattern interpreter.
  Source geometry, dimensional data and manufacturing metadata have distinct dependencies.
- **Proof:** B01 Core/JSON, imported/native support faces, strict invalid types/references, legacy
  CircleProfile cuts unchanged, update/remove/graph/repair inventory participation.

### Block 153 Plain, blind and bounded hole execution

- **Prerequisites:** 152. Implement cylindrical through-all/blind bores, flat/conical drill tips,
  ToNext/ToFace and explicit up-to-Body extent evaluation with reusable Extrude limit queries.
- **Contract:** define support orientation, entry versus exit surface, finite-face placement, remaining
  bottom thickness and tangent/no-op intersection. Up-to-Body is a limit reference, not permission
  to cut every Body. Symmetric and multi-Body participant behavior is explicit, never inferred.
- **Proof:** B01 geometric volumes/depths and independent section checks on Extrude, Revolve,
  Sweep/Loft and imported targets. Reject erased solids, ambiguous limits and ineffective bores
  atomically. Verify generated cylindrical/seat references downstream.

### Block 154 Seats and manufacturing hole variants

- **Prerequisites:** 153. Add Counterbore, Countersink, Spotface, stepped, fitted/reamed and center-hole
  intent/geometry; a slot variant consumes the existing slot profile rather than inventing Sketch
  geometry. Extend HoleFeature's validated discriminated types and the pinned table dimensions.
- **Contract:** define depth origin for seats, included angle, intersecting entry faces, fit metadata,
  permitted variant combinations and warning/error boundaries. Impossible geometry blocks Apply;
  engineering spacing recommendations are identifiable warnings unless explicitly required.
- **Proof:** B02 formula-based seat/step volumes, axial sections and grazing/insufficient-material
  cases; update mode changes preserve placement IDs and downstream references where meaningful.

### Block 155 Semantic threads and explicit modeled threads

- **Prerequisites:** 154,149. One Thread definition serves internal Hole threads and standalone
  external ThreadFeature: system, designation, pitch, handedness, tolerance, start and length.
- **Implement:** default cosmetic metadata and derived display; optional modeled helical cut/add
  uses a checked tolerance-controlled executor and explicit cost/capability limits. Do not depend
  on a future rewrite of the general Sweep executor; share the low-level curve helper with 166 later.
- **Contract:** preserve pinned standard revision, distinguish thread/drill depths, and state which
  metadata the existing STEP writer actually preserves. Cosmetic export must never imply real threads.
- **Proof:** B03 right/left-hand, taper/straight supported types, blind bottoms, thread start/length,
  actual modeled pitch/diameter and semantic-only geometry invariance. Freeze unsupported combinations.

### Block 156 Placement patterns and stable Hole consumers

- **Prerequisites:** 153,155. Reuse existing linear/circular Pattern and semantic instance identity.
  Extend it where needed for two-direction placement, skipped instances and stable Hole seat/axis
  references. Define distinct placement IDs within one multi-position Hole and pattern occurrence IDs.
- **Contract:** a patterned operation and a multi-position Hole cannot double-apply the same cut.
  Changing count preserves retained logical instances; excluded instances remain explicit intent.
  A repeated Hole has per-instance references, not one ambiguous pattern-wide axis.
- **Proof:** B04 counts 1/2/many, full/partial circles, retained IDs, skip/restore, diameter changes,
  modifiers before/after pattern, downstream Assembly Insert and exact replay after reopening.

### Block 157 Hole and Thread property panel workflow

- **Prerequisites:** 156,145. Model → Hole/Thread opens ordered selectors and property groups:
  positions → target/direction → type/seat → extent → thread → advanced checks.
- **UI:** face/point preselection, in-canvas positions/depth/diameter, sketch breadcrumb return,
  sourced/custom size choice, multiple positions, presets and Apply/Create Another. Editing uses
  the same panel and true Hole/Thread intent. Legacy cuts stay editable; conversion is an explicit
  command with preview, new semantic metadata and preserved downstream mapping or a diagnostic.
- **Proof:** B01–B04/UI01 native keyboard/mouse workflows, invalid transitions, pick filters,
  presets without captured target IDs, compact UI, Undo/Redo, reopening and physical STEP checks.

### Block 158 Everyday modeling integrated acceptance

- **Prerequisites:** 145–157 complete. Run the plate/cover scenario B04 with new Hole/Thread,
  suppression/reorder, material mass, old Fillet/Chamfer/Pattern, Assembly relationships and STEP.
- **Proof:** Core + Geometry + full GUI, required native combination at 100%/150%, migrated and fresh
  documents, failed edits and exact undo chains. Measure incremental edits against fresh recompute.
- **Done:** all Phase A/B claims have current evidence; deferred advanced blocks remain unavailable.

## Phase C Planar and spatial sketch extensions

### Block 159 Persistent planar G2 constraint and solver support

- **Prerequisites:** 145. Extend the canonical planar constraint/authoring system with Smooth/G2
  for compatible spline endpoints against spline, arc or line. Reuse point IDs, rational weights,
  conflict attribution, drag equations and dimension sidecars.
- **Contract:** define oriented tangent and curvature-vector residuals invariant to curve parameter
  speed, normalized scales, zero-speed/degenerate endpoints and inflection behavior. A line has
  zero curvature; equal unsigned curvature alone does not prove a smooth junction.
- **Proof:** S01 analytic derivatives, reversed operands, rational conics, fixed/driving constraints,
  local DOF, redundant/conflicting G1+G2, drag release and unchanged historical solver behavior.

### Block 160 Equation curve persistent intent and safe expressions

- **Prerequisites:** 159. Extend the expression engine with an explicit curve-local independent
  variable/domain and typed 2D/3D coordinate expressions. Geometry samples never become authored
  points. Parameter references join the existing dependency graph and cannot shadow persistent IDs.
- **Contract:** define Cartesian/polar/parametric input modes, units of coordinates/angles/domain,
  finite domains, domain edits, deterministic errors and supported function grammar. No general
  language evaluation or filesystem/network access. Unsupported singularities fail explicitly.
- **Proof:** S02 Core parsing/JSON, dimensions, cycles, scope, parameter edits, malformed expressions,
  unknown functions and legacy Sketch/Sketch3D loading.

### Block 161 Equation curve Geometry and downstream profiles

- **Prerequisites:** 160. Implement analytic cases where available and adaptive error-controlled
  curve construction otherwise. Persist tolerance/policy as intent when it changes modeled shape;
  keep display chord error transient. Bound subdivision/evaluation and reject unresolved intervals.
- **Contract:** stable endpoint/curve roles survive tessellation changes. Closed domains require
  proven closure/orientation; self-intersection/ambiguous region detection follows existing rules.
  Discontinuities are not bridged by artificial edges.
- **Proof:** S02/G01 analytic circle/parabola/helix comparisons, high-frequency and singular cases,
  region materialization, Extrude/Revolve/Sweep consumers and deterministic resource-limit failures.

### Block 162 Planar spline and equation authoring UI

- **Prerequisites:** 159–161. Sketch → Constrain exposes Smooth; Draw exposes Equation Curve with
  expression/domain/tolerance preview, parameter binding and direct edit. Reuse spline handles and
  diagnostics; generated equation curves expose only meaningful endpoint/domain/parameter controls.
- **UI:** show tangent and curvature diagnostics in the task/canvas without claiming G2 from a
  control-polygon picture. Reference/derived curves cannot be freely dragged into unrelated splines.
- **Proof:** S01–S02/UI01 native G2 authoring, conflicts/corrections, expression edits, Finish Sketch
  to existing solid features, history, save/open and invalid-candidate cancellation.

### Block 163 Spatial constraint model and persistence

- **Prerequisites:** 160. Extend existing Sketch3D entities with stable constraint IDs and dimensional
  bindings, not a second point/curve model. Plan point variables X/Y/Z plus entity-specific scalar
  variables with explicit deterministic ordering and external read-only reference targets.
- **Scope:** coincident/fixed, axis/plane parallelism, point-on-line/plane/curve/supported face,
  parallel/perpendicular, tangent/G2 for supported curve endpoints, distance/angle/radius and length.
  Distinguish coordinate bindings from solver-driven DOF and make conflicts visible.
- **Proof:** S03 Core/JSON, target capability matrix, dependency cycles, external reference loss,
  parameter-driven points, and exact historical arc/spline/helix interpretation.

### Block 164 Spatial solver and diagnostics

- **Prerequisites:** 163. Implement normalized residuals/Jacobian, bounded deterministic solve,
  final rank/DOF and conflict/redundancy attribution using the existing numeric infrastructure.
  Reuse mathematical helpers, without forcing planar and spatial persistent schemas to coincide.
- **Contract:** define gauge freedoms, stable branch/orientation choice, curve parameters versus
  spatial distance and signed/unsigned angle conventions. External face projection remains derived
  Geometry capability supplied to the solver boundary, never a Core OCCT dependency.
- **Proof:** S03 independent residual/rank oracles, rigid motion invariance, badly scaled models,
  repeated seeds, singular arcs and nonconvergence. All rejected solves preserve source coordinates.

### Block 165 Solver-backed 3D sketch interaction

- **Prerequisites:** 164,162. Extend existing Sketch3D task with curve creation/edit/delete/rebind,
  arc/spline/helix grips, spatial constraints/dimensions and visible DOF. Reuse triads, axis/plane
  locks, distance-direction input, anchored pointer placement and exact-release drag transactions.
- **Contract:** parameter-driven geometry requires explicit rebind/override intent; dragging cannot
  silently erase formulas. Distinguish source-linked projected geometry from the existing explicit
  planar-point copy. Definition changes preview all downstream paths/features.
- **Proof:** S03–S04/UI01 native spatial authoring to Sweep/Loft, all cancellation paths, rotated
  camera/high DPI, conflicting drags, lost capture, exact Undo/Redo and persisted constraints.

## Phase D Curves paths and surfaces

### Block 166 Curve-preserving and tolerance-controlled path execution

- **Prerequisites:** 161,165. Replace fixed sampling only under an explicit compatible policy.
  Preserve exact supported spline/rational segments; use analytic or tolerance-controlled helix
  geometry where exact representation is unavailable. Reuse PathCurve order, reversal and frames.
- **Contract:** modeling tolerance, display tolerance and solver tolerance are distinct. Semantic
  endpoint identity cannot depend on number of subdivisions. Old fixed-sample documents retain
  their historical policy until explicit upgrade; compare and preview an upgrade before applying.
- **Proof:** G01 against analytic distance/tangent/length oracles, mixed reversed segments, seams,
  twists, guide paths, near-degenerate curves and Sweep/SweepCut/Surface/path Extrude consumers.

### Block 167 Semantic-edge paths and multi-wire Sweep

- **Prerequisites:** 166. Extend PathCurve execution to capability-compatible generated/imported
  semantic edges and Sweep profiles containing inner loops. Reuse topology recovery and existing
  orientation/twist/guide controls; source capability is explicit, not inferred from display type.
- **Contract:** orient outer/inner wires consistently, preserve holes, handle closed-path seams and
  reject self-intersection/non-manifold results. Define closed-spine frame closure and guide
  incompatibilities before enabling them. Ambiguous edge recovery never chooses the first match.
- **Proof:** G01 hollow pipe cross-sections, mixed-source paths, upstream edge splits, transformed
  imported edges, closed loops and all Body operations with independent section/volume checks.

### Block 168 Loft continuity and guide binding intent

- **Prerequisites:** 166,159. Extend existing section/seam/guide intent with explicit support-face
  boundary continuity and guide anchors when required. Preserve the existing longitudinal C0/G1
  interpretation; it is distinct from joining a neighboring surface with G1/G2.
- **Contract:** define guide station/anchor parameters, unequal sections, seam reversal, pole points,
  tangent scales and curvature orientation. Persist only authored bindings, never sampled sections.
  Old `g2` remains a diagnosed unsupported request until 169 supplies verified execution.
- **Proof:** G02 Core/JSON compatibility and ambiguity/cycle checks, parameter edits, distinct
  boundary-versus-longitudinal continuity and source/section identity after compatibility refinement.

### Block 169 Verified G2 Loft execution

- **Prerequisites:** 168. Add constrained surface construction and independent differential continuity
  checks for supported section/support combinations; document capability limits before GUI enablement.
- **Contract:** positional, oriented tangent and curvature checks use actual Geometry derivatives
  across joins/sections with controlled sampling/refinement and stated tolerances. A surface's
  internal C2 flag does not prove geometric G2 to a neighboring surface or exact guide satisfaction.
  Failed verification rejects the candidate; no fallback to G1 or approximate success label.
- **Proof:** G02 exact/known smooth transitions, airfoil sections, unequal edge counts, leading/trailing
  rails, reversed normals, singular poles and near-but-failing continuity with an independent oracle.

### Block 170 OffsetSurface and Thicken intent

- **Prerequisites:** 168. Extend `SurfaceFeatureKind` and typed semantic inputs with offset distance,
  side/symmetry, face/quilt selection, explicit approximation policy and Solid/Surface result mode.
  Reuse Body history, operation modes, semantic propagation and feature update commands.
- **Contract:** Offset creates surfaces without material; Thicken creates/changes a solid through
  offset and side-wall closure. Define tolerance, minimum/maximum thickness policy, inward/outward
  normals, multiple regions and permitted open boundaries. Distinguish existing Shell semantics.
- **Proof:** G03 Core/JSON/graph, old surface enum handling, Body identity, unsupported combinations,
  strict migration and dependency-protected deletion.

### Block 171 Checked Offset and Thicken Geometry

- **Prerequisites:** 170,169. Build offset faces and Thicken closure using derived OCCT results;
  validate intersection, orientation, manifoldness and actual thickness/declared approximation.
- **Contract:** narrow curvature/offset singularities and colliding side walls produce explicit
  diagnostics. No guessed healing publishes a solid. Preserve/derive semantic edge/face roles
  through history for subsequent Sketches, holes and finishing.
- **Proof:** G03 planes, cylinders, curved quilts, inner holes, symmetric thickness, excessive
  inward offset, multiple components and new/join/cut/intersect modes with section measurements.

### Block 172 Curved Trim Extend and surface gap queries

- **Prerequisites:** 171,166. Extend the planar-only Trim/Extend boundary to supported curved faces,
  curved boundaries, multiple faces and explicit keep/remove region selection. Add derived free-edge,
  gap and non-manifold query products shared by Stitch/Thicken diagnostics.
- **Contract:** freeze supported natural/tangent extension modes, trim side identity and multi-region
  output mapping. Do not infer a side from traversal order. Stitch tolerance never hides gaps larger
  than the authored limit; validation overlays are not persisted topology.
- **Proof:** G04 curved trimmed quilts, seam crossings, ambiguous region choice, parametric contour
  edits, gap tolerance boundaries, Stitch/Shell-to-Solid and failed downstream repair atomically.

### Block 173 Advanced curve and Surface workflow

- **Prerequisites:** 167,169,172. Extend Surface and Model Sweep/Loft property panels with compatible
  selectors, ordered rails/sections, continuity controls, tolerance/approximation feedback,
  Offset/Thicken direction manipulators, Trim keep/remove preview and free-edge/gap overlays.
- **UI:** maintain selectable source geometry while previewing. Cross-highlight selected inputs and
  invalid boundaries; offer existing reference repair. Editing reuses the creation controls and
  preserves supported preselection/selection filters on Cancel.
- **Proof:** G01–G04/UI01 native airfoil/duct and stitched-to-thickened workflows, exact release,
  mode changes, hidden selectors, compact layout, state restoration and physical STEP roundtrip.

### Block 174 Curves and surfaces integrated acceptance

- **Prerequisites:** 159–173. Combine equation/G2 sketches, constrained 3D guides, improved Sweep,
  verified Loft, curved Trim/Extend, Offset/Thicken, Stitch and existing Body/Pattern/finishing tools.
- **Proof:** S04/G04/X01 with edited middle section, changed guide, impossible offset/G2 request,
  undo to the original document, reopening and independent geometric checks after STEP reimport.
- **Done:** every approximation and continuity claim is supported by numerical evidence and UI
  wording; no display tessellation test substitutes for model-geometry validation.

## Phase E Advanced finishing and sketch-driven features

### Block 175 Variable Fillet and semantic chain intent

- **Prerequisites:** 146,166. Extend existing FilletFeature with a compatible discriminated radius
  law, ordered semantic edge groups, stable radius stations and explicit propagation rules.
- **Contract:** stations use authored normalized curve length or semantic endpoints, not kernel
  parameter values; define interpolation, orientation/reversal and duplicate station behavior.
  Tangent/connected chains have deterministic branch rejection and source provenance. Existing
  constant Fillet and three Chamfer modes keep their current record/geometry meaning.
- **Proof:** F01 Core/JSON, radius parameters, station edits, tangent-group expansion, graph/repair,
  chain reversal, split/lost edges and exact constant-mode compatibility.

### Block 176 Variable-radius Fillet Geometry

- **Prerequisites:** 175. Resolve groups on the current target Body and execute checked variable
  laws through OCCT. Verify local radius progression, side orientation and valid resulting solids.
- **Contract:** near-singular/self-intersecting radii, discontinuous chains and unsupported kernel
  combinations fail before publication. Report failing edges/stations without an invented maximum
  valid radius. Changing the seed recomputes downstream topology through normal reference recovery.
- **Proof:** F01 planar and curved edges, increasing/decreasing/multi-station radii, mirrored/scaled
  Bodies, imported and new Hole edges, independent section circles and atomic failure after Pattern.

### Block 177 Face full-round and corner-management intent

- **Prerequisites:** 175,168. Add typed face-set inputs for face blends and full rounds, with explicit
  side/middle roles, continuity choice, hold lines and declared setback/corner policies where supported.
- **Contract:** full-round removes/replaces the selected middle face set; it is not a constant edge
  radius. Distinguish tangent propagation, face-set ordering and three-way/multi-way corner handling.
  Core validates roles/capabilities; Geometry decides whether the blend can be built and verified.
- **Proof:** F02 Core/JSON on planar/cylindrical/spline faces, repeated roles, invalid ownership,
  support removal, parameter/dependency edits and unambiguous legacy mode dispatch.

### Block 178 Face blends full rounds and checked corners

- **Prerequisites:** 177,176. Execute the supported face/full-round/setback policies, retaining
  their semantic provenance and independently checking contact/continuity and solid validity.
- **Contract:** define a capability table for each corner/support/continuity combination. Unsupported
  corner solutions are diagnosed, not substituted by arbitrary caps. G2 uses the differential
  verification principles from 169; an OCCT blend label alone is insufficient.
- **Proof:** F02 two/three-face blends, full-round rib tops, three-edge corners, curved supports,
  tiny middle faces, upstream Hole edits, Stitch/Thicken inputs and failed continuity verification.

### Block 179 Advanced finishing property panels

- **Prerequisites:** 178. Model → Fillet exposes constant/variable/face/full-round modes in one
  owned panel. Show edge/face groups, editable stations, radius manipulators, chain propagation,
  corner choices and in-model failure locations. Existing Chamfer remains alongside it.
- **Contract:** mode switches stage a typed replacement with documented identity changes; incompatible
  fields cannot leak into Apply. Double-click restores all groups/laws. Unsupported capabilities are
  explained before a user invests in selecting unusable inputs.
- **Proof:** F01–F02/UI01 native station drags, face roles, repeated selection, Cancel/Escape,
  upstream changes, complete history/Save/Open and compact/scaled property panels.

### Block 180 Rib and Web intent

- **Prerequisites:** 147,167. Introduce Rib/Web as normal feature families using existing open Sketch
  curves/profiles, target Body, thickness/side, direction and termination references.
- **Contract:** define parallel/perpendicular-to-sketch extrusion, single/multiple connected curves,
  symmetric/asymmetric thickness, draft and extension to supported target faces. Rib and Web are
  related explicit modes of one authority where their mathematics agrees, not duplicate engines.
- **Proof:** F03 Core/JSON, acyclic support/termination dependencies, update/remove/suppression,
  graph inventory and legacy open-line Thin Extrude remaining distinct and unchanged.

### Block 181 Rib and Web Geometry

- **Prerequisites:** 180. Build thin volumes from the supported skeleton, resolve terminations and
  join to the target, checking material connection and minimum numerical thickness.
- **Contract:** disconnected/no-op ribs, branched ambiguous offset regions and self-intersections
  reject explicitly. Define how crossings between multiple rib curves combine. New semantic faces
  support later Shell/Hole/Fillet; retain all unrelated Bodies and transform ownership.
- **Proof:** F03 analytic rib volumes, curved/straight skeletons, cross webs, symmetric thickness,
  draft, imported supports, termination edits and full-round finishing across intersections.

### Block 182 Emboss and Engrave intent

- **Prerequisites:** 161,168. Add feature intent consuming existing Sketch regions/text and semantic
  target faces, with raise/recess depth, mapping mode, orientation and Body operation.
- **Contract:** begin with planar projection and explicit cylindrical/conical wrapping. Persist
  source/mapping intent and seam anchors, never flattened glyph BRep. Define text/font resolution
  reproducibility using the existing Sketch text contract. Arbitrary doubly curved wrapping requires
  a separately specified extension and is not silently approximated.
- **Proof:** F03 Core/JSON, parameter-driven text, mapping units, reversed normals, seam identity,
  unsupported supports and semantic source/font failure diagnostics.

### Block 183 Emboss and Engrave Geometry

- **Prerequisites:** 182,171. Implement supported projection/wrapping, depth offsets and checked
  Boolean results. Retain inner text/profile loops and semantic generated output roles.
- **Contract:** reject overlapping wrapped contours, ambiguous seams, singular supports and recesses
  that erase the target. No automatic mode change to planar projection. Depth remains physically
  meaningful relative to the documented surface normal/mapping convention.
- **Proof:** F03 independent planar volumes and cylindrical/conical mapping coordinates, seam
  crossings, text holes, changed text/height, existing thickness/fillets and material mass updates.

### Block 184 Rib Web Emboss and Engrave interaction

- **Prerequisites:** 181,183,179. Add Model creation tools and contextual starts from open curves,
  profiles/text and compatible faces. Reuse ordered selectors, depth/thickness grips and editing.
- **UI:** show termination arrows, side/draft and wrap seam orientation in the model; use the
  existing Sketch breadcrumb. Failed joins/mappings highlight the actual source or support.
- **Proof:** F03/UI01 native create/edit, parameter-driven text, invalid targets, termination repair,
  Undo/Redo, reopening, export and subsequent Hole/Fillet use of generated geometry.

## Phase F Direct editing of existing and imported geometry

### Block 185 DirectEditFeature authority and topology lineage

- **Prerequisites:** 147,175,170. Extend history after a native/imported Body with typed face Move,
  Rotate, Offset/Size and Delete-and-Heal operations. Whole-Body transforms reuse BodyTransform.
- **Contract:** identify selected faces semantically with expected capabilities and predecessor
  provenance. Record values, reference frames and explicit blend/heal policy. Do not rewrite the
  immutable STEP base, infer an invented original feature tree or mutate Reference-mode Parts.
  Direct resizing of a semantic Hole/Thread must route to its authored parameters or reject clearly.
- **Proof:** D01 Core/JSON, reorder/suppression, missing faces, stale imported catalogs, duplicate
  selections, supported parameter types and strict operation-version dispatch.

### Block 186 Direct face modification and checked healing

- **Prerequisites:** 185,178. Execute supported face changes, adjacent surface extension and
  delete/heal; produce checked solids and explicit old-to-new semantic provenance for recovery.
- **Contract:** healer tolerance/policies are explicit. Ambiguous blends, missing supports, multiple
  unintended solids or material changes outside the selected operation reject the candidate.
  Source refresh replays the complete history against strict recovered imported identities.
- **Proof:** D01–D02 physical planar translation, cylinder resizing, rotation and hole-face removal,
  failed healing, changed STEP source, exact base hashes, downstream Sketch/Hole/Fillet and export.

### Block 187 Direct Edit mini-toolbar and property workflow

- **Prerequisites:** 186. Model → Direct Edit uses face preselection, Move/Rotate/Size/Delete modes,
  local/world triad, reference origin, numeric parameters and normal task Preview/Apply/Cancel.
- **UI:** display dependent feature effects and failed heal locations. Reference Parts explain the
  existing conversion action; conversion remains explicit and produces a separate Editable Part.
  Double-clicking a Direct Edit history item restores operation, face set and values.
- **Proof:** D01–D02/UI01 native triad relocation/orientation, diameter/radius mode, failed deletion,
  lost capture, mode changes and exact undo/save/open with immutable source assets.

### Block 188 Advanced Part integrated acceptance

- **Prerequisites:** 175–187. Combine variable/full rounds, ribs/webs, emboss/engrave, direct edits,
  holes/threads, improved surfaces, material properties and existing patterns/body transforms.
- **Proof:** D02 and the advanced Part portion of X01: native and imported fixtures, failed refresh,
  suppression/reorder, generated
  reference recovery, corrected invalid edits, full/full-incremental equivalence and physical STEP.
- **Done:** both creation and editing are reachable for every new Part family; update the existing
  mutation/coverage inventories without rewriting historical accepted counts.

## Phase G Variants and assembly design

### Block 189 Model state and representation intent

- **Prerequisites:** 146,149. Add stable Primary/named model states with typed sparse overrides for
  allowed literal parameters, feature/component suppression, materials and selected child state.
  Formula changes use an explicit authored formula override rather than a hidden evaluated number.
- **Contract:** keep design-view properties and positional representations distinct from model states.
  Primary remains stable; inactive states do not duplicate whole documents. No recursive state
  inheritance initially. Define deletion/references and selected-state context on component occurrences.
- **Proof:** V01 Core/JSON, absent defaults, invalid override types, state cycles, repeated Part
  occurrences using different states and formula/parameter/category preservation.

### Block 190 State evaluation and isolated result caches

- **Prerequisites:** 189,147,150. Evaluate an immutable effective model for each requested state,
  with separate cache/freshness contexts for parameters, resources, topology and downstream results.
- **Contract:** activation is session state; authored state edits and component state references are
  persistent. Saving stores definitions/default policy without baking evaluated overrides into Primary.
  Define topology availability and relationship diagnostics when an occurrence changes state.
- **Proof:** V01–V02 state switching, suppressed roots, material-only variants, two concurrent state
  consumers, incremental/full equivalence, state-specific mass/export and stale result rejection.

### Block 191 Variant table and explicit member export

- **Prerequisites:** 190. Browser model-state controls and Manage table edit typed overrides with
  Preview/Apply. Support create/duplicate/rename/delete, Primary navigation and explicit state choice
  in Assembly placement, Physical Properties and export.
- **Contract:** bulk table edits are one validated transaction. Save Copy/member export generates
  independent documents with clear new identity and preserved pinned resources; it does not
  silently create an iPart/iAssembly factory or full release/BOM system. Define failed member rollback.
- **Proof:** V02/UI01 native table edits, invalid cells, external references, state switching during
  tasks, selecting two variants in an Assembly, independent export and unchanged Primary semantics.

### Block 192 Occurrence-local flexible pose authority

- **Prerequisites:** 189 and the existing accepted assembly pipeline. Extend authority with explicit rigid
  versus flexible occurrence intent and internal pose overrides keyed by stable rooted occurrence
  paths. Historical child-document transforms remain the default definition pose.
- **Contract:** two instances of one child may have independent internal poses without mutating the
  shared definition. Version and persist overrides, define orphan-path/restructure policy and
  precedence relative to positional representations. Avoid storing composed world transforms.
- **Proof:** A01 Core/JSON, repeated/nested occurrences, renamed labels versus stable paths,
  deleted child references and exact backward compatibility of shared-child local solving.

### Block 193 Flexible occurrence solving and all posed consumers

- **Prerequisites:** 192,190. Extend numeric transform authority, solve/applier freshness and joint
  motion to occurrence-local variables. Keep existing document-local solve commands operational.
- **Contract:** viewport, geometric targets, measurement, mass, contact and flat/structured STEP use
  the same effective pose resolver. Hidden physical components are retained for mass while display
  and export keep their explicit visibility rules. Reject conflicting authority assignments.
- **Proof:** A01 two differently extended cylinders, repeated nested children, cross-hierarchy
  joints, grounded members, constraints across occurrences and stale application. Check actual posed
  geometry/STEP and unchanged child-definition JSON, not only solver vectors.

### Block 194 Cross-document design references and dependency graph

- **Prerequisites:** 189,166. Extend the existing Assembly parameter bindings with explicit directed
  cross-Part datum/curve/face references, source occurrence/state/frame and target definition owner.
  Separate a source-linked projection/derived reference from a copied snapshot.
- **Contract:** define assembly-to-Part frame conversion, parameter scope and topology recovery.
  Cycle detection spans parameters, geometry, model state and adaptive pose dependencies. No raw
  OCCT or world-space sampled geometry becomes persistent source identity.
- **Proof:** A02 Core/JSON on translated/rotated/repeated sources, explicit detach/snapshot, missing
  source assets, read-only Reference Parts, multiple consuming Parts and cross-document cycle chains.

### Block 195 Adaptive evaluation and atomic Project transactions

- **Prerequisites:** 194,193. Implement context geometry queries and directed rebuild of dependent
  Part definitions inside disposable Project candidates, then validate the complete Assembly.
- **Contract:** an adaptive shared Part has one explicit driving occurrence/context; all occurrences
  share its definition change. Flexible pose and adaptive shape are separate. Reject recursive
  shape↔pose solve cycles in this program; there is no implicit iterative feedback solver.
  Authoring failure preserves every child, resource, sidecar and root transform/history entry.
- **Proof:** A02 source length/support changes update a cover/duct, irrelevant Parts stay unchanged,
  failed downstream boolean/solve restores the whole Project, explicit multi-asset Save failure
  follows existing publication rules and reopening reproduces frames/dependencies.

### Block 196 AssemblyHoleFeature and participant intent

- **Prerequisites:** 195,156. Implement the existing `hole-wizard.md` assembly-hole target using
  shared Hole definitions, shared placement/pattern identities and explicit participant roles.
- **Contract:** identify each affected Part/occurrence, its clearance/thread/seat recipe and authoring
  space. Multiple occurrences of a shared definition must not be cut twice or acquire contradictory
  definition changes; require an explicit shared-definition action or make-independent conversion.
  Reference-mode participants reject edits until explicitly converted.
- **Proof:** A03 Core/JSON, coordinated cover/base recipes, revision-pinned standard data, duplicate
  participants, participant suppression/state changes, ordered dependencies and role repair.

### Block 197 Coordinated assembly hole execution

- **Prerequisites:** 196. Map assembly placement to each participant, execute shared Hole geometry
  and replay downstream histories in one Project transaction, then recompute relations and physical data.
- **Contract:** participant-specific entry/exit and blind thread depths are defined in participant
  coordinates; assembly placement does not imply cutting unrelated Parts. No partially modified
  cover may survive failure to thread the base. Generated Hole references remain usable by Insert.
- **Proof:** A03 physical coordinated bores on nonidentity transforms, moved source layout, failed
  participant, shared definitions, model states, independent flex poses and exact root-level undo.

### Block 198 Advanced Assembly interaction and positions

- **Prerequisites:** 191,193,195,197. Add Flexible occurrence controls, in-context Edit Part with
  root/Part breadcrumb, adaptive source selectors, state/position browser controls and assembly Hole
  participant table. Extend existing Assembly tools instead of creating a separate design environment.
- **Contract:** positional representations store explicit joint/pose settings; design views store
  display state and model states store manufacturing/variant choices. Switching context releases
  temporary filters/previews. Adaptivity has one visible driver; users can inspect the dependency chain.
- **Proof:** A01–A04/V02/UI01 native two-cylinder and cover/base scenarios, context cancellation,
  rejected cycles/participants, named position recall, reopen/STEP and clear root-level Undo labels.

## Phase H Inspection performance and final acceptance

### Block 199 Broader motion and collision analysis

- **Prerequisites:** 193,198. Extend the existing sampled Revolute service to declared trajectories
  for supported joint coordinates, including prismatic/cylindrical and explicitly defined spherical
  orientation paths. Add deterministic broad-phase pruning and adaptive time refinement.
- **Contract:** report sampled contact separately from a guaranteed collision-free interval. For
  supported bounded rigid trajectories, add conservative interval bounds/time-of-impact brackets;
  unresolved intervals return Unknown at limits, never Clear. This is analysis, not contact response,
  force/friction/dynamics or a gear/rack transmission solver.
- **Proof:** Q01 fast thin obstacles between samples, tangencies, nested flexible/state occurrences,
  stopped/limited trajectories, deterministic pair ordering and source immutability.

### Block 200 Finite edge and trimmed-face measurements

- **Prerequisites:** 193,172. Extend `GeometryMeasureQuery` with minimum-distance queries on actual
  finite edges, trimmed faces, Bodies and posed occurrences, retaining analytic-support measurements
  as an explicitly labelled option. Add curve length and general local curvature readouts where defined.
- **Contract:** witness points lie on the selected finite domains; distinguish multiple minima,
  intersections and undefined curvature. Selection/freshness includes state, pose and source revisions.
- **Proof:** Q02 independent finite-segment, circle/sphere/cylinder and trimmed-domain oracles,
  spline extrema, assemblies and stale results. Preserve current analytic distance/angle conventions.

### Block 201 Surface quality and thickness query engine

- **Prerequisites:** 169,172,200. Add Zebra continuity products, signed/principal/Gaussian/mean
  surface curvature, section queries and wall-thickness analysis over actual modeled geometry.
- **Contract:** freeze frames, normal orientation, units, section loops and thickness definition
  (normal-ray/local versus global minimum). Return coverage/resolution/uncertainty for approximated
  regions and reject open/invalid domains when a solid-only metric is requested.
  Keep spline curvature combs from existing sketch tools; reuse their differential helpers.
- **Proof:** Q03 planes/spheres/cylinders, known G0/G1/G2 seams, constant-wall hollow solids,
  embossed/recessed thin areas, tiny features and mesh-refinement convergence with independent oracles.

### Block 202 Inspection UI and actionable overlays

- **Prerequisites:** 199–201. Inspect exposes Measurement mode, Physical Properties, Zebra,
  Curvature, Section, Thickness and Motion/Contact analysis with scoped selectors and legends.
- **Contract:** transient overlays never become pickable model identity. Distinguish sampled/verified/
  unknown results and show units, scope and stale status. Saved analysis recipes, if supported, store
  settings and semantic inputs only; saved view settings cannot change CAD or material intent.
- **Proof:** Q01–Q04/UI01 native overlays, camera/zoom/scaling, section manipulator, thin-wall
  highlight, clipboard, failed analysis, state/pose switches and zero document mutation on inspection.

### Block 203 Measured performance and robustness gates

- **Prerequisites:** 158,174,188,198,202. Run P01 with deterministic seeded fixtures, independent
  geometry/residual oracles, cold/warm timing, peak memory and fresh/incremental equivalence.
- **Scope:** dense Sketch constraints, 3D guides, 20/40/100-blade rotor, modeled versus cosmetic
  threads, long suppression/reorder histories, many variants and nested assemblies at increasing
  occurrence counts. Broad-phase and caching must preserve exact semantic/evaluation contexts.
- **Contract:** publish measured baseline and chosen budgets before tuning. Count attempts, discarded
  previews, failures and unknown intervals. Coalescing/cancellation must remain responsive and final
  pointer release exact. No portable speed claim from one development machine.
- **Done:** any missed budget or kernel capability is documented with a concrete follow-up; required
  functional guarantees cannot be waived by a faster visual approximation.

### Block 204 Final combined acceptance and delivery audit

- **Prerequisites:** all 145–203 complete. Run every mandatory scenario and combination edge in
  `inventor-parity-test-plan.md`, full Core/Geometry/GUI, relevant native suites at 100%/150%,
  compact layout captures, startup and real filesystem STEP/source workflows.
- **Audit:** all APIs/commands appear in the current coverage and update inventories, all migrations
  and examples are current, UI mapping matches implemented controls and every planned claim has
  evidence. Remove obsolete competing future sequence text only where replaced by actual contracts.
- **Done:** reviewed tree/staged diff, focused commits, normal push, protected PR delivery and tested
  merged-main reconciliation. Record exact versions, commands, results, skips, captures and unresolved
  scope. Increment implemented status only after the complete block passes; retain historical results.

## Delivery discipline and per-block handoff

Use the following handoff in the relevant `mvp-plan.md` entry and owning contract. Do not replace it
with an unqualified “done” or a list of new buttons.

```text
Block / named serial slice:
Starting main commit and preceding merged PR:
Existing owners read and contracts amended:
Supported inputs, math, capability limits and chosen schema/migration:
Changed Core / Geometry / GUI / example files:
New and existing tests; independent oracles; combination scenario IDs:
Focused / full / native 100% / native 150% results and actual skips:
UI captures and reviewed ordinary/compact/scaled controls:
Failure atomicity, freshness and fresh/incremental comparison evidence:
Performance observations if affected:
Reviewed diff, commit, pushed branch, PR and merge status:
Remaining slice(s) and exact next prerequisite:
```

New native scenarios use real Qt mouse/keyboard actions and the actual OCCT viewport. Direct
controller calls establish headless correctness but cannot close the native UI gate. Tests proposed
in this planning delivery are future obligations, not executed results.

## Primary workflow references

Use these Autodesk sources as workflow references; BLCAD's documented semantic, compatibility and
transaction rules remain its implementation authority. Review sources again when implementing a
workflow, since vendor documentation can change. Do not import Autodesk assets or proprietary code.

- [Inventor property panels](https://help.autodesk.com/cloudhelp/2026/ENU/Inventor-Help/files/GUID-3E5505B9-9F60-43A0-BF32-97D183B91A70.htm): ordered groups, selectors, breadcrumbs, value editing and create-another.
- [Inventor model browser](https://help.autodesk.com/cloudhelp/2026/ENU/Inventor-Help/files/GUID-EDF6B951-C309-44A8-868C-B72D99F6D5A7.htm): browser navigation, finding and editing features, recoverable panels.
- [Hole workflow](https://help.autodesk.com/cloudhelp/2026/ENU/Inventor-Help/files/GUID-94EB6947-0347-4888-870D-4BA22DB144D8.htm): positions, seat/thread choices, extents and previews.
- [Geometric constraints](https://help.autodesk.com/cloudhelp/2026/ENU/Inventor-Help/files/GUID-A85A7A30-7D81-4C75-8769-CAD034EEA930.htm): planar/spatial constraint interaction and Smooth/G2.
- [Placed features](https://help.autodesk.com/cloudhelp/2026/ENU/Inventor-Help/files/GUID-1FE5C77C-167B-4F89-BC39-FBFD7B27C197.htm): Fillet/Chamfer and Hole families.
- [Thicken and Offset](https://help.autodesk.com/cloudhelp/2025/ENU/Inventor-Help/files/GUID-A9A4FF6E-9569-4BED-8AC7-EB245E6456F8.htm): output, direction, thickness and approximation choices.
- [Direct Edit](https://help.autodesk.com/cloudhelp/2026/ENU/Inventor-Help/files/GUID-3C3258FE-E5B2-4219-BE90-BD8011580855.htm): face selection, operation modes and triads.
- [Model states](https://help.autodesk.com/cloudhelp/2026/ENU/Inventor-Help/files/GUID-8E771DBE-1107-4AE8-BE3E-AF3A7977F3C6.htm): configurations within one model and separate representation types.
- [Adaptive geometry](https://help.autodesk.com/cloudhelp/2026/ENU/Inventor-Help/files/GUID-82388C38-6027-46AE-AF55-E718FCF0580B.htm) and [positional representations](https://help.autodesk.com/cloudhelp/2026/ENU/Inventor-Help/files/GUID-C728DFAC-1B45-4879-A004-45A8DC991954.htm): contextual dependencies and recalled positions.
- [Physical properties](https://help.autodesk.com/cloudhelp/2026/ENU/Inventor-Help/files/GUID-877564AC-78CE-4216-9913-003A2DE8D698.htm) and [Part analysis](https://help.autodesk.com/cloudhelp/2026/ENU/Inventor-Help/files/GUID-C67F8283-DFDF-498F-89BE-91E131CE6171.htm): engineering readouts and quality inspection.

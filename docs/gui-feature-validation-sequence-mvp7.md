# GUI Feature Validation Sequence MVP-7

Status: active. Blocks 95–99 are implemented; Blocks 100–105 are planned. Block 100 parameters,
bodies, and foundational Part workflows are the current next technical step. STEP Import follows in
Blocks 106–112.

This document is the canonical numbered implementation sequence for a simple desktop UI that makes
every feature implemented through Block 94 reachable and testable. The interaction model follows
familiar conventions from established parametric CAD systems such as SolidWorks and Inventor:

```text
application menu + tabbed command bar
left model/assembly browser
central 3D viewport
property/task editor
bottom status and diagnostic area
```

The goal is familiarity, not a pixel-identical clone. Product names, icons, artwork, proprietary
workflows, and exact layouts of other CAD systems are not copied.

## MVP cut

This phase is a validation UI over the existing headless contracts. It provides enough interaction
to create, inspect, edit, solve, analyze, save, load, and export representative models. It is not a
production-complete CAD desktop.

Included:

- new/open/save project and part workflows, dirty state, recompute, undo/redo, and diagnostics;
- shaded/wireframe display, CAD camera controls, semantic selection, highlighting, and fit/view
  commands;
- model and assembly browser, property/task editing, visibility, suppression, and selection sync;
- all Sketch, reference, Part, surface, Assembly, motion, analysis, and STEP-export capabilities
  implemented through Block 94;
- deterministic sample documents and automated GUI acceptance coverage.

Explicitly deferred:

- STEP import, which starts only after GUI acceptance in Block 106;
- production styling, custom themes, workspace customization, macros, add-ins, and command search;
- manufacturing, drawings, simulation, rendering, collaboration, and cloud workflows;
- exact parity with any commercial CAD product;
- new modeling semantics introduced only to make a widget possible.

## Non-negotiable architecture rules

1. `blcad_core` remains independent of Qt and OpenCASCADE UI types.
2. `blcad_geometry` remains the only BLCAD layer that translates Core intent into OCCT geometry.
3. Widgets do not own persistent CAD authority. They edit public Core/application commands and show
   derived Geometry results.
4. A GUI document session owns transient selection, preview, command state, view state, dirty state,
   and undo history. These are not silently added to the project file format.
5. Persistent selections use existing semantic references. OCCT subshape handles, traversal indices,
   and memory identity never become model identity.
6. Preview is disposable. Apply validates and recomputes a candidate transaction before committing;
   Cancel leaves the document unchanged.
7. Failed recompute, solve, analysis, or export keeps the last valid visible result and presents the
   authoritative diagnostic without partially committing intent.
8. Every feature command exposed by the GUI has an equivalent headless Core/Geometry path. The UI
   must not become a second solver, recompute engine, expression evaluator, or transform authority.
9. Long Geometry work must not freeze the event loop indefinitely. The first implementation may use
   a simple cancellable task boundary, but result publication remains atomic on the GUI thread.
10. GUI tests run with Qt's offscreen platform wherever possible; kernel-dependent acceptance remains
    separately tagged and may require `BLCAD_BUILD_GEOMETRY=ON`.

## Frozen desktop layout

```text
+------------------------------------------------------------------+
| Menu | File | Undo/Redo | Recompute | workspace tabs             |
+----------------------+-------------------------------------------+
| Model/Assembly tree  |                                           |
|                      |              3D viewport                   |
|                      |                                           |
|----------------------|                                           |
| Properties / Tasks   |                                           |
+----------------------+-------------------------------------------+
| status | selection hint | units | recompute/solve diagnostics     |
+------------------------------------------------------------------+
```

The left browser and task editor may be dockable Qt widgets. Commands are grouped into contextual
tabs such as `Sketch`, `Part`, `Surface`, `Assembly`, `Inspect`, and `Exchange`. A command uses one
consistent lifecycle:

```text
inactive -> collecting semantic selections -> editing parameters -> preview -> apply | cancel
```

## Frozen phase order

```text
95  Qt application shell, GUI document session, and command architecture
96  document lifecycle, transactions, recompute, and diagnostic workflow
97  OCCT viewport, navigation, display, and semantic picking
98  model/assembly browser, property editor, and selection synchronization
99  datum, workplane, 2D Sketch, reference, and repair workflows
100 parameters, bodies, and foundational Part-feature workflows
101 patterns, finishing, shell, draft, and body-operation workflows
102 3D paths, Sweep, Loft, and Surface workflows
103 Assembly authoring, relationships, joints, motion, and hierarchy workflows
104 analysis and STEP-export workflows
105 integrated GUI feature-coverage acceptance
106–112 STEP Import MVP-8
```

Do not collapse Blocks 95–105 into one large GUI change. The shell, viewport, editing surfaces, and
feature workbenches have different ownership and test boundaries.

## Block 95 — Qt application shell, session, and command architecture — Implemented

Primary boundary: optional desktop application over the existing libraries.

The first Qt 6 executable is implemented behind `BLCAD_BUILD_GUI=ON`. The application layer
contains:

```text
GuiDocumentSession
GuiCommandRegistry
GuiCommandContext
GuiSelectionModel
GuiTaskState
```

Freeze the build matrix: Core-only and Core+Geometry builds remain unchanged; the desktop requires
both `BLCAD_BUILD_GUI=ON` and `BLCAD_BUILD_GEOMETRY=ON`, with an explicit configure-time diagnostic
for an unsupported combination. GUI tests are registered only when their required layers exist.

Implemented targets and boundaries:

```text
blcad_gui / BLCAD::gui  optional static GUI/application library
blcad_app               executable target, output name: blcad
blcad_gui_tests         offscreen Catch2/Qt tests
```

`blcad_gui` owns no persistent Part or Project representation. `GuiDocumentSession` carries the
active document kind/display name, workspace, derived-result freshness, semantic selection, and task
state. `GuiSelectionModel` rejects empty and duplicate semantic ids. `GuiTaskState` enforces
selection -> parameter editing -> preview -> Apply, while Cancel is valid from any active stage.
Workspace replacement and session close fail while a task is active.

`MainWindow` provides File/Edit/View menus, a Part/Assembly/Inspect/Exchange tab bar, empty
model/assembly browser and property/task docks, a central OCCT viewport, bottom
diagnostics, and status feedback. `GuiCommandRegistry` rejects incomplete/duplicate definitions and
derives command enablement solely from `GuiCommandContext`.

Application startup uses a frameless `StartupSplash` with the embedded user-provided Archimedes
illustration cropped to remove its lateral white border, an italic Times New Roman
`BETTER LINUX CAD` title whose glyph top meets the artwork edge, normal-weight monospace `LOADING`
text, and a minimal single-line white progress indicator. The title overlaps the artwork edge by a
few pixels; the horizontal track is slightly heavier than its vertical progress marker. Tinos and
Liberation Serif are metric Linux fallbacks for the title. The artwork is a Qt resource, so the
splash has no dependency on an external runtime file path. Construction milestones advance the
indicator and a short 700 ms non-blocking animation hands over to `MainWindow`.
`BLCAD_SPLASH_DURATION_MS` allows a 250–60000 ms development override.

Verified with:

```bash
cmake -S . -B build/dev-gui -G Ninja \
  -DBLCAD_BUILD_GEOMETRY=ON -DBLCAD_BUILD_GUI=ON -DBLCAD_BUILD_TESTS=ON
cmake --build build/dev-gui --target blcad_gui_tests blcad_app
ctest --test-dir build/dev-gui -R '^gui\.'
```

Core-only configuration remains valid. `BLCAD_BUILD_GUI=ON` with
`BLCAD_BUILD_GEOMETRY=OFF` fails at configure time with the frozen explicit diagnostic.

Focused tags:

```text
[gui][application-shell]
[gui][command-state]
[gui][startup-splash]
```

## Block 96 — Document lifecycle, transactions, recompute, and diagnostics — Implemented

Primary boundary: safe GUI mutation of existing persistence and recompute authorities.

Implemented new/open/save/save-as for supported `PartDocument` and `Project` JSON, dirty-state
prompts, explicit recompute, last-valid-result behavior, and document transaction history:

```text
validated command -> candidate document -> recompute/solve -> atomic commit -> undo record
```

`GuiDocumentSession` owns exactly one `PartDocument` or `Project`, its file path, canonical JSON
history, saved baseline, recompute status, last valid ShapeCache set, and structured diagnostics.
Part and Project mutations execute against a JSON-cloned candidate. Every candidate Part, or every
owned Project Part, recomputes into fresh caches before atomic publication. Failed validation or
Geometry execution retains the previous document and cache and adds no undo entry.

Undo/redo restores canonical persistent snapshots and deterministically regenerates derived results.
Save updates the dirty baseline only after the existing Core writer succeeds. New/Open/Close reject
replacement of a dirty document unless the caller confirms discard, and active tasks must first be
applied or cancelled. The Qt File/Edit/View actions expose these operations, dirty window titles,
discard prompts, errors, and diagnostic object ids. File-dialog paths remain UI state and do not enter
portable model identity.

Verified by the focused tags below with Part save/open/dirty/undo/redo, Project open and transaction
recompute, failure atomicity, last-valid cache retention, action enablement, and offscreen shell tests.

Focused tags:

```text
[gui][document-session]
[gui][document-transaction]
[gui][diagnostics]
```

## Block 97 — OCCT viewport, navigation, display, and semantic picking — Implemented

Primary boundary: transient visualization and selection of authoritative Geometry results.

`OcctViewport` now embeds an OCCT `V3d_View`/`AIS_InteractiveContext` in the Qt central widget on
the Linux xcb platform. The application selects xcb when a display is available and no explicit Qt
platform was requested; offscreen tests retain a deterministic logical presentation path. The
implemented viewer provides:

- shaded, shaded-with-edges, and wireframe display;
- orbit, pan, zoom, fit-all, standard orthographic views, and perspective/orthographic toggle;
- display of visible Part Solid/Surface Bodies, datum planes/axes, explicit construction
  planes/lines, planar line/arc/spline entities, converted 3D Sketch products, PathCurve segments,
  and posed visible-active Assembly leaf occurrences;
- hover preselection, selection highlighting and clear-selection behavior;
- a typed selection-filter boundary for body, face, edge, vertex, datum, sketch entity, component,
  and assembly target presentations;
- a picking bridge that converts every displayed AIS owner back to its stable BLCAD semantic id.

`ViewportSceneBuilder` is the Geometry-owned conversion boundary. `OcctShapeView` exposes native
shape data read-only for transient presentation without moving OCCT ownership into Core. Duplicate,
empty, or shape-less presentation entries are rejected before publication; a valid recompute
revision replaces the complete presentation set in one viewer update, while a failed transaction
keeps the previous revision. View actions expose shaded, shaded-with-edges, wireframe, fit-all,
seven standard views, and perspective/orthographic projection. Middle-drag pans, right-drag orbits,
the wheel zooms, and left-click performs semantic picking. AIS/TopoDS owner identity is never
serialized.

Verified offscreen through logical scene publication, navigation/display state, selection filters,
stable semantic owner mapping, rejection atomicity, and Part datum/sketch scene generation. The
native xcb initialization path is also launch-smoke-tested against a local display.

Focused tags:

```text
[gui][viewport]
[gui][navigation]
[gui][semantic-picking]
```

## Block 98 — Browser, property editor, and selection synchronization

Status: implemented. Detailed contract: `docs/gui-browser-property-selection-mvp7.md`.

Primary boundary: inspect and edit the persistent document graph without duplicating it.

Populate a deterministic browser for parameters, datums, sketches, features, bodies, assemblies,
components, subassemblies, constraints, joints, and analysis entries. Provide:

- tree-to-viewport and viewport-to-tree selection synchronization;
- visibility, suppression, grounding, active-body, and rename actions where existing contracts allow;
- a generic property/task editor for identifiers, typed quantities, expressions, enums, booleans,
  semantic references, and ordered inputs;
- validation messages beside invalid fields and Preview/Apply/Cancel behavior;
- dependency/consumer and stale/failed status inspection.

Generated ids and read-only derived fields remain visibly read-only. Reordering is offered only for
collections whose Core contract defines order as editable intent.

Implemented with a transient `GuiDocumentBrowser` projection rather than widget-owned model state.
The typed table exposes current Core-authorized parameter/expression, Body visibility, and assembly
occurrence state edits through Preview/Apply/Cancel and undoable document transactions. Rename and
active-body remain visibly read-only because current Core contracts do not define those mutations.
Stable semantic ids synchronize tree, session selection, and viewport in both directions.

Focused tags:

```text
[gui][model-browser]
[gui][property-editor]
[gui][selection-sync]
```

## Block 99 — Datum, workplane, 2D Sketch, reference, and repair workflows

Status: implemented. Detailed contract: `docs/gui-sketch-workbench-mvp7.md`.

Primary boundary: interactive access to all implemented planar sketch and reference features.

Expose datum planes/axes, derived workplanes, sketch creation/editing, construction geometry, line,
arc/circle, spline, trim/extend, projected/reference geometry, supported constraints and dimensions,
profile/region inspection, and the implemented diagnostics/repair helpers. Sketch editing uses a
normal-to-plane camera and maps pointer input into plane coordinates; numeric entry remains
available for deterministic testing.

Selection prompts must state the required semantic capability. Under/over-constrained and invalid
profiles are shown using Core diagnostics. Automatic repair remains an explicit previewed action and
is one undoable transaction.

The implemented GUI application workbench creates datum and construction geometry, resolves
derived workplanes, authors every currently supported planar Sketch entity/reference/constraint/
dimension/profile family, and replaces a Sketch atomically through `PartDocument::update_sketch`.
The normal-to-plane viewport and reusable plane-coordinate mapper support deterministic spatial and
numeric input. The model browser projects the authored Sketch structure and Core diagnostics;
region inspection, repair preview on a copy, and explicit repair application reuse existing Core
authorities. Applying a repair produces exactly one GUI history entry.

Focused tags:

```text
[gui][datum-workplane]
[gui][sketch-workbench]
[gui][sketch-repair]
```

## Block 100 — Parameters, bodies, and foundational Part workflows

Primary boundary: the first complete solid-feature authoring loop.

Expose parameter/expression editing, Body creation/activation, profile selection, Extrude and
Extruded Cut including implemented extent/direction modes, Revolve/RevolveCut, the bolt-circle/hole
workflow, feature suppression, and body-scoped recompute/inspection. New/Join/Cut/Intersect body
operation choices must match current feature intent exactly.

Each command provides selection prompts, typed unit fields, semantic input summaries, preview, and
atomic Apply. A deterministic tutorial model must be creatable entirely through the GUI and survive
save/load/recompute.

Focused tags:

```text
[gui][parameters]
[gui][part-foundation]
[gui][extrude-revolve]
```

## Block 101 — Patterns, finishing, shell, draft, and body operations

Primary boundary: UI coverage of the remaining conventional mechanical Part features.

Expose linear/circular Pattern, Mirror, Fillet, Chamfer, Shell, Draft, Body Boolean, and associative
Body Transform workflows. Ordered target lists, seed features/bodies, axes/planes, edge/face
references, radii, angles, thicknesses, directions, and operation modes use the generic semantic
selection and property infrastructure from Blocks 97–98.

Invalid or lost topology must remain visible and fail closed. Preview must not publish a partial body
cache. Representative multi-body creation, boolean composition, patterning, finishing, transform,
save/load, recompute, and STEP export are covered by a GUI-driven scenario.

Focused tags:

```text
[gui][part-pattern]
[gui][part-finishing]
[gui][body-operation]
```

## Block 102 — 3D paths, Sweep, Loft, and Surface workflows

Primary boundary: UI coverage of spatial and surface modeling.

Expose model-space 3D Sketch point/line/polyline/arc/spline/helix/guide entities, connected
PathCurves, Sweep/SweepCut/SweepSurface, path-following Extrude/Cut, two- and multi-section
Loft/LoftCut/LoftSurface, guide/continuity controls, Boundary/Fill Surface, Trim/Extend Surface,
Stitch/Knit/Sew, and closed-shell-to-solid conversion.

The task panel must make ordered profiles, paths, guides, continuity, orientation, twist, surface
inputs, and output Body kinds explicit. Both planar 2D-sketch paths and model-space 3D paths are
selectable wherever the underlying feature contract permits them. The viewer distinguishes open
curves, surfaces, shells, and solids.

Focused tags:

```text
[gui][path-workbench]
[gui][sweep-loft]
[gui][surface-workbench]
```

## Block 103 — Assembly authoring, relationships, joints, motion, and hierarchy

Primary boundary: interactive use of the complete Assembly MVP.

Expose Project/Assembly creation, insertion of shared Parts and nested subassemblies, component
placement, visibility/suppression/grounding, assembly parameters/bindings, and deterministic browser
hierarchy. Provide relationship commands for supported Mate/Distance/Concentric/Insert/Angle and
generic compatible targets, plus Revolute, Prismatic, Cylindrical, Planar, and passive Spherical
joints.

The command task shows first/second target capability, orientation choices, limits, initial/driven
coordinates, and compatibility diagnostics before Apply. Recompute/solve updates posed occurrences
atomically. The UI exposes solve status, degrees of freedom, residual diagnostics, motion controls,
and nested occurrence paths without creating a second transform authority.

Focused tags:

```text
[gui][assembly-authoring]
[gui][assembly-relationships]
[gui][assembly-motion]
```

## Block 104 — Analysis and STEP-export workflows

Primary boundary: GUI access to existing inspection and exchange consumers.

Expose solve/DOF inspection, interference, clearance, contact-related queries, sampled revolute
motion sweeps, and available headless assembly analyses. Results appear as deterministic lists linked
to highlighted occurrences/targets and retain authoritative diagnostic values.

Expose current Part multi-body and Assembly flattened/structured STEP export modes, output selection,
freshness/recompute preflight, progress, cancellation before commit, and completion diagnostics.
This block does not add STEP import or change product/body naming contracts.

Focused tags:

```text
[gui][analysis]
[gui][step-export]
```

## Block 105 — Integrated GUI feature-coverage acceptance

Primary boundary: proof that the desktop can exercise every implemented feature family through
Block 94 without bypassing public authorities.

Add checked-in deterministic sample documents and a feature-coverage manifest mapping every public
Core/Geometry feature family to:

```text
create or edit command
browser/property inspection
viewport presentation and semantic selection
save/load/recompute proof where persistent
focused GUI or headless-equivalence test
```

Acceptance must prove at least these complete workflows:

1. create and constrain a planar sketch, build a parameterized multi-body mechanical Part, apply
   conventional and spatial/surface features, save/load/recompute, and export STEP;
2. create an Assembly containing repeated Parts and a nested subassembly, author relationships and
   supported joints, solve and drive motion, inspect DOF/interference/clearance, save/load, and export
   flattened plus structured STEP;
3. undo/redo representative sketch, Part, and Assembly edits while derived results remain consistent;
4. reject invalid selection capability, failed feature preview, failed solve, and failed export
   preflight without partial model mutation;
5. compare GUI-produced document JSON and derived results with equivalent headless workflows.

The manifest fails CI when an implemented feature family has no declared GUI disposition. A feature
may be marked read-only only if its existing Core contract is also read-only; it may not silently be
omitted. Manual smoke instructions cover camera and pointer interaction that cannot be proven
reliably offscreen.

Focused tags:

```text
[integration][gui-feature-coverage]
[integration][gui-headless-equivalence]
```

After Block 105, the GUI Validation MVP is complete and Block 106 begins STEP Import MVP-8.

## Coverage ownership summary

| Existing capability through Block 94 | GUI owner |
|---|---:|
| Document JSON, recompute, diagnostics, transactions | 96 |
| Geometry display, camera, semantic selection | 97 |
| Parameters, feature/body tree, properties, suppression/visibility | 98, 100 |
| Datum/workplanes, planar Sketch, constraints, references, repair | 99 |
| Extrude/Cut, Revolve/Cut, holes, basic multi-body | 100 |
| Pattern, Mirror, Fillet, Chamfer, Shell, Draft, Boolean, Transform | 101 |
| 3D Sketch, PathCurve, Sweep, path Extrude, Loft, Surface features | 102 |
| Project, Assembly hierarchy, constraints, joints, solve, motion | 103 |
| DOF, interference, clearance, motion analysis, STEP export | 104 |
| Cross-feature and headless-equivalence proof | 105 |

Any feature discovered during Block-105 inventory but absent from this table is added to its natural
workbench block before acceptance is declared complete; it is not deferred merely because the UI is
described as simple.

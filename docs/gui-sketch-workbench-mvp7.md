# GUI Datum, Workplane, and Planar Sketch Workbench MVP-7

Status: implemented in Block 99.

## Authority boundary

`GuiSketchWorkbench` exposes the implemented planar Core contracts through `GuiDocumentSession`
transactions. Widgets never mutate `Sketch` storage directly. `PartDocument::update_sketch` validates
a replacement with the same rules as initial insertion, preserves authored sketch order and
downstream dependency edges, marks affected graph nodes stale, and publishes the replacement only
after the complete candidate is valid.

The workbench covers datum planes/axes, construction geometry, derived workplanes, planar sketch
creation/editing, lines, arcs, splines, circle/rectangle and explicit closed profiles, trim/extend,
projected and reference-generated geometry, implemented constraints/dimensions, ordered profiles,
profile/region inspection, Core diagnostics, suggestions, preview, and repair.

Each operation has a capability-specific selection prompt. Numeric sketch-plane coordinates remain
available through the workbench and initial `Sketch` menu. The plane mapper converts between model
and local 2D coordinates. Entering sketch edit mode switches the OCCT camera to orthographic
projection along the resolved workplane normal and uses the workplane Y axis as up.

The Block-98 browser now expands planar sketches into entities, projected references, constraints,
dimensions, trim/extend records, profiles, and Core diagnostic entries. Stable entity presentation
ids continue to synchronize browser and viewport selection.

## Repair policy

Diagnostics and suggestions are read-only until the user explicitly previews a repair. Preview
applies the existing executor to a sketch copy. Apply repeats it inside one document transaction,
so recompute, dirty state, undo, and redo remain atomic. Unsupported suggestions cannot be silently
applied.

## Verification

```bash
./build/dev-gui/blcad_core_tests "[core][sketch-update]"
./build/dev-gui/blcad_gui_tests "[gui][datum-workplane]"
./build/dev-gui/blcad_gui_tests "[gui][sketch-workbench]"
./build/dev-gui/blcad_gui_tests "[gui][sketch-repair]"
```

Coverage includes atomic replacement/order preservation, construction and projected geometry,
planar entities, constraints, coordinate mapping, semantic prompts, diagnostics/repair preview,
undoable repair, and normal-to-plane camera activation.

## Block-110 direct-manipulation integration

The MVP-7 Sketch workbench remains a validation/transaction client over historical Sketch intent. Block
110 does not move live drag authority into `GuiSketchWorkbench`. `GuiSketchDragController` consumes
Block-108 topology and Block-109 solving; successful release enters the same
`GuiDocumentSession::commit_part_transaction(...)` authority used by validation workbenches.

The final solved topology is materialized and re-migrated exactly before `PartDocument::update_sketch`.
Live handle positions, pointer samples, temporary drag constraints, and preview Sketches remain
transient. This preserves MVP-7 atomic recompute/undo semantics while adding direct manipulation.

## Replacement-shell construction authoring — 2026-10-23

`ShellWindow` explicitly owns `ShellConstructionTools`. **3D-Modell → Konstruktion** provides
**Konstruktionsgeometrie** and **Arbeitsebene**, opening one nonmodal task with nine creation modes:

- Explicit XYZ point, line through a point with a direction, and datum axis with an origin/direction.
- Construction line through two existing construction points, and datum axis from a construction line.
- Explicit plane with an origin and perpendicular X/Y directions, a positive offset from an existing
  datum/construction/derived plane, and a plane through three existing construction points.
- Derived workplane on a supported generated semantic face. The picker offers only faces resolved by
  `WorkplaneResolver`; the rectangular additive-extrude topology contract remains authoritative.

Coordinates use millimeters; direction vectors are normalized before Core frame validation. Zero
directions, nonperpendicular plane axes, coincident/collinear defining points, empty names and missing
references disable Apply and clear the preview. Source roles use explicit task selectors with stable
document identities. This slice does not add viewport pick collection or geometry manipulators.

Preview adds intent to a disposable Part copy and resolves it through the existing construction and
workplane authorities. The viewport displays only the new candidate shape. Neither preview nor
Cancel changes intent, history, dirty state or save data. Offset planes may bind an existing Length
parameter (including an expression) or provision a new positive Length parameter. A new parameter and
its plane enter one `commit_part_transaction`; one Undo removes both. Existing parameter bindings
are never overwritten. Id allocation avoids existing objects and the generated parameter id.

Apply consumes pending numeric editor text, then revalidates inputs and revision before the
transaction. Escape, dock close,
document replacement, Undo/Redo, stale Geometry or a session presentation revision change discard the
task and preview. Saving during preview writes committed intent and expires the task. Project and
active-Sketch contexts reject construction authoring, and a construction task blocks sketch entry.

`ViewportSceneBuilder` now presents resolved construction points, relation-driven lines/planes,
construction-line-derived axes and derived workplanes. Browser and viewport identities agree.
Selecting a resolvable construction or derived plane in either surface enables **2D-Skizze erstellen**;
camera mapping, bounded face checks and downstream feature recompute use the existing workplane
contracts. These changes add no persistence fields or raw OCCT identities.

In-place object editing/rebinding, generated-edge/vertex construction authoring, parallel-through-point
creation and broader native reference collection remain open Block-136 work. Offset parameter value
and formula edits remain available through the existing parameter property inspector. Reference
diagnostics/repair retain their separate acceptance gap.

Proof: `[block136-construction]` in `tests/gui/gui_shell_construction_tools_tests.cpp` and
`[geometry][construction-presentation]` in `tests/geometry/viewport_scene_tests.cpp`. Commands and
native/offscreen accounting live in `docs/development-setup.md` and
`docs/gui-shell-acceptance-mvp9r.md`.

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

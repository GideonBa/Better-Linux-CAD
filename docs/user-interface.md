# User Interface Architecture

Status: MVP-7 accepted and Interactive Sketcher MVP-8 in progress. Blocks 95–105 provide the
optional Qt shell, document transactions, OCCT viewport, deterministic model/assembly browser, typed
property editor, and semantic selection synchronization. Block 99 adds datum, derived-workplane,
planar Sketch, inspection, and explicit repair workflows. Block 100 adds parameters, Bodies, and
foundational solid features. Block 101 adds patterns, finishing, shell, draft, Body
operations/transforms, and fresh-result STEP export. Block 102 adds model-space paths, Sweep, Loft,
Surface workflows, and distinct path/Surface/Solid viewport products. Block 103 adds complete
Assembly authoring, hierarchy, relationships, joints, DOF/solve diagnostics, and motion. Block 104
adds deterministic analysis and freshness-gated Part, flattened Assembly, and structured Assembly
STEP export. Block 105 completes integrated acceptance. Block 106 adds the real contextual Sketch
workspace and command lifecycle; Block 107 adds device-independent plane interaction, hit testing,
box selection, grid, snapping, and inference preview. Block 108 is the current next technical step.
Blocks 106–121 form the productive Interactive Sketcher; Blocks 122–131 add interactive Part,
Surface, and Assembly modeling (`docs/interactive-modeling-sequence-mvp9.md`); STEP Import begins
with Block 132.

The UI is deliberately not built like FreeCAD. The goal is a modern, consistent, reduced interface with a clear separation between model, parameters, features, and assembly. Crucially, the UI only operates the core; it must not contain CAD logic (last of the core principles in `docs/architecture-summary.md`).

## Main areas

| UI area | Purpose |
|---------|---------|
| 3D viewport | display and interaction with parts, sketches, faces, edges, axes, assemblies. |
| Feature tree | chronological/logical feature list of a part. |
| Assembly tree | parts, instances, and constraints of an assembly. |
| Parameter window | table of parameters: name, value, unit, formula, scope, usage sites. |
| Property panel | edit the currently selected object/feature. |
| Command palette | quick access to Sketch, Extrude, Cut, Fillet, Hole, Pattern, Assembly Constraint. |
| Engineering panel | access technical assistants (bolt, bearing, shaft). |

## Contextual Sketch workspace

Block 106 promotes planar Sketch editing to `GuiWorkspace::Sketch`, a transient contextual Part mode.
The global tab row remains `Part | Assembly | Inspect | Exchange`; entering Sketch does not create a
fifth persistent project mode or any Part feature.

The Sketch command surface is frozen as:

```text
Create | Constrain | Dimension | Modify | Project
```

The Sketch browser contract is:

```text
Entities | Constraints | Dimensions | Diagnostics
```

The command bar renders the group strip and numeric HUD. The existing left model browser remains the
semantic selection authority and the existing Properties / Tasks dock remains the contextual task
surface. The status bar exposes tool hint, raw plane cursor coordinates, snap/inference, remaining
DOF, and solve state. Block 107 now supplies cursor, hover, and snap/inference products; Block 109
remains responsible for general DOF and solve status.

`Enter Sketch` captures workspace, semantic selection, full transient camera state, and the viewport
selection-filter mask. It resolves the Sketch workplane, enters normal orthographic view, restricts
picking to SketchEntity/Edge/Vertex, and uses a crosshair. `Finish Sketch` rejects an active command
or diagnostic errors, then restores the previous workspace, selection, Eye/Target/Up/Scale/projection
camera state, and selection filter without inventing an Extrude or other feature.

The command lifecycle is specified in `docs/gui-interactive-sketch-workspace-mvp8.md`. For example,
the first HUD client accepts `line.web, 0, 0, 40, 0`, validates the line on a disposable Sketch copy,
enters Preview, and commits through the existing undoable Block-99 workbench mutation path.

## Sketch canvas interaction

Block 107 gives the planar Sketch canvas one mapping between Qt device-independent pixels, OCCT view
rays, active-plane coordinates, and model points. Hit and snap tolerances are expressed in DIP, so a
6-DIP curve tolerance remains a 6-DIP interaction tolerance after zoom or on a high-DPI display.
The native OCCT bridge converts through the current device-pixel ratio; offscreen tests use a
deterministic orthographic mapper.

Hover and repeated click use the frozen priority:

```text
Point -> Curve -> Dimension -> Glyph
```

Repeated clicks at the same position cycle the stable overlapping-hit stack. Empty-space drag uses
direction to select the conventional CAD mode:

```text
left -> right   Window: fully contained curves
right -> left   Crossing: contained or crossed curves
```

A transparent viewport overlay draws the highest-priority hover product, active snap marker, and the
rubber-band selection rectangle. These are view products only.

The contextual Sketch menu now exposes `Show grid` and `Snap to grid`. The default grid is 10 mm
with every fifth line marked major. Grid display is generated only for the visible active-plane
range and is bounded for very large views. Display subsampling never changes the configured snap
spacing.

Implemented snap/inference families are:

```text
origin | axis | endpoint | midpoint | center | quadrant
intersection | nearest | grid
horizontal | vertical | X alignment | Y alignment
```

Snap choice is deterministic: eligible candidates inside the DIP tolerance are ordered by
model-space distance, then screen-space distance, then stable family priority and candidate id. The
status bar displays the raw plane coordinate separately from the chosen snap/inference text. For
example, the pointer may show `Cursor: 19.84, 10.11 mm` while `Snap: endpoint: line.web:end` and the
overlay marker is drawn at the endpoint.

The canonical Block-107 contract is `docs/gui-sketch-plane-interaction-mvp8.md`.

## Parameter window

Long-term important but not the first priority: the internal parameter model must work first, then the UI is laid over it as an editor. Target shape:

```text
Assembly Parameters
Name                     Value  Unit  Formula          Used by
bolt_circle_radius       50     mm    -                3 parts
bolt_count               8      -     -                3 parts
bolt_clearance_diameter  6.6    mm    from bolt_size   2 parts
plate_thickness          8      mm    -                BasePlate
gasket_thickness         2      mm    -                Gasket
```

The "Used by" column comes from parameter usage tracking (`docs/parameter-model.md`).

## Feature dialogs

Feature-creation dialogs (hole wizard, fillet/chamfer, pattern/mirror, assembly constraints) share a consistent structure and are specified in their own documents:

- hole wizard dialog: `docs/hole-wizard.md`.
- fillet/chamfer dialog: `docs/fillet-chamfer-features.md`.
- pattern/mirror dialog: `docs/pattern-and-mirror-features.md`.
- assembly-constraint dialog with reference selection, suggested constraints, preview, remaining-DOF display, and validation: `docs/assembly-system.md`.

Constraint suggestions are deterministic from geometry types (plane+plane → Mate/Flush/Distance/Angle; cylinder+cylinder → Concentric/Tangent; screw+hole → Insert) and need no AI.

## Recompute feedback

After a change the UI updates the viewport, feature tree, and parameter window from the recompute result. Errors are surfaced, not swallowed. See the recompute process in `docs/architecture-summary.md` and `docs/recompute-plan-mvp1-data-model.md`.

## Proposed implementation sequence

1. Keep everything headless until the core, features, and serialization are stable.
2. Add a read-only viewport that renders the recomputed final shape.
3. Add a feature/assembly tree bound to transient projections of persistent Core models. Implemented
   in Block 98.
4. Add typed parameter/property editing that drives validated transactions and recompute. The
   generic Block-98 layer, Block-99 planar Sketch workbench, and Block-100 foundational Part
   workbench, Block-101 conventional Part operations, and Block-102 spatial/Surface workflows are
   implemented together with Block-103 Assembly editing and Block-104 analysis/export surfaces;
   Block 105 supplies integrated coverage acceptance. Implemented and accepted.
5. Keep selection synchronized through stable semantic ids across tree, session, and viewport.
6. Add feature-creation dialogs one module at a time.
7. Add the assembly tree and constraint dialog once the assembly system exists.
8. Replace the validation-level Sketch surface with the solver-backed direct-manipulation workbench
   sequenced in `docs/interactive-sketcher-sequence-mvp8.md`. Blocks 106–107 have established the
   contextual workspace plus plane interaction/hit/snap authority; Block 108 adds shared persistent
   point/entity topology and editable Core commands next.

## Out of scope for the first versions

- any UI before the headless model/recompute path is reliable.
- putting CAD logic in the UI layer.
- a full command/undo-redo system before the command layer is designed (see the command system layer in `docs/architecture-summary.md`).

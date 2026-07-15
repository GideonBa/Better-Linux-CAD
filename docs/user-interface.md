# User Interface Architecture

Status: MVP-7 accepted and Interactive Sketcher MVP-8 in progress. Blocks 95–105 provide the optional
Qt shell, document transactions, OCCT viewport, deterministic browser/property surfaces, and semantic
selection synchronization. Blocks 106–108 now establish the contextual Sketch workspace, transient
plane interaction, and persistent shared planar point/entity topology. Block 109 is the current next
technical step and owns the general planar constraint solver. Blocks 122–131 add interactive Part,
Surface, and Assembly modeling; STEP Import begins with Block 132.

The UI is deliberately not built like FreeCAD. The goal is a modern, consistent, reduced interface
with clear separation between model, parameters, features, Sketch topology, and Assembly. The UI only
operates Core/Geometry authorities; it must not own CAD or solver logic.

## Main areas

| UI area | Purpose |
|---------|---------|
| 3D viewport | display and interaction with Parts, Sketches, faces, edges, axes, and Assemblies |
| Feature tree | chronological/logical feature list of a Part |
| Assembly tree | Parts, occurrences, relationships, and joints |
| Parameter window | parameter name, value, unit, formula, scope, and usage sites |
| Property panel | edit the selected object/feature through validated commands |
| Command palette | quick access to Sketch, Part, Surface, Assembly, Inspect, and Exchange commands |
| Engineering panel | later technical assistants for mechanical engineering domains |

## Contextual Sketch workspace

Block 106 promotes planar Sketch editing to `GuiWorkspace::Sketch`, a transient contextual Part mode.
The global workspace row remains `Part | Assembly | Inspect | Exchange`; entering Sketch does not
create a persistent project mode or Part feature.

Sketch command surface:

```text
Create | Constrain | Dimension | Modify | Project
```

Sketch browser contract:

```text
Entities | Constraints | Dimensions | Diagnostics
```

The command bar renders the group strip and numeric HUD. The existing model browser remains semantic
selection authority and Properties/Tasks remains the contextual task surface. The status bar exposes
tool hint, raw plane cursor coordinates, snap/inference, remaining DOF, and solve state.

Producer staging is explicit:

```text
Block 107  cursor / hover / hit / box selection / grid / snap / inference
Block 108  persistent shared SketchPointId / SketchTopology identity
Block 109  general solve state and remaining DOF
Block 110  solver-backed drag candidates
```

Therefore real Cursor/Snap values are already available while `DOF: —` and
`Solve: Not evaluated` remain correct until Block 109. The UI does not infer DOF from topology
connectivity alone.

`Enter Sketch` captures workspace, semantic selection, full transient camera state, and viewport
selection filter. It resolves the workplane, enters normal orthographic view, restricts picking to
SketchEntity/Edge/Vertex, and uses a crosshair. `Finish Sketch` rejects active commands or diagnostic
errors and restores workspace, selection, Eye/Target/Up/Scale/projection, and selection filtering.
It never invents an Extrude or another downstream feature.

The command lifecycle is specified in `docs/gui-interactive-sketch-workspace-mvp8.md`. For example,
the first HUD client accepts `line.web, 0, 0, 40, 0`, validates on a disposable historical Sketch
copy, enters Preview, and commits through the existing undoable Block-99 workbench path. Block 108
does not silently retrofit that seed command into topology-native creation; Blocks 110–111 integrate
solver-backed direct manipulation and creation in sequence.

## Sketch canvas interaction

Block 107 gives the planar Sketch canvas one mapping between Qt device-independent pixels, OCCT view
rays, active-plane coordinates, and model points. Hit/snap tolerances are specified in DIP, so a
6-DIP curve tolerance remains a 6-DIP interaction tolerance across zoom and high-DPI displays. The
native bridge converts through current device-pixel ratio; offscreen tests use a deterministic
orthographic mapper.

Hover and repeated-click priority is:

```text
Point -> Curve -> Dimension -> Glyph
```

Repeated clicks at the same location cycle the stable overlapping-hit stack. Empty-space drag uses:

```text
left -> right   Window: fully contained curves
right -> left   Crossing: contained or crossed curves
```

A transparent viewport overlay draws grid, highest-priority hover product, active snap marker, and
rubber-band selection. The shell binder also draws transient dashed horizontal/vertical/alignment
inference guides. These are view products only.

The Sketch menu exposes `Show grid` and `Snap to grid`. Default grid spacing is 10 mm with every fifth
line major. Grid display is generated only for the visible active-plane range and bounded for large
views; display subsampling never changes snap spacing.

Implemented snap/inference families are:

```text
origin | axis | endpoint | midpoint | center | quadrant
intersection | nearest | grid
horizontal | vertical | X alignment | Y alignment
```

Eligible candidates are ordered by model-space distance, screen-space distance, stable family
priority, then candidate id. The status bar displays raw plane coordinates separately from chosen
snap/inference text. For example:

```text
Cursor: 19.84, 10.11 mm
Snap: endpoint: line.web:end
```

The canonical Block-107 contract is `docs/gui-sketch-plane-interaction-mvp8.md`.

## Shared point/entity topology and UI identity

Block 108 introduces Core `SketchPointId` and `SketchTopology`. This is the persistent identity model
for future solver and direct-manipulation workflows.

Historical line/arc/spline records embed `Point2` values. Legacy migration converts them to topology
entities referencing shared point ids. For example:

```text
historical
  line.a.end   = (20, 0)
  line.b.start = (20, 0)

topology
  entity/line.a.points[1] = entity/line.a/end
  entity/line.b.points[0] = entity/line.a/end
```

One moved topology point therefore remains one shared junction.

This does not mean a Block-107 point hit or snap landmark is automatically persistent point identity.
The current interaction scene remains transient and still projects the historical Sketch compatibility
representation. Later accepted drag/creation commands resolve or create `SketchPointId` through
explicit Core topology operations.

Block-108 Core commands support Add, Move, Replace, and Remove with dependency-safe validation.
Reference topology is read-only. Every successful command provides exact complete before/after
topology snapshots for deterministic undo/redo.

Canonical topology persistence is `blcad.sketch_topology.mvp8`. Historical PartDocument JSON remains
load-compatible and migrates explicitly; entering Sketch does not rewrite it. A topology edit may be
projected back through `PartDocument::update_sketch(...)` only when materialize -> re-migrate preserves
the exact topology without identity/flag loss.

Canonical contract: `docs/sketch-shared-topology-mvp8.md`.

## Parameter window

The parameter model remains Core authority; UI is an editor over it. Target shape:

```text
Assembly Parameters
Name                     Value  Unit  Formula          Used by
bolt_circle_radius       50     mm    -                3 parts
bolt_count               8      -     -                3 parts
bolt_clearance_diameter  6.6    mm    from bolt_size   2 parts
plate_thickness          8      mm    -                BasePlate
gasket_thickness         2      mm    -                Gasket
```

The `Used by` column comes from parameter usage tracking.

## Feature and Assembly command surfaces

Feature-creation workflows share a consistent task/preview/commit structure and consume existing Core
and Geometry authorities. Current validation workbenches expose Part, Surface, Assembly, analysis,
and exchange families. Interactive selection-first manipulators are sequenced separately in
`docs/interactive-modeling-sequence-mvp9.md`.

Constraint suggestions remain deterministic from compatible geometry/capability types; they do not
require AI.

## Recompute feedback

After a change, the UI updates viewport, browser, properties, and diagnostics from authoritative
recompute/solve results. Errors are surfaced and the last valid result is retained; failed candidates
do not publish partial state.

## Implementation sequence

1. Keep model/recompute/serialization authorities headless-first. Implemented.
2. Add read-only OCCT viewport and semantic presentation. Implemented.
3. Add browser/property projections and synchronized semantic selection. Implemented.
4. Add typed command/task transactions and feature-validation workbenches. Implemented through Block
   105 and accepted.
5. Add a contextual Sketch workspace and device-independent canvas interaction. Implemented in Blocks
   106–107.
6. Replace equal-coordinate connectivity with stable shared planar Core topology. Implemented in
   Block 108.
7. Add deterministic general planar solving over that topology. Block 109 is next.
8. Add solver-backed drag, creation, constraints, dimensions, modify/project tools, regions, and
   Interactive Sketch3D through Block 121.
9. Add selection-first Part/Surface/Assembly modeling through Blocks 122–131.
10. Add STEP Reference/EditableBody import through Blocks 132–138.

## Current boundary

Block 109 is the next UI-enabling Core step. It must produce deterministic solve state and remaining
DOF over Block-108 topology before Block 110 allows mouse dragging to commit constrained geometry.
No widget may implement substitute constraint mathematics.

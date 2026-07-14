# Interactive Sketcher Sequence MVP-8

Status: planned. Blocks 106–121 precede STEP Import. Block 106 is the current next technical step.

This phase turns the Block-99 validation surface into a productive, Inventor-familiar Sketch
workbench. The goal is the same interaction quality—direct manipulation, visible constraints,
in-canvas dimensions, predictable commands, and immediate feedback—not a pixel-identical copy of
Inventor or its protected assets.

## Product outcome

A user can enter a planar Sketch, create common mechanical profiles without typing JSON, drag
unconstrained points with the mouse, watch constrained geometry solve continuously, add and edit
dimensions in the canvas, repair conflicts, and leave the Sketch with one undoable document result.
All persistent intent remains usable by the existing headless Core/Geometry path.

The phase also gives currently implemented Sketch3D points and curves direct manipulation. It does
not claim full commercial-CAD parity; unsupported tools stay visibly unavailable.

## Why this needs a separate phase

The current planar model embeds endpoints directly in line, arc, and spline records and the current
diagnostic layer does not solve a general constraint system. A GUI-only drag implementation would
therefore tear connected profiles apart or create geometry that headless recompute cannot reproduce.
Blocks 108–110 first establish shared point identity, a deterministic solver, and transactional
candidate solving. The viewport then becomes a client of those authorities.

## Frozen Sketch workspace

```text
+--------------------------------------------------------------------------------+
| Finish Sketch | Undo/Redo | Create | Constrain | Dimension | Modify | Project  |
+----------------------+---------------------------------------------------------+
| Sketch browser       | constraint glyphs, dimensions, origin and axes          |
| - entities           |                                                         |
| - constraints        |                  planar Sketch canvas                    |
| - dimensions         |                                                         |
| - diagnostics        |                                                         |
|----------------------|                                                         |
| contextual task      |                                                         |
| panel / numeric HUD  |                                                         |
+----------------------+---------------------------------------------------------+
| tool hint | cursor coordinates | snap/inference | remaining DOF | solve status  |
+--------------------------------------------------------------------------------+
```

Mouse conventions are frozen before implementation:

- left click selects or places; drag moves a handle or box-selects from empty space;
- `Esc` backs out one command stage, then exits the command; it never silently commits;
- right click opens a small context menu and repeats the last compatible command where sensible;
- `Ctrl` toggles selection, `Shift` adds selection, `Delete` removes validated selected intent;
- numeric typing while creating or dragging opens a transient heads-up field;
- middle drag pans, wheel zooms, and the existing orbit gesture remains disabled while normal to a
  planar Sketch unless the user deliberately leaves the normal view;
- double click edits a dimension or entity property; `Enter` accepts the active numeric value.

## Architecture rules

1. Screen coordinates, hover state, snap candidates, rubber-band previews, and glyph placement are
   transient GUI state and never persistent model identity.
2. Shared Sketch points, entities, constraints, dimensions, and construction/reference state are
   persistent Core intent with stable ids and canonical JSON.
3. The solver is a headless deterministic Core service. Widgets do not contain constraint math.
4. Dragging solves a disposable candidate at interactive frequency. Mouse release commits exactly
   one document transaction; `Esc` or solver failure restores the pre-drag state.
5. A constrained point may move other geometry through the solver. Fixed or fully constrained
   geometry refuses incompatible drag instead of deleting constraints.
6. Automatic constraints are previewed as inference glyphs and become persistent only when the
   placement is accepted. Holding the frozen suppression modifier disables inference for that pick.
7. Projected geometry remains associative and read-only. Conversion to ordinary geometry is an
   explicit command.
8. Every creation and edit command has keyboard-accessible Apply/Cancel behavior and an equivalent
   headless command path.
9. Solver failure, non-convergence, ambiguity, or over-constraint publishes diagnostics but no
   partial document mutation.
10. Existing Block-99 JSON loads deterministically through an explicit schema migration; save/load
    never depends on GUI session state.

## Frozen phase order

```text
106 Sketch workspace, interaction state, command HUD, and usability contract
107 plane mapping, hit testing, box selection, grid, snapping, and inference preview
108 shared planar point/entity topology, mutation commands, JSON migration, and undo semantics
109 deterministic planar constraint solver, DOF accounting, conflicts, and diagnostics
110 solver-backed mouse dragging, handles, live preview, and atomic commit
111 point, line, polyline, rectangle, polygon, and construction-geometry creation
112 circle, arc, ellipse, and slot creation/editing
113 spline fit/control-point editing, continuity handles, and Sketch text
114 manual and automatic geometric constraints with glyph interaction
115 driving/reference dimensions, in-canvas editing, and parameter/expression binding
116 trim, extend, split, corner fillet, and corner chamfer tools
117 offset, project/include, construction axes, and associative reference workflows
118 move, rotate, scale, copy, mirror, and rectangular/circular Sketch patterns
119 region recognition, profile selection, diagnostics, repair, and Finish Sketch workflow
120 interactive Sketch3D creation and direct point/curve manipulation
121 integrated usability, persistence, solver, performance, and GUI/headless acceptance
122–128 STEP Import MVP-9
```

## Block 106 — Sketch workspace and interaction contract

Promote Sketch to a real contextual workspace. Add `Enter Sketch`/`Finish Sketch`, command groups,
entity/constraint/dimension browser sections, a contextual task panel, numeric heads-up input,
crosshair/cursor feedback, status-bar DOF and solve state, command repeat, keyboard focus rules, and
consistent selection filters. Freeze the state machine:

```text
idle -> hover -> collecting picks -> numeric input -> preview -> commit | cancel
idle -> selected handle -> drag candidate -> commit | cancel
```

Entering Sketch saves the previous camera and selection, resolves the workplane, selects normal
view, and isolates/dims surrounding bodies according to transient view settings. Finish Sketch
validates the active profile state and returns to the previous workspace without inventing a feature.

Focused tags: `[gui][sketch-workspace]`, `[gui][sketch-command-lifecycle]`.

## Block 107 — Plane interaction, hit testing, snapping, and inference

Create one device-independent mapping between pixels, view rays, workplane coordinates, and model
space. Add zoom-stable hit tolerances, point/curve/dimension/glyph priority, cycling through stacked
hits, window/crossing selection, hover highlighting, grid display, grid snap, origin/axis snap,
endpoint/midpoint/center/quadrant/intersection/nearest snaps, and horizontal/vertical/alignment
inference previews. Snap selection is deterministic in model space with screen-space tie breaking.

Focused tags: `[gui][sketch-hit-test]`, `[gui][sketch-snap]`, `[gui][sketch-box-selection]`.

## Block 108 — Shared point topology and editable Core commands

Replace disconnected embedded endpoints with stable shared planar point identity. Lines, arcs,
splines, and profiles reference points; coincident endpoints may share one point without relying on
equal floating-point coordinates. Add headless add/move/replace/remove commands, dependency-safe
deletion, construction/reference flags, canonical ordering, schema migration from existing Sketch
JSON, and exact undo/redo. Migration must preserve existing generated geometry and semantic ids
where possible and report every unavoidable identity change.

Focused tags: `[core][sketch-topology]`, `[core][sketch-edit-command]`,
`[core][sketch-json-migration]`.

## Block 109 — General planar constraint solver

Add a deterministic headless nonlinear solver over planar point and curve parameters. Freeze
variable ordering, scale normalization, tolerances, iteration limits, convergence reporting, and
stable conflict attribution. Initially solve coincident, fixed, horizontal, vertical, parallel,
perpendicular, collinear, equal, tangent, concentric, midpoint, symmetric, point-on-object, and
horizontal/vertical/aligned/radial/diameter/angular distance constraints. Report:

```text
fully constrained | under constrained (remaining DOF)
redundant | conflicting | non-convergent | invalid reference
```

No solver result is persisted as an opaque cache; canonical intent plus deterministic solve remains
the authority.

Focused tags: `[core][sketch-solver]`, `[core][sketch-dof]`,
`[core][sketch-conflict-diagnostics]`.

## Block 110 — Mouse-draggable points and live constrained solving

Expose endpoint, midpoint, center, radius, arc, spline, and dimension handles. Pressing a handle
captures the pre-drag snapshot; every move maps to a target point and asks the Core solver for a
candidate. The canvas draws solved candidate geometry without changing the document. Release
commits one transaction; `Esc`, lost capture, or failed solve restores the snapshot. Throttle solves
without dropping the final pointer position and keep typical mechanical sketches responsive.

Dragging selected geometry supports rigid candidate translation when constraints permit. Fixed and
fully constrained geometry explains why it cannot move. Tests cover connected chains, dimensions,
tangent arcs, conflict rejection, undo/redo, and identical results for scripted headless targets.

Focused tags: `[gui][sketch-drag]`, `[integration][sketch-live-solve]`.

## Block 111 — Basic creation tools

Implement persistent point, two-point line, continuous polyline, center/corner rectangle,
three-point rectangle, parallelogram, regular polygon, centerline, and construction geometry tools.
Multi-click commands show rubber-band previews, accept typed length/angle values, reuse snapped
points, and preview automatic constraints. Composite tools expand into ordinary points, lines, and
constraints rather than opaque GUI-only primitives.

Focused tags: `[gui][sketch-create-basic]`, `[integration][sketch-basic-profile]`.

## Block 112 — Circles, arcs, ellipses, and slots

Extend Core/Geometry intent where required for center-radius/diameter, two-point, three-point, and
tangent circles; center/start/end, three-point, and tangent arcs; ellipses/elliptical arcs; and
center-to-center, overall-length, and three-point slots. Add
center, radius, endpoint, quadrant, major/minor-axis, and slot handles. Full circles are real curve
entities rather than nearly closed arcs. Degenerate radii and ambiguous collinear picks fail closed.

Focused tags: `[core][sketch-conics]`, `[geometry][sketch-conics]`,
`[gui][sketch-create-conics]`.

## Block 113 — Spline editing and Sketch text

Support fit-point and control-point cubic splines, insertion/removal of points, draggable control
polygon, endpoint tangent/curvature handles, open/closed state, and deterministic conversion between
supported representations where mathematically defined. Continuity constraints use the solver;
screen-space handles remain transient. Self-intersection and insufficient-control-point diagnostics
are explicit.

Add parameter/expression-backed single- and multiline Sketch text with alignment, baseline/path
placement, height, spacing, and explicit font-family fallback diagnostics. Text remains semantic
Sketch intent; Geometry converts glyph outlines into selectable profile loops. Missing fonts and
invalid/self-intersecting outlines fail visibly, and the JSON never embeds platform font handles.

Focused tags: `[core][sketch-spline-edit]`, `[geometry][sketch-spline-edit]`,
`[gui][sketch-spline]`, `[core][sketch-text]`, `[geometry][sketch-text]`.

## Block 114 — Constraint authoring and inference UX

Expose every Block-109 constraint through selection-driven commands and familiar glyphs. The task
panel explains required selection capabilities before accepting picks. Creation tools preview likely
coincident, horizontal, vertical, tangent, parallel, perpendicular, midpoint, and concentric
constraints. Users can select, inspect, suppress where Core intent permits, delete, and locate
constraints from glyphs or browser entries. Conflicting additions preview the conflict set and do
not mutate the document.

Focused tags: `[gui][sketch-constraints]`, `[integration][sketch-auto-constraint]`.

## Block 115 — Dimensions and expressions

Add horizontal, vertical, aligned, point-to-point, length, radius, diameter, angle, and arc-length
dimensions plus driven/reference mode. Placement includes extension lines, arrows, text collision
avoidance, zoom-stable hit testing, and draggable annotation positions as transient presentation
state. Double click edits value, unit, parameter name, or expression; driving values bind to the
existing parameter authority and solve atomically. Switching driven/driving mode must diagnose
over-constraint before commit.

Focused tags: `[core][sketch-dimensions]`, `[gui][sketch-dimensions]`,
`[integration][sketch-expression-edit]`.

## Block 116 — Trim, extend, split, fillet, and chamfer

Implement hover-side preview and deterministic intersection choice for trim/extend/split. Add
two-entity Sketch fillet and chamfer with typed radius/distance, optional trim, preserved usable
constraints, and stable replacement mapping. Commands operate on candidate topology and commit one
undo step. Ambiguous intersections or reference geometry never mutate partially.

Focused tags: `[core][sketch-modify]`, `[geometry][sketch-modify]`,
`[gui][sketch-trim-extend]`.

## Block 117 — Offset, projection, and construction references

Add single/chain/loop offset with side preview, distance parameter, corner policy, and robust
self-intersection diagnostics. Expose associative projection of supported model edges, vertices,
axes, and silhouette boundaries, plus explicit break-link conversion. Projected geometry is colored
and locked as read-only; lost/ambiguous semantic references enter the existing repair workflow.

Focused tags: `[core][sketch-offset-project]`, `[geometry][sketch-offset-project]`,
`[gui][sketch-project]`.

## Block 118 — Transform, mirror, and Sketch patterns

Add move, rotate, scale, copy, mirror, rectangular pattern, and circular pattern for selected Sketch
geometry. Preview includes a manipulator, base point, count, spacing/angle, and constraint-copy
policy. Associative patterns require explicit Core intent; exploded copies are ordinary entities.
The command reports which external constraints cannot be copied instead of silently discarding
them.

Focused tags: `[core][sketch-transform-pattern]`, `[gui][sketch-transform-pattern]`.

## Block 119 — Regions, profiles, diagnostics, and Finish Sketch

Recognize bounded regions continuously from solved visible non-construction geometry. Shade closed
regions lightly, support click-to-select outer/inner loops, expose open endpoints and self-crossings,
and build stable profile intent without requiring manual entity ordering. Finish Sketch shows
remaining DOF, conflicts, unused geometry, lost references, and downstream profile effects; warnings
may be accepted explicitly, errors cannot. Repair suggestions preview before one atomic commit.

Focused tags: `[geometry][sketch-regions]`, `[gui][sketch-profile-selection]`,
`[integration][sketch-finish]`.

## Block 120 — Interactive Sketch3D

Provide orthogonal triad/plane locks, model-space point and line placement, direct point dragging,
typed XYZ/distance/angle input, arc/spline/helix handles, guide-curve roles, and projection of planar
Sketch points. A constrained 3D solver beyond currently supported relationships is not implied:
unsupported constraint commands remain disabled and direct manipulation respects parameter-driven
coordinates. Camera navigation and point manipulation have unambiguous modifier separation.

Focused tags: `[gui][sketch-3d-edit]`, `[integration][sketch-3d-direct-manipulation]`.

## Block 121 — Interactive Sketcher acceptance

Add deterministic tutorial documents and a coverage manifest for every planned tool. Acceptance
creates and edits a dimensioned plate, bracket, slotted profile, tangent/filleted profile, projected
reference profile, spline profile, and 3D Sweep path exclusively through public commands. It proves
mouse/script equivalence, save/load/recompute, undo/redo, conflict atomicity, reference repair,
keyboard-only Apply/Cancel, high-DPI coordinate mapping, and no stale preview publication.

Performance budgets are measured, not guessed: hover and drag remain interactive on representative
sketches; solver and region-recognition timings are reported by entity/constraint count. Manual
smoke checks cover cursor feel and visual legibility that offscreen tests cannot establish.

Focused tags: `[integration][interactive-sketcher]`, `[integration][sketch-gui-headless]`,
`[performance][sketch-interaction]`.

After Block 121, STEP Import MVP-9 begins with Block 122.

## Explicit deferrals

- automatic tracing from raster images;
- unconstrained freehand drawing and handwriting recognition;
- proprietary Inventor command names, icons, shortcuts, or exact layout cloning;
- a full variational 3D Sketch constraint solver;
- collaboration cursors or multi-user concurrent editing;
- manufacturing-specific Sketch annotations.

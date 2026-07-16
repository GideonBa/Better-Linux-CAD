# Basic Sketch Creation Tools MVP-8

Status: implemented in Block 111.

This document is the canonical GUI/Core integration contract for the Block-111 basic creation
tools. Multi-click creation composes the Block-106 command lifecycle, Block-107 snap/inference,
and the existing Block-99 Sketch/construction transaction authorities. No new solver, drag, or
topology authority is introduced.

## Authority boundary

```text
Create action (Sketch menu / command group)
  -> GuiSketchWorkspace command lifecycle
     Idle -> CollectingPicks -> NumericInput -> Preview -> commit | cancel
  -> snapped Block-107 plane picks
  -> GuiSketchCreateController (headless pick/preview/expansion authority)
  -> expansion into ordinary persistent intent
     LineSegment[] (+ ClosedProfile for composite tools)
     ConstructionPoint / ConstructionLine
  -> one GuiSketchWorkbench transaction per completed tool
  -> one undoable document history entry
```

`GuiSketchCreateController` owns picks, rubber-band preview products, numeric-entry
interpretation, and deterministic expansion. `install_sketch_create_binder(...)` connects the
controller to the Qt actions, viewport pointer events, numeric HUD keys, and Esc semantics.

## Implemented tools

```text
Point                  1 pick   -> ConstructionPoint (PartDocument construction geometry)
Line                   2 picks  -> one LineSegment
Polyline               2+ picks -> chained LineSegments; Enter or double-click finishes
Corner rectangle       2 picks  -> 4 LineSegments + ClosedProfile
Center rectangle       2 picks  -> 4 LineSegments + ClosedProfile
Three-point rectangle  3 picks  -> 4 LineSegments + ClosedProfile
Parallelogram          3 picks  -> 4 LineSegments + ClosedProfile
Regular polygon        2 picks  -> N LineSegments + ClosedProfile (N typed before the first pick)
Centerline             2 picks  -> ConstructionLine (PartDocument construction geometry)
```

Composite tools expand into ordinary shared-corner lines plus one explicit `ClosedProfile`; no
opaque GUI-only primitive is persisted. The ordered closed profile is exactly what gives the
corner points shared `SketchPointId` identity under Block-108 migration and keeps the result a
Block-119/extrude-consumable profile region.

Qt action object names are stable:

```text
blcad.action.sketch_create_point
blcad.action.sketch_create_line
blcad.action.sketch_create_polyline
blcad.action.sketch_create_corner_rectangle
blcad.action.sketch_create_center_rectangle
blcad.action.sketch_create_three_point_rectangle
blcad.action.sketch_create_parallelogram
blcad.action.sketch_create_polygon
blcad.action.sketch_create_centerline
```

They are enabled exactly when a Part Sketch workspace is active and idle.

## Picks, snapping, and identity

Every accepted pick is the Block-107 snapped plane coordinate; the controller records the snap
family for automatic-constraint preview. Snap candidate ids never become persistent identity:
lines are created with their own endpoint coordinates, and only explicit ordered profile
connectivity produces shared Block-108 point identity. Persistent shared junctions for open
polylines or against existing entities require canonical topology JSON persistence and remain
outside Block 111; the fail-closed Block-108 PartDocument bridge is not weakened.

Polyline junctions are therefore numerically coincident but distinct point identities, exactly as
`docs/sketch-shared-topology-mvp8.md` documents for unrelated coincident endpoints; Block 114 may
later relate them with explicit Coincident intent.

## Rubber-band preview and inference

Every pointer move updates a zero-delay-coalesced preview. Open tools preview the pick chain plus
the current hover point; composite tools preview the complete closed corner ring once enough
anchors exist. The preview is published through the new transient
`OcctViewport::set_sketch_preview_polyline(...)` overlay and is never model state.

Automatic constraints are **previewed only**: pick/hover snap families map to constraint names
(`Endpoint/Origin/Intersection -> coincident`, `Midpoint -> midpoint`, `Center/Quadrant ->
concentric`, `Axis -> point_on_object`, `Horizontal/VerticalInference -> horizontal/vertical`,
`AlignmentX/Y -> aligned`). Persistent automatic constraint authoring remains Block 114.

## Numeric heads-up input

Typing while a tool collects picks accumulates a numeric entry (mirrored into the Block-106
workspace numeric input). `Enter` applies it:

```text
before the first pick   "x;y"   absolute plane point
after a pick            "a;b"   relative offset from the last pick
after a pick            "L<A"   positive length at explicit angle in degrees
after a pick            "L"     positive length toward the current hover direction
regular polygon, before the first pick: a plain integer sets the side count (3..64)
```

Invalid text, non-positive lengths, and a missing hover direction fail closed without adding a
pick. `Backspace` edits the entry; `Esc` clears it before popping picks.

## Command lifecycle and cancellation

The binder walks the frozen Block-106 machine: tool start enters `CollectingPicks`; completion
walks `NumericInput -> Preview -> commit_command` and publishes exactly one workbench transaction:

```text
curves/profiles      GuiSketchWorkbench::edit_sketch("Create sketch <tool>", ...)
point / centerline   create_construction_point / create_construction_line
```

`Esc` backs out one stage at a time: numeric entry first, then the newest pick, then the command.
A failed expansion or transaction publishes the authoritative diagnostic, restores the scene, and
leaves the document unchanged. Fixed-pick tools commit on their final pick; Polyline finishes via
`Enter` or double-click.

After a successful commit the binder republishes the interaction scene and triggers the internal
`blcad.action.sketch_topology_changed` action so the Block-110 drag binder rebuilds its handles
against the new topology.

## Degenerate input policy

Fail-closed without partial mutation: zero-extent rectangles, collinear parallelogram picks,
zero-radius polygons, coincident consecutive polyline picks, coincident centerline picks, and
polygon side counts outside 3..64.

## Persistence

Block 111 adds no JSON field or schema. Created lines, closed profiles, construction points, and
construction lines persist through the existing Sketch and PartDocument JSON authorities. Picks,
snap kinds, rubber bands, numeric buffers, and constraint previews are transient GUI state.

## Focused proof

```text
[gui][sketch-create-basic]
[integration][sketch-basic-profile]
```

The proof covers every tool's expansion (ids, corner geometry, closed profile), degenerate
fail-closed inputs, absolute/relative/polar numeric picks, polygon side-count entry, rubber-band
and constraint previews, pick backout, one-transaction commits with exact undo/redo labels,
construction point/centerline products, and an offscreen Qt mouse path through the installed
binder that creates a corner-rectangle profile, verifies region inspection, JSON roundtrip, and
undo/redo.

## Extension and next boundary

Block 112 reuses this command lifecycle for circles, arcs, ellipses, elliptical arcs, and slots. Its
persistent expansion, exact full-circle parameter contract, transient conic preview, and failure rules
are canonical in `docs/gui-sketch-conic-slot-creation-mvp8.md`.

Block 113 spline editing and Sketch text, Block 114 accepted constraint authoring, Block 115
dimension authoring, Block 116 trim/extend/split/fillet/chamfer, Block 117 offset/projection,
Block 118 transforms/patterns, Block 119 regions/Finish Sketch, and Block 120 Interactive Sketch3D
are implemented in their canonical contracts. Block 121 integrated acceptance is next.

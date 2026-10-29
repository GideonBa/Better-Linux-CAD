# Basic Sketch Creation Tools MVP-8

Status: implemented in Block 111.

> **GUI Shell Reset MVP-9R:** Block 133 deleted the Qt shell this document names — `MainWindow` and
> all seven self-installing binders — and replaced it with `ShellWindow` + `ShellRibbon`
> (`src/gui/shell/`, contract: `docs/gui-shell-reset-sequence-mvp9r.md`). The headless authorities and
> the Core/Geometry contracts below are unchanged and are consumed by the new shell; read every
> "`MainWindow`" and "binder" below as the shell's Qt presentation layer, whose current form is
> Block 135/136 work.

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
Point                  1 pick   -> SketchPoint in the active Sketch
Line                   2 picks  -> one LineSegment
Polyline               2+ picks -> chained LineSegments; Enter or double-click finishes
Corner rectangle       2 picks  -> 4 LineSegments + ClosedProfile
Center rectangle       2 picks  -> 4 LineSegments + ClosedProfile
Three-point rectangle  3 picks  -> 4 LineSegments + ClosedProfile
Parallelogram          3 picks  -> 4 LineSegments + ClosedProfile
Regular polygon        2 picks  -> N LineSegments + ClosedProfile (N typed before the first pick)
Centerline             2 picks  -> LineSegment(role=centerline), selectable in its Sketch
ConstructionLine       2 picks  -> LineSegment(role=construction), selectable in its Sketch
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

Every accepted pick is the Block-107 snapped plane coordinate. For supported line creation, an
endpoint pick also carries transient `(SketchId, SketchPointId)` evidence resolved from the exact
snap candidate's curve and endpoint role. Coincident coordinates never choose the target. At an
overlap, the selected candidate determines which existing point is related; other coincident
locations stay independent. The point must still exist in the same Sketch at the accepted position
when expansion/commit validates it. Missing, moved or foreign evidence fails without partial intent.

The issue #104 follow-up makes accepted canonical endpoint snaps persistent for Line, Polyline,
Construction Line and Centerline. Consecutive Polyline segments receive internal Coincident intent.
The shell's repeating Line carries the newly committed endpoint identity into its next first pick,
so continuation, closure and restarted commands use the same path. Geometry and inferred relations
share one `commit_part_sketch_intent_transaction`; relations have `Automatic` provenance, with no
second post-commit constraint transaction. Duplicate/self pairs and zero-length segments are rejected
or omitted as appropriate. Undo/Redo and Save/Open include the relations with the geometry.

Snap candidate strings, pixels and OCCT indices are never stored as model identity. Created
endpoints retain their own canonical ids; Coincident intent couples them without welding unrelated
points or inventing canonical topology JSON persistence. Ordered profiles and fitted families keep
their existing shared-point authority. Other snap families remain geometric inference. In particular,
virtual parametric-profile corners and unresolved projected/reference endpoints without a canonical
point target do not acquire invented detached point identities. The fail-closed compatibility bridge
and reference-target limits in `docs/sketch-shared-topology-mvp8.md` remain authoritative.

The viewport press phase snapshots its resolved snap before deferred creation runs. A later hover
cannot change that accepted point. Cancellation, tool replacement or pick backout invalidate pending
presses; automatic Line/Point rearming retains the logical input session so bursts stay ordered across
successive segments. Source evidence, session tokens and queued input are transient GUI state.

## Rubber-band preview and inference

Every pointer move updates a zero-delay-coalesced preview. Open tools preview the pick chain plus
the current hover point; composite tools preview the complete closed corner ring once enough
anchors exist. The preview is published through the new transient
`OcctViewport::set_sketch_preview_polyline(...)` overlay and is never model state.

Pick/hover snap families map to possible constraint-preview names
(`Endpoint/Origin/Intersection -> coincident`, `Midpoint -> midpoint`, `Center/Quadrant ->
concentric`, `Axis -> point_on_object`, `Horizontal/VerticalInference -> horizontal/vertical`,
`AlignmentX/Y -> aligned`). These hints remain transient. Supported canonical endpoint snaps and
internal line junctions persist through the automatic Coincident path above; other inference kinds
do not imply that a persistent relation was created. Compound-shape automatic recipes retain their
existing constraint-authoring authority.

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
walks `NumericInput -> Preview -> commit_command` and publishes exactly one session transaction:

```text
curves/profiles/points/helper lines
  GuiDocumentSession::commit_part_sketch_intent_transaction("Create sketch <tool>", ...)
  insert primitives, add automatic recipes, solve/validate, publish together
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

The shell follow-up adds an optional `line_segments[].role`: `geometry` (default when missing),
`construction` (dashed), or `centerline` (dash-dot). Helper lines remain selectable Sketch entities
for axes, constraints and transformations, survive solver materialization and geometry edits, and
do not contribute to inferred profile regions. Existing Part construction datums retain their
separate authority. See `docs/file-format.md` for persistence. Picks,
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

Block 112 reuses this command lifecycle for circles, arcs, ellipses and slots. Its
persistent expansion, exact full-circle parameter contract, transient conic preview, and failure rules
are canonical in `docs/gui-sketch-conic-slot-creation-mvp8.md`.

Block 113 spline editing and Sketch text, Block 114 accepted constraint authoring, Block 115
dimension authoring, Block 116 trim/extend/split/fillet/chamfer, Block 117 offset/projection,
Block 118 transforms/patterns, Block 119 regions/Finish Sketch, and Block 120 Interactive Sketch3D
are implemented in their canonical contracts. Block 121 integrated acceptance is implemented and
accepted in `docs/interactive-sketcher-mvp8-acceptance.md`; Block 122 is next.

## Endpoint persistence follow-up — issue #104, 2026-10-29

Four added cases and two strengthened existing pointer-loop cases reproduce and guard the missing
closure/restart relation and the former separate history entry. Coverage includes an exact loop,
a 4-DIP endpoint snap, twelve segments, an overlapping but unrelated endpoint, Save/Open and drag,
polyline internal/external links, missing/moved/foreign target rejection, zero-length rejection,
press-time snapshot versus later hover, cancellation/restart and a three-press queued burst.
The transaction tests compare canonical document/catalog JSON and assert no partial lines or intent.

Final focused checks pass **6 / 313** offscreen and **6 / 331** natively at both 100% and 150%,
without native skips. Native fixtures keep every input inside the visible viewport, using ordinary
wheel zoom when necessary. Captures `endpoint-overlap-after-drag` and
`endpoint-twelve-segment-after-drag` were inspected at both scales: the selected relation follows
its source, the unrelated overlapping endpoint stays independent and the twelve-edge region remains
closed after reopen/drag. Broader validation and source fingerprints are recorded in
`docs/development-setup.md`. This fix advances no numbered implementation block.

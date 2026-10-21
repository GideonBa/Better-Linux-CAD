# GUI Sketch Transform, Mirror, and Pattern MVP-8

Status: Core authority implemented in Block 118; interactive shell commands completed in the October 2026 Sketch follow-up.

## Command workflow

In an active planar Sketch, select ordinary lines, arcs, spline spans, or an explicit closed profile.
The **Transformieren** ribbon group provides **Verschieben**, **Drehen**, **Skalieren**, **Kopieren**,
**Spiegeln**, **Rechteckmuster**, and **Kreismuster**. Profile selections expand to their member curves.

Commands can start with an existing compatible selection or an empty selection. Each opens a
nonmodal editor: **Elemente auswählen** collects multiple curve picks or a selection rectangle,
then **Mit der Maus ziehen** updates the same request as the numeric fields. Rotate, scale and
circular pattern also expose a center pick. Pointer motion and gesture start use the same snap
result; mouse release stops the gesture without committing. Numeric refinement remains available.

Each command retains its numeric editor. Translation and centers use Sketch-plane millimeters;
rotation uses degrees; scale is a non-zero signed uniform factor. Mirror takes two distinct axis
points or a picked sketch line and creates a reflected copy. The axis and element pick controls
are separate, so changing the element selection does not replace the chosen axis. Pattern counts
include the seed. Rectangular patterns accept independent X/Y step vectors for both directions.
Circular patterns accept a center, count, and signed total angle up to one revolution.

Edits show separate dashed preview curves in the viewport without connecting unrelated instances.
The committed geometry, dirty state, and undo history remain unchanged until **Übernehmen**.
Unsupported or mixed selections disable the operation and reject it as a whole; selecting a line
alongside a parameter-backed circle must never transform only the line. Free points and projected
references are likewise excluded from ordinary curve transforms. Offset enablement distinguishes
line chains and single circular arcs from unsupported spline/mixed curved offsets.
Cancel/Escape clears the preview and restores selection and drag handles. An invalid candidate
clears the previous preview, explains the error, and disables Apply. Apply re-evaluates the current
document and publishes exactly one atomic transaction, including downstream feature recompute.
Undo/Redo and Save/Open use the existing document and intent persistence.

## Authority boundary

```text
ShellSketchTools: selection + nonmodal pointer/numeric editor + viewport preview
  -> GuiSketchOperationController: disposable candidate and intent validation
  -> SketchTransformPatternService: exact Core geometry
  -> GuiDocumentSession: one transaction and downstream recompute
```

The dialog-free `preview_operation`, `apply_operation`, and `cancel_operation_preview` surface uses
the same implementation as the ribbon. Explicit curve ids address `LineSegment`, `ArcSegment`, and
polynomial/rational cubic `SplineSegment`. Rational weights survive every transform. Free-standing
Sketch points and unrelated profiles/references survive candidate cloning.

## Identity, profiles, and constraints

Move, rotate, and scale preserve curve and profile ids. Moving only part of a closed contour must
still produce a valid closed profile. Copy and mirror create deterministic `<source>.copy.N` and
`<source>.mirror.N` ids. A fully selected closed, arc-closed, or composite profile is also copied with
its mapped outer/inner contours; a partial selection creates ordinary curves only.

The controller checks existing embedded and catalog constraints and driving dimensions against the
requested geometry. Fixed targets retain their pre-command coordinates. A transform that violates
these relationships is rejected atomically; the solver does not silently change the requested result.

Copy, mirror, and patterns keep source intent intact and create independent instances. Both embedded
and catalog constraint/dimension ids that are not duplicated are shown in the editor and completion
message. This report is also available as `uncopied_references` to headless callers.

## Symmetric constraint workflow

**Symmetrisch** opens the same axis/element selection workflow, with an ordinary line, dashed
construction line or dash-dot centerline as its axis. It constrains existing curves rather than
creating copies. Matching curve types are paired by reflected proximity; endpoint and spline control
point correspondence may reverse orientation. One selected line constrains its own endpoints.
Shared point pairs are deduplicated. All authored constraints and driving dimensions participate in
one solve, with the axis fixed during authoring. Conflicting candidates publish no geometry or
partial constraint catalog; a successful Apply stores the entire batch in one undo step.

## Pattern semantics

Rectangular patterns emit every cell except seed `(0,0)` as ordinary geometry with `.rect.N` ids.
Circular patterns use `.circular.N` ids. For a full ±360° revolution, angular spacing is
`total_angle / count`, so the final instance does not overlap the seed. For a partial sweep, spacing
is `total_angle / (count - 1)`, including the last endpoint. A one-instance pattern contains only its
seed. Duplicate selections, zero counts, nonfinite values, multi-revolution sweeps, and more than
10,000 source-curve instances are rejected before generating geometry.

The shell explicitly creates independently editable copies (`SketchPatternMode::Exploded`). The Core
also offers `Associative`, returning a `SketchPatternIntent` with source ids, counts, steps and center.
That returned intent has no persistent shell catalog or downstream regeneration authority yet;
associative pattern editing is a remaining extension.

## Focused proof

```bash
./build/dev-gui/blcad_core_tests "[core][sketch-transform-pattern]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-transform-pattern]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[sketch-mouse-operations],[sketch-delivery-review]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[sketch-mouse-operations],[sketch-delivery-review]"
```

The suite exercises every transform and pattern through the shell command surface, actual ribbon
editor Apply/Cancel, preview/history isolation, constraint and dimension conflict rollback,
profile-selection expansion, rational conic preservation, copied profile boundaries, full-circle
spacing in both directions, count/selection guards, exact Undo/Redo, and Save/Open.

## Native UI regression

`[gui][sketch-ui-review]` opens a real shell window, selects geometry through viewport pointer events,
scrolls the ribbon to the actual tool button, and clicks through every numeric operation. It checks
preview/cancel isolation, actual Escape handling, one-step main-window Undo/Redo, and removal of
transient preview paths. The ribbon scrolls horizontally at ordinary desktop widths; labels retain
their requested width. Optional `BLCAD_UI_CAPTURE_DIR` captures only the test application's widgets.

The same cases run offscreen and with `QT_QPA_PLATFORM=xcb`, including 150% and 200% scaling.
Offscreen assertions do not substitute for native OpenGL display and visual inspection.

## Remaining boundaries

Projected/reference entities remain read-only transform sources. Parameter-backed full circle and
rectangle profiles, free-standing point transforms, persistent associative pattern editing, and
arbitrary non-uniform scale are not implemented by this command family.

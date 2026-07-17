# GUI Viewport Manipulator Infrastructure MVP-9

Status: implemented in Block 123.

Block 123 provides one reusable transient manipulation layer for the interactive Part, Surface, and
Assembly commands sequenced in Blocks 124–130. The layer maps viewport pointer motion to candidate
parameter values, renders zoom-stable handles, couples those values to a numeric HUD, and deliberately
contains no document mutation or feature-specific preview logic.

## Authority boundary

```text
owning modeling command
  -> stable handle descriptors + current parameter values
  -> GuiViewportManipulatorLayer
     camera/model/screen mapping
     fixed-DIP presentation and hit testing
     pointer drag or numeric override
     transient candidate value
  -> owning modeling command
     disposable Geometry preview
     one validated Apply transaction
```

`GuiViewportManipulatorLayer` does not receive a `GuiDocumentSession`, `PartDocument`, `Project`,
feature record, relationship, joint, or `ShapeCache`. It cannot write persistent intent. A later owning
command consumes a candidate and remains responsible for validation, live preview, freshness, Apply,
Cancel, and Undo/Redo.

## Reusable handle families

The frozen handle families are:

```text
Linear          signed distances and thicknesses
Angular         angles and twist
Radial          radii and diameters
TranslateAxis   X/Y/Z translation-triad axes
RotateAxis      X/Y/Z rotation-triad rings
PatternCount    integer count along a model-space direction
PatternSpacing  non-negative spacing along a model-space direction
```

`translation_triad(...)` and `rotation_triad(...)` produce three stable `.x`, `.y`, and `.z` handles.
Every handle descriptor contains a stable id, model-space origin, axis, plane frame where applicable,
reference value, optional limits, fixed presentation size in DIP, hit tolerance in DIP, and the model
step used to turn PatternCount drag distance into integer count changes.

Handle descriptors and candidates are transient GUI/application data. They are not feature intent and
are never serialized.

## Camera and model-space mapping

`GuiViewportManipulatorMapping` accepts explicit screen-to-view-ray and model-to-screen providers. The
shell uses `GuiViewportCameraBookmark` to derive a deterministic orthographic or perspective mapping:

```text
screen DIP
  -> camera ray
  -> closest point on a model axis
     or ray/handle-plane intersection
  -> signed distance, radius, or angle
```

Linear, translation, spacing, and count handles use closest-line model-space mapping. A view-parallel
axis uses the projected model-axis direction as a deterministic fallback. Radial and angular handles
intersect the pointer ray with the explicit handle plane; they never infer a plane from display
pixels.

Presentation is zoom-stable: arrows and rings have a fixed DIP size independent of model scale. Model
mapping still uses the current camera, so zoom affects model distance per pointer movement without
affecting handle visibility or hit tolerance.

## Deterministic hit testing

The layer derives a presentation primitive for every valid handle. Arrow-like handles use point-to-
segment screen distance. Angular and rotation handles use distance to a fixed-DIP ring.

Eligible hits are ordered by:

```text
1. screen distance in DIP
2. stable handle id
```

The stable-id tie break generalizes the Block-107 screen-space convention without introducing AIS
owner, OCCT traversal, widget address, or insertion-order identity.

## Drag lifecycle and exact release

Starting a drag freezes:

```text
active stable handle id
start pointer
start model-space measurement
reference parameter value
```

Pointer updates emit disposable candidates relative to that frozen start. No value jumps to the
visual arrow endpoint because the initial model measurement is subtracted from every later sample.
Release synchronously processes the exact final pointer sample before returning the candidate, matching
the Block-110 final-sample rule.

Cancel, invalid mapping, invalid final sample, lost window activation, or `Esc` clears the active
handle and candidate. Since no document was modified, no compensating transaction is required.

## Candidate values

`GuiViewportManipulatorCandidate` identifies:

```text
stable handle id
Distance | Angle | Radius | Translation | Rotation | Count | Spacing
Drag | Numeric input source
scalar value
axis-scaled vector for translation/rotation
integer count where applicable
```

Radial and spacing values are non-negative. Pattern count is integral and at least one. Explicit handle
limits are applied before publication.

The candidate is not a command result. For example, dragging an Extrude arrow in Block 124 will only
produce a candidate distance; the Block-124 command must build and validate the disposable Extrude
preview and commit the accepted feature transaction.

## Bidirectional numeric HUD coupling

Dragging formats the current candidate into the shell HUD using the value family unit (`mm`, `deg`, or
unitless count). Typing a valid value updates the same candidate and becomes the authoritative input
source. While a numeric override is active, further pointer movement does not replace the typed value.
The override may be explicitly cleared to resume drag-derived values from the latest pointer sample.

Invalid text, incompatible unit suffixes, and fractional PatternCount values are rejected without
changing the last valid candidate. Numeric input is parsed locale-independently (`.` decimal),
matching the layer's own dot-formatted HUD text; a Qt application resetting the C locale to a
comma-decimal system locale does not change parsing.

## Qt shell boundary

`GuiViewportManipulatorShellBinder` installs:

```text
blcad.viewport_manipulator_overlay
blcad.manipulator_numeric_hud
```

The transparent overlay renders arrows, rings, endpoint markers, active values, and pattern-count
markers. The binder owns mouse capture, exact release, `Esc`/deactivation cancellation, camera refresh,
and HUD synchronization. A revision check updates presentation when later commands replace handles.

The binder may publish a candidate callback, but that callback is still only candidate delivery. The
binder performs no Core/Geometry mutation and no preview transaction.

## Focused proof

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][viewport-manipulators]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][manipulator-numeric-coupling]"
```

The proof covers fixed-DIP presentation, deterministic tie breaking, model-space linear mapping, exact
final release, numeric override precedence, non-mutation of a live Part session, angular/radial
mapping, translate/rotate triad construction, PatternCount quantization, and visible shell ownership.

## Next boundary

Block 124 consumes this infrastructure for interactive Extrude, path Extrude, and Revolve authoring.
Block 123 itself does not create feature previews or commit feature parameters.

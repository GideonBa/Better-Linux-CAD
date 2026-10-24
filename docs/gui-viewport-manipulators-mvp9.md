# GUI Viewport Manipulator Infrastructure MVP-9

Status: implemented in Block 123. `GuiViewportManipulatorLayer` is unchanged, but
`GuiViewportManipulatorShellBinder` — the transparent Qt overlay described here — was deleted with the
Block-133 shell reset (`docs/gui-shell-reset-sequence-mvp9r.md`). Block 135 replaces it with the
shell-owned, notification-driven `ShellFeatureTools` overlay; the old binder specifics below are
historical.

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

The handle families are:

```text
Linear          signed distances and thicknesses
Angular         angles and twist
Radial          radii and diameters
TranslateAxis   X/Y/Z translation-triad axes
RotateAxis      X/Y/Z rotation-triad rings
PatternCount    integer count along a model-space direction
PatternSpacing  non-negative spacing along a model-space direction
UniformScale    dimensionless uniform scale factor along a model-space direction (Block 136)
```

`translation_triad(...)` and `rotation_triad(...)` produce three stable `.x`, `.y`, and `.z` handles.
Every handle descriptor contains a stable id, model-space origin, axis, plane frame where applicable,
reference value, optional limits, fixed presentation size in DIP, hit tolerance in DIP, and the model
step used to turn PatternCount drag distance into integer count changes. UniformScale has an
independent positive `scale_model_step` (25 model millimeters per factor unit by default).

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
Distance | Angle | Radius | Translation | Rotation | Count | Spacing | ScaleFactor
Drag | Numeric input source
scalar value
axis-scaled vector for translation/rotation
integer count where applicable
```

Radial and spacing values are non-negative. Pattern count is integral and at least one. Explicit handle
limits are applied before publication.
ScaleFactor is unitless; the Body-transform controller validates its strictly positive value before
publishing Geometry or committing a record.

The candidate is not a command result. For example, dragging an Extrude arrow in Block 124 will only
produce a candidate distance; the Block-124 command must build and validate the disposable Extrude
preview and commit the accepted feature transaction.

## Bidirectional numeric HUD coupling

Dragging formats the current candidate into the shell HUD using the value family unit (`mm`, `deg`, or
unitless count/scale factor). Typing a valid value updates the same candidate and becomes the authoritative input
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

Block 135 uses those stable names again for the replacement overlay and task-panel numeric editor.
The replacement path remaps on viewport gestures and ribbon camera/projection commands. If `Esc`,
window deactivation, or an invalid mapping ends a live drag, `ShellFeatureTools` restores the
controller value captured at drag start before republishing handles.

The transparent overlay renders arrows, rings, endpoint markers, active values, and pattern-count
markers. The binder owns mouse capture, exact release, `Esc`/deactivation cancellation, camera refresh,
and HUD synchronization. A revision check updates presentation when later commands replace handles.

The binder may publish a candidate callback, but that callback is still only candidate delivery. The
binder performs no Core/Geometry mutation and no preview transaction.

## Focused proof

The `[gui][viewport-manipulators]` and `[gui][manipulator-numeric-coupling]` cases asserted against
the deleted overlay binder and were removed with Block 133; Block 135 restores the replacement path
against `ShellWindow` under `[gui][shell-features]`. The layer's model-space mapping and candidate contract are meanwhile exercised through
the Block 124–128 controller proofs, and its hit performance budget still has a focused case:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[performance][manipulator-hit]"
```

The proof covers fixed-DIP presentation, deterministic tie breaking, model-space linear mapping, exact
final release, numeric override precedence, non-mutation of a live Part session, angular/radial
mapping, translate/rotate triad construction, PatternCount quantization, and visible shell ownership.

## Next boundary

Block 124 consumes this infrastructure for interactive Extrude, path Extrude, and Revolve authoring.
Block 123 itself does not create feature previews or commit feature parameters.

## Replacement-shell construction consumer — 2026-10-23

`ShellConstructionTools` owns a separate layer and transparent overlay, with World XYZ position,
World-axis direction/frame rotation and positive new-offset handles. Axis labels and separated ring
radii keep three-axis controls identifiable. The owner freezes full-precision pre-drag vectors,
consumes candidates into existing task fields and runs the existing disposable construction preview;
only Apply commits. Relative rotations preserve both plane axes together. Numeric override, exact
final release, drag-only Escape, focus/camera cancellation and command expiry use the same layer
contracts, without granting it document authority. Reference-defined objects and existing shared
parameter bindings retain their established reference/read-only controls.

The render notification also occurs on preview redraw. This consumer compares camera and viewport
size before aborting a drag, keeping ordinary redraws from resetting a live interaction. Native
mapping continues to use the actual viewport ray/projection providers. Without a native viewer,
restoring a bookmark now retains its camera values and notifies consumers instead of discarding the
eye/target/scale used by the headless mapping.

Contract/proof: `docs/gui-sketch-workbench-mvp7.md`, `[shell-construction-manipulators]`,
`[shell-combined-manipulators]` and `[construction-manipulator-camera]`.

## Native projection precision — 2026-10-24

`OcctViewport::model_to_screen` retains subpixel coordinates from OCCT's own camera projection,
maps normalized device coordinates to the actual render-window size and divides by the device
pixel ratio. Integer pixel conversion previously collapsed nearby model points, which could give
different handle axes the same displayed direction and select the wrong handle. The correction
retains the native camera, perspective, window dimensions and DIP boundary; screen-to-ray picking
continues through the existing native provider.

`[gui][viewport][native-projection-precision]` projects small Y/Z displacements in orthographic and
perspective views, checks positive subpixel lengths and distinct directions. It failed natively
before the correction with a zero-length displacement. The Thin Extrude pointer/Escape regression
also exercises separate depth/thickness handles in an oblique view. Full/native GUI evidence lives
in `docs/gui-shell-acceptance-mvp9r.md`.

## Camera restoration and interaction capture — 2026-10-24

Named-view restoration switches the native OCCT projection before restoring eye/target/up/scale.
`Graphic3d_Camera::SetScale` converts perspective scale to eye distance; applying it under the old
perspective projection displaced a saved orthographic eye after a projection switch. The navigation
regression exposed 120→152.0954546225 before the fix and compares all captured fields after resize and
both projection transitions. Snapshot fields and persistence boundaries are unchanged.

Without a native viewer, standard views now derive their directions from OCCT's +Z-up orientations
and store a valid orthogonal up vector in the fallback camera. Projection changes notify consumers on
both backends. Native rendering and picking continue to use OCCT's actual camera mapping.

Shell grips use `OcctViewport::grab_interaction_mouse` / `release_interaction_mouse` to capture the
widget mouse and notify presentation consumers at both boundaries. Navigation/projection controls
therefore follow actual capture without timers or discovery. Camera-sensitive consumers still compare
camera/viewport values before cancelling a drag, so a capture notification itself is not a camera
change. `[block136-navigation]` and `[shell-combined-navigation]` cover automatic disable/re-enable and
a real extent drag/Escape after restoring a named view.

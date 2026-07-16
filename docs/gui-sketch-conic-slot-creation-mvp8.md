# GUI Sketch Conic and Slot Creation MVP-8

Status: implemented in Block 112.

This contract extends the Block-111 multi-click creation authority with circles, circular arcs,
ellipses, elliptical arcs, and mechanical slots. It keeps all screen-space interaction transient and
commits only existing Core intent through one `GuiDocumentSession` Part transaction.

## Authority boundary

`GuiSketchCreateController` owns the device-independent pick sequence, numeric input reuse,
rubber-band preview, deterministic expansion, and atomic commit request. Qt owns only action wiring,
event delivery, and preview publication. Core remains authoritative for Parameters, Sketch entities,
profiles, validation, JSON, dependency tracking, and undo/redo. Geometry remains authoritative for
later region and feature materialization.

The Block-107 snap result supplies plane coordinates. Snap kinds may publish automatic-constraint
previews, but no inferred relation becomes persistent in Block 112. Block 114 owns accepted automatic
and manual constraint authoring.

## Implemented tools and pick sequences

| Tool | Ordered picks | Persistent expansion |
|---|---|---|
| Center/radius circle | center, rim | `CircleProfile` + diameter `Parameter` |
| Center/diameter circle | center, rim | `CircleProfile` + diameter `Parameter` |
| Two-point circle | diameter endpoint, diameter endpoint | `CircleProfile` + diameter `Parameter` |
| Three-point circle | three non-collinear rim points | `CircleProfile` + diameter `Parameter` |
| Tangent circle | three candidate points | three-point `CircleProfile`; tangency remains preview-only |
| Center/start/end arc | center, start, end direction | one three-point `ArcSegment` |
| Three-point arc | start, through-point, end | one `ArcSegment` |
| Tangent arc | start, through-point, end | one `ArcSegment`; tangency remains preview-only |
| Ellipse | center, major-axis point, minor-radius point | four cubic `SplineSegment` entities + `ArcClosedProfile` |
| Elliptical arc | center, major-axis point, minor-radius point, start, end | one or more cubic `SplineSegment` entities |
| Center-to-center slot | first cap center, second cap center, half-width point | two lines + two semicircular arcs + `ArcClosedProfile` |
| Overall-length slot | first outer endpoint, second outer endpoint, half-width point | inset cap centers, two lines + two semicircular arcs + `ArcClosedProfile` |

All commands use the Block-106 `CollectingPicks -> NumericInput -> Preview -> commit` lifecycle.
`Esc` removes one pick or cancels without mutation. Completed fixed-pick tools commit automatically;
all committed output is represented by one undo/redo history entry.

## Full-circle persistence

A full circle is not stored as a sampled polyline. Its exact authored intent is:

```text
CircleProfile
  center: Point2
  diameter_parameter: ParameterId

Parameter
  type: Length
  value: positive diameter in mm
```

The profile and its new parameter are added inside the same Part transaction. A failed parameter,
profile, Sketch update, serialization, or recompute therefore publishes neither half. Existing
`blcad.part_document.mvp1` JSON spellings remain authoritative; Block 112 adds no schema or version.

The persisted center is projected into Block-108 topology as the stable profile-center point. The
creation rim and quadrant locations are transient preview handles. Numeric diameter/radius authoring
and parameter-backed in-canvas editing are completed by Block 115 rather than by duplicating a radius
coordinate in GUI state.

## Circular arc construction

`ArcSegment::create_three_point(...)` remains the sole persistent circular-arc representation.
Center/start/end input is converted deterministically to start, mid-sweep, and end points on the start
radius. Three-point and tangent-arc input use the selected through-point directly.

Duplicate points, zero radius, zero sweep, or collinear three-point input fail closed before the
transaction begins. No fallback line or partial entity is emitted.

## Ellipse representation

The Core model has exact circle and three-point arc intent but no independent analytic ellipse record.
Block 112 therefore uses a deterministic persistent cubic-Bezier representation rather than a sampled
polyline:

- a full ellipse is split into four 90-degree cubic `SplineSegment` spans;
- an elliptical arc is split into the minimum number of spans whose absolute sweep is at most 90
  degrees;
- control points use `4/3 * tan(delta_parameter / 4)`;
- a full ellipse adds one ordered `ArcClosedProfile`; an open elliptical arc adds no profile.

This representation survives JSON, topology migration, Geometry materialization, and exact undo/redo.
Block 113 now edits these ordinary spline spans through the same cubic representation, with control/
fit handles and complete candidate validation; no hidden ellipse-specific GUI curve is introduced.

## Slot representation

Both slot families resolve one axis, one perpendicular half-width, and two cap centers. Persistent
contour order is frozen as:

```text
top line
second semicircular arc
bottom line
first semicircular arc
```

The contour uses two `LineSegment` and two `ArcSegment` entities plus one `ArcClosedProfile`. The
center-to-center family keeps the first two picks as cap centers. The overall-length family moves each
cap center inward by the radius and rejects an overall length that is not greater than the width.

Because the contour is ordinary line/arc intent, existing region, profile, drag-handle, JSON, and
Geometry paths consume it without a slot-specific persistence type.

## Preview and handle behavior

Preview samples are transient and never serialized:

- circles and ellipses publish closed sampled rubber bands;
- arcs and elliptical arcs publish open sampled rubber bands;
- slots publish a closed line/arc-shaped rubber band;
- center, endpoint, quadrant, nearest, and inference snaps publish automatic-constraint previews;
- tangent tools additionally publish explicit `tangent` preview records.

After commit, arcs expose endpoint, through-point, center, and radius handles through Block 110.
Ellipse splines expose Block-113 control/fit/continuity handles. Slot lines and arcs expose ordinary
line/arc handles. Exact full-circle diameter editing remains parameter-owned and is surfaced by
Block 115.

## Determinism and failure rules

- ids use the existing lexicographically increasing `*.create.N` allocation rules;
- full-circle Parameters use `parameter.sketch.circle_diameter.N`;
- all coordinates, radii, and generated control points must be finite;
- zero or negative effective radii fail validation;
- collinear three-point circle/arc input fails validation;
- ellipse major and minor radii must be non-zero;
- elliptical-arc sweep must be non-zero;
- slot axis and width must be non-zero;
- overall slot length must exceed slot width;
- no command mutates the document during preview;
- one successful tool completion creates exactly one Part history entry.

## Verification

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[core][sketch-conics]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[geometry][sketch-conics]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-create-conics]"
```

Acceptance covers exact CircleProfile/Parameter persistence, JSON roundtrip, atomic undo/redo,
degenerate three-point rejection, tangent preview, cubic ellipse expansion, ordered slot contours, and
action registration in the Sketch command lifecycle.

## Following boundaries

Block 113 is implemented in `docs/gui-sketch-spline-text-mvp8.md` and owns fit/control-point spline
editing, control polygons, insertion/removal, continuity handles, deterministic conversion, and Sketch
text. Block 114 is next and owns persistent tangent and other automatic/manual constraint acceptance;
Block 115 owns radius/diameter dimensions and direct parameter editing.

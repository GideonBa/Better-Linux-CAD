# GUI Sketch Conic and Slot Creation MVP-8

Status: conic and slot creation is implemented in the current shell. Exact rational ellipse intent
and focused Core, Geometry and GUI regression coverage replace the original Block-112 cubic
approximation. Tangent tools construct and persist actual contact/tangent relations.

This contract extends the Block-111 multi-click creation authority with circles, circular arcs,
ellipses and mechanical slots. It keeps all screen-space interaction transient and
commits only existing Core intent through one `GuiDocumentSession` Part transaction.

## Authority boundary

`GuiSketchCreateController` owns the device-independent pick sequence, numeric input reuse,
rubber-band preview, deterministic expansion, and atomic commit request. Qt owns only action wiring,
event delivery, and preview publication. Core remains authoritative for Parameters, Sketch entities,
profiles, validation, JSON, dependency tracking, and undo/redo. Geometry remains authoritative for
later region and feature materialization.

The Block-107 snap result supplies plane coordinates. Snap kinds may publish automatic-constraint
previews. Block 112 itself never persists an inferred relation. Block 114 may convert an explicitly
identified authored/reference topology pair into a `SketchConstraintIntent`, run the normal disposable
solve/conflict preview, and persist only an accepted candidate.

## Implemented tools and pick sequences

| Tool | Ordered picks | Persistent expansion |
|---|---|---|
| Center/radius circle | center, rim | `CircleProfile` + diameter `Parameter` |
| Center/diameter circle | center, rim | `CircleProfile` + diameter `Parameter` |
| Two-point circle | diameter endpoint, diameter endpoint | `CircleProfile` + diameter `Parameter` |
| Three-point circle | three non-collinear rim points | `CircleProfile` + diameter `Parameter` |
| Tangent circle | source contact, rim point | exact `CircleProfile` + diameter parameter + tangent relation |
| Center/start/end arc | center, start, end direction | one three-point `ArcSegment` |
| Three-point arc | start, through-point, end | one `ArcSegment` |
| Tangent arc | source endpoint, end | one `ArcSegment` + coincidence and tangent relations |
| Ellipse | center, major-axis point, minor-radius point | four exact rational `SplineSegment` entities + `ArcClosedProfile` |
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
radius. Three-point input uses the selected through-point directly. Tangent input derives the center and sweep
from the source tangent and chosen endpoint.

Duplicate points, zero radius, zero sweep, or collinear three-point input fail closed before the
transaction begins. No fallback line or partial entity is emitted.

## Tangent construction and persistent contact

`SketchTangentCreation` is the Core construction authority. The first pick must identify an editable
line, arc or spline. Tangent arcs continue an endpoint in the source curve's outgoing direction;
tangent circles accept a line contact anywhere on the segment and arc/spline contacts at endpoints.
The second pick is the desired new arc endpoint or another point on the new circle. The center lies
on the source normal and is solved analytically from the second point. Coincident picks, a second
point on the tangent line, a missing source and an irregular zero-derivative contact are rejected.

The shell resolves an actual curve hit for the first pick. The controller stores its stable source id,
previews the real tangent construction, and re-resolves the source against the current Sketch before
commit. A coordinate-only first pick cannot masquerade as a tangent reference.

Arc creation persists endpoint coincidence and entity tangency. Circle creation persists tangency
and, for endpoint contacts, a point-on-circle relation. The solver supports circle tangency to a line,
or to an arc/spline whose endpoint is constrained onto that circle. Circle-circle tangency and arbitrary
interior contacts on curved sources are outside this pick contract. All creation recipes solve with
existing dimensions as well as constraints; a conflict rolls back the curve, parameter and relations
as one transaction. Save/Open and Undo/Redo preserve the complete intent.

## Ellipse representation

`SplineSegment::create_elliptical_arc` is the Core authority for exact ellipse spans. It accepts a
center, major-axis vector, positive minor radius, start angle and signed sweep up to a quarter turn.
The construction uses an exact rational quadratic conic, elevated in homogeneous coordinates to a
rational cubic. The full ellipse consists of four spans in an ordered `ArcClosedProfile`; open
ellipse spans remain available for internal conic operations and projections. The standalone
elliptical-arc GUI command has been removed; the ribbon offers the fitted Spline tool described in
`docs/gui-sketch-spline-text-mvp8.md`.

The persistent curve stores four poles and four finite positive weights. Its evaluation is the
weighted Bernstein sum divided by the weighted basis sum. This is mathematically an exact conic;
there is no polynomial fitting tolerance. Ordinary cubic splines retain unit weights and their
existing JSON spelling. Rational spans use `kind: "rational_cubic_bezier"` with a required four-value
`weights` array in Part JSON. Topology JSON uses `kind: "rational_spline"` with the same weights, and materialization preserves
them. Both formats use explicit new kind spellings so older readers reject rather than flatten the curve. Old polynomial documents continue to load unchanged; older readers reject the new kind.

`RationalBezier` centralizes evaluation, first/second derivatives and exact interval subdivision.
Split, trim, transform, topology solve, control-point editing, interaction sampling, OCCT viewport
edges and feature adapters retain weights. Extrude, Revolve, Sweep and Loft profiles construct weighted
OCCT Bezier curves. Surface path workflows retain their existing sampled-path contract but sample the
rational curve. Extension rejects a rational continuation crossing a singular interval.

Control-point editing intentionally allows the user to deform a conic into a general rational curve.
Converting rational spans to the polynomial fit-point editor is rejected because that conversion
would change the curve. There is no separate center/axis handle editor for an already-created ellipse.

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

## Preview, handles, and accepted inference

Preview samples are transient and never serialized:

- circles and ellipses publish closed sampled rubber bands;
- circular arcs publish open sampled rubber bands;
- slots publish a closed line/arc-shaped rubber band;
- center, endpoint, quadrant, nearest, and inference snaps publish automatic-constraint previews;
- tangent tools publish their resolved contact and exact tangent geometry.

For optional snap inference, a supported preview becomes persistent only when Block 114 receives stable
point/entity targets, produces a valid automatic candidate, and the disposable Block-109 solve returns
`under_constrained` or `fully_constrained`. Conflicting, redundant, invalid, or non-convergent inference
remains preview-only.

After commit, arcs expose endpoint, through-point, center, and radius handles through Block 110.
Ellipse spans expose control/continuity handles with their rational weights preserved. Slot lines and arcs expose ordinary
line/arc handles. Exact full-circle diameter editing remains parameter-owned and is surfaced by
Block 115.

## Determinism and failure rules

- ids use the existing lexicographically increasing `*.create.N` allocation rules;
- full-circle Parameters use `parameter.sketch.circle_diameter.N`;
- all coordinates, radii, and generated control points must be finite;
- zero or negative effective radii fail validation;
- collinear three-point circle/arc input fails validation;
- ellipse major and minor radii must be non-zero;
- internal rational conic-span sweeps must be non-zero;
- slot axis and width must be non-zero;
- overall slot length must exceed slot width;
- no command mutates the document during preview;
- one successful tool completion creates exactly one Part history entry.

## Verification

```bash
./build/dev-gui/blcad_core_tests '[core][sketch-conics]'
./build/dev-gui/blcad_geometry_tests '[geometry][sketch-conics]'
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests '[gui][sketch-create-conics]'
```

Core tests verify analytic ellipse residuals, rotated axes, derivatives, reversal, exact subdivision,
JSON/topology roundtrips, materialization and invalid weights. Geometry verifies analytic extrusion
volume before and after splitting a profile span. GUI tests exercise circle, arc, ellipse and both slot
families, preview cancellation, degenerate picks, shape/profile counts, Save/Open and one-step undo/redo. Tangent cases additionally verify reference validation, exact contact/radius,
persistent relationships and rollback of degenerate requests.

## Following boundaries

Block 113 is implemented in `docs/gui-sketch-spline-text-mvp8.md`. Block 114 is implemented in
`docs/gui-sketch-constraint-authoring-mvp8.md` and owns accepted tangent and other automatic/manual
constraint intent. Block 115 owns radius/diameter dimensions and direct parameter editing.

# MVP 3: Parametric Bolt Circle Pattern

Status: implemented first seed.

This block implements the first meaningful parametric feature test from `docs/mvp-plan.md`: a bolt circle whose radius, hole count, and hole diameter are parameters, with automatic recompute on change. It follows the bolt-circle intent described in `docs/feature-system.md`.

## Count parameters

Counts are dimensionless whole numbers, so lengths cannot express them.

- `QuantityKind` distinguishes `LengthMm` and `Count`.
- `Quantity::count(value)` validates finite, `>= 1`, whole-number values and exposes `count_value()`.
- `Parameter::create_count(...)` creates a `ParameterType::Count` parameter.
- `Parameter::with_value` keeps count validation on updates, so `PartDocument::set_parameter_value` works for counts.
- JSON: `"type": "count"`, `"unit": "1"`, plain numeric `value`.

Length parameters reject count quantities and vice versa.

## CircularHolePattern record

The pattern is model intent stored in a sketch, not baked geometry:

```text
CircularHolePattern
  id                       # ProfileId, unique across all profile kinds in the sketch
  center                   # Point2, sketch-local
  angle_offset_deg         # first-hole angle offset, finite
  radius_parameter         # ParameterId, must be a Length parameter
  count_parameter          # ParameterId, must be a Count parameter
  hole_diameter_parameter  # ParameterId, must be a Length parameter
```

Hole `i` of `n` sits at angle `angle_offset + 2*pi*i/n` on the circle of `radius` around `center`. This matches the closed-360° pattern formula in `docs/pattern-and-mirror-features.md`.

## Validation and dependency graph

`PartDocument::add_sketch` validates that all three parameters exist and have the right type (`Length`/`Count`/`Length`) and adds dependency edges:

```text
part.bolt_circle_radius  -> sketch.bolt_circle
part.bolt_count          -> sketch.bolt_circle
part.bolt_hole_diameter  -> sketch.bolt_circle
```

With a subtractive cut consuming the sketch, changing any pattern parameter marks the sketch and the cut feature, and the recompute plan reruns only the affected features.

## Geometry execution

`GeometryRecomputeExecutor::execute_subtractive_extrude` accepts a sketch with exactly one circular hole pattern. It expands the pattern into sequential axis-directed through-all circular cuts, one per hole, reusing the existing circular-cut adapter. Every hole is bounds-checked against the resolved workplane (bounded derived-face workplanes reject holes outside the source face, reporting `pattern.<id>.hole.<index>`).

## JSON

```json
"circular_hole_patterns": [
  {
    "id": "pattern.bolt_circle",
    "center": {"x": 0.0, "y": 0.0},
    "angle_offset_deg": 0.0,
    "radius_parameter": "part.bolt_circle_radius",
    "count_parameter": "part.bolt_count",
    "hole_diameter_parameter": "part.bolt_hole_diameter"
  }
]
```

`examples/bolt_circle_plate.blcad.json` is the checked-in reference model: a 120x80x8 plate with a 6-hole M6-clearance bolt circle at radius 30. It loads, recomputes, and exports through `blcad_export_step`.

## Covered by tests

- count-quantity and count-parameter validation (rejects 0, negatives, fractions, non-finite).
- pattern record validation and cross-kind profile-id uniqueness.
- parameter existence/type validation in `PartDocument::add_sketch`.
- dependency edges from all three parameters to the sketch.
- invalidation: changing `bolt_count` marks the sketch and the cut feature.
- JSON roundtrip for count parameters and patterns, including restored graph edges.
- geometry: 6 holes remove exactly 6 hole volumes; count 6 -> 12 recomputes incrementally; radius change keeps volume but recomputes; out-of-bounds pattern on a derived top-face workplane fails validation.

## Deliberate limitations

- the pattern lives in one sketch of one part; assembly-scoped parameters are MVP 4 (`docs/mvp-plan.md`).
- holes are plain through-all cylindrical cuts; hole semantics (clearance/thread/countersink) belong to the hole wizard (`docs/hole-wizard.md`).
- no skip instances, partial angles, or seed-feature patterns yet (`docs/pattern-and-mirror-features.md`).
- each hole is a separate boolean; batching the tool bodies is a later optimization.

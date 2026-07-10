# Spline and tangent-continuity sketch profile seed

This document describes the first spline-profile seed.

The goal is to add one deterministic spline representation to the sketch/profile pipeline without introducing a full spline editor, a NURBS model, or a sketch constraint solver.

## Implemented scope

The core sketch model now includes `SplineSegment` as a cubic Bezier curve:

```text
start -> control1 -> control2 -> end
```

A spline segment is explicit model intent. It stores its own control points and is not computed from solver state.

The existing `ArcClosedProfile` profile record is extended to accept ordered line, circular-arc, and spline curve segment IDs. The name remains historical; at this stage it acts as the first mixed-curve closed-profile record.

## Tangent metadata

`SketchTangentContinuity` stores first tangent-continuity intent between two explicit sketch curve entities.

The first seed validates that both referenced entities exist and that the two entities are distinct. It does not solve or modify curve control points. Tangency is persisted as model intent for later solver work.

## JSON persistence

Sketch JSON now supports:

```json
{
  "spline_segments": [
    {
      "id": "spline.right",
      "kind": "cubic_bezier",
      "start": {"x": 10.0, "y": -8.0},
      "control1": {"x": 18.0, "y": -4.0},
      "control2": {"x": 18.0, "y": 4.0},
      "end": {"x": 10.0, "y": 8.0}
    }
  ],
  "tangent_continuities": [
    {
      "id": "tangent.bottom_spline",
      "kind": "tangent",
      "first_entity": "line.bottom",
      "second_entity": "spline.right"
    }
  ]
}
```

Existing `arc_closed_profiles` can reference spline IDs in their ordered `curve_segments` list.

## Geometry behavior

The optional geometry layer maps spline control points through the resolved sketch workplane and builds an OCCT Bezier edge from four poles.

Spline profiles can be used for:

- additive extrude from one mixed line/arc/spline closed profile
- subtractive through-all cut from one mixed line/arc/spline closed profile

Through-all cuts use the resolved sketch-normal axis and are no longer restricted to principal X/Y/Z axes for this path.

## Test coverage

The tests cover:

- JSON roundtrip for `spline_segments`
- JSON roundtrip for `tangent_continuities`
- restored spline control points
- additive spline-profile recompute
- subtractive spline-profile recompute

A checked-in example model is available at:

```text
examples/spline_profile_prism.blcad.json
```

## Deliberate limitations

This block does not implement a full spline editor.

It does not implement NURBS weights, knot vectors, degree elevation, automatic tangent solving, automatic fillets, constraint solving, driven tangent dimensions, GUI spline handles, 3D splines, sweep, loft, surface stitching, or closed-shell-to-solid conversion.

Spline self-intersection validation is intentionally conservative. The first seed validates profile connectivity and uses a chord-level approximation for spline interactions where exact spline-curve intersection is not implemented yet.

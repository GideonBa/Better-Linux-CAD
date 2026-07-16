# Spline and tangent-continuity sketch profile seed

This document describes the original spline-profile seed and its Block-113 interactive extension.

The persistent spline representation remains one deterministic cubic Bezier curve:

```text
start -> control1 -> control2 -> end
```

A spline segment is explicit model intent. It stores its own control points and is not computed from
screen state or an opaque solver cache.

## Original implemented scope

The Core Sketch model includes `SplineSegment` as a cubic Bezier curve. The historical
`ArcClosedProfile` record accepts ordered line, circular-arc, and spline entity ids and therefore acts
as the mixed-curve closed-profile carrier.

`SketchTangentContinuity` stores tangent-continuity intent between two distinct explicit Sketch curve
entities. The record validates references and persists semantic intent; Geometry and solver consumers
remain responsible for evaluation.

## JSON persistence

Historical Part Sketch JSON supports:

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

Existing `arc_closed_profiles` can reference spline ids in their ordered `curve_segments` list.

## Geometry behavior

The optional Geometry layer maps spline control points through the resolved Sketch workplane and
builds an OCCT Bezier edge from four poles. Spline profiles support additive extrude and subtractive
through-all cut. Through-all cuts use the resolved Sketch-normal axis and are not restricted to
principal axes.

## Block-113 interactive extension

Block 113 adds `SketchSplineEditModel`, `SketchSplineGeometryEvaluator`, and
`GuiSketchSplineController` while preserving cubic `SplineSegment` as the sole persistent curve type.
The extension implements:

- semantic endpoint, control-point, fit-point, and continuity handles;
- visible control polygons;
- control-point movement with connected endpoint preservation;
- deterministic control-to-fit and fit-to-control conversion;
- fit-point insertion and removal with at least two retained points;
- Catmull-Rom-to-cubic-Bezier fit expansion;
- C0/G1 continuity diagnostics and tangent-handle alignment;
- immutable preview candidates and one atomic `Edit sketch spline` document transaction;
- stale-source and dependency-aware fail-closed behavior.

Fit points, control polygons, sampled display curves, curvature samples, and handle positions are
derived authoring state. Persisted output remains cubic segments plus existing tangent-continuity
records and survives the existing Part JSON path.

The complete interactive contract, including Sketch text and font fallback, is
`docs/gui-sketch-spline-text-mvp8.md`.

## Test coverage

The original and Block-113 tests cover:

- spline and tangent-continuity JSON roundtrip;
- restored spline control points;
- additive and subtractive spline-profile recompute;
- deterministic fit/control conversion and generated ids;
- fit-point insertion/removal;
- control polygons and C0/G1 continuity handles;
- candidate immutability, atomic commit, and exact undo/redo;
- deterministic spline sampling, derivatives, curvature, and continuity diagnostics.

A checked-in example model is available at:

```text
examples/spline_profile_prism.blcad.json
```

## Deliberate limitations

The persistent model does not introduce NURBS weights, knot vectors, arbitrary degree elevation, or a
second fit-spline save format. Exact spline self-intersection remains conservative where the existing
profile pipeline uses chord-level interaction tests. Block 114 owns general constraint authoring;
Block 115 owns driving/reference dimensions.

# Arc and trim/extend sketch profile seed

This document describes the first sketch arc and trim/extend seed.

The goal is to support one deterministic sketch profile containing circular arcs while keeping the existing line-only profile path stable. This is not a full sketch solver and does not implement GUI trim/extend editing.

## Implemented scope

The core model now has:

- `ArcSegment`
- `ArcClosedProfile`
- `SketchTrimExtendOperation`
- `SketchTrimOperationId`

`ArcSegment` is currently defined by three sketch-local points:

- start
- mid
- end

The three points must be distinct and non-collinear.

`ArcClosedProfile` stores an ordered list of curve entity IDs. Each referenced entity must be an explicit `LineSegment` or explicit `ArcSegment`. The first seed keeps this separate from the existing line-only `ClosedProfile` so old line-profile behavior stays stable.

`SketchTrimExtendOperation` stores explicit trim/extend metadata:

- operation id
- operation kind: `trim` or `extend`
- target entity id
- replacement endpoint

This metadata is persisted in the model as sketch intent, but it does not yet solve or rewrite curves.

## Validation behavior

The first validation seed checks:

- arc IDs are non-empty
- arc points are distinct
- arc points are non-collinear
- trim/extend operation IDs are non-empty
- trim/extend targets are explicit line or arc entities
- arc closed profiles contain at least three curve segments
- arc closed profiles reference existing explicit line or arc entities
- ordered line/arc profile segments are connected end-to-start and close back to the first segment

The seed does not yet check curved self-intersections exactly. It also does not infer tangent continuity or solve constraints.

## Geometry behavior

The optional geometry layer now supports `ArcClosedProfile` through `ClosedProfileAdapter` curve APIs:

- `make_extruded_profile_from_curves`
- `cut_profile_curves_through_all`

The adapter builds OCCT wires from explicit line edges and circular-arc edges. Circular arcs are created from the three-point arc definition using OCCT arc construction rather than storing tessellated arc points as model intent.

`GeometryRecomputeExecutor` now accepts exactly one `ArcClosedProfile` for:

- additive extrude
- subtractive through-all cut

The arc profile is resolved through the current sketch workplane, checked against workplane bounds, converted into OCCT curve edges, and then passed into the existing prism/cut pipeline.

## JSON intent

The intended stable JSON shape for this block is:

```json
{
  "arc_segments": [
    {
      "id": "arc.top",
      "kind": "three_point",
      "start": {"x": 10.0, "y": 0.0},
      "mid": {"x": 0.0, "y": 10.0},
      "end": {"x": -10.0, "y": 0.0}
    }
  ],
  "trim_extend_operations": [
    {
      "id": "trim.arc.top",
      "kind": "trim",
      "target_entity": "arc.top",
      "replacement_endpoint": {"x": 8.0, "y": 1.0}
    }
  ],
  "arc_closed_profiles": [
    {
      "id": "profile.arc",
      "curve_segments": ["line.left", "line.bottom", "line.right", "arc.top"]
    }
  ]
}
```

The source-level JSON serializer still needs to be extended in a follow-up pass before arc profiles roundtrip through `.blcad.json`. The model shape above is the intended persisted representation; it deliberately stores model intent, not tessellated arc caches or raw OCCT edges.

## Test coverage

The tests cover:

- rejection of collinear three-point arcs
- storing trim metadata on explicit arc entities
- ordered line/arc profile validation
- additive recompute from one arc closed profile
- subtractive through-all recompute from one arc closed profile

## Deliberate limitations

This block does not implement splines, tangent constraint solving, automatic fillets, trim/extend solving, GUI curve editing, exact curved self-intersection checks, 3D sketches, sweep, loft, surface stitching, or closed-shell-to-solid conversion.

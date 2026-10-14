# Arc and trim/extend sketch profile seed

This document describes the first sketch arc and trim/extend seed.

The goal is to support one deterministic sketch profile containing circular arcs while keeping the existing line-only profile path stable. This is not a full sketch solver and does not implement GUI trim/extend editing.

## Implemented scope

The core model has:

- `ArcSegment`
- `ArcClosedProfile`
- `SketchTrimExtendOperation`
- `SketchTrimOperationId`

`ArcSegment` is currently defined by three sketch-local points:

- start
- mid
- end

The three points must be distinct and non-collinear.

`ArcClosedProfile` stores an ordered list of curve entity IDs. Current profiles may reference explicit
lines, circular arcs and polynomial or rational cubic splines. This remains separate from the existing
line-only `ClosedProfile`.

`SketchTrimExtendOperation` stores explicit trim/extend metadata:

- operation id
- operation kind: `trim` or `extend`
- target entity id
- replacement endpoint

This metadata is persisted in the model as sketch intent, but it does not yet solve or rewrite curves.

## Validation behavior

The validation seed checks:

- arc IDs are non-empty
- arc points are distinct
- arc points are non-collinear
- trim/extend operation IDs are non-empty
- trim/extend targets are explicit line or arc entities
- arc closed profiles contain at least two curve segments; a two-edge contour needs a curved segment
- arc closed profiles reference existing explicit line, arc or spline entities
- ordered profile segments form one closed contour; connectivity determines whether each source
  curve is traversed forwards or backwards
- line/line, line/arc, and arc/arc intersections are rejected when they are not the expected adjacent shared endpoint

This is still not a full sketch solver. It does not infer tangent continuity, solve constraints, trim curves automatically, or prove all advanced curve-topology cases.

## Geometry behavior

The optional geometry layer supports `ArcClosedProfile` through `ClosedProfileAdapter` curve APIs:

- `make_extruded_profile_from_curves`
- `cut_profile_curves_through_all`

The adapter builds OCCT wires from explicit line edges and circular-arc edges. Circular arcs are created from the three-point arc definition using OCCT arc construction rather than storing tessellated arc points as model intent.

`GeometryRecomputeExecutor` accepts exactly one `ArcClosedProfile` for:

- additive extrude
- subtractive through-all cut

The arc profile is resolved through the current sketch workplane, checked against workplane bounds, converted into OCCT curve edges, and then passed into the existing prism/cut pipeline.

## JSON intent

Arc sketch entities, trim/extend metadata, and arc closed profiles now roundtrip through source-level `.blcad.json` serialization:

```json
{
  "arc_segments": [
    {
      "id": "arc.top",
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

The JSON stores model intent only. It does not store tessellated arc caches, resolved OCCT edges, or trim-solver state.

## Curved-contour follow-up (October 2026)

Two semicircular arcs can bound a circle, and one line plus one arc can bound a semicircle.
`resolve_sketch_contour` derives traversal directions from the existing ordered ids. Source coordinates,
curve ids and authored start/end roles are preserved. No extra JSON orientation flag is required.
Shared topology joins the actual traversal endpoints, including start-to-start or end-to-end contacts.
Geometry reverses only its derived edge representation, retaining rational weights and pole order.

Proof: `[sketch-curved-regions]` checks two-edge profiles, mixed authored directions, canonical point
sharing, topology materialization and analytic circle/semicircle extrusion volume.

A checked-in example model is available at `examples/arc_profile_prism.blcad.json`.

## Test coverage

The tests cover:

- rejection of collinear three-point arcs
- storing trim metadata on explicit arc entities
- ordered line/arc profile validation
- rejection of self-intersecting line/arc closed profiles
- JSON roundtrip for arc segments, trim/extend operations, and arc closed profiles
- additive recompute from one arc closed profile
- subtractive through-all recompute from one arc closed profile

## Deliberate limitations

This block does not implement splines, tangent constraint solving, automatic fillets, trim/extend solving, GUI curve editing, full advanced curved-topology validation, 3D sketches, sweep, loft, surface stitching, or closed-shell-to-solid conversion.

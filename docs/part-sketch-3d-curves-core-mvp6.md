# Part 3D Sketch Curves Core MVP-6

Status: implemented in Block 76.

Block 76 extends the Block-75 model-space sketch domain with persistent in-memory Arc, Spline,
Helix, and Guide-Curve intent. It freezes curve representation, cross-source point identity,
parameter dependencies, ownership, and failure behavior without executing geometry.

## Point-reference contract

`SketchPointReference3D` is exactly one of:

- an owned `SketchPoint3D` id
- a Part `ConstructionPointId`
- a planar `SketchId` plus a point-valued `SketchReferenceTarget`

The planar target is a line-segment start/end or projected point. The reference stores source
identity, never a resolved or copied model-space coordinate. `PartDocument` validates the external
source and creates ConstructionPoint/planar-Sketch dependency edges transactionally. Block 77
freezes its JSON grammar; generated vertex/edge-point persistence remains on that boundary.

## Arc and spline contract

`SketchArc3D` is a three-point arc with distinct start, intermediate, and end references.

`SketchSpline3D` explicitly selects `FitPoints` or `ControlPoints`, preserves authored point order,
and supports degree 1 through 5. The first seed requires more references than the degree and rejects
duplicate point sources. Continuity is explicit:

- `Positional` for every supported degree
- `Tangent` for degree 2 or higher
- `Curvature` for degree 3 or higher

Local point removal fails while an Arc or Spline references the point.

## Helix contract

`SketchHelix3D` owns a typed DatumAxis or ConstructionLine axis, positive Length radius and pitch
parameters, a positive integer Count turns parameter, and explicit right- or left-handed intent.
The axis and all three parameters feed the `Sketch3D` dependency node. Wrong-unit, missing-axis, or
missing-parameter input fails before document ownership changes.

## Guide-curve contract

`SketchGuideCurve3D` annotates an already-owned Line, Polyline, Arc, Spline, or Helix with one role:
`General`, `SweepPath`, `LoftGuide`, or `Centerline`, plus an optional label. It does not duplicate
the source curve. A referenced source cannot be removed before its guide metadata is removed.

All Block-75/76 entities share one sketch-local `SketchEntityId` scope.

## Deliberate boundary

There is still no general 3D constraint solver and no OCCT curve. The current Part serializer
continues to reject documents containing `Sketch3D`. Block 77 defines additive JSON, deterministic
entity order, and cross-source semantic-reference grammar. Block 78 performs Geometry conversion.

## Verified behavior

- mixed local, construction, and planar point references
- three-point Arc identity and local-point removal protection
- fit/control Spline order, degree 1–5, and C0/C1/C2-equivalent limits
- a Spline referencing three planar sketches on differently oriented planes
- DatumAxis Helix radius/pitch/turn/handedness validation and invalidation
- Guide-Curve roles and source-removal protection
- transactional missing source, wrong unit, duplicate, and invalid continuity failure

```text
./build/dev-geometry/blcad_core_tests "[core][sketch-3d-curves]"
```

Blocks 48–76 are implemented. Block 77 3D Sketch JSON and semantic references is next.

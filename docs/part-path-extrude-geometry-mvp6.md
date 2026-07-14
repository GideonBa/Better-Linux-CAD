# Part Path-following Extrude Geometry MVP-6

Status: implemented in Block 83.

Block 83 extends the existing `AdditiveExtrude` and `SubtractiveExtrude` feature types with
`direction = path` and a required `PathCurveId`. The authored feature identity remains an Extrude
or Extruded Cut; it is not persisted as a `SweepFeature`.

## Core intent and persistence

Path-directed features reference one existing open `PathCurve`. The reference participates in the
dependency graph, invalidation, recompute ordering, and removal protection. Straight Extrudes reject
`path_curve`; path-directed Extrudes reject straight extent, taper, and thin-wall intent.

JSON keeps the existing `additive_extrude` or `subtractive_extrude` type and writes:

```json
{
  "direction": "path",
  "path_curve": "path.main"
}
```

No `extrude` extent object is written for this mode. Existing straight-feature records remain
unchanged and readable.

## Geometry execution

The executor resolves the same connected planar, construction, and model-space Sketch3D path
sources as Blocks 81–82. It reuses the shared OCCT path-frame pipeline and the `PathCurve`
orientation rule, then applies the result transactionally through the established Body operation:
`NewBody`, `Join`, `Cut`, or `Intersect`.

Supported closed profiles are rectangle, circle, bounded line/arc loops, and detected regions.
Composite regions with holes remain outside this adapter boundary. Twist and guide controls remain
Sweep-specific; path-directed Extrude deliberately exposes the simpler PathCurve orientation
contract.

## Focused proof

```text
./build/blcad_core_tests "[core][path-extrude]"
./build/blcad_geometry_tests "[geometry][path-extrude]"
```

The proof covers stable feature identity, validation, dependency/removal protection, JSON
round-trip and malformed input, plus additive, curved-route, and subtractive transactional
geometry execution.

Blocks 48–89 are implemented. Block 84 ProfileSectionReference and Loft Core intent plus JSON is
next.

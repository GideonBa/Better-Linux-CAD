# Part Draft Geometry MVP-6

Status: implemented in Block 74.

Block 74 executes the persistent Block-73 `DraftFeature` intent as checked OCCT solid geometry.
Core remains authoritative for target Body, ordered semantic faces, Axis/Line pull direction,
neutral plane, and signed Angle; Geometry owns current-topology recovery and draft execution.

## Geometry contract

`DraftAdapter` resolves every authored `FaceReference` against the current target solid. Planar
faces are recovered from the current positions of three semantic vertices of their rectangular
additive-extrude producer. Cylindrical faces are recovered from the subtractive circular profile's
axis and radius. Current Body-transform state participates in recovery; persisted OCCT identities
and traversal indices do not.

The pull direction accepts all typed `AxisReference` sources already supported by the common
resolver: datum axes, construction lines, semantic circular axes, and semantic linear edges. The
neutral plane accepts datum, construction, and semantic planar-face references. Reference
transforms are applied before execution.

The public signed convention remains independent of OCCT: positive values expand selected faces
away from the neutral plane while advancing along positive pull direction; negative values
contract them. The adapter maps that convention to OCCT's opposite signed removal convention. A
reversed neutral-plane normal does not change the authored sign.

Every face must be accepted by `BRepOffsetAPI_DraftAngle`; the result must be exactly one valid
solid. Missing or ambiguous topology, tangent/incompatible directions, null or multi-solid output,
self-intersection, and invalid topology fail closed.

## Recompute and cache publication

`GeometryRecomputeExecutor` treats Draft as an in-place Body-history producer. It finds the latest
dependent target-Body product, validates the current finite non-zero Angle in `(-90°, +90°)`,
resolves pull direction and neutral plane, and runs the adapter in a working `ShapeCache`. Feature,
Body, and legacy final-shape products are published only after complete success.

Angle changes execute only Draft. Upstream dimension changes execute the upstream producer and
Draft, whose semantic faces are recovered on the changed solid. Any failure preserves the caller's
previous cache atomically.

## Verified behavior

- positive and negative draft with the documented signed result
- multiple ordered semantic faces
- arbitrary construction-line pull direction
- non-root construction neutral planes
- incremental Angle recompute and semantic recovery after an upstream width edit
- broken-reference, tangent-direction, and self-intersection rejection without cache replacement

```text
./build/dev-geometry/blcad_geometry_tests "[geometry][draft-feature]"
```

Blocks 48–76 are implemented. Block 75 Basic 3D Sketch Core intent and Block 76 richer 3D curve intent are implemented; Block 77 3D Sketch JSON and semantic references is next.

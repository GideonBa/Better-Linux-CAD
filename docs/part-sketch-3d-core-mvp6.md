# Part Basic 3D Sketch Core MVP-6

Status: implemented in Block 75.

Block 75 introduces a separate model-space sketch domain instead of extending planar `Sketch`
coordinates beyond their workplane. It freezes the in-memory Core identity and ownership boundary
for `Sketch3D`, `SketchPoint3D`, `SketchLine3D`, and `SketchPolyline3D`.

## Coordinate and entity contract

Every point owns three `SketchCoordinate3D` values. A coordinate is exactly one of:

- a finite signed explicit `LinearDisplacementMm` in Part model space
- a typed reference to an existing Part `Length` parameter

This allows zero and negative explicit coordinates while rejecting Angle, Count, non-finite, or
missing coordinate inputs. Parameter values remain governed by the existing positive-Length
parameter contract; richer signed parameter expressions are outside this block.

A `Sketch3D` owns points, lines, and polylines in one sketch-local `SketchEntityId` scope. Lines
reference two distinct owned point ids. Polylines preserve their authored vertex order, require at
least two owned vertices, and reject consecutive duplicates. Entity creation never copies or
implicitly creates points.

Point removal fails while a line or polyline references that point. Lines and polylines can be
removed independently. A 3D sketch id shares the Part-document sketch node namespace with planar
sketches, preventing ambiguous dependency identities.

## Part-document and dependency contract

`PartDocument` owns 3D sketches independently from planar sketches and exposes add, find, count,
and remove operations. Adding a sketch validates all parameter-bound coordinates transactionally,
adds one graph node, and connects every distinct coordinate parameter to that node. Parameter
changes therefore invalidate the 3D sketch and place it in a recompute plan.

Removing a 3D sketch removes its graph node and incoming coordinate dependencies. Removal fails if
later model intent depends on that sketch. Invalid additions leave ownership, graph, and
invalidation state unchanged.

## Deliberate boundary

Block 75 provides no general 3D constraint solver, curve geometry, OCCT conversion, or planar
workplane. Block 76 adds spline, arc, helix, and guide-curve intent in
`docs/part-sketch-3d-curves-core-mvp6.md`. Block 77 owns additive JSON and cross-source semantic
references; therefore Block-75/76 3D sketches are not yet serialized and the current Part
serializer rejects them explicitly instead of dropping intent. Block 78 converts the stable intent
to geometry.

## Verified behavior

- finite signed explicit and typed parameter-driven coordinates
- shared sketch-local entity-id scope and owned endpoint validation
- exact ordered polyline vertices
- line/polyline point-removal protection
- parameter dependencies, invalidation, and recompute planning
- transactional wrong-unit, missing-reference, and planar/3D id-collision rejection
- document-level removal and graph cleanup

```text
./build/dev-geometry/blcad_core_tests "[core][sketch-3d]"
```

Blocks 48–76 are implemented. Block 77 3D Sketch JSON and semantic references is next.

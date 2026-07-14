# Body Transform Geometry and Associative Ownership MVP-6

Status: implemented in Block 57.

## Scope and authority

Block 57 executes the persistent Block-56 `BodyTransform` stack during full and incremental
recompute. `PartDocument` remains the only model-intent authority. OCCT shapes, cumulative affine
states, transformed reference frames, and transformed semantic results live only in `ShapeCache`.
No transform writes back into a Sketch or construction record.

## Geometry execution

`BodyTransformAdapter` applies:

```text
translate      BRepBuilderAPI_Transform + gp_Trsf translation
rotate         BRepBuilderAPI_Transform + axis/angle rotation
uniform_scale  BRepBuilderAPI_Transform + positive uniform scale
```

The executor consumes the preceding producer's feature-scoped shape, stores a transform-scoped
shape, advances the target Body result, and commits transactionally. This prevents repeated
incremental execution from applying a transform twice. Cumulative affine state interprets World,
BodyLocal, SketchLocal, and construction-reference coordinates without adding OCCT types to Core.
Explicit, datum-axis, construction-line, and semantic-edge rotation axes resolve at execution time.

Stack order is the persistent insertion/graph order and is intentionally non-commutative. Focused
proof compares translate-then-rotate with rotate-then-translate.

## Associative ownership

When `apply_to_owned_sketches` is set, every matching `SketchOwnership` derives a transformed
workplane frame in the cache. The Sketch's local entities and 2D coordinates are untouched.
Sketches without a matching ownership record keep their authored frame.

When `apply_to_owned_construction_geometry` is set, the derived association closure contains the
owned Sketch's workplane reference and construction point/line references projected into that
Sketch. Their persistent records stay unchanged; cache-aware workplane/projector queries apply the
derived affine state. This is the first explicit construction-ownership cut and does not infer
ownership for unrelated construction geometry.

Generated semantic face/edge/vertex queries have cache-aware overloads. They recompute semantic
geometry from persistent intent and then apply the latest derived Body transform. Cache-aware
projected-point and projected-line resolution consumes these refreshed semantic results.

## Failure and compatibility

Missing predecessor shapes/states, unresolved frames/axes, empty OCCT results, and invalid Geometry
operations fail without publishing a partial cache update. Historical zero-transform documents and
the original resolver overloads retain their previous behavior.

Focused proof:

```bash
./build/blcad_geometry_tests "[geometry][body-transform]"
```

The proof covers translation, rotation through ordered stacks, uniform-scale volume, incremental
idempotence, transformed semantic edges, owned Sketch/reference frames, unchanged local Sketch
coordinates, and fixed unowned Sketches.

Block 58 provides the reusable Part feature semantic-input contract without broadening this cache
state into persistent matrices or raw OCCT identity. Block 59 richer Extrude/Cut intent is
implemented together with Block 60 Geometry; Blocks 61–62 Revolve/RevolveCut intent and Geometry
are implemented. Block 64 General Linear Pattern Geometry is the current next boundary.

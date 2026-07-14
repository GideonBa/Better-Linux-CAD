# Part Shell Geometry MVP-6

Status: implemented in Block 72.

Block 72 executes the persistent Block-71 `ShellFeature` intent as checked OCCT thin-wall solid
geometry. Core remains the authority for target Body, ordered removed faces, positive Length
thickness, and Inward/Outward direction; Geometry owns semantic topology recovery and offset
execution.

## Geometry contract

`ShellAdapter` resolves every authored `FaceReference` against the current target solid. Planar
faces are recovered from the current positions of three semantic vertices of the rectangular
additive-extrude producer. Cylindrical faces are recovered from the subtractive circular profile's
axis and radius. Body-transform state is applied during semantic recovery; no OCCT face identity,
traversal index, or persisted topology handle participates.

The ordered resolved faces are passed to `BRepOffsetAPI_MakeThickSolid::MakeThickSolidByJoin`.
Positive persisted thickness maps to a negative OCCT offset for `Inward` and a positive offset for
`Outward`. The result must be exactly one valid solid. Null, invalid, multi-solid, or non-manifold
results fail closed.

## Recompute and cache publication

`GeometryRecomputeExecutor` treats Shell as an in-place Body-history producer. It selects the
latest dependent producer of the target Body, evaluates the current Length parameter, executes the
adapter in a working `ShapeCache`, and publishes feature, Body, and legacy final-shape products only
after complete success.

Thickness changes execute only Shell. Upstream dimension changes execute the upstream producer and
Shell, whose semantic face is recovered from the new geometry. Missing or ambiguous faces,
non-positive/wrong-unit thickness, excessive offsets, OCCT failure, and invalid results leave the
caller's previous cache unchanged.

## Verified behavior

- closed box to open-top inward shell, including checked wall volume
- multiple ordered removal faces
- distinct inward and outward extent behavior
- incremental thickness recompute
- semantic planar-face recovery after an upstream width edit
- broken-reference and excessive-thickness failure without cache replacement

```text
./build/blcad_geometry_tests "[geometry][shell-feature]"
```

Blocks 73–74 DraftFeature intent/JSON and Geometry are implemented. Block 75 Basic 3D Sketch Core
intent is the next boundary.

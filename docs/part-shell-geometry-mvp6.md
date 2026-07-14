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

Blocks 73–74 DraftFeature intent/JSON and Geometry, Block 75 Basic 3D Sketch Core, and Block 76
richer 3D curve intent are implemented. Block 77 3D Sketch JSON and semantic references is implemented; Block 78 3D Sketch Geometry conversion is implemented; Block 79 connected PathCurve Core intent, JSON, and validation is implemented; Block 80 Sweep feature Core intent and JSON is implemented; Block 81 Basic Sweep Geometry is implemented; Block 82 3D path, twist, and guide-controlled Sweep is implemented; Block 83 Path-following Extrude and Extruded Cut is implemented; Block 84 ProfileSectionReference and Loft Core intent plus JSON is implemented; Block 85 Two-section Loft Geometry on arbitrary planes is implemented; Block 86 Multi-section Loft is implemented; Block 87 Guided and continuity-controlled Loft is implemented; Block 88 Surface feature Core intent and JSON is implemented; Block 89 Boundary and Fill Surface Geometry is implemented; Block 90 Trim and Extend Surface Geometry is implemented; Block 91 Stitch/Knit/Sew shell Geometry is implemented; Block 92 Closed shell to solid conversion is implemented; Block 93 multi-body STEP export is implemented; Block 94 integrated Part Construction MVP acceptance is next.

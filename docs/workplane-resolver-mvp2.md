# Workplane Resolver

Status: geometry-layer resolver for standard datum planes, derived top/bottom/right/left/front/back workplanes of a simple additive extrude, explicit construction planes, offset construction planes, three-point construction planes, and planes parallel to another plane through a construction point.

The core stores semantic model intent only. The geometry layer resolves supported intent into concrete frames for recompute. Raw OCCT topology IDs are not stored in `PartDocument` JSON.

## Goal

The resolver makes paths such as these geometrically meaningful:

```text
feature.base_extrude.back
  -> workplane.base_back
  -> sketch.back_hole local point
  -> global cut center
```

```text
construction_plane.offset_xy
  -> sketch.closed_rectangle_on_plane local point
  -> global profile vertex
```

```text
construction_plane.parallel_xy
  -> source workplane datum.xy
  -> construction point through_point
  -> resolved parallel plane frame
```

## Supported workplanes

For `datum.xy`, the resolver returns the stored datum-plane frame and leaves bounds disabled.

For an explicit construction plane, the resolver returns the construction plane frame directly and leaves bounds disabled.

For an offset construction-plane relation, the resolver first resolves the source workplane, then translates the source origin along the source normal by the offset parameter value:

```text
origin = source.origin + offset_mm * source.normal
x_axis = source.x_axis
y_axis = source.y_axis
normal = source.normal
bounds.enabled = false
```

For a three-point construction-plane relation, the resolver builds a right-handed frame from the referenced construction points:

```text
origin = first_point.position
x_axis = normalize(second_point.position - first_point.position)
normal = normalize(cross(second - first, third - first))
y_axis = normalize(cross(normal, x_axis))
bounds.enabled = false
```

For a plane-parallel-through-point relation, the resolver resolves the source workplane and moves the frame origin to the referenced construction point:

```text
origin = through_point.position
x_axis = source.x_axis
y_axis = source.y_axis
normal = source.normal
bounds.enabled = false
```

Generated-face workplanes for top, bottom, right, left, front, and back of a simple rectangular additive extrude keep the established right-handed frames and rectangular bounds.

## Rectangular bounds

Generated-face derived workplanes keep rectangular bounds for top, bottom, right, left, front, and back faces. Construction planes remain unbounded in the current implementation because they represent infinite user reference planes.

## Recompute integration

`GeometryRecomputeExecutor` resolves the input sketch workplane before executing circular cuts and closed-profile extrudes/cuts. The mapping formula is:

```text
global = origin + local.x * x_axis + local.y * y_axis
```

For `construction_plane.offset_xy` with resolved origin `(0, 0, 25)` and local `(2, 3)`, this becomes:

```text
global = (2, 3, 25)
```

For `construction_plane.parallel_xy` through point `(5, 6, 7)` and local `(2, 3)`, this becomes:

```text
global = (7, 9, 7)
```

`GeometryRecomputeExecutor` passes the resolved workplane normal to circular cuts and closed-profile extrudes/cuts. Top and bottom use Z-axis cuts. Right and left use X-axis cuts. Front and back use Y-axis cuts. Construction planes use their explicit or relation-resolved normal.

## Relation scope

`WorkplaneResolver` intentionally resolves only workplane-like relations that can be evaluated from current headless model intent:

- explicit construction planes
- `PlaneOffsetFromPlane`
- `PlaneThroughThreePoints`
- `PlaneParallelToPlaneThroughPoint`

Semantic generated edge and vertex references are evaluated by `SemanticReferenceEvaluator`, not by `WorkplaneResolver`. Deterministic construction points and construction lines are evaluated by `ConstructionPointResolver` and `ConstructionLineResolver`.

## Example models

```text
examples/top_face_cut.blcad.json
examples/bottom_face_cut.blcad.json
examples/right_face_cut.blcad.json
examples/left_face_cut.blcad.json
examples/front_face_cut.blcad.json
examples/back_face_cut.blcad.json
examples/construction_plane_prism.blcad.json
examples/generated_semantic_references.blcad.json
```

## Test coverage

Geometry tests cover:

- resolving `datum.xy`
- resolving generated-face derived workplanes
- resolving explicit construction planes
- resolving offset construction-plane relations
- resolving three-point construction-plane relations
- resolving planes parallel to another plane through a point
- checking construction plane origins, axes, normals, and unbounded status
- checking rectangular bounds for resolved generated-face workplanes
- mapping local sketch points through resolved frames
- rejecting missing workplanes
- recomputing circular cuts and closed-profile operations from sketches on resolved workplanes
- evaluating rectangular-additive-extrude semantic generated edges and vertices
- evaluating deterministic construction points and construction lines derived from generated semantic references

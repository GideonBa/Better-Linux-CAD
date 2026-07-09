# Workplane Resolver

Status: geometry-layer resolver for standard datum planes, derived top/bottom/right/left/front/back workplanes of a simple additive extrude, explicit construction planes, offset construction planes, three-point construction planes, and planes parallel to another plane through a construction point. The resolved workplane frames are used by recompute, by `SketchReferenceProjector`, and by sketch-origin override handling.

The core stores semantic model intent only. The geometry layer resolves supported intent into concrete frames for recompute and projection. Raw OCCT topology IDs are not stored in `PartDocument` JSON.

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

```text
feature.base_extrude.top_front
  -> projected sketch reference
  -> sketch-local point-plus-direction helper line
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

## Sketch origin override

`resolve` returns the raw workplane frame.

`resolve_for_sketch` first resolves the sketch workplane, then applies an optional `SketchOriginOverrideRecord` from the `PartDocument`.

The origin override is a 2D local point in the resolved workplane frame:

```text
origin = resolved.origin
       + override.x * resolved.x_axis
       + override.y * resolved.y_axis
```

If the resolved workplane has rectangular bounds, the bounds center is shifted by `(-override.x, -override.y)` so the valid local bounded area remains correct in the shifted sketch coordinate system.

`GeometryRecomputeExecutor` and `SketchReferenceProjector` use `resolve_for_sketch`, so closed-profile recompute, circular cut centers, and projected reference coordinates all see the same sketch-local frame.

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

## Projected sketch reference integration

`SketchReferenceProjector` uses `WorkplaneResolver::resolve_for_sketch` before projecting reference geometry. It then maps resolved 3D reference geometry into the local sketch frame:

```text
local.x = dot(global - origin, x_axis)
local.y = dot(global - origin, y_axis)
```

Projected points are accepted only when their signed distance to the sketch workplane is within tolerance. Projected lines are accepted only when their anchor point lies on the workplane and their direction is parallel to the workplane.

## Relation scope

`WorkplaneResolver` intentionally resolves only workplane-like relations that can be evaluated from current headless model intent:

- explicit construction planes
- `PlaneOffsetFromPlane`
- `PlaneThroughThreePoints`
- `PlaneParallelToPlaneThroughPoint`

Semantic generated edge and vertex references are evaluated by `SemanticReferenceEvaluator`, not by `WorkplaneResolver`. Deterministic construction points and construction lines are evaluated by `ConstructionPointResolver` and `ConstructionLineResolver`. Projected sketch references combine those resolvers with resolved workplane frames through `SketchReferenceProjector`.

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
examples/projected_sketch_references.blcad.json
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
- applying sketch-origin overrides through `resolve_for_sketch`
- rejecting missing workplanes
- recomputing circular cuts and closed-profile operations from sketches on resolved workplanes
- evaluating rectangular-additive-extrude semantic generated edges and vertices
- evaluating deterministic construction points and construction lines derived from generated semantic references
- projecting semantic generated vertices and edges into sketch-local reference geometry
- projecting generated-reference construction points and construction lines into sketch-local reference geometry
- checking generated-face workplane movement when source feature dimensions change
- rejecting out-of-plane projected sketch references

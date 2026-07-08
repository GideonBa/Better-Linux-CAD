# MVP 2 Workplane Resolver

Status: geometry-layer resolver for the derived top-face workplane of a simple additive extrude.

This document describes the next step after introducing `SemanticFaceReference` and `DerivedWorkplane` in the core model. The core still stores semantic model intent only. The geometry layer now resolves that semantic workplane into a concrete frame for recompute.

## Goal

The goal is to make this path geometrically meaningful:

```text
feature.base_extrude.top
  -> workplane.base_top
  -> sketch.top_hole local point
  -> global cut center
```

The resolver proves that a sketch on a generated planar face can be evaluated without storing raw OCCT face IDs in `PartDocument`.

## Public interface

Header:

```text
include/blcad/geometry/workplane_resolver.hpp
```

Main types:

```text
ResolvedWorkplane
WorkplaneResolver
```

A resolved workplane contains:

```text
id
origin
x_axis
y_axis
normal
```

The resolver exposes:

```text
resolve(document, workplane_id)
evaluate_point(workplane, local_point)
```

## Supported workplanes

### Standard datum plane

For `datum.xy`, the resolver returns the stored datum-plane frame:

```text
origin = (0, 0, 0)
x_axis = (1, 0, 0)
y_axis = (0, 1, 0)
normal = (0, 0, 1)
```

### Derived top-face workplane

For a derived workplane such as:

```text
workplane.base_top -> feature.base_extrude.top
```

the resolver validates that:

- the source feature exists
- the source feature is an `AdditiveExtrude`
- the source sketch exists
- the source sketch has exactly one rectangle profile
- the rectangle width and height parameters exist
- the additive extrude thickness parameter exists
- the semantic face is currently `top`

Then it returns the top-face frame:

```text
origin = (rectangle_center.x, rectangle_center.y, thickness)
x_axis = (1, 0, 0)
y_axis = (0, 1, 0)
normal = (0, 0, 1)
```

For the current reference model this means:

```text
origin = (0, 0, 8)
```

The width and height are intentionally resolved even though the current frame is axis-aligned. This keeps the resolver tied to the supported simple rectangular extrusion and prepares the next step: face-bound validation.

## Recompute integration

`GeometryRecomputeExecutor::execute_subtractive_extrude` now resolves the input sketch workplane before executing the circular cut.

The circle profile center is first evaluated as a local workplane point:

```text
local = (25, -10)
```

Then it is mapped into global coordinates:

```text
global = origin + local.x * x_axis + local.y * y_axis
```

For `workplane.base_top` this becomes:

```text
global = (25, -10, 8)
```

The current circular cut adapter still performs a through-all vertical cut. It uses the resolved global X/Y center. The resolved Z value proves the workplane placement, even though the current through-all cutter spans the target's full Z bounds.

## Example model

The checked-in example is:

```text
examples/top_face_cut.blcad.json
```

It uses an off-center hole on the derived top-face workplane:

```json
"center": {
  "x": 25.0,
  "y": -10.0
}
```

Export command:

```bash
./build/dev-geometry/blcad_export_step examples/top_face_cut.blcad.json build/top_face_cut.step
```

## Test coverage

Geometry tests cover:

- resolving `datum.xy`
- resolving `workplane.base_top`
- checking that the top-face origin is at `z = thickness`
- mapping an off-center sketch point through the resolved frame
- rejecting missing workplanes
- recomputing an off-center circular cut from a sketch on the derived top-face workplane
- verifying that the final volume matches the expected removed cylinder volume within tolerance

## Deliberate limitation

Not included yet:

- face-bound validation
- arbitrary planar faces
- side faces
- flipped workplane orientation
- non-rectangular source faces
- storing or matching raw OCCT face IDs
- GUI face selection

The resolver is intentionally limited to the top face of a simple additive rectangle extrusion. This keeps MVP 2 incremental and avoids prematurely building a full topological-naming system.

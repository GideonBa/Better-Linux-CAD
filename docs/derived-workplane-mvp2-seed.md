# MVP 2 Seed: Derived Workplanes on Generated Planar Faces

Status: semantic top and bottom derived workplanes for sketches on generated planar faces, with geometry-layer resolution and bounded validation documented separately.

This document describes the first carefully limited path toward MVP 2. The implementation allows a sketch to reference a workplane derived from a generated planar face without storing raw OCCT face IDs in the core document.

## Goal

The current supported model paths are:

```text
feature.base_extrude.top
  -> workplane.base_top
  -> sketch.top_hole
  -> feature.top_hole_cut
```

```text
feature.base_extrude.bottom
  -> workplane.base_bottom
  -> sketch.bottom_hole
  -> feature.bottom_hole_cut
```

The paths are intentionally narrow:

- only top and bottom faces of a simple `AdditiveExtrude` are supported
- the references are semantic, not OCCT `TopoDS_Face` handles
- the core remains free of OCCT
- the geometry layer can resolve the workplane, validate its bounds, and recompute a cut from a sketch placed on it
- no general topological naming system is introduced yet
- no GUI is introduced yet

## New core concepts

### `SemanticFace`

`SemanticFace` currently supports:

```text
top
bottom
```

These represent generated faces of a simple additive extrude in a semantic way.

### `SemanticFaceReference`

A semantic face reference points to a feature and a named semantic face role:

```text
SemanticFaceReference
  source_feature = feature.base_extrude
  face = top | bottom
```

The reference deliberately does not store OCCT face IDs. It describes model intent and is resolved by the geometry layer when needed.

### `DerivedWorkplane`

A derived workplane is a workplane created from a semantic face reference:

```text
DerivedWorkplane
  id = workplane.base_bottom
  name = BaseBottomFace
  kind = feature_face
  source_feature = feature.base_extrude
  face = bottom
```

The ID reuses the current workplane reference type used by `Sketch`. A sketch can therefore reference either a standard datum plane or a derived workplane through its existing `workplane` field.

## `PartDocument` integration

`PartDocument` stores:

- standard datum planes
- derived workplanes

A workplane ID must be unique across both standard datum planes and derived workplanes.

When adding a derived workplane, `PartDocument` validates that:

- the workplane ID is unique
- the source feature exists
- the source feature is an `AdditiveExtrude`
- the semantic face is currently `top` or `bottom`

When adding a sketch, `PartDocument` accepts the sketch workplane if it is either:

- an existing `DatumPlane`
- an existing `DerivedWorkplane`

If the sketch uses a derived workplane, the dependency graph receives an edge:

```text
workplane.base_bottom -> sketch.bottom_hole
```

When the derived workplane is added, the dependency graph receives an edge:

```text
feature.base_extrude -> workplane.base_bottom
```

This gives dependency paths such as:

```text
feature.base_extrude -> workplane.base_bottom -> sketch.bottom_hole -> feature.bottom_hole_cut
```

## JSON model format

The model-intent JSON supports `derived_workplanes`:

```json
{
  "id": "workplane.base_bottom",
  "name": "BaseBottomFace",
  "kind": "feature_face",
  "source_feature": "feature.base_extrude",
  "face": "bottom"
}
```

The deserializer resolves dependent objects in multiple passes so that a model can contain:

- a base sketch
- a base extrude
- a derived top or bottom workplane
- a sketch on that derived workplane
- a cut feature using that sketch

The dependency graph and invalidation state are rebuilt from the model during deserialization.

## Geometry resolution and validation

`WorkplaneResolver` resolves derived workplanes into concrete frames:

Top face:

```text
origin = (rectangle_center.x, rectangle_center.y, thickness)
normal = (0, 0, 1)
```

Bottom face:

```text
origin = (rectangle_center.x, rectangle_center.y, 0)
normal = (0, 0, -1)
```

Both use:

```text
x_axis = (1, 0, 0)
y_axis = (0, 1, 0)
```

The resolved workplane also carries rectangular local bounds:

```text
center = (0, 0)
width = source rectangle width
height = source rectangle height
```

`GeometryRecomputeExecutor` uses this frame to map circle-profile centers from sketch-local coordinates to global coordinates and rejects circle profiles that do not lie fully inside the bounds before executing the circular cut.

Details:

- `docs/workplane-resolver-mvp2.md`
- `docs/bounded-workplane-validation-mvp2.md`
- `docs/bottom-workplane-mvp2.md`

## Example models

Top-face model:

```text
examples/top_face_cut.blcad.json
```

Bottom-face model:

```text
examples/bottom_face_cut.blcad.json
```

Headless export:

```bash
./build/dev-geometry/blcad_export_step examples/top_face_cut.blcad.json build/top_face_cut.step
./build/dev-geometry/blcad_export_step examples/bottom_face_cut.blcad.json build/bottom_face_cut.step
```

## Test coverage

Core tests cover:

- semantic face references for top and bottom
- rejecting empty source feature IDs
- adding derived top and bottom workplanes after an additive extrude
- rejecting a derived workplane with a missing source feature
- accepting sketches on derived workplanes
- dependency graph edges through derived workplanes
- JSON roundtrip for top and bottom derived workplanes

Geometry tests cover:

- resolving derived top and bottom workplanes
- mapping local sketch points through resolved workplanes
- full document recompute for cuts whose sketches are placed on derived top or bottom workplanes
- off-center cut volume after resolving workplanes
- near-edge valid hole placement inside bounds
- out-of-bounds invalid hole placement rejected before cutting
- incremental recompute through derived-workplane dependencies

## Deliberate limitation

Not included yet:

- side faces
- arbitrary planar faces
- edge or vertex references
- persistent topological naming
- GUI selection of faces

This is still only a controlled seed for MVP 2. It proves the semantic-reference architecture and the geometry-layer resolution path before broader face support is added.

# MVP 2 Seed: Derived Workplanes on Generated Planar Faces

Status: semantic top, bottom, right, left, front, and back derived workplanes for sketches on generated planar faces, with geometry-layer resolution and bounded validation documented separately.

This document describes the first carefully limited path toward MVP 2. The implementation allows a sketch to reference a workplane derived from a generated planar face without storing raw OCCT face IDs in the core document.

## Goal

The current supported model paths include:

```text
feature.base_extrude.top
  -> workplane.base_top
  -> sketch.top_hole
  -> feature.top_hole_cut
```

```text
feature.base_extrude.back
  -> workplane.base_back
  -> sketch.back_hole
  -> feature.back_hole_cut
```

The paths are intentionally narrow:

- only selected semantic faces of a simple `AdditiveExtrude` are supported
- the references are semantic, not OCCT `TopoDS_Face` handles
- the core remains free of OCCT
- the geometry layer can resolve the workplane, validate its bounds, and recompute a cut from a sketch placed on it
- no general topological naming system is introduced yet
- no GUI is introduced yet

## Core concepts

`SemanticFace` currently supports:

```text
top
bottom
right
left
front
back
```

A semantic face reference points to a feature and a named semantic face role:

```text
SemanticFaceReference
  source_feature = feature.base_extrude
  face = top | bottom | right | left | front | back
```

A derived workplane is a workplane created from a semantic face reference:

```text
DerivedWorkplane
  id = workplane.base_back
  name = BaseBackFace
  kind = feature_face
  source_feature = feature.base_extrude
  face = back
```

The ID reuses the current workplane reference type used by `Sketch`. A sketch can therefore reference either a standard datum plane or a derived workplane through its existing `workplane` field.

## `PartDocument` integration

`PartDocument` stores standard datum planes and derived workplanes. A workplane ID must be unique across both groups.

When adding a derived workplane, `PartDocument` validates that:

- the workplane ID is unique
- the source feature exists
- the source feature is an `AdditiveExtrude`
- the semantic face is currently `top`, `bottom`, `right`, `left`, `front`, or `back`

If the sketch uses a derived workplane, the dependency graph receives an edge:

```text
workplane.base_back -> sketch.back_hole
```

When the derived workplane is added, the dependency graph receives an edge:

```text
feature.base_extrude -> workplane.base_back
```

This gives dependency paths such as:

```text
feature.base_extrude -> workplane.base_back -> sketch.back_hole -> feature.back_hole_cut
```

## JSON model format

The model-intent JSON supports `derived_workplanes`:

```json
{
  "id": "workplane.base_back",
  "name": "BaseBackFace",
  "kind": "feature_face",
  "source_feature": "feature.base_extrude",
  "face": "back"
}
```

The deserializer resolves dependent objects in multiple passes so that a model can contain a base sketch, a base extrude, a derived workplane, a sketch on that derived workplane, and a cut feature using that sketch.

The dependency graph and invalidation state are rebuilt from the model during deserialization.

## Geometry resolution and validation

`WorkplaneResolver` resolves derived workplanes into concrete frames. The side-face frames are documented in detail in `docs/workplane-resolver-mvp2.md` and include right, left, front, and back faces.

Back face:

```text
origin = (rectangle_center.x, rectangle_center.y - height / 2, thickness / 2)
x_axis = (1, 0, 0)
y_axis = (0, 0, 1)
normal = (0, -1, 0)
```

The resolved workplane carries rectangular local bounds. Top and bottom use source rectangle width and height. Right and left use source rectangle height and extrude thickness. Front and back use source rectangle width and extrude thickness.

`GeometryRecomputeExecutor` uses this frame to map circle-profile centers from sketch-local coordinates to global coordinates and rejects circle profiles that do not lie fully inside the bounds before executing the circular cut.

Details:

- `docs/workplane-resolver-mvp2.md`
- `docs/bounded-workplane-validation-mvp2.md`
- `docs/bottom-workplane-mvp2.md`
- `docs/right-workplane-mvp2.md`
- `docs/left-workplane-mvp2.md`
- `docs/front-workplane-mvp2.md`
- `docs/back-workplane-mvp2.md`

## Example models

```text
examples/top_face_cut.blcad.json
examples/bottom_face_cut.blcad.json
examples/right_face_cut.blcad.json
examples/left_face_cut.blcad.json
examples/front_face_cut.blcad.json
examples/back_face_cut.blcad.json
```

Headless export:

```bash
./build/dev-geometry/blcad_export_step examples/top_face_cut.blcad.json build/top_face_cut.step
./build/dev-geometry/blcad_export_step examples/bottom_face_cut.blcad.json build/bottom_face_cut.step
./build/dev-geometry/blcad_export_step examples/right_face_cut.blcad.json build/right_face_cut.step
./build/dev-geometry/blcad_export_step examples/left_face_cut.blcad.json build/left_face_cut.step
./build/dev-geometry/blcad_export_step examples/front_face_cut.blcad.json build/front_face_cut.step
./build/dev-geometry/blcad_export_step examples/back_face_cut.blcad.json build/back_face_cut.step
```

## Test coverage

Core tests cover:

- semantic face references for top, bottom, right, left, front, and back
- rejecting empty source feature IDs
- adding derived workplanes after an additive extrude
- rejecting a derived workplane with a missing source feature
- accepting sketches on derived workplanes
- dependency graph edges through derived workplanes
- JSON roundtrip for top, bottom, right, left, front, and back derived workplanes

Geometry tests cover:

- resolving derived top, bottom, right, left, front, and back workplanes
- mapping local sketch points through resolved workplanes
- full document recompute for cuts whose sketches are placed on derived workplanes
- off-center cut volume after resolving workplanes
- valid hole placement inside bounds
- out-of-bounds invalid hole placement rejected before cutting
- incremental recompute through derived-workplane dependencies

## Deliberate limitation

Not included yet:

- arbitrary planar faces
- edge or vertex references
- persistent topological naming
- GUI selection of faces
- general closed sketch profiles

This is still only a controlled seed for MVP 2. It proves the semantic-reference architecture and the geometry-layer resolution path before broader face support is added.

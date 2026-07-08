# MVP 2 Seed: Derived Workplanes on Generated Planar Faces

Status: minimal semantic-face and derived-workplane path for sketches on generated planar faces, with geometry-layer resolution documented separately in `docs/workplane-resolver-mvp2.md`.

This document describes the first carefully limited step toward MVP 2. The implementation allows a sketch to reference a workplane derived from a generated planar face without storing raw OCCT face IDs in the core document.

## Goal

The goal is to prove this model path:

```text
feature.base_extrude.top
  -> workplane.base_top
  -> sketch.top_hole
  -> feature.top_hole_cut
```

The path is intentionally narrow:

- only the top face of a simple `AdditiveExtrude` is supported
- the reference is semantic, not an OCCT `TopoDS_Face`
- the core remains free of OCCT
- the geometry layer can resolve the workplane and recompute a cut from a sketch placed on it
- no general topological naming system is introduced yet
- no GUI is introduced yet

## New core concepts

### `SemanticFace`

`SemanticFace` currently has one value:

```text
top
```

It represents the generated top face of an additive extrude in a semantic way.

### `SemanticFaceReference`

A semantic face reference points to a feature and a named semantic face role:

```text
SemanticFaceReference
  source_feature = feature.base_extrude
  face = top
```

The reference deliberately does not store OCCT face IDs. It describes model intent and is resolved by the geometry layer when needed.

### `DerivedWorkplane`

A derived workplane is a workplane created from a semantic face reference:

```text
DerivedWorkplane
  id = workplane.base_top
  name = BaseTopFace
  kind = feature_face
  source_feature = feature.base_extrude
  face = top
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
- the semantic face is currently `top`

When adding a sketch, `PartDocument` accepts the sketch workplane if it is either:

- an existing `DatumPlane`
- an existing `DerivedWorkplane`

If the sketch uses a derived workplane, the dependency graph receives an edge:

```text
workplane.base_top -> sketch.top_hole
```

When the derived workplane is added, the dependency graph receives an edge:

```text
feature.base_extrude -> workplane.base_top
```

This gives the final dependency path:

```text
feature.base_extrude -> workplane.base_top -> sketch.top_hole -> feature.top_hole_cut
```

## JSON model format

The model-intent JSON supports `derived_workplanes`:

```json
{
  "id": "workplane.base_top",
  "name": "BaseTopFace",
  "kind": "feature_face",
  "source_feature": "feature.base_extrude",
  "face": "top"
}
```

The deserializer resolves dependent objects in multiple passes so that a model can contain:

- a base sketch
- a base extrude
- a derived top-face workplane
- a sketch on that derived workplane
- a cut feature using that sketch

The dependency graph and invalidation state are rebuilt from the model during deserialization.

## Geometry resolution

`WorkplaneResolver` resolves the derived workplane into a concrete frame:

```text
origin = (rectangle_center.x, rectangle_center.y, thickness)
x_axis = (1, 0, 0)
y_axis = (0, 1, 0)
normal = (0, 0, 1)
```

`GeometryRecomputeExecutor` uses this frame to map circle-profile centers from sketch-local coordinates to global coordinates before executing the circular cut.

Details: `docs/workplane-resolver-mvp2.md`.

## Example model

The repository contains a derived-workplane example:

```text
examples/top_face_cut.blcad.json
```

It describes a rectangular plate where the hole sketch is placed on:

```text
workplane.base_top
```

That workplane is derived from:

```text
feature.base_extrude.top
```

The current example uses an off-center hole point:

```text
(25, -10)
```

## Headless execution

The existing headless exporter can load and export the derived-workplane example:

```bash
./build/dev-geometry/blcad_export_step examples/top_face_cut.blcad.json build/top_face_cut.step
```

## Test coverage

Core tests cover:

- semantic face references
- rejecting empty source feature IDs
- adding a derived workplane after an additive extrude
- rejecting a derived workplane with a missing source feature
- accepting a sketch on a derived workplane
- dependency graph edges through the derived workplane
- JSON roundtrip for derived workplanes

Geometry tests cover:

- resolving a derived top-face workplane
- mapping local sketch points through the resolved workplane
- full document recompute for a cut whose sketch is placed on a derived top-face workplane
- off-center cut volume after resolving the workplane

## Deliberate limitation

Not included yet:

- face-bound validation
- arbitrary planar faces
- side faces
- bottom faces
- edge or vertex references
- persistent topological naming
- GUI selection of faces

This is only the first seed for MVP 2. It proves the semantic-reference architecture and the geometry-layer resolution path before broader face support is added.

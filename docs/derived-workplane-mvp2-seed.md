# MVP 2 Seed: Derived Workplanes on Generated Planar Faces

Status: minimal semantic-face and derived-workplane path for sketches on generated planar faces.

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
- the geometry layer can recompute a cut from a sketch placed on the derived workplane
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

The reference deliberately does not store OCCT face IDs. It describes model intent and can later be resolved by the geometry layer.

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

`PartDocument` now stores:

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

The model-intent JSON now supports `derived_workplanes`:

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

## Headless execution

The existing headless exporter can load and export the derived-workplane example:

```bash
./build/dev-geometry/blcad_export_step examples/top_face_cut.blcad.json build/top_face_cut.step
```

The geometry recompute currently uses the same centered through-cut adapter as the MVP-1 reference model. The important architectural proof is that the sketch is now attached to a semantic generated face reference in the core model.

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

- full document recompute for a cut whose sketch is placed on a derived top-face workplane
- final shape remains one solid
- final volume is smaller than the base extrude volume

## Deliberate limitation

Not included yet:

- arbitrary planar faces
- side faces
- bottom faces
- edge or vertex references
- persistent topological naming
- derived workplane orientation resolution beyond the current simple extrusion convention
- GUI selection of faces

This is only the first seed for MVP 2. It proves the semantic-reference architecture before broader face support is added.

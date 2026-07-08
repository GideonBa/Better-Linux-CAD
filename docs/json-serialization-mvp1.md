# JSON Serialization

Status: core serialization for `PartDocument` model intent, including file-level helpers and derived workplanes for the current controlled semantic faces.

This document describes the JSON serialization layer. The implementation persists model intent only. It does not serialize OCCT shapes, `GeometryShape`, or `ShapeCache` contents.

## Goal

The goal is to make a `PartDocument` reproducible from a textual representation:

1. Build a `PartDocument` with parameters, datum planes, derived workplanes, sketches, and features.
2. Serialize that model intent to JSON.
3. Optionally write the JSON as a `.blcad.json` model file.
4. Rebuild the `PartDocument` from JSON through the normal validated construction APIs.
5. Recompute the restored document into a fresh `ShapeCache`.
6. Export the recomputed final shape as STEP.

This proves that the JSON stores CAD model intent, not computed output geometry.

## JSON scope

The current JSON format stores:

- schema identifier
- schema version
- document ID and name
- length parameters
- datum planes
- derived workplanes with `top`, `bottom`, `right`, `left`, or `front` semantic face references
- rectangle and circle sketch profiles
- additive and subtractive extrude features

The current JSON format does not store OCCT shapes, final BRep geometry, ShapeCache contents, exported STEP data, GUI state, solver state, or assembly data.

## Reference files

The derived-workplane seed models are:

```text
examples/top_face_cut.blcad.json
examples/bottom_face_cut.blcad.json
examples/right_face_cut.blcad.json
examples/left_face_cut.blcad.json
examples/front_face_cut.blcad.json
```

These files describe sketches placed on semantic generated-face workplanes such as:

```text
workplane.base_front -> feature.base_extrude.front
```

No raw OCCT face IDs are stored.

## Derived workplanes

A derived workplane stores a semantic feature-face reference:

```json
{
  "id": "workplane.base_front",
  "name": "BaseFrontFace",
  "kind": "feature_face",
  "source_feature": "feature.base_extrude",
  "face": "front"
}
```

For now, only `kind = "feature_face"` and these face names are supported:

```text
top
bottom
right
left
front
```

## Sketches and features

A sketch stores its identity, workplane reference, and profiles. The workplane reference may point to either a standard datum plane or a derived workplane.

For now, only `+Z` and `through_all` are supported in feature intent. The geometry layer determines the actual cut axis from the resolved workplane normal.

## Deserialization model

Deserialization goes through the normal construction path. Because derived workplanes and dependent sketches may refer to features that appear later in the file, the loader resolves objects in multiple passes:

1. Create `PartDocument`.
2. Add parameters.
3. Add datum planes.
4. Add any sketches whose workplane already exists.
5. Add any features whose sketch and target feature dependencies already exist.
6. Add any derived workplanes whose source feature exists.
7. Repeat until all sketches, features, and derived workplanes are resolved.

This rebuilds the dependency graph and invalidation state from the model instead of trusting serialized graph data.

## Test coverage

Core tests verify JSON roundtrip for top, bottom, right, left, and front derived workplanes. Geometry tests verify that a document restored from JSON can be recomputed into a fresh `ShapeCache` and exported as STEP.

## Deliberate limitation

Not included yet:

- JSON schema file generation
- formula or expression serialization
- assembly serialization
- ShapeCache serialization
- automatic parent-directory creation for model-file output
- back semantic reference
- arbitrary face references

The current implementation is enough to prove that model intent can be persisted, stored as a file, loaded again, and rebuilt independently of computed geometry.

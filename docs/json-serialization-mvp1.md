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

## Public interface

Header:

```text
include/blcad/core/part_document_json.hpp
```

Operations:

```text
serialize_part_document_to_json(document)
deserialize_part_document_from_json(content)
write_part_document_json_file(document, path)
read_part_document_json_file(path)
```

The implementation lives in:

```text
src/core/part_document_json.cpp
```

The serialization code belongs to `blcad_core` because it describes the document model. It uses `nlohmann-json`, but it does not depend on OCCT or Qt.

## JSON scope

The current JSON format stores:

- schema identifier
- schema version
- document ID and name
- length parameters
- datum planes
- derived workplanes with `top`, `bottom`, `right`, or `left` semantic face references
- rectangle and circle sketch profiles
- additive and subtractive extrude features

The current JSON format does not store:

- OCCT shapes
- final BRep geometry
- ShapeCache contents
- exported STEP data
- GUI state
- solver state
- assembly data

## Schema marker

The root object contains a schema marker and a version:

```json
{
  "schema": "blcad.part_document.mvp1",
  "version": 1
}
```

Unknown schemas and unsupported versions are rejected during deserialization.

## Reference files

The checked-in MVP-1 reference model is:

```text
examples/reference_plate.blcad.json
```

The derived-workplane seed models are:

```text
examples/top_face_cut.blcad.json
examples/bottom_face_cut.blcad.json
examples/right_face_cut.blcad.json
examples/left_face_cut.blcad.json
```

These files describe sketches placed on semantic generated-face workplanes such as:

```text
workplane.base_left -> feature.base_extrude.left
```

No raw OCCT face IDs are stored.

## Derived workplanes

A derived workplane stores a semantic feature-face reference:

```json
{
  "id": "workplane.base_left",
  "name": "BaseLeftFace",
  "kind": "feature_face",
  "source_feature": "feature.base_extrude",
  "face": "left"
}
```

For now, only `kind = "feature_face"` and these face names are supported:

```text
top
bottom
right
left
```

## Sketches and features

A sketch stores its identity, workplane reference, and profiles. The workplane reference may point to either a standard datum plane or a derived workplane.

A subtractive extrude stores its input sketch, target feature, depth, and direction:

```json
{
  "id": "feature.left_hole_cut",
  "name": "LeftHoleCut",
  "type": "subtractive_extrude",
  "input_sketch": "sketch.left_hole",
  "direction": "+Z",
  "target_feature": "feature.base_extrude",
  "depth": "through_all"
}
```

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

## Validation behavior

The deserializer rejects:

- invalid JSON
- unsupported schema or version
- unsupported parameter type, scope, or unit
- unsupported datum-plane kind
- unsupported derived-workplane kind
- unsupported semantic face
- unsupported feature type
- unsupported extrude direction
- unsupported subtractive extrude depth
- missing references that cannot be resolved
- duplicate IDs caught by the existing model APIs

Errors are returned as `ErrorCategory::Validation` through `Result<T>`.

## Test coverage

Core tests verify that:

- the MVP-1 reference document round-trips through JSON
- parameters, sketches, features, and dependency edges survive the roundtrip
- a restored document starts clean and creates an empty recompute plan
- `.blcad.json` files can be written and read again
- top-face derived workplanes round-trip through JSON
- bottom-face derived workplanes round-trip through JSON
- right-face derived workplanes round-trip through JSON
- left-face derived workplanes round-trip through JSON
- sketches can reference derived workplanes after deserialization
- unsupported schemas and unsupported feature types are rejected

Geometry tests verify that a document restored from JSON can be recomputed into a fresh `ShapeCache` and exported as STEP.

## Deliberate limitation

Not included yet:

- JSON schema file generation
- formula or expression serialization
- assembly serialization
- ShapeCache serialization
- automatic parent-directory creation for model-file output
- front/back semantic references
- arbitrary face references

The current implementation is enough to prove that model intent can be persisted, stored as a file, loaded again, and rebuilt independently of computed geometry.

# MVP 1 JSON Serialization

Status: core serialization for MVP-1 `PartDocument` model intent.

This document describes the JSON serialization step that completes the previous README next technical step. The implementation persists model intent only. It does not serialize OCCT shapes, `GeometryShape`, or `ShapeCache` contents.

## Goal

The goal is to make an MVP-1 `PartDocument` reproducible from a textual representation:

1. Build a `PartDocument` with parameters, datum planes, sketches, and features.
2. Serialize that model intent to JSON.
3. Rebuild the `PartDocument` from JSON through the normal validated construction APIs.
4. Recompute the restored document into a fresh `ShapeCache`.
5. Export the recomputed final shape as STEP.

This proves that the JSON stores the CAD model intent, not only computed output geometry.

## Public interface

Header:

```text
include/blcad/core/part_document_json.hpp
```

Operations:

```text
serialize_part_document_to_json(document)
deserialize_part_document_from_json(content)
```

The implementation lives in:

```text
src/core/part_document_json.cpp
```

The serialization code belongs to `blcad_core` because it describes the document model. It uses `nlohmann-json`, but it does not depend on OCCT or Qt.

## JSON scope

The MVP-1 JSON format stores:

- schema identifier
- schema version
- document ID and name
- length parameters
- datum planes
- rectangle and circle sketch profiles
- additive and subtractive extrude features

The MVP-1 JSON format does not store:

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

## Parameters

Parameters are serialized as model intent:

```json
{
  "id": "part.width",
  "name": "width",
  "type": "length",
  "scope": "part",
  "unit": "mm",
  "value": 120.0
}
```

For MVP 1, only length parameters in millimeters and part scope are supported. The deserializer reconstructs parameters through `Quantity::length_mm` and `Parameter::create_length`, so the normal validation path remains active.

## Datum planes

MVP 1 supports the standard XY datum plane:

```json
{
  "id": "datum.xy",
  "name": "XY",
  "kind": "xy"
}
```

The serializer also writes origin, axes, and normal as diagnostic data. The deserializer currently supports only `kind = "xy"` and reconstructs the plane through `DatumPlane::xy`.

## Sketches and profiles

A sketch stores its identity, workplane reference, and profiles:

```json
{
  "id": "sketch.base",
  "name": "Sketch_BaseRectangle",
  "workplane": "datum.xy",
  "rectangle_profiles": [
    {
      "id": "profile.base_rectangle",
      "center": { "x": 0.0, "y": 0.0 },
      "width_parameter": "part.width",
      "height_parameter": "part.height"
    }
  ],
  "circle_profiles": []
}
```

The deserializer reconstructs profiles first, inserts them into the sketch, and then adds the sketch to `PartDocument`. This means workplane and parameter references are validated by the existing document API.

## Features

An additive extrude stores its input sketch, length parameter, and direction:

```json
{
  "id": "feature.base_extrude",
  "name": "BaseExtrude",
  "type": "additive_extrude",
  "input_sketch": "sketch.base",
  "direction": "+Z",
  "length_parameter": "part.thickness"
}
```

A subtractive extrude stores its input sketch, target feature, depth, and direction:

```json
{
  "id": "feature.center_hole_cut",
  "name": "CenterHoleCut",
  "type": "subtractive_extrude",
  "input_sketch": "sketch.hole",
  "direction": "+Z",
  "target_feature": "feature.base_extrude",
  "depth": "through_all"
}
```

For MVP 1, only `+Z` and `through_all` are supported.

## Deserialization model

Deserialization intentionally goes through the normal construction path:

1. Create `PartDocument`.
2. Add parameters.
3. Add datum planes.
4. Add sketches.
5. Add features.

This rebuilds the dependency graph and invalidation state from the model, instead of trusting serialized graph data. The dependency graph remains derived data.

## Validation behavior

The deserializer rejects:

- invalid JSON
- unsupported schema or version
- unsupported parameter type, scope, or unit
- unsupported datum-plane kind
- unsupported feature type
- unsupported extrude direction
- unsupported subtractive extrude depth
- missing references caught by `PartDocument`
- duplicate IDs caught by the existing model APIs

Errors are returned as `ErrorCategory::Validation` through `Result<T>`.

## Test coverage

Core tests verify that:

- the MVP-1 reference document round-trips through JSON
- parameters, sketches, features, and dependency edges survive the roundtrip
- a restored document starts clean and creates an empty recompute plan
- unsupported schemas are rejected
- unsupported feature types are rejected

Geometry tests verify that:

- a document restored from JSON can be recomputed into a fresh `ShapeCache`
- the recomputed final shape is valid
- the recomputed final shape can be exported as STEP

## Deliberate limitation

Not included yet:

- filesystem read/write helpers
- a checked-in `.blcad.json` example model
- JSON schema file generation
- formula or expression serialization
- assembly serialization
- ShapeCache serialization

The current implementation is enough to prove that MVP-1 model intent can be persisted and rebuilt independently of computed geometry.

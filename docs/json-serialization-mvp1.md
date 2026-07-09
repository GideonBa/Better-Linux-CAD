# JSON Serialization

Status: core serialization for `PartDocument` model intent, including file-level helpers, derived workplanes, line-based closed sketch profiles, explicit construction geometry, relation-driven construction geometry, chained construction relations, and semantic generated edge/vertex references.

The JSON serialization layer persists model intent only. It does not serialize OCCT shapes, `GeometryShape`, `ShapeCache` contents, raw face IDs, raw edge IDs, raw vertex IDs, BRep handles, or exported STEP data.

## Goal

The goal is to make a `PartDocument` reproducible from a textual representation:

1. Build a `PartDocument` with parameters, datum planes, construction geometry, construction relations, derived workplanes, sketches, sketch entities, profiles, and features.
2. Serialize that model intent to JSON.
3. Optionally write the JSON as a `.blcad.json` model file.
4. Rebuild the `PartDocument` from JSON through the normal validated construction APIs.
5. Recompute the restored document into a fresh `ShapeCache`.
6. Export the recomputed final shape as STEP.

## JSON scope

The current JSON format stores:

- schema identifier and version
- document ID and name
- length parameters
- datum planes
- explicit construction points, lines, and planes
- relation-driven construction lines and planes
- embedded construction relation intent for supported relation-driven construction objects
- semantic generated edge references as `{source_feature, edge}`
- semantic generated vertex references as `{source_feature, vertex}`
- construction-geometry parameter dependencies
- derived workplanes with `top`, `bottom`, `right`, `left`, `front`, or `back` semantic face references
- line-segment sketch entities
- rectangle profiles
- circle profiles
- line-based closed profiles
- additive and subtractive extrude features

The current JSON format does not store final BRep geometry, ShapeCache contents, exported STEP data, GUI state, sketch solver state, general relation collections independent from construction objects, or assembly data.

## Construction geometry

Explicit construction geometry is serialized as numeric model intent:

```json
{
  "construction_points": [
    {
      "id": "point.anchor",
      "name": "Anchor",
      "kind": "explicit",
      "position": {"x": 0.0, "y": 0.0, "z": 25.0},
      "parameter_dependencies": ["part.plane_offset"]
    }
  ]
}
```

Relation-driven construction geometry stores an embedded relation object:

```json
{
  "construction_lines": [
    {
      "id": "line.axis_ab",
      "name": "AxisAB",
      "kind": "through_two_points",
      "relation": {
        "id": "relation.axis_ab",
        "type": "line_through_two_points",
        "first_point": "point.a",
        "second_point": "point.b"
      }
    },
    {
      "id": "line.edge_parallel",
      "name": "GeneratedEdgeParallel",
      "kind": "parallel_to_generated_edge_through_point",
      "relation": {
        "id": "relation.edge_parallel",
        "type": "line_parallel_to_generated_edge_through_point",
        "generated_edge": {
          "source_feature": "feature.base",
          "edge": "top_front"
        },
        "through_point": "point.z"
      }
    }
  ],
  "construction_planes": [
    {
      "id": "construction_plane.parallel_xy",
      "name": "ParallelXY",
      "kind": "parallel_to_plane_through_point",
      "relation": {
        "id": "relation.parallel_xy",
        "type": "plane_parallel_to_plane_through_point",
        "source_plane": "datum.xy",
        "through_point": "point.z"
      }
    }
  ]
}
```

Supported relation type strings are:

```text
plane_offset_from_plane
line_through_two_points
plane_through_three_points
point_on_plane
point_on_line
point_on_generated_edge
point_on_generated_vertex
line_on_plane
plane_parallel_to_plane_through_point
line_parallel_to_line_through_point
line_parallel_to_generated_edge_through_point
```

## Semantic generated references

Generated edge and vertex references are serialized semantically:

```json
{
  "generated_edge": {
    "source_feature": "feature.base",
    "edge": "top_front"
  },
  "generated_vertex": {
    "source_feature": "feature.base",
    "vertex": "bottom_back_left"
  }
}
```

The source feature must restore to an additive extrude before the relation is accepted by `PartDocument`. Deserialization therefore restores construction lines after sketches and features so generated-edge line relations can validate their source feature.

## Derived workplanes

A derived workplane stores a semantic feature-face reference:

```json
{
  "id": "workplane.base_back",
  "name": "BaseBackFace",
  "kind": "feature_face",
  "source_feature": "feature.base_extrude",
  "face": "back"
}
```

For now, only `kind = "feature_face"` and these face names are supported:

```text
top
bottom
right
left
front
back
```

## Sketches and features

A sketch stores its identity, workplane reference, sketch entities, and profiles. The workplane reference may point to a standard datum plane, a derived workplane, an explicit construction plane, or a relation-driven construction plane.

Line-based closed profiles are serialized as ordered references to line-segment sketch entities. The line order is significant. Deserialization rebuilds the `Sketch`, validates that the line chain is connected and non-self-intersecting, and then adds the closed profile through the normal construction API.

Features are serialized as model intent: additive extrudes reference input sketches and length parameters; subtractive extrudes reference input sketches and target features. The final generated shape is not serialized.

## File helpers

`write_part_document_json_file` serializes the document, writes it to disk, flushes and closes the stream, and then returns the final file size. `read_part_document_json_file` reads the file and deserializes it through the same validated APIs as string deserialization.

## Test coverage

Core JSON tests cover:

- MVP-1 rectangular plate roundtrip
- JSON file helper write/read behavior
- derived workplane roundtrips
- line-based closed-profile roundtrips
- explicit construction geometry roundtrips
- relation-driven construction geometry roundtrips
- chained construction relation roundtrips
- semantic generated edge reference roundtrips
- unsupported schema and unsupported feature rejection

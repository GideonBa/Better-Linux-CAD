# JSON Serialization

Status: core serialization for `PartDocument` model intent, including file-level helpers, derived workplanes, line-based closed sketch profiles, composite closed profiles with inner contours, explicit construction geometry, relation-driven construction geometry, relation-driven construction points, chained construction relations, semantic generated edge/vertex references, projected sketch reference entities, first reference-driven sketch constraints, first-class reference-generated sketch helper lines, sketch geometric constraints, sketch driving dimensions, generated-region profile selections, reference recovery metadata, reference remap records, and sketch-origin override records.

The JSON serialization layer persists model intent only. It does not serialize OCCT shapes, `GeometryShape`, `ShapeCache` contents, raw face IDs, raw edge IDs, raw vertex IDs, BRep handles, resolved projected coordinates, solver state, automatic region-search caches, or exported STEP data.

## Goal

The goal is to make a `PartDocument` reproducible from a textual representation:

1. Build a `PartDocument` with parameters, datum planes, construction geometry, construction relations, derived workplanes, sketches, sketch entities, projected sketch reference entities, reference-generated sketch helper lines, reference-driven sketch constraints, sketch geometric constraints, sketch driving dimensions, selected generated-region profiles, composite closed profiles, reference recovery records, profiles, and features.
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
- relation-driven construction points, lines, and planes
- embedded construction relation intent for supported relation-driven construction objects
- semantic generated edge references as `{source_feature, edge}`
- semantic generated vertex references as `{source_feature, vertex}`
- construction-geometry parameter dependencies
- derived workplanes with `top`, `bottom`, `right`, `left`, `front`, or `back` semantic face references
- line-segment sketch entities
- projected sketch point references
- projected sketch line references
- reference-generated sketch helper lines
- reference-driven sketch constraints
- sketch geometric constraints
- sketch driving dimensions
- selected generated-region profile intent as normal `closed_profiles`
- composite closed profiles with `outer_contour` and `inner_contours`
- reference status records
- reference remap records
- sketch-origin override records
- rectangle profiles
- circle profiles
- line-based closed profiles
- additive and subtractive extrude features

The current JSON format does not store final BRep geometry, ShapeCache contents, exported STEP data, GUI state, full sketch solver state, resolved projection caches, automatic region-search caches, automatic topology matching state, general relation collections independent from construction objects, or assembly data.

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

Relation-driven construction points store embedded relation intent:

```json
{
  "construction_points": [
    {
      "id": "point.top_front_mid",
      "name": "TopFrontMidpoint",
      "kind": "on_generated_edge",
      "relation": {
        "id": "relation.point_top_front_mid",
        "type": "point_on_generated_edge",
        "point": "point.top_front_mid",
        "generated_edge": {
          "source_feature": "feature.base",
          "edge": "top_front"
        }
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

Deserialization restores construction lines together with sketches, features, and derived workplanes so generated-edge line relations and projected construction-line references can validate once their source feature and construction point dependencies exist.

## Projected sketch references, helper lines, constraints, dimensions, and generated regions

Sketches can store projected reference entities in `projected_points` and `projected_lines`. These are sketch-local model-intent references. They do not serialize resolved 2D coordinates, OCCT topology, or cached projection results.

Projected point references support construction points and semantic generated vertices. Projected line references support construction lines and semantic generated edges. Reference-driven constraints are stored in a sketch-level `constraints` array.

Reference-generated helper lines are stored in `reference_generated_lines`:

```json
{
  "reference_generated_lines": [
    {
      "id": "helper.ab",
      "start_constraint": "constraint.ab.start",
      "end_constraint": "constraint.ab.end",
      "direction_constraint": "constraint.ab.parallel_x"
    },
    {
      "id": "helper.bc",
      "start_constraint": "constraint.bc.start",
      "end_constraint": "constraint.bc.end"
    }
  ]
}
```

Sketch geometric constraints are stored in `geometric_constraints`, and driving dimensions are stored in `driving_dimensions`:

```json
{
  "geometric_constraints": [
    {
      "id": "constraint.bottom.horizontal",
      "kind": "horizontal",
      "first": {"kind": "line_segment", "entity": "line.bottom"}
    }
  ],
  "driving_dimensions": [
    {
      "id": "dim.width.bottom",
      "kind": "horizontal_distance",
      "first": {"kind": "line_segment_start", "entity": "line.bottom"},
      "second": {"kind": "line_segment_end", "entity": "line.bottom"},
      "parameter": "part.width"
    }
  ]
}
```

Automatically detected regions are not serialized as solver caches. If the user selects a generated region candidate, it is persisted as a normal `closed_profiles` entry with a stable generated-region id and ordered line references:

```json
{
  "closed_profiles": [
    {
      "id": "generated.region.sketch.main.0",
      "line_segments": ["line.a", "line.b", "line.c", "line.d"]
    }
  ]
}
```

Composite closed profiles are stored as explicit contour intent. The JSON stores one ordered `outer_contour` plus one or more ordered `inner_contours`:

```json
{
  "composite_closed_profiles": [
    {
      "id": "profile.plate_with_hole",
      "outer_contour": ["outer.bottom", "outer.right", "outer.top", "outer.left"],
      "inner_contours": [
        ["hole.bottom", "hole.right", "hole.top", "hole.left"]
      ]
    }
  ]
}
```

Supported reference target kinds are:

```text
line_segment
line_segment_start
line_segment_end
projected_point
projected_line
```

Supported first projected-reference constraint kinds are:

```text
coincident_to_projected_point
parallel_to_projected_line
collinear_with_projected_line
```

Supported first geometric constraint kinds are:

```text
fixed
horizontal
vertical
parallel
perpendicular
equal_length
```

Supported first driving dimension kinds are:

```text
horizontal_distance
vertical_distance
aligned_distance
point_to_point_distance
```

## Reference recovery metadata

Reference status records are stored at document level:

```json
{
  "reference_statuses": [
    {
      "id": "status.old_top_face",
      "status": "lost",
      "target": {
        "kind": "face",
        "source_feature": "feature.old",
        "face": "top"
      },
      "message": "old source feature was removed before this model was saved"
    }
  ]
}
```

Reference remap records are explicit model intent. They do not silently rewrite sketches during JSON load.

Sketch-origin overrides are stored as local 2D coordinates in the sketch's resolved workplane frame:

```json
{
  "sketch_origin_overrides": [
    {
      "sketch": "sketch.top_references",
      "local_origin": {"x": 0.0, "y": 0.0}
    }
  ]
}
```

Supported semantic target kinds are:

```text
face
edge
vertex
```

Supported reference status values are:

```text
resolved
lost
```

# JSON Serialization

Status: core serialization for `PartDocument` model intent, including file-level helpers, persistent Solid/Surface Body records, Feature Body-result operations, derived workplanes, line-based closed sketch profiles, arc closed profiles, spline curve segments, tangent-continuity metadata, composite closed profiles with inner contours, explicit construction geometry, relation-driven construction geometry, chained construction relations, semantic generated edge/vertex references, projected sketch reference entities, reference-driven sketch constraints, reference-generated sketch helper lines, sketch geometric constraints, sketch driving dimensions, generated-region profile selections, reference recovery metadata, reference remap records, and sketch-origin override records.

The JSON serialization layer persists model intent only. It does not serialize OCCT shapes, `GeometryShape`, `ShapeCache` contents, raw face IDs, raw edge IDs, raw vertex IDs, BRep handles, resolved projected coordinates, solver state, automatic region-search caches, trim-solver caches, tessellated arc caches, tessellated spline caches, tangent-solver caches, or exported STEP data.

## Goal

The goal is to make a `PartDocument` reproducible from a textual representation:

1. Build a `PartDocument` with Bodies, parameters, datum planes, construction geometry, construction relations, derived workplanes, sketches, sketch entities, projected sketch reference entities, reference-generated sketch helper lines, reference-driven sketch constraints, sketch geometric constraints, sketch driving dimensions, selected generated-region profiles, arc/spline closed profiles, composite closed profiles, reference recovery records, profiles, and features.
2. Serialize that model intent to JSON.
3. Optionally write the JSON as a `.blcad.json` model file.
4. Rebuild the `PartDocument` from JSON through the normal validated construction APIs.
5. Recompute the restored document into a fresh `ShapeCache`.
6. Export the recomputed final shape as STEP.

## JSON scope

The current JSON format stores:

- schema identifier and version
- document ID and name
- deterministic Solid/Surface Body records with stable ID, name, and visibility
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
- three-point arc sketch entities
- cubic Bezier spline sketch entities
- trim/extend metadata records
- tangent-continuity metadata records
- projected sketch point references
- projected sketch line references
- reference-generated sketch helper lines
- reference-driven sketch constraints
- sketch geometric constraints
- sketch driving dimensions
- selected generated-region profile intent as normal `closed_profiles`
- arc/spline-capable closed profiles with ordered `curve_segments`
- composite closed profiles with `outer_contour` and `inner_contours`
- reference status records
- reference remap records
- sketch-origin override records
- rectangle profiles
- circle profiles
- line-based closed profiles
- additive and subtractive extrude features

The current JSON format does not store final BRep geometry, ShapeCache contents, exported STEP data, GUI state, full sketch solver state, resolved projection caches, automatic region-search caches, automatic topology matching state, general relation collections independent from construction objects, or assembly data.

Block 51 now stores explicit Feature Body-result context through optional `operation_mode`,
`target_body`, and `produced_body` fields. Existing Feature records with all three absent restore a
null Body context and retain their historical single-final-shape compatibility behavior. Exact
combinations and failure policy are canonical in `docs/part-feature-body-dependency-mvp6.md`.

## Bodies

`bodies[]` stores required `id`, `name`, `kind`, and `visibility` strings. Canonical kinds are
`solid` and `surface`; canonical visibility values are `visible` and `hidden`. Output is ordered
lexicographically by Body ID. Files without the additive field remain compatible and load with no
implicit Body records. See `docs/part-body-json-mvp6.md` and `docs/file-format.md`.

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

Relation-driven construction points store embedded relation intent, including semantic generated edge and vertex references where needed.

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

Reference-generated helper lines are stored in `reference_generated_lines`. Reference-driven constraints are stored in a sketch-level `constraints` array.

Sketch geometric constraints are stored in `geometric_constraints`, and driving dimensions are stored in `driving_dimensions`.

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

## Arc, spline, trim, and tangent records

Arc sketch entities are serialized in `arc_segments` using the stable three-point definition:

```json
{
  "arc_segments": [
    {
      "id": "arc.top",
      "start": {"x": 10.0, "y": 0.0},
      "mid": {"x": 0.0, "y": 10.0},
      "end": {"x": -10.0, "y": 0.0}
    }
  ]
}
```

Cubic Bezier spline sketch entities are serialized in `spline_segments`:

```json
{
  "spline_segments": [
    {
      "id": "spline.right",
      "kind": "cubic_bezier",
      "start": {"x": 10.0, "y": -8.0},
      "control1": {"x": 18.0, "y": -4.0},
      "control2": {"x": 18.0, "y": 4.0},
      "end": {"x": 10.0, "y": 8.0}
    }
  ]
}
```

Trim/extend metadata is serialized in `trim_extend_operations`:

```json
{
  "trim_extend_operations": [
    {
      "id": "trim.arc.top",
      "kind": "trim",
      "target_entity": "arc.top",
      "replacement_endpoint": {"x": 8.0, "y": 1.0}
    }
  ]
}
```

Tangent-continuity intent is serialized in `tangent_continuities`:

```json
{
  "tangent_continuities": [
    {
      "id": "tangent.bottom_spline",
      "kind": "tangent",
      "first_entity": "line.bottom",
      "second_entity": "spline.right"
    }
  ]
}
```

Arc/spline-capable profiles are serialized in `arc_closed_profiles` as ordered curve references. Each curve reference points to a `line_segments`, `arc_segments`, or `spline_segments` entry:

```json
{
  "arc_closed_profiles": [
    {
      "id": "profile.curved",
      "curve_segments": ["line.bottom", "spline.right", "line.top", "arc.left"]
    }
  ]
}
```

## Feature directions

Extrude direction is serialized as sketch-plane-relative intent:

```text
sketch_normal
opposite_sketch_normal
```

Legacy `+Z` is still accepted on load and maps to `sketch_normal`.

## Composite profiles

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

Sketch-origin overrides are stored as local 2D coordinates in the sketch's resolved workplane frame.

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

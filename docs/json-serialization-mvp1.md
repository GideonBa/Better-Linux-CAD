# JSON Serialization

Status: core serialization for `PartDocument` model intent, including file-level helpers, derived workplanes for the current controlled semantic faces, line-based closed sketch profiles, and explicit construction geometry.

This document describes the JSON serialization layer. The implementation persists model intent only. It does not serialize OCCT shapes, `GeometryShape`, or `ShapeCache` contents.

## Goal

The goal is to make a `PartDocument` reproducible from a textual representation:

1. Build a `PartDocument` with parameters, datum planes, construction geometry, derived workplanes, sketches, sketch entities, profiles, and features.
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
- explicit construction points
- explicit construction lines
- explicit construction planes
- construction-geometry parameter dependencies
- derived workplanes with `top`, `bottom`, `right`, `left`, `front`, or `back` semantic face references
- line-segment sketch entities
- rectangle profiles
- circle profiles
- line-based closed profiles
- additive and subtractive extrude features

The current JSON format does not store OCCT shapes, final BRep geometry, ShapeCache contents, exported STEP data, GUI state, solver state, relation-driven construction definitions, or assembly data.

## Reference files

The derived-workplane seed models are:

```text
examples/top_face_cut.blcad.json
examples/bottom_face_cut.blcad.json
examples/right_face_cut.blcad.json
examples/left_face_cut.blcad.json
examples/front_face_cut.blcad.json
examples/back_face_cut.blcad.json
```

The line-based closed-profile models are:

```text
examples/triangle_prism.blcad.json
examples/triangle_cut_plate.blcad.json
```

The explicit construction-geometry model is:

```text
examples/construction_plane_prism.blcad.json
```

These files describe model intent such as sketches placed on semantic generated-face workplanes, sketches containing line segments plus closed profiles, and sketches placed on explicit construction planes. No raw OCCT face IDs, wires, faces, shapes, or BRep handles are stored.

## Construction geometry

Explicit construction geometry is serialized as model intent:

```json
{
  "construction_points": [
    {
      "id": "construction_point.anchor",
      "name": "Anchor",
      "kind": "explicit",
      "position": {"x": 0.0, "y": 0.0, "z": 25.0},
      "parameter_dependencies": ["part.plane_offset"]
    }
  ],
  "construction_lines": [
    {
      "id": "construction_line.axis_z",
      "name": "AxisZ",
      "kind": "explicit",
      "point": {"x": 0.0, "y": 0.0, "z": 0.0},
      "direction": {"x": 0.0, "y": 0.0, "z": 1.0},
      "parameter_dependencies": []
    }
  ],
  "construction_planes": [
    {
      "id": "construction_plane.offset_xy",
      "name": "OffsetXY",
      "kind": "explicit",
      "origin": {"x": 0.0, "y": 0.0, "z": 25.0},
      "x_axis": {"x": 1.0, "y": 0.0, "z": 0.0},
      "y_axis": {"x": 0.0, "y": 1.0, "z": 0.0},
      "normal": {"x": 0.0, "y": 0.0, "z": 1.0},
      "parameter_dependencies": ["part.plane_offset"]
    }
  ]
}
```

The first version only supports `kind = "explicit"`. `parameter_dependencies` are invalidation edges; they do not yet evaluate expressions into coordinates.

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

A sketch stores its identity, workplane reference, sketch entities, and profiles. The workplane reference may point to a standard datum plane, a derived workplane, or an explicit construction plane.

Line-based closed profiles are serialized as ordered references to line-segment sketch entities:

```json
{
  "id": "sketch.triangle",
  "name": "Sketch_Triangle",
  "workplane": "datum.xy",
  "line_segments": [
    {
      "id": "line.triangle_a",
      "start": {"x": 0.0, "y": 0.0},
      "end": {"x": 40.0, "y": 0.0}
    }
  ],
  "rectangle_profiles": [],
  "circle_profiles": [],
  "closed_profiles": [
    {
      "id": "profile.triangle",
      "line_segments": ["line.triangle_a", "line.triangle_b", "line.triangle_c"]
    }
  ]
}
```

The line order is significant. Deserialization rebuilds the `Sketch`, validates that the line chain is connected and non-self-intersecting, and then adds the closed profile through the normal construction API.

For now, only `+Z` and `through_all` are supported in feature intent. The geometry layer determines the actual cut axis from the resolved workplane normal.

## Deserialization model

Deserialization goes through the normal construction path. Because derived workplanes and dependent sketches may refer to features that appear later in the file, the loader resolves some objects in multiple passes:

1. Create `PartDocument`.
2. Add parameters.
3. Add datum planes.
4. Add explicit construction points, lines, and planes.
5. Add any sketches whose workplane already exists.
6. Add any features whose sketch and target feature dependencies already exist.
7. Add any derived workplanes whose source feature exists.
8. Repeat until all sketches, features, and derived workplanes are resolved.

This rebuilds the dependency graph and invalidation state from the model instead of trusting serialized graph data.

## Test coverage

Core tests verify JSON roundtrip for top, bottom, right, left, front, and back derived workplanes. Core tests also verify JSON roundtrip for line segments, line-based closed profiles, explicit construction points, explicit construction lines, explicit construction planes, and construction-geometry parameter dependencies. Geometry tests verify that restored and checked-in JSON documents can be recomputed into a fresh `ShapeCache` and exported as STEP.

## Deliberate limitation

Not included yet:

- JSON schema file generation
- formula or expression serialization
- relation-driven construction geometry serialization
- assembly serialization
- ShapeCache serialization
- automatic parent-directory creation for model-file output
- arbitrary face references
- arcs, splines, multiple contours, inner holes, or profile-region selection
- 3D sketch and surfacing serialization

The current implementation is enough to prove that model intent can be persisted, stored as a file, loaded again, and rebuilt independently of computed geometry.

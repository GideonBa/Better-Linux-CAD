# Part 3D Sketch JSON MVP-6

Status: implemented in Block 77.

Block 77 adds the optional-on-read and always-emitted top-level `sketches_3d` array to the Part
document JSON. It persists every Block-75/76 entity and freezes cross-source point identity without
storing resolved geometry.

## Deterministic sketch structure

Each record contains exactly:

```text
id, name
points[]
lines[]
polylines[]
arcs[]
splines[]
helices[]
guide_curves[]
```

Sketches preserve Part-document insertion order. Entities preserve insertion order inside the
fixed type sequence above. All entity records are strict: missing, extra, mistyped, or unsupported
fields fail loading.

Coordinates use exactly one form:

```json
{"source":"explicit","value_mm":-2.0}
{"source":"parameter","parameter":"coordinate.x"}
```

## Point-reference grammar

Curve points persist one unambiguous source identity:

```json
{"source":"local_point","point":"p0"}
{"source":"construction_point","point":"point.external"}
{"source":"planar_sketch_point","sketch":"sketch.xy","target":{"kind":"line_segment_start","entity":"line.xy"}}
```

Planar targets are restricted to line start/end or projected-point identity by the Core factory.
No resolved `position`, sampled coordinate, or kernel identity is allowed in a reference record.
Thus a Spline joining multiple planes roundtrips as its three source Sketch ids and semantic point
targets.

## Curve records

- Arcs store ordered `start`, `intermediate`, and `end` references.
- Splines store `fit_points` or `control_points`, degree, `positional`/`tangent`/`curvature`, and
  ordered point references.
- Helices store a typed `datum_axis` or `construction_line`, parameter ids for radius, pitch, and
  turns, plus `right_handed` or `left_handed`.
- Guide curves store source-curve id, `general`/`sweep_path`/`loft_guide`/`centerline` role, and
  label.

Deserialization rebuilds entities in dependency-safe order and then delegates external source,
unit, graph, and removal validation to `PartDocument::add_sketch_3d`.

## Compatibility and verification

Files without `sketches_3d` still load as documents with zero 3D sketches. New files always emit
the array, including when empty.

```text
./build/dev-geometry/blcad_core_tests "[core][sketch-3d-json]"
```

Blocks 48–85 are implemented. Block 78 3D Sketch Geometry conversion is implemented; Block 79 connected PathCurve Core intent, JSON, and validation is implemented; Block 80 Sweep feature Core intent and JSON is implemented; Block 81 Basic Sweep Geometry is implemented; Block 82 3D path, twist, and guide-controlled Sweep is implemented; Block 83 Path-following Extrude and Extruded Cut is implemented; Block 84 ProfileSectionReference and Loft Core intent plus JSON is implemented; Block 85 Two-section Loft Geometry on arbitrary planes is implemented; Block 86 Multi-section Loft is next.

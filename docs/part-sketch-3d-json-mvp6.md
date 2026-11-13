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

Block 160 adds optional `equation_curves[]` to this same record under Part **V8**. Historical
records retain the exact fields above and omit the new array when empty; older Part versions reject
the reserved field. The [V8 format](file-format.md#equation-curve-intent--160)
owns its exact typed expression/domain/policy grammar. Equations use the existing sketch-local entity
ID scope; no evaluated points, sampled splines or compiled programs enter these arrays. Project
resource-aware embedding preserves V8 through its existing owner/reference envelope.

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

Blocks 48–94 are implemented. Block 78 3D Sketch Geometry conversion is implemented; Block 79 connected PathCurve Core intent, JSON, and validation is implemented; Block 80 Sweep feature Core intent and JSON is implemented; Block 81 Basic Sweep Geometry is implemented; Block 82 3D path, twist, and guide-controlled Sweep is implemented; Block 83 Path-following Extrude and Extruded Cut is implemented; Block 84 ProfileSectionReference and Loft Core intent plus JSON is implemented; Block 85 Two-section Loft Geometry on arbitrary planes is implemented; Block 86 Multi-section Loft is implemented; Block 87 Guided and continuity-controlled Loft is implemented; Block 88 Surface feature Core intent and JSON is implemented; Block 89 Boundary and Fill Surface Geometry is implemented; Block 90 Trim and Extend Surface Geometry is implemented; Block 91 Stitch/Knit/Sew shell Geometry is implemented; Block 92 Closed shell to solid conversion is implemented; Block 93 multi-body STEP export is implemented; Block 94 integrated Part Construction MVP acceptance is implemented; Block 95 Qt application shell, GUI document session, and command architecture is implemented; Block 96 document lifecycle, transactions, recompute, and diagnostics is implemented; Block 97 OCCT viewport, navigation, display, and semantic picking is implemented; Block 98 browser, property editor, and selection synchronization is next.

## Spatial constraints and owned scalars — 163

Part V9 extends the existing Sketch3D record with optional `constraints[]` and, only when required,
positive explicit helix radius/pitch sources. The [V9 envelope](file-format.md#spatial-sketch-intent--163)
is emitted only for new spatial intent; historical V1–V8 documents retain their canonical records.
The existing point, three-point arc, fit/control spline, guide and equation arrays are unchanged.

Each constraint stores `id`, `kind`, ordered `targets`, and only its applicable intent fields:
`value` for dimensions, `fixed_position_mm` for Fixed, `angle_convention` for Angle and
`parameter_seed` for point-on-curve. Kinds use the lower-case underscore spellings of the
[structural capability matrix](part-sketch-3d-core-mvp6.md#spatial-constraint-ownership--163).
Constraint IDs retain authored order and are unique within the owning Sketch3D.

Target records are exactly one of:

```text
{kind:point, reference:<existing SketchPointReference3D>}
{kind:local_curve, entity:<id>, location:whole|start|end}
{kind:external_curve, reference:<existing PathSegmentReference>, location:whole|start|end}
{kind:axis|plane|face, reference:<existing typed input source>}
```

Axis sources reuse the existing source-kind grammar; plane/face inputs also retain their explicit
`spatial_constraint` role and capability fields. Dimensional values use either
`{source:parameter, parameter:<id>}` or `{source:literal, unit:mm|deg, value:<number>}`.
Fixed position is exactly numeric `{x,y,z}` in millimetres; normalized branch seeds are finite [0,1].

A parameter-only helix retains `radius_parameter` and `pitch_parameter`. A helix with at least one
explicit source instead stores both `radius` and `pitch` bindings using the existing coordinate
source grammar (`explicit/value_mm` or `parameter/parameter`), validated as positive Length.
Mixed legacy/new fields, a new-form parameter-only record, wrong units, unknown roles/kinds/fields,
missing sources, duplicate IDs and unsupported tuples reject loading. V9 without new spatial intent
and spatial fields in older envelopes are rejected. Project resource-aware embedding preserves V9
through its existing owner/reference envelope.

No solved coordinates, variables, residuals, DOF, rank, Jacobian, compiled expressions, tessellation,
external resolved positions or kernel identity are stored. Existing coordinates remain authored
seeds/bindings; derived solving belongs to 164 and interactive publication to 165. Core and session
proof sources are [schema/JSON tests](../tests/core/sketch_3d_constraint_tests.cpp) and
[session/native preservation](../tests/gui/gui_spatial_constraint_schema_tests.cpp).

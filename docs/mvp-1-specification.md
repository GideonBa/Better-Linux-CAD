# MVP 1 Specification: Single-Part Modeling

Status: implemented MVP-1 vertical slice with core model intent, dependency tracking, recompute planning, optional OCCT geometry execution, STEP export, numeric parameter update, and JSON model-intent serialization.

## Goal

MVP 1 proves the smallest useful core of the CAD system:

A `PartDocument` can describe a single parametric part, compute OCCT geometry from parameters and features, export that geometry as a STEP file, serialize the model intent to JSON, restore the document from JSON, and recompute the restored document into fresh geometry.

The reference part is a rectangular plate with a centered hole:

```text
width         = 120 mm
height        = 80 mm
thickness     = 8 mm
hole_diameter = 20 mm
```

The most important proof is not graphical display. The most important proof is that the model stores design intent:

- the plate is a rectangle that was extruded
- the hole is a circle that cuts through the body
- all relevant values are parameters
- the dependency graph and recompute plan are derived from model references
- the final OCCT shape is only a computed cache
- JSON stores model intent, not computed BRep geometry

## Non-goals

The following topics explicitly do not belong to MVP 1:

- no full GUI
- no UI feature tree
- no assembly documents
- no assembly constraints
- no top-down design
- no sketches on existing body faces
- no bolt circle
- no formulas between parameters
- no full constraint solver
- no semantic face references beyond simple feature outputs
- no material database
- no standard-part library
- no technical drawings
- no DXF import
- no required STL export
- no ShapeCache serialization

This limitation keeps MVP 1 focused on one robust vertical line through document, parameters, feature intent, dependency tracking, recompute, OCCT geometry, STEP export, and model-intent persistence.

## Success criteria

MVP 1 is reached when automated tests prove that the system can:

1. Create a `PartDocument` for a plate.
2. Set the four parameters `width`, `height`, `thickness`, and `hole_diameter`.
3. Create a base sketch on the XY plane that describes a centered rectangle.
4. Create an `AdditiveExtrude` that turns the rectangle into a solid body.
5. Create a second sketch on the same XY plane that describes a centered circle.
6. Create a `SubtractiveExtrude` that cuts the hole through the body.
7. Export the result as a valid STEP file.
8. Recompute the shape when `hole_diameter` or `thickness` changes.
9. Serialize the document's model intent to JSON.
10. Restore the document from JSON through the normal validation path.
11. Recompute the restored document into a fresh `ShapeCache`.
12. Export the restored and recomputed model as STEP.

## Coordinates and units

MVP 1 uses a fixed world coordinate system:

- X axis: plate width
- Y axis: plate height
- Z axis: plate thickness
- origin: center of the lower rectangular face

The base sketch lies on the XY plane at `z = 0`. The plate extrusion runs in positive Z direction.

All lengths are stored internally in millimeters. A parameter still stores type, scope, value, and unit so later unit support can be introduced without changing the core concept.

```text
Parameter
  id        = "part.width"
  name      = "width"
  type      = "length"
  value     = 120
  unit      = "mm"
  scope     = "part"
```

For MVP 1, only `length`, `mm`, and `part` scope are supported.

## Current object model

Implemented core objects:

- `Quantity`
- typed IDs
- `Error`
- `Result<T>`
- `Parameter`
- `PartDocument`
- `DatumPlane`
- `Sketch`
- `RectangleProfile`
- `CircleProfile`
- `Feature`
- `AdditiveExtrude`
- `SubtractiveExtrude`
- `DependencyGraph`
- `InvalidationState`
- `RecomputePlan`

Implemented geometry objects in the optional `blcad_geometry` target:

- `GeometryShape`
- `ShapeCache`
- `RectangleExtrusionAdapter`
- `CircularCutAdapter`
- `GeometryRecomputeExecutor`
- `StepExporter`

Implemented persistence helpers:

- `serialize_part_document_to_json`
- `deserialize_part_document_from_json`

Details:

- `docs/core-mvp1-skeleton.md`
- `docs/json-serialization-mvp1.md`

## `PartDocument`

`PartDocument` is the central document object for MVP 1. It stores:

- `DocumentId`
- name
- parameters
- datum planes
- sketches
- features
- dependency graph
- invalidation state

Rules:

- an MVP-1 document contains exactly one part
- there is no assembly
- there are no external part references
- IDs are stable and typed
- parameter names are unique in the document
- sketch workplanes must exist in the document
- profile parameter references must exist in the document
- feature sketch references must exist in the document
- `SubtractiveExtrude` target features must already exist
- the dependency graph is derived from parameters, sketches, and feature references
- the final `ShapeCache` is not part of the core document and can be rebuilt

## Sketches and profiles

MVP 1 supports a deliberately narrow sketch model:

- standard `XY` datum plane
- centered `RectangleProfile`
- centered `CircleProfile`

The rectangle describes the base plate:

```text
RectangleProfile
  id = profile.base_rectangle
  center = (0, 0)
  width_parameter = part.width
  height_parameter = part.height
```

The circle describes the centered hole:

```text
CircleProfile
  id = profile.center_hole
  center = (0, 0)
  diameter_parameter = part.hole_diameter
```

There is no general sketch constraint solver in MVP 1.

## Features

MVP 1 supports two feature types.

`AdditiveExtrude` creates the base solid from the rectangle profile:

```text
AdditiveExtrude
  id = feature.base_extrude
  input_sketch = sketch.base
  length_parameter = part.thickness
  direction = +Z
```

`SubtractiveExtrude` cuts the centered hole through the body:

```text
SubtractiveExtrude
  id = feature.center_hole_cut
  input_sketch = sketch.hole
  target_feature = feature.base_extrude
  depth = through_all
  direction = +Z
```

For MVP 1, direction is fixed to `+Z` and subtractive depth is fixed to `through_all`.

## Dependency graph

The dependency graph describes which objects depend on which other objects.

For the reference part:

```text
part.width            -> sketch.base
part.height           -> sketch.base
part.hole_diameter    -> sketch.hole
sketch.base           -> feature.base_extrude
part.thickness        -> feature.base_extrude
sketch.hole           -> feature.center_hole_cut
feature.base_extrude  -> feature.center_hole_cut
```

The graph supports stable node IDs, directed dependency edges, direct dependents, transitive dependents, topological order, and cycle detection. The graph itself does not recompute anything.

## Invalidation and recompute plan

When a parameter changes, the changed parameter is marked as `changed`. All transitively dependent nodes are marked as `dirty`.

Example for `part.hole_diameter`:

```text
part.hole_diameter       changed
sketch.hole              dirty
feature.center_hole_cut  dirty
```

A `RecomputePlan` then lists only the `dirty` nodes in topological order. The changed parameter itself is not included because it is already the source of the change.

## Geometry execution

Geometry execution lives in `blcad_geometry`, not in `blcad_core`.

Execution rules:

- `AdditiveExtrude` resolves the rectangle profile and thickness parameter, calls `RectangleExtrusionAdapter`, and writes the result to `ShapeCache`.
- `SubtractiveExtrude` resolves the circle profile and diameter parameter, reads the target shape from `ShapeCache`, calls `CircularCutAdapter`, and writes the result back as the final shape.
- `execute_document` executes a complete document in topological order.
- `execute_plan` executes only affected feature nodes from a recompute plan.

The `ShapeCache` stores computed geometry, not model intent. It is deliberately not serialized.

## STEP export

`StepExporter` writes a computed `GeometryShape` to a STEP file.

Rules:

- export path must not be empty
- shape must not be empty
- expected export errors are reported as `ErrorCategory::Export`
- the output file must be non-empty
- the test checks the `ISO-10303-21` STEP header

## JSON serialization

MVP 1 now supports JSON serialization of model intent through:

```text
serialize_part_document_to_json(document)
deserialize_part_document_from_json(content)
```

The JSON root contains:

```json
{
  "schema": "blcad.part_document.mvp1",
  "version": 1
}
```

The JSON stores:

- document ID and name
- length parameters
- datum planes
- sketches and profiles
- additive and subtractive extrude features

The JSON does not store:

- OCCT shapes
- `GeometryShape`
- `ShapeCache`
- STEP files
- GUI state

Deserialization rebuilds the document through the normal APIs. This revalidates references and rebuilds dependency graph and invalidation state instead of trusting serialized derived data.

Details: `docs/json-serialization-mvp1.md`.

## End-to-end reference flow

The full MVP-1 flow is:

1. Build a `PartDocument`.
2. Add parameters: width, height, thickness, hole diameter.
3. Add datum plane `XY`.
4. Add a base rectangle sketch.
5. Add a centered circle sketch.
6. Add an `AdditiveExtrude` for the base body.
7. Add a `SubtractiveExtrude` for the through-hole.
8. Execute the complete document into a `ShapeCache`.
9. Export the final shape as STEP.
10. Serialize the document to JSON.
11. Deserialize the document from JSON.
12. Recompute the restored document into a fresh `ShapeCache`.
13. Export the restored model as STEP.

## Numeric incremental recompute

A real parameter-value change is supported through `PartDocument::set_parameter_value`.

Example:

```text
set_parameter_value("part.hole_diameter", 40 mm)
```

The expected effect is:

- the hole diameter parameter becomes `changed`
- the hole sketch and cut feature become `dirty`
- the recompute plan contains only the affected cut path
- the unchanged base body is reused from `ShapeCache`
- the final volume becomes smaller because the hole is larger

## Test coverage

The current implementation is covered by tests for:

- quantity validation
- parameter validation and updates
- part document creation and lookup
- datum-plane storage and validation
- sketch and profile storage
- feature storage and validation
- dependency graph construction and cycle detection
- invalidation state
- recompute plan creation
- JSON roundtrip and JSON validation behavior
- rectangle extrusion adapter
- centered circular cut adapter
- ShapeCache storage
- additive and subtractive recompute execution
- full document recompute
- STEP export
- numeric incremental recompute after a real parameter change
- recompute and STEP export from a JSON-restored document

## Next technical step

The next technical step should stay small and turn the in-memory JSON roundtrip into an actual file-level workflow:

1. Add filesystem read/write helpers for `.blcad.json` model files.
2. Add a checked-in reference model under `examples/`.
3. Add a small non-GUI command-line example that loads the JSON model, recomputes it, and exports STEP.
4. Keep the `ShapeCache` as a computed result and do not serialize OCCT geometry.
5. Do not build a general solver yet.
6. Do not build a GUI yet.

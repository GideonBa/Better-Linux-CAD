# MVP 1 Specification: Single-Part Modeling

Source: `zielarchitektur-parametrisches-cad-system.tex`

Status: specification with implemented core skeleton, dependency graph, recompute planning, optional OCCT geometry execution, STEP export, and numeric parameter update.

## Goal

MVP 1 proves the smallest useful core of the CAD system:

A `PartDocument` can describe a single parametric part, compute OCCT geometry from parameters and features, and export that geometry as a STEP file.

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
- the final OCCT shape is only a computed cache

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

This limitation is important. MVP 1 should keep the core small and deliver only the first robust vertical line through document, parameters, feature recompute, OCCT geometry, and STEP export.

## Success criterion

MVP 1 is reached when an automated flow can do the following:

1. Create a `PartDocument` for a plate.
2. Set the four parameters `width`, `height`, `thickness`, and `hole_diameter`.
3. Create a base sketch on the XY plane that describes a centered rectangle.
4. Create an `AdditiveExtrude` that turns the rectangle into a solid body.
5. Create a second sketch on the same XY plane that describes a centered circle.
6. Create a `SubtractiveExtrude` that cuts the hole through the body.
7. Export the result as a valid STEP file.
8. Recompute the shape when `hole_diameter` or `thickness` changes.

## Coordinates and units

### Coordinate system

MVP 1 uses a fixed world coordinate system:

- X axis: plate width
- Y axis: plate height
- Z axis: plate thickness
- origin: center of the lower rectangular face

The base sketch lies on the XY plane at `z = 0`.

The plate extrusion runs in positive Z direction.

### Unit model

All lengths are stored internally in millimeters.

A parameter still stores unit and type so real unit support can be added later.

```text
Parameter
  id        = "part.width"
  name      = "width"
  type      = "length"
  value     = 120
  unit      = "mm"
  scope     = "part"
```

For MVP 1, only this parameter type is required:

- `length`

For MVP 1, only this unit is required:

- `mm`

## Object model

Current implementation state:

- `Quantity`, `Error`, `Result`, typed IDs, `Parameter`, and `PartDocument` exist as the first core skeleton.
- `DatumPlane`, `Sketch`, `RectangleProfile`, and `CircleProfile` exist as pure data models and are integrated into `PartDocument`.
- `Feature`, `AdditiveExtrude`, and `SubtractiveExtrude` exist as pure data models and are integrated into `PartDocument`.
- `DependencyGraph` exists as a pure data model and is integrated into `PartDocument`.
- `InvalidationState` exists as a pure data model and is integrated into `PartDocument`.
- `RecomputePlan` exists as a pure data model and can be derived from `PartDocument`.
- `ShapeCache` exists as an optional geometry data model in `blcad_geometry`; it is intentionally not embedded in `PartDocument`.
- `GeometryRecomputeExecutor` can execute `AdditiveExtrude` for exactly one rectangle profile and `SubtractiveExtrude` for exactly one circle profile in the optional geometry target.
- `GeometryRecomputeExecutor::execute_document` can compute a complete `PartDocument` into a `ShapeCache` in topological order.
- `StepExporter` can export a computed `GeometryShape` as a STEP file.
- An end-to-end test creates the reference part and exports it as a valid STEP file.
- `PartDocument::set_parameter_value` changes a parameter value and marks dependents as changed, enabling numeric incremental recompute.

Details: `docs/core-mvp1-skeleton.md`

## `PartDocument`

`PartDocument` is the central document object for MVP 1.

Minimum content:

```text
PartDocument
  id
  name
  parameters
  datum_planes
  sketches
  features
  dependency_graph
  shape_cache
```

Rules:

- an MVP-1 document contains exactly one part
- there is no assembly
- there are no external part references
- the final shape cache may be deleted and recomputed

Current state:

- `PartDocument` stores `DocumentId`, name, parameters, datum planes, sketches, features, dependency graph, and invalidation state.
- Parameters can be added.
- Parameter IDs and parameter names must be unique.
- Parameters can be found by ID or name.
- Datum planes can be added and must have unique IDs.
- Sketches can be added and must have unique IDs.
- Sketch workplanes must exist in the document.
- Profile parameter references must exist in the document.
- Features can be added and must have unique IDs.
- Feature sketch references must exist in the document.
- Additive extrude length parameters must exist in the document.
- Subtractive extrude target features must exist in the document.
- `PartDocument` creates dependency graph nodes for parameters, sketches, and features.
- `PartDocument` creates dependency graph edges from profile and feature references.
- `PartDocument` marks affected graph nodes after a parameter change.
- `PartDocument` can derive `dirty` nodes as a recompute plan in topological order.
- `PartDocument` can return to a clean state after recompute through `mark_all_clean()`.
- `PartDocument` can change a parameter value through `set_parameter_value()`, validate the new value, and mark dependents as changed.
- The shape cache intentionally remains in the geometry layer and is generated from the document through `execute_document`, rather than being embedded as a core field.

## Parameters

Parameters are named values with type, unit, and scope.

Minimum fields:

```text
Parameter
  id
  name
  type
  value
  unit
  scope
```

Validation:

- `name` is unique within the part document
- `type` is always `length` for MVP 1
- `unit` is always `mm` for MVP 1
- `value` must be numeric
- length parameters must be greater than `0`

Reserved fields for later:

```text
formula
description
locked
```

These fields may be prepared in the data model but do not need to be evaluated in MVP 1.

Current state:

- `Parameter` is immutable; `Parameter::with_value` returns a copy with a new validated value.
- `PartDocument::set_parameter_value` changes a value and marks the parameter and dependents as changed.

## Datum planes

MVP 1 needs only standard planes.

Required:

```text
DatumPlane XY
  origin = (0, 0, 0)
  x_axis = (1, 0, 0)
  y_axis = (0, 1, 0)
  normal = (0, 0, 1)
```

Optional for internal completeness:

```text
DatumPlane YZ
DatumPlane XZ
```

For MVP 1, features only need to work with `XY`.

Current state:

- `DatumPlane::xy()` creates the standard `XY` plane.
- ID and name are validated.
- Datum planes can be stored in `PartDocument` and found by ID.
- Free or derived planes are not implemented yet.

## Sketches

A sketch lies on a `DatumPlane` and contains simple 2D geometry.

Minimum fields:

```text
Sketch
  id
  name
  workplane
  entities
  constraints
```

MVP-1 entities:

- `RectangleProfile`
- `CircleProfile`

`Line` may exist internally as a building block for a rectangle, but it does not need to be exposed as a freely editable entity yet.

MVP-1 constraints:

- no general constraint solving
- profiles are generated directly from parameters
- centering is described by the profile definition

Current state:

- `Sketch` stores `SketchId`, name, and `DatumPlaneId`.
- Rectangle and circle profiles can be added.
- Profile IDs must be unique within a sketch.
- Workplane references are validated when adding the sketch to `PartDocument`.
- Profile parameter references are validated when adding the sketch to `PartDocument`.

## Rectangle profile

The rectangle describes the base plate.

```text
RectangleProfile
  id
  center = (0, 0)
  width_parameter = part.width
  height_parameter = part.height
```

Derived corner points:

```text
(-width / 2, -height / 2)
( width / 2, -height / 2)
( width / 2,  height / 2)
(-width / 2,  height / 2)
```

Validation:

- `width > 0`
- `height > 0`

Current state:

- `RectangleProfile` stores `ProfileId`, center, width parameter reference, and height parameter reference.
- Missing profile IDs and parameter references are rejected.
- Parameter references are validated against existing parameters when the sketch is added to `PartDocument`.
- Parameter values are resolved by geometry execution, not by the profile object itself.

## Circle profile

The circle describes the centered hole.

```text
CircleProfile
  id
  center = (0, 0)
  diameter_parameter = part.hole_diameter
```

Validation:

- `diameter > 0`
- later: `diameter < min(width, height)`

Current state:

- `CircleProfile` stores `ProfileId`, center, and diameter parameter reference.
- Missing profile IDs and parameter references are rejected.
- Parameter references are validated against existing parameters when the sketch is added to `PartDocument`.

## Features

MVP 1 needs two feature types:

- `AdditiveExtrude`
- `SubtractiveExtrude`

### `AdditiveExtrude`

Creates the base solid from the rectangle profile.

```text
AdditiveExtrude
  id = feature.base_extrude
  input_sketch = sketch.base
  length_parameter = part.thickness
  direction = +Z
```

Rules:

- the input sketch must exist
- the length parameter must exist
- the length must be greater than `0`
- for MVP 1, the sketch must contain exactly one rectangle profile

### `SubtractiveExtrude`

Cuts the centered hole through the body.

```text
SubtractiveExtrude
  id = feature.center_hole_cut
  input_sketch = sketch.hole
  target_feature = feature.base_extrude
  depth = through_all
  direction = +Z
```

Rules:

- the input sketch must exist
- the target feature must exist
- for MVP 1, the sketch must contain exactly one circle profile
- depth is fixed to `through_all`
- direction is fixed to `+Z`

## Dependency graph

The dependency graph describes which objects depend on which other objects.

For the reference part:

```text
part.width           -> sketch.base
part.height          -> sketch.base
part.hole_diameter   -> sketch.hole
sketch.base          -> feature.base_extrude
part.thickness       -> feature.base_extrude
sketch.hole          -> feature.center_hole_cut
feature.base_extrude -> feature.center_hole_cut
```

The graph supports:

- stable node IDs
- directed dependency edges
- direct dependents
- transitive dependents
- topological order
- cycle detection

The graph itself does not recompute anything. It only defines the dependency structure.

## Invalidation and recompute plan

When a parameter changes, the changed parameter is marked as `changed`. All transitively dependent nodes are marked as `dirty`.

Example for `part.hole_diameter`:

```text
part.hole_diameter       changed
sketch.hole              dirty
feature.center_hole_cut  dirty
```

A `RecomputePlan` then lists only the `dirty` nodes in topological order. The changed parameter itself is not included because it is already the source of the change.

For a width change:

```text
part.width               changed
sketch.base              dirty
feature.base_extrude     dirty
feature.center_hole_cut  dirty
```

The resulting plan is:

```text
sketch.base
feature.base_extrude
feature.center_hole_cut
```

## Geometry execution

Geometry execution lives in `blcad_geometry`, not in `blcad_core`.

The current geometry path contains:

- `RectangleExtrusionAdapter`
- `CircularCutAdapter`
- `ShapeCache`
- `GeometryRecomputeExecutor`
- `StepExporter`

Execution rules:

- `AdditiveExtrude` resolves the rectangle profile and thickness parameter, calls the rectangle extrusion adapter, and writes the result to the `ShapeCache`.
- `SubtractiveExtrude` resolves the circle profile and diameter parameter, reads the target shape from the `ShapeCache`, calls the circular cut adapter, and writes the result back as the final shape.
- `execute_document` executes a complete document in topological order.
- `execute_plan` executes only the affected feature nodes from a recompute plan.

## ShapeCache

The `ShapeCache` stores computed geometry, not model intent.

It can store:

- feature shapes by `FeatureId`
- one final shape
- the source feature of the final shape

The cache can be cleared and rebuilt from the document. It intentionally remains in the geometry layer because it contains OCCT-backed geometry.

## STEP export

`StepExporter` writes a computed `GeometryShape` to a STEP file.

Rules:

- export path must not be empty
- shape must not be empty
- expected export errors are reported as `ErrorCategory::Export`
- the output file must be non-empty
- the test checks the `ISO-10303-21` STEP header

## End-to-end reference flow

The MVP-1 end-to-end test performs this flow:

1. Build a `PartDocument`.
2. Add parameters: width, height, thickness, hole diameter.
3. Add datum plane `XY`.
4. Add a base rectangle sketch.
5. Add a centered circle sketch.
6. Add an `AdditiveExtrude` for the base body.
7. Add a `SubtractiveExtrude` for the through-hole.
8. Execute the complete document into a `ShapeCache`.
9. Verify that the final shape has one solid.
10. Verify that the cut reduces the volume compared with the full plate.
11. Export the final shape as STEP.
12. Verify that the STEP file is valid and non-empty.

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
- the unchanged base body is reused from the `ShapeCache`
- the final volume becomes smaller because the hole is larger

## Error handling

Expected errors are returned as `Error` objects wrapped in `Result<T>`.

Important categories:

- `validation`
- `dependency`
- `geometry`
- `export`
- `internal`

Examples:

- empty IDs
- duplicate IDs
- missing parameter references
- missing sketch references
- missing target feature
- dependency graph cycle
- missing target shape in `ShapeCache`
- failed OCCT operation
- failed STEP export

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
- rectangle extrusion adapter
- centered circular cut adapter
- ShapeCache storage
- additive and subtractive recompute execution
- full document recompute
- STEP export
- numeric incremental recompute after a real parameter change

## Next technical step

The next technical step should stay small:

1. Prepare JSON serialization for the model intent of `PartDocument`.
2. Rebuild the document from JSON and recompute it.
3. Continue to treat the `ShapeCache` only as a computed result.
4. Do not build a general solver yet.
5. Do not build a GUI yet.

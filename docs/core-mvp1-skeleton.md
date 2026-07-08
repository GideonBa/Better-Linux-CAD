# Core Skeleton and MVP-2 Seed

Status: implemented core skeleton for MVP-1 data models, recompute planning, JSON model-intent serialization, `.blcad.json` file helpers, semantic top-face workplanes, and geometry-layer workplane resolution.

The core remains free of OCCT and Qt. Geometry is handled only in the optional `blcad_geometry` target. JSON serialization and `.blcad.json` file helpers stay in the core because they store model intent rather than computed shapes.

## Goal of this step

The skeleton makes the first architecture decisions executable:

- typed IDs from `0002-object-identity-and-naming.md`
- `Quantity` for millimeter lengths from `0003-units-and-quantities.md`
- `Error` and `Result` from `0004-error-handling.md`
- `Parameter` as the first building block of the part model
- `PartDocument` as the first container for parameters, datum planes, derived workplanes, sketches, features, dependency graph, invalidation state, and recompute plan
- `DatumPlane` and `Sketch` as pure data models for the first plate
- `SemanticFaceReference` and `DerivedWorkplane` as the first semantic generated-face reference path
- `RectangleProfile` and `CircleProfile` as the first sketch profiles
- `Feature`, `AdditiveExtrude`, and `SubtractiveExtrude` as the first feature data models
- `DependencyGraph` as a pure dependency structure
- `InvalidationState` as a pure state model for changed and affected nodes
- `RecomputePlan` as an ordered list of `dirty` nodes
- `part_document_json` as schema-versioned JSON serialization for model intent
- `.blcad.json` read/write helpers for file-level persistence
- `ShapeCache` as an optional geometry data model for computed shapes
- `WorkplaneResolver` as the first geometry-layer bridge from semantic workplane intent to a concrete frame

## CMake targets

### `blcad_core`

Static core library.

Currently contains:

- `blcad/core/id.hpp`
- `blcad/core/spatial.hpp`
- `blcad/core/error.hpp`
- `blcad/core/result.hpp`
- `blcad/core/quantity.hpp`
- `blcad/core/parameter.hpp`
- `blcad/core/part_document.hpp`
- `blcad/core/part_document_json.hpp`
- `blcad/core/datum_plane.hpp`
- `blcad/core/sketch.hpp`
- `blcad/core/feature.hpp`
- `blcad/core/dependency_graph.hpp`
- `blcad/core/invalidation_state.hpp`
- `blcad/core/recompute_plan.hpp`

This target does not link against OCCT or Qt.

### `blcad_core_tests`

Catch2 test program for the core skeleton.

Current test areas:

- `Quantity`
- `Parameter`
- `PartDocument`
- `PartDocument` JSON serialization
- `.blcad.json` file read/write helpers
- `DatumPlane`
- `DerivedWorkplane`
- `SemanticFaceReference`
- `Sketch`
- `RectangleProfile`
- `CircleProfile`
- `Feature`
- `DependencyGraph`
- `InvalidationState`
- `RecomputePlan`
- `Error`
- `Result`

### `blcad_geometry`

Optional static library for the first OCCT adapters and geometry execution.

Currently contains:

- `blcad/geometry/rectangle_extrusion_adapter.hpp`
- `blcad/geometry/circular_cut_adapter.hpp`
- `blcad/geometry/recompute_executor.hpp`
- `blcad/geometry/shape_cache.hpp`
- `blcad/geometry/step_exporter.hpp`
- `blcad/geometry/workplane_resolver.hpp`

This target is built only when `BLCAD_BUILD_GEOMETRY=ON` is set. It links against OCCT, but not against Qt.

### `blcad_export_step`

Small non-GUI command-line example built with the geometry target.

It loads a `.blcad.json` model, recomputes it through the geometry layer, and exports the final shape as STEP:

```text
blcad_export_step <input.blcad.json> <output.step>
```

### `blcad_geometry_tests`

Catch2 test program for the optional geometry layer.

Current test areas:

- `RectangleExtrusionAdapter`
- `CircularCutAdapter`
- `GeometryShape`
- `GeometryRecomputeExecutor`
- `ShapeCache`
- `StepExporter`
- `WorkplaneResolver`
- full reference-part recompute
- recompute from a JSON-restored document
- recompute from a sketch on a derived top-face workplane
- off-center cut evaluation through a resolved derived workplane

## Core types

### Typed IDs

IDs are typed internally so that, for example, a `ParameterId` cannot accidentally be used as a `FeatureId`.

Currently prepared:

- `DocumentId`
- `ParameterId`
- `DatumPlaneId`
- `SketchId`
- `FeatureId`
- `ShapeCacheId`
- `ProfileId`

`DatumPlaneId` currently identifies both standard datum planes and derived workplanes. This is intentional for now because `Sketch` still stores one workplane reference field.

### `Quantity`

`Quantity` represents only positive lengths in millimeters for the current scope.

Rules:

- internal unit: `mm`
- value type: `double`
- `0`, negative values, and non-finite values are invalid
- creation runs through `Result<Quantity>`

### `PartDocument`

`PartDocument` currently contains:

- `DocumentId`
- name
- parameters
- datum planes
- derived workplanes
- sketches
- features
- dependency graph
- invalidation state

Rules:

- document ID and name must not be empty
- parameter IDs and names must be unique
- datum-plane, derived-workplane, sketch, and feature IDs must be unique
- workplane IDs must be unique across standard datum planes and derived workplanes
- sketch workplanes must point to an existing datum plane or derived workplane
- profile parameter references must point to existing parameters
- feature input sketches must point to existing sketches
- additive extrude length parameters must point to existing parameters
- subtractive extrude target features must point to existing features
- derived workplanes must point to an existing additive extrude source feature
- parameters, sketches, features, and derived workplanes create dependency graph nodes
- profile, feature, and derived-workplane references create dependency graph edges
- `PartDocument` synchronizes its invalidation state with the dependency graph
- parameter changes can mark affected nodes as `changed` and `dirty`
- `PartDocument` can derive a recompute plan from `dirty` nodes

`PartDocument` intentionally does not contain OCCT geometry. The `ShapeCache` remains in the geometry layer.

### `SemanticFaceReference` and `DerivedWorkplane`

The first semantic face reference supports:

```text
feature.base_extrude.top
```

A derived workplane exposes that generated top face as a sketch workplane:

```text
workplane.base_top -> feature.base_extrude.top
```

The dependency graph records:

```text
feature.base_extrude -> workplane.base_top -> sketch.top_hole
```

No raw OCCT face IDs are stored in the core.

### Geometry workplane resolution

`WorkplaneResolver` lives in the geometry layer. It resolves:

- `datum.xy` from a stored datum-plane frame
- `workplane.base_top` from the source additive rectangle extrude

For the supported top-face workplane, the resolved frame is:

```text
origin = (rectangle_center.x, rectangle_center.y, thickness)
x_axis = (1, 0, 0)
y_axis = (0, 1, 0)
normal = (0, 0, 1)
```

`GeometryRecomputeExecutor` uses that frame to map local circle-profile centers into global cut centers before executing `CircularCutAdapter`.

## JSON serialization and file workflow

`serialize_part_document_to_json` serializes model intent into a schema-versioned JSON document. `deserialize_part_document_from_json` rebuilds a `PartDocument` from that JSON through the normal validated construction APIs.

`write_part_document_json_file` and `read_part_document_json_file` provide the file-level `.blcad.json` workflow.

The JSON contains:

- schema marker `blcad.part_document.mvp1`
- version `1`
- document ID and name
- length parameters
- datum planes
- derived workplanes
- sketches and profiles
- additive and subtractive extrude features

The JSON deliberately does not contain:

- OCCT shapes
- `GeometryShape`
- `ShapeCache` contents
- STEP data
- GUI state

Details:

- `docs/json-serialization-mvp1.md`
- `docs/json-file-workflow-mvp1.md`
- `docs/derived-workplane-mvp2-seed.md`
- `docs/workplane-resolver-mvp2.md`

## Optional geometry layer

The geometry layer is intentionally outside the core. It uses OCCT behind adapter boundaries and keeps `PartDocument` free of OCCT headers and linking.

Current geometry capabilities:

- `RectangleExtrusionAdapter` creates a centered rectangular OCCT solid from validated width, height, and thickness values.
- `CircularCutAdapter` cuts a through-hole from an existing `GeometryShape` using a resolved global center.
- `ShapeCache` stores feature shapes and the current final shape.
- `WorkplaneResolver` resolves standard datum planes and the derived top face of a simple additive rectangle extrude.
- `GeometryRecomputeExecutor` executes `AdditiveExtrude` for a sketch with exactly one rectangle profile.
- `GeometryRecomputeExecutor` executes `SubtractiveExtrude` for a sketch with exactly one circle profile if the target shape is already in the `ShapeCache`.
- `execute_document` recomputes a complete `PartDocument` into a `ShapeCache` in topological order.
- `StepExporter` writes the final shape as a STEP file.
- `blcad_export_step` wires `.blcad.json` input to recompute and STEP export.

The cache is not integrated into `PartDocument`. It remains a computed result next to the document.

## Reference models

The checked-in MVP-1 model is:

```text
examples/reference_plate.blcad.json
```

The checked-in MVP-2 seed model is:

```text
examples/top_face_cut.blcad.json
```

Both can be exported with:

```text
blcad_export_step <input.blcad.json> <output.step>
```

## Deliberate limitation

This skeleton still does not implement:

- GUI
- assemblies
- general sketch constraints
- general constraint solver
- semantic face references beyond the top face of a simple additive extrude
- face-bound validation for profiles on derived workplanes
- ShapeCache serialization
- command-line argument parsing beyond the minimal example

This order is intentional. The core first needs safe data types, validation, dependency tracking, recompute planning, serialization, file workflow, a narrow geometry pipeline, semantic face references, and workplane resolution before larger CAD subsystems are added.

## Standard commands

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

The tests must pass before new core building blocks are added.

Optional geometry build:

```bash
cmake --preset dev-geometry
cmake --build --preset dev-geometry
ctest --preset dev-geometry
```

Further details are documented in the dedicated data-model, geometry, JSON, file-workflow, and derived-workplane documents in `docs/`.

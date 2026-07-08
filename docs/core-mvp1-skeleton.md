# MVP 1 Core Skeleton

Status: implemented core skeleton for the MVP-1 data models, recompute planning, and JSON model-intent serialization.

This document describes the current MVP-1 core state. The core remains free of OCCT and Qt. Geometry is handled only in the optional `blcad_geometry` target, while JSON serialization stays in the core because it stores model intent rather than computed shapes.

## Goal of this step

The skeleton makes the first architecture decisions executable:

- typed IDs from `0002-object-identity-and-naming.md`
- `Quantity` for millimeter lengths from `0003-units-and-quantities.md`
- `Error` and `Result` from `0004-error-handling.md`
- `Parameter` as the first building block of the part model
- `PartDocument` as the first container for MVP-1 parameters, datum planes, sketches, features, dependency graph, invalidation state, and recompute plan
- `DatumPlane` and `Sketch` as pure data models for the first plate
- `RectangleProfile` and `CircleProfile` as the first sketch profiles
- `Feature`, `AdditiveExtrude`, and `SubtractiveExtrude` as the first feature data models
- `DependencyGraph` as a pure dependency structure
- `InvalidationState` as a pure state model for changed and affected nodes
- `RecomputePlan` as an ordered list of `dirty` nodes
- `part_document_json` as schema-versioned JSON serialization for MVP-1 model intent
- `ShapeCache` as an optional geometry data model for computed shapes

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
- `DatumPlane`
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

Optional static library for the first OCCT adapters.

Currently contains:

- `blcad/geometry/rectangle_extrusion_adapter.hpp`
- `blcad/geometry/circular_cut_adapter.hpp`
- `blcad/geometry/recompute_executor.hpp`
- `blcad/geometry/shape_cache.hpp`
- `blcad/geometry/step_exporter.hpp`

This target is built only when `BLCAD_BUILD_GEOMETRY=ON` is set. It links against OCCT, but not against Qt.

### `blcad_geometry_tests`

Catch2 test program for the optional geometry layer.

Current test areas:

- `RectangleExtrusionAdapter`
- `CircularCutAdapter`
- `GeometryShape`
- `GeometryRecomputeExecutor`
- `ShapeCache`
- `StepExporter`
- full reference-part recompute
- recompute from a JSON-restored document

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

### `Quantity`

`Quantity` represents only positive lengths in millimeters for MVP 1.

Rules:

- internal unit: `mm`
- value type: `double`
- `0`, negative values, and non-finite values are invalid
- creation runs through `Result<Quantity>`

### `Error` and `Result<T>`

`Error` describes expected errors in the core. `Result<T>` wraps either a successful value or an expected error.

Current error categories:

- `validation`
- `dependency`
- `geometry`
- `export`
- `internal`

### `Parameter`

`Parameter` currently represents only length parameters in part scope.

Required:

- non-empty `ParameterId`
- non-empty name
- positive `Quantity`
- type `length`
- scope `part`

`Parameter::with_value` returns a copy with a new validated value while preserving identity.

### `PartDocument`

`PartDocument` is the first document container for MVP 1. It currently contains:

- `DocumentId`
- name
- parameters
- datum planes
- sketches
- features
- dependency graph
- invalidation state

Rules:

- document ID and name must not be empty
- parameter IDs and names must be unique
- datum-plane, sketch, and feature IDs must be unique
- sketch workplanes must point to existing datum planes
- profile parameter references must point to existing parameters
- feature input sketches must point to existing sketches
- additive extrude length parameters must point to existing parameters
- subtractive extrude target features must point to existing features
- parameters, sketches, and features create dependency graph nodes
- profile and feature references create dependency graph edges
- `PartDocument` synchronizes its invalidation state with the dependency graph
- parameter changes can mark affected nodes as `changed` and `dirty`
- `PartDocument` can derive a recompute plan from `dirty` nodes

`PartDocument` intentionally does not contain OCCT geometry. The `ShapeCache` remains in the geometry layer.

### Sketch and feature model

MVP 1 currently supports:

- the standard `XY` datum plane
- centered rectangle profiles
- centered circle profiles
- additive extrude in `+Z`
- subtractive extrude in `+Z` with `through_all`

The data model stores intent and references. Parameter values are resolved by the geometry execution layer.

### `DependencyGraph`

`DependencyGraph` describes dependencies between stable string node IDs.

An edge:

```text
A -> B
```

means:

```text
B depends on A.
```

The graph supports direct dependents, transitive dependents, topological order, and cycle detection. It does not execute invalidation and does not start recompute.

### `InvalidationState`

`InvalidationState` stores whether graph nodes are `clean`, `changed`, `dirty`, or `error`. A parameter change marks the parameter as `changed` and all transitively dependent nodes as `dirty`.

### `RecomputePlan`

`RecomputePlan` describes which `dirty` nodes should later be recomputed.

Rules:

- only `dirty` nodes are included
- `changed` nodes are not included
- the order follows the dependency graph's topological order
- graph cycles are reported as dependency errors

The plan is a task list. It does not execute features and does not create geometry.

## JSON serialization

`serialize_part_document_to_json` serializes MVP-1 model intent into a schema-versioned JSON document. `deserialize_part_document_from_json` rebuilds a `PartDocument` from that JSON through the normal validated construction APIs.

The JSON contains:

- schema marker `blcad.part_document.mvp1`
- version `1`
- document ID and name
- length parameters
- datum planes
- sketches and profiles
- additive and subtractive extrude features

The JSON deliberately does not contain:

- OCCT shapes
- `GeometryShape`
- `ShapeCache` contents
- STEP data
- GUI state

Details: `docs/json-serialization-mvp1.md`.

## Optional geometry layer

The geometry layer is intentionally outside the core. It uses OCCT behind adapter boundaries and keeps `PartDocument` free of OCCT headers and linking.

Current geometry capabilities:

- `RectangleExtrusionAdapter` creates a centered rectangular OCCT solid from validated width, height, and thickness values.
- `CircularCutAdapter` cuts a centered through-hole from an existing `GeometryShape`.
- `ShapeCache` stores feature shapes and the current final shape.
- `GeometryRecomputeExecutor` executes `AdditiveExtrude` for a sketch with exactly one rectangle profile.
- `GeometryRecomputeExecutor` executes `SubtractiveExtrude` for a sketch with exactly one circle profile if the target shape is already in the `ShapeCache`.
- `execute_document` recomputes a complete `PartDocument` into a `ShapeCache` in topological order.
- `StepExporter` writes the final shape as a STEP file.

The cache is not integrated into `PartDocument`. It remains a computed result next to the document.

## Deliberate limitation

This skeleton still does not implement:

- filesystem read/write helpers for `.blcad.json`
- a command-line export tool
- GUI
- assemblies
- general sketch constraints
- general constraint solver
- semantic face references beyond the current simple model

This order is intentional. The core first needs safe data types, validation, dependency tracking, recompute planning, serialization, and a narrow geometry pipeline before larger CAD subsystems are added.

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

Further details are documented in the dedicated MVP-1 data-model, geometry, and JSON documents in `docs/`.

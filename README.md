# BLCAD

BLCAD is planned as an independent parametric CAD system for Linux. The condensed target architecture is documented in `docs/architecture-summary.md`.

The current state is a deliberately small MVP-1 vertical slice. It contains base types for parameters, length quantities, error handling, a pure `PartDocument` model, datum planes, sketches, feature-intent data models, a dependency graph, invalidation state, recompute planning, JSON model persistence, optional OCCT geometry execution, and STEP export. There is no GUI yet.

The optional `blcad_geometry` target contains OCCT adapters for rectangle extrusion and centered circular cuts, a small `ShapeCache`, recompute execution for `AdditiveExtrude` and `SubtractiveExtrude`, full document recompute, and STEP export for the final shape. MVP-1 model intent can be serialized to `.blcad.json`, loaded again, recomputed into a fresh `ShapeCache`, and exported as STEP through a small headless command-line example.

## Technical basis

- Language: C++20
- Build system: CMake with Ninja
- Geometry kernel: OCCT / Open CASCADE Technology
- Future GUI base: Qt 6
- Math helper library: Eigen
- File-format prototyping: nlohmann-json
- Logging and formatting: spdlog, fmt
- Tests: Catch2

## Project structure

- `docs/` contains architecture, MVP, and setup documents.
- `include/blcad/` contains the public headers of the core skeleton.
- `src/` contains the initial core implementation.
- `tests/` contains Catch2 tests for the core and geometry layer.
- `examples/` contains the checked-in MVP-1 reference model and headless export example.
- `cmake/` is reserved for later CMake helper modules.
- `assets/` is reserved for later UI and example resources.

## Documentation

- `docs/development-setup.md`: local setup and standard commands
- `docs/architecture-summary.md`: architecture overview
- `docs/core-mvp1-skeleton.md`: current core skeleton
- `docs/sketch-mvp1-data-model.md`: datum-plane and sketch data model
- `docs/feature-mvp1-data-model.md`: feature data model
- `docs/dependency-graph-mvp1-data-model.md`: dependency graph data model
- `docs/invalidation-mvp1-data-model.md`: invalidation-state data model
- `docs/recompute-plan-mvp1-data-model.md`: recompute-plan data model
- `docs/geometry-adapter-mvp1-rectangle-extrusion.md`: first OCCT adapter
- `docs/geometry-adapter-mvp1-circular-cut.md`: OCCT adapter for the centered cut
- `docs/shape-cache-mvp1-data-model.md`: ShapeCache data model
- `docs/recompute-execution-mvp1-additive-extrude.md`: first additive recompute execution
- `docs/recompute-execution-mvp1-subtractive-cut.md`: subtractive recompute execution
- `docs/step-export-mvp1.md`: STEP export for the final shape
- `docs/document-recompute-mvp1.md`: full document recompute and reference-part pipeline
- `docs/parameter-update-mvp1.md`: parameter-value update and numeric incremental recompute
- `docs/json-serialization-mvp1.md`: JSON serialization of MVP-1 model intent
- `docs/json-file-workflow-mvp1.md`: `.blcad.json` file workflow and headless export example
- `docs/mvp-plan.md`: MVP sequence
- `docs/mvp-1-specification.md`: detailed MVP-1 specification
- `docs/decisions/`: architecture decision records

## Current technical state

The current core skeleton covers `Quantity`, typed IDs, `Error`, `Result`, `Parameter`, `PartDocument`, `DatumPlane`, `Sketch`, `RectangleProfile`, and `CircleProfile`, as well as `Feature`, `AdditiveExtrude`, `SubtractiveExtrude`, `DependencyGraph`, `InvalidationState`, and `RecomputePlan`. `PartDocument` validates workplane, profile, and feature references, creates graph nodes and graph edges from them, marks affected nodes after a parameter change, and derives an ordered recompute plan from that state.

The optional geometry build creates an OCCT solid for a centered rectangle extrusion from three length values and can store computed feature shapes in a `ShapeCache`. `CircularCutAdapter` cuts a centered through-hole from a cached base body. `GeometryRecomputeExecutor` can execute additive and subtractive extrude features, recompute a full `PartDocument` in topological order into a `ShapeCache`, and recompute only affected feature nodes from an incremental plan. `StepExporter` writes the final shape as a STEP file.

The MVP-1 persistence path is now file-based: `serialize_part_document_to_json` and `deserialize_part_document_from_json` handle in-memory model-intent JSON, while `write_part_document_json_file` and `read_part_document_json_file` handle `.blcad.json` files. The checked-in `examples/reference_plate.blcad.json` model can be loaded by `blcad_export_step`, recomputed through the geometry layer, and exported as STEP.

## Headless example

Build the geometry preset:

```bash
cmake --preset dev-geometry
cmake --build --preset dev-geometry
```

Export the checked-in reference model:

```bash
./build/dev-geometry/blcad_export_step examples/reference_plate.blcad.json build/reference_plate.step
```

Depending on the local CMake preset output directory, the executable path may differ. The command shape is:

```text
blcad_export_step <input.blcad.json> <output.step>
```

## Next technical step

The next technical step should start MVP 2 carefully: sketches on generated planar faces.

1. Introduce minimal semantic face references for the MVP-1 generated body, starting with the top face of `feature.base_extrude`.
2. Add a derived workplane representation that can reference that semantic face without storing raw OCCT face IDs in `PartDocument`.
3. Allow a sketch to use the derived workplane as its workplane reference.
4. Recompute a cut from a sketch placed on the generated top face.
5. Keep the first implementation limited to planar faces from simple extrudes.
6. Do not build a general topological naming system yet.
7. Do not build a GUI yet.

The detailed MVP-1 specification is in `docs/mvp-1-specification.md`.

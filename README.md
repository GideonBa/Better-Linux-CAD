# BLCAD

BLCAD is planned as an independent parametric CAD system for Linux. The condensed target architecture is documented in `docs/architecture-summary.md`.

The current state is a deliberately small MVP-1 core skeleton. It contains base types for parameters, length quantities, error handling, a pure `PartDocument` data model, integrated datum-plane and sketch data models, initial feature data models, a dependency graph integrated into `PartDocument`, an invalidation state, and a recompute plan as a data model.

In addition, the repository contains an optional `blcad_geometry` target with OCCT adapters for rectangle extrusion and centered circular cuts, a small `ShapeCache`, recompute execution for `AdditiveExtrude` and `SubtractiveExtrude`, full document recompute, and STEP export for the final shape. Parameter values can be changed through `PartDocument`, so a change invalidates and incrementally recomputes the dependent geometry. MVP-1 model intent can now be serialized to JSON, restored through the normal validated `PartDocument` APIs, recomputed into a fresh `ShapeCache`, and exported again as STEP. There is no GUI yet.

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
- `tests/` contains Catch2 tests for the core.
- `cmake/` is reserved for later CMake helper modules.
- `assets/` is reserved for later UI and example resources.
- `examples/` is reserved for later example models.

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
- `docs/mvp-plan.md`: MVP sequence
- `docs/mvp-1-specification.md`: detailed MVP-1 specification
- `docs/decisions/`: architecture decision records

## Current technical state

The current core skeleton covers `Quantity`, typed IDs, `Error`, `Result`, `Parameter`, `PartDocument`, `DatumPlane`, `Sketch`, `RectangleProfile`, and `CircleProfile`, as well as `Feature`, `AdditiveExtrude`, `SubtractiveExtrude`, `DependencyGraph`, `InvalidationState`, and `RecomputePlan`. `PartDocument` validates workplane, profile, and feature references, creates graph nodes and graph edges from them, marks affected nodes after a parameter change, and derives an ordered recompute plan from that state.

The optional geometry build creates an OCCT solid for a centered rectangle extrusion from three length values and can store computed feature shapes in a `ShapeCache`. `GeometryRecomputeExecutor` can execute an `AdditiveExtrude` node from a `RecomputePlan` if the corresponding sketch contains exactly one rectangle profile. `CircularCutAdapter` cuts a centered through-hole from a cached base body, and the executor executes a `SubtractiveExtrude` node if the sketch contains exactly one circle profile and the target body already exists in the `ShapeCache`. `execute_document` recomputes a full `PartDocument` in topological order into the `ShapeCache`, and `StepExporter` writes the final shape as a STEP file.

An end-to-end test creates the MVP-1 reference part, a 120 x 80 x 8 mm plate with a centered 20 mm hole, and exports it as a valid STEP file. The `ShapeCache` remains in the geometry layer; `PartDocument` remains OCCT-free, and the caller invokes `mark_all_clean()` after recompute. `PartDocument::set_parameter_value` changes a parameter value, validates it, and marks dependents as changed. As a result, an incremental plan recomputes only the cut after an actual diameter change, and a larger hole reduces the final volume.

The previous next step is now implemented: `serialize_part_document_to_json` writes MVP-1 model intent to a schema-versioned JSON document, and `deserialize_part_document_from_json` rebuilds a `PartDocument` from that JSON through the normal validation path. Core tests verify the roundtrip and validation behavior. Geometry tests verify that a restored document can be recomputed into a fresh `ShapeCache` and exported as STEP.

## Next technical step

The next technical step should stay small and turn the in-memory JSON roundtrip into an actual file-level workflow:

1. Add filesystem read/write helpers for `.blcad.json` model files.
2. Add a checked-in reference model under `examples/`.
3. Add a small non-GUI command-line example that loads the JSON model, recomputes it, and exports STEP.
4. Keep the `ShapeCache` as a computed result and do not serialize OCCT geometry.
5. Do not build a general solver yet.
6. Do not build a GUI yet.

The detailed MVP-1 specification is in `docs/mvp-1-specification.md`.
